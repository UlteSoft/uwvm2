// Included in the lowering details namespace, after the LLVM IR headers.
// A pointer-sized relocation initializes nearby JIT-owned storage. Machine
// instructions address that storage, rather than a possibly far host image.
// The object contains a symbol identity; RuntimeDyld binds its current address
// on every load, including a persistent-cache hit in another process.
[[nodiscard]] inline ::llvm::Value* get_llvm_relocatable_host_symbol_pointer(
    ::llvm::IRBuilder<>& builder, ::llvm::GlobalValue* symbol) noexcept
{
    auto const block{builder.GetInsertBlock()};
    auto const function{block == nullptr ? nullptr : block->getParent()};
    auto const module{function == nullptr ? nullptr : function->getParent()};
    if(symbol == nullptr || module == nullptr || symbol->getParent() != module ||
       !symbol->getType()->isPointerTy() || symbol->getAddressSpace() != 0u ||
       !symbol->isDeclaration() || !symbol->hasExternalLinkage()) [[unlikely]]
    { return nullptr; }

    auto const name{::uwvm2::utils::container::u8concat_uwvm(u8"uwvm.host.binding.",
        ::uwvm2::utils::container::u8string_view{
            reinterpret_cast<char8_t const*>(symbol->getName().data()), symbol->getName().size()})};
    auto const name_ref{get_llvm_string_ref(name)};
    ::llvm::GlobalVariable* carrier{};
    if(auto const existing{module->getNamedValue(name_ref)}; existing != nullptr)
    {
        carrier = ::llvm::dyn_cast<::llvm::GlobalVariable>(existing);
        if(carrier == nullptr || carrier->getValueType() != symbol->getType() ||
           !carrier->hasPrivateLinkage() || carrier->isConstant() || carrier->isThreadLocal() ||
           carrier->getAddressSpace() != 0u || !carrier->hasInitializer() ||
           carrier->getInitializer() != symbol) [[unlikely]]
        { return nullptr; }
    }
    else
    {
        carrier = new ::llvm::GlobalVariable{*module, symbol->getType(), false,
            ::llvm::GlobalValue::PrivateLinkage, symbol, name_ref};
        carrier->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
    }
    ::llvm::Value* location{carrier};
    auto const triple{::llvm::Triple{module->getTargetTriple()}};
    bool riscv_pc_relative{triple.isRISCV()};
#if defined(__riscv)
    // Full-module emission installs the TargetMachine triple after lowering.
    // An absent triple still denotes this executable's native target.
    if(triple.getArch() == ::llvm::Triple::UnknownArch) { riscv_pc_relative = true; }
#endif
    if(riscv_pc_relative)
    {
        // Static RISC-V MCJIT otherwise chooses absolute HI20/LO12 for the
        // carrier itself, truncating a JIT allocation above 4 GiB. `lla`
        // requires only supported PCREL_HI20/LO12_I relocations to nearby
        // JIT storage; the following load has no LO12_S relocation.
        auto const type{::llvm::FunctionType::get(carrier->getType(), {carrier->getType()}, false)};
        auto const address{::llvm::InlineAsm::get(type, "lla $0, $1", "=r,i", false)};
        auto const call{builder.CreateCall(address, {carrier}, "uwvm.host.binding.addr")};
        call->setDoesNotAccessMemory();
        call->setDoesNotThrow();
        location = call;
    }
    auto const pointer{builder.CreateLoad(symbol->getType(), location, "uwvm.host.bound.ptr")};
    // Folding this load would restore the target's unsupported far data/call
    // relocation. Volatile preserves the nearby load through O1/O2/O3.
    pointer->setVolatile(true);
    pointer->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
    return pointer;
}
