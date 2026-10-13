// The scalar ABI transports an opaque reference token and returns length/status
// in registers. Authentication and foreign-owner retention remain synchronous;
// no object address or lifetime proof escapes this call.
[[nodiscard]] inline ::std::uint64_t llvm_jit_gc_array_length32_bridge(
    ::std::uintptr_t module_address, ::std::uint32_t reference_kind,
    ::std::uintptr_t opaque_payload) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    using status = storage::gc_object_status;
    static_assert(sizeof(status) == sizeof(::std::uint32_t));
    auto const failure{[](status value) noexcept
    { return static_cast<::std::uint64_t>(value) << 32u; }};
    if(module_address == 0u) { return failure(status::invalid_value); }
    // Relocation supplies the actual, retained native module, never Wasm memory.
    auto const* module{reinterpret_cast<storage::wasm_module_storage_t const*>(module_address)};
    auto* store{module->gc_store.get()};
    if(store == nullptr || !store->valid()) { return failure(status::invalid_store); }
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    ::uwvm2::runtime::gc::managed_page_boundary(false);
#endif
    storage::gc_reference reference{};
    reference.kind = static_cast<::uwvm2::object::global::wasm_ref_kind>(reference_kind);
    // This is the original token representation, not a dereferenceable object.
    reference.storage.ptr = reinterpret_cast<void*>(opaque_payload);
    ::std::size_t length{};
    auto const outcome{store->array_length(reference, length)};
    if(outcome != status::ok) { return failure(outcome); }
    // Keep the original generic bridge's representability/status contract.
    if(length > (::std::numeric_limits<::std::uint32_t>::max)())
    { return failure(status::size_overflow); }
    return static_cast<::std::uint32_t>(length);
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_array_length32(
    runtime_local_func_llvm_jit_emit_state_t& state, bool use_logical_length = true) noexcept
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
       state.local_func_storage_ptr == nullptr || state.operand_stack.empty()) { return false; }
    auto& builder{*state.ir_builder};
    auto& stack{state.operand_stack};
    // Copy the complete descriptor before changing the vector.
    auto const operand{stack.back()};
    if(operand.type != wasm_type::funcref || operand.value == nullptr ||
       !operand.value->getType()->isIntegerTy(static_cast<unsigned>(bytes * CHAR_BIT))) { return false; }
    auto const* module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr || !module->gc_store || !module->gc_store->valid()) { return false; }
    auto const& layout{state.llvm_module->getDataLayout()};
    if(state.llvm_module->getDataLayoutStr().empty() ||
       layout.getPointerSize() != sizeof(::std::uintptr_t) ||
       layout.isLittleEndian() != (::std::endian::native == ::std::endian::little)) { return false; }
#if defined(UWVM_EXPERIMENTAL_ARRAY_LENGTH_VALUES) && UWVM_EXPERIMENTAL_ARRAY_LENGTH_VALUES == 1
    if(use_logical_length)
    {
        auto const fact{llvm_jit_find_array_length_value(state, operand.value)};
        ::llvm::Value* known_length{fact.length};
        ::llvm::Value* known_valid{fact.valid};
        if(known_length != nullptr && known_valid != nullptr)
        {
            auto const known{::llvm::BasicBlock::Create(builder.getContext(),
                get_llvm_string_ref(u8"gc.array.len.known"), state.llvm_function)};
            auto const checked{::llvm::BasicBlock::Create(builder.getContext(),
                get_llvm_string_ref(u8"gc.array.len.checked"), state.llvm_function)};
            auto const merge{::llvm::BasicBlock::Create(builder.getContext(),
                get_llvm_string_ref(u8"gc.array.len.merge"), state.llvm_function)};
            builder.CreateCondBr(known_valid, known, checked);
            builder.SetInsertPoint(known);
            builder.CreateBr(merge);
            builder.SetInsertPoint(checked);
            if(!try_emit_runtime_local_func_llvm_jit_gc_array_length32(state, false)) { return false; }
            auto const checked_value{stack.back().value};
            stack.pop_back();
            auto const checked_success{builder.GetInsertBlock()};
            builder.CreateBr(merge);
            builder.SetInsertPoint(merge);
            auto const phi{builder.CreatePHI(builder.getInt32Ty(), 2u,
                get_llvm_string_ref(u8"gc.array.len.value"))};
            phi->addIncoming(known_length, known);
            phi->addIncoming(checked_value, checked_success);
            stack.push_back({.type = wasm_type::i32, .value = phi});
            return true;
        }
    }
#else
    static_cast<void>(use_logical_length);
#endif
    llvm_jit_gc_status_dispatch_plan status_dispatch{};
    if(!status_dispatch.prepare(*state.llvm_module, builder)) { return false; }
    auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const i32{builder.getInt32Ty()};
    auto const i64{builder.getInt64Ty()};
    constexpr bool little_endian{::std::endian::native == ::std::endian::little};
    constexpr auto payload_shift{static_cast<unsigned>((little_endian ? payload_offset :
        bytes - payload_offset - payload_bytes) * CHAR_BIT)};
    constexpr auto kind_shift{static_cast<unsigned>((little_endian ? kind_offset :
        bytes - kind_offset - kind_bytes) * CHAR_BIT)};
    auto const payload{builder.CreateTrunc(builder.CreateLShr(operand.value, payload_shift),
        intptr, get_llvm_string_ref(u8"gc.array.len.payload"))};
    auto const kind{builder.CreateTrunc(builder.CreateLShr(operand.value, kind_shift),
        i32, get_llvm_string_ref(u8"gc.array.len.kind"))};
    auto const module_name{get_llvm_runtime_module_object_symbol_name(*module)};
    auto const module_address{get_llvm_external_host_object_address(builder,
        reinterpret_cast<::std::uintptr_t>(module),
        ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()})};
    if(module_address == nullptr) { return false; }
    auto const bridge_type{::llvm::FunctionType::get(i64, {intptr, i32, intptr}, false)};
    auto const callee{get_llvm_runtime_bridge_function_symbol_value<llvm_jit_gc_array_length32_bridge>(
        builder, bridge_type, ::uwvm2::utils::container::u8string_view{u8"gc_array_length32_v1"})};
    if(callee == nullptr) { return false; }
    ::llvm::Value* arguments[]{module_address, kind, payload};
    auto const packed{apply_llvm_jit_host_calling_conv(builder.CreateCall(bridge_type, callee, arguments))};
    packed->setDoesNotThrow();
    // Do not mark readonly/readnone: object authentication can retain a foreign
    // owner and update native metadata. Keep all original status-to-trap edges.
    auto const status{builder.CreateTrunc(builder.CreateLShr(packed, 32u), i32,
        get_llvm_string_ref(u8"gc.array.len.status"))};
    status_dispatch.commit(*status);
    auto const length{builder.CreateTrunc(packed, i32, get_llvm_string_ref(u8"gc.array.len.i32"))};
    stack.pop_back();
    stack.push_back({.type = wasm_type::i32, .value = length});
    return true;
}
