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
        using cli_mode = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode;

        /// @brief Access the global wasm1.1 parser feature parameter used by CLI callbacks.
        inline constexpr auto& wasm1p1_parameter() noexcept
        {
            return ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(
                ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para);
        }

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

        /// @brief Return a canonical spelling for whichever feature switch already owns the CLI.
        inline constexpr ::uwvm2::utils::container::u8string_view selected_feature_name(auto const& para) noexcept
        {
            switch(para.cli_mode)
            {
                case cli_mode::direct_mvp: return u8"--wasm-feature-wasmmvp";
                case cli_mode::direct_wasm1p1: return u8"--wasm-feature-wasm1p1";
                case cli_mode::direct_wasm2: return u8"--wasm-feature-wasm2";
                case cli_mode::scoped:
                {
                    if(para.explicit_enable_multi_value) { return u8"--wasm-feature-enable-multi-value"; }
                    if(para.explicit_enable_reference_types) { return u8"--wasm-feature-enable-reference-types"; }
                    if(para.explicit_enable_table_instructions) { return u8"--wasm-feature-enable-table-instructions"; }
                    if(para.explicit_enable_multiple_tables) { return u8"--wasm-feature-enable-multiple-tables"; }
                    if(para.explicit_enable_bulk_memory) { return u8"--wasm-feature-enable-bulk-memory"; }
                    if(para.explicit_enable_sign_extension) { return u8"--wasm-feature-enable-sign-extension"; }
                    if(para.explicit_enable_nontrapping_float_to_int) { return u8"--wasm-feature-enable-nontrapping-float-to-int"; }
                    if(para.explicit_enable_simd) { return u8"--wasm-feature-enable-simd"; }
                    if(para.explicit_disable_multi_value) { return u8"--wasm-feature-disable-multi-value"; }
                    if(para.explicit_disable_reference_types) { return u8"--wasm-feature-disable-reference-types"; }
                    if(para.explicit_disable_table_instructions) { return u8"--wasm-feature-disable-table-instructions"; }
                    if(para.explicit_disable_multiple_tables) { return u8"--wasm-feature-disable-multiple-tables"; }
                    if(para.explicit_disable_bulk_memory) { return u8"--wasm-feature-disable-bulk-memory"; }
                    if(para.explicit_disable_sign_extension) { return u8"--wasm-feature-disable-sign-extension"; }
                    if(para.explicit_disable_nontrapping_float_to_int) { return u8"--wasm-feature-disable-nontrapping-float-to-int"; }
                    if(para.explicit_disable_simd) { return u8"--wasm-feature-disable-simd"; }
                    return u8"--wasm-feature-enable/disable-*";
                }
                case cli_mode::unspecified: [[fallthrough]];
                default: return u8"--wasm-feature-*";
            }
        }

        /// @brief Claim the CLI for one complete feature-set selector.
        inline constexpr parameter_return_type begin_direct_feature(::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,
                                                                    cli_mode requested,
                                                                    bool& explicit_flag) noexcept
        {
            auto& para{wasm1p1_parameter()};
            if(para.cli_mode != cli_mode::unspecified) [[unlikely]]
            {
                return print_conflict(para_curr->str, selected_feature_name(para));
            }
            para.cli_mode = requested;
            explicit_flag = true;
            return parameter_return_type::def;
        }

        /// @brief Claim the CLI for scoped feature disables, rejecting either complete feature-set selector.
        inline constexpr parameter_return_type begin_scoped_feature(::uwvm2::utils::cmdline::parameter_parsing_results* para_curr) noexcept
        {
            auto& para{wasm1p1_parameter()};
            if(para.cli_mode != cli_mode::unspecified && para.cli_mode != cli_mode::scoped) [[unlikely]]
            {
                return print_conflict(para_curr->str, selected_feature_name(para));
            }
            para.cli_mode = cli_mode::scoped;
            return parameter_return_type::def;
        }

        /// @brief Disable the complete wasm1.1 feature collection and restore MVP runtime guards.
        inline constexpr void apply_mvp_feature_set(auto& para) noexcept
        {
            para.disable_multi_value = true;
            para.disable_reference_types = true;
            para.disable_table_instructions = true;
            para.disable_multiple_tables = true;
            para.disable_bulk_memory = true;
            para.disable_sign_extension = true;
            para.disable_nontrapping_float_to_int = true;
            para.disable_simd = true;
            para.controllable_allow_multi_result_vector = true;
            para.controllable_allow_multi_table = true;
        }

        /// @brief Enable the complete wasm1.1 feature collection and relax MVP runtime guards.
        inline constexpr void apply_wasm1p1_feature_set(auto& para) noexcept
        {
            para.disable_multi_value = false;
            para.disable_reference_types = false;
            para.disable_table_instructions = false;
            para.disable_multiple_tables = false;
            para.disable_bulk_memory = false;
            para.disable_sign_extension = false;
            para.disable_nontrapping_float_to_int = false;
            para.disable_simd = false;
            para.controllable_allow_multi_result_vector = false;
            para.controllable_allow_multi_table = false;
        }

        inline constexpr void apply_wasm2_feature_set(auto& para) noexcept { apply_wasm1p1_feature_set(para); }

        /// @brief Configure one independent WebAssembly 2.0 feature group.
        inline constexpr parameter_return_type configure_single_feature(::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,
                                                                        ::uwvm2::utils::container::u8string_view same_name,
                                                                        ::uwvm2::utils::container::u8string_view opposite_name,
                                                                        bool& explicit_flag,
                                                                        bool& opposite_explicit_flag,
                                                                        bool& disable_flag,
                                                                        bool disable,
                                                                        bool controls_multi_result,
                                                                        bool controls_multiple_tables) noexcept
        {
            if(auto const ret{begin_scoped_feature(para_curr)}; ret != parameter_return_type::def) [[unlikely]] { return ret; }
            if(explicit_flag) [[unlikely]] { return print_conflict(para_curr->str, same_name); }
            if(opposite_explicit_flag) [[unlikely]] { return print_conflict(para_curr->str, opposite_name); }

            auto& para{wasm1p1_parameter()};
            explicit_flag = true;
            disable_flag = disable;
            if(controls_multi_result) { para.controllable_allow_multi_result_vector = disable; }
            if(controls_multiple_tables) { para.controllable_allow_multi_table = disable; }
            return parameter_return_type::def;
        }
    }  // namespace wasm_feature_details

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        /// @brief Handle --wasm-feature-mvp.
        ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_mvp_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto& para{wasm_feature_details::wasm1p1_parameter()};
        if(auto const ret{wasm_feature_details::begin_direct_feature(
               para_curr, wasm_feature_details::cli_mode::direct_mvp, para.explicit_feature_mvp)};
           ret != wasm_feature_details::parameter_return_type::def) [[unlikely]]
        {
            return ret;
        }
        wasm_feature_details::apply_mvp_feature_set(para);
        return wasm_feature_details::parameter_return_type::def;
    }

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        /// @brief Handle --wasm-feature-wasm1.1.
        ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_wasm1p1_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto& para{wasm_feature_details::wasm1p1_parameter()};
        if(auto const ret{wasm_feature_details::begin_direct_feature(
               para_curr, wasm_feature_details::cli_mode::direct_wasm1p1, para.explicit_feature_wasm1p1)};
           ret != wasm_feature_details::parameter_return_type::def) [[unlikely]]
        {
            return ret;
        }
        wasm_feature_details::apply_wasm1p1_feature_set(para);
        return wasm_feature_details::parameter_return_type::def;
    }

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        /// @brief Handle --wasm-feature-wasm2.
        ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_wasm2_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_end) noexcept
    {
        auto& para{wasm_feature_details::wasm1p1_parameter()};
        if(auto const ret{wasm_feature_details::begin_direct_feature(
               para_curr, wasm_feature_details::cli_mode::direct_wasm2, para.explicit_feature_wasm2)};
           ret != wasm_feature_details::parameter_return_type::def) [[unlikely]]
        {
            return ret;
        }
        wasm_feature_details::apply_wasm2_feature_set(para);
        return wasm_feature_details::parameter_return_type::def;
    }

