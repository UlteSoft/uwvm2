// Genuine LLVM waiting-call selector with an actual child call while its parent
// stays on the native stack. This component grants no VM capture/restore authority.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <fast_io.h>
#include <array>
namespace uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details
{
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_initialization_emit.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_packet_emit.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_resume_landing_emit.h>
}
namespace cp = ::uwvm2::runtime::checkpoint;
namespace emit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
static void require(bool condition, char const* text)
{
    if(!condition) { ::fast_io::io::perrln("checkpoint LLVM nested waiting: ", ::fast_io::mnp::os_c_str(text)); ::fast_io::fast_terminate(); }
}
int main()
{
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(), "actual LLVM native target");
    ::llvm::LLVMContext context{}; auto module{::std::make_unique<::llvm::Module>("checkpoint-waiting-child", context)};
    ::std::unique_ptr<::llvm::TargetMachine> actual_target{::llvm::EngineBuilder{}.selectTarget()};
    require(bool(actual_target), "actual native target before selected restore IR"); module->setDataLayout(actual_target->createDataLayout());
    auto const integer{::llvm::Type::getIntNTy(context, static_cast<unsigned>(sizeof(::std::uintptr_t)*CHAR_BIT))};
    auto const counter{new ::llvm::GlobalVariable{*module, ::llvm::Type::getInt64Ty(context), false,
        ::llvm::GlobalValue::ExternalLinkage, ::llvm::ConstantInt::get(::llvm::Type::getInt64Ty(context), 0u), "actual_child_calls"}};
    auto const child_type{::llvm::FunctionType::get(::llvm::Type::getInt64Ty(context), {::llvm::Type::getInt64Ty(context)}, false)};
    auto const child{::llvm::Function::Create(child_type, ::llvm::GlobalValue::ExternalLinkage, "actual_child", *module)};
    child->addFnAttr(::llvm::Attribute::NoInline); ::llvm::IRBuilder<> leaf{::llvm::BasicBlock::Create(context,"entry",child)};
    leaf.CreateStore(leaf.CreateAdd(leaf.CreateLoad(leaf.getInt64Ty(),counter),leaf.getInt64(1u)),counter);
    leaf.CreateRet(leaf.CreateAdd(child->getArg(0u),leaf.getInt64(1u)));
    auto const parent_type{::llvm::FunctionType::get(integer,{::llvm::Type::getInt64Ty(context),integer,integer,integer,integer},false)};
    auto const parent{::llvm::Function::Create(parent_type,::llvm::GlobalValue::ExternalLinkage,"actual_parent",*module)};
    emit::runtime_local_func_llvm_jit_emit_state_t state{};state.llvm_context_holder=&context;state.llvm_module=module.get();state.llvm_function=parent;
    state.ir_builder=::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(::llvm::BasicBlock::Create(context,"entry",parent));
    auto& builder{*state.ir_builder};cp::function_plan plan{};plan.profile=cp::compilation_profile::create_for_trusted_manager();
    plan.function_generation=1u;plan.expression_bytes=16u;
    cp::safepoint_layout waiting{};waiting.identifier=1u;waiting.opcode_offset=2u;waiting.caller_return_offset=3u;
    waiting.phase=cp::frame_phase::awaiting_call_return;waiting.local_count=1u;waiting.operand_count=1u;
    waiting.slots={{{cp::types::value_kind::i64},true},{{cp::types::value_kind::i64},true}};
    cp::control_layout function{};function.end_offset=15u;waiting.controls.push_back(function);
    auto after{waiting};after.identifier=2u;after.phase=cp::frame_phase::before_opcode;after.opcode_offset=3u;after.caller_return_offset=0u;
    after.operand_count=2u;after.slots.push_back({{cp::types::value_kind::i64},true});plan.sites={waiting,after};
    state.checkpoint_plan=&plan;state.local_types.resize(1u);state.local_pointers.resize(1u);
    state.local_types[0u]=emit::runtime_operand_stack_value_type::i64;
    state.local_pointers[0u]=emit::create_llvm_jit_entry_block_alloca(builder,builder.getInt64Ty(),nullptr,"actual.local");
    builder.CreateStore(builder.getInt64(3u),state.local_pointers[0u]);::llvm::AllocaInst* flags{};
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_initialization_flags(state,0u,flags),"actual original local flags");
    auto const reject{::llvm::BasicBlock::Create(context,"reject",parent)};::llvm::IRBuilder<> rejected{reject};rejected.CreateRet(::llvm::ConstantInt::get(integer,UINTPTR_MAX));
    emit::llvm_jit_checkpoint_resume_dispatch_emit_state dispatch{};
    require(emit::prepare_runtime_local_func_llvm_jit_checkpoint_resume_dispatch(state,flags,parent->getArg(0u),
        builder.CreateIntToPtr(parent->getArg(1u),builder.getPtrTy()),parent->getArg(2u),
        builder.CreateIntToPtr(parent->getArg(3u),builder.getPtrTy()),parent->getArg(4u),reject,dispatch),"actual parent selector");
    // Explicit instruction in the NORMAL block: it does not dominate the saved
    // waiting selector and must NEVER become the restored child's call operand.
    auto const original_prefix{builder.Insert(new ::llvm::FreezeInst(builder.getInt64(5u)),"normal.only.prefix")};
    ::std::array<::llvm::Value*,1u> prefix{original_prefix};::std::array<cp::types::core_value_type,1u> exact{{{cp::types::value_kind::i64}}};
    auto const restored{emit::emit_runtime_local_func_llvm_jit_checkpoint_resume_landing(state,dispatch,plan.sites[0u],prefix,exact,true)};
    require(restored.valid&&restored.selected&&restored.restored_predecessor&&restored.actual_nonlocals.size()==1u,
        "genuine waiting restore-only edge without nondominating normal arguments");
    auto const normal_result{builder.CreateCall(child,{builder.CreateAdd(original_prefix,builder.getInt64(2u))})};auto const normal_return{builder.GetInsertBlock()};
    ::llvm::IRBuilder<> resumed{restored.restored_predecessor};
    auto const child_result{resumed.CreateCall(child,{resumed.CreateAdd(restored.actual_nonlocals[0u],resumed.getInt64(2u))})};
    auto const resumed_return{resumed.GetInsertBlock()};auto const joined{::llvm::BasicBlock::Create(context,"child.return",parent)};
    resumed.CreateBr(joined);builder.CreateBr(joined);builder.SetInsertPoint(joined);
    auto const prefix_phi{builder.CreatePHI(builder.getInt64Ty(),2u)};prefix_phi->addIncoming(original_prefix,normal_return);prefix_phi->addIncoming(restored.actual_nonlocals[0u],resumed_return);
    auto const result_phi{builder.CreatePHI(builder.getInt64Ty(),2u)};result_phi->addIncoming(normal_result,normal_return);result_phi->addIncoming(child_result,resumed_return);
    ::std::array<::llvm::Value*,2u> current{prefix_phi,result_phi};::std::array<cp::types::core_value_type,2u> post_types{{exact[0u],exact[0u]}};
    auto const post{emit::emit_runtime_local_func_llvm_jit_checkpoint_resume_landing(state,dispatch,plan.sites[1u],current,post_types)};
    require(post.valid&&post.selected&&post.actual_nonlocals.size()==2u,"actual post-call SSA merge retains both entry paths");
    auto const local{builder.CreateLoad(builder.getInt64Ty(),state.local_pointers[0u])};
    builder.CreateRet(builder.CreateZExtOrTrunc(builder.CreateAdd(local,builder.CreateAdd(post.actual_nonlocals[0u],post.actual_nonlocals[1u])),integer));
    plan.resume_sites=dispatch.installed_sites;plan.resume_abi_revision=2u;
    require(cp::validate_plan(plan)==cp::status::ok&&plan.resume_sites.size()==2u,"both same-walk waiting/after entry metadata seal");
    auto malformed{plan};malformed.sites[1u].slots[0u].type={cp::types::value_kind::f64};
    require(cp::validate_plan(malformed)!=cp::status::ok,"mismatching paired prefix cannot seal waiting entry");
    require(!::llvm::verifyModule(*module),"real restored child call+normal call+post-call PHIs satisfy dominance");
    ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{::std::move(module)}.setEngineKind(::llvm::EngineKind::JIT).create()};
    require(bool(engine),"actual native component engine");engine->finalizeObject();
    auto const address{engine->getFunctionAddress("actual_parent")},count_address{engine->getGlobalValueAddress("actual_child_calls")};
    require(address!=0u&&count_address!=0u,"actual retained symbols");
    using entry=::std::uintptr_t(*)(::std::uint64_t,::std::uintptr_t,::std::uintptr_t,::std::uintptr_t,::std::uintptr_t);
    // [actual engine-owned named symbols] end
    // [safe] native caller matches the exact constructed component ABI and live
    // counter owner; neither saved native addresses nor wire DATA are invoked.
    auto const run{reinterpret_cast<entry>(static_cast<::std::uintptr_t>(address))};
    auto const count{reinterpret_cast<::std::uint64_t const*>(static_cast<::std::uintptr_t>(count_address))};
    require(run(0u,0u,0u,0u,0u)==16u&&*count==1u,"normal parent invokes original child once");
    ::std::array<::std::byte,3u*cp::native_slot_bytes> packet{};::std::array<::std::uint8_t,1u> initialized{1u};
    ::std::array<::std::uint64_t,3u> values{30u,50u,70u};
    for(::std::size_t i{};i!=values.size();++i)
    {
        // [complete owned 3*16 native packet] end
        // [safe] i<3 and uint64 width<=16 BEFORE offset/copy into each slot.
        ::fast_io::freestanding::my_memcpy(packet.data()+i*cp::native_slot_bytes,::std::addressof(values[i]),sizeof(values[i]));
    }
    require(run(1u,reinterpret_cast<::std::uintptr_t>(packet.data()),2u*cp::native_slot_bytes,
        reinterpret_cast<::std::uintptr_t>(initialized.data()),initialized.size())==133u&&*count==2u,
        "restored parent calls actual child while preserving saved local+prefix");
    require(run(2u,reinterpret_cast<::std::uintptr_t>(packet.data()),packet.size(),
        reinterpret_cast<::std::uintptr_t>(initialized.data()),initialized.size())==150u&&*count==2u,
        "already returned after-call entry bypasses both child calls");
    initialized[0u]=2u;
    require(run(1u,reinterpret_cast<::std::uintptr_t>(packet.data()),2u*cp::native_slot_bytes,
        reinterpret_cast<::std::uintptr_t>(initialized.data()),initialized.size())==UINTPTR_MAX&&*count==2u,
        "invalid execution flag rejects before any restored child call");
    require(run(99u,0u,0u,0u,0u)==UINTPTR_MAX&&*count==2u,"unknown selector cannot execute child");
    ::fast_io::io::println("checkpoint actual waiting child LLVM SSA/native component PASS; VM_restore_authority=false");
}
