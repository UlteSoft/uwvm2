#include "../uwvm_int_translate_strict_common.h"

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/cmdline/callback/impl.h>
#endif

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;
    using errc = ::uwvm2::validation::error::code_validation_error_code;
    using feature_kind = ::uwvm2::parser::wasm::base::wasm2_feature_kind;

    template <::std::size_t N>
    [[nodiscard]] byte_vec bytes(::std::uint8_t const (&input)[N])
    {
        byte_vec out{};
        out.reserve(N);
        for(auto value: input) { append_u8(out, value); }
        return out;
    }

    [[nodiscard]] byte_vec table_get_zero_module()
    {
        constexpr ::std::uint8_t input[]{
            0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
            0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x01u,
            0x0au, 0x09u, 0x01u, 0x07u, 0x00u, 0x41u, 0x00u, 0x25u, 0x00u, 0x1au, 0x0bu};
        return bytes(input);
    }

    [[nodiscard]] byte_vec table_get_one_module()
    {
        constexpr ::std::uint8_t input[]{
            0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
            0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x04u, 0x07u, 0x02u, 0x70u, 0x00u, 0x01u,
            0x70u, 0x00u, 0x01u, 0x0au, 0x09u, 0x01u, 0x07u, 0x00u, 0x41u, 0x00u, 0x25u, 0x01u,
            0x1au, 0x0bu};
        return bytes(input);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int expect_feature_rejection(byte_vec const& wasm,
                                                ::uwvm2::utils::container::u8string_view module_name,
                                                wasm_feature_parameter_t const& prepare_features,
                                                wasm_feature_parameter_t const& compile_features,
                                                feature_kind expected_feature) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        bool started_compile{};
        try
        {
            auto prep{prepare_runtime_from_wasm(wasm, module_name, {}, prepare_features)};
            UWVM2TEST_REQUIRE(prep.mod != nullptr);
            optable::compile_option cop{};
            started_compile = true;
            static_cast<void>(compiler::compile_all_from_uwvm_single_func<Opt>(
                *prep.mod, cop, err, ::std::addressof(compile_features)));
        }
        catch(::fast_io::error const&)
        {}
        catch(...)
        {
            return fail(__LINE__, "unexpected exception type");
        }

        UWVM2TEST_REQUIRE(started_compile);
        UWVM2TEST_REQUIRE(err.err_code == errc::wasm2_feature_required);
        UWVM2TEST_REQUIRE(err.err_selectable.wasm2_feature_required.feature == expected_feature);
        return 0;
    }

    [[nodiscard]] int test_wasm2_feature_gates() noexcept
    {
        auto enabled{make_wasm1p1_feature_parameter()};
        using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
        using cli_mode = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode;
        auto& enabled_policy{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(enabled)};
        enabled_policy.cli_mode = cli_mode::direct_wasm2;

        auto table_instructions_off{enabled};
        auto& table_policy{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(table_instructions_off)};
        table_policy.cli_mode = cli_mode::scoped;
        table_policy.disable_table_instructions = true;

        auto multiple_tables_off{enabled};
        auto& multiple_policy{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(multiple_tables_off)};
        multiple_policy.cli_mode = cli_mode::scoped;
        multiple_policy.disable_multiple_tables = true;

        constexpr auto opt{k_test_byref_opt};
        UWVM2TEST_REQUIRE(expect_feature_rejection<opt>(table_get_zero_module(),
                                                        u8"uwvm2test_wasm2_table_instructions_gate",
                                                        enabled,
                                                        table_instructions_off,
                                                        feature_kind::table_instructions) == 0);
        UWVM2TEST_REQUIRE(expect_feature_rejection<opt>(table_get_one_module(),
                                                        u8"uwvm2test_wasm2_multiple_tables_gate",
                                                        enabled,
                                                        multiple_tables_off,
                                                        feature_kind::multiple_tables) == 0);
        return 0;
    }
}

int main()
{
    return test_wasm2_feature_gates();
}
