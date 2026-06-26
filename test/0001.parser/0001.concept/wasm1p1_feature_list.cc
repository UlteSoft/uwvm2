/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly parser feature-list smoke test
 * @details     Ensures both wasm1-only and wasm1+wasm1.1 feature lists instantiate and parse.
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
 * @copyright   APL-2.0 License
 */

#include <cstddef>
#include <cstdint>
#include <type_traits>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/impl.h>
# include <uwvm2/uwvm/cmdline/callback/wasm_feature.h>
#else
# error "Module testing is not currently supported"
#endif

namespace test
{
    inline constexpr unsigned char empty_wasm[]{
        0x00u,
        0x61u,
        0x73u,
        0x6du,
        0x01u,
        0x00u,
        0x00u,
        0x00u,
    };

    [[noreturn]] inline void fail(char const* msg)
    {
        ::fast_io::io::perrln("wasm1p1_feature_list: ", ::fast_io::mnp::os_c_str(msg));
        ::fast_io::fast_terminate();
    }

    inline void expect(bool condition, char const* msg)
    {
        if(!condition) [[unlikely]] { fail(msg); }
    }

    inline auto& cli_wasm1p1_parameter() noexcept
    {
        using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
        return ::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(
            ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para);
    }

    inline void reset_cli_wasm_parameter() noexcept { ::uwvm2::uwvm::wasm::storage::wasm_parameter = {}; }

    inline void expect_no_explicit_subfeatures(auto const& p1_para)
    {
        expect(!p1_para.explicit_enable_multi_value, "multi-value explicit flag should be disabled");
        expect(!p1_para.explicit_enable_reference_types, "reference-types explicit flag should be disabled");
        expect(!p1_para.explicit_enable_bulk_memory, "bulk-memory explicit flag should be disabled");
        expect(!p1_para.explicit_enable_sign_extension, "sign-extension explicit flag should be disabled");
        expect(!p1_para.explicit_enable_nontrapping_float_to_int, "nontrapping explicit flag should be disabled");
        expect(!p1_para.explicit_enable_simd, "simd explicit flag should be disabled");
    }

    inline void expect_no_enabled_subfeatures(auto const& p1_para)
    {
        expect(!p1_para.enable_multi_value, "multi-value should be disabled");
        expect(!p1_para.enable_reference_types, "reference-types should be disabled");
        expect(!p1_para.enable_bulk_memory, "bulk-memory should be disabled");
        expect(!p1_para.enable_sign_extension, "sign-extension should be disabled");
        expect(!p1_para.enable_nontrapping_float_to_int, "nontrapping should be disabled");
        expect(!p1_para.enable_simd, "simd should be disabled");
    }

    inline void expect_default_mvp_feature_state(auto const& p1_para)
    {
        expect(!p1_para.explicit_feature_1p1, "wasm1p1 collection should not be explicit by default");
        expect_no_explicit_subfeatures(p1_para);
        expect_no_enabled_subfeatures(p1_para);
        expect(p1_para.controllable_allow_multi_result_vector, "default state must keep MVP single-result validation");
        expect(p1_para.controllable_allow_multi_table, "default state must keep MVP single-table validation");
    }

    inline void expect_only_multi_value_enabled(auto const& p1_para)
    {
        expect(p1_para.enable_multi_value, "multi-value was not enabled");
        expect(!p1_para.enable_reference_types, "multi-value switch enabled reference-types");
        expect(!p1_para.enable_bulk_memory, "multi-value switch enabled bulk-memory");
        expect(!p1_para.enable_sign_extension, "multi-value switch enabled sign-extension");
        expect(!p1_para.enable_nontrapping_float_to_int, "multi-value switch enabled nontrapping");
        expect(!p1_para.enable_simd, "multi-value switch enabled simd");
        expect(p1_para.explicit_enable_multi_value, "multi-value explicit flag was not set");
        expect(!p1_para.controllable_allow_multi_result_vector, "multi-value should relax the MVP single-result check");
        expect(p1_para.controllable_allow_multi_table, "multi-value should not relax the MVP single-table check");
    }

    inline void expect_only_reference_types_enabled(auto const& p1_para)
    {
        expect(!p1_para.enable_multi_value, "reference-types switch enabled multi-value");
        expect(p1_para.enable_reference_types, "reference-types was not enabled");
        expect(!p1_para.enable_bulk_memory, "reference-types switch enabled bulk-memory");
        expect(!p1_para.enable_sign_extension, "reference-types switch enabled sign-extension");
        expect(!p1_para.enable_nontrapping_float_to_int, "reference-types switch enabled nontrapping");
        expect(!p1_para.enable_simd, "reference-types switch enabled simd");
        expect(p1_para.explicit_enable_reference_types, "reference-types explicit flag was not set");
        expect(p1_para.controllable_allow_multi_result_vector, "reference-types should not relax the MVP single-result check");
        expect(!p1_para.controllable_allow_multi_table, "reference-types should relax the MVP single-table check");
    }

