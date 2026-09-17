// Emit a deliberately large native frame using the production JIT attributes.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <cstdlib>

namespace d = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
int main(int argc, char** argv)
{
    if(argc > 3) { return 2; }
    auto const frame_bytes{argc == 3 ? ::std::strtoull(argv[2], nullptr, 10) : 20000ull};
    if(frame_bytes == 0 || frame_bytes > 131072ull) { return 2; }
    llvm::LLVMContext context;
    llvm::Module module("uwvm-native-stack-probes", context);
    llvm::Triple requested_target{argc >= 2 ? argv[1] : llvm::sys::getDefaultTargetTriple()};
#if LLVM_VERSION_MAJOR >= 21
    module.setTargetTriple(requested_target);
#else
    module.setTargetTriple(requested_target.str());
#endif
    llvm::IRBuilder<> builder(context);
    auto* callback_type = llvm::FunctionType::get(builder.getVoidTy(), {builder.getPtrTy()}, false);
    auto* consume = llvm::Function::Create(callback_type, llvm::Function::ExternalLinkage, "consume_stack", module);
    auto* function_type = llvm::FunctionType::get(builder.getVoidTy(), false);
    auto* large = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, "large_frame", module);
    d::apply_llvm_jit_common_function_attrs(*large);
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", large));
    auto* space = builder.CreateAlloca(llvm::ArrayType::get(builder.getInt8Ty(), frame_bytes));
    space->setAlignment(llvm::Align(16));
    builder.CreateCall(consume, {space});
    builder.CreateRetVoid();
    auto* small = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, "small_frame", module);
    d::apply_llvm_jit_common_function_attrs(*small);
    builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", small));
    builder.CreateRetVoid();
    llvm::Triple const triple{module.getTargetTriple()};
    bool const should_probe{!triple.isOSWindows() &&
        (triple.isX86() || triple.isAArch64() || triple.getArch() == llvm::Triple::systemz
#if LLVM_VERSION_MAJOR >= 20
         || triple.isRISCV()
#endif
        )};
    if(large->hasFnAttribute("probe-stack") != should_probe) { return 3; }
    auto const* expected_size{triple.isRISCV() ? "2048" : "4096"};
#if defined(__APPLE__) && defined(__aarch64__)
    auto const page{uwvm2::object::memory::platform_page::get_platform_page_size()};
    if(triple.getArch() == llvm::Triple::aarch64 && triple.isMacOSX() && page.success && page.page_size == 16384u)
    { expected_size = "16384"; }
#endif
    if(should_probe && large->getFnAttribute("stack-probe-size").getValueAsString() != expected_size) { return 4; }
    if(llvm::verifyModule(module, &llvm::errs())) { return 5; }
    if(argc >= 2) { module.print(llvm::outs(), nullptr); }
}
