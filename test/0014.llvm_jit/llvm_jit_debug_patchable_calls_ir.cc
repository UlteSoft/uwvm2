// Actual Wasm-to-LLVM qualification only. This does not publish replacement code
// or execute a synthetic target: runtime replacement/retirement remains separate.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/Verifier.h>
#include <fstream>
#include <string>
#include <vector>
namespace strict = uwvm2test::uwvm_int_strict;
namespace compiler = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x); std::abort(); } } while(false)
static strict::byte_vec fixture()
{
    strict::module_builder module;
    module.has_memory=true; module.memory_min=1;
    module.has_table=true; module.table_min=1;
    auto add=[&](strict::func_type type,std::initializer_list<unsigned> code)
    {
        strict::func_body body;
        for(auto b:code) { strict::append_u8(body.code,static_cast<std::uint8_t>(b)); }
        module.add_func(std::move(type),std::move(body));
    };
    strict::func_type scalar{{strict::k_val_i32},{strict::k_val_i32}};
    add(scalar,{0x20,0,0x41,1,0x6a,0x0b}); // 0: leaf
    add(scalar,{0x20,0,0x10,0,0x0b}); // 1: direct call
    add(scalar,{0x20,0,0x12,0,0x0b}); // 2: direct tail
    add(scalar,{0x20,0,0x45,0x04,0x40,0x41,7,0x0f,0x0b,0x20,0,0x41,1,0x6b,0x12,3,0x0b}); // 3: self tail
    add(scalar,{0x20,0,0x41,0,0x11,0,0,0x0b}); // 4: call_indirect type 0, table 0
    add(scalar,{0x20,0,0x41,0,0x13,0,0,0x0b}); // 5: return_call_indirect
    strict::func_type tuple{{strict::k_val_i32,strict::k_val_i64,strict::k_val_f32,strict::k_val_f64,0x7bu},
                            {strict::k_val_i32,strict::k_val_i64,strict::k_val_f32,strict::k_val_f64,0x7bu}};
    add(tuple,{0x20,0,0x20,1,0x20,2,0x20,3,0x20,4,0x0b}); // 6: all scalar/vector parameters, hidden result tuple
    add(tuple,{0x20,0,0x20,1,0x20,2,0x20,3,0x20,4,0x10,6,0x0b}); // 7: tuple direct
    add(tuple,{0x20,0,0x20,1,0x20,2,0x20,3,0x20,4,0x12,6,0x0b}); // 8: tuple tail
    // 9: normal mmap access baseline. No replacement table operation belongs in this function.
    add(scalar,{0x41,0,0x20,0,0x36,2,0,0x41,0,0x28,2,0,0x0b});
    strict::element_segment element{}; element.func_indices={0};
    for(unsigned b:{0x41u,0u,0x0bu}) { strict::append_u8(element.offset_expr,static_cast<std::uint8_t>(b)); }
    module.elements.push_back(std::move(element));
    return module.build();
}
int main(int argc,char** argv)
{
    REQUIRE(argc==3);
    bool const instruction{std::string{argv[2]}=="instruction"};
    REQUIRE(instruction||std::string{argv[2]}=="unwind");
    REQUIRE(!llvm::InitializeNativeTarget()&&!llvm::InitializeNativeTargetAsmPrinter());
    std::unique_ptr<llvm::TargetMachine> machine{llvm::EngineBuilder{}.selectTarget()}; REQUIRE(machine);
    auto bytes{fixture()};
    auto features{strict::make_wasm1p1_feature_parameter()};
    auto& policy{uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
    policy.disable_tail_call=false; policy.disable_simd=false; policy.disable_multi_value=false;
    auto prepared{strict::prepare_runtime_from_wasm(bytes,u8"debug-patchable-ir",{},features)};
    std::ofstream fixture_file(std::string{argv[1]}+"/fixture.wasm",std::ios::binary);
    fixture_file.write(reinterpret_cast<char const*>(bytes.data()),bytes.size()); fixture_file.close();
    compiler::compile_option options{};
    options.validator_feature_parameter=&features;
    options.emit_call_stack_frames=instruction;
    options.emit_unwind_call_stack_frames=!instruction;
    options.native_exception_target_machine=machine.get();
    options.compilation_mode=compiler::llvm_jit_compilation_mode::full;
    std::vector<std::uintptr_t> slots(prepared.mod->local_defined_function_vec_storage.size());
    REQUIRE(slots.size()==10);
    auto translate=[&](char const* label,bool emitted,bool patchable)
    {
        uwvm2::validation::error::code_validation_error_impl error{};
        auto compiled{compiler::compile_all_from_uwvm(*prepared.mod,options,error,0)};
        REQUIRE(error.err_code==uwvm2::validation::error::code_validation_error_code::ok);
        REQUIRE(compiled.llvm_jit_module.emitted==emitted);
        if(!emitted)
        {
            REQUIRE(!compiled.llvm_jit_module.llvm_module);
            std::printf("PASS rejected %s\n",label); std::fflush(stdout); return;
        }
        auto& module{*compiled.llvm_jit_module.llvm_module}; REQUIRE(!llvm::verifyModule(module,&llvm::errs()));
        std::size_t slot_loads{},musttails{},self_backedges{},table_typed_loads{},live_target_slots{};
        for(auto const& function:module) for(auto const& block:function) for(auto const& item:block)
        {
            if(auto const allocation{llvm::dyn_cast<llvm::AllocaInst>(&item)};
               allocation&&allocation->getName()=="debug.call_indirect.target.slot")
            {
                REQUIRE(options.emit_debug_safe_points);
                REQUIRE(allocation->getAlign().value()>=alignof(uwvm2::uwvm::runtime::storage::llvm_jit_raw_call_target_t));
                ++live_target_slots;
            }
            if(auto const load{llvm::dyn_cast<llvm::LoadInst>(&item)})
            {
                if(load->getName()=="call.debug.full.target.address")
                {
                    REQUIRE(patchable&&load->isVolatile()&&load->getOrdering()==llvm::AtomicOrdering::Acquire);
                    REQUIRE(load->getAlign().value()>=std::atomic_ref<std::uintptr_t>::required_alignment);
                    // IRBuilder may fold the constant array GEP into a
                    // ConstantExpr, or erase the [0,0] GEP entirely with opaque
                    // pointers. All three forms must retain the exact array.
                    auto const pointer{load->getPointerOperand()};
                    llvm::GlobalVariable const* global{};
                    if(auto const gep{llvm::dyn_cast<llvm::GEPOperator>(pointer)})
                    {
                        REQUIRE(gep->isInBounds()&&gep->getNumIndices()==2);
                        auto const array{llvm::dyn_cast<llvm::ArrayType>(gep->getSourceElementType())}; REQUIRE(array&&array->getNumElements()==slots.size());
                        auto const zero{llvm::dyn_cast<llvm::ConstantInt>(gep->getOperand(1))}; REQUIRE(zero&&zero->isZero());
                        auto const index{llvm::dyn_cast<llvm::ConstantInt>(gep->getOperand(2))}; REQUIRE(index&&index->getZExtValue()<slots.size());
                        global=llvm::dyn_cast<llvm::GlobalVariable>(gep->getPointerOperand());
                    }
                    else { global=llvm::dyn_cast<llvm::GlobalVariable>(pointer); }
                    REQUIRE(global&&global->getName().ends_with("_debug_full_typed_targets"));
                    auto const allocation{llvm::dyn_cast<llvm::ArrayType>(global->getValueType())};
                    REQUIRE(allocation&&allocation->getNumElements()==slots.size()&&global->isDeclaration());
                    ++slot_loads;
                }
                if(load->getName()=="call_indirect.typed.entry.addr")
                {
                    REQUIRE(load->isVolatile());
                    REQUIRE(load->getOrdering()==(patchable?llvm::AtomicOrdering::Acquire:llvm::AtomicOrdering::NotAtomic));
                    ++table_typed_loads;
                }
            }
            if(auto const branch{llvm::dyn_cast<llvm::BranchInst>(&item)};branch&&branch->isUnconditional()&&
                branch->getSuccessor(0)->getName()=="body"&&block.getName()!="entry") { ++self_backedges; }
            auto const call{llvm::dyn_cast<llvm::CallInst>(&item)};
            if(call&&call->isMustTailCall())
            {
                REQUIRE(llvm::isa<llvm::ReturnInst>(call->getNextNode()));
                REQUIRE(call->getCallingConv()==function.getCallingConv());
                REQUIRE(call->getFunctionType()->getReturnType()==function.getReturnType());
                ++musttails;
            }
        }
        REQUIRE(table_typed_loads==(options.emit_debug_safe_points?0uz:2uz));
        REQUIRE(live_target_slots==(options.emit_debug_safe_points?2uz:0uz));
        REQUIRE(slot_loads==(patchable?5uz:0uz));
        REQUIRE(self_backedges==(patchable?0uz:1uz));
        REQUIRE(musttails==(patchable?5uz:4uz));
        for(std::size_t index:{1uz,2uz,3uz,7uz,8uz})
        {
            auto const name{compiler::details::get_llvm_wasm_function_name(*prepared.mod,index)};
            auto const function{module.getFunction(llvm::StringRef{reinterpret_cast<char const*>(name.data()),name.size()})}; REQUIRE(function);
            std::size_t indirect_typed{};
            for(auto const& block:*function) for(auto const& item:block)
                if(auto const call{llvm::dyn_cast<llvm::CallBase>(&item)};call&&!call->getCalledFunction()&&call->getCallingConv()==function->getCallingConv())
                    { ++indirect_typed; }
            REQUIRE(!patchable||indirect_typed==1);
        }
#ifndef UWVM2TEST_DEBUG_PATCHABLE_BASELINE
        // Reject a mismatched exact signature and an out-of-range index before
        // the helper publishes a symbol or emits a pointer/GEP/load.
        if(patchable)
        {
            compiler::details::runtime_local_func_llvm_jit_emit_state_t state{};
            state.local_func_storage_ptr=&compiled.local_funcs.index_unchecked(1);
            state.debug_full_patchable_typed_target_base_address=reinterpret_cast<std::uintptr_t>(slots.data());
            state.debug_full_patchable_typed_target_count=slots.size();
            auto& context{module.getContext()};
            auto const wrong{llvm::FunctionType::get(llvm::Type::getVoidTy(context),false)};
            auto const fn{llvm::Function::Create(wrong,llvm::GlobalValue::InternalLinkage,"bounds-probe",module)};
            auto const block{llvm::BasicBlock::Create(context,"entry",fn)};
            state.ir_builder=uwvm2::utils::container::make_delete_owned<llvm::IRBuilder<>>(block);
            REQUIRE(!compiler::details::emit_runtime_local_func_llvm_jit_debug_full_typed_target(state,*prepared.mod,0,wrong));
            REQUIRE(!compiler::details::emit_runtime_local_func_llvm_jit_debug_full_typed_target(state,*prepared.mod,slots.size(),wrong));
            REQUIRE(block->empty()); state.ir_builder.reset(); fn->eraseFromParent();
        }
#endif
        std::error_code ec; llvm::raw_fd_ostream output(std::string{argv[1]}+"/"+label+".ll",ec); REQUIRE(!ec);
        module.print(output,nullptr); output.flush();
        std::printf("PASS %s slots=%zu musttail=%zu self_backedges=%zu indirect=%zu\n",label,slot_loads,musttails,self_backedges,table_typed_loads); std::fflush(stdout);
    };
    translate("off-normal",true,false);
    options.emit_debug_safe_points=true;
    translate("off-debug",true,false);
#ifndef UWVM2TEST_DEBUG_PATCHABLE_BASELINE
    options.debug_full_patchable_typed_target_base_address=reinterpret_cast<std::uintptr_t>(slots.data());
    options.debug_full_patchable_typed_target_count=slots.size();
    translate("on-debug",true,true);
    auto const valid{options};
    options.emit_debug_safe_points=false; translate("requires-debug",false,false); options=valid;
    for(auto mode:{compiler::llvm_jit_compilation_mode::unspecified,compiler::llvm_jit_compilation_mode::lazy,compiler::llvm_jit_compilation_mode::tiered})
    { options.compilation_mode=mode; translate("requires-full",false,false); } options=valid;
    options.debug_full_patchable_typed_target_base_address=0; translate("missing-base",false,false); options=valid;
    ++options.debug_full_patchable_typed_target_base_address; translate("misaligned-base",false,false); options=valid;
    for(auto count:{0uz,slots.size()-1,slots.size()+1,SIZE_MAX})
    { options.debug_full_patchable_typed_target_count=count; translate("invalid-count",false,false); } options=valid;
    options.debug_full_patchable_typed_target_base_address=UINTPTR_MAX-(alignof(std::uintptr_t)-1);
    translate("overflow-base",false,false); options=valid;
    options.route_wasm_calls_through_runtime_bridge=true; translate("raw-route-conflict",false,false); options=valid;
#if !defined(UWVM2TEST_ROS)
    options.lazy_defined_targets_are_atomic=true; translate("lazy-target-conflict",false,false); options=valid;
    options.emit_tiered_loop_reentry_entries=true; translate("osr-conflict",false,false);
#endif
#endif
}
