// Cross-backend contract probe, not a cross-built runtime or native unwinder.
// Apply production function attributes and scalar FP emission to a portable
// byte-buffer ABI. Actual Wasm translation/push-pop exclusion is independently
// checked by llvm_jit_native_unwind_ir and live recursive trap/cache tests.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/IR/Verifier.h>
#include <memory>

int main(int argc, char** argv)
{
    if(argc != 4) { return 2; }
    llvm::InitializeAllTargetInfos(); llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs(); llvm::InitializeAllAsmPrinters();
    // Normalize short command-line spellings before inspecting the ABI.
    // Without this, the N32 probe can accidentally emit an N64 DataLayout.
    llvm::Triple triple{llvm::Triple::normalize(argv[1])};
    std::string error;
    auto target{llvm::TargetRegistry::lookupTarget("", triple, error)};
    if(!target) { llvm::errs() << error; return 3; }
    std::unique_ptr<llvm::TargetMachine> machine{target->createTargetMachine(triple, argv[2], argv[3], {}, llvm::Reloc::PIC_)};
    if(!machine) { return 4; }
    llvm::LLVMContext context;
    llvm::Module module{"uwvm-target-contract", context};
    module.setTargetTriple(triple); module.setDataLayout(machine->createDataLayout());
    if(triple.isABIN32() && module.getDataLayout().getPointerSizeInBits() != 32u) { return 6; }
    llvm::IRBuilder<> builder{context};
    namespace d = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
    auto ptr{llvm::PointerType::getUnqual(context)};
    auto signature{llvm::FunctionType::get(builder.getVoidTy(), {ptr, ptr}, false)};
    auto make = [&](char const* name)
    {
        auto fn{llvm::Function::Create(signature, llvm::Function::ExternalLinkage, name, module)};
        fn->addFnAttr("target-cpu", argv[2]); fn->addFnAttr("target-features", argv[3]);
        d::apply_llvm_jit_common_function_attrs(*fn);
        d::apply_llvm_jit_unwind_call_stack_function_attrs(*fn);
        return fn;
    };
    auto leaf{make("uwvm_contract_leaf")};
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", leaf));
    // Integer buffers preserve signaling-NaN bits at the test ABI boundary.
    for(unsigned width: {32u, 64u})
    {
        auto integer{builder.getIntNTy(width)};
        auto type{width == 32 ? builder.getFloatTy() : builder.getDoubleTy()};
        auto offset{builder.getInt32(width == 32 ? 0 : 8)};
        auto in{builder.CreateGEP(builder.getInt8Ty(), leaf->getArg(1), offset)};
        auto out{builder.CreateGEP(builder.getInt8Ty(), leaf->getArg(0), offset)};
        auto bits{builder.CreateLoad(integer, in)}; bits->setAlignment(llvm::Align{1});
        d::llvm_wasm_arithmetic_scope scope{builder};
        auto value{d::emit_llvm_float_binary(builder, builder.CreateBitCast(bits, type), llvm::ConstantFP::get(type, 1.0), 3u)};
        builder.CreateStore(builder.CreateBitCast(value, integer), out)->setAlignment(llvm::Align{1});
    }
    builder.CreateRetVoid();
    auto caller{make("uwvm_contract_caller")};
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", caller));
    builder.CreateCall(leaf, {caller->getArg(0), caller->getArg(1)});
    builder.CreateRetVoid();
    if(llvm::verifyModule(module, &llvm::errs())) { return 5; }
    module.print(llvm::outs(), nullptr);
}
