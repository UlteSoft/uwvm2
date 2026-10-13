#define UWVM_RUNTIME_LLVM_JIT 1
#define UWVM_USE_LLVM_JIT 1
#include <uwvm2/runtime/compiler/llvm_jit/native_provenance.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Verifier.h>
#include <fast_io.h>
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("x86 numeric feature failure line=",__LINE__);return 1; } } while(false)
int main()
{
#if defined(__linux__) && defined(__i386__)
    CHECK(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter() && !::llvm::InitializeNativeTargetAsmParser());
    ::llvm::LLVMContext context{};auto module{::std::make_unique<::llvm::Module>("actual-no-sse2-i64",context)};
    auto* function{::llvm::Function::Create(::llvm::FunctionType::get(::llvm::Type::getInt64Ty(context),
        {::llvm::Type::getInt64Ty(context)},false),::llvm::Function::ExternalLinkage,"no_sse2_i64",*module)};
    function->addFnAttr("target-cpu","pentium3");function->addFnAttr("target-features","-sse2");
    auto* scope{::uwvm2::runtime::compiler::llvm_jit::native_provenance::attach(*function,"uwvm-m0-f1-g1.wasm-native-v1")};CHECK(scope);
    auto* block{::llvm::BasicBlock::Create(context,"entry",function)};::llvm::IRBuilder<> ir{block};
    CHECK(::uwvm2::runtime::compiler::llvm_jit::native_provenance::location(ir,scope,0u,1u));
    auto* actual{ir.CreateXor(function->getArg(0u),ir.getInt64(0xfedcba9876543210ull))};
    ::llvm::Instruction* witness{};
    auto* held{::uwvm2::runtime::compiler::llvm_jit::native_provenance::materialize_numeric_register(ir,actual,&witness)};CHECK(held);
    ir.CreateRet(held);CHECK(!::llvm::verifyModule(*module));
    ::std::string error{};::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder(::std::move(module))
        .setEngineKind(::llvm::EngineKind::JIT).setMCPU("pentium3").setMAttrs(::llvm::SmallVector<::std::string,1u>{"-sse2"}).setErrorStr(&error).create()};CHECK(engine);
    engine->finalizeObject();CHECK(!engine->hasError());
    auto const address{engine->getFunctionAddress("no_sse2_i64")};CHECK(address);
    auto call{reinterpret_cast<::std::uint64_t(*)(::std::uint64_t)>(static_cast<::std::uintptr_t>(address))};
    CHECK(call(0x123456789abcdef0ull)==(0x123456789abcdef0ull^0xfedcba9876543210ull));
    CHECK(held==actual && witness==nullptr);
    ::fast_io::io::println("PASS actual pentium3/-sse2 target i64 MCJIT execution; ordinary split locations retained without forced XMM");
#else
    ::fast_io::io::println("SKIP i686-only CPU feature control");
#endif
}
