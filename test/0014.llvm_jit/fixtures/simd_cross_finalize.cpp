// Cross-generated SIMD IR must run the same pre-optimization FP lowering and
// post-optimization vector legalization as a production materialization. The
// compiler running this utility may be x86_64 even when its module is i386;
// select the destination's contract explicitly instead of inheriting host
// constexpr values. This utility does not assert whole-runtime cross support.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/compiler/shared/strict_float_jit.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/SourceMgr.h>
#include <string_view>

int main(int argc, char** argv)
{
    if(argc != 4) { return 2; }
    llvm::LLVMContext context;
    llvm::SMDiagnostic diagnostic;
    auto module{llvm::parseIRFile(argv[1], diagnostic, context)};
    if(!module) { diagnostic.print(argv[0], llvm::errs()); return 3; }
    std::string_view mode{argv[3]};
    namespace fp = uwvm2::runtime::compiler::shared::strict_float_jit;
    if(mode == "i386")
    {
        llvm::Triple const target{module->getTargetTriple()};
        if(!target.isX86() || !target.isArch32Bit()) { return 4; }
        // i386's private integer FP-result ABI is mandatory even when its JIT
        // can use SSE2. The production pass keeps native arithmetic where the
        // function's actual features allow it, and lowers x87-sensitive work.
        fp::lower(*module, true, true, false);
    }
    else if(mode == "x86-extended")
    {
        llvm::Triple const target{module->getTargetTriple()};
        if(!target.isX86() || !target.isArch64Bit()) { return 4; }
        // Model a no-SSE x86_64 compiler's extended-rounding contract, not
        // the emitter host's default SSE2 macros. Unlike i386 this does not
        // opt into the private no-x87 FP-result ABI. Failures of external
        // x86_64 FP libcalls must remain visible to the cross-target audit.
        fp::lower(*module, true, false, false);
    }
    else if(mode == "native") { fp::lower(*module, false, false, false); }
    else if(mode == "native-nan") { fp::lower(*module, true, false, true); }
    else if(mode == "legalize")
    {
        uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::legalize_llvm_jit_native_vectors(*module);
    }
    else { return 4; }
    if(llvm::verifyModule(*module, &llvm::errs())) { return 5; }
    std::error_code error;
    llvm::raw_fd_ostream output{argv[2], error};
    if(error) { return 6; }
    module->print(output, nullptr);
    return 0;
}
