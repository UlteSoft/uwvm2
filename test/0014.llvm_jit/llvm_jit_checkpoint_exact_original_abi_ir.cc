// Source/LLVM verifier component only. It owns no actual VM continuation TLS,
// pause ticket, reconstructed root store, native execution or restore authority.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <fast_io.h>
#include <array>
namespace cp=::uwvm2::runtime::checkpoint;
namespace emit=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
namespace compiled=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
namespace storage=::uwvm2::uwvm::runtime::storage;
static void require(bool valid,char const* message)
{ if(!valid) { ::fast_io::io::perrln("checkpoint exact ABI IR: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static void verify_exact(::llvm::CallingConv::ID calling_conv,::std::uint64_t ordinal,bool multivalue=false)
{
    ::llvm::LLVMContext context{};::llvm::Module module{"checkpoint-exact-ABI",context};
    ::std::unique_ptr<::llvm::TargetMachine> target{::llvm::EngineBuilder{}.selectTarget()};
    require(bool(target),"real native target layout");module.setDataLayout(target->createDataLayout());
    ::std::array<emit::runtime_operand_stack_value_type,6u> parameter_types{
        emit::runtime_operand_stack_value_type::i64,emit::runtime_operand_stack_value_type::i32,
        emit::runtime_operand_stack_value_type::f64,emit::runtime_operand_stack_value_type::v128,
        emit::runtime_operand_stack_value_type::i64,emit::runtime_operand_stack_value_type::i64};
    ::std::array<emit::runtime_operand_stack_value_type,2u> result_types{emit::runtime_operand_stack_value_type::i64,emit::runtime_operand_stack_value_type::i32};
    storage::wasm_binfmt1_final_function_type_t declaration{};
    // [owned parameter tuple6][owned result tuple2, active 1 or 2] end
    // [safe] arrays own these exact one-past endpoints for this whole verifier;
    // neither range is a runtime initialized-module permission.
    declaration.parameter={parameter_types.data(),parameter_types.data()+parameter_types.size()};
    declaration.result={result_types.data(),result_types.data()+(multivalue?2u:1u)};
    auto const type{emit::get_llvm_function_type_from_wasm_function_type(context,declaration)};
    require(type!=nullptr,"actual generated six-parameter mixed original ABI");
    auto const original{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"actual_original",module)};
    original->setCallingConv(calling_conv);original->addParamAttr(0u,::llvm::Attribute::InReg);if(!multivalue){original->addRetAttr(::llvm::Attribute::InReg);}
    auto const tail{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"ordinary_tail_target",module)};
    tail->setCallingConv(calling_conv);tail->addParamAttr(0u,::llvm::Attribute::InReg);if(!multivalue){tail->addRetAttr(::llvm::Attribute::InReg);}
    auto const personality_type{::llvm::FunctionType::get(::llvm::Type::getInt32Ty(context),true)};
    auto const personality{::llvm::Function::Create(personality_type,::llvm::GlobalValue::ExternalLinkage,"owned_personality",module)};
    original->setPersonalityFn(personality);
    compiled::local_func_storage_t local{};local.function_type_ptr=::std::addressof(declaration);local.module_id=3u;local.function_index=4u;
    emit::runtime_local_func_llvm_jit_emit_state_t state{};state.llvm_context_holder=::std::addressof(context);
    state.llvm_module=::std::addressof(module);state.llvm_function=state.llvm_public_entry_function=original;
    state.local_func_storage_ptr=::std::addressof(local);state.debug_activation_enabled=true;state.debug_compiled_function_generation=ordinal;
    auto const entry{::llvm::BasicBlock::Create(context,"entry",original)};
    state.ir_builder=::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(entry);auto& builder{*state.ir_builder};
    cp::function_plan plan{};plan.profile=cp::compilation_profile::create_for_trusted_manager();plan.module=3u;plan.function=4u;
    plan.expression_bytes=2u;plan.function_generation=ordinal;
    cp::safepoint_layout site{};site.identifier=1u;site.local_count=parameter_types.size();
    for(auto parameter:parameter_types)
    {
        cp::types::core_value_type exact{};
        switch(parameter)
        {
            case emit::runtime_operand_stack_value_type::i64:exact.kind=cp::types::value_kind::i64;break;
            case emit::runtime_operand_stack_value_type::i32:exact.kind=cp::types::value_kind::i32;break;
            case emit::runtime_operand_stack_value_type::f64:exact.kind=cp::types::value_kind::f64;break;
            case emit::runtime_operand_stack_value_type::v128:exact.kind=cp::types::value_kind::v128;break;
            default: ::fast_io::fast_terminate();
        }
        site.slots.push_back({exact,true});
    }
    cp::control_layout control{};control.end_offset=1u;control.declared_results={{cp::types::value_kind::i64}};if(multivalue){control.declared_results.push_back({cp::types::value_kind::i32});}
    site.controls.push_back(control);plan.sites.push_back(site);state.checkpoint_plan=::std::addressof(plan);
    auto const activation_type{::llvm::FunctionType::get(builder.getInt64Ty(),false)};
    auto const activation{::llvm::Function::Create(activation_type,::llvm::GlobalValue::ExternalLinkage,"component_activation_only",module)};
    state.debug_activation_token=builder.CreateCall(activation); // component SSA, NEVER a runtime authority token
    auto const integer{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t)*CHAR_BIT))};
    ::std::array<::llvm::Value*,5u> zero{builder.getInt64(0u),::llvm::ConstantPointerNull::get(builder.getPtrTy()),
        ::llvm::ConstantInt::get(integer,0u),::llvm::ConstantPointerNull::get(builder.getPtrTy()),::llvm::ConstantInt::get(integer,0u)};
    for(::std::size_t i{};i!=zero.size();++i)
    {
        // [fixed actual component SSA handles0..5] end
        // [safe] i<5 before selecting/storing either handle in original owner.
        state.checkpoint_resume.placeholders[i]=builder.Insert(new ::llvm::FreezeInst(zero[i]),"checkpoint.normal.zero");
    }
    auto const normal{::llvm::BasicBlock::Create(context,"normal",original)};
    auto const restored{::llvm::BasicBlock::Create(context,"restored",original)};
    auto const rejected{::llvm::BasicBlock::Create(context,"reject",original)};
    auto& dispatch{state.checkpoint_resume};dispatch.actual_state=::std::addressof(state);dispatch.actual_plan=::std::addressof(plan);
    dispatch.dispatch=builder.CreateSwitch(dispatch.placeholders[0u],rejected);dispatch.dispatch->addCase(builder.getInt64(0u),normal);
    dispatch.dispatch->addCase(builder.getInt64(1u),restored);dispatch.installed_sites={1u};dispatch.reject=rejected;
    ::std::vector<::llvm::Value*> arguments{};for(auto& arg:original->args()){arguments.push_back(::std::addressof(arg));}
    builder.SetInsertPoint(normal);
    auto const ordinary_recursion{builder.CreateCall(original,arguments)};ordinary_recursion->setCallingConv(calling_conv);
    ordinary_recursion->setAttributes(original->getAttributes());
    auto const transfer{builder.CreateCall(tail,arguments)};transfer->setCallingConv(calling_conv);transfer->setAttributes(tail->getAttributes());
    transfer->setTailCallKind(::llvm::CallInst::TCK_MustTail);if(multivalue){builder.CreateRetVoid();}else{builder.CreateRet(transfer);}
    builder.SetInsertPoint(restored);if(multivalue){builder.CreateRetVoid();}else{builder.CreateRet(original->getArg(0u));}
    builder.SetInsertPoint(rejected);builder.CreateUnreachable();
    require(!::llvm::verifyFunction(*original),"actual original musttail/ordinary recursion IR verifies");
    auto const original_blocks{original->size()};auto const original_entry_size{entry->size()};
    require(emit::finalize_runtime_local_func_llvm_jit_checkpoint_resume_entry(state),"actual production exact ABI clone transform");
    auto const clone{module.getFunction("actual_original.checkpoint.resume.v2")};
    auto const raw{module.getFunction("actual_original.checkpoint.resume.v2.raw")};
    require(clone&&raw&&clone->getFunctionType()==original->getFunctionType()&&clone->arg_size()==original->arg_size()&&
        clone->getCallingConv()==calling_conv&&clone->hasFnAttribute(::llvm::Attribute::NoInline)&&clone->getAttributes().getParamAttrs(0u)==original->getAttributes().getParamAttrs(0u)&&
        clone->getAttributes().getRetAttrs()==original->getAttributes().getRetAttrs()&&clone->getPersonalityFn()==personality,
        "exact original type/CC/parameter/return/personality survive clone");
    ::std::size_t tails{},selfcalls{},input_queries{},raw_clone_calls{},raw_binds{},public_inputs{};
    for(auto const& block:*clone)for(auto const& instruction:block)
    {
        auto const call{::llvm::dyn_cast<::llvm::CallInst>(::std::addressof(instruction))};if(call==nullptr){continue;}
        if(call->isMustTailCall())
        {
            ++tails;require(call->getCalledFunction()==tail&&::llvm::isa<::llvm::ReturnInst>(call->getNextNode()),"unchanged immediate musttail target+ret");
        }
        if(call->getCalledFunction()==original){++selfcalls;}
        if(call->arg_size()==2u&&call->getType()->isIntegerTy(64u)&&call->doesNotThrow()){++input_queries;}
    }
    for(auto const& block:*raw)for(auto const& instruction:block)
    {
        auto const call{::llvm::dyn_cast<::llvm::CallInst>(::std::addressof(instruction))};if(call==nullptr){continue;}
        if(call->getCalledFunction()==clone)
        {
            ++raw_clone_calls;require(call->arg_size()==original->arg_size()&&call->getCallingConv()==calling_conv&&
                call->getAttributes().getParamAttrs(0u)==clone->getAttributes().getParamAttrs(0u)&&
                call->getAttributes().getRetAttrs()==clone->getAttributes().getRetAttrs(),"raw call preserves exact original ABI attrs");
        }
        if(call->arg_size()==10u&&call->doesNotThrow()){++raw_binds;}
    }
    for(auto const& block:*original)for(auto const& instruction:block)
    {if(::llvm::isa<::llvm::FreezeInst>(instruction)){++public_inputs;}}
    require(tails==1u&&selfcalls==1u&&input_queries==5u&&raw_clone_calls==1u&&raw_binds==1u&&public_inputs==5u&&
        original->size()==original_blocks&&entry->size()==original_entry_size,"selected clone only; public body/normal zeros unchanged and no self replay");
    require(plan.resume_abi_revision==2u&&plan.resume_sites==dispatch.installed_sites&&cp::validate_plan(plan)==cp::status::ok,
        "verified exact clone sealed only under ABI2");
    auto old{plan};old.resume_abi_revision=1u;require(cp::validate_plan(old)==cp::status::invalid_plan,"stale hidden-input ABI1 DATA cannot seal");
    require(!::llvm::verifyModule(module),"complete actual exact ABI2 module verifier");
}
int main()
{
    require(!::llvm::InitializeNativeTarget()&&!::llvm::InitializeNativeTargetAsmPrinter(),"native LLVM verifier target");
    verify_exact(::llvm::CallingConv::C,1u);verify_exact(::llvm::CallingConv::Fast,2u);verify_exact(::llvm::CallingConv::C,3u,true);
    ::fast_io::io::println("checkpoint_exact_original_ABI: SOURCE/LLVM verifier component PASS; native_restore_authority=false");
}
