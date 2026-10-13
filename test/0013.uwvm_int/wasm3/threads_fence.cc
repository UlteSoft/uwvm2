#include "../strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/validation/standard/wasm3/impl.h>
#if defined(UWVM2TEST_FENCE_LAZY)
# include <uwvm2/runtime/compiler/uwvm_int/compile_cu_from_lazy_validator/impl.h>
#endif
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
# include <llvm/IR/Verifier.h>
# include <llvm/IR/Instructions.h>
# include <llvm/Support/raw_ostream.h>
#endif
namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace wasm3 = uwvm2::validation::standard::wasm3;
    using error_t = uwvm2::validation::error::code_validation_error_impl;
    using error_code = uwvm2::validation::error::code_validation_error_code;
    struct test_case { std::initializer_list<unsigned char> immediate; bool valid; };
    // Padded subopcodes are legal; padded/nonzero reserved bytes are not.
    test_case const cases[]{
        {{3,0},true}, {{0x83,0,0},true}, {{0x83,0x80,0x80,0x80,0,0},true},
        {{},false}, {{0x83},false}, {{3},false}, {{3,1},false}, {{3,0x80,0},false},
        {{0x83,0x80,0x80,0x80,0x10,0},false}, {{0x83,0x80,0x80,0x80,0x80,0,0},false},
        {{4,0},false}};
    byte_vec build(test_case const& item, bool unreachable)
    {
        module_builder builder{};
        func_type type{{},{0x7f}};
        func_body body{};
        if(unreachable) { append_u8(body.code,0x00); }
        append_u8(body.code,0x41); append_u8(body.code,42);
        append_u8(body.code,0xfe);
        for(auto byte:item.immediate) { append_u8(body.code,byte); }
        append_u8(body.code,0x41); append_u8(body.code,1);
        append_u8(body.code,0x6a); append_u8(body.code,0x0b);
        builder.add_func(std::move(type),std::move(body));
        return builder.build();
    }

#if !defined(UWVM2TEST_STRICT_NO_INTERPRETER)
    template <optable::uwvm_interpreter_translate_option_t Option>
#endif
    int check()
    {
        auto features{make_wasm1p1_feature_parameter()};
        auto& selected{uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
        unsigned checks{};
        for(auto const& item:cases)
        {
            byte_vec immediate{};
            for(auto byte:item.immediate) { immediate.push_back(static_cast<std::byte>(byte)); }
            // Use a nonnull stable sentinel for the empty range too.
            immediate.push_back(std::byte{0x0b});
            std::byte const* const begin{immediate.data()};
            auto const end{begin+immediate.size()-1};
            auto cursor{begin};
            UWVM2TEST_REQUIRE(wasm3::scan_atomic_fence_immediate(cursor,end)==item.valid);
            UWVM2TEST_REQUIRE(cursor==(item.valid ? end : begin));
            for(bool unreachable:{false,true})
            {
                selected.disable_threads=false;
                auto wasm{build(item,unreachable)};
                auto prepared{prepare_runtime_from_wasm(wasm,u8"threads-fence",{},features)};
                auto const& parsed{uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
                auto const& codes=[]<uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,uwvm2::utils::container::tuple<Fs...>)->auto const&
                { return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(sections); }
                    (parsed.sections,uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);
                auto const& body{codes.codes.index_unchecked(0).body};
                for(bool disabled:{false,true})
                {
                    selected.disable_threads=disabled;
                    bool const accepted{item.valid&&!disabled};
                    error_t standalone{};
                    try { wasm3::validate_code_with_runtime_policy(parsed,0,
                        reinterpret_cast<std::byte const*>(body.expr_begin),reinterpret_cast<std::byte const*>(body.code_end),standalone,features); }
                    catch(fast_io::error const&) {}
                    UWVM2TEST_REQUIRE((standalone.err_code==error_code::ok)==accepted);
#if defined(UWVM2TEST_FENCE_LAZY)
                    error_t lazy_error{};
                    auto lazy_cursor{begin};
                    try { uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::details::skip_wasm1_non_structural_immediates(
                        lazy_cursor,end,begin,static_cast<uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xfe),*prepared.mod,features,lazy_error); }
                    catch(fast_io::error const&) {}
                    UWVM2TEST_REQUIRE((lazy_error.err_code==error_code::ok)==accepted);
                    UWVM2TEST_REQUIRE(lazy_cursor==(accepted ? end : begin));
#endif
                    error_t integrated{};
                    try
                    {
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
                        namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
                        jit::compile_option options{}; options.validator_feature_parameter=&features; options.verify_llvm_jit_ir=true;
                        auto compiled{jit::compile_all_from_uwvm(*prepared.mod,options,integrated,0)};
                        UWVM2TEST_REQUIRE(accepted && compiled.llvm_jit_module.emitted);
                        auto& module{*compiled.llvm_jit_module.llvm_module};
                        UWVM2TEST_REQUIRE(!llvm::verifyModule(module,&llvm::errs()));
                        unsigned fences{};
                        for(auto& f:module) for(auto& block:f) for(auto& instruction:block)
                        { if(auto* fence=llvm::dyn_cast<llvm::FenceInst>(&instruction)) { ++fences;UWVM2TEST_REQUIRE(fence->getOrdering()==llvm::AtomicOrdering::SequentiallyConsistent); } }
                        UWVM2TEST_REQUIRE(fences==(unreachable ? 0u : 1u));
                        if(!unreachable)
                        {
                            std::uint32_t actual{};
                            uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,0,reinterpret_cast<std::byte*>(&actual),4,nullptr,0);
                            UWVM2TEST_REQUIRE(actual==43);
                        }
#else
                        optable::compile_option options{};
                        auto compiled{compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod,options,integrated,&features)};
                        UWVM2TEST_REQUIRE(accepted);
                        if(!unreachable)
                        {
                            byte_vec arguments{};
                            auto result{interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(0),
                                prepared.mod->local_defined_function_vec_storage.index_unchecked(0),arguments,nullptr,nullptr)};
                            UWVM2TEST_REQUIRE(result.results.size()==4);
                            std::uint32_t actual{};std::memcpy(&actual,result.results.data(),4);
                            UWVM2TEST_REQUIRE(actual==43);
                        }
#endif
                    }
                    catch(fast_io::error const&) {}
                    UWVM2TEST_REQUIRE((integrated.err_code==error_code::ok)==accepted);
                    ++checks;
                }
            }
        }
        std::printf("PASS atomic.fence: %u validation cases, memoryless execution, padded subopcode and literal reserved byte\n",checks);
        return 0;
    }
}
int main()
{
#if !defined(UWVM2TEST_STRICT_NO_INTERPRETER)
    install_unexpected_traps();
#endif
#if defined(UWVM2TEST_STRICT_NO_INTERPRETER)
    return check();
#else
    constexpr optable::uwvm_interpreter_translate_option_t uncached{.is_tail_call=false};
    constexpr optable::uwvm_interpreter_translate_option_t ring{
        .is_tail_call=true,.i32_stack_top_begin_pos=3uz,.i32_stack_top_end_pos=5uz,
        .i64_stack_top_begin_pos=3uz,.i64_stack_top_end_pos=5uz,
        .f32_stack_top_begin_pos=5uz,.f32_stack_top_end_pos=7uz,
        .f64_stack_top_begin_pos=5uz,.f64_stack_top_end_pos=7uz};
    UWVM2TEST_REQUIRE(check<uncached>()==0);
    return check<ring>();
#endif
}
