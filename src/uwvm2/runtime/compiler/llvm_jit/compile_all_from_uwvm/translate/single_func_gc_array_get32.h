// Included inside the translator namespace only for the exact numeric-array
// experiment. The native integer ABI carries an opaque token, not a pointer to
// a GC object or a guest byte buffer. Generic aggregate operations are intact.
template<bool SignExtend>
[[nodiscard]] inline ::std::uint64_t llvm_jit_gc_array_get32_bridge(
    ::std::uintptr_t module_address, ::std::uint32_t reference_kind,
    ::std::uintptr_t opaque_payload, ::std::uint32_t element_index) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    using status = storage::gc_object_status;
    static_assert(sizeof(status) == sizeof(::std::uint32_t));
    auto const failure{[](status value) noexcept
    { return static_cast<::std::uint64_t>(value) << 32u; }};
    if(module_address == 0u) { return failure(status::invalid_value); }
    // [actual compiler-bound module object][synchronous native borrow]
    // [safe ] JIT relocation binds the owned native instance; a Wasm linear
    // memory address cannot supply this argument or extend its lifetime.
    auto const* module{reinterpret_cast<storage::wasm_module_storage_t const*>(module_address)};
    auto* store{module->gc_store.get()};
    if(store == nullptr || !store->valid()) { return failure(status::invalid_store); }
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    // Arrays never use the provisional numeric-struct page format. Preserve
    // the original generic-array boundary before object/foreign membership.
    ::uwvm2::runtime::gc::managed_page_boundary(false);
