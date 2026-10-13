// Production numeric snapshot IR + LLVM local promotion + real native execution.
// Component DATA only: no publication, paused activation, roots or restore rights.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <uwvm2/runtime/compiler/llvm_jit/debug_numeric_local_ssa.h>
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
    auto module{::std::make_unique<::llvm::Module>("numeric-local-snapshot-data",*context)};
    module->setTargetTriple(target->getTargetTriple());module->setDataLayout(target->createDataLayout());
    emit::runtime_local_func_llvm_jit_emit_state_t state{};
    auto function{::llvm::Function::Create(::llvm::FunctionType::get(::llvm::Type::getInt32Ty(*context),false),
        ::llvm::GlobalValue::ExternalLinkage,"main",*module)};
    state.llvm_context_holder=context.get();state.llvm_module=module.get();state.llvm_function=function;
    state.ir_builder=::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(::llvm::BasicBlock::Create(*context,"entry",function));
    auto& ir{*state.ir_builder};
    constexpr ::std::size_t count{5u},prefix{count*::uwvm2::runtime::checkpoint::observer_local_metadata_bytes};
    constexpr auto extent{prefix+count*16u};
    auto storage{new ::llvm::GlobalVariable(*module,::llvm::ArrayType::get(ir.getInt8Ty(),extent),false,
        ::llvm::GlobalValue::InternalLinkage,::llvm::ConstantAggregateZero::get(::llvm::ArrayType::get(ir.getInt8Ty(),extent)),"actual.component.data")};
    auto factory{::llvm::Function::Create(::llvm::FunctionType::get(ir.getInt64Ty(),{ir.getInt64Ty()},false),
        ::llvm::GlobalValue::InternalLinkage,"component.data.owner",*module)};
    ::llvm::IRBuilder<> owner{::llvm::BasicBlock::Create(*context,"entry",factory)};
    owner.CreateRet(owner.CreatePtrToInt(storage,owner.getInt64Ty()));
    state.checkpoint_observer_workspace_allocation=ir.CreateCall(factory,{ir.getInt64(extent)});
    state.checkpoint_observer_workspace=ir.CreateIntToPtr(state.checkpoint_observer_workspace_allocation,ir.getPtrTy());
    // Real entry capture runs before the opcode control map is bound. The
    // immutable observer profile selects storage; this mock DATA owner grants
    // no actual activation, reference-root or restore permission.
    ::uwvm2::runtime::checkpoint::function_plan plan{};
    plan.profile=::uwvm2::runtime::checkpoint::compilation_profile::create_for_trusted_observer();
    require(bool(plan.profile),"real immutable observer profile");state.checkpoint_plan=::std::addressof(plan);
    emit::llvm_jit_checkpoint_observer_control_map control_data{};
    constexpr ::std::array types{emit::runtime_operand_stack_value_type::i32,emit::runtime_operand_stack_value_type::i64,
        emit::runtime_operand_stack_value_type::f32,emit::runtime_operand_stack_value_type::f64,emit::runtime_operand_stack_value_type::v128};
    constexpr ::std::array<::std::uint64_t,count> low{0x87654321u,0xfedcba9876543210u,0x7f812345u,0x7ff0123456789abcu,0x1020304050607080u};
    ::std::array<::llvm::ConstantInt*,count> expected{};
    for(::std::size_t i{};i!=count;++i)
    {
        auto type{emit::get_llvm_type_from_wasm_value_type(*context,types[i])};
        auto local{emit::create_llvm_jit_entry_block_alloca(ir,type,nullptr,"actual.numeric.local")};
        state.local_types.push_back(types[i]);state.local_pointers.push_back(local);
        auto width{static_cast<unsigned>(emit::get_runtime_wasm_value_type_abi_size(types[i])*8u)};
        ::llvm::APInt bits{width,low[i]};
        if(width==128u) { bits|=::llvm::APInt{128u,0xa0b0c0d0e0f00102u}.shl(64u); }
        auto value{::llvm::ConstantInt::get(*context,bits)};expected[i]=value;
        ir.CreateStore(::llvm::ConstantExpr::getBitCast(value,type),local);
    }
    auto flags{state.checkpoint_observer_workspace};
    auto saved_flags{ir.CreateInBoundsGEP(ir.getInt8Ty(),flags,ir.getInt64(count))};
    auto values{ir.CreateInBoundsGEP(ir.getInt8Ty(),flags,ir.getInt64(prefix))};
    for(::std::size_t i{};i!=count;++i)
    { ir.CreateStore(ir.getInt8(1u),ir.CreateInBoundsGEP(ir.getInt8Ty(),flags,ir.getInt64(i)))->setAlignment(::llvm::Align{1u}); }
    ::llvm::Value* ok{ir.getTrue()};
    auto check{[&](::llvm::Value* condition){ok=ir.CreateAnd(ok,condition);}};
    auto capture{[&](unsigned f32_flag,bool changed)
    {
        ir.CreateStore(ir.getInt8(f32_flag),ir.CreateInBoundsGEP(ir.getInt8Ty(),flags,ir.getInt64(2u)))->setAlignment(::llvm::Align{1u});
        require(emit::emit_runtime_local_func_llvm_jit_snapshot_copy(state,count,values,saved_flags,flags,false),"production complete numeric capture");
        require(state.snapshot_local_pointers==nullptr && state.checkpoint_local_snapshot_copy==nullptr,"no local address escape");
        for(::std::size_t i{};i!=count;++i)
        {
            auto saved{ir.CreateLoad(ir.getInt8Ty(),ir.CreateInBoundsGEP(ir.getInt8Ty(),saved_flags,ir.getInt64(i)))};saved->setAlignment(::llvm::Align{1u});
            check(ir.CreateICmpEQ(saved,ir.getInt8(i==2u ? f32_flag:1u)));
            auto actual{ir.CreateLoad(ir.getInt128Ty(),ir.CreateInBoundsGEP(ir.getInt8Ty(),values,ir.getInt64(i*16u)))};actual->setAlignment(::llvm::Align{1u});
            auto bits{expected[i]->getValue().zextOrTrunc(128u)};
            if(i==0u && changed) { bits=::llvm::APInt{128u,42u}; }
            if(i==2u && f32_flag!=1u) { bits=::llvm::APInt{128u,0u}; }
            check(ir.CreateICmpEQ(actual,::llvm::ConstantInt::get(*context,bits)));
        }
    }};
    require(state.checkpoint_observer_controls==nullptr,"entry has no opcode control map");
    capture(1u,false);capture(0u,false);capture(2u,false);
    // Subsequent opcode captures retain the existing bound-control path.
    state.checkpoint_observer_controls=::std::addressof(control_data);
    ir.CreateStore(ir.getInt32(42u),state.local_pointers[0u]);capture(1u,true);
    // An owned allocation alone cannot select early observer captures. Real
    // resumable profiles, unbound profiles and ordinary compilation retain
    // their original bounded address-table copier and native result.
    for(unsigned policy{};policy!=3u;++policy)
    {
        emit::runtime_local_func_llvm_jit_emit_state_t fallback{};
        auto probe{::llvm::Function::Create(::llvm::FunctionType::get(ir.getInt32Ty(),false),
            ::llvm::GlobalValue::InternalLinkage,::llvm::Twine{"entry.fallback."}+::llvm::Twine{policy},*module)};
        fallback.llvm_context_holder=context.get();fallback.llvm_module=module.get();fallback.llvm_function=probe;
        fallback.ir_builder=::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(::llvm::BasicBlock::Create(*context,"entry",probe));
        auto& fi{*fallback.ir_builder};
        fallback.checkpoint_observer_workspace_allocation=fi.CreateCall(factory,{fi.getInt64(extent)});
        fallback.checkpoint_observer_workspace=fi.CreateIntToPtr(fallback.checkpoint_observer_workspace_allocation,fi.getPtrTy());
        ::uwvm2::runtime::checkpoint::function_plan fallback_plan{};
        if(policy==0u) { fallback_plan.profile=::uwvm2::runtime::checkpoint::compilation_profile::create_for_trusted_manager(); }
        if(policy!=2u) { fallback.checkpoint_plan=::std::addressof(fallback_plan); }
        for(::std::size_t i{};i!=count;++i)
        {
            auto type{emit::get_llvm_type_from_wasm_value_type(*context,types[i])};
            auto slot{emit::create_llvm_jit_entry_block_alloca(fi,type,nullptr,"actual.fallback.local")};
            fallback.local_types.push_back(types[i]);fallback.local_pointers.push_back(slot);
            fi.CreateStore(::llvm::ConstantExpr::getBitCast(expected[i],type),slot);
            fi.CreateStore(fi.getInt8(1u),fi.CreateInBoundsGEP(fi.getInt8Ty(),fallback.checkpoint_observer_workspace,fi.getInt64(i)));
        }
        auto ff{fallback.checkpoint_observer_workspace};
        auto fv{fi.CreateInBoundsGEP(fi.getInt8Ty(),ff,fi.getInt64(prefix))};
        require(emit::emit_runtime_local_func_llvm_jit_snapshot_copy(fallback,count,fv,
            fi.CreateInBoundsGEP(fi.getInt8Ty(),ff,fi.getInt64(count)),ff,false),"nonobserver entry retains bounded capture");
        require(fallback.snapshot_local_pointers!=nullptr && fallback.checkpoint_local_snapshot_copy!=nullptr,
            "resumable, missing profile and ordinary entries retain address-table copier");
        auto actual{fi.CreateLoad(fi.getInt32Ty(),fv)};actual->setAlignment(::llvm::Align{1u});fi.CreateRet(actual);
        check(ir.CreateICmpEQ(ir.CreateCall(probe),ir.getInt32(static_cast<::std::uint32_t>(low[0u]))));
    }
    // Exercise both actual hot GP pressure and nonconstant cold reloads.
    // Only hot GP writes trigger the bound; cold stores still participate in
    // the real LLVM promotion and native execution. Component DATA only.
    for(bool hot_gp : {true,false})
    {
    emit::runtime_local_func_llvm_jit_emit_state_t pressure{};
    auto dense{::llvm::Function::Create(::llvm::FunctionType::get(ir.getInt32Ty(),
        {ir.getInt32Ty(),ir.getInt32Ty(),ir.getInt32Ty(),ir.getDoubleTy()},false),
        ::llvm::GlobalValue::InternalLinkage,hot_gp ? "numeric.pressure":"numeric.cold.reload",*module)};
    pressure.llvm_context_holder=context.get();pressure.llvm_module=module.get();pressure.llvm_function=dense;
    pressure.ir_builder=::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(::llvm::BasicBlock::Create(*context,"entry",dense));
    auto& pi{*pressure.ir_builder};
    pressure.checkpoint_observer_workspace_allocation=pi.CreateCall(factory,{pi.getInt64(extent)});
    pressure.checkpoint_observer_workspace=pi.CreateIntToPtr(pressure.checkpoint_observer_workspace_allocation,pi.getPtrTy());
    pressure.checkpoint_observer_controls=::std::addressof(control_data);
    constexpr ::std::array pressure_types{emit::runtime_operand_stack_value_type::i32,
        emit::runtime_operand_stack_value_type::i32,emit::runtime_operand_stack_value_type::i32,
        emit::runtime_operand_stack_value_type::f64,emit::runtime_operand_stack_value_type::i32};
    for(::std::size_t i{};i!=count;++i)
    {
        auto type{emit::get_llvm_type_from_wasm_value_type(*context,pressure_types[i])};
        auto slot{emit::create_llvm_jit_entry_block_alloca(pi,type,nullptr,"actual.pressure.local")};
        pressure.local_types.push_back(pressure_types[i]);pressure.local_pointers.push_back(slot);
        pi.CreateStore(i==4u ? static_cast<::llvm::Value*>(pi.getInt32(7u)):dense->getArg(static_cast<unsigned>(i)),slot);
        if(hot_gp && i<3u)
        { slot->setMetadata("uwvm.debug.numeric.mutable.gp",::llvm::MDNode::get(*context,{})); }
        pi.CreateStore(pi.getInt8(1u),pi.CreateInBoundsGEP(pi.getInt8Ty(),pressure.checkpoint_observer_workspace,pi.getInt64(i)))->setAlignment(::llvm::Align{1u});
    }
    // Internal entry temporaries also need SSA in low-pressure functions.
    // This slot is not a Wasm local and never enters the observation packet.
    auto scratch{emit::create_llvm_jit_entry_block_alloca(pi,pi.getInt8Ty(),nullptr,"internal.capture.flag")};
    pi.CreateStore(pi.getInt8(9u),scratch);
    auto pf{pressure.checkpoint_observer_workspace};
    require(emit::emit_runtime_local_func_llvm_jit_snapshot_copy(pressure,count,
        pi.CreateInBoundsGEP(pi.getInt8Ty(),pf,pi.getInt64(prefix)),
        pi.CreateInBoundsGEP(pi.getInt8Ty(),pf,pi.getInt64(count)),pf,false),"actual dense numeric copy");
    auto sum{pi.CreateAdd(pi.CreateAdd(pi.CreateLoad(pi.getInt32Ty(),pressure.local_pointers[0u]),
        pi.CreateLoad(pi.getInt32Ty(),pressure.local_pointers[1u])),pi.CreateLoad(pi.getInt32Ty(),pressure.local_pointers[2u]))};
    sum=pi.CreateAdd(sum,pi.CreateLoad(pi.getInt32Ty(),pressure.local_pointers[4u]));
    sum=pi.CreateAdd(sum,pi.CreateZExt(pi.CreateLoad(pi.getInt8Ty(),scratch),pi.getInt32Ty()));
    pi.CreateRet(pi.CreateAdd(sum,pi.CreateFPToSI(pi.CreateLoad(pi.getDoubleTy(),pressure.local_pointers[3u]),pi.getInt32Ty())));
    require(support::promote_runtime_llvm_jit_debug_numeric_locals(*dense)==(hot_gp ? 2u:6u),"actual hot GP count controls production promotion");
    unsigned retained{};
    for(auto const& block:*dense)for(auto const& instruction:block)
    { if(auto slot{::llvm::dyn_cast<::llvm::AllocaInst>(::std::addressof(instruction))})
      { require(slot->getAllocatedType()==pi.getInt32Ty() || slot->getAllocatedType()==pi.getInt8Ty(),"dense GP path retains GP and internal temporary slots");++retained; } }
    require(retained==(hot_gp ? 4u:0u),"cold reloads do not trigger GP pressure bound");
    check(ir.CreateICmpEQ(ir.CreateCall(dense,{ir.getInt32(11u),ir.getInt32(22u),ir.getInt32(33u),
        ::llvm::ConstantFP::get(ir.getDoubleTy(),3.5)}),ir.getInt32(85u)));
    }
    ir.CreateRet(ir.CreateSelect(ok,ir.getInt32(0u),ir.getInt32(1u)));
    require(!::llvm::verifyModule(*module),"full IR before promotion");
    ::llvm::SmallVector<::llvm::AllocaInst*,8u> locals{};
    for(auto pointer : state.local_pointers)
    { auto local{::llvm::cast<::llvm::AllocaInst>(pointer)};require(::llvm::isAllocaPromotable(local),"typed actual locals remain promotable");locals.push_back(local); }
    require(support::promote_runtime_llvm_jit_debug_numeric_locals(*function)==count,"production promotion of all five numeric locals");
    require(!::llvm::verifyModule(*module),"full IR after promotion");
    for(auto const& block:*function)for(auto const& instruction:block)
    { require(!::llvm::isa<::llvm::AllocaInst>(instruction),"promoted numeric stack slots removed"); }
    if(argc==2)
    { ::std::error_code error{};::llvm::raw_fd_ostream output{argv[1],error};require(!error,"IR output");module->print(output,nullptr); }
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{::llvm::EngineBuilder{::std::move(module)}
        .setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::Less)
        .setMCJITMemoryManager(::std::make_unique<support::runtime_llvm_jit_section_memory_manager>()).create(target.release())}};
    require(bool(engine),"native MCJIT");engine->finalizeObject();require(!engine->hasError(),"native materialization");
    auto address{engine->getFunctionAddress("main")};require(address!=0u,"native entry");
    require(reinterpret_cast<int(*)()>(static_cast<::std::uintptr_t>(address))()==0,"all bits, padding, changed values and flags survive promotion");
    ::fast_io::io::println("PASS production numeric snapshot + local SSA: all 16 bytes, sNaNs, v128, flags0/1/2, fresh changed values, hot GP bound, cold reloads, internal temporaries; DATA only");
}
