#include "../strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/validation/standard/wasm3/impl.h>
#include "multi_memory_vectors.h"

namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace wasm3 = uwvm2::validation::standard::wasm3;
    using error_t = uwvm2::validation::error::code_validation_error_impl;
    using error_code = uwvm2::validation::error::code_validation_error_code;

    template <optable::uwvm_interpreter_translate_option_t Option>
    int check()
    {
        auto features{make_wasm1p1_feature_parameter()};
        optable::compile_option options{};
        auto& selected{uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
        for(auto const& item: multi_memory_cases)
        {
            std::fprintf(stderr, "case=%s tail=%u\n", item.name, unsigned(Option.is_tail_call));
            selected.disable_multi_memory = false;
            byte_vec wasm{};
            for(std::size_t i{}; i != item.size; ++i) { wasm.push_back(static_cast<std::byte>(item.data[i])); }
            auto prepared{prepare_runtime_from_wasm(wasm, u8"multi_memory", {}, features)};
            auto const& parsed{uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
            auto const& codes = []<uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections, uwvm2::utils::container::tuple<Fs...>) -> auto const&
            { return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(sections); }
                (parsed.sections, uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);
            auto const& body{codes.codes.index_unchecked(0).body};
            for(bool disabled: {false, true})
            {
                selected.disable_multi_memory = disabled;
                bool const expected{item.valid && (!disabled || item.disabled_valid)};
                error_t standalone{};
                try { wasm3::validate_code_with_runtime_policy(parsed, 0,
                    reinterpret_cast<std::byte const*>(body.expr_begin), reinterpret_cast<std::byte const*>(body.code_end), standalone, features); }
                catch(fast_io::error const&) {}
                UWVM2TEST_REQUIRE((standalone.err_code == error_code::ok) == expected);
                error_t integrated{};
                try
                {
                    auto compiled{compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod, options, integrated, &features)};
                    UWVM2TEST_REQUIRE(expected);
                    if(!disabled)
                    {
                        byte_vec arguments{};
                        auto result{interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(0),
                            prepared.mod->local_defined_function_vec_storage.index_unchecked(0), arguments, nullptr, nullptr)};
                        UWVM2TEST_REQUIRE(result.results.size() == 4);
                        std::uint32_t actual{}; std::memcpy(&actual, result.results.data(), 4);
                        if(actual != item.expected)
                        { std::fprintf(stderr, "FAIL %s got=%x expected=%x\n",item.name,actual,item.expected); return 1; }
                    }
                }
                catch(fast_io::error const&) {}
                UWVM2TEST_REQUIRE((integrated.err_code == error_code::ok) == expected);
            }
        }
        return 0;
    }
}
int main()
{
    install_unexpected_traps();
    constexpr optable::uwvm_interpreter_translate_option_t uncached{.is_tail_call = false};
    constexpr optable::uwvm_interpreter_translate_option_t ring{
        .is_tail_call = true, .i32_stack_top_begin_pos = 3uz, .i32_stack_top_end_pos = 5uz,
        .i64_stack_top_begin_pos = 3uz, .i64_stack_top_end_pos = 5uz,
        .f32_stack_top_begin_pos = 5uz, .f32_stack_top_end_pos = 7uz,
        .f64_stack_top_begin_pos = 5uz, .f64_stack_top_end_pos = 7uz};
    UWVM2TEST_REQUIRE(check<uncached>() == 0);
#if !defined(UWVM2TEST_UNCACHED_ONLY)
    UWVM2TEST_REQUIRE(check<ring>() == 0);
    std::puts("PASS multi-memory: scalar/SIMD/bulk/index-129, feature gates and malformed validation, uncached and register-ring");
#else
    std::puts("PASS multi-memory: scalar/SIMD/bulk/index-129, feature gates and malformed validation, uncached only");
#endif
}
