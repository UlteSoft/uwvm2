// Included inside owned_pending_native_bindings only for the exact opt-in.
// Keep all original eight slots untouched; a separate native declaration is
// captured before EngineBuilder and bound only in that same owned engine.
// [optional borrowed LLVM declaration][engine/module-owned lifetime]
// [safe                                                         ] Never an
// arbitrary host/guest function pointer or a process-wide symbol registration.
::llvm::Function* fused_catch{};

[[nodiscard]] bool capture_fused_catch(::llvm::Module& ir) noexcept
{
    namespace details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
    namespace leaf = ::uwvm2::runtime::exception::pending_experiment::numeric_guest_bridge;
    static_assert(::std::is_same_v<decltype(&leaf::uwvm2_pending_numeric_take_r3), leaf::quaternary_abi>);
    // [actual target/native ABI word Type][context-owned LLVM types]
    // [safe                                                     ] Exactly the
    // original leaf ABI, with four words and one nonthrowing word result.
    auto const word_type{::llvm::Type::getIntNTy(ir.getContext(), sizeof(::std::uintptr_t) *
        ::std::numeric_limits<unsigned char>::digits)};
    ::llvm::Type* parameters[4u]{word_type, word_type, word_type, word_type};
    auto const type{::llvm::FunctionType::get(word_type, {parameters, 4uz}, false)};
    auto const name{details::get_llvm_runtime_bridge_function_symbol_name<
        leaf::uwvm2_pending_numeric_take_r3>(type, {leaf::take_semantic})};
    // [same actual optimized module's named IR value][optional declaration]
    // [safe                                                             ]
    // An alias/global/definition cannot masquerade as an unused native leaf.
    auto const named{ir.getNamedValue(details::get_llvm_string_ref(name))};
    // [reset optional IR borrow][no object destruction or lifetime extension]
    // [safe                                                             ]
    fused_catch = nullptr;
    if(named == nullptr) { return true; } // O3 may remove a genuinely unused leaf.
    auto const function{::llvm::dyn_cast<::llvm::Function>(named)};
    if(function == nullptr || !function->isDeclaration() ||
       function->getParent() != ::std::addressof(ir) ||
       function->getFunctionType() != type ||
       function->getCallingConv() != ::llvm::CallingConv::C ||
       !function->hasExternalLinkage() || function->hasDLLImportStorageClass() ||
       function->hasDLLExportStorageClass()) { return false; }
    // [checked declaration][same module subsequently owned by this engine]
    // [safe                                                           ] No
    // function can execute until all original/new local mappings are complete.
    fused_catch = function;
    return true;
}

[[nodiscard]] bool bind_fused_catch(::llvm::ExecutionEngine& engine) noexcept
{
    namespace leaf = ::uwvm2::runtime::exception::pending_experiment::numeric_guest_bridge;
    if(fused_catch == nullptr) { return true; }
    // [complete native noexcept function][host ABI-qualified actual engine]
    // [safe                                                              ]
    // Convert this one retained native function address without dereference or
    // pointer advance. Never use DynamicLibrary::AddSymbol or a plan address.
    auto const address{reinterpret_cast<void*>(reinterpret_cast<::std::uintptr_t>(
        leaf::uwvm2_pending_numeric_take_r3))};
    engine.addGlobalMapping(fused_catch, address);
    return engine.getPointerToGlobalIfAvailable(fused_catch) == address;
}