#if defined(UWVM_MODULE)
# define UWVM_WASM_FEATURE_CALLBACK_LINKAGE extern "C++" UWVM_GNU_COLD
#else
# define UWVM_WASM_FEATURE_CALLBACK_LINKAGE UWVM_GNU_COLD inline constexpr
#endif

#define UWVM_DEFINE_WASM_FEATURE_PAIR(member_name, cli_suffix, controls_multi_result, controls_multiple_tables)                                       \
    UWVM_WASM_FEATURE_CALLBACK_LINKAGE ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_enable_##member_name##_callback(                   \
        [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_begin,                                                              \
        ::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,                                                                                \
        [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_end) noexcept                                                       \
    {                                                                                                                                                  \
        auto& para{wasm_feature_details::wasm1p1_parameter()};                                                                                         \
        return wasm_feature_details::configure_single_feature(                                                                                        \
            para_curr, u8"--wasm-feature-enable-" cli_suffix, u8"--wasm-feature-disable-" cli_suffix,                                                \
            para.explicit_enable_##member_name, para.explicit_disable_##member_name, para.disable_##member_name,                                     \
            false, controls_multi_result, controls_multiple_tables);                                                                                   \
    }                                                                                                                                                  \
    UWVM_WASM_FEATURE_CALLBACK_LINKAGE ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_disable_##member_name##_callback(                  \
        [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_begin,                                                              \
        ::uwvm2::utils::cmdline::parameter_parsing_results* para_curr,                                                                                \
        [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results* para_end) noexcept                                                       \
    {                                                                                                                                                  \
        auto& para{wasm_feature_details::wasm1p1_parameter()};                                                                                         \
        return wasm_feature_details::configure_single_feature(                                                                                        \
            para_curr, u8"--wasm-feature-disable-" cli_suffix, u8"--wasm-feature-enable-" cli_suffix,                                                \
            para.explicit_disable_##member_name, para.explicit_enable_##member_name, para.disable_##member_name,                                     \
            true, controls_multi_result, controls_multiple_tables);                                                                                    \
    }

    UWVM_DEFINE_WASM_FEATURE_PAIR(multi_value, "multi-value", true, false)
    UWVM_DEFINE_WASM_FEATURE_PAIR(reference_types, "reference-types", false, false)
    UWVM_DEFINE_WASM_FEATURE_PAIR(table_instructions, "table-instructions", false, false)
    UWVM_DEFINE_WASM_FEATURE_PAIR(multiple_tables, "multiple-tables", false, true)
    UWVM_DEFINE_WASM_FEATURE_PAIR(bulk_memory, "bulk-memory", false, false)
    UWVM_DEFINE_WASM_FEATURE_PAIR(sign_extension, "sign-extension", false, false)
    UWVM_DEFINE_WASM_FEATURE_PAIR(nontrapping_float_to_int, "nontrapping-float-to-int", false, false)
    UWVM_DEFINE_WASM_FEATURE_PAIR(simd, "simd", false, false)

#undef UWVM_DEFINE_WASM_FEATURE_PAIR
#undef UWVM_WASM_FEATURE_CALLBACK_LINKAGE
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
