#include "../../wasm1p1/uwvm_int_wasm1p1_full_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;
    using namespace ::uwvm2test::uwvm_int_wasm1p1_full;

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_and_run(byte_vec const& wasm,
                                      ::uwvm2::utils::container::u8string_view module_name,
                                      wasm_feature_parameter_t const& features) noexcept
    {
        auto prep{prepare_runtime_from_wasm(wasm, module_name, {}, features)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt{*prep.mod};
        UWVM2TEST_REQUIRE(rt.local_defined_function_vec_storage.size() == k_fn_count);

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm{compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err, ::std::addressof(features))};
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == k_fn_count);

        using Runner = interpreter_runner<Opt>;
        auto run_func = [&](::std::size_t local_index, byte_vec const& params)
        {
            return Runner::run(cm.local_funcs.index_unchecked(local_index),
                               rt.local_defined_function_vec_storage.index_unchecked(local_index),
                               params,
                               nullptr,
                               nullptr);
        };

        UWVM2TEST_REQUIRE((run_full_wasm1p1_suite<Opt>(rt, run_func) == 0));
        return 0;
    }

    [[nodiscard]] int test_translate_wasm1p1_full_interpreter() noexcept
    {
        install_unexpected_traps();
        optable::call_func = strict_terminate_call;
        optable::call_indirect_func = strict_terminate_call_indirect;

        auto wasm{build_full_wasm1p1_module()};
        auto features{make_wasm1p1_feature_parameter()};

        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_full_byref", features) == 0);
        }

        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(compile_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_full_tail_min", features) == 0);
        }

        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(compile_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_full_tail_sysv", features) == 0);
        }

        {
            constexpr auto opt{k_test_tail_sysv_v128_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(compile_and_run<opt>(wasm, u8"uwvm2test_wasm1p1_full_tail_v128", features) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_wasm1p1_full_interpreter();
}
