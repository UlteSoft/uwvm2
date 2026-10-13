// Actual production refresh emitter + original complete copier -> MCJIT.
// Component DATA only; these pointers/epochs grant no VM/native authority.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <array>
#include <atomic>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <source_location>
namespace cp=uwvm2::runtime::checkpoint;
namespace emit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
static unsigned context_data;
static void* component_workspace;
static void require(bool condition,std::source_location where=std::source_location::current())
{ if(!condition) { std::fprintf(stderr,"canonical numeric local IR failed line %u\n",where.line());std::abort(); } }
static std::uintptr_t allocate_workspace(std::uint64_t bytes)
{ require(bytes==104u); return reinterpret_cast<std::uintptr_t>(component_workspace); }
static void change_policy(unsigned mode)
{
    if(mode==1u) { cp::observation_probe_hints::require_all(&context_data); }
    else if(mode==2u)
    { cp::observation_probe_hints::bitmap sparse{};require(cp::observation_probe_hints::publish(&context_data,sparse)); }
    else
    {
        // Test exhaustion DATA through the real atomic object, never alias a
        // packed word or fabricate native stop/restore admission.
        auto const address=cp::observation_probe_hints::full_token_address();require(address!=nullptr);
        auto const token=static_cast<std::atomic<std::uint64_t>*>(const_cast<void*>(address));
        token->store(0u,std::memory_order_release);
    }
}
int main()
{
#if defined(__linux__) && defined(__x86_64__)
    require(!llvm::InitializeNativeTarget() && !llvm::InitializeNativeTargetAsmPrinter());
    llvm::EngineBuilder select;select.setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Less);
    std::unique_ptr<llvm::TargetMachine> target{select.selectTarget()};require(bool(target));
    llvm::LLVMContext context;auto module=std::make_unique<llvm::Module>("actual-canonical-local-updates",context);
    module->setTargetTriple(target->getTargetTriple());module->setDataLayout(target->createDataLayout());
    auto const function=llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt32Ty(context),
        {llvm::PointerType::getUnqual(context),llvm::PointerType::getUnqual(context)},false),
        llvm::Function::ExternalLinkage,"probe",*module);
    emit::runtime_local_func_llvm_jit_emit_state_t state;
    cp::function_plan plan;plan.profile=cp::compilation_profile::create_for_trusted_observer();
    plan.sites.emplace_back();plan.sites[0].local_count=4u;plan.sites[0].slots.resize(4u);
    for(auto& slot:plan.sites[0].slots) { slot.initialized=true; }plan.sites[0].slots[3].initialized=false;
    emit::llvm_jit_checkpoint_observer_control_map controls;controls.plan=&plan;state.checkpoint_observer_controls=&controls;
    state.checkpoint_plan=&plan;state.llvm_context_holder=&context;
    state.llvm_module=module.get();state.llvm_function=function;
    state.ir_builder=uwvm2::utils::container::make_delete_owned<llvm::IRBuilder<>>(
        llvm::BasicBlock::Create(context,"entry",function));auto& builder=*state.ir_builder;
    auto const allocator=module->getOrInsertFunction("component_workspace",llvm::FunctionType::get(builder.getInt64Ty(),{builder.getInt64Ty()},false));
    auto allocation=builder.CreateCall(allocator,{builder.getInt64(104u)});state.checkpoint_observer_workspace_allocation=allocation;
    state.checkpoint_observer_workspace=builder.CreateIntToPtr(allocation,builder.getPtrTy());
    constexpr std::array kinds{emit::runtime_operand_stack_value_type::i32,emit::runtime_operand_stack_value_type::i64,emit::runtime_operand_stack_value_type::f32,emit::runtime_operand_stack_value_type::funcref};
    for(auto kind:kinds)
    {
        state.local_types.push_back(kind);state.local_pointers.push_back(emit::create_llvm_jit_entry_block_alloca(
            builder,emit::get_llvm_type_from_wasm_value_type(context,kind),nullptr,"actual.numeric.local"));
    }
    require(emit::prepare_runtime_local_func_llvm_jit_canonical_numeric_locals(state));
    require(state.checkpoint_observer_canonical_numeric_locals && state.checkpoint_observer_local_epoch);
    auto cell=[&](std::size_t offset) { return builder.CreateInBoundsGEP(builder.getInt8Ty(),state.checkpoint_observer_workspace,builder.getInt64(offset)); };
    builder.CreateStore(builder.getInt32(0x00010101u),cell(0u))->setAlignment(llvm::Align{1u});
    builder.CreateStore(builder.getInt64(0u),function->getArg(1));
    builder.CreateStore(builder.getInt32(42u),state.local_pointers[0]);
    builder.CreateStore(builder.getInt64(0x123456789abcdef0ull),state.local_pointers[1]);
    builder.CreateStore(builder.getInt32(0x7f812345u),state.local_pointers[2]);
    llvm::Value* ok=builder.getTrue();
    auto check=[&](llvm::Value* condition) { ok=builder.CreateAnd(ok,condition); };
    auto const policy=module->getOrInsertFunction("component_change_policy",llvm::FunctionType::get(builder.getVoidTy(),{builder.getInt32Ty()},false));
    auto capture=[&](unsigned expected_copies,unsigned expected_value,std::uint64_t expected_ref)
    {
        llvm::Value* epoch{};
        require(emit::emit_runtime_local_func_llvm_jit_canonical_local_refresh(state,epoch,[&](bool retain) noexcept
        {
            if(!retain)
            {
                auto count=builder.CreateLoad(builder.getInt64Ty(),function->getArg(1));
                builder.CreateStore(builder.CreateAdd(count,builder.getInt64(1u)),function->getArg(1));
            }
            return emit::emit_runtime_local_func_llvm_jit_snapshot_copy(state,4u,cell(40u),cell(4u),state.checkpoint_observer_workspace,false,retain);
        }));
        require(epoch!=nullptr);
        check(builder.CreateICmpEQ(builder.CreateLoad(builder.getInt64Ty(),function->getArg(1)),builder.getInt64(expected_copies)));
        auto value=builder.CreateLoad(builder.getInt32Ty(),cell(40u));value->setAlignment(llvm::Align{1u});
        check(builder.CreateICmpEQ(value,builder.getInt32(expected_value)));
        auto wide=builder.CreateLoad(builder.getInt64Ty(),cell(56u));wide->setAlignment(llvm::Align{1u});
        check(builder.CreateICmpEQ(wide,builder.getInt64(0x123456789abcdef0ull)));
        auto floating=builder.CreateLoad(builder.getInt32Ty(),cell(72u));floating->setAlignment(llvm::Align{1u});
        check(builder.CreateICmpEQ(floating,builder.getInt32(0x7f812345u)));
        auto reference=builder.CreateLoad(builder.getInt128Ty(),cell(88u));reference->setAlignment(llvm::Align{1u});
        check(builder.CreateICmpEQ(reference,llvm::ConstantInt::get(builder.getInt128Ty(),expected_ref)));
        for(unsigned i=0u;i!=4u;++i)
        {
            check(builder.CreateICmpEQ(builder.CreateLoad(builder.getInt8Ty(),cell(4u+i)),builder.getInt8(i==3u && expected_ref==0u ? 0u:1u)));
            if(i==3u) { continue; }
            for(unsigned j=i==1u?8u:4u;j!=16u;++j)
            { check(builder.CreateICmpEQ(builder.CreateLoad(builder.getInt8Ty(),cell(40u+i*16u+j)),builder.getInt8(0u))); }
        }
    };
    capture(1u,42u,0u);capture(1u,42u,0u); // real uninitialized reference is never loaded
    builder.CreateStore(llvm::ConstantInt::get(builder.getInt128Ty(),12345u),state.local_pointers[3]);
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_assignment_flag(state,nullptr,3u));
    capture(1u,42u,12345u); // reference refreshed despite unchanged numeric epoch
    builder.CreateStore(llvm::ConstantInt::get(builder.getInt128Ty(),56789u),state.local_pointers[3]);
    capture(1u,42u,56789u); // relocation DATA: references never retained
    builder.CreateStore(builder.getInt32(43u),state.local_pointers[0]);
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_assignment_flag(state,nullptr,0u));
    capture(1u,43u,56789u); // assignment writes the complete canonical cell directly
    builder.CreateStore(builder.getInt32(44u),state.local_pointers[0]); // external/native edit simulation
    builder.CreateCall(policy,{builder.getInt32(1u)});capture(2u,44u,56789u); // republish -> original full refresh
    builder.CreateCall(policy,{builder.getInt32(2u)});capture(3u,44u,56789u);capture(3u,44u,56789u); // stable sparse token
    builder.CreateStore(builder.getInt32(45u),state.local_pointers[0]);
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_assignment_flag(state,nullptr,0u));
    capture(3u,45u,56789u); // direct update still executes with observation gates disabled
    builder.CreateStore(builder.getInt32(46u),state.local_pointers[0]);
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_reset_initialization_flags(state,nullptr));
    capture(3u,46u,0u);capture(3u,46u,0u); // real self-tail resets numeric cells and reference init flags
    builder.CreateCall(policy,{builder.getInt32(0u)});capture(4u,46u,0u);capture(5u,46u,0u); // zero always full
    require(!emit::emit_runtime_local_func_llvm_jit_snapshot_copy(state,4u,cell(40u),cell(4u),cell(0u),true,true));
    builder.CreateRet(builder.CreateSelect(ok,builder.getInt32(0u),builder.getInt32(1u)));
    require(!llvm::verifyModule(*module,&llvm::errs()));
    state.ir_builder.reset();state.llvm_module=nullptr;state.llvm_function=nullptr;
    std::unique_ptr<llvm::ExecutionEngine> engine{llvm::EngineBuilder(std::move(module)).setEngineKind(llvm::EngineKind::JIT)
        .setOptLevel(llvm::CodeGenOptLevel::Less).create(target.release())};require(bool(engine));
    engine->addGlobalMapping("component_workspace",reinterpret_cast<std::uintptr_t>(&allocate_workspace));
    engine->addGlobalMapping("component_change_policy",reinterpret_cast<std::uintptr_t>(&change_policy));
    engine->finalizeObject();require(!engine->hasError());auto address=engine->getFunctionAddress("probe");require(address!=0u);
    auto probe=reinterpret_cast<int(*)(void*,std::uint64_t*)>(static_cast<std::uintptr_t>(address));
    std::array<std::uint8_t,128u> workspace;component_workspace=workspace.data();std::uint64_t copies{};
    for(unsigned activation=0u;activation!=2u;++activation)
    {
        workspace.fill(0xccu);cp::observation_probe_hints::configure(&context_data);
        require(probe(workspace.data(),&copies)==0 && copies==5u);
    }
    cp::observation_probe_hints::configure(nullptr);
    std::puts("PASS actual canonical numeric-local IR: verifier/MCJIT, executed full scalar cell writes, NaN/padding/flags, full+sparse refresh, reference init+relocation, tail reset, native epoch invalidation, zero token, fresh activation");
#else
    std::puts("SKIP: qualified canonical numeric locals only Linux x86_64");
#endif
}
