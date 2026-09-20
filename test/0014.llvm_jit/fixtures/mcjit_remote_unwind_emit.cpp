// Emit a foreign diagnostic object with EngineBuilder's actual JIT defaults.
// llc -relocation-model=pic is NOT equivalent: e.g. ELF x86/MIPS JITs default
// to static relocations, and x86_64 JIT uses the large code model. Comparing
// PIC-only loader failures with native MCJIT would incorrectly label runtime
// paths as broken without testing the object format they actually consume.
// An explicit ABI matches the cross compiler used by the isolated guest; this
// does not certify a full ROS cross build or its host-feature discovery.
#include <llvm/ADT/SmallVector.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <memory>
#include <string>

int main(int argc, char** argv)
{
    if(argc != 7 && argc != 8) { return 2; }
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::LLVMContext context;
    llvm::SMDiagnostic diagnostic;
    auto module{llvm::parseIRFile(argv[1], diagnostic, context)};
    if(!module) { diagnostic.print(argv[0], llvm::errs()); return 3; }
    llvm::SmallVector<llvm::StringRef, 16> split;
    llvm::StringRef{argv[4]}.split(split, ',', -1, false);
    llvm::SmallVector<std::string, 16> features;
    for(auto feature: split) { features.push_back(feature.str()); }
    llvm::TargetOptions options;
    options.MCOptions.ABIName = argv[5];
    llvm::EngineBuilder builder;
    builder.setEngineKind(llvm::EngineKind::JIT)
        .setOptLevel(llvm::CodeGenOptLevel::Aggressive).setTargetOptions(options);
    std::unique_ptr<llvm::TargetMachine> target{
        builder.selectTarget(llvm::Triple{llvm::Triple::normalize(argv[2])}, {}, argv[3], features)};
    if(!target) { return 4; }
    // Ordinary uwvm2 can use an unpatched external LLVM. This optional control
    // tests unique temporary names without changing executable instructions.
    if(argc == 8) { target->Options.MCOptions.MCSaveTempLabels = true; }
    module->setTargetTriple(target->getTargetTriple());
    module->setDataLayout(target->createDataLayout());
    llvm::outs() << "triple=" << target->getTargetTriple().str()
                 << " relocation_model=" << unsigned(target->getRelocationModel())
                 << " code_model=" << unsigned(target->getCodeModel()) << '\n';
    std::error_code error;
    llvm::raw_fd_ostream output{argv[6], error};
    if(error) { return 5; }
    llvm::legacy::PassManager passes;
    if(target->addPassesToEmitFile(passes, output, nullptr, llvm::CodeGenFileType::ObjectFile)) { return 6; }
    passes.run(*module);
    output.flush();
    return output.has_error() ? 7 : 0;
}