    inline void expect_only_bulk_memory_enabled(auto const& p1_para)
    {
        expect(!p1_para.enable_multi_value, "bulk-memory switch enabled multi-value");
        expect(!p1_para.enable_reference_types, "bulk-memory switch enabled reference-types");
        expect(p1_para.enable_bulk_memory, "bulk-memory was not enabled");
        expect(!p1_para.enable_sign_extension, "bulk-memory switch enabled sign-extension");
        expect(!p1_para.enable_nontrapping_float_to_int, "bulk-memory switch enabled nontrapping");
        expect(!p1_para.enable_simd, "bulk-memory switch enabled simd");
        expect(p1_para.explicit_enable_bulk_memory, "bulk-memory explicit flag was not set");
        expect(p1_para.controllable_allow_multi_result_vector, "bulk-memory should not relax the MVP single-result check");
        expect(p1_para.controllable_allow_multi_table, "bulk-memory should not relax the MVP single-table check");
    }

    inline void expect_only_sign_extension_enabled(auto const& p1_para)
    {
        expect(!p1_para.enable_multi_value, "sign-extension switch enabled multi-value");
        expect(!p1_para.enable_reference_types, "sign-extension switch enabled reference-types");
        expect(!p1_para.enable_bulk_memory, "sign-extension switch enabled bulk-memory");
        expect(p1_para.enable_sign_extension, "sign-extension was not enabled");
        expect(!p1_para.enable_nontrapping_float_to_int, "sign-extension switch enabled nontrapping");
        expect(!p1_para.enable_simd, "sign-extension switch enabled simd");
        expect(p1_para.explicit_enable_sign_extension, "sign-extension explicit flag was not set");
        expect(p1_para.controllable_allow_multi_result_vector, "sign-extension should not relax the MVP single-result check");
        expect(p1_para.controllable_allow_multi_table, "sign-extension should not relax the MVP single-table check");
    }

    inline void expect_only_nontrapping_enabled(auto const& p1_para)
    {
        expect(!p1_para.enable_multi_value, "nontrapping switch enabled multi-value");
        expect(!p1_para.enable_reference_types, "nontrapping switch enabled reference-types");
        expect(!p1_para.enable_bulk_memory, "nontrapping switch enabled bulk-memory");
        expect(!p1_para.enable_sign_extension, "nontrapping switch enabled sign-extension");
        expect(p1_para.enable_nontrapping_float_to_int, "nontrapping was not enabled");
        expect(!p1_para.enable_simd, "nontrapping switch enabled simd");
        expect(p1_para.explicit_enable_nontrapping_float_to_int, "nontrapping explicit flag was not set");
        expect(p1_para.controllable_allow_multi_result_vector, "nontrapping should not relax the MVP single-result check");
        expect(p1_para.controllable_allow_multi_table, "nontrapping should not relax the MVP single-table check");
    }

    inline void expect_only_simd_enabled(auto const& p1_para)
    {
        expect(!p1_para.enable_multi_value, "simd switch enabled multi-value");
        expect(!p1_para.enable_reference_types, "simd switch enabled reference-types");
        expect(!p1_para.enable_bulk_memory, "simd switch enabled bulk-memory");
        expect(!p1_para.enable_sign_extension, "simd switch enabled sign-extension");
        expect(!p1_para.enable_nontrapping_float_to_int, "simd switch enabled nontrapping");
        expect(p1_para.enable_simd, "simd was not enabled");
        expect(p1_para.explicit_enable_simd, "simd explicit flag was not set");
        expect(p1_para.controllable_allow_multi_result_vector, "simd should not relax the MVP single-result check");
        expect(p1_para.controllable_allow_multi_table, "simd should not relax the MVP single-table check");
    }

    inline void expect_full_wasm1p1_enabled(auto const& p1_para)
    {
        expect(p1_para.explicit_feature_1p1, "wasm1p1 collection explicit flag was not set");
        expect_no_explicit_subfeatures(p1_para);
        expect(p1_para.enable_multi_value, "wasm1p1 collection did not enable multi-value");
        expect(p1_para.enable_reference_types, "wasm1p1 collection did not enable reference-types");
        expect(p1_para.enable_bulk_memory, "wasm1p1 collection did not enable bulk-memory");
        expect(p1_para.enable_sign_extension, "wasm1p1 collection did not enable sign-extension");
        expect(p1_para.enable_nontrapping_float_to_int, "wasm1p1 collection did not enable nontrapping");
        expect(p1_para.enable_simd, "wasm1p1 collection did not enable simd");
        expect(!p1_para.controllable_allow_multi_result_vector, "wasm1p1 collection should relax the MVP single-result check");
        expect(!p1_para.controllable_allow_multi_table, "wasm1p1 collection should relax the MVP single-table check");
    }

