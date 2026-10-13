// Production observer-return emitter -> real MCJIT native ABI. Mock DATA
// supplies no actual VM frame, pause, GC root or execution authority.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
namespace cp=uwvm2::runtime::checkpoint;
namespace emit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
static unsigned expected_kind, specialised_calls, generic_calls;
static void check(bool value) { if(!value)std::abort(); }
namespace uwvm2::runtime::lib::details
{
extern "C++" void llvm_jit_checkpoint_observer_return_abi_bridge(std::uint64_t incarnation,std::uintptr_t buffer) noexcept
{ check(expected_kind==0u && incarnation==0x123456789abcdef0ull && buffer==0xfedcba9876543210ull);++specialised_calls; }
extern "C++" void llvm_jit_checkpoint_observer_activation_leave_abi_bridge(std::uint64_t incarnation,std::uintptr_t kind,std::uintptr_t buffer) noexcept
{ check(kind==expected_kind && incarnation==0x123456789abcdef0ull && buffer==0xfedcba9876543210ull);++generic_calls; }
extern "C++" void llvm_jit_checkpoint_activation_leave_abi_bridge(std::uint64_t,std::uintptr_t) noexcept {std::abort();}
extern "C++" void llvm_jit_debug_activation_leave_abi_bridge(std::uint64_t,std::uintptr_t) noexcept {std::abort();}
}
int main()
{
#if defined(__linux__) && defined(__x86_64__)
 static_assert(emit::llvm_jit_debug_control_bridge<uwvm2::runtime::lib::details::llvm_jit_checkpoint_observer_return_abi_bridge>);
 check(!llvm::InitializeNativeTarget() && !llvm::InitializeNativeTargetAsmPrinter());
 llvm::EngineBuilder select;select.setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Less);
 std::unique_ptr<llvm::TargetMachine> target(select.selectTarget());check(bool(target));
 llvm::LLVMContext context;auto module=std::make_unique<llvm::Module>("actual-observer-return",context);
 module->setTargetTriple(target->getTargetTriple());module->setDataLayout(target->createDataLayout());
 cp::function_plan plan;plan.profile=cp::compilation_profile::create_for_trusted_observer();
 for(unsigned index=0;index!=8;++index)
 {
  auto fn=llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(context),{},false),llvm::Function::ExternalLinkage,"probe"+std::to_string(index),*module);
  emit::runtime_local_func_llvm_jit_emit_state_t state;state.llvm_module=module.get();state.llvm_function=fn;state.checkpoint_plan=&plan;
  state.debug_activation_enabled=true;state.emit_debug_safe_points=index<4;
  state.ir_builder=uwvm2::utils::container::make_delete_owned<llvm::IRBuilder<>>(llvm::BasicBlock::Create(context,"entry",fn));
  auto& builder=*state.ir_builder;state.debug_activation_token=builder.getInt64(0x123456789abcdef0ull);
  state.checkpoint_observer_workspace=builder.CreateIntToPtr(builder.getInt64(0xfedcba9876543210ull),builder.getPtrTy());
  check(!emit::emit_runtime_local_func_llvm_jit_debug_activation_leave(builder,state,4u));check(fn->getEntryBlock().empty());
  check(emit::emit_runtime_local_func_llvm_jit_debug_activation_leave(builder,state,index%4));
  auto call=llvm::dyn_cast<llvm::CallInst>(&fn->getEntryBlock().back());check(call && call->doesNotThrow());
  check(call->arg_size()==(index==0u ? 2u:3u));builder.CreateRetVoid();
 }
 check(!llvm::verifyModule(*module,&llvm::errs()));
 std::string error;llvm::EngineBuilder build(std::move(module));build.setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Less).setErrorStr(&error);
 std::unique_ptr<llvm::ExecutionEngine> engine(build.create());check(bool(engine));engine->finalizeObject();
 for(unsigned index=0;index!=8;++index)
 {
  expected_kind=index%4;auto fn=reinterpret_cast<void(*)()>(engine->getFunctionAddress("probe"+std::to_string(index)));check(fn!=nullptr);fn();
 }
 check(specialised_calls==1u && generic_calls==7u);
 std::puts("actual production return emitter native ABI: constant ordinary return, typed/host tails, EH and non-observing fallback PASS");
#endif
}
