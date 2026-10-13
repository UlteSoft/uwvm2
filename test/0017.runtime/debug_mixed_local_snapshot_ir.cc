// Real production capture, LLVM promotion and native execution of mixed locals.
// Component DATA only; these slots confer no publication, root or restore rights.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/debug_numeric_local_ssa.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <array>
#include <memory>
#include <fast_io.h>
namespace emit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
namespace support = ::uwvm2::runtime::compiler::llvm_jit::details;
static void require(bool ok,char const* why)
{ if(!ok) { ::fast_io::io::perrln(::fast_io::mnp::os_c_str(why)); ::fast_io::fast_terminate(); } }
int main(int argc,char** argv)
{
    require(argc<=2,"optional verified IR output");
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(),"native target");
    ::llvm::EngineBuilder select{};select.setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::Less);
    auto target{::std::unique_ptr<::llvm::TargetMachine>{select.selectTarget()}};require(bool(target),"target machine");
    auto context{::std::make_unique<::llvm::LLVMContext>()};
    auto module{::std::make_unique<::llvm::Module>("mixed-local-snapshot-data",*context)};
    module->setTargetTriple(target->getTargetTriple());module->setDataLayout(target->createDataLayout());
    emit::runtime_local_func_llvm_jit_emit_state_t state{};
    auto function{::llvm::Function::Create(::llvm::FunctionType::get(::llvm::Type::getInt32Ty(*context),false),
        ::llvm::GlobalValue::ExternalLinkage,"main",*module)};
    state.llvm_context_holder=context.get();state.llvm_module=module.get();state.llvm_function=function;
    state.ir_builder=::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(::llvm::BasicBlock::Create(*context,"entry",function));
    auto& ir{*state.ir_builder};
    constexpr ::std::size_t count{8u},prefix{count*::uwvm2::runtime::checkpoint::observer_local_metadata_bytes};
    constexpr auto extent{prefix+count*16u};
    auto storage{new ::llvm::GlobalVariable(*module,::llvm::ArrayType::get(ir.getInt8Ty(),extent),false,
        ::llvm::GlobalValue::InternalLinkage,::llvm::ConstantAggregateZero::get(::llvm::ArrayType::get(ir.getInt8Ty(),extent)),"actual.component.data")};
    auto factory{::llvm::Function::Create(::llvm::FunctionType::get(ir.getInt64Ty(),{ir.getInt64Ty()},false),
        ::llvm::GlobalValue::InternalLinkage,"component.data.owner",*module)};
    ::llvm::IRBuilder<> owner{::llvm::BasicBlock::Create(*context,"entry",factory)};
    owner.CreateRet(owner.CreatePtrToInt(storage,owner.getInt64Ty()));
    state.checkpoint_observer_workspace_allocation=ir.CreateCall(factory,{ir.getInt64(extent)});
    state.checkpoint_observer_workspace=ir.CreateIntToPtr(state.checkpoint_observer_workspace_allocation,ir.getPtrTy());
    ::uwvm2::runtime::checkpoint::function_plan plan{};
    plan.profile=::uwvm2::runtime::checkpoint::compilation_profile::create_for_trusted_observer();
    require(bool(plan.profile),"real immutable observer profile");state.checkpoint_plan=::std::addressof(plan);
    emit::llvm_jit_checkpoint_observer_control_map control_data{};
    constexpr ::std::array types{emit::runtime_operand_stack_value_type::i32,emit::runtime_operand_stack_value_type::i64,
        emit::runtime_operand_stack_value_type::f32,emit::runtime_operand_stack_value_type::f64,
        emit::runtime_operand_stack_value_type::v128,emit::runtime_operand_stack_value_type::funcref,
        emit::runtime_operand_stack_value_type::externref,emit::runtime_operand_stack_value_type::funcref};
    ::std::array<::std::uint64_t,count> low{0x87654321u,0xfedcba9876543210u,0x7f812345u,
        0x7ff0123456789abcu,0x1020304050607080u,0x123456789abcdef0u,0u,0u};
    ::std::array<::std::uint64_t,count> high{0u,0u,0u,0u,0xa0b0c0d0e0f00102u,0x1122334400000003u,0u,0u};
    auto store_local{[&](::std::size_t i)
    {
        auto type{emit::get_llvm_type_from_wasm_value_type(*context,types[i])};
        auto width{static_cast<unsigned>(emit::get_runtime_wasm_value_type_abi_size(types[i])*8u)};
        ::llvm::APInt bits{width,low[i]};
        if(width==128u) { bits|=::llvm::APInt{128u,high[i]}.shl(64u); }
        ir.CreateStore(::llvm::ConstantExpr::getBitCast(::llvm::ConstantInt::get(*context,bits),type),state.local_pointers[i]);
    }};
    for(::std::size_t i{};i!=count;++i)
    {
        auto type{emit::get_llvm_type_from_wasm_value_type(*context,types[i])};
        auto local{emit::create_llvm_jit_entry_block_alloca(ir,type,nullptr,"actual.mixed.local")};
        state.local_types.push_back(types[i]);state.local_pointers.push_back(local);
        // Last reference has genuinely uninitialized value storage initially.
        if(i!=7u) { store_local(i); }
    }
    auto flags{state.checkpoint_observer_workspace};
    auto saved_flags{ir.CreateInBoundsGEP(ir.getInt8Ty(),flags,ir.getInt64(count))};
    auto values{ir.CreateInBoundsGEP(ir.getInt8Ty(),flags,ir.getInt64(prefix))};
    ::llvm::Value* ok{ir.getTrue()};
    auto check{[&](::llvm::Value* condition){ok=ir.CreateAnd(ok,condition);}};
    ::llvm::GlobalVariable* original_zero{};
    auto capture{[&](bool public_locals,unsigned absent_flag,unsigned numeric_flag)
    {
        for(::std::size_t i{};i!=count;++i)
        {
            unsigned flag{i==7u ? absent_flag:(i==2u ? numeric_flag:1u)};
            ir.CreateStore(ir.getInt8(flag),ir.CreateInBoundsGEP(ir.getInt8Ty(),flags,ir.getInt64(i)))->setAlignment(::llvm::Align{1u});
        }
        require(emit::emit_runtime_local_func_llvm_jit_snapshot_copy(state,count,values,saved_flags,flags,public_locals),"production complete mixed capture");
        require(state.snapshot_local_pointers==nullptr && state.checkpoint_local_snapshot_copy==nullptr &&
            state.debug_local_snapshot_copy==nullptr,"small mixed capture has no address table or private call");
        require(state.debug_inline_snapshot_zero!=nullptr,"uninitialized reference has immutable fallback");
        if(original_zero==nullptr) { original_zero=state.debug_inline_snapshot_zero; }
        require(original_zero==state.debug_inline_snapshot_zero,"same function reuses exactly one immutable fallback");
        for(::std::size_t i{};i!=count;++i)
        {
            unsigned flag{i==7u ? absent_flag:(i==2u ? numeric_flag:1u)};
            auto saved{ir.CreateLoad(ir.getInt8Ty(),ir.CreateInBoundsGEP(ir.getInt8Ty(),saved_flags,ir.getInt64(i)))};saved->setAlignment(::llvm::Align{1u});
            check(ir.CreateICmpEQ(saved,ir.getInt8(flag)));
            auto actual{ir.CreateLoad(ir.getInt128Ty(),ir.CreateInBoundsGEP(ir.getInt8Ty(),values,ir.getInt64(i*16u)))};actual->setAlignment(::llvm::Align{1u});
            ::llvm::APInt bits{128u,low[i]};
            if(emit::get_runtime_wasm_value_type_abi_size(types[i])==16u) { bits|=::llvm::APInt{128u,high[i]}.shl(64u); }
            if(i>=5u && public_locals) { bits=::llvm::APInt{128u,low[i]==0u && high[i]==0u ? 1u:0u}; }
            if(flag!=1u) { bits=::llvm::APInt{128u,0u}; }
            check(ir.CreateICmpEQ(actual,::llvm::ConstantInt::get(*context,bits)));
        }
    }};
    // Entry has no opcode control map, including when an actual reference
    // local has never been initialized. Exercise raw and public snapshots.
    require(state.checkpoint_observer_controls==nullptr,"entry has no opcode control map");
    capture(false,0u,1u);capture(true,0u,1u);
    capture(false,2u,2u);capture(true,2u,0u);
    state.checkpoint_observer_controls=::std::addressof(control_data);
    low[0u]=42u;low[5u]=0u;low[6u]=0xabcdef1234567890u;low[7u]=0x12345678u;
    high[5u]=0u;high[6u]=0x5566778800000004u;high[7u]=0x0102030400000003u;
    for(auto i : {0u,5u,6u,7u}) { store_local(i); }
    capture(false,1u,1u);capture(true,1u,1u);
    // An initialized reference becoming absent cannot leak its previous value.
    capture(false,0u,1u);capture(true,0u,1u);
    ir.CreateRet(ir.CreateSelect(ok,ir.getInt32(0u),ir.getInt32(1u)));
    require(!::llvm::verifyModule(*module),"full mixed IR before promotion");
    unsigned guarded_loads{};
    for(auto const& block:*function)for(auto const& instruction:block)
    {
        if(auto load{::llvm::dyn_cast<::llvm::LoadInst>(::std::addressof(instruction))})
        {
            if(auto selected{::llvm::dyn_cast<::llvm::SelectInst>(load->getPointerOperand())})
            {
                require(selected->getFalseValue()==original_zero,"absent references select zero before reading storage");
                ++guarded_loads;
            }
        }
    }
    require(guarded_loads==24u,"all eight captures guard all three reference loads");
    for(::std::size_t i{};i!=count;++i)
    {
        auto local{::llvm::cast<::llvm::AllocaInst>(state.local_pointers[i])};
        require((local->getMetadata("uwvm.debug.numeric.local")!=nullptr)==(i<5u),"only numeric declarations carry promotion metadata");
        require(::llvm::isAllocaPromotable(local)==(i<5u),"references retain their actual guarded storage");
    }
    require(support::promote_runtime_llvm_jit_debug_numeric_locals(*function)==5u,"all five numeric locals promoted in mixed function");
    require(!::llvm::verifyModule(*module),"full mixed IR after promotion");
    unsigned retained{};
    for(auto const& block:*function)for(auto const& instruction:block)
    { if(auto local{::llvm::dyn_cast<::llvm::AllocaInst>(::std::addressof(instruction))})
      { require(local->getMetadata("uwvm.debug.numeric.local")==nullptr,"no numeric local remains address escaped");++retained; } }
    require(retained==3u,"exactly three reference slots retained");
    if(argc==2)
    { ::std::error_code error{};::llvm::raw_fd_ostream output{argv[1],error};require(!error,"IR output");module->print(output,nullptr); }
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{::llvm::EngineBuilder{::std::move(module)}
        .setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::Less)
        .setMCJITMemoryManager(::std::make_unique<support::runtime_llvm_jit_section_memory_manager>()).create(target.release())}};
    require(bool(engine),"native MCJIT");engine->finalizeObject();require(!engine->hasError(),"native materialization");
    auto address{engine->getFunctionAddress("main")};require(address!=0u,"native entry");
    require(reinterpret_cast<int(*)()>(static_cast<::std::uintptr_t>(address))()==0,"all mixed bits, padding, nullness, invalid flags and fresh updates survive promotion");
    ::fast_io::io::println("PASS production mixed snapshot + numeric SSA: eight captures, all 16 bytes, sNaNs, v128, raw/public references, genuinely uninitialized storage, flags0/1/2 and fresh updates; DATA only");
}
