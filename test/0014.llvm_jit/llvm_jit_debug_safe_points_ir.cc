// Actual Wasm-to-LLVM IR qualification; generated guest execution is covered
// separately by the host cooperative-pause integration test. No native stubs.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Verifier.h>
#include <fstream>
#include <string>
#include <set>
namespace strict = uwvm2test::uwvm_int_strict;
namespace compiler = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
#undef UWVM2TEST_REQUIRE
#define REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x); std::abort(); } } while(false)
static strict::byte_vec fixture()
{
    strict::module_builder module;
    module.has_memory=true; module.memory_min=1;
    auto add=[&](std::initializer_list<unsigned> code)
    {
        strict::func_body body;
        for(auto b:code) { strict::append_u8(body.code,static_cast<std::uint8_t>(b)); }
        module.add_func({{strict::k_val_i32},{strict::k_val_i32}},std::move(body));
    };
    add({0x20,0,0x41,1,0x6a,0x0b}); // callee
    add({0x20,0,0x12,0,0x0b}); // genuine cross-function return_call
    // block; loop at expression byte 2; memory read/write and decrement; br0.
    add({0x02,0x40,0x03,0x40,0x20,0,0x45,0x0d,1,
         0x41,0,0x41,0,0x28,2,0,0x41,1,0x6a,0x36,2,0,
         0x20,0,0x41,1,0x6b,0x21,0,0x0c,0,0x0b,0x0b,0x41,0,0x28,2,0,0x0b});
    // Self-tail recursion must return through the entry safe point after loopification.
    add({0x20,0,0x45,0x04,0x40,0x41,7,0x0f,0x0b,0x20,0,0x41,1,0x6b,0x12,3,0x0b});
    return module.build();
}
int main(int argc,char** argv)
{
    REQUIRE(argc==3);
    bool const instruction{std::string{argv[2]}=="instruction"};
    REQUIRE(instruction||std::string{argv[2]}=="unwind");
    REQUIRE(!llvm::InitializeNativeTarget()&&!llvm::InitializeNativeTargetAsmPrinter());
    std::unique_ptr<llvm::TargetMachine> machine{llvm::EngineBuilder{}.selectTarget()};
    REQUIRE(machine);
    auto bytes{fixture()};
    auto features{strict::make_wasm1p1_feature_parameter()};
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_tail_call=false;
    auto prepared{strict::prepare_runtime_from_wasm(bytes,u8"safe-point-ir",{},features)};
    std::ofstream fixture_file(std::string{argv[1]}+"/fixture.wasm",std::ios::binary);
    fixture_file.write(reinterpret_cast<char const*>(bytes.data()),bytes.size()); fixture_file.close();
    compiler::compile_option options{};
    options.validator_feature_parameter=&features;
    options.emit_call_stack_frames=instruction;
    options.emit_unwind_call_stack_frames=!instruction;
    options.native_exception_target_machine=machine.get();
    auto translate=[&](char const* label,bool expect_emitted,bool expect_points)
    {
        uwvm2::validation::error::code_validation_error_impl error{};
        auto compiled{compiler::compile_all_from_uwvm(*prepared.mod,options,error,0)};
        REQUIRE(error.err_code==uwvm2::validation::error::code_validation_error_code::ok);
        REQUIRE(compiled.llvm_jit_module.emitted==expect_emitted);
        if(!expect_emitted) { std::printf("PASS rejected %s\n",label); std::fflush(stdout); return; }
#ifndef UWVM2TEST_DEBUG_BASELINE
        // The compiler must reject null/empty/one-past source locations without
        // creating a host call or a host symbol declaration.
        llvm::LLVMContext bounds_context;
        llvm::Module bounds_module{"invalid-debug-offsets",bounds_context};
        auto const bounds_type{llvm::FunctionType::get(llvm::Type::getVoidTy(bounds_context),false)};
        auto const bounds_function{llvm::Function::Create(bounds_type,llvm::GlobalValue::ExternalLinkage,"bounds",bounds_module)};
        llvm::IRBuilder<> bounds_builder{llvm::BasicBlock::Create(bounds_context,"entry",bounds_function)};
        compiler::local_func_storage_t invalid{};
        REQUIRE(!compiler::details::emit_runtime_local_func_llvm_jit_debug_safe_point(bounds_builder,invalid,0));
        auto const& local{compiled.local_funcs.index_unchecked(0)};
        // [pinned expression bytes] | code_end
        // [safe                   ] | unsafe (one-past)
        // Copy only existing endpoints; no pointer is advanced or dereferenced.
        invalid.code_begin=local.code_begin; invalid.code_end=local.code_begin;
        REQUIRE(!compiler::details::emit_runtime_local_func_llvm_jit_debug_safe_point(bounds_builder,invalid,0));
        invalid.code_end=local.code_end;
        auto const extent{reinterpret_cast<std::uintptr_t>(local.code_end)-reinterpret_cast<std::uintptr_t>(local.code_begin)};
        REQUIRE(!compiler::details::emit_runtime_local_func_llvm_jit_debug_safe_point(bounds_builder,invalid,extent));
        REQUIRE(!compiler::details::emit_runtime_local_func_llvm_jit_debug_safe_point(bounds_builder,invalid,SIZE_MAX));
        REQUIRE(bounds_module.size()==1&&bounds_function->getEntryBlock().empty());
#endif
        auto const& module{*compiled.llvm_jit_module.llvm_module};
        REQUIRE(!llvm::verifyModule(module,&llvm::errs()));
        std::size_t points{},loops{},tails{},self_backedges{};
#ifndef UWVM2TEST_DEBUG_BASELINE
        bool const instruction_points{std::string{label}=="instructions"};
        std::set<std::pair<std::size_t,std::size_t>> expected,observed;
        auto expect=[&](std::size_t f,std::initializer_list<std::size_t> offsets)
        { for(auto offset:offsets) { expected.emplace(f,offset); } };
        expect(0,{0,2,4,5});
        expect(1,{0,2}); // lexical end is unreachable after musttail.
        expect(2,{0,2,4,6,7,9,11,13,16,18,19,22,24,26,27,29,32,33,35,38});
        expect(3,{0,2,3,5,7,8,9,11,13,14}); // early return and self-tail skip final end.

        auto const symbol_prefix{compiler::details::get_llvm_runtime_bridge_function_symbol_name<
            uwvm2::runtime::lib::details::llvm_jit_debug_safe_point_abi_bridge>()};
#endif
        for(auto const& function:module) for(auto const& block:function) for(auto const& item:block)
        {
            if(auto const branch{llvm::dyn_cast<llvm::BranchInst>(&item)};branch&&branch->isUnconditional())
            {
                if(branch->getSuccessor(0)->getName()=="body"&&block.getName()!="entry") { ++self_backedges; }
            }
            auto const call{llvm::dyn_cast<llvm::CallBase>(&item)};
            if(!call) { continue; }
            if(auto const direct{llvm::dyn_cast<llvm::CallInst>(call)};direct&&direct->isMustTailCall())
            { ++tails; REQUIRE(llvm::isa<llvm::ReturnInst>(direct->getNextNode())); }
#ifndef UWVM2TEST_DEBUG_BASELINE
            auto const callee{call->getCalledFunction()};
            if(!callee||!callee->getName().starts_with(llvm::StringRef{reinterpret_cast<char const*>(symbol_prefix.data()),symbol_prefix.size()})) { continue; }
            REQUIRE(expect_points&&llvm::isa<llvm::CallInst>(call)&&call->arg_size()==3);
            REQUIRE(call->doesNotThrow()&&callee->doesNotThrow());
            REQUIRE(!callee->hasFnAttribute(llvm::Attribute::NoSync)&&!callee->hasFnAttribute(llvm::Attribute::Speculatable));
            REQUIRE(!call->onlyReadsMemory()&&!callee->onlyReadsMemory());
            auto const module_id{llvm::dyn_cast<llvm::ConstantInt>(call->getArgOperand(0))};
            auto const func_id{llvm::dyn_cast<llvm::ConstantInt>(call->getArgOperand(1))};
            auto const offset{llvm::dyn_cast<llvm::ConstantInt>(call->getArgOperand(2))};
            REQUIRE(module_id&&func_id&&offset&&module_id->getZExtValue()==options.curr_wasm_id);
            auto const f{func_id->getZExtValue()},o{offset->getZExtValue()};
            REQUIRE(observed.emplace(f,o).second);
            if(instruction_points)
            {
                REQUIRE(expected.contains({f,o}));
                if(f==2&&o==2) { REQUIRE(block.getName().starts_with("loop")); }
                if(f==2&&o==32) { REQUIRE(block.getName().starts_with("block.end")); }
                if(f==3&&o==8) { REQUIRE(block.getName().starts_with("if.end")); }
            }
            else
            {
                REQUIRE(f<4&&(o==0||(f==2&&o==2)));
                if(o==2) { ++loops; REQUIRE(block.getName().starts_with("loop")); }
                else { REQUIRE(block.getName()=="body"); }
            }
            auto const registered{llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(callee->getName().str())};
            REQUIRE(registered==reinterpret_cast<void*>(compiler::details::get_llvm_runtime_bridge_function_address(
                uwvm2::runtime::lib::details::llvm_jit_debug_safe_point_abi_bridge)));
            ++points;
#endif
        }
        REQUIRE(tails!=0);
#ifndef UWVM2TEST_DEBUG_BASELINE
        if(instruction_points) { REQUIRE(observed==expected&&points==36&&self_backedges==1); }
        else { REQUIRE(!expect_points||(points==5&&loops==1&&self_backedges==1)); }
#endif
        std::error_code error_code;
        llvm::raw_fd_ostream output(std::string{argv[1]}+"/"+label+".ll",error_code);
        REQUIRE(!error_code); module.print(output,nullptr); output.flush();
        std::printf("PASS %s points=%zu loops=%zu musttail=%zu self_backedges=%zu\n",label,points,loops,tails,self_backedges); std::fflush(stdout);
    };
    translate("off-default",true,false);
#ifndef UWVM2TEST_DEBUG_BASELINE
    options.compilation_mode=compiler::llvm_jit_compilation_mode::full;
    translate("off-full",true,false);
    options.emit_debug_safe_points=true;
    translate("on",true,true);
    options.debug_safe_point_granularity=compiler::llvm_jit_debug_safe_point_granularity::instruction;
    translate("instructions",true,true);
    options.debug_safe_point_granularity=static_cast<compiler::llvm_jit_debug_safe_point_granularity>(UINT_MAX);
    translate("invalid-granularity",false,false);
    options.debug_safe_point_granularity=compiler::llvm_jit_debug_safe_point_granularity::entry_loop;
    for(auto mode:{compiler::llvm_jit_compilation_mode::unspecified,compiler::llvm_jit_compilation_mode::lazy,compiler::llvm_jit_compilation_mode::tiered})
    { options.compilation_mode=mode; translate("unsupported-mode",false,false); }
    options.compilation_mode=compiler::llvm_jit_compilation_mode::full;
    options.route_wasm_calls_through_runtime_bridge=true;
    translate("raw-bridge-routing",false,false);
    options.route_wasm_calls_through_runtime_bridge=false;
#if !defined(UWVM2TEST_ROS)
    options.lazy_defined_raw_call_target_count=1;
    translate("lazy-target-metadata",false,false);
    options.lazy_defined_raw_call_target_count=0;
    options.emit_tiered_loop_reentry_entries=true;
    translate("tiered-osr-metadata",false,false);
#endif
#endif
}
