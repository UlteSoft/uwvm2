#pragma once
// Compiler-private snapshot copies. Include after the actual emit state and
// entry-alloca helper. An entry-owned pointer table with a bounded activation
// budget lets cached noinline bodies serve every opcode instead of cloning loads.
// Neither the table nor the helper is a Wasm/ASM/native-address capability.
inline constexpr ::std::size_t llvm_jit_snapshot_copy_local_limit{256u};

[[nodiscard]] inline bool prepare_runtime_local_func_llvm_jit_snapshot_local_pointers(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t count) noexcept
{
    if(count == 0u || state.llvm_function == nullptr || state.llvm_module == nullptr || state.ir_builder == nullptr ||
       state.llvm_function->getParent() != state.llvm_module || count > state.local_pointers.size() ||
       state.local_pointers.size() != state.local_types.size()) { return false; }
    bool const heap_table{state.checkpoint_observer_workspace != nullptr};
    auto const table_count{heap_table ? state.local_pointers.size() :
        (::std::min)(state.local_pointers.size(),llvm_jit_snapshot_copy_local_limit)};
    if(count > table_count || table_count > static_cast<::std::size_t>((::std::numeric_limits<unsigned>::max)()))
    { return false; }
    auto& context{state.llvm_function->getContext()};
    for(::std::size_t i{}; i != table_count; ++i)
    {
        auto const pointer{state.local_pointers.index_unchecked(i)};
        auto const type{get_llvm_type_from_wasm_value_type(context,state.local_types.index_unchecked(i))};
        if(pointer == nullptr || type == nullptr || pointer->getFunction() != state.llvm_function ||
           !pointer->isStaticAlloca() || pointer->isArrayAllocation() || pointer->getAllocatedType() != type)
        { return false; }
    }
    auto const array{::llvm::ArrayType::get(::llvm::PointerType::getUnqual(context),table_count)};
    auto const heap_owner{::llvm::dyn_cast_or_null<::llvm::IntToPtrInst>(state.checkpoint_observer_workspace)};
    auto const allocation{state.checkpoint_observer_workspace_allocation};
    auto const extent{checkpoint_observer_allocation_extent(state)};
    if(heap_table && (heap_owner == nullptr || allocation == nullptr || extent == nullptr ||
        heap_owner->getFunction() != state.llvm_function || allocation->getFunction() != state.llvm_function ||
        heap_owner->getParent() != ::std::addressof(state.llvm_function->getEntryBlock()) ||
        allocation->getParent() != heap_owner->getParent() || !checkpoint_observer_workspace_result_matches(state,heap_owner->getOperand(0u)) ||
        !allocation->comesBefore(heap_owner) ||
        !::uwvm2::runtime::checkpoint::observer_workspace_fits(table_count,table_count) ||
        extent->getZExtValue() < table_count *
            (::uwvm2::runtime::checkpoint::observer_local_metadata_bytes +
             ::uwvm2::runtime::checkpoint::native_slot_bytes))) { return false; }
    if(state.snapshot_local_pointers != nullptr)
    {
        if(state.snapshot_local_pointers_count != table_count) { return false; }
        if(heap_table)
        {
            auto const table{::llvm::dyn_cast<::llvm::GetElementPtrInst>(state.snapshot_local_pointers)};
            auto const offset{table != nullptr && table->getNumIndices() == 1u ?
                ::llvm::dyn_cast<::llvm::ConstantInt>(table->getOperand(1u)) : nullptr};
            return table != nullptr && table->getFunction() == state.llvm_function && table->getParent() == heap_owner->getParent() && table->isInBounds() &&
                table->getPointerOperand() == heap_owner && table->getSourceElementType()->isIntegerTy(8u) &&
                offset != nullptr && offset->getZExtValue() == table_count * 2u;
        }
        auto const table{::llvm::dyn_cast<::llvm::AllocaInst>(state.snapshot_local_pointers)};
        return table != nullptr && table->getFunction() == state.llvm_function && table->isStaticAlloca() &&
            !table->isArrayAllocation() && table->getAllocatedType() == array;
    }
    // Complete declarations use private storage inside the bounded observer
    // allocation; normal legacy/public prefixes retain a <=256 entry table.
    auto& entry{state.llvm_function->getEntryBlock()};
    ::llvm::IRBuilder<> initialize{context};
    auto const terminator{!entry.empty() && entry.back().isTerminator() ? ::std::addressof(entry.back()) : nullptr};
    if(terminator != nullptr) { initialize.SetInsertPoint(terminator); }
    else { initialize.SetInsertPoint(::std::addressof(entry)); }
    ::llvm::Value* table{};
    if(heap_table)
    {
        table = initialize.CreateInBoundsGEP(initialize.getInt8Ty(),heap_owner,
            initialize.getInt64(table_count * 2u),"debug.private.local.addresses");
    }
    else
    {
        table = create_llvm_jit_entry_block_alloca(*state.ir_builder,array,nullptr,
            get_llvm_string_ref(u8"debug.private.local.addresses"));
    }
    if(table == nullptr) { return false; }
    // Initialize before ordinary/resumed entry dispatch. Reading an address
    // never reads an uninitialized nondefaultable local's value. Heap table
    // alignment follows two N-byte flag ranges and can be just one byte.
    for(::std::size_t i{}; i != table_count; ++i)
    {
        auto const cell{initialize.CreateInBoundsGEP(array,table,{initialize.getInt32(0u),initialize.getInt32(static_cast<unsigned>(i))})};
        initialize.CreateStore(state.local_pointers.index_unchecked(i),cell)->setAlignment(::llvm::Align{1u});
    }
    state.snapshot_local_pointers = table; state.snapshot_local_pointers_count = table_count;
    return true;
}

