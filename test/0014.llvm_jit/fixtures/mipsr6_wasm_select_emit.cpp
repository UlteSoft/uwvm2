// Use the production SIMD emitter, not a handwritten substitute for its IR.
// Arguments are TRIPLE OUTPUT.ll. Compile against either main or ROS headers.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/Support/FileSystem.h>

int main(int argc, char** argv)
{
    if(argc != 3) { return 2; }
    namespace d = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
    llvm::LLVMContext context;
    llvm::Module module{"mipsr6-wasm-select", context};
    llvm::Triple target{argv[1]};
    if(!target.isMIPS() || !target.isArch32Bit()) { return 3; }
#if LLVM_VERSION_MAJOR >= 21
    module.setTargetTriple(target);
#else
    module.setTargetTriple(argv[1]);
#endif
    module.setDataLayout(target.isLittleEndian() ? "e-m:m-p:32:32-i8:8:32-i16:16:32-i64:64-n32-S64"
                                                 : "E-m:m-p:32:32-i8:8:32-i16:16:32-i64:64-n32-S64");
    llvm::IRBuilder<> b{context};
    auto pointer{llvm::PointerType::getUnqual(context)};
    auto vector{llvm::FixedVectorType::get(b.getInt8Ty(), 16u)};
    auto fn{llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(), {pointer, pointer, pointer, b.getInt32Ty()}, false),
        llvm::Function::ExternalLinkage, "mipsr6_wasm_pmin", module)};
    fn->addFnAttr("target-cpu", "mips32r6");
    fn->addFnAttr("target-features", "+nan2008,+noabicalls,-msa");
    b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", fn));
    auto a{b.CreateAlignedLoad(vector, fn->getArg(1), llvm::Align{1})};
    auto c{b.CreateAlignedLoad(vector, fn->getArg(2), llvm::Align{1})};
    auto selected{b.CreateSelect(b.CreateICmpNE(fn->getArg(3), b.getInt32(0)), c, a)};
    // Wasm pmin returns its FIRST argument for ties/unordered inputs. Reverse
    // these arguments to match the unchanged standalone reproduction exactly.
    auto result{d::simd_ir::emit_value(b, d::llvm_jit_simd_code::f64x2_pmin, selected, a)};
    b.CreateAlignedStore(result, fn->getArg(0), llvm::Align{1});
    b.CreateRetVoid();
    if(llvm::verifyModule(module, &llvm::errs())) { return 4; }
    std::error_code error;
    llvm::raw_fd_ostream output{argv[2], error};
    if(error) { return 5; }
    module.print(output, nullptr);
}
