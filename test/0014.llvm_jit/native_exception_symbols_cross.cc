// Cross-object qualification of the production symbol declarations; no foreign code executes here.
#define UWVM_RUNTIME_LLVM_JIT
#include <uwvm2/runtime/compiler/llvm_jit/native_exception_symbols.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <cstdio>
#include <memory>
#include <string>
namespace eh = uwvm2::runtime::compiler::llvm_jit::native_exception_symbols;
int main(int argc, char** argv)
{
    if(argc != 2) { return 2; }
    LLVMInitializeX86TargetInfo(); LLVMInitializeX86Target(); LLVMInitializeX86TargetMC(); LLVMInitializeX86AsmPrinter();
    LLVMInitializeAArch64TargetInfo(); LLVMInitializeAArch64Target(); LLVMInitializeAArch64TargetMC(); LLVMInitializeAArch64AsmPrinter();
    LLVMInitializeRISCVTargetInfo(); LLVMInitializeRISCVTarget(); LLVMInitializeRISCVTargetMC(); LLVMInitializeRISCVAsmPrinter();
    for(bool const pic: {false, true})
    for(auto const triple: {"aarch64-unknown-linux-gnu", "riscv64-unknown-linux-gnu", "i386-unknown-linux-gnu"})
    {
        llvm::EngineBuilder selector;
        selector.setEngineKind(llvm::EngineKind::JIT);
        if(pic) { selector.setRelocationModel(llvm::Reloc::PIC_); }
        llvm::SmallVector<std::string, 0> features;
        auto target{std::unique_ptr<llvm::TargetMachine>{selector.selectTarget(llvm::Triple{triple}, {}, {}, features)}};
        if(!target || !eh::supports_itanium_dwarf_object(*target)) { return 3; }
        llvm::LLVMContext context;
        llvm::Module module{"native-eh-cross-symbol-probe", context};
        module.setTargetTriple(target->getTargetTriple());
        module.setDataLayout(target->createDataLayout());
        auto const symbols{eh::declare_itanium_dwarf_symbols(module, *target)};
        if(symbols.status != eh::error::ok || !llvm::isa<llvm::GlobalVariable>(symbols.type_info)) { return 4; }
        llvm::IRBuilder<> builder{context};
        auto const i32{builder.getInt32Ty()};
        auto const pointer{builder.getPtrTy()};
        auto const raise{llvm::Function::Create(llvm::FunctionType::get(builder.getVoidTy(), false), llvm::GlobalValue::ExternalLinkage, "probe_raise", module)};
        auto const begin{llvm::Function::Create(llvm::FunctionType::get(pointer, {pointer}, false), llvm::GlobalValue::ExternalLinkage, "__cxa_begin_catch", module)};
        auto const end{llvm::Function::Create(llvm::FunctionType::get(builder.getVoidTy(), false), llvm::GlobalValue::ExternalLinkage, "__cxa_end_catch", module)};
        auto const function{llvm::Function::Create(llvm::FunctionType::get(i32, false), llvm::GlobalValue::ExternalLinkage, "probe_entry", module)};
        function->setPersonalityFn(symbols.personality);
        // Every insertion-point/value pointer refers to a block owned by this complete live module.
        auto const entry{llvm::BasicBlock::Create(context, "entry", function)};
        auto const normal{llvm::BasicBlock::Create(context, "normal", function)};
        auto const landing{llvm::BasicBlock::Create(context, "landing", function)};
        auto const caught{llvm::BasicBlock::Create(context, "caught", function)};
        auto const foreign{llvm::BasicBlock::Create(context, "foreign", function)};
        builder.SetInsertPoint(entry);
        builder.CreateInvoke(raise, normal, landing);
        builder.SetInsertPoint(normal);
        builder.CreateRet(builder.getInt32(17));
        builder.SetInsertPoint(landing);
        auto const record{builder.CreateLandingPad(llvm::StructType::get(context, {pointer, i32}), 1u)};
        record->setCleanup(true);
        record->addClause(symbols.type_info);
        auto const type_id{llvm::Intrinsic::isOverloaded(llvm::Intrinsic::eh_typeid_for)
            ? llvm::Intrinsic::getOrInsertDeclaration(&module, llvm::Intrinsic::eh_typeid_for, {symbols.type_info->getType()})
            : llvm::Intrinsic::getOrInsertDeclaration(&module, llvm::Intrinsic::eh_typeid_for)};
        builder.CreateCondBr(builder.CreateICmpEQ(builder.CreateExtractValue(record, 1u), builder.CreateCall(type_id, {symbols.type_info})), caught, foreign);
        builder.SetInsertPoint(caught);
        builder.CreateCall(begin, {builder.CreateExtractValue(record, 0u)});
        builder.CreateCall(end);
        builder.CreateRet(builder.getInt32(42));
        builder.SetInsertPoint(foreign);
        builder.CreateResume(record);
        if(llvm::verifyModule(module, &llvm::errs())) { return 5; }
        std::error_code error;
        std::string const prefix{std::string{argv[1]} + "/" + triple + (pic ? "-pic" : "-jit-default")};
        llvm::raw_fd_ostream ir{prefix + ".ll", error};
        if(error) { return 6; }
        module.print(ir, nullptr);
        ir.close();
        llvm::raw_fd_ostream object{prefix + ".o", error};
        if(error) { return 7; }
        llvm::legacy::PassManager passes;
        if(target->addPassesToEmitFile(passes, object, nullptr, llvm::CodeGenFileType::ObjectFile)) { return 8; }
        passes.run(module);
        object.flush();
        if(object.has_error()) { return 9; }
        std::printf("PASS cross EH symbol object %s model=%s relocation=%u code-model=%u\n", triple, pic ? "pic" : "jit-default", unsigned(target->getRelocationModel()), unsigned(target->getCodeModel()));
    }
}
