// ELF test harness for the exact target-selection function bodies extracted
// from the runtime/lazy implementation. Only the owned string/container adapters
// are replaced; no guest code or memory/unwind implementation is mocked here.
// --wrap injects the configured triple on an x86 test host. This tests target
// selection and object encoding, NOT execution on a native ILP32 LLVM library.
#include <llvm/ADT/SmallVector.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/MC/TargetRegistry.h>
#include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <memory>
#include <string>
#include <vector>

namespace uwvm2::utils::container
{
    using u8string = std::u8string;
    template<class T> using vector = std::vector<T>;
    template<class T> using delete_owned_ptr = std::unique_ptr<T>;
}
namespace uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details
{
    llvm::StringRef get_llvm_string_ref(std::u8string const& value)
    { return {reinterpret_cast<char const*>(value.data()), value.size()}; }
}
static std::string injected_default;
static unsigned process_calls;
extern "C" std::string __wrap__ZN4llvm3sys22getDefaultTargetTripleEv()
{ return injected_default; }
extern "C" std::string __wrap__ZN4llvm3sys16getProcessTripleEv()
{
    ++process_calls;
    llvm::Triple triple{llvm::Triple::normalize(injected_default)};
    // Model the actual LLVM sizeof(void*) == 4 branch, not a new UWVM policy.
    if(triple.isArch64Bit()) { triple = triple.get32BitArchVariant(); }
    return triple.str();
}
#if defined(_GLIBCXX_USE_CXX11_ABI) && _GLIBCXX_USE_CXX11_ABI
// Ordinary UWVM can use libstdc++ LLVM packages. Their std::string-returning
// symbols carry an ABI tag; wrap them too, rather than accidentally testing
// the real x86 host triple while claiming an injected N32 configuration.
extern "C" std::string __wrap__ZN4llvm3sys22getDefaultTargetTripleB5cxx11Ev()
{ return __wrap__ZN4llvm3sys22getDefaultTargetTripleEv(); }
extern "C" std::string __wrap__ZN4llvm3sys16getProcessTripleB5cxx11Ev()
{ return __wrap__ZN4llvm3sys16getProcessTripleEv(); }
#endif
namespace selected
{
    namespace all_details = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
    struct llvm_jit_native_target_config
    {
        std::u8string cpu_name;
        std::vector<std::u8string> feature_storage;
    };
#include "native_target_functions.inc"
}
int main(int argc, char** argv)
{
    if(argc != 6) { return 2; }
    injected_default = argv[1];
    llvm::Triple expected{llvm::Triple::normalize(injected_default)};
    llvm::InitializeAllTargetInfos(); llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs(); llvm::InitializeAllAsmPrinters();
    llvm::EngineBuilder engine;
    engine.setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Aggressive);
    auto string = [](char const* text) { return std::u8string{reinterpret_cast<char8_t const*>(text)}; };
    std::u8string cpu{string(argv[2])};
    std::vector<std::u8string> features;
    llvm::SmallVector<llvm::StringRef, 16> parts;
    llvm::StringRef{argv[3]}.split(parts, ',', -1, false);
    for(auto part: parts) { features.emplace_back(reinterpret_cast<char8_t const*>(part.data()), part.size()); }
    std::unique_ptr<llvm::TargetMachine> machine;
    bool const negative{llvm::StringRef{argv[5]} == "implicit32"};
    if(negative)
    {
        engine.setMCPU(argv[2]).setMAttrs(parts);
        machine.reset(engine.selectTarget());
    }
    else
    {
#ifdef UWVM_TEST_NATIVE_TARGET_LAZY
        machine = selected::select_llvm_jit_target(engine, {cpu, features});
#else
        machine = selected::select_runtime_llvm_jit_target(engine, cpu, features);
#endif
        if(process_calls != 0) { return 3; }
    }
    if(!machine) { return 4; }
    llvm::outs() << "unique_temp_labels=" << unsigned(machine->Options.MCOptions.MCSaveTempLabels) << '\n';
    llvm::outs() << "selected=" << machine->getTargetTriple().str() << "\nfeatures=" << machine->getTargetFeatureString() << '\n';
    if(!negative && machine->getTargetTriple() != expected) { return 5; }
    llvm::LLVMContext context;
    llvm::Module module{"native-target-selection", context};
    module.setTargetTriple(machine->getTargetTriple()); module.setDataLayout(machine->createDataLayout());
    llvm::outs() << "pointer_bits=" << module.getDataLayout().getPointerSizeInBits() << '\n';
    llvm::IRBuilder<> builder{context};
    auto ptr{builder.getPtrTy()};
    auto fn{llvm::Function::Create(llvm::FunctionType::get(builder.getVoidTy(), {ptr, ptr}, false),
                                    llvm::Function::ExternalLinkage, "native_abi_add", module)};
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", fn));
    auto value{builder.CreateLoad(builder.getInt64Ty(), fn->getArg(1))};
    value->setAlignment(llvm::Align{1});
    builder.CreateStore(builder.CreateAdd(value, builder.getInt64(0x123456789ULL)), fn->getArg(0))->setAlignment(llvm::Align{1});
    builder.CreateRetVoid();
    std::error_code error;
    llvm::raw_fd_ostream output{argv[4], error};
    if(error) { return 6; }
    llvm::legacy::PassManager passes;
    if(machine->addPassesToEmitFile(passes, output, nullptr, llvm::CodeGenFileType::ObjectFile)) { return 7; }
    passes.run(module);
    return 0;
}
