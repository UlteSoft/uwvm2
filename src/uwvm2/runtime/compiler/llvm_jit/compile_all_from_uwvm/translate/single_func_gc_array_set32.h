// Exact SET32=1 only. Five integer C arguments and one uint32 status;
// opaque reference/carrier authority is unchanged. No scratch/output buffer.
extern "C" [[nodiscard]] inline ::std::uint32_t uwvm2_llvm_jit_gc_array_set32_bridge_r1(
    ::std::uintptr_t module_address, ::std::uint32_t reference_kind,
    ::std::uintptr_t opaque_payload, ::std::uint32_t element_index,
    ::std::uint32_t raw_bits) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    using status = storage::gc_object_status;
    static_assert(sizeof(status) == sizeof(::std::uint32_t));
    if(module_address == 0u) { return static_cast<::std::uint32_t>(status::invalid_value); }
    // [actual compiler relocation][synchronous owned module]
    // [safe ] Wasm operands cannot supply this argument. The original actual
    // runtime entry/source keeps this native module alive throughout the call.
    auto const* module{reinterpret_cast<storage::wasm_module_storage_t const*>(module_address)};
    auto* store{module->gc_store.get()};
    if(store == nullptr || !store->valid()) { return static_cast<::std::uint32_t>(status::invalid_store); }
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    // Preserve the SAME generic-array retirement boundary. This leaf cannot
    // admit a provisional numeric-struct page or bypass outer root/admission.
    ::uwvm2::runtime::gc::managed_page_boundary(false);