[[nodiscard]] inline ::llvm::Function* prepare_runtime_local_func_llvm_jit_snapshot_copy(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t count, bool public_locals) noexcept
{
    if(!prepare_runtime_local_func_llvm_jit_snapshot_local_pointers(state,count)) { return nullptr; }
    // Numeric declarations have identical public/raw bit copies. Reuse ONE
    // body for both packets; references retain their separate nullness/raw ABI.
    bool shared_numeric{true};
    for(::std::size_t i{}; i != count; ++i)
    {
        auto const kind{get_runtime_wasm_value_type_encoding(state.local_types.index_unchecked(i))};
        if(kind != get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type::i32) &&
           kind != get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type::i64) &&
           kind != get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type::f32) &&
           kind != get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type::f64) &&
           kind != get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type::v128))
        { shared_numeric = false; break; }
    }
    auto& cached{public_locals ? state.debug_local_snapshot_copy : state.checkpoint_local_snapshot_copy};
    auto& cached_count{public_locals ? state.debug_local_snapshot_copy_count : state.checkpoint_local_snapshot_copy_count};
    auto& context{state.llvm_function->getContext()};

    // Only tiny numeric observer copies are cloned. References stay shared. Keep large declarations and
    // chunks shared so opcode count cannot multiply their lowering footprint.
#if defined(__linux__) && defined(__x86_64__)
    bool const tiny{state.emit_debug_safe_points && state.debug_activation_enabled &&
        checkpoint_observer_workspace_selected(state) && shared_numeric && count <= 4u};
#else
    constexpr bool tiny{false};
