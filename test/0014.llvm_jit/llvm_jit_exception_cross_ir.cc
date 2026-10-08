// Verify actual Wasm translation of Core 3 exception fixtures. This test does
// not execute JIT code: the CLI suite independently verifies runtime semantics.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Verifier.h>
#include <fstream>
#include <iterator>
#include <string_view>
namespace strict = uwvm2test::uwvm_int_strict;
namespace compiler = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
namespace eh = uwvm2::runtime::compiler::llvm_jit::native_exception_symbols;
#undef UWVM2TEST_REQUIRE
#define UWVM2TEST_REQUIRE(x) do { if(!(x)) { std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x); std::abort(); } } while(false)
template<auto Bridge> bool is_bridge(llvm::Function const& function)
{
    auto const name{compiler::details::get_llvm_runtime_bridge_function_symbol_name<Bridge>()};
    return function.getName().starts_with(llvm::StringRef{reinterpret_cast<char const*>(name.data()),name.size()});
}
int main(int argc,char** argv)
{
    if(argc<4) { std::fprintf(stderr,"usage: probe instruction|unwind|none plain|tiered|lazy|bridge|debug module.wasm...\n"); return 2; }
    bool const instruction{std::string_view{argv[1]}=="instruction"};
    bool const unwind{std::string_view{argv[1]}=="unwind"};
    bool const tiered{std::string_view{argv[2]}=="tiered"};
    bool const lazy{std::string_view{argv[2]}=="lazy"};
    bool const bridge{std::string_view{argv[2]}=="bridge"};
    bool const debug{std::string_view{argv[2]}=="debug"};
    UWVM2TEST_REQUIRE(tiered||lazy||bridge||debug||std::string_view{argv[2]}=="plain");
    UWVM2TEST_REQUIRE(instruction||unwind||std::string_view{argv[1]}=="none");
    UWVM2TEST_REQUIRE(llvm::InitializeNativeTarget()==false);
    UWVM2TEST_REQUIRE(llvm::InitializeNativeTargetAsmPrinter()==false);
    std::unique_ptr<llvm::TargetMachine> machine{llvm::EngineBuilder{}.selectTarget()};
    UWVM2TEST_REQUIRE(machine&&eh::supports_itanium_dwarf_object(*machine));
    for(int input=3;input<=argc;++input)
    {
        bool const transit{input==argc};
        char const* label{transit?"syntax-disabled-import-transit":argv[input]};
        bool const imports{transit||std::string_view{label}.ends_with("/import-tag-alias.wasm")};
        strict::byte_vec bytes{},provider{};
        auto read=[](std::string const& path)
        {
            std::ifstream file(path,std::ios::binary);
            std::vector<char> data{std::istreambuf_iterator<char>{file},{}};
            UWVM2TEST_REQUIRE(file&&!data.empty());
            strict::byte_vec result(data.size()); std::memcpy(result.data(),data.data(),data.size()); return result;
        };
        if(transit)
        {
            strict::module_builder module;
            module.types.push_back({{strict::k_val_i32},{}});
            module.add_import_func("p","raise",0);
            strict::func_body body;
            for(unsigned opcode:{0x20u,0u,0x10u,0u,0x0bu}) { strict::append_u8(body.code,opcode); }
            module.add_func({{strict::k_val_i32},{}},std::move(body)); bytes=module.build();
        }
        else { bytes=read(argv[input]); }
        if(imports)
        {
            std::string path{transit?argv[3]:label}; auto const separator{path.find_last_of('/')};
            path=separator==std::string::npos?"provider.wasm":path.substr(0,separator+1)+"provider.wasm";
            provider=read(path);
        }
        auto features{strict::make_wasm1p1_feature_parameter()};
        auto& policy{uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
        policy.disable_exceptions=transit; policy.disable_tail_call=false;
        auto provider_features{features};
        uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(provider_features).disable_exceptions=false;
        auto prepared{imports?strict::prepare_runtime_from_wasm(bytes,transit?uwvm2::utils::container::u8string_view{u8"disabled-transit"}:uwvm2::utils::container::u8string_view{u8"tag-alias-ir"},{{&provider,u8"p",&provider_features}},features):
            strict::prepare_runtime_from_wasm(bytes,u8"native-exception-ir",{},features)};
        compiler::compile_option options{};
        options.validator_feature_parameter=&features;
        options.verify_llvm_jit_ir=true;
        options.emit_call_stack_frames=instruction;
        options.emit_unwind_call_stack_frames=unwind;
        options.native_exception_target_machine=machine.get();
        options.route_wasm_calls_through_runtime_bridge=lazy||bridge;
        if(debug)
        {
            options.compilation_mode=compiler::llvm_jit_compilation_mode::full;
            options.emit_debug_safe_points=true;
            options.debug_safe_point_granularity=compiler::llvm_jit_debug_safe_point_granularity::instruction;
        }
#if !defined(UWVM2TEST_ROS)
        options.emit_tiered_loop_reentry_entries=tiered;
        // Stable host allocations make the generated lazy fast/slow paths real
        // compiler inputs; no generated code or dummy target is executed here.
        auto const local_count{prepared.mod->local_defined_function_vec_storage.size()};
        std::vector<std::uintptr_t> raw_targets(local_count*2),typed_targets(local_count);
        if(lazy)
        {
            options.lazy_defined_raw_call_target_base_address=reinterpret_cast<std::uintptr_t>(raw_targets.data());
            options.lazy_defined_raw_call_target_count=local_count;
            options.lazy_defined_typed_entry_target_base_address=reinterpret_cast<std::uintptr_t>(typed_targets.data());
            options.lazy_defined_typed_entry_target_count=local_count;
            options.lazy_defined_targets_are_atomic=true;
        }
#else
        UWVM2TEST_REQUIRE(!tiered&&!lazy);
#endif
        uwvm2::validation::error::code_validation_error_impl error{};
        compiler::full_function_symbol_t compiled;
        try { compiled=compiler::compile_all_from_uwvm(*prepared.mod,options,error,0); }
        catch(...) { std::fprintf(stderr,"translation failed %s error=%u\n",label,unsigned(error.err_code)); throw; }
        UWVM2TEST_REQUIRE(error.err_code==uwvm2::validation::error::code_validation_error_code::ok);
        if(bridge&&std::string_view{label}.ends_with("/tail-bypass-direct.wasm"))
        {
            // Synthetic raw-only routing has no published typed tail target.
            // The existing contract rejects this configuration rather than
            // lowering return_call to a stack-growing ordinary bridge call.
            UWVM2TEST_REQUIRE(!compiled.llvm_jit_module.emitted);
            std::printf("PASS expected rejection: raw-only direct tail transfer %s\n",argv[1]);
            continue;
        }
        UWVM2TEST_REQUIRE(compiled.llvm_jit_module.emitted&&compiled.llvm_jit_module.llvm_module);
        auto const& module{*compiled.llvm_jit_module.llvm_module};
        UWVM2TEST_REQUIRE(!llvm::verifyModule(module,&llvm::errs()));
        std::size_t definitions{},invokes{},landings{},typed{},pushes{},pops{},tails{},debug_points{};
        for(auto const& function:module)
        {
            if(function.isDeclaration()) { continue; }
            ++definitions;
            UWVM2TEST_REQUIRE(function.getUWTableKind()==(unwind?llvm::UWTableKind::Async:llvm::UWTableKind::Sync));
            for(auto const& block:function) for(auto const& item:block)
            {
                if(auto const landing=llvm::dyn_cast<llvm::LandingPadInst>(&item))
                {
                    ++landings;
                    UWVM2TEST_REQUIRE(landing->isCleanup());
                    for(unsigned clause{};clause!=landing->getNumClauses();++clause)
                    {
                        auto const type=llvm::dyn_cast<llvm::GlobalVariable>(landing->getClause(clause));
                        UWVM2TEST_REQUIRE(type&&type->getName()==eh::type_info_symbol&&type->isDeclaration());
                        ++typed;
                    }
                }
                if(llvm::isa<llvm::InvokeInst>(item)) { ++invokes; }
                if(auto const call=llvm::dyn_cast<llvm::CallBase>(&item))
                {
                    if(auto const direct=llvm::dyn_cast<llvm::CallInst>(call); direct&&direct->isMustTailCall())
                    { ++tails; UWVM2TEST_REQUIRE(llvm::isa<llvm::ReturnInst>(direct->getNextNode())); }
                    if(auto const callee=call->getCalledFunction())
                    {
                        bool const push{is_bridge<uwvm2::runtime::lib::llvm_jit_push_call_stack_frame>(*callee)};
                        bool const pop{is_bridge<uwvm2::runtime::lib::llvm_jit_pop_call_stack_frame>(*callee)};
                        pushes+=push;
                        pops+=pop;
                        if(push||pop) { UWVM2TEST_REQUIRE(call->doesNotThrow()&&callee->doesNotThrow()); }
                        if(is_bridge<uwvm2::runtime::lib::details::llvm_jit_debug_safe_point_abi_bridge>(*callee))
                        {
                            ++debug_points;
                            UWVM2TEST_REQUIRE(debug&&llvm::isa<llvm::CallInst>(call)&&call->doesNotThrow()&&callee->doesNotThrow());
                            auto const f{llvm::dyn_cast<llvm::ConstantInt>(call->getArgOperand(1))};
                            auto const offset{llvm::dyn_cast<llvm::ConstantInt>(call->getArgOperand(2))};
                            UWVM2TEST_REQUIRE(f&&offset&&f->getZExtValue()>=prepared.mod->imported_function_vec_storage.size());
                            auto const index{f->getZExtValue()-prepared.mod->imported_function_vec_storage.size()};
                            UWVM2TEST_REQUIRE(index<compiled.local_funcs.size());
                            auto const& local{compiled.local_funcs.index_unchecked(index)};
                            auto const size{reinterpret_cast<std::uintptr_t>(local.code_end)-reinterpret_cast<std::uintptr_t>(local.code_begin)};
                            UWVM2TEST_REQUIRE(offset->getZExtValue()<size);
                        }
                        if(is_bridge<uwvm2::runtime::lib::details::llvm_jit_throw_numeric_abi_bridge>(*callee))
                        { UWVM2TEST_REQUIRE(!call->doesNotThrow()&&!callee->doesNotThrow()); }
                    }
                }
            }
        }
        UWVM2TEST_REQUIRE(definitions!=0);
        UWVM2TEST_REQUIRE(debug?debug_points!=0:debug_points==0);
        UWVM2TEST_REQUIRE(instruction||(pushes==0&&pops==0));
        if(transit)
        {
            // The transit module contains no exception syntax. Its imported
            // call still obeys the VM unwind ABI and cleans an owned frame.
            UWVM2TEST_REQUIRE(typed==0);
            UWVM2TEST_REQUIRE(!instruction||(invokes!=0&&landings!=0));
        }
        std::printf("PASS %s %s style=%s definitions=%zu invokes=%zu landings=%zu typed=%zu pushes=%zu pops=%zu musttail=%zu\n",
            label,argv[1],argv[2],definitions,invokes,landings,typed,pushes,pops,tails);
        std::fflush(stdout);
    }
}