#endif
    storage::gc_reference reference{};
    reference.kind = static_cast<::uwvm2::object::global::wasm_ref_kind>(reference_kind);
    // [original opaque representation][no arithmetic/dereference]
    // [safe ] membership/kind/actual owner MUST be established in array_set32
    // before forming an array address. This cast alone grants no authority.
    reference.storage.ptr = reinterpret_cast<void*>(opaque_payload);
    return static_cast<::std::uint32_t>(store->array_set32(reference, element_index, raw_bits));
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_array_set32(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded,
    runtime_operand_stack_value_type value_type) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
    using wasm_type = runtime_operand_stack_value_type;
    using reference = storage::gc_reference;
    static_assert(::std::is_standard_layout_v<reference>);
    constexpr auto bytes{sizeof(reference)};
    constexpr auto payload_offset{offsetof(reference, storage)};
    constexpr auto payload_bytes{sizeof(reference::storage)};
    constexpr auto kind_offset{offsetof(reference, kind)};
    constexpr auto kind_bytes{sizeof(reference::kind)};
    static_assert(payload_bytes == sizeof(::std::uintptr_t));
    static_assert(kind_bytes == sizeof(::std::uint32_t));
    static_assert(payload_offset <= bytes && payload_bytes <= bytes - payload_offset);
    static_assert(kind_offset <= bytes && kind_bytes <= bytes - kind_offset);
    static_assert(payload_offset + payload_bytes <= kind_offset ||
        kind_offset + kind_bytes <= payload_offset);
    if(!state.valid || state.ir_builder == nullptr || state.llvm_module == nullptr ||
       state.local_func_storage_ptr == nullptr || state.operand_stack.size() < 3uz ||
       decoded.opcode != 14u ||
       (value_type != wasm_type::i32 && value_type != wasm_type::f32)) { return false; }
    auto& stack{state.operand_stack};
    // [live SSA prefix][complete array reference][i32 index][raw scalar] end
    // [safe ] size>=3 proves each subscript. Copy every descriptor before any
    // mutation; no pointer into the operand vector survives the native call.
    auto const operand{stack[stack.size() - 3uz]};
    auto const index{stack[stack.size() - 2uz]};
    auto const input{stack.back()};
    if(operand.type != wasm_type::funcref || operand.value == nullptr ||
       !operand.value->getType()->isIntegerTy(static_cast<unsigned>(bytes * CHAR_BIT)) ||
       index.type != wasm_type::i32 || index.value == nullptr ||
       !index.value->getType()->isIntegerTy(32u) || input.type != value_type || input.value == nullptr ||
       (value_type == wasm_type::i32 ? !input.value->getType()->isIntegerTy(32u) :
        !input.value->getType()->isFloatTy())) { return false; }
    auto const* module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr || !module->gc_store || !module->gc_store->valid()) { return false; }
    type::composite_kind kind{};
    if(!module->gc_store->type_kind(decoded.first, kind) || kind != type::composite_kind::array)
    { return false; }
    auto const* field{module->gc_store->field_at(decoded.first, 0uz)};
    if(field == nullptr || !field->mutable_) { return false; }
    if(value_type == wasm_type::f32)
    {
        if(field->storage.packed != type::packed_kind::none ||
           field->storage.value.kind != type::value_kind::f32) { return false; }
    }
    else if(field->storage.value.kind != type::value_kind::i32 ||
            (field->storage.packed != type::packed_kind::none &&
             field->storage.packed != type::packed_kind::i8 &&
             field->storage.packed != type::packed_kind::i16)) { return false; }
    auto const& layout{state.llvm_module->getDataLayout()};
    // Every declinable native-layout/type condition precedes creation of IR.
    // Wider/reference arrays continue to the complete original helper.
    if(state.llvm_module->getDataLayoutStr().empty() ||
       layout.getPointerSize() != sizeof(::std::uintptr_t) ||
       layout.isLittleEndian() != (::std::endian::native == ::std::endian::little)) { return false; }
    auto& builder{*state.ir_builder};
    auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const i32{builder.getInt32Ty()};
    constexpr bool little_endian{::std::endian::native == ::std::endian::little};
    constexpr auto payload_shift{static_cast<unsigned>((little_endian ? payload_offset :
        bytes - payload_offset - payload_bytes) * CHAR_BIT)};
    constexpr auto kind_shift{static_cast<unsigned>((little_endian ? kind_offset :
        bytes - kind_offset - kind_bytes) * CHAR_BIT)};
    // [complete native 16B carrier SSA][two disjoint actual fields]
    // [safe ] shifts/truncations extract representation, never a dereferenced
    // native address or authority from padding/guest token arithmetic.
    auto const payload{builder.CreateTrunc(builder.CreateLShr(operand.value, payload_shift),
        intptr, get_llvm_string_ref(u8"gc.array.set32.payload"))};
    auto const reference_kind{builder.CreateTrunc(builder.CreateLShr(operand.value, kind_shift),
        i32, get_llvm_string_ref(u8"gc.array.set32.kind"))};
    auto const raw{value_type == wasm_type::f32 ?
        builder.CreateBitCast(input.value, i32, get_llvm_string_ref(u8"gc.array.set32.raw.f32")) : input.value};
    auto const module_name{get_llvm_runtime_module_object_symbol_name(*module)};
    // [actual owned instance][compiler host-symbol relocation]
    // [safe ] binding retains the original native module lifetime; this is
    // not a direct unowned store pointer encoded into guest data.
    auto const module_address{get_llvm_external_host_object_address(builder,
        reinterpret_cast<::std::uintptr_t>(module),
        ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()})};
    if(module_address == nullptr) { return false; }
    auto const bridge_type{::llvm::FunctionType::get(i32, {intptr, i32, intptr, i32, i32}, false)};
    auto const callee{get_llvm_runtime_bridge_function_symbol_value<uwvm2_llvm_jit_gc_array_set32_bridge_r1>(
        builder, bridge_type, ::uwvm2::utils::container::u8string_view{u8"gc_array_set32_raw_v1"})};
    if(callee == nullptr) { return false; }
    ::llvm::Value* arguments[]{module_address, reference_kind, payload, index.value, raw};
    // [five real C integer arguments][status-only return]
    // [safe ] no scratch input/output pointer is passed. The original
    // synchronous membership/lease/lock setter is noexcept and does not poll.
    auto const status{apply_llvm_jit_host_calling_conv(builder.CreateCall(bridge_type, callee, arguments))};
    status->setDoesNotThrow();
    auto const failed{[&](storage::gc_object_status value)
    { return builder.CreateICmpEQ(status, ::llvm::ConstantInt::get(i32, static_cast<::std::uint32_t>(value))); }};
    emit_llvm_conditional_trap(*state.llvm_module, builder, failed(storage::gc_object_status::null_reference),
        ::uwvm2::runtime::lib::llvm_jit_trap_kind::null_reference);
    emit_llvm_conditional_trap(*state.llvm_module, builder, failed(storage::gc_object_status::out_of_bounds),
        ::uwvm2::runtime::lib::llvm_jit_trap_kind::array_out_of_bounds);
    emit_llvm_conditional_trap(*state.llvm_module, builder,
        builder.CreateOr(failed(storage::gc_object_status::out_of_memory),
            failed(storage::gc_object_status::size_overflow)),
        ::uwvm2::runtime::lib::llvm_jit_trap_kind::gc_allocation_failure);
    emit_llvm_conditional_trap(*state.llvm_module, builder,
        builder.CreateICmpNE(status, ::llvm::ConstantInt::get(i32, 0u)),
        ::uwvm2::runtime::lib::llvm_jit_trap_kind::runtime_invariant_failure);
    // [SSA prefix][three copied input descriptors] end -> [SSA prefix] end
    // [safe ] only status-success can retire inputs. There is no result slot,
    // native byte borrow or descriptor alias across this vector mutation.
    stack.pop_back();
    stack.pop_back();
    stack.pop_back();
    return true;
}