#endif
    auto const copy_attribute{tiny ? ::llvm::Attribute::AlwaysInline : ::llvm::Attribute::NoInline};
    auto const expected_kind{shared_numeric ? "numeric" : (public_locals ? "public-reference" : "raw-reference")};
    auto const pointer_type{::llvm::PointerType::getUnqual(context)};
    auto const signature{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context),
        {pointer_type,pointer_type,pointer_type,pointer_type},false)};
    if(cached != nullptr)
    {
        return cached_count == count && cached->getParent() == state.llvm_module && cached->getFunctionType() == signature &&
               cached->hasLocalLinkage() && cached->hasFnAttribute(copy_attribute) &&
               cached->hasFnAttribute("uwvm.debug.private.local-copy") && cached->getSubprogram() == nullptr &&
               cached->hasFnAttribute("uwvm.debug.private.local-copy.kind") &&
               cached->getFnAttribute("uwvm.debug.private.local-copy.kind").getValueAsString() == expected_kind ?
               cached : nullptr;
    }
    // Validate every physical byte extent before creating any function/body.
    // The C++ compiler ABI is not a semantic reference/nullability oracle.
    for(::std::size_t i{}; i != count; ++i)
    {
        auto const width{get_runtime_wasm_value_type_abi_size(state.local_types.index_unchecked(i))};
        if(width == 0u || width > ::uwvm2::runtime::lib::details::llvm_jit_debug_local_slot_bytes) { return nullptr; }
    }
    // Materialize while every child Twine is alive; a stored concatenation
    // otherwise retains references to temporaries from this full expression.
    auto const name{(::llvm::Twine{state.llvm_function->getName()} +
        (shared_numeric ? ".uwvm.private.numeric.locals" :
            (public_locals ? ".uwvm.private.debug.locals" : ".uwvm.private.checkpoint.locals")) +
        ".count." + ::llvm::Twine{static_cast<::std::uint64_t>(count)}).str()};
    auto const function{::llvm::Function::Create(signature,::llvm::GlobalValue::InternalLinkage,name,state.llvm_module)};
    if(function == nullptr) { return nullptr; }
    function->addFnAttr(copy_attribute); function->setDoesNotThrow();
    function->addFnAttr("uwvm.debug.private.local-copy");
    function->addFnAttr("uwvm.debug.private.local-copy.kind",expected_kind);
    // No DISubprogram, Wasm provenance, activation or safepoint is attached:
    // instrumentation bodies must remain outside public ASM stepping ranges.
    ::llvm::IRBuilder<> copy{::llvm::BasicBlock::Create(context,"entry",function)};
    auto const values{function->getArg(0u)}, saved_flags{function->getArg(1u)};
    auto const addresses{function->getArg(2u)}, available_flags{function->getArg(3u)};
    auto const table_type{::llvm::ArrayType::get(pointer_type,state.snapshot_local_pointers_count)};
    auto const clear_type{::llvm::ArrayType::get(copy.getInt8Ty(),
        ::uwvm2::runtime::lib::details::llvm_jit_debug_local_slot_bytes)};
    // A false flag selects this immutable zero storage BEFORE the value load.
    // Entry-owned table cells contain addresses even for uninitialized locals;
    // reading an address does not read that local's value. Each leaf is straight-line:
    // no absent/invalid flag can cause an absent value to be dereferenced.
    auto const zero{new ::llvm::GlobalVariable(*state.llvm_module,clear_type,true,
        ::llvm::GlobalValue::PrivateLinkage,::llvm::ConstantAggregateZero::get(clear_type),
        ::llvm::Twine{function->getName()} + ".zero")};
    zero->setUnnamedAddr(::llvm::GlobalValue::UnnamedAddr::Global); zero->setAlignment(::llvm::Align{1u});
    auto const emit_range{[&](::llvm::IRBuilder<>& copy, ::llvm::Function* owner,
        ::std::size_t first, ::std::size_t last) noexcept
    {
        auto const values{owner->getArg(0u)}, saved_flags{owner->getArg(1u)};
        auto const addresses{owner->getArg(2u)}, available_flags{owner->getArg(3u)};
        for(::std::size_t i{first}; i != last; ++i)
        {
            auto const flag_cell{copy.CreateInBoundsGEP(copy.getInt8Ty(),available_flags,copy.getInt64(i))};
            auto const flag{copy.CreateLoad(copy.getInt8Ty(),flag_cell)}; flag->setAlignment(::llvm::Align{1u});
            auto const saved_cell{copy.CreateInBoundsGEP(copy.getInt8Ty(),saved_flags,copy.getInt64(i))};
            copy.CreateStore(flag,saved_cell)->setAlignment(::llvm::Align{1u});
            auto const slot{copy.CreateInBoundsGEP(copy.getInt8Ty(),values,
                copy.getInt64(i * ::uwvm2::runtime::lib::details::llvm_jit_debug_local_slot_bytes))};
            auto const available{copy.CreateICmpEQ(flag,copy.getInt8(1u))};
            auto const address_cell{copy.CreateInBoundsGEP(table_type,addresses,{copy.getInt32(0u),copy.getInt32(static_cast<unsigned>(i))})};
            auto const address{copy.CreateLoad(pointer_type,address_cell)}; address->setAlignment(::llvm::Align{1u});
            auto const readable_address{copy.CreateSelect(available,address,zero)};
            auto const local_type{state.local_types.index_unchecked(i)};
            auto const native_type{get_llvm_type_from_wasm_value_type(context,local_type)};
            auto const kind{get_runtime_wasm_value_type_encoding(local_type)};
            bool const numeric{kind == 0x7fu || kind == 0x7eu || kind == 0x7du || kind == 0x7cu || kind == 0x7bu};
            if(numeric)
            {
                // Load ONLY the declared carrier's real extent. Integer bit
                // carriers preserve sNaNs without a float conversion or ABI.
                // One complete store replaces clear-16 + copy-4/8; absent
                // flags still select immutable zero bytes before the load.
                auto const width{get_runtime_wasm_value_type_abi_size(local_type)};
                auto const bits{copy.CreateLoad(copy.getIntNTy(static_cast<unsigned>(width*CHAR_BIT)),readable_address)};
                bits->setAlignment(::llvm::Align{1u});
                ::llvm::Value* packed{width == 16u ? bits : copy.CreateZExt(bits,copy.getInt128Ty())};
                if(width != 16u && state.llvm_module->getDataLayout().isBigEndian())
                { packed = copy.CreateShl(packed,::llvm::ConstantInt::get(copy.getInt128Ty(),(16u-width)*CHAR_BIT)); }
                copy.CreateStore(packed,slot)->setAlignment(::llvm::Align{1u});
                continue;
            }
            copy.CreateStore(::llvm::ConstantAggregateZero::get(clear_type),slot)->setAlignment(::llvm::Align{1u});
            if(public_locals && native_type->isIntegerTy() &&
               (kind == get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type::funcref) ||
                kind == get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type::externref)))
            {
                auto const value{copy.CreateLoad(native_type,readable_address)}; value->setAlignment(::llvm::Align{1u});
                auto const is_null{copy.CreateICmpEQ(value,::llvm::ConstantInt::get(native_type,0u))};
                copy.CreateStore(copy.CreateZExt(copy.CreateAnd(available,is_null),copy.getInt8Ty()),slot)->setAlignment(::llvm::Align{1u});
            }
            else
            {
                // Inline byte copies preserve scalar/v128 bits, including sNaNs.
                // No float argument/return or external memcpy call crosses an ABI.
                copy.CreateMemCpyInline(slot,::llvm::Align{1u},readable_address,::llvm::Align{1u},
                    copy.getInt64(get_runtime_wasm_value_type_abi_size(local_type)));
            }
        }
    }};
    if(count <= llvm_jit_snapshot_copy_local_limit)
    { emit_range(copy,function,0u,count); }
    else
    {
        // Bound each private copy block's lowering work, including at O0.
        // Entry capture and packet clearing must be bounded separately.
        // Each private leaf uses original indices and copies real live bytes;
        // the per-opcode caller still has just one cached wrapper invocation.
        for(::std::size_t first{}; first != count;)
        {
            auto const last{first + (::std::min)(count-first,llvm_jit_snapshot_copy_local_limit)};
            auto const part{::llvm::Function::Create(signature,::llvm::GlobalValue::InternalLinkage,
                ::llvm::Twine{function->getName()} + ".chunk." + ::llvm::Twine{static_cast<::std::uint64_t>(first)},state.llvm_module)};
            part->addFnAttr(::llvm::Attribute::NoInline); part->addFnAttr(::llvm::Attribute::NoMerge); part->setDoesNotThrow();
            part->addFnAttr("uwvm.debug.private.local-copy");
            part->addFnAttr("uwvm.debug.private.local-copy.kind",expected_kind);
            ::llvm::IRBuilder<> chunk{::llvm::BasicBlock::Create(context,"entry",part)};
            emit_range(chunk,part,first,last); chunk.CreateRetVoid();
            copy.CreateCall(part,{values,saved_flags,addresses,available_flags})->setDoesNotThrow();
            first = last;
        }
    }
    copy.CreateRetVoid(); cached = function; cached_count = count;
    auto const public_count{(::std::min)(state.local_types.size(),llvm_jit_snapshot_copy_local_limit)};
    // Sharing is safe only if the public prefix and complete packet have the
    // same extent. A 256-slot destination must NEVER receive a 10000-slot copy.
    if(shared_numeric && public_count == state.local_types.size() && count == public_count)
    {
        state.debug_local_snapshot_copy = function; state.debug_local_snapshot_copy_count = count;
        state.checkpoint_local_snapshot_copy = function; state.checkpoint_local_snapshot_copy_count = count;
    }
    return function;
}

