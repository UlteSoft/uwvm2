// Exact native storage bindings for scalar memory in the pending numeric island.
// Included inside owned_pending_native_bindings. Source identity/epoch is
// checked by bind() before these mappings; guest pointers are never authority.
::uwvm2::utils::container::vector<::llvm::GlobalVariable*> numeric_memories{};
template<class Visitor>
[[nodiscard]] bool visit_owned_scalar_memory_symbols(
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module,
    ::llvm::LLVMContext& context, Visitor&& visitor) noexcept
{
    namespace d=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
    if(!module.imported_memory_vec_storage.empty() ||
       module.local_defined_memory_vec_storage.size() >
       (::std::numeric_limits<d::validation_module_traits_t::wasm_u32>::max)()/3u) { return false; }
    auto const prefix{d::get_llvm_runtime_module_symbol_prefix(module)};
    auto const i8{::llvm::Type::getInt8Ty(context)};
    auto const word{::llvm::Type::getIntNTy(context,sizeof(::std::uintptr_t)*CHAR_BIT)};
    for(::std::size_t index{};index != module.local_defined_memory_vec_storage.size();++index)
    {
        auto const memory_index{static_cast<d::validation_module_traits_t::wasm_u32>(index)};
        auto const info{d::resolve_runtime_memory_access_info(module,memory_index)};
        if(info.memory_p == nullptr || info.local_imported_module_ptr != nullptr) { return false; }
        auto const object_name{d::get_llvm_native_memory_object_symbol_name(module,memory_index)};
        if(!visitor(object_name,i8,static_cast<void const*>(info.memory_p))) { return false; }
        auto const begin_name{::uwvm2::utils::container::u8concat_uwvm(prefix,u8"_memory",index,u8"_begin")};
        auto const length_name{::uwvm2::utils::container::u8concat_uwvm(prefix,u8"_memory",index,u8"_length")};
        if constexpr(d::runtime_native_memory_t::can_mmap)
        {
            if(info.stable_memory_reserved_span_bytes == 0uz) { return false; }
            // The LLVM array extent is exactly the retained native reservation,
            // including guards; changing it to a one-byte declaration loses the
            // real object/provenance contract used by the validated emitter.
            auto const span{::llvm::ArrayType::get(i8,info.stable_memory_reserved_span_bytes)};
            if(!visitor(begin_name,span,static_cast<void const*>(info.stable_memory_begin)) ||
               !visitor(length_name,word,static_cast<void const*>(info.stable_memory_length_p))) { return false; }
        }
        else
        {
            if(!visitor(begin_name,d::get_llvm_pointer_type(i8),static_cast<void const*>(info.memory_begin_value_p)) ||
               !visitor(length_name,word,static_cast<void const*>(info.stable_memory_length_value_p))) { return false; }
        }
    }
    return true;
}
[[nodiscard]] static bool valid_owned_memory_declaration(::llvm::GlobalVariable const& global,
    ::llvm::Type const* type) noexcept
{
    return global.isDeclaration() && global.hasExternalLinkage() && !global.isConstant() &&
        !global.isThreadLocal() && !global.hasDLLImportStorageClass() && !global.hasDLLExportStorageClass() &&
        global.getAddressSpace() == 0u && global.getValueType() == type;
}
[[nodiscard]] bool capture_owned_scalar_memories(::llvm::Module& ir,
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module) noexcept
{
    namespace d=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
    numeric_memories.clear();
    return visit_owned_scalar_memory_symbols(module,ir.getContext(),[&](auto const& name,auto* type,void const* address) noexcept
    {
        if(address == nullptr) { return false; }
        auto const named{ir.getNamedValue(d::get_llvm_string_ref(name))};
        auto const global{named == nullptr ? nullptr : ::llvm::dyn_cast<::llvm::GlobalVariable>(named)};
        if(named != nullptr && (global == nullptr || !valid_owned_memory_declaration(*global,type))) { return false; }
        numeric_memories.push_back(global);return true;
    });
}
[[nodiscard]] bool bind_owned_scalar_memories(::llvm::ExecutionEngine& engine) noexcept
{
    namespace d=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
    if(actual_module == nullptr || plan_global == nullptr) { return false; }
    ::std::size_t slot{};
    auto const bound{visit_owned_scalar_memory_symbols(*actual_module,plan_global->getContext(),
        [&](auto const& name,auto* type,void const* address) noexcept
        {
            if(slot >= numeric_memories.size() || address == nullptr) { return false; }
            auto const global{numeric_memories[slot++]};if(global == nullptr) { return true; }
            if(global->getName() != d::get_llvm_string_ref(name) || !valid_owned_memory_declaration(*global,type)) { return false; }
            auto const mapping{const_cast<void*>(address)};
            engine.addGlobalMapping(global,mapping);
            return engine.getPointerToGlobalIfAvailable(global) == mapping;
        })};
    return bound && slot == numeric_memories.size();
}
