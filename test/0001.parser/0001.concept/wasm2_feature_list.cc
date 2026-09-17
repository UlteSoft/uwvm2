/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly 2.0 command-line feature policy test
 * @details     Locks down the wasm1p1 default and the aggregate/scoped mutual-exclusion policy.
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-07-11
 * @copyright   APL-2.0 License
 */

#include <cstddef>
#include <memory>
#include <type_traits>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/uwvm/cmdline/callback/wasm_feature.h>
# include <uwvm2/validation/standard/wasm2/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace test
{
    namespace callbacks = ::uwvm2::uwvm::cmdline::params::details;
    namespace parameters = ::uwvm2::uwvm::cmdline::params;
    namespace wasm1p1_features = ::uwvm2::parser::wasm::standard::wasm1p1::features;
    namespace wasm2_features = ::uwvm2::parser::wasm::standard::wasm2::features;

    using feature_parameter = wasm1p1_features::wasm_binfmt1p1_feature_parameter;
    using callback_type = ::uwvm2::utils::cmdline::handle_func_type;
    using return_type = ::uwvm2::utils::cmdline::parameter_return_type;

    struct feature_case
    {
        callback_type enable{};
        callback_type disable{};
        bool feature_parameter::* disabled{};
        ::uwvm2::utils::cmdline::parameter const* enable_parameter{};
        ::uwvm2::utils::cmdline::parameter const* disable_parameter{};
        ::uwvm2::utils::container::u8string_view enable_name{};
        ::uwvm2::utils::container::u8string_view disable_name{};
        wasm2_features::wasm2_feature_kind kind{};
    };

#define UWVM2_TEST_FEATURE_CASE(member_name, suffix)                                                                                                  \
    {                                                                                                                                                  \
        callbacks::wasm_feature_enable_##member_name##_callback,                                                                                       \
        callbacks::wasm_feature_disable_##member_name##_callback,                                                                                      \
        &feature_parameter::disable_##member_name,                                                                                                     \
        ::std::addressof(parameters::wasm_feature_enable_##member_name),                                                                                \
        ::std::addressof(parameters::wasm_feature_disable_##member_name),                                                                               \
        u8"--wasm-feature-enable-" suffix,                                                                                                             \
        u8"--wasm-feature-disable-" suffix,                                                                                                            \
        wasm2_features::wasm2_feature_kind::member_name                                                                                                 \
    }

    inline constexpr feature_case feature_cases[]{
        UWVM2_TEST_FEATURE_CASE(multi_value, u8"multi-value"),
        UWVM2_TEST_FEATURE_CASE(reference_types, u8"reference-types"),
        UWVM2_TEST_FEATURE_CASE(table_instructions, u8"table-instructions"),
        UWVM2_TEST_FEATURE_CASE(multiple_tables, u8"multiple-tables"),
        UWVM2_TEST_FEATURE_CASE(bulk_memory, u8"bulk-memory"),
        UWVM2_TEST_FEATURE_CASE(sign_extension, u8"sign-extension"),
        UWVM2_TEST_FEATURE_CASE(nontrapping_float_to_int, u8"nontrapping-float-to-int"),
        UWVM2_TEST_FEATURE_CASE(simd, u8"simd"),
    };

#undef UWVM2_TEST_FEATURE_CASE

    inline constexpr callback_type aggregate_callbacks[]{
        callbacks::wasm_feature_mvp_callback,
        callbacks::wasm_feature_wasm1p1_callback,
        callbacks::wasm_feature_wasm2_callback,
    };

    [[noreturn]] inline void fail(char const* message)
    {
        ::fast_io::io::perrln("wasm2_feature_list: ", ::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }

    inline void expect(bool condition, char const* message)
    {
        if(!condition) [[unlikely]] { fail(message); }
    }

    inline auto& cli_parameter() noexcept
    {
        using wasm1p1 = wasm1p1_features::wasm1p1;
        return ::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(
            ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para);
    }

    inline void reset() noexcept { ::uwvm2::uwvm::wasm::storage::wasm_parameter = {}; }

    inline return_type invoke(callback_type callback) noexcept
    {
        ::uwvm2::utils::cmdline::parameter_parsing_results argument[1]{};
        argument[0].str = u8"--wasm-feature-test";
        argument[0].type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::parameter;
        return callback(argument, argument, argument + 1uz);
    }

    inline void expect_success(callback_type callback, char const* message)
    {
        expect(invoke(callback) == return_type::def, message);
    }

    inline void expect_conflict(callback_type callback, char const* message)
    {
        expect(invoke(callback) == return_type::return_m1_imme, message);
    }

    inline void expect_all_enabled(feature_parameter const& para)
    {
        expect(!para.disable_multi_value, "multi-value must be enabled by default");
        expect(!para.disable_reference_types, "reference-types must be enabled by default");
        expect(!para.disable_table_instructions, "table-instructions must be enabled by default");
        expect(!para.disable_multiple_tables, "multiple-tables must be enabled by default");
        expect(!para.disable_bulk_memory, "bulk-memory must be enabled by default");
        expect(!para.disable_sign_extension, "sign-extension must be enabled by default");
        expect(!para.disable_nontrapping_float_to_int, "nontrapping-float-to-int must be enabled by default");
        expect(!para.disable_simd, "simd must be enabled by default");
    }

    inline void expect_all_disabled(feature_parameter const& para)
    {
        expect(para.disable_multi_value, "MVP must disable multi-value");
        expect(para.disable_reference_types, "MVP must disable reference-types");
        expect(para.disable_table_instructions, "MVP must disable table-instructions");
        expect(para.disable_multiple_tables, "MVP must disable multiple-tables");
        expect(para.disable_bulk_memory, "MVP must disable bulk-memory");
        expect(para.disable_sign_extension, "MVP must disable sign-extension");
        expect(para.disable_nontrapping_float_to_int, "MVP must disable nontrapping-float-to-int");
        expect(para.disable_simd, "MVP must disable simd");
    }

    inline bool has_alias(::uwvm2::utils::cmdline::parameter const& parameter, ::uwvm2::utils::container::u8string_view alias) noexcept
    {
        for(::std::size_t i{}; i != parameter.alias.len; ++i)
        {
            if(parameter.alias.base[i] == alias) { return true; }
        }
        return false;
    }

    inline void test_defaults_and_spellings()
    {
        reset();
        auto const& para{cli_parameter()};
        expect_all_enabled(para);
        expect(!::uwvm2::validation::standard::wasm2::use_wasm2_runtime_validation_strategy(para),
               "native default must retain the wasm1p1 code-validation strategy");
        expect(para.cli_mode == wasm1p1_features::wasm_feature_cli_mode::unspecified, "default CLI mode must be unspecified");
        expect(!para.explicit_feature_mvp && !para.explicit_feature_wasm1p1 && !para.explicit_feature_wasm2,
               "no aggregate selector may be explicit by default");

        expect(parameters::wasm_feature_mvp.name == u8"--wasm-feature-wasmmvp", "unexpected canonical MVP spelling");
        expect(parameters::wasm_feature_wasm1p1.name == u8"--wasm-feature-wasm1p1", "unexpected canonical wasm1p1 spelling");
        expect(parameters::wasm_feature_wasm2.name == u8"--wasm-feature-wasm2", "unexpected canonical wasm2 spelling");
        expect(has_alias(parameters::wasm_feature_mvp, u8"--wasm-feature-mvp"), "legacy MVP spelling must remain an alias");
        expect(has_alias(parameters::wasm_feature_wasm1p1, u8"--wasm-feature-wasm1.1"), "legacy wasm1.1 spelling must remain an alias");
        for(auto const& feature: feature_cases)
        {
            expect(feature.enable_parameter->name == feature.enable_name, "unexpected canonical scoped enable spelling");
            expect(feature.disable_parameter->name == feature.disable_name, "unexpected canonical scoped disable spelling");
        }
    }

    inline void test_scoped_switches()
    {
        for(auto const& feature: feature_cases)
        {
            reset();
            cli_parameter().*feature.disabled = true;
            expect_success(feature.enable, "scoped enable callback failed");
            expect(!(cli_parameter().*feature.disabled), "scoped enable callback did not enable its feature");
            expect(wasm2_features::feature_enabled(cli_parameter(), feature.kind),
                   "scoped enable callback did not reach the central validator policy");
            expect(::uwvm2::validation::standard::wasm2::use_wasm2_runtime_validation_strategy(cli_parameter()),
                   "a scoped feature policy must select the wasm2 validator");

            reset();
            expect_success(feature.disable, "scoped disable callback failed");
            expect(cli_parameter().*feature.disabled, "scoped disable callback did not disable its feature");
            expect(!wasm2_features::feature_enabled(cli_parameter(), feature.kind),
                   "scoped disable callback did not reach the central validator policy");

            reset();
            expect_success(feature.enable, "scoped enable callback failed before opposite-direction conflict");
            expect_conflict(feature.disable, "enable followed by disable must conflict for the same feature");

            reset();
            expect_success(feature.disable, "scoped disable callback failed before opposite-direction conflict");
            expect_conflict(feature.enable, "disable followed by enable must conflict for the same feature");
        }

        reset();
        expect_success(feature_cases[0].enable, "first independent scoped switch failed");
        expect_success(feature_cases[7].disable, "different scoped feature switches must coexist");
    }

    inline void test_central_policy_legacy_controls()
    {
        using feature_kind = wasm2_features::wasm2_feature_kind;
        feature_parameter para{};

        expect(wasm2_features::feature_enabled(para, feature_kind::multi_value), "default central multi-value policy must be enabled");
        expect(wasm2_features::feature_enabled(para, feature_kind::multiple_tables),
               "default central multiple-tables policy must be enabled");

        para.controllable_allow_multi_result_vector = true;
        expect(!wasm2_features::feature_enabled(para, feature_kind::multi_value),
               "legacy single-result COP control must disable the central multi-value policy");
        para.controllable_allow_multi_result_vector = false;

        para.controllable_allow_multi_table = true;
        expect(!wasm2_features::feature_enabled(para, feature_kind::multiple_tables),
               "legacy single-table COP control must disable the central multiple-tables policy");
    }

    inline void test_aggregate_switches()
    {
        reset();
        expect_success(callbacks::wasm_feature_mvp_callback, "MVP aggregate callback failed");
        expect_all_disabled(cli_parameter());
        expect(!::uwvm2::validation::standard::wasm2::use_wasm2_runtime_validation_strategy(cli_parameter()),
               "MVP aggregate must not replace the native validator with wasm2");

        reset();
        expect_success(callbacks::wasm_feature_wasm1p1_callback, "wasm1p1 aggregate callback failed");
        expect_all_enabled(cli_parameter());
        expect(!::uwvm2::validation::standard::wasm2::use_wasm2_runtime_validation_strategy(cli_parameter()),
               "wasm1p1 aggregate must retain the native validator");

        reset();
        expect_success(callbacks::wasm_feature_wasm2_callback, "wasm2 aggregate callback failed");
        expect_all_enabled(cli_parameter());
        expect(::uwvm2::validation::standard::wasm2::use_wasm2_runtime_validation_strategy(cli_parameter()),
               "wasm2 aggregate must select the wasm2 validator");

        // Every ordered pair is tested, so all three aggregates are pairwise exclusive in both orders.
        for(::std::size_t first{}; first != 3uz; ++first)
        {
            for(::std::size_t second{}; second != 3uz; ++second)
            {
                if(first == second) { continue; }
                reset();
                expect_success(aggregate_callbacks[first], "first aggregate callback failed");
                expect_conflict(aggregate_callbacks[second], "different aggregate callbacks must conflict");
            }
        }

        // Lock down every ordered aggregate/scoped combination:
        // 3 aggregates * 8 feature groups * 2 scoped directions * 2 ordering directions = 96 conflicts.
        for(auto const aggregate: aggregate_callbacks)
        {
            for(auto const& feature: feature_cases)
            {
                callback_type const scoped_callbacks[]{feature.enable, feature.disable};
                for(auto const scoped: scoped_callbacks)
                {
                    reset();
                    expect_success(aggregate, "aggregate callback failed before scoped conflict");
                    expect_conflict(scoped, "aggregate followed by scoped must conflict");

                    reset();
                    expect_success(scoped, "scoped callback failed before aggregate conflict");
                    expect_conflict(aggregate, "scoped followed by aggregate must conflict");
                }
            }
        }
    }
}

int main()
{
    test::test_defaults_and_spellings();
    test::test_scoped_switches();
    test::test_central_policy_legacy_controls();
    test::test_aggregate_switches();
}