[[nodiscard]] inline bool emit_runtime_local_func_llvm_jit_snapshot_copy(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t count,
    ::llvm::Value* values, ::llvm::Value* saved_flags, ::llvm::Value* available_flags,
    bool public_locals, bool retain_canonical_scalars = false,
    ::uwvm2::runtime::checkpoint::safepoint_layout const* initial_entry = nullptr) noexcept
{
    if(initial_entry != nullptr)
    {
        namespace cp = ::uwvm2::runtime::checkpoint;
        auto const plan{state.checkpoint_plan};
        // Only the actual initial producer can use declaration-time flags.
        // Later opcode/call/restore captures must read their executed flags.
        if(public_locals || retain_canonical_scalars || !checkpoint_observer_workspace_selected(state) ||
           plan->producer_availability != cp::status::ok || plan->compiler_failure != cp::status::ok ||
           state.checkpoint_observer_controls != nullptr || plan->sites.size() != 1u ||
           initial_entry != ::std::addressof(plan->sites.front()) ||
           initial_entry->phase != cp::frame_phase::before_opcode || initial_entry->local_count != count ||
           initial_entry->slots.size() != count || initial_entry->operand_count != 0u ||
           initial_entry->saved_parameter_count != 0u || available_flags != state.checkpoint_observer_workspace ||
           cp::validate_site(*initial_entry,*plan) != cp::status::ok) { return false; }
    }
    if(retain_canonical_scalars && (public_locals || !state.checkpoint_observer_canonical_numeric_locals ||
       count != state.local_types.size() || !checkpoint_canonical_local_owner(state))) { return false; }
    if(retain_canonical_scalars)
    {
        auto const exact_prefix{[&](::llvm::Value* pointer, ::std::size_t offset) noexcept
        {
            auto const cell{::llvm::dyn_cast_or_null<::llvm::GetElementPtrInst>(pointer)};
            auto const index{cell != nullptr && cell->getNumIndices() == 1u ?
                ::llvm::dyn_cast<::llvm::ConstantInt>(cell->getOperand(1u)) : nullptr};
            return cell != nullptr && cell->getFunction() == state.llvm_function && cell->isInBounds() &&
                cell->getSourceElementType()->isIntegerTy(8u) && cell->getPointerOperand() == state.checkpoint_observer_workspace &&
                index != nullptr && index->getZExtValue() == offset;
        }};
        if(available_flags != state.checkpoint_observer_workspace ||
           !exact_prefix(values,count*::uwvm2::runtime::checkpoint::observer_local_metadata_bytes) ||
           !exact_prefix(saved_flags,count)) { return false; }
    }
    if(values == nullptr || saved_flags == nullptr || available_flags == nullptr ||
       !values->getType()->isPointerTy() || !saved_flags->getType()->isPointerTy() || !available_flags->getType()->isPointerTy())
    { return false; }
#if defined(__linux__) && defined(__x86_64__)
    // Small complete mixed observation prefixes need no address table or
    // private copier call. Every capture still writes fresh flags and ALL
    // sixteen bytes. Numeric locals may remain nonescaping; reference locals
    // retain an initialization-selected address and are never marked numeric.
    // Entry capture precedes binding the opcode control map. Its immutable
    // observer profile already selects this owned workspace; an address table
    // there would escape every local before later inline captures can promote
    // them. The same extent/entry-owner/type checks apply to both captures.
    // Large/non-observer/other-target copies retain the bounded copier below.
    auto const heap_owner{::llvm::dyn_cast_or_null<::llvm::IntToPtrInst>(state.checkpoint_observer_workspace)};
    auto const allocation{state.checkpoint_observer_workspace_allocation};
    auto const extent{checkpoint_observer_allocation_extent(state)};
    bool inline_small{(state.checkpoint_observer_controls != nullptr || checkpoint_observer_workspace_selected(state)) &&
        heap_owner != nullptr &&
        allocation != nullptr && extent != nullptr && state.llvm_function != nullptr &&
        state.llvm_module != nullptr && state.ir_builder != nullptr && count != 0u && count <= 8u &&
        count == state.local_types.size() && count == state.local_pointers.size() &&
        state.llvm_function->getParent() == state.llvm_module &&
        state.ir_builder->GetInsertBlock() != nullptr && state.ir_builder->GetInsertBlock()->getParent() == state.llvm_function &&
        heap_owner->getFunction() == state.llvm_function && allocation->getFunction() == state.llvm_function &&
        heap_owner->getParent() == ::std::addressof(state.llvm_function->getEntryBlock()) &&
        allocation->getParent() == heap_owner->getParent() && checkpoint_observer_workspace_result_matches(state,heap_owner->getOperand(0u)) &&
        allocation->comesBefore(heap_owner) &&
        ::uwvm2::runtime::checkpoint::observer_workspace_fits(count,count) &&
        extent->getZExtValue() >= count * (::uwvm2::runtime::checkpoint::observer_local_metadata_bytes + 16u) &&
        !state.llvm_module->getDataLayout().isDefault() && state.llvm_module->getDataLayout().isLittleEndian() &&
        state.llvm_module->getDataLayout().getPointerSizeInBits() == 64u};
    bool has_reference{}, has_non_gp{};
    for(::std::size_t i{}; inline_small && i != count; ++i)
    {
        auto const kind{get_runtime_wasm_value_type_encoding(state.local_types.index_unchecked(i))};
        auto const type{get_llvm_type_from_wasm_value_type(state.llvm_function->getContext(),state.local_types.index_unchecked(i))};
        auto const local{::llvm::dyn_cast_or_null<::llvm::AllocaInst>(state.local_pointers.index_unchecked(i))};
        bool const numeric{kind == 0x7fu || kind == 0x7eu || kind == 0x7du || kind == 0x7cu || kind == 0x7bu};
        auto const width{get_runtime_wasm_value_type_abi_size(state.local_types.index_unchecked(i))};
        bool const reference{(kind == 0x70u || kind == 0x6fu) && type != nullptr && type->isIntegerTy() &&
            width != 0u && width <= 16u && type->getIntegerBitWidth() == width * CHAR_BIT};
        inline_small = (numeric || reference) &&
            local != nullptr && local->getFunction() == state.llvm_function && local->isStaticAlloca() &&
            !local->isArrayAllocation() && local->getAllocatedType() == type;
        if(reference) { has_reference = true; }
        has_non_gp = has_non_gp || reference || kind == 0x7du || kind == 0x7cu || kind == 0x7bu;
    }
    // The existing tiny GP entry copier already folds its initial flags and
    // retains the original argument storage. Keep that native layout for
    // call-heavy scalar functions; mixed/FP/vector prefixes benefit from
    // removing their outlined entry copy and address table.
    if(initial_entry != nullptr && !has_non_gp) { inline_small = false; }
    if(inline_small)
    {
        auto& copy{*state.ir_builder};
        auto const clear_type{copy.getInt128Ty()};
        if(has_reference)
        {
            auto& zero{state.debug_inline_snapshot_zero};
            if(zero == nullptr)
            {
                zero = new ::llvm::GlobalVariable(*state.llvm_module,clear_type,true,
                    ::llvm::GlobalValue::PrivateLinkage,::llvm::ConstantInt::get(clear_type,0u),
                    ::llvm::Twine{state.llvm_function->getName()} + ".uwvm.private.inline.local.zero");
                zero->setUnnamedAddr(::llvm::GlobalValue::UnnamedAddr::Global);zero->setAlignment(::llvm::Align{1u});
            }
            if(zero->getParent()!=state.llvm_module || zero->getValueType()!=clear_type ||
               !zero->hasPrivateLinkage() || !zero->isConstant() || !zero->hasInitializer() ||
               !zero->getInitializer()->isNullValue()) { return false; }
        }
        for(::std::size_t i{}; i != count; ++i)
        {
            if(retain_canonical_scalars && checkpoint_canonical_scalar(state.local_types.index_unchecked(i))) { continue; }
            ::llvm::Value* flag{};
            if(initial_entry != nullptr)
            {
                // Real entry initialization already checked parameters and
                // nullable/defaultable declarations and stored these flags.
                // No host call intervenes; keep absent references unreadable.
                flag = copy.getInt8(initial_entry->slots[i].initialized ? 1u : 0u);
            }
            else
            {
                auto const actual{copy.CreateLoad(copy.getInt8Ty(),copy.CreateInBoundsGEP(copy.getInt8Ty(),available_flags,copy.getInt64(i)))};
                actual->setAlignment(::llvm::Align{1u});flag = actual;
            }
            copy.CreateStore(flag,copy.CreateInBoundsGEP(copy.getInt8Ty(),saved_flags,copy.getInt64(i)))->setAlignment(::llvm::Align{1u});
            auto const type{get_llvm_type_from_wasm_value_type(copy.getContext(),state.local_types.index_unchecked(i))};
            auto const bits_type{copy.getIntNTy(static_cast<unsigned>(get_runtime_wasm_value_type_abi_size(state.local_types.index_unchecked(i))*8u))};
            auto const kind{get_runtime_wasm_value_type_encoding(state.local_types.index_unchecked(i))};
            bool const numeric{kind == 0x7fu || kind == 0x7eu || kind == 0x7du || kind == 0x7cu || kind == 0x7bu};
            auto const available{copy.CreateICmpEQ(flag,copy.getInt8(1u))};
            auto const local{state.local_pointers.index_unchecked(i)};
            // Numeric declarations are initialized at real entry. References
            // may be nondefaultable: choose immutable zero storage BEFORE any
            // value load. An invalid flag is preserved for the runtime to reject.
            if(numeric)
            {
                local->setMetadata("uwvm.debug.numeric.local",::llvm::MDNode::get(copy.getContext(),{}));
                // Actual GP arguments must survive the real activation-entry
                // call. Keeping their original stack cell avoids introducing
                // another callee-saved register in tiny call-heavy functions.
                // Other declarations/internal temporaries still admit SSA.
                if(initial_entry != nullptr && (kind == 0x7fu || kind == 0x7eu) &&
                   local->getMetadata("uwvm.debug.numeric.mutable.gp") != nullptr)
                { local->setMetadata("uwvm.debug.entry.stack.gp",::llvm::MDNode::get(copy.getContext(),{})); }
            }
            auto const readable{numeric ? static_cast<::llvm::Value*>(local) :
                copy.CreateSelect(available,local,state.debug_inline_snapshot_zero)};
            if(!numeric && !public_locals && bits_type->isIntegerTy(128u))
            {
                // Raw references are sixteen physical bytes, not a C++/Wasm
                // vector value. One unaligned integer-vector move avoids i128
                // scalar expansion at debug codegen levels. The selected
                // address already yields sixteen zero bytes for flag != 1;
                // a second value-select would only duplicate that clearing.
                // Every capture still refreshes payload, tag and padding.
                auto const carrier{::llvm::FixedVectorType::get(copy.getInt64Ty(),2u)};
                auto const bytes{copy.CreateLoad(carrier,readable,"debug.reference.bytes")};
                bytes->setAlignment(::llvm::Align{1u});
                copy.CreateStore(bytes,copy.CreateInBoundsGEP(copy.getInt8Ty(),values,copy.getInt64(i*16u)))->setAlignment(::llvm::Align{1u});
                continue;
            }
            auto const live{copy.CreateLoad(type,readable)};live->setAlignment(::llvm::Align{1u});
            auto const bits{copy.CreateBitCast(live,bits_type)};
            ::llvm::Value* whole{copy.CreateZExtOrBitCast(bits,copy.getInt128Ty())};
            if(!numeric && public_locals)
            {
                auto const is_null{copy.CreateICmpEQ(live,::llvm::ConstantInt::get(type,0u))};
                whole = copy.CreateZExt(copy.CreateAnd(available,is_null),copy.getInt128Ty());
            }
            auto const saved{copy.CreateSelect(available,whole,::llvm::ConstantInt::get(copy.getInt128Ty(),0u))};
            copy.CreateStore(saved,copy.CreateInBoundsGEP(copy.getInt8Ty(),values,copy.getInt64(i*16u)))->setAlignment(::llvm::Align{1u});
        }
        return true;
    }
#endif
    if(retain_canonical_scalars) { return false; } // retained prefixes require the fully checked inline owner above
    auto const function{prepare_runtime_local_func_llvm_jit_snapshot_copy(state,count,public_locals)};
    if(function == nullptr) { return false; }
    auto const call{state.ir_builder->CreateCall(function,{values,saved_flags,state.snapshot_local_pointers,available_flags})};
    if(call == nullptr) { return false; }
    call->setDoesNotThrow();
    if(function->hasFnAttribute(::llvm::Attribute::AlwaysInline))
    {
        // This private copier contains only the same flag/value/padding stores.
        // InlineFunction preserves its absent-address select BEFORE each load;
        // it never inlines a guest/native bridge or removes an observation site.
        // Keep an explicit continuation as the builder's insertion owner:
        // cloning can split the old block and erase the original call iterator.
        auto const continuation{::llvm::BasicBlock::Create(state.ir_builder->getContext(),
            "debug.tiny.copy.continue",state.llvm_function)};
        state.ir_builder->CreateBr(continuation);
        state.ir_builder->SetInsertPoint(continuation);
        ::llvm::InlineFunctionInfo information{};
        if(!::llvm::InlineFunction(*call,information).isSuccess()) { return false; }
    }
    return true;
}

