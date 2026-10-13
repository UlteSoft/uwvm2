// Real compiler bridge selection and MCJIT execution. Component data only:
// local scope witnesses are not VM activation, stop or restore credentials.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <memory>
#include <cstdio>
#include <cstdlib>
namespace cp=uwvm2::runtime::checkpoint;
namespace emit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
static unsigned entered{},left{};
static void require(bool value) { if(!value) { std::fputs("GC scalar leaf IR assertion failed\n",stderr);std::abort(); } }
namespace uwvm2::runtime::lib::details
{
extern "C++" std::uint64_t llvm_jit_debug_host_enter_abi_bridge() noexcept { ++entered; return 1u; }
extern "C++" void llvm_jit_debug_host_leave_abi_bridge(std::uint64_t token) noexcept { require(token==1u);++left; }
extern "C++" void llvm_jit_debug_host_unwind_abi_bridge(std::uint64_t) noexcept { std::abort(); }
}
template<bool Signed>
static void probe(cp::function_plan* plan,bool debug,unsigned expected)
{
    llvm::LLVMContext context;auto module=std::make_unique<llvm::Module>("checked-scalar-leaf",context);
    llvm::EngineBuilder choose;choose.setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Less);
    std::unique_ptr<llvm::TargetMachine> target{choose.selectTarget()};require(bool(target));
    module->setTargetTriple(target->getTargetTriple());module->setDataLayout(target->createDataLayout());
    auto i64=llvm::Type::getInt64Ty(context);auto i32=llvm::Type::getInt32Ty(context);
    auto ip=llvm::Type::getIntNTy(context,sizeof(std::uintptr_t)*CHAR_BIT);
    auto function=llvm::Function::Create(llvm::FunctionType::get(i64,{},false),llvm::Function::ExternalLinkage,"probe",*module);
    if(debug)function->addFnAttr("uwvm.debug.activation");
    auto block=llvm::BasicBlock::Create(context,"entry",function);llvm::IRBuilder<> builder(block);
    emit::runtime_local_func_llvm_jit_emit_state_t state;state.checkpoint_plan=plan;
    auto type=llvm::FunctionType::get(i64,{ip,i32,ip,i32},false);
    auto callee=emit::get_llvm_jit_gc_struct_get32_bridge_symbol_value<Signed>(state,builder,type,u8"component_gc_get32");require(callee);
    auto call=emit::apply_llvm_jit_host_calling_conv(builder.CreateCall(type,callee,{builder.getIntN(sizeof(std::uintptr_t)*CHAR_BIT,0u),builder.getInt32(0u),builder.getIntN(sizeof(std::uintptr_t)*CHAR_BIT,0u),builder.getInt32(0u)}));
    call->setDoesNotThrow();builder.CreateRet(call);require(!llvm::verifyModule(*module,&llvm::errs()));
    std::string error;llvm::EngineBuilder build(std::move(module));build.setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Less).setErrorStr(&error);
    std::unique_ptr<llvm::ExecutionEngine> engine{build.create()};require(bool(engine));engine->finalizeObject();
    auto entry=reinterpret_cast<std::uint64_t(*)()>(engine->getFunctionAddress("probe"));require(entry);
    entered=left=0u;auto actual=entry();require(actual==emit::llvm_jit_gc_struct_get32_bridge<Signed>(0u,0u,0u,0u));
    require(entered==expected&&left==expected);
}
int main()
{
    require(!llvm::InitializeNativeTarget()&&!llvm::InitializeNativeTargetAsmPrinter());
    cp::function_plan observer;observer.profile=cp::compilation_profile::create_for_trusted_observer();
    cp::function_plan resumable;resumable.profile=cp::compilation_profile::create_for_trusted_manager();
    cp::function_plan missing;
    auto cases=[&]<bool Signed>()
    {
        probe<Signed>(&observer,true,0u);probe<Signed>(&resumable,true,1u);
        probe<Signed>(&missing,true,1u);probe<Signed>(nullptr,true,1u);
        probe<Signed>(nullptr,false,0u);
    };
    cases.template operator()<false>();cases.template operator()<true>();
    std::puts("GC checked scalar observer leaf: actual MCJIT signed/unsigned selection, resumable/unbound fallback passed");
}
