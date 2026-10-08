// Complete 32-bit-host i64 carriers: actual bit-preserving MCJIT execution
// plus explicit target-feature refusals. No selected register/stack reads.
#define UWVM_RUNTIME_LLVM_JIT 1
#define UWVM_USE_LLVM_JIT 1
#include <uwvm2/runtime/compiler/llvm_jit/native_provenance.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <fast_io.h>
#include <array>
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("32-bit numeric feature failure line=",__LINE__);return 1; } } while(false)
int main()
{
#if defined(__linux__) && (defined(__arm__) || (defined(__powerpc__) && __SIZEOF_POINTER__ == 4))
    namespace p=::uwvm2::runtime::compiler::llvm_jit::native_provenance;
    CHECK(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter() && !::llvm::InitializeNativeTargetAsmParser());
    for(unsigned phase{}; phase!=6u; ++phase)
    {
        ::llvm::LLVMContext context{};auto module{::std::make_unique<::llvm::Module>("actual-32bit-i64-carrier",context)};
        auto* f{::llvm::Function::Create(::llvm::FunctionType::get(::llvm::Type::getInt64Ty(context),
            {::llvm::Type::getInt64Ty(context)},false),::llvm::Function::ExternalLinkage,"whole_i64",*module)};
        if(phase==1u) { f->addFnAttr("target-features","+soft-float"); }
        if(phase==2u) { f->addFnAttr("target-features","-fpregs"); }
        if(phase==3u) { f->addFnAttr("target-features","-fpregs64"); }
        if(phase==4u) { f->addFnAttr("target-features","-fp64"); }
        if(phase==5u) { f->addFnAttr("target-cpu","unqualified-cpu"); }
        auto* scope{p::attach(*f,"uwvm-m0-f1-g1.wasm-native-v1")};CHECK(scope);
        auto* block{::llvm::BasicBlock::Create(context,"entry",f)};::llvm::IRBuilder<> ir{block};
        CHECK(p::location(ir,scope,0u,1u));
        auto* value{ir.CreateXor(f->getArg(0u),ir.getInt64(0xfedcba9876543210ull))};
        ::llvm::Instruction* witness{};auto* held{p::materialize_numeric_register(ir,value,&witness)};CHECK(held);
        ir.CreateRet(held);CHECK(!::llvm::verifyModule(*module));
        if(phase!=0u)
        {
            CHECK(held==value && witness==nullptr);
            auto* fp{::llvm::Function::Create(::llvm::FunctionType::get(ir.getFloatTy(),{ir.getFloatTy()},false),
                ::llvm::Function::InternalLinkage,"disabled_fp",*module)};
            fp->setAttributes(f->getAttributes());
            auto* fp_scope{p::attach(*fp,"uwvm-m0-f2-g1.wasm-native-v1")};CHECK(fp_scope);
            auto* fp_block{::llvm::BasicBlock::Create(context,"entry",fp)};::llvm::IRBuilder<> fp_ir{fp_block};
            CHECK(p::location(fp_ir,fp_scope,0u,1u));
            auto* fp_value{fp_ir.CreateFAdd(fp->getArg(0u),::llvm::ConstantFP::get(ir.getFloatTy(),1.0))};
            ::llvm::Instruction* fp_witness{};
            CHECK(p::materialize_numeric_register(fp_ir,fp_value,&fp_witness)==fp_value && fp_witness==nullptr);
            CHECK(p::numeric(fp_ir,fp_value,0x7du,0u,true));
            fp_ir.CreateRet(fp_value);CHECK(!::llvm::verifyModule(*module));continue;
        }
        CHECK(held!=value && witness!=nullptr);
        ::std::string error{};::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder(::std::move(module))
            .setEngineKind(::llvm::EngineKind::JIT).setMCPU(::llvm::sys::getHostCPUName()).setErrorStr(&error).create()};CHECK(engine);
        engine->finalizeObject();CHECK(!engine->hasError());
        auto const address{engine->getFunctionAddress("whole_i64")};CHECK(address);
        auto call{reinterpret_cast<::std::uint64_t(*)(::std::uint64_t)>(static_cast<::std::uintptr_t>(address))};
        for(auto bits: ::std::array<::std::uint64_t,6u>{0u,0x123456789abcdef0ull,0xffffffffffffffffull,
            0x7ff8000000000001ull,0xfff0000000000000ull,0x8000000000000000ull})
        { CHECK(call(bits)==(bits^0xfedcba9876543210ull)); }
    }
    ::fast_io::io::println("PASS actual complete i64 FP bit carrier including NaN encodings; five disabled/unqualified target policies refuse i64 and scalar FP materialization");
#else
    ::fast_io::io::println("SKIP ARM32/PPC32 numeric feature controls");
#endif
}