#endif
    storage::gc_reference reference{};
    reference.kind = static_cast<::uwvm2::object::global::wasm_ref_kind>(reference_kind);
    // [opaque non-recycled token][no dereference or pointer arithmetic]
    // [safe ] assign only its original native representation; checked_object
    // must prove actual membership, kind and owner before any array load.
    reference.storage.ptr = reinterpret_cast<void*>(opaque_payload);
    return store->template array_get32<SignExtend>(reference, element_index);
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_array_get32(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded,
    runtime_operand_stack_value_type result_type) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
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
       state.local_func_storage_ptr == nullptr || state.operand_stack.size() < 2uz ||
       decoded.opcode < 11u || decoded.opcode > 13u ||
       (result_type != wasm_type::i32 && result_type != wasm_type::f32)) { return false; }
    auto& builder{*state.ir_builder};
    auto& stack{state.operand_stack};
    // [live SSA prefix][array reference][i32 index] end
    // [safe ] size>=2 precedes both subscripts. Copy descriptors before any
    // vector mutation; no pointer into the operand vector is retained.
    auto const operand{stack[stack.size() - 2uz]};
    auto const index{stack.back()};
    if(operand.type != wasm_type::funcref || operand.value == nullptr ||
       !operand.value->getType()->isIntegerTy(static_cast<unsigned>(bytes * CHAR_BIT)) ||
       index.type != wasm_type::i32 || index.value == nullptr ||
       !index.value->getType()->isIntegerTy(32u)) { return false; }
    auto const* module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr || !module->gc_store || !module->gc_store->valid()) { return false; }
    auto const* field{module->gc_store->field_at(decoded.first, 0uz)};
    if(field == nullptr) { return false; }
    namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
    if(result_type == wasm_type::f32)
    {
        if(field->storage.packed != type::packed_kind::none ||
           field->storage.value.kind != type::value_kind::f32 || decoded.opcode != 11u) { return false; }
    }
    else if(field->storage.value.kind != type::value_kind::i32 ||
            (field->storage.packed == type::packed_kind::none ? decoded.opcode != 11u :
             (field->storage.packed != type::packed_kind::i8 &&
              field->storage.packed != type::packed_kind::i16) || decoded.opcode == 11u))
    { return false; }
    auto const& layout{state.llvm_module->getDataLayout()};
    // Decline before creating any IR if this is not the actual native carrier
    // layout. The caller checks that declinable layout condition before
    // selecting this path, preserving its generic path for other targets.
    if(state.llvm_module->getDataLayoutStr().empty() ||
       layout.getPointerSize() != sizeof(::std::uintptr_t) ||
       layout.isLittleEndian() != (::std::endian::native == ::std::endian::little)) { return false; }
    auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const i32{builder.getInt32Ty()};
    auto const i64{builder.getInt64Ty()};
    constexpr bool little_endian{::std::endian::native == ::std::endian::little};
    constexpr auto payload_shift{static_cast<unsigned>((little_endian ? payload_offset :
        bytes - payload_offset - payload_bytes) * CHAR_BIT)};
    constexpr auto kind_shift{static_cast<unsigned>((little_endian ? kind_offset :
        bytes - kind_offset - kind_bytes) * CHAR_BIT)};
    // [complete native reference SSA integer][two proved fields]
    // [safe ] shift/truncate the actual native layout; no object address is
    // formed and native padding never becomes lookup authority.
    auto const payload{builder.CreateTrunc(builder.CreateLShr(operand.value, payload_shift),
        intptr, get_llvm_string_ref(u8"gc.array.get32.payload"))};
    auto const kind{builder.CreateTrunc(builder.CreateLShr(operand.value, kind_shift),
        i32, get_llvm_string_ref(u8"gc.array.get32.kind"))};
    auto const module_name{get_llvm_runtime_module_object_symbol_name(*module)};
    // [owned native module identity][compiler relocation declaration]
    // [safe ] convert only that live instance for the existing host-symbol
    // binding; cached machine code does not embed an unretained store pointer.
    auto const module_address{get_llvm_external_host_object_address(builder,
        reinterpret_cast<::std::uintptr_t>(module),
        ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()})};
    if(module_address == nullptr) { return false; }
    auto const bridge_type{::llvm::FunctionType::get(i64, {intptr, i32, intptr, i32}, false)};
    ::llvm::Value* arguments[]{module_address, kind, payload, index.value};
    auto const call{[&]<bool Signed>() noexcept -> ::llvm::CallInst*
    {
        auto const discriminator{Signed ? ::uwvm2::utils::container::u8string_view{u8"gc_array_get32_signed_v1"} :
            ::uwvm2::utils::container::u8string_view{u8"gc_array_get32_unsigned_v1"}};
        auto const callee{get_llvm_runtime_bridge_function_symbol_value<llvm_jit_gc_array_get32_bridge<Signed>>(
            builder, bridge_type, discriminator)};
        if(callee == nullptr) { return nullptr; }
        // [actual four-integer native ABI][complete live SSA arguments]
        // [safe ] no scratch/native output pointer crosses this call. The
        // synchronous checked getter allocates no GC object and is noexcept.
        auto const value{apply_llvm_jit_host_calling_conv(builder.CreateCall(bridge_type, callee, arguments))};
        value->setDoesNotThrow();
        return value;
    }};
    auto const packed{decoded.opcode == 12u ? call.template operator()<true>() :
        call.template operator()<false>()};
    if(packed == nullptr) { return false; }
    auto const status{builder.CreateTrunc(builder.CreateLShr(packed, 32u), i32,
        get_llvm_string_ref(u8"gc.array.get32.status"))};
    auto const failed{[&](storage::gc_object_status value)
    { return builder.CreateICmpEQ(status,
        ::llvm::ConstantInt::get(i32, static_cast<::std::uint32_t>(value))); }};
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
    auto const raw{builder.CreateTrunc(packed, i32, get_llvm_string_ref(u8"gc.array.get32.bits"))};
    auto const value{result_type == wasm_type::f32 ?
        builder.CreateBitCast(raw, builder.getFloatTy(), get_llvm_string_ref(u8"gc.array.get32.f32")) : raw};
    // [SSA prefix][two input descriptors] end -> [SSA prefix][one scalar]
    // [safe ] every input was copied and the status-success block dominates
    // this scalar. No descriptor pointer or GC byte address survives mutation.
    stack.pop_back();
    stack.pop_back();
    stack.push_back({.type = result_type, .value = value});
    return true;
}