    inline void expect_callback_success(::uwvm2::utils::cmdline::handle_func_type callback, char const* label)
    {
        ::uwvm2::utils::cmdline::parameter_parsing_results args[1]{};
        args[0].type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::parameter;
        auto const res{callback(args, args, args + 1uz)};
        if(res != ::uwvm2::utils::cmdline::parameter_return_type::def) [[unlikely]] { fail(label); }
    }

    inline void test_cli_feature_controller()
    {
        namespace callbacks = ::uwvm2::uwvm::cmdline::params::details;

        reset_cli_wasm_parameter();
        expect_default_mvp_feature_state(cli_wasm1p1_parameter());

        reset_cli_wasm_parameter();
        expect_callback_success(callbacks::wasm_feature_enable_multi_value_callback, "multi-value callback failed");
        expect_only_multi_value_enabled(cli_wasm1p1_parameter());

        reset_cli_wasm_parameter();
        expect_callback_success(callbacks::wasm_feature_enable_reference_types_callback, "reference-types callback failed");
        expect_only_reference_types_enabled(cli_wasm1p1_parameter());

        reset_cli_wasm_parameter();
        expect_callback_success(callbacks::wasm_feature_enable_bulk_memory_callback, "bulk-memory callback failed");
        expect_only_bulk_memory_enabled(cli_wasm1p1_parameter());

        reset_cli_wasm_parameter();
        expect_callback_success(callbacks::wasm_feature_enable_sign_extension_callback, "sign-extension callback failed");
        expect_only_sign_extension_enabled(cli_wasm1p1_parameter());

        reset_cli_wasm_parameter();
        expect_callback_success(callbacks::wasm_feature_enable_nontrapping_float_to_int_callback, "nontrapping callback failed");
        expect_only_nontrapping_enabled(cli_wasm1p1_parameter());

        reset_cli_wasm_parameter();
        expect_callback_success(callbacks::wasm_feature_enable_simd_callback, "simd callback failed");
        expect_only_simd_enabled(cli_wasm1p1_parameter());

        reset_cli_wasm_parameter();
        expect_callback_success(callbacks::wasm_feature_1p1_callback, "wasm1p1 collection callback failed");
        expect_full_wasm1p1_enabled(cli_wasm1p1_parameter());
    }

    inline void test_parser_feature_defaults()
    {
        using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
        using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
        using value_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type;
        using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;

        ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1> fs_para{};
        auto const& p1_para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        expect_default_mvp_feature_state(p1_para);
        expect(::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(value_type::i32, fs_para), "i32 should remain MVP");
        expect(::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(value_type::i64, fs_para), "i64 should remain MVP");
        expect(::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(value_type::f32, fs_para), "f32 should remain MVP");
        expect(::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(value_type::f64, fs_para), "f64 should remain MVP");
        expect(!::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(value_type::v128, fs_para),
               "v128 should require simd");
        expect(!::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(value_type::funcref, fs_para),
               "funcref locals/globals should require reference-types");
        expect(!::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(value_type::externref, fs_para),
               "externref should require reference-types");
        expect(::uwvm2::parser::wasm::standard::wasm1p1::features::reference_type_enabled(reference_type::funcref, fs_para),
               "funcref table element type should remain MVP");
        expect(!::uwvm2::parser::wasm::standard::wasm1p1::features::reference_type_enabled(reference_type::externref, fs_para),
               "externref table element type should require reference-types");
    }

    /// @brief Parse the minimal empty module with the requested parser feature list.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline void parse_empty_module(::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para)
    {
        auto const* const begin{reinterpret_cast<::std::byte const*>(empty_wasm)};
        auto const* const end{begin + sizeof(empty_wasm)};
        ::uwvm2::parser::wasm::base::error_impl err{};
        auto module_storage{::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<Fs...>(begin, end, err, fs_para)};
        static_cast<void>(module_storage);
    }
}  // namespace test

int main()
{
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;

    test::test_cli_feature_controller();
    test::test_parser_feature_defaults();

    static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<wasm1>,
                                 ::uwvm2::parser::wasm::standard::wasm1::type::value_type>);
    static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<wasm1, wasm1p1>,
                                 ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type>);

    ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1> wasm1_para{};
    test::parse_empty_module<wasm1>(wasm1_para);

    ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1> wasm1p1_para{};
    auto& p1_para{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(wasm1p1_para)};
    p1_para.enable_multi_value = true;
    p1_para.enable_reference_types = true;
    p1_para.enable_bulk_memory = true;
    p1_para.enable_sign_extension = true;
    p1_para.enable_nontrapping_float_to_int = true;
    p1_para.enable_simd = true;
    p1_para.controllable_allow_multi_result_vector = false;
    p1_para.controllable_allow_multi_table = false;
    test::parse_empty_module<wasm1, wasm1p1>(wasm1p1_para);
}
