// Real production fused entry/growth emitter -> MCJIT, native aggregate ABI.
// Component DATA only. Mock identities/storage grant no real VM admission.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <array>
#include <memory>
#include <cstdio>
#include <cstdlib>
namespace cp=uwvm2::runtime::checkpoint;
namespace emit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
static unsigned calls;
static std::size_t expected_bytes=100u;
static std::array<std::byte,512> storage;
static void require_at(bool b,unsigned line) { if(!b) { std::fprintf(stderr,"observer entry IR assertion failed at %u\n",line);std::abort(); } }
#define require(condition) require_at((condition),__LINE__)
namespace uwvm2::runtime::lib::details
{
extern "C++" llvm_jit_checkpoint_observer_entry_result llvm_jit_checkpoint_observer_activation_enter_abi_bridge(
    std::size_t bytes,std::uintptr_t module,std::uintptr_t function,std::uint64_t generation) noexcept
{ require(bytes==expected_bytes && module==7u && function==11u && generation==13u);++calls;
  storage.fill(std::byte{0xa5u});std::memset(storage.data(),0,bytes);
  return {0xdecaf123u,reinterpret_cast<std::uintptr_t>(storage.data())}; }
extern "C++" llvm_jit_checkpoint_observer_entry_result llvm_jit_checkpoint_observer_small_enter_abi_bridge(
    std::size_t bytes,std::uintptr_t module,std::uintptr_t function,std::uint64_t generation) noexcept
{ require(bytes==expected_bytes && bytes<=64u && module==7u && function==11u && generation==13u);++calls;
  storage.fill(std::byte{0xa5u});std::memset(storage.data(),0,bytes);
  return {0xdecaf123u,reinterpret_cast<std::uintptr_t>(storage.data())}; }
extern "C++" std::uint64_t llvm_jit_checkpoint_activation_enter_abi_bridge(std::uintptr_t,std::uintptr_t,std::uint64_t) noexcept
{ std::abort(); }
extern "C++" std::uint64_t llvm_jit_debug_activation_enter_abi_bridge(std::uintptr_t,std::uintptr_t,std::uint64_t) noexcept
{ std::abort(); }
extern "C++" std::uintptr_t llvm_jit_checkpoint_observer_workspace_abi_bridge(std::size_t) noexcept
{ std::abort(); }
}
int main()
{
#if defined(__linux__) && defined(__x86_64__)
    static_assert(sizeof(uwvm2::runtime::lib::details::llvm_jit_checkpoint_observer_entry_result)==16u);
    require(!llvm::InitializeNativeTarget() && !llvm::InitializeNativeTargetAsmPrinter());
    llvm::EngineBuilder choose;choose.setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Less);
    std::unique_ptr<llvm::TargetMachine> target{choose.selectTarget()};require(bool(target));
    llvm::LLVMContext context;auto module=std::make_unique<llvm::Module>("fused-observer-entry",context);
    module->setTargetTriple(target->getTargetTriple());module->setDataLayout(target->createDataLayout());
    auto function=llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt64Ty(context),{},false),llvm::Function::ExternalLinkage,"probe",*module);
    cp::function_plan plan;plan.profile=cp::compilation_profile::create_for_trusted_observer();
    uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::local_func_storage_t local;local.module_id=7u;local.function_index=11u;
    emit::runtime_local_func_llvm_jit_emit_state_t state;state.checkpoint_plan=&plan;state.local_func_storage_ptr=&local;
    state.llvm_context_holder=&context;state.llvm_module=module.get();state.llvm_function=function;
    state.debug_activation_enabled=true;state.emit_debug_safe_points=true;state.debug_compiled_function_generation=13u;
    state.ir_builder=uwvm2::utils::container::make_delete_owned<llvm::IRBuilder<>>(llvm::BasicBlock::Create(context,"entry",function));
    auto& builder=*state.ir_builder;
    for(auto kind:{emit::runtime_operand_stack_value_type::i32,emit::runtime_operand_stack_value_type::funcref})
    { state.local_types.push_back(kind);state.local_pointers.push_back(emit::create_llvm_jit_entry_block_alloca(builder,emit::get_llvm_type_from_wasm_value_type(context,kind),nullptr,"local")); }
    require(emit::emit_runtime_local_func_llvm_jit_debug_activation_enter(state));
    require(emit::prepare_runtime_local_func_llvm_jit_checkpoint_observer_workspace(state));
    require(emit::grow_runtime_local_func_llvm_jit_checkpoint_observer_workspace(state,5u));
    require(emit::checkpoint_observer_allocation_extent(state)->getZExtValue()==100u);
    require(emit::checkpoint_canonical_local_owner(state));
    require(!state.checkpoint_observer_workspace_allocation->getMetadata("uwvm.debug.observer.entry.small"));
    require(emit::prepare_checkpoint_observer_small_entry(state));
    auto bad=builder.CreateExtractValue(state.checkpoint_observer_workspace_allocation,{0u});
    require(!emit::checkpoint_observer_workspace_result_matches(state,bad));
    auto token=state.debug_activation_token;state.debug_activation_token=builder.CreateExtractValue(state.checkpoint_observer_workspace_allocation,{1u});
    auto pointer=llvm::cast<llvm::IntToPtrInst>(state.checkpoint_observer_workspace);
    require(!emit::checkpoint_observer_workspace_result_matches(state,pointer->getOperand(0u)));state.debug_activation_token=token;
    builder.CreateStore(builder.getInt32(42u),state.local_pointers[0]);
    require(emit::update_runtime_local_func_llvm_jit_canonical_numeric_local(state,0u));
    builder.CreateRet(state.debug_activation_token);
    require(!llvm::verifyModule(*module,&llvm::errs()));
    unsigned real_calls{};for(auto& block:*function)for(auto& instruction:block)if(llvm::isa<llvm::CallInst>(instruction))++real_calls;
    require(real_calls==1u);
    auto small=llvm::Function::Create(function->getFunctionType(),llvm::Function::ExternalLinkage,"small_probe",*module);
    emit::runtime_local_func_llvm_jit_emit_state_t ls;ls.checkpoint_plan=&plan;ls.local_func_storage_ptr=&local;
    ls.llvm_context_holder=&context;ls.llvm_module=module.get();ls.llvm_function=small;
    ls.debug_activation_enabled=true;ls.emit_debug_safe_points=true;ls.debug_compiled_function_generation=13u;
    ls.ir_builder=uwvm2::utils::container::make_delete_owned<llvm::IRBuilder<>>(llvm::BasicBlock::Create(context,"entry",small));
    auto& si=*ls.ir_builder;ls.local_types.push_back(emit::runtime_operand_stack_value_type::i32);
    ls.local_pointers.push_back(emit::create_llvm_jit_entry_block_alloca(si,si.getInt32Ty(),nullptr,"local"));
    require(emit::emit_runtime_local_func_llvm_jit_debug_activation_enter(ls) &&
        emit::prepare_runtime_local_func_llvm_jit_checkpoint_observer_workspace(ls));
    require(emit::grow_runtime_local_func_llvm_jit_checkpoint_observer_workspace(ls,2u));
    require(emit::checkpoint_observer_allocation_extent(ls)->getZExtValue()==42u &&
        ls.checkpoint_observer_workspace_allocation->getMetadata("uwvm.debug.observer.entry.small"));
    require(emit::prepare_checkpoint_observer_small_entry(ls));
    auto small_callee=ls.checkpoint_observer_workspace_allocation->getCalledOperand();
    ls.checkpoint_observer_workspace_allocation->setCalledOperand(state.checkpoint_observer_workspace_allocation->getCalledOperand());
    require(!emit::grow_runtime_local_func_llvm_jit_checkpoint_observer_workspace(ls,3u));
    require(emit::checkpoint_observer_allocation_extent(ls)->getZExtValue()==42u);
    ls.checkpoint_observer_workspace_allocation->setCalledOperand(small_callee);
    si.CreateRet(ls.debug_activation_token);require(!llvm::verifyModule(*module,&llvm::errs()));
    std::string error;llvm::EngineBuilder build(std::move(module));build.setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Less).setErrorStr(&error);
    std::unique_ptr<llvm::ExecutionEngine> engine{build.create()};require(bool(engine));engine->finalizeObject();
    auto entry=reinterpret_cast<std::uint64_t(*)()>(engine->getFunctionAddress("probe"));require(entry && entry()==0xdecaf123u && calls==1u);
    std::uint32_t value{};std::memcpy(&value,storage.data()+20u,4u);require(value==42u && storage[2u]==std::byte{1u});
    for(std::size_t i=24u;i<100u;++i)require(storage[i]==std::byte{});
    for(std::size_t i=100u;i<storage.size();++i)require(storage[i]==std::byte{0xa5u});
    expected_bytes=42u;auto small_entry=reinterpret_cast<std::uint64_t(*)()>(engine->getFunctionAddress("small_probe"));
    require(small_entry && small_entry()==0xdecaf123u && calls==2u);
    for(std::size_t i{};i<42u;++i)require(storage[i]==std::byte{});
    for(std::size_t i=42u;i<storage.size();++i)require(storage[i]==std::byte{0xa5u});
    std::puts("actual fused entry, small native selection, large growth fallback, repeated selection, wrong-callee rejection, aggregate ABI, full bytes and canonical store: passed");
#endif
}
