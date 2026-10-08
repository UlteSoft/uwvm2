// Included only after runtime_local_func_llvm_jit_emit_state_t and the exact
// debug activation emitters. Entry capture is an incremental native producer:
// it does NOT implement opcode/call/PHI/EH resumptions or grant restore rights.
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_checkpoint_entry(
    runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    namespace checkpoint = ::uwvm2::runtime::checkpoint;
    auto const plan{state.checkpoint_plan};
    if(plan == nullptr || plan->producer_availability != checkpoint::status::ok) { return true; } // no ordinary/unavailable payload IR
    if(!plan->profile || plan->compiler_failure != checkpoint::status::ok || plan->sites.size() != 1u || !state.debug_activation_enabled ||
       state.debug_activation_token == nullptr || state.ir_builder == nullptr ||
       checkpoint::validate_site(plan->sites.front(), *plan) != checkpoint::status::ok)
    { return false; }
    auto const& site{plan->sites.front()};
    if(site.local_count != state.local_pointers.size() || site.local_count != state.local_types.size() ||
       site.operand_count != 0u || site.saved_parameter_count != 0u || site.slots.size() != site.local_count)
    { return false; }
    if(!checkpoint_saved_control_workspace_fits(state, site.local_count, 0u))
    { plan->producer_availability = checkpoint::status::quota_exceeded; return true; }
    auto& builder{*state.ir_builder};
    auto const integer{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    ::llvm::Value* address{::llvm::ConstantInt::get(integer, 0u)};
    if(site.local_count != 0u)
    {
        // [compiler-owned local slots 0 ... N] exclusive end
        // [safe                              ] validate_site/profile prove
        // N*16 <= PTRDIFF_MAX before LLVM byte array/GEP construction.
        auto const bytes{site.local_count * checkpoint::native_slot_bytes};
        auto const packet_type{::llvm::ArrayType::get(builder.getInt8Ty(), bytes)};
        ::llvm::Value* packet{};
        if(checkpoint_observer_workspace_selected(state))
        {
            if(state.checkpoint_observer_workspace == nullptr) { return false; }
            packet = builder.CreateInBoundsGEP(builder.getInt8Ty(), state.checkpoint_observer_workspace,
                builder.getInt64(site.local_count * checkpoint::observer_local_metadata_bytes));
        }
        else
        {
            auto const allocated{create_llvm_jit_entry_block_alloca(builder, packet_type, nullptr,
                get_llvm_string_ref(u8"checkpoint.entry.typed.locals.v1"))};
            if(allocated == nullptr) { return false; }
            allocated->setAlignment(::llvm::Align{checkpoint::native_slot_bytes});
            packet = allocated;
        }
        if(packet == nullptr) { return false; }
        bool const observing{checkpoint_observer_workspace_selected(state)};
        if(observing)
        {
            // Entry packets can contain the complete declaration too. Reuse
            // the bounded private copier; a huge aggregate zero/store followed
            // by one inline load per local defeats that bound at entry.
            auto const saved_flags{builder.CreateInBoundsGEP(builder.getInt8Ty(), state.checkpoint_observer_workspace,
                builder.getInt64(site.local_count))};
            if(!emit_runtime_local_func_llvm_jit_snapshot_copy(state, site.local_count, packet, saved_flags,
                state.checkpoint_observer_workspace, false)) { return false; }
        }
        else
        {
            builder.CreateMemSet(packet, builder.getInt8(0u), bytes, ::llvm::MaybeAlign{1u});
        }
        for(::std::size_t i{}; !observing && i != site.local_count; ++i)
        {
            if(!site.slots[i].initialized) { continue; } // NEVER load an unset nondefaultable local
            auto const llvm_type{get_llvm_type_from_wasm_value_type(builder.getContext(), state.local_types.index_unchecked(i))};
            auto const width{get_runtime_wasm_value_type_abi_size(state.local_types.index_unchecked(i))};
            if(llvm_type == nullptr || width == 0u || width > checkpoint::native_slot_bytes ||
               state.local_pointers.index_unchecked(i) == nullptr) { return false; }
            // [complete N*16-byte entry packet ... i*16 ...] packet_end
            // [safe                                      ] i<N and width<=16
            // prove both this byte offset and the full typed store before GEP.
            auto const slot{builder.CreateInBoundsGEP(packet_type, packet,
                {builder.getInt32(0u), builder.getInt64(i * checkpoint::native_slot_bytes)})};
            auto const live{builder.CreateLoad(llvm_type, state.local_pointers.index_unchecked(i))};
            // Native ABI bit preservation, including floating NaN and complete
            // reference carrier. This internal buffer is never serialized or
            // exposed to guest memory; the GC exporter must later relocate IDs.
            auto const store{builder.CreateStore(live, slot)};
            store->setAlignment(::llvm::Align{1u});
        }
        address = builder.CreatePtrToInt(packet, integer);
    }
    auto const type{::llvm::FunctionType::get(builder.getVoidTy(),
        {builder.getInt64Ty(), builder.getInt64Ty(), integer, integer}, false)};
    // Internal control hook, not a host import: do not suspend the live Wasm
    // activation or invent a foreign-host island around its materialization.
    auto const bridge{get_llvm_runtime_bridge_function_symbol_value_unwrapped<
        ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_abi_bridge>(builder, type)};
    if(bridge == nullptr) { return false; }
    if(auto const declaration{::llvm::dyn_cast<::llvm::Function>(bridge)}; declaration != nullptr)
    { declaration->setDoesNotThrow(); }
    auto const call{apply_llvm_jit_host_calling_conv(builder.CreateCall(type, bridge,
        {state.debug_activation_token, builder.getInt64(site.identifier), address,
         ::llvm::ConstantInt::get(integer, site.local_count * checkpoint::native_slot_bytes)}))};
    if(call == nullptr) { return false; }
    call->setDoesNotThrow(); // real TLS state write; no readonly/nosync/speculation promise
    return true;
}
