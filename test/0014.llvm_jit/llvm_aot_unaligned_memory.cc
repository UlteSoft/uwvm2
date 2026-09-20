#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

#include <cstdint>
#include <memory>

namespace
{
    namespace llvm_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;

    [[nodiscard]] int check_unaligned_memory_ir() noexcept
    {
        ::llvm::LLVMContext context{};
        ::llvm::Module module{"wasm-unaligned-memory", context};
        auto function_type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context), false)};
        auto function{::llvm::Function::Create(function_type, ::llvm::Function::ExternalLinkage, "probe", module)};
        auto block{::llvm::BasicBlock::Create(context, "entry", function)};
        ::llvm::IRBuilder<> builder{block};

        // Model a validator-legal `i32.load/store align=2` at dynamic address 1.  The memarg exponent is the natural
        // alignment hint, but the generated LLVM accesses may promise only byte alignment for this pointer.
        auto storage{builder.CreateAlloca(::llvm::ArrayType::get(::llvm::Type::getInt8Ty(context), 8u))};
        auto unaligned_pointer{builder.CreateConstInBoundsGEP1_64(storage->getAllocatedType(), storage, 1u)};
        auto i32_pointer{builder.CreatePointerCast(unaligned_pointer, llvm_details::get_llvm_pointer_type(::llvm::Type::getInt32Ty(context)))};
        auto const wasm_alignment{llvm_details::get_llvm_wasm_memory_access_alignment(4u, 2u)};

        auto load{builder.CreateLoad(::llvm::Type::getInt32Ty(context), i32_pointer)};
        load->setAlignment(wasm_alignment);
        auto store{builder.CreateStore(load, i32_pointer)};
        store->setAlignment(wasm_alignment);
        builder.CreateRetVoid();

        if(load->getAlign() != ::llvm::Align{1u} || store->getAlign() != ::llvm::Align{1u}) { return 1; }
        return ::llvm::verifyModule(module, ::std::addressof(::llvm::errs())) ? 2 : 0;
    }
}

int main()
{
    return check_unaligned_memory_ir();
}
