// Included inside the LLVM lowering details namespace after the LLVM IR headers.
// Kept independent of native predefines so cross-codegen tests use the exact
// production emitter, even when the test generator itself runs on another ISA.
[[nodiscard]] inline ::llvm::Value* get_llvm_riscv64_immediate_pointer_value(::llvm::IRBuilder<>& ir_builder,
                                                                          ::std::uintptr_t host_address,
                                                                          ::llvm::Type* pointer_type) noexcept
{
    if constexpr(sizeof(::std::uintptr_t) != 8u) { return nullptr; }
    if(host_address == 0u || pointer_type == nullptr || !pointer_type->isPointerTy()) [[unlikely]] { return nullptr; }
    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || current_block->getParent() == nullptr) [[unlikely]] { return nullptr; }

    // RuntimeDyld cannot safely encode arbitrary far RISC-V64 data references.
    // A volatile zero plus byte shifts is insufficient: O3 can still put the
    // constant part in a literal pool. Force the assembler's full-width `li`
    // expansion instead: register-only instructions, no symbol/data relocation.
    // This inline-asm IR call is not a native bridge call. The immediate is the
    // complete 64-bit address, including its sign bit; no address bits are lost.
    auto integer_type{ir_builder.getInt64Ty()};
    auto signature{::llvm::FunctionType::get(integer_type, {integer_type}, false)};
    auto materializer{::llvm::InlineAsm::get(signature, "li $0, $1", "=r,i", false)};
    auto address{ir_builder.CreateCall(materializer, {::llvm::ConstantInt::get(integer_type, host_address)}, "uwvm.riscv64.host.addr")};
    address->setDoesNotAccessMemory();
    address->setDoesNotThrow();
    return ir_builder.CreateIntToPtr(address, pointer_type, "uwvm.host.ptr");
}
