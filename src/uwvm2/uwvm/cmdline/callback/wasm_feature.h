/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#ifndef UWVM_MODULE
# ifndef UWVM
#  define UWVM
# endif
// std
# include <cstddef>
# include <cstdint>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/handle.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/features/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/params/wasm_feature.h>
# include <uwvm2/uwvm/wasm/storage/mode.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
    namespace wasm_feature_details
    {
        using parameter_return_type = ::uwvm2::utils::cmdline::parameter_return_type;
        using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;

        /// @brief Access the global wasm1.1 parser feature parameter used by CLI callbacks.
        inline constexpr auto& wasm1p1_parameter() noexcept
        { return ::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para); }

        /// @brief Print a deterministic conflict diagnostic for wasm feature parameters.
        inline constexpr parameter_return_type print_conflict(::uwvm2::utils::container::u8string_view curr,
                                                              ::uwvm2::utils::container::u8string_view conflict) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Conflicting Wasm feature parameters: \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                curr,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" conflicts with \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                conflict,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\".\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            return parameter_return_type::return_m1_imme;
        }

        /// @brief Return whether any independent wasm1.1 subfeature was explicitly selected.
        inline constexpr bool any_subfeature_was_explicitly_enabled(auto const& para) noexcept
        {
            return para.explicit_enable_multi_value || para.explicit_enable_reference_types || para.explicit_enable_bulk_memory ||
                   para.explicit_enable_sign_extension || para.explicit_enable_nontrapping_float_to_int || para.explicit_enable_simd;
        }

        /// @brief Return the first explicitly selected subfeature name for conflict diagnostics.
        inline constexpr ::uwvm2::utils::container::u8string_view first_explicit_subfeature_name(auto const& para) noexcept
        {
            if(para.explicit_enable_multi_value) { return u8"--wasm-feature-enable-multi-value"; }
            if(para.explicit_enable_reference_types) { return u8"--wasm-feature-enable-reference-types"; }
            if(para.explicit_enable_bulk_memory) { return u8"--wasm-feature-enable-bulk-memory"; }
            if(para.explicit_enable_sign_extension) { return u8"--wasm-feature-enable-sign-extension"; }
            if(para.explicit_enable_nontrapping_float_to_int) { return u8"--wasm-feature-enable-nontrapping-float-to-int"; }
            if(para.explicit_enable_simd) { return u8"--wasm-feature-enable-simd"; }
            return {};
        }

        /// @brief Enable the complete wasm1.1 feature collection and relax its runtime MVP guards.
        inline constexpr void enable_1p1_feature_set(auto& para) noexcept
        {
            para.enable_multi_value = true;
            para.enable_reference_types = true;
            para.enable_bulk_memory = true;
            para.enable_sign_extension = true;
            para.enable_nontrapping_float_to_int = true;
            para.enable_simd = true;
            para.controllable_allow_multi_result_vector = false;
            para.controllable_allow_multi_table = false;
        }
    }  // namespace wasm_feature_details

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        /// @brief Handle --wasm-feature-1p1 and reject already-selected subfeatures.
        ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_1p1_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto& para{wasm_feature_details::wasm1p1_parameter()};
        if(wasm_feature_details::any_subfeature_was_explicitly_enabled(para)) [[unlikely]]
        {
            return wasm_feature_details::print_conflict(para_curr->str, wasm_feature_details::first_explicit_subfeature_name(para));
        }

        para.explicit_feature_1p1 = true;
        wasm_feature_details::enable_1p1_feature_set(para);
        return wasm_feature_details::parameter_return_type::def;
    }

    /// @brief Enable one independent wasm feature and reject collection/subfeature conflicts.
    inline constexpr ::uwvm2::utils::cmdline::parameter_return_type enable_single_wasm_feature(::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
                                                                                               ::uwvm2::utils::container::u8string_view feature_name,
                                                                                               bool& explicit_flag,
                                                                                               bool& enable_flag,
                                                                                               bool disable_multi_result_check,
                                                                                               bool disable_multi_table_check) noexcept
    {
        auto& para{wasm_feature_details::wasm1p1_parameter()};
        if(para.explicit_feature_1p1) [[unlikely]] { return wasm_feature_details::print_conflict(para_curr->str, u8"--wasm-feature-1p1"); }

        if(explicit_flag) [[unlikely]] { return wasm_feature_details::print_conflict(para_curr->str, feature_name); }

        explicit_flag = true;
        enable_flag = true;
        if(disable_multi_result_check) { para.controllable_allow_multi_result_vector = false; }
        if(disable_multi_table_check) { para.controllable_allow_multi_table = false; }
        return wasm_feature_details::parameter_return_type::def;
    }

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        /// @brief Handle --wasm-feature-enable-multi-value.
        ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_enable_multi_value_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto& para{wasm_feature_details::wasm1p1_parameter()};
        return enable_single_wasm_feature(para_curr,
                                          u8"--wasm-feature-enable-multi-value",
                                          para.explicit_enable_multi_value,
                                          para.enable_multi_value,
                                          true,
                                          false);
    }

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        /// @brief Handle --wasm-feature-enable-reference-types.
        ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_enable_reference_types_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto& para{wasm_feature_details::wasm1p1_parameter()};
        return enable_single_wasm_feature(para_curr,
                                          u8"--wasm-feature-enable-reference-types",
                                          para.explicit_enable_reference_types,
                                          para.enable_reference_types,
                                          false,
                                          true);
    }

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        /// @brief Handle --wasm-feature-enable-bulk-memory.
        ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_enable_bulk_memory_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto& para{wasm_feature_details::wasm1p1_parameter()};
        return enable_single_wasm_feature(para_curr,
                                          u8"--wasm-feature-enable-bulk-memory",
                                          para.explicit_enable_bulk_memory,
                                          para.enable_bulk_memory,
                                          false,
                                          false);
    }

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        /// @brief Handle --wasm-feature-enable-sign-extension.
        ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_enable_sign_extension_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto& para{wasm_feature_details::wasm1p1_parameter()};
        return enable_single_wasm_feature(para_curr,
                                          u8"--wasm-feature-enable-sign-extension",
                                          para.explicit_enable_sign_extension,
                                          para.enable_sign_extension,
                                          false,
                                          false);
    }

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        /// @brief Handle --wasm-feature-enable-nontrapping-float-to-int.
        ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_enable_nontrapping_float_to_int_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto& para{wasm_feature_details::wasm1p1_parameter()};
        return enable_single_wasm_feature(para_curr,
                                          u8"--wasm-feature-enable-nontrapping-float-to-int",
                                          para.explicit_enable_nontrapping_float_to_int,
                                          para.enable_nontrapping_float_to_int,
                                          false,
                                          false);
    }

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        /// @brief Handle --wasm-feature-enable-simd.
        ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_enable_simd_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto& para{wasm_feature_details::wasm1p1_parameter()};
        return enable_single_wasm_feature(para_curr, u8"--wasm-feature-enable-simd", para.explicit_enable_simd, para.enable_simd, false, false);
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
