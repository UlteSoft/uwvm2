// Actual initial producer -> checked snapshot emitter -> native MCJIT.
// Mock DATA storage/identities grant no VM admission, roots or restore rights.
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
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace cp=uwvm2::runtime::checkpoint;
namespace emit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
namespace support=uwvm2::runtime::compiler::llvm_jit::details;
constexpr std::size_t count=8u, prefix=count*cp::observer_local_metadata_bytes, extent=prefix+count*16u;
static std::array<std::byte,cp::observer_site_bytes+extent+8u> storage;
static unsigned calls;
static void require(bool b,char const* why)
{ if(!b) { std::fprintf(stderr,"initial snapshot IR: %s\n",why);std::abort(); } }
namespace uwvm2::runtime::lib::details
{
extern "C++" llvm_jit_checkpoint_observer_entry_result llvm_jit_checkpoint_observer_activation_enter_abi_bridge(
    std::size_t bytes,std::uintptr_t module,std::uintptr_t function,std::uint64_t generation) noexcept
{ require(module==7u && generation==13u && ((function==11u && bytes==extent) ||
    (function==12u && bytes==cp::observer_local_metadata_bytes+16u)),"actual fused entry arguments");++calls;
  storage.fill(std::byte{0xa5u});std::memset(storage.data()+cp::observer_site_bytes,0,extent);
  return {0xdecaf123u,reinterpret_cast<std::uintptr_t>(storage.data()+cp::observer_site_bytes)}; }
extern "C++" llvm_jit_checkpoint_observer_entry_result llvm_jit_checkpoint_observer_small_enter_abi_bridge(
    std::size_t bytes,std::uintptr_t module,std::uintptr_t function,std::uint64_t generation) noexcept
{ require(module==7u && generation==13u && function==12u && bytes==cp::observer_local_metadata_bytes+16u,"actual small entry arguments");++calls;
  storage.fill(std::byte{0xa5u});std::memset(storage.data(),0,cp::observer_site_bytes+bytes);
  return {0xdecaf123u,reinterpret_cast<std::uintptr_t>(storage.data()+cp::observer_site_bytes)}; }
extern "C++" std::uint64_t llvm_jit_checkpoint_activation_enter_abi_bridge(std::uintptr_t,std::uintptr_t,std::uint64_t) noexcept { std::abort(); }
extern "C++" std::uint64_t llvm_jit_debug_activation_enter_abi_bridge(std::uintptr_t,std::uintptr_t,std::uint64_t) noexcept { std::abort(); }
extern "C++" std::uintptr_t llvm_jit_checkpoint_observer_workspace_abi_bridge(std::size_t) noexcept { std::abort(); }
extern "C++" void llvm_jit_checkpoint_materialize_abi_bridge(std::uint64_t,std::uint64_t,std::uintptr_t,std::uintptr_t) noexcept { std::abort(); }
}
int main(int argc,char** argv)
{
#if defined(__linux__) && defined(__x86_64__)
    require(argc<=2,"optional verified IR output");
    require(!llvm::InitializeNativeTarget() && !llvm::InitializeNativeTargetAsmPrinter(),"native target");
    llvm::EngineBuilder choose;choose.setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Less);
    std::unique_ptr<llvm::TargetMachine> target{choose.selectTarget()};require(bool(target),"native target machine");
    llvm::LLVMContext context;auto module=std::make_unique<llvm::Module>("initial-observer-snapshot",context);
    module->setTargetTriple(target->getTargetTriple());module->setDataLayout(target->createDataLayout());
    auto function=llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getInt64Ty(context),{},false),llvm::Function::ExternalLinkage,"probe",*module);
    cp::function_plan plan;plan.profile=cp::compilation_profile::create_for_trusted_observer();plan.expression_bytes=32u;plan.function_generation=13u;
    cp::safepoint_layout entry{};entry.identifier=1u;entry.local_count=count;
    cp::control_layout root{};root.end_offset=31u;entry.controls.push_back(root);
    constexpr std::array types{emit::runtime_operand_stack_value_type::i32,emit::runtime_operand_stack_value_type::i64,
        emit::runtime_operand_stack_value_type::f32,emit::runtime_operand_stack_value_type::f64,
        emit::runtime_operand_stack_value_type::v128,emit::runtime_operand_stack_value_type::funcref,
        emit::runtime_operand_stack_value_type::externref,emit::runtime_operand_stack_value_type::funcref};
    constexpr std::array kinds{cp::types::value_kind::i32,cp::types::value_kind::i64,cp::types::value_kind::f32,
        cp::types::value_kind::f64,cp::types::value_kind::v128};
    for(std::size_t i{};i!=count;++i)
    {
        cp::types::core_value_type type{};
        if(i<5u) { type.kind=kinds[i]; }
        else { type={cp::types::value_kind::reference,{static_cast<std::int_least64_t>(i==6u ? cp::types::abstract_heap_type::extern_ : cp::types::abstract_heap_type::func)},i!=7u}; }
        entry.slots.push_back({type,i!=7u});
    }
    plan.sites.push_back(entry);require(cp::validate_site(plan.sites.front(),plan)==cp::status::ok,"real initial metadata");
    uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::local_func_storage_t local;local.module_id=7u;local.function_index=11u;
    emit::runtime_local_func_llvm_jit_emit_state_t state;state.checkpoint_plan=&plan;state.local_func_storage_ptr=&local;
    state.llvm_context_holder=&context;state.llvm_module=module.get();state.llvm_function=function;
    state.debug_activation_enabled=true;state.emit_debug_safe_points=true;state.debug_compiled_function_generation=13u;
    state.ir_builder=uwvm2::utils::container::make_delete_owned<llvm::IRBuilder<>>(llvm::BasicBlock::Create(context,"entry",function));auto& ir=*state.ir_builder;
    constexpr std::array<std::uint64_t,count> low{0x87654321u,0xfedcba9876543210u,0x7f812345u,
        0x7ff0123456789abcu,0x1020304050607080u,0x123456789abcdef0u,0u,0u};
    constexpr std::array<std::uint64_t,count> high{0u,0u,0u,0u,0xa0b0c0d0e0f00102u,0x1122334400000003u,0u,0u};
    for(std::size_t i{};i!=count;++i)
    {
        auto type=emit::get_llvm_type_from_wasm_value_type(context,types[i]);auto slot=emit::create_llvm_jit_entry_block_alloca(ir,type,nullptr,"actual.initial.local");
        state.local_types.push_back(types[i]);state.local_pointers.push_back(slot);
        if(i<2u) { slot->setMetadata("uwvm.debug.numeric.mutable.gp",llvm::MDNode::get(context,{})); }
        if(i==7u) { continue; } // Genuinely never initialized, never readable.
        unsigned width=static_cast<unsigned>(emit::get_runtime_wasm_value_type_abi_size(types[i])*8u);llvm::APInt bits{width,low[i]};
        if(width==128u) { bits|=llvm::APInt{128u,high[i]}.shl(64u); }
        ir.CreateStore(llvm::ConstantExpr::getBitCast(llvm::ConstantInt::get(context,bits),type),slot);
    }
    require(emit::emit_runtime_local_func_llvm_jit_debug_activation_enter(state),"actual fused entry producer");
    require(emit::prepare_runtime_local_func_llvm_jit_checkpoint_observer_workspace(state),"actual workspace owner");
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_initialization_flags(state,6u,state.checkpoint_executed_local_flags),"executed initial flags");
    require(state.checkpoint_observer_controls==nullptr,"entry precedes opcode controls");
    auto from=ir.GetInsertBlock()->size();
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_entry(state),"actual initial capture");
    require(state.snapshot_local_pointers==nullptr && state.checkpoint_local_snapshot_copy==nullptr,"entry has no local address escape");
    unsigned index{},byte_loads{};
    for(auto& instruction:*ir.GetInsertBlock())
    { if(index++>=from) { if(auto load=llvm::dyn_cast<llvm::LoadInst>(&instruction)) { if(load->getType()->isIntegerTy(8u)) { ++byte_loads; } } } }
    require(byte_loads==0u,"initial capture does not reread known initialization flags");
    // Metadata copies and a later bound opcode cannot reuse initial authority.
    auto flags=state.checkpoint_observer_workspace;auto values=ir.CreateInBoundsGEP(ir.getInt8Ty(),flags,ir.getInt64(prefix));auto saved=ir.CreateInBoundsGEP(ir.getInt8Ty(),flags,ir.getInt64(count));
    auto copy=plan.sites.front();require(!emit::emit_runtime_local_func_llvm_jit_snapshot_copy(state,count,values,saved,flags,false,false,&copy),"copied metadata grants no initial capture");
    emit::llvm_jit_checkpoint_observer_control_map controls{};state.checkpoint_observer_controls=&controls;
    require(!emit::emit_runtime_local_func_llvm_jit_snapshot_copy(state,count,values,saved,flags,false,false,&plan.sites.front()),"bound opcode rejects declaration-time flags");state.checkpoint_observer_controls=nullptr;
    ir.CreateRet(state.debug_activation_token);
    require(!llvm::verifyModule(*module,&llvm::errs()),"actual complete initial IR");
    require(support::promote_runtime_llvm_jit_debug_numeric_locals(*function)==4u,"FP/vector declarations and unread absent slot promote; GP arguments retain original cells");
    unsigned retained_gp{};
    for(auto& instruction:function->getEntryBlock())
    { if(auto slot=llvm::dyn_cast<llvm::AllocaInst>(&instruction);slot!=nullptr && slot->getMetadata("uwvm.debug.entry.stack.gp")!=nullptr) { ++retained_gp; } }
    require(retained_gp==2u,"both actual GP storage hints survive promotion");
    require(!llvm::verifyModule(*module,&llvm::errs()),"actual promoted initial IR");
    // Pure GP entries retain the original tiny copier and stack argument.
    cp::function_plan gp_plan;gp_plan.profile=cp::compilation_profile::create_for_trusted_observer();gp_plan.expression_bytes=32u;gp_plan.function_generation=13u;
    auto gp_entry=entry;gp_entry.local_count=1u;gp_entry.slots.resize(1u);gp_entry.controls.front().declared_results.push_back({cp::types::value_kind::i32});gp_plan.sites.push_back(gp_entry);
    auto gp=llvm::Function::Create(llvm::FunctionType::get(ir.getInt32Ty(),{ir.getInt32Ty()},false),llvm::Function::ExternalLinkage,"gp_probe",*module);
    auto gp_local=local;gp_local.function_index=12u;
    emit::runtime_local_func_llvm_jit_emit_state_t gs;gs.checkpoint_plan=&gp_plan;gs.local_func_storage_ptr=&gp_local;
    gs.llvm_context_holder=&context;gs.llvm_module=module.get();gs.llvm_function=gp;
    gs.debug_activation_enabled=true;gs.emit_debug_safe_points=true;gs.debug_compiled_function_generation=13u;
    gs.ir_builder=uwvm2::utils::container::make_delete_owned<llvm::IRBuilder<>>(llvm::BasicBlock::Create(context,"entry",gp));auto& gi=*gs.ir_builder;
    gs.local_types.push_back(emit::runtime_operand_stack_value_type::i32);auto gp_slot=emit::create_llvm_jit_entry_block_alloca(gi,gi.getInt32Ty(),nullptr,"actual.gp.argument");gs.local_pointers.push_back(gp_slot);
    gp_slot->setMetadata("uwvm.debug.numeric.mutable.gp",llvm::MDNode::get(context,{}));gi.CreateStore(gp->getArg(0u),gp_slot);
    require(emit::emit_runtime_local_func_llvm_jit_debug_activation_enter(gs) &&
        emit::prepare_runtime_local_func_llvm_jit_checkpoint_observer_workspace(gs) &&
        emit::emit_runtime_local_func_llvm_jit_checkpoint_initialization_flags(gs,1u,gs.checkpoint_executed_local_flags) &&
        emit::emit_runtime_local_func_llvm_jit_checkpoint_entry(gs),"actual pure GP initial producer");
    require(gs.snapshot_local_pointers!=nullptr && gs.checkpoint_local_snapshot_copy!=nullptr,"pure GP keeps original tiny copier");
    gi.CreateRet(gi.CreateAdd(gi.CreateLoad(gi.getInt32Ty(),gp_slot),gi.getInt32(3u)));
    require(support::promote_runtime_llvm_jit_debug_numeric_locals(*gp)==0u,"pure GP argument retains original address-bound stack cell");
    require(!llvm::verifyModule(*module,&llvm::errs()),"both actual initial paths verified");
    if(argc==2) { std::error_code error;llvm::raw_fd_ostream output{argv[1],error};require(!error,"IR output");module->print(output,nullptr); }
    llvm::EngineBuilder build(std::move(module));build.setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Less);
    build.setMCJITMemoryManager(std::make_unique<support::runtime_llvm_jit_section_memory_manager>());
    std::unique_ptr<llvm::ExecutionEngine> engine{build.create(target.release())};require(bool(engine),"native MCJIT");engine->finalizeObject();require(!engine->hasError(),"native materialization");
    auto probe=reinterpret_cast<std::uint64_t(*)()>(engine->getFunctionAddress("probe"));require(probe && probe()==0xdecaf123u && calls==1u,"actual native entry and identity");
    std::uint64_t site{};std::memcpy(&site,storage.data(),sizeof(site));require(site==1u,"real initial publication");
    auto base=storage.data()+cp::observer_site_bytes;
    for(std::size_t i{};i!=count;++i)
    {
        require(base[i]==static_cast<std::byte>(i==7u ? 0u:1u) && base[count+i]==base[i],"full original initialization flags");
        std::array<std::byte,16> want{};
        if(i!=7u) { auto size=emit::get_runtime_wasm_value_type_abi_size(types[i]);std::memcpy(want.data(),&low[i],std::min(size,std::size_t{8u}));if(size==16u) { std::memcpy(want.data()+8u,&high[i],8u); } }
        require(std::memcmp(base+prefix+i*16u,want.data(),16u)==0,"all native bits, padding and absent references exact");
    }
    for(std::size_t i=2u*count;i!=prefix;++i) { require(base[i]==std::byte{},"unused address-table bytes remain cleared"); }
    for(std::size_t i=cp::observer_site_bytes+extent;i!=storage.size();++i) { require(storage[i]==std::byte{0xa5u},"allocation guard unchanged"); }
    auto gp_probe=reinterpret_cast<std::uint32_t(*)(std::uint32_t)>(engine->getFunctionAddress("gp_probe"));
    require(gp_probe && gp_probe(0x87654321u)==0x87654324u && calls==2u,"pure GP actual native value preserved");
    std::uint32_t saved_gp{};std::memcpy(&saved_gp,storage.data()+cp::observer_site_bytes+cp::observer_local_metadata_bytes,4u);
    require(saved_gp==0x87654321u,"pure GP original entry snapshot exact");
    std::puts("PASS actual initial producer: full scalar/vector/reference bytes, sNaNs, genuinely absent reference, flags, owner, publication and numeric SSA; no repeated flag reads; DATA only");
#endif
}