[[nodiscard]] inline ::llvm::GlobalVariable* prepare_runtime_local_func_llvm_jit_debug_snapshot_flags(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t count) noexcept
{
    if(state.ir_builder == nullptr || state.llvm_module == nullptr || count == 0u ||
       count > llvm_jit_snapshot_copy_local_limit || count > state.local_types.size() ||
       state.debug_local_initialization.context == nullptr || state.debug_local_initialization.initialized == nullptr)
    { return nullptr; }
    ::llvm::SmallVector<::std::uint8_t,llvm_jit_snapshot_copy_local_limit> flags{};
    for(::std::size_t i{}; i != count; ++i)
    {
        bool initialized{};
        if(!state.debug_local_initialization.initialized(state.debug_local_initialization.context,i,initialized)) { return nullptr; }
        flags.push_back(initialized ? 1u : 0u);
    }
    auto const bytes{::llvm::ConstantDataArray::get(state.ir_builder->getContext(),flags)};
    auto const found{state.debug_local_snapshot_flags.find(bytes)};
    if(found != state.debug_local_snapshot_flags.end()) { return found->second; }
    auto const global{new ::llvm::GlobalVariable(*state.llvm_module,bytes->getType(),true,
        ::llvm::GlobalValue::PrivateLinkage,bytes,"uwvm.private.debug.local.flags")};
    global->setUnnamedAddr(::llvm::GlobalValue::UnnamedAddr::Global); global->setAlignment(::llvm::Align{1u});
    state.debug_local_snapshot_flags[bytes] = global; return global;
}
