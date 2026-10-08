/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
// FP global initialization is bit transport, including imported/global.get copies.
// The parser's preserved f32/f64 bytes must reach rec.global/g.global storage via
// memcpy; a native floating assignment/return can quiet sNaNs on x87/68881.
// Public execution guards do not cover this earlier stage and cannot restore
// bits already lost. Keep both widths and resolved-global paths byte-based.
// See documents/runtime/floating-point-change-rationale.md.

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <atomic>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <concepts>
# include <limits>
# include <memory>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/const_expr/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/features/impl.h>
# include <uwvm2/parser/wasm/standard/wasm3/type/impl.h>
# include <uwvm2/validation/standard/wasm3/constant_expression.h>
# include <uwvm2/validation/standard/wasm3/reference_policy.h>
# include <uwvm2/validation/standard/wasm3/value_immediate.h>
# include <uwvm2/validation/standard/wasm3/declaration_policy.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/object/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/imported/wasi/wasip1/storage/impl.h>
# include <uwvm2/uwvm/wasm/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/runtime/lib/uwvm_runtime_generated_wasm_bridge.h>
# include "init_limit.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::initializer
{
    extern "C++" { class restoration_context; }
}

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::initializer
{
    namespace details
    {
#include "initialization_context.h"
        template <typename... Args>
        inline constexpr void verbose_info(Args&&... args) noexcept
        {
#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
            // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                ::std::forward<Args>(args)...,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_GREEN),
                                u8"[",
                                ::uwvm2::uwvm::io::get_local_realtime(),
                                u8"] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                u8"(verbose)\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#else
            // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
            auto const local_realtime{::uwvm2::uwvm::io::get_local_realtime()};
            auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
            ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
            auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

            ::fast_io::io::perr(u8log_output_ul,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE));
            (::fast_io::io::perr(u8log_output_ul, ::std::forward<Args>(args)), ...);
            ::fast_io::io::perr(u8log_output_ul,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_GREEN),
                                u8"[",
                                local_realtime,
                                u8"] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                u8"(verbose)\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#endif
        }

        // Runtime import resolution repeats the WASI Preview 1 visibility check
        // used by the pre-runtime dependency pass. The dependency pass provides
        // the user-facing error in normal executable mode, while this runtime
        // guard keeps the linker defensive when runtime initialization is called
        // after a non-standard module-table change or with unresolved imports
        // intentionally preserved for diagnostics.
        template <initialization_purpose Purpose>
        [[nodiscard]] inline constexpr bool is_wasip1_import_visible_for_wasm_module_in_context(initialization_context<Purpose>& world, ::uwvm2::utils::container::u8string_view consumer_module_name,
                                                                                     ::uwvm2::utils::container::u8string_view import_module_name) noexcept
        {
#if defined(UWVM_IMPORT_WASI_WASIP1) && !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1)
            if(import_module_name != u8"wasi_snapshot_preview1") [[likely]] { return true; }

            auto const consumer_module_it{world.declarations().find(consumer_module_name)};
            if(consumer_module_it == world.declarations().end()) [[unlikely]] { return true; }

            switch(consumer_module_it->second.type)
            {
                case ::uwvm2::uwvm::wasm::type::module_type_t::exec_wasm:
                {
                    if(auto const override_state{::uwvm2::uwvm::imported::wasi::wasip1::storage::find_wasip1_module_override_const(
                           ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_module_target_kind_t::main_wasm,
                           consumer_module_name)};
                       override_state != nullptr && override_state->enabled_is_set) [[unlikely]]
                    {
                        return override_state->enabled;
                    }
                    return ::uwvm2::uwvm::wasm::storage::local_preload_wasip1;
                }
                case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_wasm:
                {
                    if(auto const override_state{::uwvm2::uwvm::imported::wasi::wasip1::storage::find_wasip1_module_override_const(
                           ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_module_target_kind_t::preload_wasm,
                           consumer_module_name)};
                       override_state != nullptr && override_state->enabled_is_set) [[unlikely]]
                    {
                        return override_state->enabled;
                    }
                    return ::uwvm2::uwvm::wasm::storage::local_preload_wasip1;
                }
                [[unlikely]] default:
                {
                    return true;
                }
            }
#else
            static_cast<void>(consumer_module_name);
            static_cast<void>(import_module_name);
            return true;
#endif
        }

        [[nodiscard]] inline constexpr bool is_wasip1_import_visible_for_wasm_module(::uwvm2::utils::container::u8string_view consumer_module_name,
                                                                                     ::uwvm2::utils::container::u8string_view import_module_name) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return is_wasip1_import_visible_for_wasm_module_in_context(world, consumer_module_name, import_module_name);
        }

        // use for verbose output
        inline ::uwvm2::utils::container::u8string_view current_initializing_module_name{};  // [global]
        // After `error_on_unresolved_imports_after_linking()` succeeds, imported-alias cycles must not exist.
        // This flag allows later resolution helpers to skip redundant cycle checks in the normal initialization pipeline,
        // while still remaining defensive if those helpers are used elsewhere.
        inline bool import_alias_sanity_checked{};  // [global]

        template <initialization_purpose Purpose, typename... Args>
        inline constexpr void verbose_module_info_in_context(initialization_context<Purpose>& world, Args&&... args) noexcept;
        template <typename... Args>
        inline constexpr void verbose_module_info(Args&&... args) noexcept;

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view
            wasm1p1_initializer_feature_name(::uwvm2::parser::wasm::base::wasm1p1_feature_kind feature) noexcept
        {
            using feature_kind = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
            switch(feature)
            {
                case feature_kind::multi_value: return u8"multi-value";
                case feature_kind::bulk_memory: return u8"bulk-memory";
                case feature_kind::reference_types: return u8"reference-types";
                case feature_kind::sign_extension: return u8"sign-extension";
                case feature_kind::nontrapping_float_to_int: return u8"nontrapping-float-to-int";
                case feature_kind::simd: return u8"simd";
                case feature_kind::threads: return u8"threads";
                case feature_kind::memory64: return u8"--wasm-feature-enable-memory64";
                case feature_kind::table64: return u8"--wasm-feature-enable-table64";
                case feature_kind::function_references: return u8"--wasm-feature-enable-function-references";
                case feature_kind::gc: return u8"--wasm-feature-enable-gc";
                case feature_kind::exceptions: return u8"--wasm-feature-enable-exceptions";
                case feature_kind::extended_const: return u8"--wasm-feature-enable-extended-const";
                case feature_kind::table_initializer: return u8"--wasm-feature-enable-table-initializer";
                [[unlikely]] default: return u8"unknown";
            }
        }

        template <initialization_purpose Purpose>
        [[noreturn]] inline constexpr void fatal_wasm1p1_initializer_feature_required_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind feature,
            ::uwvm2::utils::container::u8string_view subject,
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 value) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"initializer: In module \"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                world.current_module(),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                ::fast_io::mnp::cond((feature == ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::memory64 ||
                                    feature == ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::table64 ||
                                    feature == ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::function_references ||
                                    feature == ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc ||
                                    feature == ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions ||
                                    feature == ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::extended_const ||
                                    feature == ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::table_initializer),
                                    u8"\", WebAssembly 3.0 ", u8"\", WebAssembly 1.1 "),
                                subject,
                                u8" requires ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                wasm1p1_initializer_feature_name(feature),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8" (value=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                value,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8").\n\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            ::fast_io::fast_terminate();
        }

        [[noreturn]] inline constexpr void fatal_wasm1p1_initializer_feature_required(
            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind feature,
            ::uwvm2::utils::container::u8string_view subject,
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 value) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            fatal_wasm1p1_initializer_feature_required_in_context(world, feature, subject, value);
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view
            wasm2_initializer_feature_name(::uwvm2::parser::wasm::base::wasm2_feature_kind feature) noexcept
        {
            using feature_kind = ::uwvm2::parser::wasm::base::wasm2_feature_kind;
            switch(feature)
            {
                case feature_kind::table_instructions: return u8"table-instructions";
                case feature_kind::multiple_tables: return u8"multiple-tables";
                [[unlikely]] default: return u8"unknown";
            }
        }

        template <initialization_purpose Purpose>
        [[noreturn]] inline constexpr void fatal_wasm2_initializer_feature_required_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::parser::wasm::base::wasm2_feature_kind feature,
            ::uwvm2::utils::container::u8string_view subject,
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 value) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"initializer: In module \"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                world.current_module(),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\", WebAssembly 2.0 ",
                                subject,
                                u8" requires ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                wasm2_initializer_feature_name(feature),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8" (value=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                value,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8").\n\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            ::fast_io::fast_terminate();
        }

        [[noreturn]] inline constexpr void fatal_wasm2_initializer_feature_required(
            ::uwvm2::parser::wasm::base::wasm2_feature_kind feature,
            ::uwvm2::utils::container::u8string_view subject,
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 value) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            fatal_wasm2_initializer_feature_required_in_context(world, feature, subject, value);
        }

        [[nodiscard]] inline constexpr ::uwvm2::parser::wasm::base::wasm1p1_feature_kind wasm1p1_initializer_feature_for_value_type(
            ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type value_type) noexcept
        {
            using feature_kind = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
            using value_type_t = ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type;
            switch(value_type)
            {
                case value_type_t::v128: return feature_kind::simd;
                case value_type_t::funcref: [[fallthrough]];
                case value_type_t::externref: return feature_kind::reference_types;
                default:
                {
                    auto const code{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(value_type)};
                    if(code == 0x69u || code == 0x74u) { return feature_kind::exceptions; }
                    if(code == 0x72u || code == 0x73u) { return feature_kind::function_references; }
                    if((code >= 0x6au && code <= 0x6eu) || code == 0x71u) { return feature_kind::gc; }
                    return feature_kind::multi_value;
                }
            }
        }

        template <initialization_purpose Purpose, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void check_wasm1p1_initializer_value_type_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type value_type,
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
            ::uwvm2::utils::container::u8string_view subject) noexcept
        {
            // Core 3 exnref uses the existing 0x69 ABI carrier. The Wasm 1.1
            // value_type_enabled() predicate predates exceptions, so applying it
            // here would misreport every valid exnref as a multi-value failure.
            if(static_cast<unsigned>(value_type) == 0x69u)
            {
                auto const& policy{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
                if(policy.disable_exceptions) [[unlikely]]
                { fatal_wasm1p1_initializer_feature_required_in_context(world, 
                    ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions, subject, 0x69u); }
                if(policy.disable_reference_types) [[unlikely]]
                { fatal_wasm1p1_initializer_feature_required_in_context(world, 
                    ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types, subject, 0x69u); }
                return;
            }
            if(!::uwvm2::parser::wasm::standard::wasm1p1::features::value_type_enabled(value_type, fs_para)) [[unlikely]]
            {
                fatal_wasm1p1_initializer_feature_required_in_context(world, 
                    wasm1p1_initializer_feature_for_value_type(value_type),
                    subject,
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(value_type)));
            }
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void check_wasm1p1_initializer_value_type(
            ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type value_type,
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
            ::uwvm2::utils::container::u8string_view subject) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            check_wasm1p1_initializer_value_type_in_context(world, value_type, fs_para, subject);
        }

        template <initialization_purpose Purpose, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void check_wasm1p1_initializer_reference_type_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type reference_type,
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
            ::uwvm2::utils::container::u8string_view subject) noexcept
        {
            if(!::uwvm2::parser::wasm::standard::wasm1p1::features::reference_type_enabled(reference_type, fs_para)) [[unlikely]]
            {
                fatal_wasm1p1_initializer_feature_required_in_context(world, 
                    ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                    subject,
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(reference_type)));
            }
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void check_wasm1p1_initializer_reference_type(
            ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type reference_type,
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
            ::uwvm2::utils::container::u8string_view subject) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            check_wasm1p1_initializer_reference_type_in_context(world, reference_type, fs_para, subject);
        }

        template <initialization_purpose Purpose, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void check_wasm1p1_initializer_table_reference_type_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type reference_type,
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
            ::uwvm2::utils::container::u8string_view subject) noexcept
        {
            if(!::uwvm2::parser::wasm::standard::wasm1p1::features::table_reference_type_enabled(reference_type, fs_para)) [[unlikely]]
            {
                auto const& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
                if(para.disable_table_instructions) [[unlikely]]
                {
                    fatal_wasm2_initializer_feature_required_in_context(world, 
                        ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions,
                        subject,
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(reference_type)));
                }
                if(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(reference_type) == 0x69u &&
                   para.disable_exceptions) [[unlikely]]
                {
                    fatal_wasm1p1_initializer_feature_required_in_context(world, 
                        ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions,
                        subject,
                        0x69u);
                }
                fatal_wasm1p1_initializer_feature_required_in_context(world, 
                    ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types,
                    subject,
                    static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(
                        static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(reference_type)));
            }
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void check_wasm1p1_initializer_table_reference_type(
            ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type reference_type,
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
            ::uwvm2::utils::container::u8string_view subject) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            check_wasm1p1_initializer_table_reference_type_in_context(world, reference_type, fs_para, subject);
        }

        template <initialization_purpose Purpose, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void check_wasm1p1_initializer_const_expr_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...> const& expr,
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
            ::uwvm2::utils::container::u8string_view subject,
            ::std::size_t imported_global_count) noexcept
        {
            auto const& wasm1p1_para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
            for(auto const& op: expr.opcodes)
            {
                auto const opcode{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(op.opcode)};
                if(wasm1p1_para.disable_extended_const &&
                   (::uwvm2::validation::standard::wasm3::constant_integer_width(opcode) != 0u ||
                    (opcode == 0x23u && op.storage.imported_global_idx >= imported_global_count))) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                        u8"uwvm: ",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                        u8"[fatal] ",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                        u8"initializer: ", subject, u8" requires --wasm-feature-enable-extended-const.\n\n",
                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
                if(opcode == 0xD0u || opcode == 0xD2u)
                {
                    if(opcode == 0xD0u)
                    {
                        using heap = ::uwvm2::parser::wasm::standard::wasm3::type::abstract_heap_type;
                        auto const code{op.storage.ref_null_heap};
                        if(wasm1p1_para.disable_gc &&
                           (code == static_cast<::std::int_least64_t>(heap::any) ||
                            code == static_cast<::std::int_least64_t>(heap::eq) ||
                            code == static_cast<::std::int_least64_t>(heap::i31) ||
                            code == static_cast<::std::int_least64_t>(heap::struct_) ||
                            code == static_cast<::std::int_least64_t>(heap::array) ||
                            code == static_cast<::std::int_least64_t>(heap::none))) [[unlikely]]
                        { fatal_wasm1p1_initializer_feature_required_in_context(world, ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc, subject, opcode); }
                        if(wasm1p1_para.disable_exceptions &&
                           (code == static_cast<::std::int_least64_t>(heap::exn) ||
                            code == static_cast<::std::int_least64_t>(heap::noexn))) [[unlikely]]
                        { fatal_wasm1p1_initializer_feature_required_in_context(world, ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions, subject, opcode); }
                    }
                    if(opcode == 0xD0u && op.storage.ref_null_heap >= 0 && wasm1p1_para.disable_function_references) [[unlikely]]
                    {
                        fatal_wasm1p1_initializer_feature_required_in_context(world, ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::function_references, subject, opcode);
                    }
                    if(wasm1p1_para.disable_reference_types) [[unlikely]]
                    {
                        fatal_wasm1p1_initializer_feature_required_in_context(world, ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types, subject, opcode);
                    }
                }
                else if(opcode == 0xFDu)
                {
                    if(wasm1p1_para.disable_simd) [[unlikely]]
                    {
                        fatal_wasm1p1_initializer_feature_required_in_context(world, ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::simd, subject, opcode);
                    }
                }
                else if(opcode == 0xFBu)
                {
                    if(wasm1p1_para.disable_gc) [[unlikely]]
                    { fatal_wasm1p1_initializer_feature_required_in_context(world, ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc, subject, opcode); }
                    if(wasm1p1_para.disable_reference_types) [[unlikely]]
                    { fatal_wasm1p1_initializer_feature_required_in_context(world, ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types, subject, opcode); }
                }
            }
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void check_wasm1p1_initializer_const_expr(
            ::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_const_expr<Fs...> const& expr,
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
            ::uwvm2::utils::container::u8string_view subject,
            ::std::size_t imported_global_count) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            check_wasm1p1_initializer_const_expr_in_context(world, expr, fs_para, subject, imported_global_count);
        }

        template <initialization_purpose Purpose, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void enforce_wasm1p1_initializer_feature_parameters_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) noexcept
        {
            if constexpr((::std::same_as<::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1, Fs> || ...))
            {
                using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
                using feature_kind = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
                using data_type = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_type_t;
                using element_type = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_type_t;
                constexpr ::std::size_t feature_check_importdesc_table_index{1uz};
                constexpr ::std::size_t feature_check_importdesc_global_index{3uz};

                auto const& wasm1p1_para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
                auto const& tagsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1p1::features::tag_section_storage_t<Fs...>>(module_storage.sections)};
                if(wasm1p1_para.disable_exceptions && (tagsec.present || !::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(module_storage.sections).importdesc.index_unchecked(4uz).empty())) [[unlikely]]
                { fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::exceptions, u8"tag section", 13u); }


                auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(module_storage.sections)};
                if(wasm1p1_para.disable_gc && typesec.requires_gc) [[unlikely]]
                { fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::gc, u8"recursive type group", 0x4eu); }
                // Parsing and initialization may receive different policy objects. Preserve the encoding gate
                // even though the executable signature carrier is equivalent to a legacy nullable reference.
                auto const& local_section{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(module_storage.sections)};
                if(wasm1p1_para.disable_reference_types || wasm1p1_para.disable_simd)
                {
                    // Check compressed declaration runs, including zero-count runs. No guest frame is allocated or touched.
                    for(auto const& code : local_section.codes)
                    { for(auto const& local : code.locals) { check_wasm1p1_initializer_value_type_in_context(world, local.type, fs_para, u8"local declaration"); } }
                }
                if(wasm1p1_para.disable_gc)
                {
                    // Only a stricter GC policy walks exact compressed local declarations.
                    // Count-zero runs still carry a checked type and cannot erase its requirement.
                    for(auto const& code : local_section.codes)
                    {
                        for(auto const& local : code.locals)
                        {
                            if(local.has_core_type && ::uwvm2::validation::standard::wasm3::core3_value_requires_gc(
                                local.core_type, ::std::addressof(typesec.core3_context), typesec.types.size())) [[unlikely]]
                            { fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::gc, u8"local declaration", local.core_type.source_prefix); }
                        }
                    }
                }
                if(wasm1p1_para.disable_function_references)
                {
                    if(local_section.locals_require_function_references) [[unlikely]]
                    { fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::function_references, u8"local declaration", 0x63u); }
                    // The actual parser's exact bit also includes unused aggregate
                    // fields. Flat function carriers cannot retain those declarations.
                    if(typesec.requires_function_references) [[unlikely]]
                    { fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::function_references, u8"type declaration", 0x63u); }
                }

                auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(module_storage.sections)};
                auto const& tablesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>>(module_storage.sections)};
                auto const& memorysec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>(module_storage.sections)};
                auto const& globalsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>>(module_storage.sections)};
                auto const& elemsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>>(module_storage.sections)};
                auto const& datasec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::data_section_storage_t<Fs...>>(module_storage.sections)};
                auto const& datacountsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...>>(module_storage.sections)};

                if(wasm1p1_para.disable_exceptions)
                {
                    // Decode-time metadata covers all declarations, including imports,
                    // unused type/field slots and count-zero compressed local runs.
                    auto const required{::uwvm2::validation::standard::wasm3::get_exception_declaration_requirement(false,
                        {.types = typesec.requires_exceptions, .tables = tablesec.requires_exceptions,
                         .globals = globalsec.requires_exceptions, .elements = elemsec.requires_exceptions,
                         .locals = local_section.locals_require_exceptions})};
                    if(required.required) [[unlikely]]
                    {
                        ::uwvm2::utils::container::u8string_view subject{};
                        using site = ::uwvm2::parser::wasm::base::wasm1p1_error_subject;
                        switch(required.subject)
                        {
                            case site::function_type: subject = u8"type declaration"; break;
                            case site::table_type: subject = u8"table declaration"; break;
                            case site::global_type: subject = u8"global declaration"; break;
                            case site::element_segment: subject = u8"element declaration"; break;
                            case site::local_type: subject = u8"local declaration"; break;
                            default: ::uwvm2::utils::debug::trap_and_inform_bug_pos();
                        }
                        fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::exceptions, subject, required.value);
                    }
                }
                if(wasm1p1_para.disable_gc)
                {
                    if(tablesec.requires_gc) [[unlikely]]
                    { fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::gc, u8"table declaration", 0x63u); }
                    if(globalsec.requires_gc) [[unlikely]]
                    { fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::gc, u8"global declaration", 0x63u); }
                    if(elemsec.requires_gc) [[unlikely]]
                    { fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::gc, u8"element declaration", 0x63u); }
                }
                if(wasm1p1_para.disable_function_references || wasm1p1_para.disable_reference_types)
                {
                    auto const feature{wasm1p1_para.disable_function_references ? feature_kind::function_references : feature_kind::reference_types};
                    if(tablesec.requires_function_references) [[unlikely]]
                    { fatal_wasm1p1_initializer_feature_required_in_context(world, feature, u8"table declaration", 0x63u); }
                    if(globalsec.requires_function_references) [[unlikely]]
                    { fatal_wasm1p1_initializer_feature_required_in_context(world, feature, u8"global declaration", 0x63u); }
                    if(elemsec.requires_function_references) [[unlikely]]
                    { fatal_wasm1p1_initializer_feature_required_in_context(world, feature, u8"element declaration", 0x63u); }
                }

                // Exact requirements were collected in the real declaration decoders,
                // including unused aggregate fields and direct/recursive function types.
                auto const type_requirement{::uwvm2::validation::standard::wasm3::get_type_declaration_requirement(
                    !wasm1p1_para.disable_reference_types, !wasm1p1_para.disable_simd,
                    !(wasm1p1_para.disable_multi_value || wasm1p1_para.controllable_allow_multi_result_vector),
                    {.simd = typesec.requires_simd, .reference_types = typesec.requires_reference_types,
                     .multi_value = typesec.requires_multi_value})};
                if(type_requirement.required) [[unlikely]]
                { fatal_wasm1p1_initializer_feature_required_in_context(world, type_requirement.feature, u8"type declaration", type_requirement.value); }
                auto const storage_requirement{::uwvm2::validation::standard::wasm3::get_storage_declaration_requirement(
                    !wasm1p1_para.disable_reference_types, !wasm1p1_para.disable_simd,
                    {.tables_reference_types = tablesec.requires_reference_types,
                     .globals_reference_types = globalsec.requires_reference_types,
                     .elements_reference_types = elemsec.requires_reference_types, .globals_simd = globalsec.requires_simd,
                     .table_reference_value = tablesec.reference_types_diagnostic_value,
                     .global_reference_value = globalsec.reference_types_diagnostic_value,
                     .element_reference_value = elemsec.reference_types_diagnostic_value})};
                if(storage_requirement.required) [[unlikely]]
                {
                    using site = ::uwvm2::parser::wasm::base::wasm1p1_error_subject;
                    ::uwvm2::utils::container::u8string_view subject{};
                    switch(storage_requirement.subject)
                    {
                        case site::table_type: subject = u8"table declaration"; break;
                        case site::global_type: subject = u8"global declaration"; break;
                        case site::element_segment: subject = u8"element declaration"; break;
                        default: ::uwvm2::utils::debug::trap_and_inform_bug_pos();
                    }
                    fatal_wasm1p1_initializer_feature_required_in_context(world, storage_requirement.feature, subject, storage_requirement.value);
                }
                auto const address_requirement{::uwvm2::validation::standard::wasm3::get_address_declaration_requirement(
                    !wasm1p1_para.disable_memory64, !wasm1p1_para.disable_table64,
                    !wasm1p1_para.disable_threads, !wasm1p1_para.disable_multi_memory,
                    {.memory64 = memorysec.requires_memory64, .table64 = tablesec.requires_table64,
                     .shared = memorysec.requires_threads,
                     .multi_memory = ::uwvm2::validation::standard::wasm3::core3_has_multiple_declarations(
                         importsec.importdesc.index_unchecked(2uz).size(), memorysec.memories.size())})};
                if(address_requirement.required) [[unlikely]]
                {
                    ::uwvm2::utils::container::u8string_view const subject{
                        address_requirement.subject == ::uwvm2::parser::wasm::base::wasm1p1_error_subject::table_type ?
                        ::uwvm2::utils::container::u8string_view{u8"table declaration"} :
                        ::uwvm2::utils::container::u8string_view{u8"memory declaration"}};
                    fatal_wasm1p1_initializer_feature_required_in_context(world, address_requirement.feature, subject, address_requirement.value);
                }

                auto const constant_requirement{::uwvm2::validation::standard::wasm3::get_constant_declaration_requirement(
                    !wasm1p1_para.disable_table_initializer, !wasm1p1_para.disable_extended_const,
                    {.table_initializer = tablesec.requires_table_initializer,
                     .extended_const = globalsec.constant_expressions_require_extended_const,
                     .extended_const_value = globalsec.extended_const_diagnostic_value,
                     .extended_const_subject = globalsec.extended_const_diagnostic_subject})};
                if(constant_requirement.required) [[unlikely]]
                {
                    using site = ::uwvm2::parser::wasm::base::wasm1p1_error_subject;
                    ::uwvm2::utils::container::u8string_view subject{};
                    switch(constant_requirement.subject)
                    {
                        case site::table_type: subject = u8"table initializer"; break;
                        case site::element_segment: subject = u8"element constant expression"; break;
                        case site::data_segment: subject = u8"data offset constant expression"; break;
                        default: subject = u8"global constant expression"; break;
                    }
                    fatal_wasm1p1_initializer_feature_required_in_context(world, constant_requirement.feature, subject, constant_requirement.value);
                }
                auto const opcode_requirement{::uwvm2::validation::standard::wasm3::get_constant_opcode_requirement(
                    wasm1p1_para, globalsec.constant_expression_opcode_requirements)};
                if(opcode_requirement.required) [[unlikely]]
                {
                    using site = ::uwvm2::parser::wasm::base::wasm1p1_error_subject;
                    ::uwvm2::utils::container::u8string_view subject{};
                    switch(opcode_requirement.subject)
                    {
                        case site::table_type: subject = u8"table initializer"; break;
                        case site::element_segment: subject = u8"element constant expression"; break;
                        case site::data_segment: subject = u8"data offset constant expression"; break;
                        default: subject = u8"global constant expression"; break;
                    }
                    fatal_wasm1p1_initializer_feature_required_in_context(world, opcode_requirement.feature, subject, opcode_requirement.value);
                }

                for(auto const& type: typesec.types)
                {
                    for(auto curr{type.parameter.begin}; curr != type.parameter.end; ++curr)
                    {
                        check_wasm1p1_initializer_value_type_in_context(world, *curr, fs_para, u8"function parameter type");
                    }
                    auto const result_count{static_cast<::std::size_t>(type.result.end - type.result.begin)};
                    if(!::uwvm2::parser::wasm::standard::wasm2::features::feature_enabled(
                           wasm1p1_para,
                           ::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind::multi_value) &&
                       result_count > 1uz) [[unlikely]]
                    {
                        fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::multi_value,
                                                                   u8"function result vector",
                                                                   static_cast<wasm_u32>(result_count));
                    }
                    for(auto curr{type.result.begin}; curr != type.result.end; ++curr)
                    {
                        check_wasm1p1_initializer_value_type_in_context(world, *curr, fs_para, u8"function result type");
                    }
                }

                static_assert(importsec.importdesc_count > feature_check_importdesc_global_index);
                for(auto const* imported_table_ptr: importsec.importdesc.index_unchecked(feature_check_importdesc_table_index))
                {
                    if(imported_table_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    check_wasm1p1_initializer_table_reference_type_in_context(world, imported_table_ptr->imports.storage.table.reftype, fs_para, u8"imported table type");
                }
                for(auto const* imported_global_ptr: importsec.importdesc.index_unchecked(feature_check_importdesc_global_index))
                {
                    if(imported_global_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    check_wasm1p1_initializer_value_type_in_context(world, imported_global_ptr->imports.storage.global.type, fs_para, u8"imported global type");
                }

                auto const imported_table_count{importsec.importdesc.index_unchecked(feature_check_importdesc_table_index).size()};
                auto const local_table_count{tablesec.tables.size()};
                if((wasm1p1_para.disable_multiple_tables || wasm1p1_para.controllable_allow_multi_table) &&
                   (imported_table_count > 1uz || local_table_count > 1uz || (imported_table_count == 1uz && local_table_count == 1uz))) [[unlikely]]
                {
                    fatal_wasm2_initializer_feature_required_in_context(world, ::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables,
                                                             u8"multiple table definitions/imports",
                                                             static_cast<wasm_u32>(imported_table_count + local_table_count));
                }

                for(auto const& table: tablesec.tables) { check_wasm1p1_initializer_table_reference_type_in_context(world, table.reftype, fs_para, u8"local table type"); }
                for(auto const& expr: tablesec.initializers)
                {
                    if(expr.opcodes.empty()) { continue; }
                    if(wasm1p1_para.disable_table_initializer) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"initializer: table initializer requires --wasm-feature-enable-table-initializer.\n\n",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }
                    check_wasm1p1_initializer_const_expr_in_context(world, expr, fs_para, u8"table initializer",
                        importsec.importdesc.index_unchecked(feature_check_importdesc_global_index).size());
                }
                for(auto const& global: globalsec.local_globals)
                {
                    check_wasm1p1_initializer_value_type_in_context(world, global.global.type, fs_para, u8"local global type");
                    check_wasm1p1_initializer_const_expr_in_context(world, global.expr, fs_para, u8"local global initializer",
                        importsec.importdesc.index_unchecked(feature_check_importdesc_global_index).size());
                }

                if(datacountsec.present && wasm1p1_para.disable_bulk_memory) [[unlikely]]
                {
                    fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::bulk_memory,
                                                               u8"data count section",
                                                               ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<
                                                                   Fs...>::section_id);
                }

                for(auto const& data: datasec.datas)
                {
                    if((data.type == data_type::passive || data.type == data_type::active_explicit) && wasm1p1_para.disable_bulk_memory) [[unlikely]]
                    {
                        fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::bulk_memory,
                                                                   u8"data segment",
                                                                   static_cast<wasm_u32>(data.type));
                    }
                    check_wasm1p1_initializer_const_expr_in_context(world, data.storage.segment.expr, fs_para, u8"data segment offset initializer",
                        importsec.importdesc.index_unchecked(feature_check_importdesc_global_index).size());
                }

                for(auto const& elem: elemsec.elems)
                {
                    switch(elem.type)
                    {
                        case element_type::passive_funcidx:
                        {
                            if(wasm1p1_para.disable_bulk_memory) [[unlikely]]
                            {
                                fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::bulk_memory,
                                                                           u8"element segment",
                                                                           static_cast<wasm_u32>(elem.type));
                            }
                            break;
                        }
                        case element_type::active_explicit_funcidx:
                        {
                            if(wasm1p1_para.disable_multiple_tables || wasm1p1_para.controllable_allow_multi_table) [[unlikely]]
                            {
                                fatal_wasm2_initializer_feature_required_in_context(world, ::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables,
                                                                         u8"element segment",
                                                                         static_cast<wasm_u32>(elem.type));
                            }
                            break;
                        }
                        case element_type::declarative_funcidx:
                        {
                            if(wasm1p1_para.disable_bulk_memory) [[unlikely]]
                            {
                                fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::bulk_memory,
                                                                           u8"element segment",
                                                                           static_cast<wasm_u32>(elem.type));
                            }
                            break;
                        }
                        case element_type::active_implicit_expr:
                        {
                            if(wasm1p1_para.disable_reference_types) [[unlikely]]
                            {
                                fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::reference_types,
                                                                           u8"element segment",
                                                                           static_cast<wasm_u32>(elem.type));
                            }
                            break;
                        }
                        case element_type::passive_expr:
                        {
                            if(wasm1p1_para.disable_bulk_memory) [[unlikely]]
                            {
                                fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::bulk_memory,
                                                                           u8"element segment",
                                                                           static_cast<wasm_u32>(elem.type));
                            }
                            if(wasm1p1_para.disable_reference_types) [[unlikely]]
                            {
                                fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::reference_types,
                                                                           u8"element segment",
                                                                           static_cast<wasm_u32>(elem.type));
                            }
                            break;
                        }
                        case element_type::active_explicit_expr:
                        {
                            if(wasm1p1_para.disable_reference_types) [[unlikely]]
                            {
                                fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::reference_types,
                                                                           u8"element segment",
                                                                           static_cast<wasm_u32>(elem.type));
                            }
                            if(wasm1p1_para.disable_multiple_tables || wasm1p1_para.controllable_allow_multi_table) [[unlikely]]
                            {
                                fatal_wasm2_initializer_feature_required_in_context(world, ::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables,
                                                                         u8"element segment",
                                                                         static_cast<wasm_u32>(elem.type));
                            }
                            break;
                        }
                        case element_type::declarative_expr:
                        {
                            if(wasm1p1_para.disable_bulk_memory) [[unlikely]]
                            {
                                fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::bulk_memory,
                                                                           u8"element segment",
                                                                           static_cast<wasm_u32>(elem.type));
                            }
                            if(wasm1p1_para.disable_reference_types) [[unlikely]]
                            {
                                fatal_wasm1p1_initializer_feature_required_in_context(world, feature_kind::reference_types,
                                                                           u8"element segment",
                                                                           static_cast<wasm_u32>(elem.type));
                            }
                            break;
                        }
                        case element_type::active_implicit_funcidx: break;
                        [[unlikely]] default:
                        {
                            ::fast_io::fast_terminate();
                        }
                    }

                    check_wasm1p1_initializer_reference_type_in_context(world, elem.storage.segment.reftype, fs_para, u8"element segment reference type");
                    check_wasm1p1_initializer_const_expr_in_context(world, elem.storage.segment.expr, fs_para, u8"element segment offset initializer",
                        importsec.importdesc.index_unchecked(feature_check_importdesc_global_index).size());
                    for(auto const& expr: elem.storage.segment.vec_expr) { check_wasm1p1_initializer_const_expr_in_context(world, expr, fs_para, u8"element expression",
                        importsec.importdesc.index_unchecked(feature_check_importdesc_global_index).size()); }
                }
            }
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void enforce_wasm1p1_initializer_feature_parameters(
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            enforce_wasm1p1_initializer_feature_parameters_in_context(world, module_storage, fs_para);
        }

        template <initialization_purpose Purpose>
        inline constexpr void
            fatal_reserve_limit_exceeded_in_context(initialization_context<Purpose>& world, ::uwvm2::utils::container::u8string_view reserve_name, ::std::size_t requested, ::std::size_t limit) noexcept
        {
            if(world.current_module().empty())
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"initializer: Reserve limit exceeded (reserve=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                    reserve_name,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8", requested=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                    requested,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8", limit=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                    limit,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8").\n\n",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"initializer: In module \"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                world.current_module(),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\", reserve limit exceeded (reserve=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                reserve_name,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8", requested=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                requested,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8", limit=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                limit,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8").\n\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            ::fast_io::fast_terminate();
        }

        inline constexpr void
            fatal_reserve_limit_exceeded(::uwvm2::utils::container::u8string_view reserve_name, ::std::size_t requested, ::std::size_t limit) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            fatal_reserve_limit_exceeded_in_context(world, reserve_name, requested, limit);
        }

        template <initialization_purpose Purpose>
        inline constexpr void check_reserve_limit_in_context(initialization_context<Purpose>& world, ::uwvm2::utils::container::u8string_view reserve_name, ::std::size_t requested, ::std::size_t limit) noexcept
        {
            if(requested > limit) [[unlikely]] { fatal_reserve_limit_exceeded_in_context(world, reserve_name, requested, limit); }
        }

        inline constexpr void check_reserve_limit(::uwvm2::utils::container::u8string_view reserve_name, ::std::size_t requested, ::std::size_t limit) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            check_reserve_limit_in_context(world, reserve_name, requested, limit);
        }

        using runtime_memory_limits_t = ::uwvm2::uwvm::wasm::type::module_memory_limit_t;
        using configured_runtime_memory_limits_t = ::uwvm2::uwvm::wasm::storage::configured_module_memory_limit_t;
        using configured_import_reset_t = ::uwvm2::uwvm::wasm::storage::configured_import_reset_t;
        using configured_import_reset_vec_t = ::uwvm2::uwvm::wasm::storage::configured_import_reset_vec_t;

        [[nodiscard]] inline constexpr ::std::size_t default_linear_memory_page_size_bytes() noexcept
        { return static_cast<::std::size_t>(::uwvm2::object::memory::wasm_page::default_wasm32_page_size); }

        [[nodiscard]] inline constexpr ::std::size_t linear_memory_page_size_bytes_from_log2(unsigned page_size_log2) noexcept
        {
            if(page_size_log2 >= ::std::numeric_limits<::std::size_t>::digits) [[unlikely]] { ::fast_io::fast_terminate(); }
            return 1uz << page_size_log2;
        }

        [[nodiscard]] inline constexpr bool linear_memory_byte_count_overflows(::std::size_t page_count, ::std::size_t page_size_bytes) noexcept
        { return page_size_bytes != 0uz && page_count > ::std::numeric_limits<::std::size_t>::max() / page_size_bytes; }

        [[nodiscard]] inline constexpr ::std::size_t linear_memory_byte_count_unchecked(::std::size_t page_count, ::std::size_t page_size_bytes) noexcept
        { return page_count * page_size_bytes; }

        template <initialization_purpose Purpose>
        inline constexpr void emit_local_memory_init_verbose_in_context(initialization_context<Purpose>& world, ::uwvm2::utils::container::u8string_view stage,
                                                             ::std::size_t memory_index,
                                                             auto const& declared_limits,
                                                             runtime_memory_limits_t const& effective_limits,
                                                             ::std::size_t page_size_bytes) noexcept
        {
            if(!::uwvm2::uwvm::io::show_verbose) [[likely]] { return; }

            auto const initial_pages{effective_limits.min};
            auto const initial_bytes_overflow{linear_memory_byte_count_overflows(initial_pages, page_size_bytes)};

            if(initial_bytes_overflow) [[unlikely]]
            {
                verbose_module_info_in_context(world, u8"Init: ",
                                    stage,
                                    u8" local memory (memory_idx=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    memory_index,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8", backend=\"",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    ::uwvm2::object::memory::linear::native_memory_t::name,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"\", declared=\"",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    section_details(declared_limits),
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"\", effective=\"",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    ::uwvm2::uwvm::wasm::type::section_details(effective_limits),
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"\", page_size_bytes=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    page_size_bytes,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8", initial_pages=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    initial_pages,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8", initial_bytes=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    u8"overflow",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"). ");
                return;
            }

            verbose_module_info_in_context(world, u8"Init: ",
                                stage,
                                u8" local memory (memory_idx=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                memory_index,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8", backend=\"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                ::uwvm2::object::memory::linear::native_memory_t::name,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\", declared=\"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                section_details(declared_limits),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\", effective=\"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                ::uwvm2::uwvm::wasm::type::section_details(effective_limits),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\", page_size_bytes=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                page_size_bytes,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8", initial_pages=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                initial_pages,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8", initial_bytes=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                linear_memory_byte_count_unchecked(initial_pages, page_size_bytes),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"). ");
        }

        inline constexpr void emit_local_memory_init_verbose(::uwvm2::utils::container::u8string_view stage,
                                                             ::std::size_t memory_index,
                                                             auto const& declared_limits,
                                                             runtime_memory_limits_t const& effective_limits,
                                                             ::std::size_t page_size_bytes) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            emit_local_memory_init_verbose_in_context(world, stage, memory_index, declared_limits, effective_limits, page_size_bytes);
        }

        inline constexpr void runtime_warning_to_fatal() noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"Convert warnings to fatal errors. ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                u8"(runtime)\n\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            ::fast_io::fast_terminate();
        }

        [[nodiscard]] inline constexpr runtime_memory_limits_t
            parser_memory_limits_to_runtime_limits(auto const& limits) noexcept
        {
            runtime_memory_limits_t result{};
            // Declared limits are u64 even on ISA32. Reject an unrepresentable
            // minimum before any narrowing; a large maximum is a resource cap,
            // not a request to allocate that much memory.
            constexpr auto native_max{::std::numeric_limits<::std::size_t>::max()};
            if(limits.min > native_max) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                    u8"uwvm: [fatal] initializer: memory minimum exceeds the native page-count range.\n\n");
                ::fast_io::fast_terminate();
            }
            result.min = static_cast<::std::size_t>(limits.min);
            result.present_max = limits.present_max;
            result.max = limits.present_max && limits.max < native_max ? static_cast<::std::size_t>(limits.max) : native_max;
            return result;
        }

        [[nodiscard]] inline constexpr bool
            runtime_memory_limits_widen_declared(auto const& declared_limits,
                                                 runtime_memory_limits_t const& effective_limits) noexcept
        {
            if(effective_limits.min < declared_limits.min) { return true; }

            if(declared_limits.present_max)
            {
                if(!effective_limits.present_max) { return true; }
                if(effective_limits.max > declared_limits.max) { return true; }
            }

            return false;
        }

        template <initialization_purpose Purpose>
        inline constexpr void emit_runtime_memory_limit_widen_warning_in_context(initialization_context<Purpose>& world, ::uwvm2::utils::container::u8string_view memory_kind,
                                                                      ::std::size_t memory_index,
                                                                      auto const& declared_limits,
                                                                      runtime_memory_limits_t const& effective_limits) noexcept
        {
            if(!::uwvm2::uwvm::io::show_runtime_warning) [[likely]] { return; }

#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
            // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                u8"[warn]  ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"initializer: In module \"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                world.current_module(),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\", ",
                                memory_kind,
                                u8" memory limit override widens the declared limits (memory_idx=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                memory_index,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8", declared=\"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                section_details(declared_limits),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\", override=\"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                ::uwvm2::uwvm::wasm::type::section_details(effective_limits),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\"). ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                u8"(runtime)\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#else
            // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
            {
                auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                    ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                ::fast_io::io::perr(u8log_output_ul,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                    u8"[warn]  ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"initializer: In module \"");
                ::fast_io::io::perr(u8log_output_ul,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                    world.current_module(),
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"\", ",
                                    memory_kind,
                                    u8" memory limit override widens the declared limits (memory_idx=");
                ::fast_io::io::perr(u8log_output_ul,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                    memory_index,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8", declared=\"",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                    section_details(declared_limits),
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"\", override=\"");
                ::fast_io::io::perr(u8log_output_ul,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                    ::uwvm2::uwvm::wasm::type::section_details(effective_limits),
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"\"). ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                    u8"(runtime)\n",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            }
#endif

            if(::uwvm2::uwvm::io::runtime_warning_fatal) [[unlikely]] { runtime_warning_to_fatal(); }
        }

        inline constexpr void emit_runtime_memory_limit_widen_warning(::uwvm2::utils::container::u8string_view memory_kind,
                                                                      ::std::size_t memory_index,
                                                                      auto const& declared_limits,
                                                                      runtime_memory_limits_t const& effective_limits) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            emit_runtime_memory_limit_widen_warning_in_context(world, memory_kind, memory_index, declared_limits, effective_limits);
        }

        template <initialization_purpose Purpose>
        inline constexpr void fatal_runtime_memory_limit_no_local_defined_memory_in_context(initialization_context<Purpose>& world, configured_runtime_memory_limits_t const& configured_limits) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"initializer: In module \"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                world.current_module(),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\", memory limit override targets local-defined memories, but the module defines none (apply_to_all=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                configured_limits.apply_to_all_memories,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8", explicit_memory_overrides=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                configured_limits.local_defined_memory_limits.size(),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8").\n\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            ::fast_io::fast_terminate();
        }

        inline constexpr void fatal_runtime_memory_limit_no_local_defined_memory(configured_runtime_memory_limits_t const& configured_limits) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            fatal_runtime_memory_limit_no_local_defined_memory_in_context(world, configured_limits);
        }

        template <initialization_purpose Purpose>
        inline constexpr void fatal_runtime_memory_limit_out_of_range_in_context(initialization_context<Purpose>& world, ::std::size_t memory_index,
                                                                      ::std::size_t local_defined_memory_count,
                                                                      runtime_memory_limits_t const& configured_limits) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::os_c_str(::uwvm2::uwvm::utils::ansies::put_color ? UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE : u8""),
                                u8"uwvm: ",
                                ::fast_io::mnp::os_c_str(::uwvm2::uwvm::utils::ansies::put_color ? UWVM_COLOR_U8_LT_RED : u8""),
                                u8"[fatal] ",
                                ::fast_io::mnp::os_c_str(::uwvm2::uwvm::utils::ansies::put_color ? UWVM_COLOR_U8_WHITE : u8""),
                                u8"initializer: In module \"",
                                ::fast_io::mnp::os_c_str(::uwvm2::uwvm::utils::ansies::put_color ? UWVM_COLOR_U8_YELLOW : u8""),
                                world.current_module(),
                                ::fast_io::mnp::os_c_str(::uwvm2::uwvm::utils::ansies::put_color ? UWVM_COLOR_U8_WHITE : u8""),
                                u8"\", local-defined memory limit override targets an out-of-range memory index (memory_idx=",
                                ::fast_io::mnp::os_c_str(::uwvm2::uwvm::utils::ansies::put_color ? UWVM_COLOR_U8_YELLOW : u8""),
                                memory_index,
                                ::fast_io::mnp::os_c_str(::uwvm2::uwvm::utils::ansies::put_color ? UWVM_COLOR_U8_WHITE : u8""),
                                u8", local_defined_memory_count=",
                                ::fast_io::mnp::os_c_str(::uwvm2::uwvm::utils::ansies::put_color ? UWVM_COLOR_U8_YELLOW : u8""),
                                local_defined_memory_count,
                                ::fast_io::mnp::os_c_str(::uwvm2::uwvm::utils::ansies::put_color ? UWVM_COLOR_U8_WHITE : u8""),
                                u8", override=\"",
                                ::fast_io::mnp::os_c_str(::uwvm2::uwvm::utils::ansies::put_color ? UWVM_COLOR_U8_YELLOW : u8""),
                                ::uwvm2::uwvm::wasm::type::section_details(configured_limits),
                                ::fast_io::mnp::os_c_str(::uwvm2::uwvm::utils::ansies::put_color ? UWVM_COLOR_U8_WHITE : u8""),
                                u8"\").\n\n",
                                ::fast_io::mnp::os_c_str(::uwvm2::uwvm::utils::ansies::put_color ? UWVM_COLOR_U8_RST_ALL : u8""));
            ::fast_io::fast_terminate();
        }

        inline constexpr void fatal_runtime_memory_limit_out_of_range(::std::size_t memory_index,
                                                                      ::std::size_t local_defined_memory_count,
                                                                      runtime_memory_limits_t const& configured_limits) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            fatal_runtime_memory_limit_out_of_range_in_context(world, memory_index, local_defined_memory_count, configured_limits);
        }

        inline constexpr void fatal_runtime_memory_limit_unknown_module(::uwvm2::utils::container::u8string_view module_name,
                                                                        configured_runtime_memory_limits_t const& configured_limits) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"initializer: Memory limit override targets an unknown or non-runtime module \"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                module_name,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\" (apply_to_all=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                configured_limits.apply_to_all_memories,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8", explicit_memory_overrides=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                configured_limits.local_defined_memory_limits.size(),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8").\n\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            ::fast_io::fast_terminate();
        }

        struct resolved_configured_local_defined_memory_limit_t
        {
            runtime_memory_limits_t const* limits{};
            bool from_all{};
        };

        [[nodiscard]] inline constexpr auto resolve_configured_local_defined_memory_limit(configured_runtime_memory_limits_t const* override_limits,
                                                                                          ::std::size_t local_memory_index) noexcept
            -> resolved_configured_local_defined_memory_limit_t
        {
            if(override_limits == nullptr) [[likely]] { return {}; }

            if(auto const it{override_limits->local_defined_memory_limits.find(local_memory_index)}; it != override_limits->local_defined_memory_limits.cend())
                [[likely]]
            {
                return {::std::addressof(it->second), false};
            }

            if(override_limits->apply_to_all_memories) [[likely]] { return {::std::addressof(override_limits->all_limits), true}; }

            return {};
        }

        template <initialization_purpose Purpose>
        inline constexpr void emit_runtime_memory_limit_override_verbose_in_context(initialization_context<Purpose>& world, ::std::size_t memory_index,
                                                                         bool from_all,
                                                                         auto const& declared_limits,
                                                                         runtime_memory_limits_t const& effective_limits) noexcept
        {
            if(!::uwvm2::uwvm::io::show_verbose) [[likely]] { return; }
            auto const override_source{from_all ? ::uwvm2::utils::container::u8string_view{u8"`all`"}
                                                : ::uwvm2::utils::container::u8string_view{u8"`memory_idx`"}};

            verbose_module_info_in_context(world, u8"Init: apply ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                override_source,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8" memory limit override to local-defined memory (memory_idx=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                memory_index,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8", declared=\"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                section_details(declared_limits),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\", effective=\"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                ::uwvm2::uwvm::wasm::type::section_details(effective_limits),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\"). ");
        }

        inline constexpr void emit_runtime_memory_limit_override_verbose(::std::size_t memory_index,
                                                                         bool from_all,
                                                                         auto const& declared_limits,
                                                                         runtime_memory_limits_t const& effective_limits) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            emit_runtime_memory_limit_override_verbose_in_context(world, memory_index, from_all, declared_limits, effective_limits);
        }

        template <initialization_purpose Purpose>
        inline constexpr void validate_configured_runtime_memory_limit_target_modules_in_context(initialization_context<Purpose>& world) noexcept
        {
            for(auto const& [module_name, configured_limits]: world.memory_limit_policy())
            {
                auto const module_name_view{
                    ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()}
                };
                if(world.modules().find(module_name_view) ==
                   world.modules().cend()) [[unlikely]]
                {
                    fatal_runtime_memory_limit_unknown_module(module_name_view, configured_limits);
                }
            }
        }

        inline constexpr void validate_configured_runtime_memory_limit_target_modules() noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            validate_configured_runtime_memory_limit_target_modules_in_context(world);
        }

        inline constexpr void fatal_configured_import_reset_unknown_module(::uwvm2::utils::container::u8string_view module_name,
                                                                           configured_import_reset_vec_t const& rules) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"initializer: Import reset targets an unknown or non-runtime Wasm file module \"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                module_name,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\" (rules=",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                rules.size(),
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8").\n\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            ::fast_io::fast_terminate();
        }

        inline constexpr void fatal_configured_import_reset_no_match(::uwvm2::utils::container::u8string_view module_name,
                                                                     configured_import_reset_t const& rule) noexcept
        {
#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
            // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"initializer: Import reset did not match any import in module \"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                module_name,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\" (from=\"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                rule.import_module_name,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\".\"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                rule.import_extern_name,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\", to=\"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                rule.new_import_module_name,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\".\"",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                rule.new_import_extern_name,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"\").\n\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#else
            // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
            {
                auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                    ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                ::fast_io::io::perr(u8log_output_ul,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"initializer: Import reset did not match any import in module \"");
                ::fast_io::io::perr(u8log_output_ul,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                    module_name,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"\" (from=\"",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    rule.import_module_name,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"\".\"");
                ::fast_io::io::perr(u8log_output_ul,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    rule.import_extern_name,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"\", to=\"",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    rule.new_import_module_name,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"\".\"");
                ::fast_io::io::perr(u8log_output_ul,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    rule.new_import_extern_name,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"\").\n\n",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            }
#endif
            ::fast_io::fast_terminate();
        }

        template <initialization_purpose Purpose>
        inline constexpr void reset_configured_import_reset_match_counts_in_context(initialization_context<Purpose>& world) noexcept
        {
            for(auto& [module_name, rules]: world.import_reset_policy())
            {
                static_cast<void>(module_name);
                for(auto& rule: rules) { rule.matched_count = 0uz; }
            }
        }

        inline constexpr void reset_configured_import_reset_match_counts() noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            reset_configured_import_reset_match_counts_in_context(world);
        }

        template <initialization_purpose Purpose>
        inline constexpr void validate_configured_import_reset_target_modules_in_context(initialization_context<Purpose>& world) noexcept
        {
            for(auto const& [module_name, rules]: world.import_reset_policy())
            {
                auto const module_name_view{
                    ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()}
                };
                if(world.modules().find(module_name_view) ==
                   world.modules().cend()) [[unlikely]]
                {
                    fatal_configured_import_reset_unknown_module(module_name_view, rules);
                }
            }
        }

        inline constexpr void validate_configured_import_reset_target_modules() noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            validate_configured_import_reset_target_modules_in_context(world);
        }

        template <initialization_purpose Purpose>
        inline constexpr void validate_configured_import_reset_matches_in_context(initialization_context<Purpose>& world) noexcept
        {
            for(auto const& [module_name, rules]: world.import_reset_policy())
            {
                auto const module_name_view{
                    ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()}
                };
                for(auto const& rule: rules)
                {
                    if(rule.matched_count == 0uz) [[unlikely]] { fatal_configured_import_reset_no_match(module_name_view, rule); }
                }
            }
        }

        inline constexpr void validate_configured_import_reset_matches() noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            validate_configured_import_reset_matches_in_context(world);
        }

        template <initialization_purpose Purpose, typename... Args>
        inline constexpr void verbose_module_info_in_context(initialization_context<Purpose>& world, Args&&... args) noexcept;
        template <typename... Args>
        inline constexpr void verbose_module_info(Args&&... args) noexcept;

        template <initialization_purpose Purpose>
        [[nodiscard]] inline constexpr ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_import_type_t const*
            apply_configured_import_reset_to_import_type_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_import_type_t const* import_ptr,
                                                         ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& out,
                                                         configured_import_reset_vec_t* rules) noexcept
        {
            if(import_ptr == nullptr || rules == nullptr) [[likely]] { return import_ptr; }

            for(auto& rule: *rules)
            {
                if(import_ptr->module_name == rule.import_module_name && import_ptr->extern_name == rule.import_extern_name) [[unlikely]]
                {
                    ++rule.matched_count;

                    auto rewritten_import{*import_ptr};
                    rewritten_import.module_name =
                        ::uwvm2::utils::container::u8string_view{rule.new_import_module_name.data(), rule.new_import_module_name.size()};
                    rewritten_import.extern_name =
                        ::uwvm2::utils::container::u8string_view{rule.new_import_extern_name.data(), rule.new_import_extern_name.size()};
                    auto& rewritten_slot{out.rewritten_import_vec_storage.emplace_back_unchecked(::std::move(rewritten_import))};

                    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        verbose_module_info_in_context(world, u8"Init: reset import binding \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                            rule.import_module_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\".\"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                            rule.import_extern_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\" -> \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                            rule.new_import_module_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\".\"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                            rule.new_import_extern_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\". ");
                    }

                    return ::std::addressof(rewritten_slot);
                }
            }

            return import_ptr;
        }

        [[nodiscard]] inline constexpr ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_import_type_t const*
            apply_configured_import_reset_to_import_type(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_import_type_t const* import_ptr,
                                                         ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& out,
                                                         configured_import_reset_vec_t* rules) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return apply_configured_import_reset_to_import_type_in_context(world, import_ptr, out, rules);
        }

        template <initialization_purpose Purpose>
        [[nodiscard]] inline constexpr runtime_memory_limits_t
            resolve_effective_runtime_memory_limits_in_context(initialization_context<Purpose>& world, auto const& declared_limits,
                                                    runtime_memory_limits_t const* override_limits,
                                                    ::uwvm2::utils::container::u8string_view memory_kind,
                                                    ::std::size_t memory_index) noexcept
        {
            auto result{parser_memory_limits_to_runtime_limits(declared_limits)};
            if(override_limits == nullptr) [[likely]] { return result; }

            result = *override_limits;
            if(runtime_memory_limits_widen_declared(declared_limits, result)) [[unlikely]]
            {
                emit_runtime_memory_limit_widen_warning_in_context(world, memory_kind, memory_index, declared_limits, result);
            }

            return result;
        }

        [[nodiscard]] inline constexpr runtime_memory_limits_t
            resolve_effective_runtime_memory_limits(auto const& declared_limits,
                                                    runtime_memory_limits_t const* override_limits,
                                                    ::uwvm2::utils::container::u8string_view memory_kind,
                                                    ::std::size_t memory_index) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return resolve_effective_runtime_memory_limits_in_context(world, declared_limits, override_limits, memory_kind, memory_index);
        }

        template <initialization_purpose Purpose, typename... Args>
        inline constexpr void verbose_module_info_in_context(initialization_context<Purpose>& world, Args&&... args) noexcept
        {
            if(world.current_module().empty())
            {
                verbose_info(::std::forward<Args>(args)...);
                return;
            }

            verbose_info(u8"initializer: Module \"",
                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                         world.current_module(),
                         ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                         u8"\": ",
                         ::std::forward<Args>(args)...);
        }

        template <typename... Args>
        inline constexpr void verbose_module_info(Args&&... args) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            verbose_module_info_in_context(world, ::std::forward<Args>( args )...);
        }

        template <typename NodePtr, typename NextFn, typename OnCycleFn>
        inline constexpr void check_no_import_alias_cycle_floyd(NodePtr start, NextFn next, OnCycleFn on_cycle) noexcept
        {
            auto slow{start};
            auto fast{start};
            for(;;)
            {
                if(slow == nullptr || fast == nullptr) { break; }

                slow = next(slow);
                fast = next(fast);
                if(fast != nullptr) { fast = next(fast); }

                if(slow == nullptr || fast == nullptr) { break; }
                if(slow == fast) [[unlikely]] { on_cycle(); }
            }
        }

        template <typename NodePtr, typename IsImportedFn, typename NextImportedFn, typename OnNullFn>
        inline constexpr NodePtr walk_import_alias_chain_leaf(NodePtr start, IsImportedFn is_imported, NextImportedFn next_imported, OnNullFn on_null) noexcept
        {
            auto curr{start};
            if(curr == nullptr) [[unlikely]]
            {
                on_null();
                return nullptr;
            }
            while(is_imported(curr))
            {
                auto const next{next_imported(curr)};
                if(next == nullptr) [[unlikely]]
                {
                    on_null();
                    return nullptr;
                }
                curr = next;
            }
            return curr;
        }

        template <typename NodePtr, typename IsImportedFn, typename NextImportedFn, typename IsConcreteFn, typename OnNullFn, typename OnNotConcreteFn>
        inline constexpr void require_import_alias_chain_resolves(NodePtr start,
                                                                  IsImportedFn is_imported,
                                                                  NextImportedFn next_imported,
                                                                  IsConcreteFn is_concrete,
                                                                  OnNullFn on_null,
                                                                  OnNotConcreteFn on_not_concrete) noexcept
        {
            auto const leaf{walk_import_alias_chain_leaf(start, is_imported, next_imported, on_null)};
            if(leaf == nullptr) [[unlikely]] { return; }
            if(!is_concrete(leaf)) [[unlikely]] { on_not_concrete(); }
        }

        inline constexpr ::uwvm2::utils::container::u8string_view module_type_to_string(::uwvm2::uwvm::wasm::type::module_type_t t) noexcept
        {
            /// @warning Extension point: new module_type_t enumerators need readable names here and initializer/linker dispatch support.
            switch(t)
            {
                case ::uwvm2::uwvm::wasm::type::module_type_t::exec_wasm:
                {
                    return u8"exec_wasm";
                }
                case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_wasm:
                {
                    return u8"preloaded_wasm";
                }
                case ::uwvm2::uwvm::wasm::type::module_type_t::local_import:
                {
                    return u8"local_import";
                }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_dl:
                {
                    return u8"preloaded_dl";
                }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                case ::uwvm2::uwvm::wasm::type::module_type_t::weak_symbol:
                {
                    return u8"weak_symbol";
                }
#endif
                [[unlikely]] default:
                {
                    return u8"unknown";
                }
            }
        }

        inline constexpr ::std::size_t importdesc_func_index{0uz};
        inline constexpr ::std::size_t importdesc_table_index{1uz};
        inline constexpr ::std::size_t importdesc_memory_index{2uz};
        inline constexpr ::std::size_t importdesc_global_index{3uz};
        inline constexpr ::std::size_t importdesc_tag_index{4uz};

        // this is an adl function
        template <initialization_purpose Purpose>
        inline constexpr ::uwvm2::object::global::global_type
            to_object_global_type_in_context(initialization_context<Purpose>& world, ::uwvm2::parser::wasm::standard::wasm1::type::value_type t /* [adl] */) noexcept
        {
            /// @note The parser stage already validated the module version/value type, so no version/feature checks are needed here.
            /// @warning A local-imported provider reports the byte value through this wasm1 enum even
            /// for a validated later-standard global. This cold projection must preserve its storage width;
            /// parser/validator feature gates remain authoritative.
            auto const carrier{static_cast<::std::uint_least8_t>(t)};
            if(carrier == 0x7bu) { return ::uwvm2::object::global::global_type::wasm_v128; }
            if(carrier == 0x70u || carrier == 0x6fu || carrier == 0x69u)
            { return ::uwvm2::object::global::global_type::wasm_ref; }
            switch(t)
            {
                case ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32:
                {
                    return ::uwvm2::object::global::global_type::wasm_i32;
                }
                case ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64:
                {
                    return ::uwvm2::object::global::global_type::wasm_i64;
                }
                case ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32:
                {
                    return ::uwvm2::object::global::global_type::wasm_f32;
                }
                case ::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64:
                {
                    return ::uwvm2::object::global::global_type::wasm_f64;
                }
                [[unlikely]] default:
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"initializer: Unsupported wasm value type for global in module \"",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        world.current_module(),
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"\": ",
                                        ::fast_io::mnp::hex0x<true>(static_cast<::std::uint_least32_t>(static_cast<::std::uint_least8_t>(t))),
                                        u8".\n\n",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }
        }

        inline constexpr ::uwvm2::object::global::global_type
            to_object_global_type(::uwvm2::parser::wasm::standard::wasm1::type::value_type t /* [adl] */) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return to_object_global_type_in_context(world, t);
        }

        template <initialization_purpose Purpose>
        inline constexpr ::uwvm2::object::global::global_type
            to_object_global_type_in_context(initialization_context<Purpose>& world, ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type t /* [adl] */) noexcept
        {
            /// @warning Extension point: new wasm1p1 value types that can initialize globals need object::global storage and local_imported support.
            if(static_cast<::std::uint_least8_t>(t) == 0x69u)
            { return ::uwvm2::object::global::global_type::wasm_ref; }
            switch(t)
            {
                case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::i32:
                {
                    return ::uwvm2::object::global::global_type::wasm_i32;
                }
                case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::i64:
                {
                    return ::uwvm2::object::global::global_type::wasm_i64;
                }
                case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::f32:
                {
                    return ::uwvm2::object::global::global_type::wasm_f32;
                }
                case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::f64:
                {
                    return ::uwvm2::object::global::global_type::wasm_f64;
                }
                case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128:
                {
                    return ::uwvm2::object::global::global_type::wasm_v128;
                }
                case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref: [[fallthrough]];
                case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::externref:
                {
                    // All reference carriers use complete 16-byte storage; the rich
                    // declaration retains each reference kind separately.
                    return ::uwvm2::object::global::global_type::wasm_ref;
                }
                [[unlikely]] default:
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"initializer: Unsupported wasm value type for global in module \"",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        world.current_module(),
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"\": ",
                                        ::fast_io::mnp::hex0x<true>(static_cast<::std::uint_least32_t>(static_cast<::std::uint_least8_t>(t))),
                                        u8".\n\n",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }
        }

        inline constexpr ::uwvm2::object::global::global_type
            to_object_global_type(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type t /* [adl] */) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return to_object_global_type_in_context(world, t);
        }

        // wasm1 const expr allows: i32/i64/f32/f64.const and global.get (only immutable imported globals).
        // Note: uwvm2 uses `std::uint_least64_t` for runtime offsets/addresses, so wasm1 `i32` offsets need widening conversion.
        // For wasm1 table/data offsets, the expression must evaluate to an i32, so we best-effort decode:
        // - i32.const
        // - global.get (only after import-linking, see `try_eval_wasm1_const_expr_offset_after_linking`)
        inline constexpr void try_eval_wasm1_const_expr_offset(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr /* [adl] */,
                                                               ::std::uint_least64_t& out) noexcept
        {
            if(expr.opcodes.size() > 1uz) { out = 0u; return; } // Finalized after linking, before segment bounds checks.
            if(expr.opcodes.size() != 1uz) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"initializer: wasm1.0 const expr must contain exactly one opcode; got ",
                                    expr.opcodes.size(),
                                    u8".\n\n",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            // size checked before, not empty
            auto const& op{expr.opcodes.front_unchecked()};
            if(op.opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const)
            {
                out = static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(op.storage.i32));
                return;
            }
            else if(op.opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get)
            {
                // wasm1.0 allows `global.get` (imported immutable globals only), but evaluation requires import-linking.
                // Keep a placeholder here; after import-linking + global finalization, segment application will evaluate the real value.
                out = 0u;
                return;
            }

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"initializer: Constant expression offset retrieval in wasm1.0 encountered an invalid instruction: ",
                                ::fast_io::mnp::hex0x<true>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(op.opcode)),
                                u8".\n\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            // terminate
            ::fast_io::fast_terminate();
        }

        template <initialization_purpose Purpose>
        inline void ensure_wasm1_local_defined_global_initialized_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t& g) noexcept;
        inline void ensure_wasm1_local_defined_global_initialized(::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t& g) noexcept;

        template <initialization_purpose Purpose>
        inline constexpr void try_resolve_wasm1_imported_global_value_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::imported_global_storage_t const* imported_global_ptr,
                                                                      ::uwvm2::object::global::wasm_global_storage_t const*& out,
                                                                      ::uwvm2::object::global::wasm_global_storage_t& local_imported_scratch) noexcept
        {
            using imported_global_ptr_t = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t const*;
            using global_link_kind = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t::imported_global_link_kind;

            auto const next_imported{[](imported_global_ptr_t curr) constexpr noexcept -> imported_global_ptr_t
                                     {
                                         if(curr == nullptr) { return nullptr; }
                                         if(curr->link_kind != global_link_kind::imported) { return nullptr; }
                                         return curr->target.imported_ptr;
                                     }};

            // Detect circular "import -> import -> ..." alias chains in imported globals.
            // These can happen via re-exported imports across modules.
            if(!world.alias_sanity())
            {
                check_no_import_alias_cycle_floyd(imported_global_ptr,
                                                  next_imported,
                                                  [&]() constexpr noexcept
                                                  {
                                                      auto const import_ptr{imported_global_ptr == nullptr ? nullptr : imported_global_ptr->import_type_ptr};
                                                      if(import_ptr == nullptr) [[unlikely]]
                                                      {
                        // vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                          ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                                          ::fast_io::fast_terminate();
                                                      }

                                                      ::fast_io::io::perr(
                                                          ::uwvm2::uwvm::io::u8log_output,
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                          u8"uwvm: ",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                          u8"[fatal] ",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                          u8"initializer: Global \"",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                          import_ptr->module_name,
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                          u8".",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                          import_ptr->extern_name,
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                          u8"\" encountered a circular dependency during initialization.\n\n",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                                      ::fast_io::fast_terminate();
                                                  });
            }

            auto curr{imported_global_ptr};
            for(;;)
            {
                if(curr == nullptr) [[unlikely]]
                {
                    // vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }

                if(curr->link_kind == global_link_kind::imported)
                {
                    auto const next{curr->target.imported_ptr};
                    if(next == nullptr) [[unlikely]]
                    {
                        if(curr->import_type_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"initializer: Unresolved imported global \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            curr->import_type_ptr->module_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8".",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            curr->import_type_ptr->extern_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\".\n\n",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    curr = next;
                    continue;
                }

                if(curr->link_kind == global_link_kind::local_imported)
                {
                    // Resolve leaf to a local-imported global (host global).
                    auto const idx{curr->target.local_imported.index};
                    auto const li{curr->target.local_imported.module_ptr};
                    if(li == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    // Fill into caller-provided scratch (no per-thread storage) and keep union access well-defined.
                    local_imported_scratch.kind = to_object_global_type_in_context(world, li->global_value_type_from_index(idx));
                    local_imported_scratch.is_mutable = li->global_is_mutable_from_index(idx);

                    switch(local_imported_scratch.kind)
                    {
                        case ::uwvm2::object::global::global_type::wasm_i32:
                        {
                            local_imported_scratch.storage.i32 = {};
                            li->global_get_from_index(idx, reinterpret_cast<::std::byte*>(::std::addressof(local_imported_scratch.storage.i32)));
                            break;
                        }
                        case ::uwvm2::object::global::global_type::wasm_i64:
                        {
                            local_imported_scratch.storage.i64 = {};
                            li->global_get_from_index(idx, reinterpret_cast<::std::byte*>(::std::addressof(local_imported_scratch.storage.i64)));
                            break;
                        }
                        case ::uwvm2::object::global::global_type::wasm_f32:
                        {
                            local_imported_scratch.storage.f32 = {};
                            li->global_get_from_index(idx, reinterpret_cast<::std::byte*>(::std::addressof(local_imported_scratch.storage.f32)));
                            break;
                        }
                        case ::uwvm2::object::global::global_type::wasm_f64:
                        {
                            local_imported_scratch.storage.f64 = {};
                            li->global_get_from_index(idx, reinterpret_cast<::std::byte*>(::std::addressof(local_imported_scratch.storage.f64)));
                            break;
                        }
                        case ::uwvm2::object::global::global_type::wasm_v128:
                        {
                            local_imported_scratch.storage.v128 = ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128{};
                            li->global_get_from_index(idx, reinterpret_cast<::std::byte*>(::std::addressof(local_imported_scratch.storage.v128)));
                            break;
                        }
                        case ::uwvm2::object::global::global_type::wasm_ref:
                        {
                            local_imported_scratch.storage.ref = {};
                            li->global_get_from_index(idx, reinterpret_cast<::std::byte*>(::std::addressof(local_imported_scratch.storage.ref)));
                            break;
                        }
                        [[unlikely]] default:
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }
                    }

                    out = ::std::addressof(local_imported_scratch);
                    return;
                }

                if(curr->link_kind != global_link_kind::defined) [[unlikely]]
                {
                    if(curr->import_type_ptr == nullptr) [[unlikely]]
                    {
                        // vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"initializer: Unresolved imported global \"",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        curr->import_type_ptr->module_name,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8".",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        curr->import_type_ptr->extern_name,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"\".\n\n",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }

                auto const def{curr->target.defined_ptr};
                if(def == nullptr) [[unlikely]]
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }

                ensure_wasm1_local_defined_global_initialized_in_context(world, *def);

                out = ::std::addressof(def->global);
                return;
            }

            // unreachable
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
            ::fast_io::fast_terminate();
        }

        inline constexpr void try_resolve_wasm1_imported_global_value(::uwvm2::uwvm::runtime::storage::imported_global_storage_t const* imported_global_ptr,
                                                                      ::uwvm2::object::global::wasm_global_storage_t const*& out,
                                                                      ::uwvm2::object::global::wasm_global_storage_t& local_imported_scratch) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            try_resolve_wasm1_imported_global_value_in_context(world, imported_global_ptr, out, local_imported_scratch);
        }

        inline constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 wasm_ref_null_funcidx_sentinel{
            (::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max)()};

        [[nodiscard]] inline constexpr ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t
            resolve_wasm1_funcref_index(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module,
                                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 function_index) noexcept
        {
            using table_elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
            auto const index{static_cast<::std::size_t>(function_index)};
            auto const imported_count{module.imported_function_vec_storage.size()};
            auto const local_count{module.local_defined_function_vec_storage.size()};
            if(local_count > (::std::numeric_limits<::std::size_t>::max() - imported_count)) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(index >= imported_count + local_count) [[unlikely]] { ::fast_io::fast_terminate(); }

            ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t result{};
            if(index < imported_count)
            {
                result.storage.imported_ptr = ::std::addressof(module.imported_function_vec_storage.index_unchecked(index));
                result.type = table_elem_type::func_ref_imported;
            }
            else
            {
                result.storage.defined_ptr = ::std::addressof(module.local_defined_function_vec_storage.index_unchecked(index - imported_count));
                result.type = table_elem_type::func_ref_defined;
            }
            return result;
        }

        inline constexpr void canonicalize_wasm1_funcref(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& owner,
                                                          ::uwvm2::object::global::wasm_global_ref_t& ref) noexcept
        {
            if(ref.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_func) { return; }

            auto const elem{resolve_wasm1_funcref_index(owner, ref.storage.func_idx)};
            using table_elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
            if(elem.type == table_elem_type::func_ref_imported)
            {
                ref.storage.ptr = const_cast<void*>(static_cast<void const*>(elem.storage.imported_ptr));
                ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_func_imported;
            }
            else
            {
                ref.storage.ptr = const_cast<void*>(static_cast<void const*>(elem.storage.defined_ptr));
                ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_func_defined;
            }
        }

        // Evaluate only parser-validated Core 3.0 constant expressions, after imports have been linked.
        // Host/global values remain typed; arithmetic never touches guest pointers or native call frames.
        template <initialization_purpose Purpose>
        [[nodiscard]] inline constexpr ::uwvm2::object::global::wasm_global_storage_t evaluate_wasm3_constant_expression_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& curr_rt) noexcept
        {
            using value = ::uwvm2::object::global::wasm_global_storage_t;
            using kind = ::uwvm2::object::global::global_type;
            using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
            using slot = ::uwvm2::uwvm::runtime::storage::gc_object_value;
            using reference = ::uwvm2::uwvm::runtime::storage::gc_reference;
            using gc_status = ::uwvm2::uwvm::runtime::storage::gc_object_status;
            ::uwvm2::utils::container::vector<value> stack{};
            auto const pop{[&]() noexcept -> value
            {
                if(stack.empty()) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto result{stack.back_unchecked()};
                stack.pop_back_unchecked();
                return result;
            }};
            auto const pop_i32{[&]() noexcept -> ::std::uint_least32_t
            {
                auto const item{pop()};
                if(item.kind != kind::wasm_i32) [[unlikely]] { ::fast_io::fast_terminate(); }
                return static_cast<::std::uint_least32_t>(item.storage.i32);
            }};
            auto const pop_ref{[&]() noexcept -> reference
            {
                auto const item{pop()};
                if(item.kind != kind::wasm_ref) [[unlikely]] { ::fast_io::fast_terminate(); }
                return item.storage.ref;
            }};
            auto const to_slot{[](value const& item) noexcept -> slot
            {
                slot result{};
                switch(item.kind)
                {
                    case kind::wasm_i32: ::std::memcpy(result.bits.data(), ::std::addressof(item.storage.i32), sizeof(item.storage.i32)); break;
                    case kind::wasm_i64: ::std::memcpy(result.bits.data(), ::std::addressof(item.storage.i64), sizeof(item.storage.i64)); break;
                    case kind::wasm_f32: ::std::memcpy(result.bits.data(), ::std::addressof(item.storage.f32), sizeof(item.storage.f32)); break;
                    case kind::wasm_f64: ::std::memcpy(result.bits.data(), ::std::addressof(item.storage.f64), sizeof(item.storage.f64)); break;
                    case kind::wasm_v128: ::std::memcpy(result.bits.data(), ::std::addressof(item.storage.v128), sizeof(item.storage.v128)); break;
                    case kind::wasm_ref: return slot::reference(item.storage.ref);
                    [[unlikely]] default: ::fast_io::fast_terminate();
                }
                return result;
            }};
            auto const make_ref{[](reference ref) noexcept -> value
            {
                value result{};
                result.kind = kind::wasm_ref;
                result.storage.ref = ref;
                return result;
            }};
            for(auto const& op: expr.opcodes)
            {
                auto const opcode{static_cast<::std::uint_least8_t>(op.opcode)};
                auto const width{::uwvm2::validation::standard::wasm3::constant_integer_width(opcode)};
                if(width != 0u)
                {
                    if(stack.size() < 2uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const rhs{stack.back_unchecked()};
                    stack.pop_back_unchecked();
                    auto& lhs{stack.back_unchecked()}; // Two entries checked before pop: one remains.
                    auto const expected{width == 32u ? kind::wasm_i32 : kind::wasm_i64};
                    if(lhs.kind != expected || rhs.kind != expected) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const left{width == 32u ? static_cast<::std::uint_least64_t>(lhs.storage.i32)
                                                : static_cast<::std::uint_least64_t>(lhs.storage.i64)};
                    auto const right{width == 32u ? static_cast<::std::uint_least64_t>(rhs.storage.i32)
                                                 : static_cast<::std::uint_least64_t>(rhs.storage.i64)};
                    auto const result{::uwvm2::validation::standard::wasm3::evaluate_constant_integer_binary(opcode, left, right)};
                    if(width == 32u) { lhs.storage.i32 = static_cast<decltype(lhs.storage.i32)>(static_cast<::std::uint_least32_t>(result)); }
                    else { lhs.storage.i64 = static_cast<decltype(lhs.storage.i64)>(result); }
                    continue;
                }
                value next{};
                switch(opcode)
                {
                    case 0x41: next.kind = kind::wasm_i32; next.storage.i32 = op.storage.i32; break;
                    case 0x42: next.kind = kind::wasm_i64; next.storage.i64 = op.storage.i64; break;
                    case 0x43:
                        next.kind = kind::wasm_f32;
                        ::std::memcpy(::std::addressof(next.storage.f32), ::std::addressof(op.storage.f32), sizeof(next.storage.f32));
                        break;
                    case 0x44:
                        next.kind = kind::wasm_f64;
                        ::std::memcpy(::std::addressof(next.storage.f64), ::std::addressof(op.storage.f64), sizeof(next.storage.f64));
                        break;
                    case 0xfd:
                        next.kind = kind::wasm_v128;
                        next.storage.v128 = op.storage.v128;
                        break;
                    case 0xd0:
                        next.kind = kind::wasm_ref;
                        next.storage.ref = {};
                        next.storage.ref.kind = ref_kind::wasm_null;
                        break;
                    case 0xd2:
                    {
                        auto const resolved{resolve_wasm1_funcref_index(curr_rt, op.storage.ref_func_idx)};
                        using table_kind = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
                        reference ref{};
                        if(resolved.type == table_kind::func_ref_imported)
                        { ref.kind = ref_kind::wasm_func_imported; ref.storage.ptr = const_cast<void*>(static_cast<void const*>(resolved.storage.imported_ptr)); }
                        else if(resolved.type == table_kind::func_ref_defined)
                        { ref.kind = ref_kind::wasm_func_defined; ref.storage.ptr = const_cast<void*>(static_cast<void const*>(resolved.storage.defined_ptr)); }
                        else [[unlikely]] { ::fast_io::fast_terminate(); }
                        next = make_ref(ref);
                        break;
                    }
                    case 0xfb:
                    {
                        auto* const store{curr_rt.gc_store.get()};
                        if(store == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                        auto const immediate{op.storage.gc_immediate};
                        auto const type_index{immediate.typeidx};
                        reference result{};
                        gc_status status{gc_status::invalid_value};
                        switch(immediate.subopcode)
                        {
                            case 28u:
                            {
                                // i32 is a bit pattern: copying avoids an implementation-defined
                                // narrowing conversion for values with bit 31 set.
                                auto const raw{static_cast<::std::uint32_t>(pop_i32())};
                                ::std::int32_t signed_value{};
                                ::std::memcpy(::std::addressof(signed_value), ::std::addressof(raw), sizeof(raw));
                                result = ::uwvm2::object::global::make_wasm_i31_reference(signed_value);
                                status = gc_status::ok;
                                break;
                            }
                            case 0u:
                            {
                                ::std::size_t count{};
                                if(!store->field_count(type_index, count) || count > stack.size()) [[unlikely]]
                                { ::fast_io::fast_terminate(); }
                                ::uwvm2::utils::container::vector<slot> inputs{};
                                inputs.resize(count);
                                for(::std::size_t i{count}; i != 0uz; --i)
                                { inputs.index_unchecked(i - 1uz) = to_slot(pop()); }
                                status = store->struct_new(type_index, inputs.data(), count, result);
                                break;
                            }
                            case 1u: status = store->struct_new_default(type_index, result); break;
                            case 6u:
                            {
                                auto const length{pop_i32()};
                                auto const input{to_slot(pop())};
                                status = store->array_new(type_index, input, length, result);
                                break;
                            }
                            case 7u: status = store->array_new_default(type_index, pop_i32(), result); break;
                            case 8u:
                            {
                                auto const count{static_cast<::std::size_t>(immediate.count)};
                                if(count > stack.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                                ::uwvm2::utils::container::vector<slot> inputs{};
                                inputs.resize(count);
                                for(::std::size_t i{count}; i != 0uz; --i)
                                { inputs.index_unchecked(i - 1uz) = to_slot(pop()); }
                                status = store->array_new_fixed(type_index, inputs.data(), count, result);
                                break;
                            }
                            case 26u: status = store->any_convert_extern(pop_ref(), result); break;
                            case 27u: status = store->extern_convert_any(pop_ref(), result); break;
                            [[unlikely]] default: ::fast_io::fast_terminate();
                        }
                        if(status != gc_status::ok) [[unlikely]] { ::fast_io::fast_terminate(); }
                        next = make_ref(result);
                        break;
                    }
                    case 0x23:
                    {
                        auto const idx{static_cast<::std::size_t>(op.storage.imported_global_idx)};
                        auto const imported_count{curr_rt.imported_global_vec_storage.size()};
                        value const* resolved{};
                        value scratch{};
                        if(idx < imported_count)
                        {
                            // [imported globals ... idx ... count) is a checked live vector.
                            //                       ^^ addressof selects a record, without pointer arithmetic.
                            try_resolve_wasm1_imported_global_value_in_context(world, 
                                ::std::addressof(curr_rt.imported_global_vec_storage.index_unchecked(idx)), resolved, scratch);
                            // Resolver either produces a live record/scratch address or fails; check before dereference.
                            if(resolved == nullptr || resolved->is_mutable) [[unlikely]] { ::fast_io::fast_terminate(); }
                            next = *resolved;
                        }
                        else
                        {
                            auto const local_idx{idx - imported_count};
                            if(local_idx >= curr_rt.local_defined_global_vec_storage.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                            auto& local{curr_rt.local_defined_global_vec_storage.index_unchecked(local_idx)};
                            ensure_wasm1_local_defined_global_initialized_in_context(world, local);
                            if(local.global.is_mutable) [[unlikely]] { ::fast_io::fast_terminate(); }
                            next = local.global;
                        }
                        break;
                    }
                    default: ::fast_io::fast_terminate();
                }
                stack.push_back(next);
            }
            if(stack.size() != 1uz) [[unlikely]] { ::fast_io::fast_terminate(); }
            return stack.front_unchecked();
        }

        [[nodiscard]] inline constexpr ::uwvm2::object::global::wasm_global_storage_t evaluate_wasm3_constant_expression(
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& curr_rt) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return evaluate_wasm3_constant_expression_in_context(world, expr, curr_rt);
        }

        [[nodiscard]] inline constexpr ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t
            table_elem_from_canonical_wasm1_funcref(::uwvm2::object::global::wasm_global_ref_t const& ref) noexcept
        {
            using table_elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
            ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t result{};
            switch(ref.kind)
            {
                case ::uwvm2::object::global::wasm_ref_kind::wasm_null: return result;
                case ::uwvm2::object::global::wasm_ref_kind::wasm_func_imported:
                {
                    result.storage.imported_ptr = static_cast<::uwvm2::uwvm::runtime::storage::imported_function_storage_t const*>(ref.storage.ptr);
                    if(result.storage.imported_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    result.type = table_elem_type::func_ref_imported;
                    return result;
                }
                case ::uwvm2::object::global::wasm_ref_kind::wasm_func_defined:
                {
                    result.storage.defined_ptr = static_cast<::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t const*>(ref.storage.ptr);
                    if(result.storage.defined_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    result.type = table_elem_type::func_ref_defined;
                    return result;
                }
                // A bare index has no owner once it crosses an imported-global boundary. Live globals are canonicalized after
                // linking; rejecting any remaining index prevents silently resolving it in the consumer module.
                case ::uwvm2::object::global::wasm_ref_kind::wasm_func: [[fallthrough]];
                case ::uwvm2::object::global::wasm_ref_kind::wasm_extern: [[fallthrough]];
                [[unlikely]] default: ::fast_io::fast_terminate();
            }
        }

        template <initialization_purpose Purpose>
        inline constexpr ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t
            eval_wasm1p1_ref_const_expr_as_funcref_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
                                                   ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& curr_rt) noexcept
        {
            if(expr.opcodes.size() != 1uz)
            {
                auto const evaluated{evaluate_wasm3_constant_expression_in_context(world, expr, curr_rt)};
                if(evaluated.kind != ::uwvm2::object::global::global_type::wasm_ref) [[unlikely]]
                { ::fast_io::fast_terminate(); }
                return table_elem_from_canonical_wasm1_funcref(evaluated.storage.ref);
            }
            if(expr.opcodes.size() != 1uz) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const& op{expr.opcodes.front_unchecked()};
            if(op.opcode == static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xD0u)) { return {}; }
            if(op.opcode == static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xD2u))
            {
                return resolve_wasm1_funcref_index(curr_rt, op.storage.ref_func_idx);
            }
            if(op.opcode != ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const idx{static_cast<::std::size_t>(op.storage.imported_global_idx)};
            auto const imported_global_count{curr_rt.imported_global_vec_storage.size()};
            ::uwvm2::object::global::wasm_global_storage_t const* resolved_global{};
            ::uwvm2::object::global::wasm_global_storage_t local_imported_scratch{};

            if(idx < imported_global_count)
            {
                try_resolve_wasm1_imported_global_value_in_context(world, ::std::addressof(curr_rt.imported_global_vec_storage.index_unchecked(idx)),
                                                        resolved_global,
                                                        local_imported_scratch);
            }
            else
            {
                auto const local_idx{idx - imported_global_count};
                if(local_idx >= curr_rt.local_defined_global_vec_storage.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto& local_global{curr_rt.local_defined_global_vec_storage.index_unchecked(local_idx)};
                ensure_wasm1_local_defined_global_initialized_in_context(world, local_global);
                resolved_global = ::std::addressof(local_global.global);
            }

            if(resolved_global == nullptr || resolved_global->kind != ::uwvm2::object::global::global_type::wasm_ref) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }

            switch(resolved_global->storage.ref.kind)
            {
                case ::uwvm2::object::global::wasm_ref_kind::wasm_null: [[fallthrough]];
                case ::uwvm2::object::global::wasm_ref_kind::wasm_func_imported: [[fallthrough]];
                case ::uwvm2::object::global::wasm_ref_kind::wasm_func_defined:
                    return table_elem_from_canonical_wasm1_funcref(resolved_global->storage.ref);
                case ::uwvm2::object::global::wasm_ref_kind::wasm_func: [[fallthrough]];
                case ::uwvm2::object::global::wasm_ref_kind::wasm_extern: [[fallthrough]];
                [[unlikely]] default: ::fast_io::fast_terminate();
            }
        }

        inline constexpr ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t
            eval_wasm1p1_ref_const_expr_as_funcref(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
                                                   ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& curr_rt) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return eval_wasm1p1_ref_const_expr_as_funcref_in_context(world, expr, curr_rt);
        }

        template <initialization_purpose Purpose>
        inline constexpr void*
            eval_wasm2_ref_const_expr_as_externref_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
                                                   ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& curr_rt) noexcept
        {
            if(expr.opcodes.size() != 1uz)
            {
                auto const evaluated{evaluate_wasm3_constant_expression_in_context(world, expr, curr_rt)};
                if(evaluated.kind != ::uwvm2::object::global::global_type::wasm_ref) [[unlikely]]
                { ::fast_io::fast_terminate(); }
                switch(evaluated.storage.ref.kind)
                {
                    case ::uwvm2::object::global::wasm_ref_kind::wasm_null: return nullptr;
                    case ::uwvm2::object::global::wasm_ref_kind::wasm_extern: return evaluated.storage.ref.storage.ptr;
                    [[unlikely]] default: ::fast_io::fast_terminate();
                }
            }
            if(expr.opcodes.size() != 1uz) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const& op{expr.opcodes.front_unchecked()};
            if(op.opcode == static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xD0u)) { return nullptr; }
            if(op.opcode != ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const idx{static_cast<::std::size_t>(op.storage.imported_global_idx)};
            auto const imported_global_count{curr_rt.imported_global_vec_storage.size()};
            ::uwvm2::object::global::wasm_global_storage_t const* resolved_global{};
            ::uwvm2::object::global::wasm_global_storage_t local_imported_scratch{};

            if(idx < imported_global_count)
            {
                try_resolve_wasm1_imported_global_value_in_context(world, ::std::addressof(curr_rt.imported_global_vec_storage.index_unchecked(idx)),
                                                        resolved_global,
                                                        local_imported_scratch);
            }
            else
            {
                auto const local_idx{idx - imported_global_count};
                if(local_idx >= curr_rt.local_defined_global_vec_storage.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto& local_global{curr_rt.local_defined_global_vec_storage.index_unchecked(local_idx)};
                ensure_wasm1_local_defined_global_initialized_in_context(world, local_global);
                resolved_global = ::std::addressof(local_global.global);
            }

            if(resolved_global == nullptr || resolved_global->kind != ::uwvm2::object::global::global_type::wasm_ref) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }

            switch(resolved_global->storage.ref.kind)
            {
                case ::uwvm2::object::global::wasm_ref_kind::wasm_null: return nullptr;
                case ::uwvm2::object::global::wasm_ref_kind::wasm_extern: return resolved_global->storage.ref.storage.ptr;
                [[unlikely]] default: ::fast_io::fast_terminate();
            }
        }

        inline constexpr void*
            eval_wasm2_ref_const_expr_as_externref(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
                                                   ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& curr_rt) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return eval_wasm2_ref_const_expr_as_externref_in_context(world, expr, curr_rt);
        }

        // A table slot stores only the opaque exn token. The originating store retains its
        // exception value; no guest-controlled token is ever dereferenced here.
        template <initialization_purpose Purpose>
        inline constexpr void*
            eval_wasm3_ref_const_expr_as_exnref_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
                                                ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& curr_rt) noexcept
        {
            if(expr.opcodes.size() != 1uz)
            {
                auto const evaluated{evaluate_wasm3_constant_expression_in_context(world, expr, curr_rt)};
                if(evaluated.kind != ::uwvm2::object::global::global_type::wasm_ref) [[unlikely]]
                { ::fast_io::fast_terminate(); }
                switch(evaluated.storage.ref.kind)
                {
                    case ::uwvm2::object::global::wasm_ref_kind::wasm_null: return nullptr;
                    case ::uwvm2::object::global::wasm_ref_kind::wasm_exn: return evaluated.storage.ref.storage.ptr;
                    [[unlikely]] default: ::fast_io::fast_terminate();
                }
            }
            if(expr.opcodes.size() != 1uz) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& op{expr.opcodes.front_unchecked()};
            if(op.opcode == static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xD0u))
            {
                using heap = ::uwvm2::parser::wasm::standard::wasm3::type::abstract_heap_type;
                auto const code{op.storage.ref_null_heap};
                if(code != static_cast<::std::int_least64_t>(heap::exn) &&
                   code != static_cast<::std::int_least64_t>(heap::noexn)) [[unlikely]]
                { ::fast_io::fast_terminate(); }
                return nullptr;
            }
            if(op.opcode != ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const idx{static_cast<::std::size_t>(op.storage.imported_global_idx)};
            auto const imported_global_count{curr_rt.imported_global_vec_storage.size()};
            ::uwvm2::object::global::wasm_global_storage_t const* resolved_global{};
            ::uwvm2::object::global::wasm_global_storage_t local_imported_scratch{};
            if(idx < imported_global_count)
            {
                try_resolve_wasm1_imported_global_value_in_context(world, ::std::addressof(curr_rt.imported_global_vec_storage.index_unchecked(idx)),
                                                        resolved_global, local_imported_scratch);
            }
            else
            {
                auto const local_idx{idx - imported_global_count};
                if(local_idx >= curr_rt.local_defined_global_vec_storage.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto& local_global{curr_rt.local_defined_global_vec_storage.index_unchecked(local_idx)};
                ensure_wasm1_local_defined_global_initialized_in_context(world, local_global);
                resolved_global = ::std::addressof(local_global.global);
            }
            if(resolved_global == nullptr || resolved_global->kind != ::uwvm2::object::global::global_type::wasm_ref) [[unlikely]]
            { ::fast_io::fast_terminate(); }
            switch(resolved_global->storage.ref.kind)
            {
                case ::uwvm2::object::global::wasm_ref_kind::wasm_null: return nullptr;
                case ::uwvm2::object::global::wasm_ref_kind::wasm_exn: return resolved_global->storage.ref.storage.ptr;
                [[unlikely]] default: ::fast_io::fast_terminate();
            }
        }

        inline constexpr void*
            eval_wasm3_ref_const_expr_as_exnref(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
                                                ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& curr_rt) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return eval_wasm3_ref_const_expr_as_exnref_in_context(world, expr, curr_rt);
        }

        // Imported references must be linked and canonical before filling a table. Evaluate exactly once, including
        // a zero-sized table, then broadcast the reference; active element segments overwrite this initial value later.
        template <initialization_purpose Purpose>
        inline constexpr void initialize_wasm3_table_expressions_after_linking_in_context(initialization_context<Purpose>& world) noexcept
        {
            using family = ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family;
            using element_kind = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
            for(auto& [module_name, curr_rt]: world.modules())
            {
                for(auto& table: curr_rt.local_defined_table_vec_storage)
                {
                    // initializer_expr borrows live parser-owned storage. Legacy table definitions have no expression.
                    if(table.initializer_expr == nullptr || table.initializer_expr->opcodes.empty()) { continue; }
                    if(table.table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t value{};
                    auto const reference_family{::uwvm2::uwvm::runtime::storage::runtime_table_family(table)};
                    if(reference_family == family::function)
                    {
                        value = eval_wasm1p1_ref_const_expr_as_funcref_in_context(world, *table.initializer_expr, curr_rt);
                    }
                    else if(reference_family == family::external)
                    {
                        // The resolver returns either null or a live opaque host reference; Wasm never dereferences it.
                        value.storage.extern_ptr = eval_wasm2_ref_const_expr_as_externref_in_context(world, *table.initializer_expr, curr_rt);
                        value.type = element_kind::extern_ref;
                        // The initialized table owns this reference after publication. In particular, an imported
                        // global may supply a GC bridge whose originating module is independently unloadable.
                        if(!table.elems.empty() &&
                           ::uwvm2::uwvm::runtime::storage::retain_runtime_table_extern_payload(
                               ::std::addressof(table), value.storage.extern_ptr) !=
                               ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
                        { ::fast_io::fast_terminate(); }
                    }
                    else if(reference_family == family::exception)
                    {
                        value.storage.extern_ptr = eval_wasm3_ref_const_expr_as_exnref_in_context(world, *table.initializer_expr, curr_rt);
                        value.type = element_kind::exn_ref;
                        if(!table.elems.empty() &&
                           ::uwvm2::uwvm::runtime::storage::retain_runtime_table_exn_payload(
                               ::std::addressof(table), value.storage.extern_ptr) !=
                               ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
                        { ::fast_io::fast_terminate(); }
                    }
                    else if(reference_family == family::gc)
                    {
                        auto const evaluated{evaluate_wasm3_constant_expression_in_context(world, *table.initializer_expr, curr_rt)};
                        if(evaluated.kind != ::uwvm2::object::global::global_type::wasm_ref) [[unlikely]]
                        { ::fast_io::fast_terminate(); }
                        value = ::uwvm2::uwvm::runtime::storage::runtime_table_slot_from_gc_reference(evaluated.storage.ref);
                        if(!table.elems.empty() &&
                           ::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(
                               ::std::addressof(table), evaluated.storage.ref) !=
                               ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
                        { ::fast_io::fast_terminate(); }
                    }
                    else [[unlikely]] { ::fast_io::fast_terminate(); }
                    for(auto& element: table.elems) { element = value; }
                }
            }
        }

        inline constexpr void initialize_wasm3_table_expressions_after_linking() noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            initialize_wasm3_table_expressions_after_linking_in_context(world);
        }

        template <initialization_purpose Purpose>
        inline constexpr void materialize_wasm1p1_element_expr_payloads_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& curr_rt) noexcept
        {
            using family = ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family;
            ::std::size_t total_funcref_expr_count{};
            ::std::size_t total_externref_expr_count{};
            ::std::size_t total_gc_ref_expr_count{};
            for(auto const& elem_seg: curr_rt.local_defined_element_vec_storage)
            {
                if(elem_seg.element_type_ptr == nullptr || ::uwvm2::uwvm::runtime::storage::wasm_element_segment_is_dropped(elem_seg.element)) [[unlikely]] { continue; }
                auto const curr_expr_count{elem_seg.element_type_ptr->storage.segment.vec_expr.size()};
                auto const reference_family{::uwvm2::uwvm::runtime::storage::runtime_element_family(elem_seg, curr_rt)};
                auto& total_count{reference_family == family::function ? total_funcref_expr_count :
                                  ((reference_family == family::external || reference_family == family::exception) ?
                                   total_externref_expr_count : total_gc_ref_expr_count)};
                if(curr_expr_count > ::std::numeric_limits<::std::size_t>::max() - total_count) [[unlikely]] { ::fast_io::fast_terminate(); }
                total_count += curr_expr_count;
            }

            auto& owned_funcref_storage{curr_rt.element_expr_funcref_vec_storage};
            owned_funcref_storage.clear();
            owned_funcref_storage.reserve(total_funcref_expr_count);
            auto& owned_externref_storage{curr_rt.element_expr_externref_vec_storage};
            owned_externref_storage.clear();
            owned_externref_storage.reserve(total_externref_expr_count);
            auto& owned_gc_ref_storage{curr_rt.element_expr_gc_ref_vec_storage};
            owned_gc_ref_storage.clear();
            owned_gc_ref_storage.reserve(total_gc_ref_expr_count);

            for(auto& elem_seg: curr_rt.local_defined_element_vec_storage)
            {
                if(elem_seg.element_type_ptr == nullptr || ::uwvm2::uwvm::runtime::storage::wasm_element_segment_is_dropped(elem_seg.element)) [[unlikely]] { continue; }
                auto const& segment{elem_seg.element_type_ptr->storage.segment};
                auto const& exprs{segment.vec_expr};
                if(exprs.empty()) { continue; }

                auto const reference_family{::uwvm2::uwvm::runtime::storage::runtime_element_family(elem_seg, curr_rt)};
                if(reference_family == family::function)
                {
                    auto const begin_index{owned_funcref_storage.size()};
                    for(auto const& expr: exprs) { owned_funcref_storage.push_back_unchecked(eval_wasm1p1_ref_const_expr_as_funcref_in_context(world, expr, curr_rt)); }

                    auto const* const base{owned_funcref_storage.data()};
                    elem_seg.element.funcidx_begin = nullptr;
                    elem_seg.element.funcidx_end = nullptr;
                    elem_seg.element.funcref_begin = base + begin_index;
                    elem_seg.element.funcref_end = elem_seg.element.funcref_begin + exprs.size();
                }
                else if(reference_family == family::external || reference_family == family::exception)
                {
                    auto const begin_index{owned_externref_storage.size()};
                    for(auto const& expr: exprs)
                    {
                        auto const exnref_segment{reference_family == family::exception};
                        auto const payload{exnref_segment ? eval_wasm3_ref_const_expr_as_exnref_in_context(world, expr, curr_rt)
                                                          : eval_wasm2_ref_const_expr_as_externref_in_context(world, expr, curr_rt)};
                        ::uwvm2::uwvm::runtime::storage::gc_reference reference{};
                        reference.storage.ptr = payload;
                        reference.kind = payload == nullptr ? ::uwvm2::object::global::wasm_ref_kind::wasm_null :
                            (exnref_segment ? ::uwvm2::object::global::wasm_ref_kind::wasm_exn :
                                              ::uwvm2::object::global::wasm_ref_kind::wasm_extern);
                        // A passive segment is a module-owned root even before table.init copies it. Retain before
                        // publishing the payload; the checked API treats token-shaped values as keys, never pointers.
                        if(::uwvm2::uwvm::runtime::storage::uwvm2_gc_retain_reference(
                               curr_rt.gc_store.get(), ::std::addressof(reference)) !=
                               ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
                        { ::fast_io::fast_terminate(); }
                        // [reserved vector slots ... current] end
                        // [safe                          ]
                        // ^^ reserve(total_externref_expr_count) above proves this append cannot relocate an
                        // already-published segment range, and total_count was checked against size_t overflow.
                        owned_externref_storage.push_back_unchecked(payload);
                    }

                    auto const* const base{owned_externref_storage.data()};
                    elem_seg.element.externref_begin = base + begin_index;
                    elem_seg.element.externref_end = elem_seg.element.externref_begin + exprs.size();
                }
                else if(reference_family == family::gc)
                {
                    auto const begin_index{owned_gc_ref_storage.size()};
                    for(auto const& expr: exprs)
                    {
                        auto const evaluated{evaluate_wasm3_constant_expression_in_context(world, expr, curr_rt)};
                        if(evaluated.kind != ::uwvm2::object::global::global_type::wasm_ref) [[unlikely]]
                        { ::fast_io::fast_terminate(); }
                        auto const reference{evaluated.storage.ref};
                        if(::uwvm2::uwvm::runtime::storage::uwvm2_gc_retain_reference(
                               curr_rt.gc_store.get(), ::std::addressof(reference)) !=
                               ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
                        { ::fast_io::fast_terminate(); }
                        // [reserved GC reference slots ... current] end
                        // [safe                                ]
                        // ^^ reserve(total_gc_ref_expr_count) above prevents relocation of published ranges.
                        owned_gc_ref_storage.push_back_unchecked(reference);
                    }
                    auto const* const base{owned_gc_ref_storage.data()};
                    // [owned GC reference vector: begin ... begin_index ... exprs.size()] end
                    // [safe                                                    ]
                    // ^^ begin_index and exprs.size() are bounded by the reserved total; both pointers
                    //    borrow the module-owned vector until instance retirement.
                    elem_seg.element.gc_ref_begin = base + begin_index;
                    elem_seg.element.gc_ref_end = elem_seg.element.gc_ref_begin + exprs.size();
                }
                else [[unlikely]] { ::fast_io::fast_terminate(); }
            }
        }

        inline constexpr void materialize_wasm1p1_element_expr_payloads(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& curr_rt) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            materialize_wasm1p1_element_expr_payloads_in_context(world, curr_rt);
        }

        template <initialization_purpose Purpose>
        inline constexpr void try_resolve_wasm1_imported_global_i32_value_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::imported_global_storage_t const* imported_global_ptr,
                                                                          ::std::uint_least64_t& out) noexcept
        {
            using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;

            if(imported_global_ptr == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            if(imported_global_ptr->import_type_ptr == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            if(imported_global_ptr->import_type_ptr->imports.type != external_types::global) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            // wasm1.0: offsets can only read imported *immutable* globals via `global.get`.
            if(imported_global_ptr->import_type_ptr->imports.storage.global.is_mutable) [[unlikely]]
            {
                ::fast_io::io::perr(
                    ::uwvm2::uwvm::io::u8log_output,
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                    u8"uwvm: ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                    u8"[fatal] ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                    u8"initializer: In wasm1.0, constant expressions may only use `global.get` on imported immutable globals; got mutable global \"",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                    imported_global_ptr->import_type_ptr->module_name,
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                    u8".",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                    imported_global_ptr->import_type_ptr->extern_name,
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                    u8"\".\n\n",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            ::uwvm2::object::global::wasm_global_storage_t const* resolved_global{};
            ::uwvm2::object::global::wasm_global_storage_t local_imported_scratch{};
            try_resolve_wasm1_imported_global_value_in_context(world, imported_global_ptr, resolved_global, local_imported_scratch);

            if(resolved_global == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            if(resolved_global->kind != ::uwvm2::object::global::global_type::wasm_i32) [[unlikely]]
            {
                ::fast_io::io::perr(
                    ::uwvm2::uwvm::io::u8log_output,
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                    u8"uwvm: ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                    u8"[fatal] ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                    u8"initializer: In wasm1.0, constant expressions retrieve offsets from imported globals, where the global type is not i32: ",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                    ::uwvm2::object::global::get_global_type_name(resolved_global->kind),
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                    u8".\n\n",
                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            out = static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(resolved_global->storage.i32));
            return;
        }

        inline constexpr void try_resolve_wasm1_imported_global_i32_value(::uwvm2::uwvm::runtime::storage::imported_global_storage_t const* imported_global_ptr,
                                                                          ::std::uint_least64_t& out) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            try_resolve_wasm1_imported_global_i32_value_in_context(world, imported_global_ptr, out);
        }

        template <initialization_purpose Purpose>
        inline constexpr bool maybe_resolve_wasm1_imported_table_defined_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::imported_table_storage_t const* imported_table_ptr,
                                                                         ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t*& out) noexcept
        {
            using imported_table_ptr_t = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t const*;
            using table_link_kind = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;

            auto const next_imported{[](imported_table_ptr_t curr) constexpr noexcept -> imported_table_ptr_t
                                     {
                                         if(curr == nullptr) { return nullptr; }
                                         if(curr->link_kind != table_link_kind::imported) { return nullptr; }
                                         return curr->target.imported_ptr;
                                     }};

            // Detect circular "import -> import -> ..." alias chains in imported tables.
            // These can happen via re-exported imports across modules.
            if(!world.alias_sanity())
            {
                check_no_import_alias_cycle_floyd(imported_table_ptr,
                                                  next_imported,
                                                  [&]() constexpr noexcept
                                                  {
                                                      auto const import_ptr{imported_table_ptr == nullptr ? nullptr : imported_table_ptr->import_type_ptr};
                                                      if(import_ptr == nullptr) [[unlikely]]
                                                      {
                        // vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                          ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                                          ::fast_io::fast_terminate();
                                                      }

                                                      ::fast_io::io::perr(
                                                          ::uwvm2::uwvm::io::u8log_output,
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                          u8"uwvm: ",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                          u8"[fatal] ",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                          u8"initializer: Table \"",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                          import_ptr->module_name,
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                          u8".",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                          import_ptr->extern_name,
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                          u8"\" encountered a circular dependency during import resolution.\n\n",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                                      ::fast_io::fast_terminate();
                                                  });
            }

            auto curr{imported_table_ptr};
            for(;;)
            {
                if(curr == nullptr) [[unlikely]]
                {
                    // vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }

                if(curr->link_kind == table_link_kind::imported)
                {
                    auto const next{curr->target.imported_ptr};
                    if(next == nullptr) [[unlikely]] { return false; }
                    curr = next;
                    continue;
                }

                if(curr->link_kind != table_link_kind::defined) [[unlikely]] { return false; }

                auto def{curr->target.defined_ptr};
                if(def == nullptr) [[unlikely]] { return false; }

                out = def;
                return true;
            }
        }

        inline constexpr bool maybe_resolve_wasm1_imported_table_defined(::uwvm2::uwvm::runtime::storage::imported_table_storage_t const* imported_table_ptr,
                                                                         ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t*& out) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return maybe_resolve_wasm1_imported_table_defined_in_context(world, imported_table_ptr, out);
        }

        template <initialization_purpose Purpose>
        inline constexpr bool maybe_resolve_wasm1_imported_memory_defined_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t const* imported_memory_ptr,
                                                                          ::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t*& out) noexcept
        {
            using imported_memory_ptr_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t const*;
            using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;

            auto const next_imported{[](imported_memory_ptr_t curr) constexpr noexcept -> imported_memory_ptr_t
                                     {
                                         if(curr == nullptr) { return nullptr; }
                                         if(curr->link_kind != memory_link_kind::imported) { return nullptr; }
                                         return curr->target.imported_ptr;
                                     }};

            // Detect circular "import -> import -> ..." alias chains in imported memories.
            // These can happen via re-exported imports across modules.
            if(!world.alias_sanity())
            {
                check_no_import_alias_cycle_floyd(imported_memory_ptr,
                                                  next_imported,
                                                  [&]() constexpr noexcept
                                                  {
                                                      auto const import_ptr{imported_memory_ptr == nullptr ? nullptr : imported_memory_ptr->import_type_ptr};
                                                      if(import_ptr == nullptr) [[unlikely]]
                                                      {
                        // vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                          ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                                          ::fast_io::fast_terminate();
                                                      }

                                                      ::fast_io::io::perr(
                                                          ::uwvm2::uwvm::io::u8log_output,
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                          u8"uwvm: ",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                          u8"[fatal] ",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                          u8"initializer: Memory \"",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                          import_ptr->module_name,
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                          u8".",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                          import_ptr->extern_name,
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                          u8"\" encountered a circular dependency during import resolution.\n\n",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                                      ::fast_io::fast_terminate();
                                                  });
            }

            auto curr{imported_memory_ptr};
            for(;;)
            {
                if(curr == nullptr) [[unlikely]]
                {
                    // vm bug
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }

                if(curr->link_kind == memory_link_kind::imported)
                {
                    auto const next{curr->target.imported_ptr};
                    if(next == nullptr) [[unlikely]] { return false; }
                    curr = next;
                    continue;
                }

                if(curr->link_kind != memory_link_kind::defined) [[unlikely]] { return false; }

                auto def{curr->target.defined_ptr};
                if(def == nullptr) [[unlikely]] { return false; }

                out = def;
                return true;
            }
        }

        inline constexpr bool maybe_resolve_wasm1_imported_memory_defined(::uwvm2::uwvm::runtime::storage::imported_memory_storage_t const* imported_memory_ptr,
                                                                          ::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t*& out) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return maybe_resolve_wasm1_imported_memory_defined_in_context(world, imported_memory_ptr, out);
        }

        struct wasm1_resolved_imported_memory_t
        {
            ::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t* defined_ptr{};
            ::uwvm2::uwvm::wasm::type::local_imported_t* local_imported_ptr{};
            ::std::size_t local_imported_index{};
        };

        template <initialization_purpose Purpose>
        inline constexpr bool maybe_resolve_wasm1_imported_memory_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t const* imported_memory_ptr,
                                                                  wasm1_resolved_imported_memory_t& out) noexcept
        {
            using imported_memory_ptr_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t const*;
            using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;

            auto const next_imported{[](imported_memory_ptr_t curr) constexpr noexcept -> imported_memory_ptr_t
                                     {
                                         if(curr == nullptr) { return nullptr; }
                                         if(curr->link_kind != memory_link_kind::imported) { return nullptr; }
                                         return curr->target.imported_ptr;
                                     }};

            // Detect circular "import -> import -> ..." alias chains in imported memories.
            // These can happen via re-exported imports across modules.
            if(!world.alias_sanity())
            {
                check_no_import_alias_cycle_floyd(imported_memory_ptr,
                                                  next_imported,
                                                  [&]() constexpr noexcept
                                                  {
                                                      auto const import_ptr{imported_memory_ptr == nullptr ? nullptr : imported_memory_ptr->import_type_ptr};
                                                      if(import_ptr == nullptr) [[unlikely]]
                                                      {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                          ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                                          ::fast_io::fast_terminate();
                                                      }

                                                      ::fast_io::io::perr(
                                                          ::uwvm2::uwvm::io::u8log_output,
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                          u8"uwvm: ",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                          u8"[fatal] ",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                          u8"initializer: Memory \"",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                          import_ptr->module_name,
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                          u8".",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                          import_ptr->extern_name,
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                          u8"\" encountered a circular dependency during import resolution.\n\n",
                                                          ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                                      ::fast_io::fast_terminate();
                                                  });
            }

            auto curr{imported_memory_ptr};
            for(;;)
            {
                if(curr == nullptr) [[unlikely]]
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }

                if(curr->link_kind == memory_link_kind::imported)
                {
                    auto const next{curr->target.imported_ptr};
                    if(next == nullptr) [[unlikely]] { return false; }
                    curr = next;
                    continue;
                }

                if(curr->link_kind == memory_link_kind::defined)
                {
                    out.defined_ptr = curr->target.defined_ptr;
                    out.local_imported_ptr = nullptr;
                    out.local_imported_index = 0uz;
                    return true;
                }
                else if(curr->link_kind == memory_link_kind::local_imported)
                {
                    out.defined_ptr = nullptr;
                    out.local_imported_ptr = curr->target.local_imported.module_ptr;
                    out.local_imported_index = curr->target.local_imported.index;
                    return true;
                }

                return false;
            }
        }

        inline constexpr bool maybe_resolve_wasm1_imported_memory(::uwvm2::uwvm::runtime::storage::imported_memory_storage_t const* imported_memory_ptr,
                                                                  wasm1_resolved_imported_memory_t& out) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return maybe_resolve_wasm1_imported_memory_in_context(world, imported_memory_ptr, out);
        }

        template<typename MemoryType>
        [[nodiscard]] inline constexpr bool wasm_memory_is_shared(MemoryType const& memory) noexcept
        {
            if constexpr(requires { memory.shared; }) { return memory.shared; }
            else { return false; } // MVP-only feature packs have no shared-memory type.
        }

        template<typename MemoryType>
        [[nodiscard]] inline constexpr bool wasm_memory_is_address64(MemoryType const& memory) noexcept
        {
            if constexpr(requires { memory.address64; }) { return memory.address64; }
            else { return false; }
        }

        template<typename Memory>
        inline constexpr void initialize_native_memory(Memory& memory, auto const& limits, ::std::size_t instance_index,
                                                       bool shared, bool address64) noexcept
        {
            if constexpr(requires { memory.sequentially_consistent_size; })
            { memory.sequentially_consistent_size = shared; }
            else if(shared) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                    u8"uwvm: [fatal] initializer: shared memory requires a concurrent memory backend.\n\n");
                ::fast_io::fast_terminate();
            }
            if constexpr(Memory::can_mmap)
            {
                // Select the address domain before reserving or committing memory. Memory32
                // retains its full hardware protection; memory64 uses the existing partial map.
                memory.status = address64 ? decltype(memory.status)::wasm64 : decltype(memory.status)::wasm32;
                memory.init_by_page_count(limits.min,
                    limits.present_max ? limits.max : ::std::numeric_limits<::std::size_t>::max(), instance_index);
            }
            else { memory.init_by_page_count(limits.min, instance_index); }
        }

        using ::uwvm2::uwvm::runtime::storage::runtime_memory_is_address64;
        using ::uwvm2::uwvm::runtime::storage::runtime_table_is_address64;

        inline constexpr bool wasm1_limits_match(auto const& expected,
                                                 auto const& actual) noexcept
        {
            if(actual.min < expected.min) { return false; }

            if(expected.present_max)
            {
                if(!actual.present_max) { return false; }
                if(actual.max > expected.max) { return false; }
            }

            return true;
        }

        template <typename ValueType>
        inline constexpr ::std::size_t safe_ptr_range_size(ValueType const* begin, ValueType const* end) noexcept
        {
            // For many external dl/weak_symbols, provide checks.

            if(begin == end) { return 0uz; }
            if(begin == nullptr || end == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            // NOTE: `end - begin` is UB unless both pointers refer into the same array object.
            // The runtime may run with a partially-degraded parser; fail-safe instead of triggering UB.
            auto const begin_u{reinterpret_cast<::std::uintptr_t>(begin)};
            auto const end_u{reinterpret_cast<::std::uintptr_t>(end)};

            if(end_u < begin_u) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            auto const diff_bytes{end_u - begin_u};
            if((diff_bytes % sizeof(ValueType)) != 0uz) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            auto const count{diff_bytes / sizeof(ValueType)};
            if(count > ::std::numeric_limits<::std::size_t>::max()) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }
            return static_cast<::std::size_t>(count);
        }

        // Link-time only: find the module-owned type index behind a borrowed parser
        // signature. Equality comparisons below never trust an import's projected
        // carrier without first recovering any available Core 3 rich declaration.
        struct linked_function_type_location
        {
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* module{};
            ::std::size_t index{};
        };
        template <initialization_purpose Purpose>
        [[nodiscard]] inline linked_function_type_location locate_linked_function_type_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* pointer,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* owner = nullptr) noexcept
        {
            if(pointer == nullptr) { return {}; }
            for(auto const& [name, module]: world.modules())
            {
                static_cast<void>(name);
                // [live registry element] an optional owner is compared as an address only.
                // ^^ select the actual instance even when two modules share parsed type storage.
                if(owner != nullptr && owner != ::std::addressof(module)) { continue; }
                auto const& types{module.type_section_storage};
                if(types.type_section_begin == nullptr || types.type_section_end == nullptr) { continue; }
                auto const count{safe_ptr_range_size(types.type_section_begin, types.type_section_end)};
                auto const candidate_address{reinterpret_cast<::std::uintptr_t>(pointer)};
                auto const begin_address{reinterpret_cast<::std::uintptr_t>(types.type_section_begin)};
                if(candidate_address < begin_address) { continue; }
                auto const displacement{candidate_address - begin_address};
                if(displacement % sizeof(*pointer) != 0uz) { continue; }
                auto const index{displacement / sizeof(*pointer)};
                if(index >= count) { continue; }
                // [type_section_begin, type_section_end) is parser-owned until module retirement.
                // [safe                                 ] unsafe (one-past)
                // ^^ begin[index]: index < count and alignment were checked using integers;
                //    the candidate was never dereferenced or subtracted as a pointer.
                if(::std::addressof(types.type_section_begin[static_cast<::std::size_t>(index)]) == pointer)
                { return {::std::addressof(module), static_cast<::std::size_t>(index)}; }
            }
            return {};
        }

        [[nodiscard]] inline linked_function_type_location locate_linked_function_type(
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* pointer,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* owner = nullptr) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return locate_linked_function_type_in_context(world, pointer, owner);
        }

        template <initialization_purpose Purpose>
        inline bool wasm1_function_type_equal_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* expected,
                                             ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* actual) noexcept
        {
            if(expected == actual) { return true; }
            if(expected == nullptr || actual == nullptr) { return false; }

            auto const expected_para_len{safe_ptr_range_size(expected->parameter.begin, expected->parameter.end)};
            auto const actual_para_len{safe_ptr_range_size(actual->parameter.begin, actual->parameter.end)};
            if(expected_para_len != actual_para_len) { return false; }

            for(::std::size_t i{}; i != expected_para_len; ++i)
            {
                if(expected->parameter.begin[i] != actual->parameter.begin[i]) { return false; }
            }

            auto const expected_res_len{safe_ptr_range_size(expected->result.begin, expected->result.end)};
            auto const actual_res_len{safe_ptr_range_size(actual->result.begin, actual->result.end)};
            if(expected_res_len != actual_res_len) { return false; }

            for(::std::size_t i{}; i != expected_res_len; ++i)
            {
                if(expected->result.begin[i] != actual->result.begin[i]) { return false; }
            }

            using owned = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t;
            using view = ::uwvm2::validation::standard::wasm3::core3_signature_view<owned>;
            auto const expected_owner{locate_linked_function_type_in_context(world, expected)};
            auto const actual_owner{locate_linked_function_type_in_context(world, actual)};
            auto const rich_view{[](linked_function_type_location location) noexcept -> view
            {
                if(location.module == nullptr) { return {}; }
                auto const& types{location.module->type_section_storage};
                if(types.owned_signature_begin == nullptr && types.owned_signature_end == nullptr) { return {}; }
                if(types.owned_signature_begin == nullptr || types.owned_signature_end == nullptr) { return {}; }
                auto const count{safe_ptr_range_size(types.owned_signature_begin, types.owned_signature_end)};
                auto const type_count{safe_ptr_range_size(types.type_section_begin, types.type_section_end)};
                if(count != type_count || location.index >= count) { return {}; }
                // [owned_signature_begin, owned_signature_end) aligns one-to-one with type indices.
                // [safe                                      ] no pointer movement; the view borrows a frozen allocation.
                return {types.owned_signature_begin, count};
            }};
            auto const lhs{rich_view(expected_owner)}, rhs{rich_view(actual_owner)};
            if(lhs.size() != 0uz && rhs.size() != 0uz)
            {
                using comparison = ::uwvm2::validation::standard::wasm3::core3_defined_type_comparison;
                return ::uwvm2::validation::standard::wasm3::core3_function_types_equivalent_across_sections(
                    expected_owner.index, lhs, actual_owner.index, rhs,
                    [&](::std::size_t left, ::std::size_t right) noexcept
                    {
                        auto const& left_module{*expected_owner.module};
                        auto const& right_module{*actual_owner.module};
                        bool const left_recursive{left_module.type_section_storage.core3_recursive_types_ptr != nullptr};
                        bool const right_recursive{right_module.type_section_storage.core3_recursive_types_ptr != nullptr};
                        if(!left_recursive && !right_recursive) { return comparison::structural; }
                        if(!left_recursive || !right_recursive) { return comparison::different; }
                        return ::uwvm2::uwvm::runtime::storage::gc_object_store::canonical_defined_type_equal(
                            left_module.gc_store.get(), static_cast<::std::uint_least32_t>(left),
                            right_module.gc_store.get(), static_cast<::std::uint_least32_t>(right)) ?
                            comparison::equivalent : comparison::different;
                    });
            }
            if(lhs.size() == 0uz && rhs.size() == 0uz) { return true; }
            // A host/local-imported legacy signature has no rich declaration. It
            // can match only the exact nullable abstract reference represented by
            // its carrier. Defined/non-null heaps never become equal by projection.
            auto const match_legacy{[&](bool result, ::std::size_t index) noexcept
            {
                auto const left{lhs.size() == 0uz ?
                    ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(
                        result ? expected->result.begin[index] : expected->parameter.begin[index]) :
                    (result ? lhs.index_unchecked(expected_owner.index).results.index_unchecked(index) :
                              lhs.index_unchecked(expected_owner.index).parameters.index_unchecked(index))};
                auto const right{rhs.size() == 0uz ?
                    ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(
                        result ? actual->result.begin[index] : actual->parameter.begin[index]) :
                    (result ? rhs.index_unchecked(actual_owner.index).results.index_unchecked(index) :
                              rhs.index_unchecked(actual_owner.index).parameters.index_unchecked(index))};
                return left == right;
            }};
            for(::std::size_t i{}; i != expected_para_len; ++i) { if(!match_legacy(false, i)) { return false; } }
            for(::std::size_t i{}; i != expected_res_len; ++i) { if(!match_legacy(true, i)) { return false; } }
            return true;
        }

        inline bool wasm1_function_type_equal(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* expected,
                                             ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* actual) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return wasm1_function_type_equal_in_context(world, expected, actual);
        }

        // Function import admission uses actual <= expected (Core 3 defined-type
        // matching), independently of the exact ABI identity used by tags and
        // replacement. Do not reduce a recursive declaration to carrier bytes.
        template <initialization_purpose Purpose>
        [[nodiscard]] inline bool wasm1_function_type_matches_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* expected,
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* actual) noexcept
        {
            if(expected == nullptr || actual == nullptr) { return false; }
            // [retained module type arrays] respective one-past bounds
            // [safe                       ] unsafe (one-past)
            // ^^ expected/actual: borrowed signatures. Location recovery checks
            // exact element identity without subtracting unrelated pointers.
            auto const expected_location{locate_linked_function_type_in_context(world, expected)};
            auto const actual_location{locate_linked_function_type_in_context(world, actual)};
            auto const has_recursive_declaration{[](linked_function_type_location location) noexcept
            {
                return location.module != nullptr &&
                       location.module->type_section_storage.core3_recursive_types_ptr != nullptr;
            }};
            bool const expected_recursive{has_recursive_declaration(expected_location)};
            bool const actual_recursive{has_recursive_declaration(actual_location)};
            if(expected_recursive && actual_recursive)
            {
                if(expected_location.index > (::std::numeric_limits<::std::uint_least32_t>::max)() ||
                   actual_location.index > (::std::numeric_limits<::std::uint_least32_t>::max)())
                { return false; }
                // [module-owned canonical stores] pinned by initialization
                // [safe                        ] no guest address or parser cursor is used.
                // ^^ get() borrows each immutable store; the checked predicate
                // bounds both indices, checks FUNCTION kind, then actual <= expected.
                return ::uwvm2::uwvm::runtime::storage::gc_object_store::canonical_function_type_matches(
                    actual_location.module->gc_store.get(), static_cast<::std::uint_least32_t>(actual_location.index),
                    expected_location.module->gc_store.get(), static_cast<::std::uint_least32_t>(expected_location.index));
            }
            if(expected_recursive || actual_recursive)
            {
                auto const recursive_location{expected_recursive ? expected_location : actual_location};
                // [one retained module type record] initialization owns the declaration.
                // [safe                           ] no module pointer advances or escapes.
                // ^^ signature borrows the corresponding parser/host function type;
                // every parameter/result element below is bounded before access.
                auto const* signature{expected_recursive ? expected : actual};
                auto const* section{recursive_location.module->type_section_storage.core3_recursive_types_ptr};
                if(recursive_location.index >= section->type_count) { return false; }
                bool lossless_legacy_declaration{};
                for(auto const& group : section->groups)
                {
                    if(group.first_type_index != recursive_location.index || group.types.size() != 1uz) { continue; }
                    // [group.types: one owned element] one-past
                    // [safe                         ] unsafe (one-past)
                    // ^^ declaration borrows element zero after the exact singleton check.
                    auto const& declaration{group.types.index_unchecked(0uz)};
                    using composite = ::uwvm2::parser::wasm::standard::wasm3::type::composite_kind;
                    if(declaration.kind != composite::function || !declaration.final_ || !declaration.supertypes.empty())
                    { return false; }
                    auto const parameter_count{safe_ptr_range_size(signature->parameter.begin, signature->parameter.end)};
                    auto const result_count{safe_ptr_range_size(signature->result.begin, signature->result.end)};
                    if(declaration.parameters.size() != parameter_count || declaration.results.size() != result_count)
                    { return false; }
                    for(::std::size_t i{}; i != parameter_count; ++i)
                    {
                        // [parameter.begin, parameter.end) and [0, parameter_count)
                        // [safe                        ] respective checked arrays; no cursor advances.
                        // ^^ begin[i]/parameters[i]: rich nullability/heap must equal the full legacy meaning.
                        if(declaration.parameters.index_unchecked(i) !=
                           ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(signature->parameter.begin[i]))
                        { return false; }
                    }
                    for(::std::size_t i{}; i != result_count; ++i)
                    {
                        // [result.begin, result.end) and [0, result_count)
                        // [safe                  ] respective checked arrays; no cursor advances.
                        // ^^ begin[i]/results[i]: a defined/non-null heap never matches its projected carrier.
                        if(declaration.results.index_unchecked(i) !=
                           ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(signature->result.begin[i]))
                        { return false; }
                    }
                    lossless_legacy_declaration = true;
                    break;
                }
                // A legacy signature denotes one implicit final, parentless
                // function definition. A nonfinal or multi-member recursive
                // outer identity cannot become equivalent through projection.
                if(!lossless_legacy_declaration) { return false; }
            }
            return wasm1_function_type_equal_in_context(world, expected, actual);
        }

        [[nodiscard]] inline bool wasm1_function_type_matches(
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* expected,
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* actual) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return wasm1_function_type_matches_in_context(world, expected, actual);
        }

        struct linked_function_record_location
        {
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* module{};
            ::std::size_t index{};
        };

        template <typename Record, typename Records>
        [[nodiscard]] inline bool linked_function_record_index(Record const* candidate, Records const& records,
                                                                ::std::size_t& index) noexcept
        {
            if(candidate == nullptr || records.empty()) { return false; }
            // [0, records.size()) frozen native records; nonempty proves record zero exists.
            // [safe             ] unsafe (one-past)
            // ^^ begin is borrowed from the actual owning vector, not from the candidate.
            auto const begin{::std::addressof(records.index_unchecked(0uz))};
            auto const begin_address{reinterpret_cast<::std::uintptr_t>(begin)};
            auto const candidate_address{reinterpret_cast<::std::uintptr_t>(candidate)};
            if(candidate_address < begin_address) { return false; }
            auto const displacement{candidate_address - begin_address};
            if(displacement % sizeof(Record) != 0uz) { return false; }
            auto const candidate_index{displacement / sizeof(Record)};
            if(candidate_index >= records.size()) { return false; }
            index = static_cast<::std::size_t>(candidate_index);
            // [0, records.size()) frozen native records; index is bounded and aligned above.
            // [safe             ] unsafe (one-past)
            // ^^ real record[index] authenticates exact element identity without reading candidate.
            return ::std::addressof(records.index_unchecked(index)) == candidate;
        }

        template <initialization_purpose Purpose>
        [[nodiscard]] inline linked_function_record_location locate_linked_imported_function_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::uwvm::runtime::storage::imported_function_storage_t const* candidate) noexcept
        {
            for(auto const& [name, module]: world.modules())
            {
                static_cast<void>(name);
                ::std::size_t index{};
                if(linked_function_record_index(candidate, module.imported_function_vec_storage, index))
                {
                    // [live registry module] its frozen vector authenticated candidate above.
                    // ^^ returned module is a native map element, not a guest-supplied owner.
                    return {::std::addressof(module), index};
                }
            }
            return {};
        }

        [[nodiscard]] inline linked_function_record_location locate_linked_imported_function(
            ::uwvm2::uwvm::runtime::storage::imported_function_storage_t const* candidate) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return locate_linked_imported_function_in_context(world, candidate);
        }

        template <initialization_purpose Purpose>
        [[nodiscard]] inline linked_function_record_location locate_linked_defined_function_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t const* candidate) noexcept
        {
            for(auto const& [name, module]: world.modules())
            {
                static_cast<void>(name);
                ::std::size_t index{};
                if(linked_function_record_index(candidate, module.local_defined_function_vec_storage, index))
                {
                    // [live registry module] its frozen vector authenticated candidate above.
                    // ^^ returned index is local-defined function identity, not a flat type index.
                    return {::std::addressof(module), index};
                }
            }
            return {};
        }

        [[nodiscard]] inline linked_function_record_location locate_linked_defined_function(
            ::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t const* candidate) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return locate_linked_defined_function_in_context(world, candidate);
        }

        struct linked_imported_function_leaf
        {
            ::uwvm2::uwvm::runtime::storage::imported_function_storage_t const* imported{};
            ::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t const* defined{};
            linked_function_record_location defined_location{};
            linked_function_type_location type_location{};
        };

        // All registry records are linked in a complete pass before import type
        // admission and constexpr reference evaluation. Pending/unresolved chains
        // never supply an actual type. Keep this usable without compiled registries.
        template <initialization_purpose Purpose>
        [[nodiscard]] inline linked_imported_function_leaf resolve_linked_imported_function_leaf_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::uwvm::runtime::storage::imported_function_storage_t const* candidate) noexcept
        {
            using link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
            ::std::size_t remaining{};
            for(auto const& [name, module]: world.modules())
            {
                static_cast<void>(name);
                auto const count{module.imported_function_vec_storage.size()};
                if(count > ::std::numeric_limits<::std::size_t>::max() - remaining) { return {}; }
                remaining += count;
            }
            // [untrusted candidate address] no member may be read until registry authentication.
            // ^^ current is an address token only at this point.
            auto current{candidate};
            while(remaining != 0uz)
            {
                --remaining;
                auto const location{locate_linked_imported_function_in_context(world, current)};
                if(location.module == nullptr) { return {}; }
                // [0, owned imported count) location.index names an exact live vector element.
                // [safe                   ] unsafe (one-past)
                // ^^ current is rebound to the real native record before any member read.
                current = ::std::addressof(location.module->imported_function_vec_storage.index_unchecked(location.index));
                if(current->import_type_ptr == nullptr) { return {}; }
                switch(current->link_kind)
                {
                    case link_kind::imported:
                    {
                        // [authenticated source record] -> [next candidate, possibly null/invalid]
                        // ^^ current changes to an address token; next iteration authenticates it.
                        current = current->target.imported_ptr;
                        break;
                    }
                    case link_kind::defined:
                    {
                        auto const defined_location{locate_linked_defined_function_in_context(world, current->target.defined_ptr)};
                        if(defined_location.module == nullptr) { return {}; }
                        // [0, provider defined count) exact registry membership bounded the index.
                        // [safe                    ] unsafe (one-past)
                        // ^^ defined is borrowed from its actual provider's frozen native vector.
                        auto const defined{::std::addressof(
                            defined_location.module->local_defined_function_vec_storage.index_unchecked(defined_location.index))};
                        auto const type_location{locate_linked_function_type_in_context(world, defined->function_type_ptr, defined_location.module)};
                        if(type_location.module != defined_location.module) { return {}; }
                        return {current, defined, defined_location, type_location};
                    }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                    case link_kind::dl:
                        if(current->target.dl_ptr == nullptr) { return {}; }
                        return {current};
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                    case link_kind::weak_symbol:
                        if(current->target.weak_symbol_ptr == nullptr) { return {}; }
                        return {current};
#endif
                    case link_kind::local_imported:
                        if(current->target.local_imported.module_ptr == nullptr) { return {}; }
                        return {current};
                    case link_kind::unresolved: return {};
                    default: return {};
                }
            }
            // More aliases than registered imported records necessarily revisits
            // a record. Reject cycles even outside the normal initializer pipeline.
            return {};
        }

        [[nodiscard]] inline linked_imported_function_leaf resolve_linked_imported_function_leaf(
            ::uwvm2::uwvm::runtime::storage::imported_function_storage_t const* candidate) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return resolve_linked_imported_function_leaf_in_context(world, candidate);
        }

        template <initialization_purpose Purpose>
        inline bool wasm1_function_type_equal_to_capi_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* expected,
                                                                ::uwvm2::uwvm::wasm::type::capi_function_t const* actual) noexcept
        {
            if(expected == nullptr || actual == nullptr) { return false; }

            auto const expected_para_len{safe_ptr_range_size(expected->parameter.begin, expected->parameter.end)};
            if(expected_para_len != actual->para_type_vec_size) { return false; }
            if(actual->para_type_vec_size != 0uz && actual->para_type_vec_begin == nullptr) [[unlikely]] { return false; }

            for(::std::size_t i{}; i != expected_para_len; ++i)
            {
                if(static_cast<::std::uint_least8_t>(expected->parameter.begin[i]) != actual->para_type_vec_begin[i]) { return false; }
            }

            auto const expected_res_len{safe_ptr_range_size(expected->result.begin, expected->result.end)};
            if(expected_res_len != actual->res_type_vec_size) { return false; }
            if(actual->res_type_vec_size != 0uz && actual->res_type_vec_begin == nullptr) [[unlikely]] { return false; }

            for(::std::size_t i{}; i != expected_res_len; ++i)
            {
                if(static_cast<::std::uint_least8_t>(expected->result.begin[i]) != actual->res_type_vec_begin[i]) { return false; }
            }

            // The host C API carries only flat bytes. It can represent nullable abstract
            // references, but has no type-index/nullability witness for a defined heap.
            // Compare every rich declaration to the exact type that its carrier means.
            auto const owner{locate_linked_function_type_in_context(world, expected)};
            auto const* rich{owner.module == nullptr ? nullptr : owner.module->type_section_storage.owned_signature_begin};
            auto const* rich_end{owner.module == nullptr ? nullptr : owner.module->type_section_storage.owned_signature_end};
            if(rich != nullptr && rich_end != nullptr &&
               owner.index < safe_ptr_range_size(rich, rich_end))
            {
                // [rich, rich_end) is the retained module-owned type table.
                // [safe          ] unsafe (one-past)
                //        ^^ rich[owner.index] is bounded by the range check above.
                auto const& signature{rich[owner.index]};
                if(signature.parameters.size() != expected_para_len || signature.results.size() != expected_res_len) { return false; }
                for(::std::size_t i{}; i != expected_para_len; ++i)
                {
                    if(signature.parameters.index_unchecked(i) !=
                       ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(expected->parameter.begin[i])) { return false; }
                }
                for(::std::size_t i{}; i != expected_res_len; ++i)
                {
                    if(signature.results.index_unchecked(i) !=
                       ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(expected->result.begin[i])) { return false; }
                }
            }
            else
            {
                for(::std::size_t i{}; i != expected_para_len; ++i)
                { if(static_cast<unsigned>(expected->parameter.begin[i]) >= 0x69u &&
                     static_cast<unsigned>(expected->parameter.begin[i]) <= 0x70u) { return false; } }
                for(::std::size_t i{}; i != expected_res_len; ++i)
                { if(static_cast<unsigned>(expected->result.begin[i]) >= 0x69u &&
                     static_cast<unsigned>(expected->result.begin[i]) <= 0x70u) { return false; } }
            }

            return true;
        }

        inline bool wasm1_function_type_equal_to_capi(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* expected,
                                                                ::uwvm2::uwvm::wasm::type::capi_function_t const* actual) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return wasm1_function_type_equal_to_capi_in_context(world, expected, actual);
        }

        // Shared by GC casts/tests and typed aggregate-field admission, including
        // constant expressions evaluated before any compiler dispatch cache exists.
        template <initialization_purpose Purpose>
        [[nodiscard]] inline bool linked_gc_function_reference_type_matches_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::uwvm::runtime::storage::gc_reference reference,
            ::uwvm2::uwvm::runtime::storage::gc_object_store const* expected_store,
            ::std::uint_least32_t expected_index) noexcept
        {
            namespace storage = ::uwvm2::uwvm::runtime::storage;
            using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
            using function_type = storage::wasm_binfmt1_final_function_type_t;
            if(expected_store == nullptr ||
               (reference.kind != ref_kind::wasm_func_defined && reference.kind != ref_kind::wasm_func_imported) ||
               reference.storage.ptr == nullptr)
            { return false; }

            storage::wasm_module_storage_t const* expected_module{};
            // The native initializer or execution owner pins the complete registry.
            // Maintenance cannot retire its frozen vectors during this synchronous
            // lookup. Compare store identity before borrowing any type record.
            for(auto const& [name, module] : world.modules())
            {
                static_cast<void>(name);
                if(module.gc_store.get() == expected_store)
                {
                    // [map-owned module] retained by the enclosing native domain.
                    // ^^ borrow only this registered module; no guest pointer is used.
                    expected_module = ::std::addressof(module);
                    break;
                }
            }
            if(expected_module == nullptr) { return false; }

            storage::wasm_module_storage_t const* actual_module{};
            function_type const* actual_type{};
            if(reference.kind == ref_kind::wasm_func_defined)
            {
                // Convert the opaque token only for identity lookup; never read
                // a field through this untrusted token before membership succeeds.
                auto const* token{static_cast<storage::local_defined_function_storage_t const*>(reference.storage.ptr)};
                auto const location{locate_linked_defined_function_in_context(world, token)};
                if(location.module == nullptr ||
                   location.index >= location.module->local_defined_function_vec_storage.size())
                { return false; }
                // [frozen defined records, checked local index] one-past
                // [safe                                      ] unsafe (one-past)
                // ^^ read the genuine vector element, not the caller's token.
                actual_type = location.module->local_defined_function_vec_storage.index_unchecked(location.index).function_type_ptr;
                // ^^ borrow its owning module until the comparison returns.
                actual_module = location.module;
            }
            else if(reference.kind == ref_kind::wasm_func_imported)
            {
                // Every alias node, including the original token, is membership
                // checked by the resolver before reading its initialized link.
                auto const* token{static_cast<storage::imported_function_storage_t const*>(reference.storage.ptr)};
                auto const leaf{resolve_linked_imported_function_leaf_in_context(world, token)};
                if(leaf.defined != nullptr)
                {
                    if(leaf.defined_location.module == nullptr ||
                       leaf.defined_location.index >= leaf.defined_location.module->local_defined_function_vec_storage.size())
                    { return false; }
                    // [checked final provider record] the resolver proved its
                    // storage and declared type belong to this retained module.
                    // ^^ borrow metadata only; no native entry address is cached.
                    actual_module = leaf.defined_location.module;
                    // [frozen provider record, checked local index] one-past
                    // [safe                                      ] unsafe (one-past)
                    // ^^ actual_type borrows its complete immutable declaration.
                    actual_type = actual_module->local_defined_function_vec_storage.index_unchecked(
                        leaf.defined_location.index).function_type_ptr;
                }
                else if(leaf.imported != nullptr)
                {
                    auto const location{locate_linked_imported_function_in_context(world, leaf.imported)};
                    if(location.module == nullptr ||
                       location.index >= location.module->imported_function_vec_storage.size())
                    { return false; }
                    // [checked terminal host-import record] no compiled registry
                    // is required while initializer globals are being evaluated.
                    auto const& record{location.module->imported_function_vec_storage.index_unchecked(location.index)};
                    if(record.import_type_ptr == nullptr) { return false; }
                    // ^^ borrow only the registered leaf's declaration/owner.
                    actual_module = location.module;
                    // [registered host-import declaration] held by this module
                    // [safe                              ] no guest bytes are read.
                    // ^^ actual_type borrows metadata, never a native code address.
                    actual_type = record.import_type_ptr->imports.storage.function;
                }
                else { return false; }
            }
            else { return false; }
            if(actual_module == nullptr || actual_type == nullptr) { return false; }

            auto const checked_index{[](storage::wasm_module_storage_t const& module,
                                       function_type const* type, ::std::size_t& index) noexcept
            {
                auto const& types{module.type_section_storage};
                if(type == nullptr || types.type_section_begin == nullptr || types.type_section_end == nullptr)
                { return false; }
                auto const count{safe_ptr_range_size(types.type_section_begin, types.type_section_end)};
                if(count != types.type_section_count) { return false; }
                // Only integers derived from live metadata are compared here.
                // The candidate token is never subtracted from an unrelated C++
                // pointer or dereferenced to establish its type-array membership.
                auto const begin{reinterpret_cast<::std::uintptr_t>(types.type_section_begin)};
                auto const address{reinterpret_cast<::std::uintptr_t>(type)};
                if(address < begin) { return false; }
                auto const difference{address - begin};
                if(difference % sizeof(function_type) != 0uz ||
                   difference / sizeof(function_type) >= count)
                { return false; }
                index = static_cast<::std::size_t>(difference / sizeof(function_type));
                return true;
            }};
            auto const& expected_types{expected_module->type_section_storage};
            if(expected_types.type_section_begin == nullptr || expected_types.type_section_end == nullptr ||
               expected_index >= safe_ptr_range_size(expected_types.type_section_begin, expected_types.type_section_end))
            { return false; }
            // [retained expected type array, checked flat index] one-past
            // [safe                                           ] unsafe (one-past)
            // ^^ form this complete record only after the module/index proof.
            auto const* expected_type{expected_types.type_section_begin + expected_index};
            ::std::size_t expected_flat{}, actual_flat{};
            if(!checked_index(*expected_module, expected_type, expected_flat) ||
               !checked_index(*actual_module, actual_type, actual_flat))
            { return false; }
            if(expected_types.core3_recursive_types_ptr != nullptr &&
               actual_module->type_section_storage.core3_recursive_types_ptr != nullptr)
            {
                if(actual_flat > (::std::numeric_limits<::std::uint_least32_t>::max)())
                { return false; }
                // [both canonical stores] owned by checked live modules. The
                // predicate bounds both FUNCTION layouts before actual <= expected.
                // ^^ get() borrows the provider's immutable store for this call.
                return storage::gc_object_store::canonical_function_type_matches(
                    actual_module->gc_store.get(), static_cast<::std::uint_least32_t>(actual_flat),
                    expected_store, expected_index);
            }
            // Legacy/rich interoperability retains the existing singleton-final,
            // parentless and lossless-reference proof; a carrier alone is insufficient.
            return wasm1_function_type_matches_in_context(world, expected_type, actual_type);
        }

        [[nodiscard]] inline bool linked_gc_function_reference_type_matches(
            ::uwvm2::uwvm::runtime::storage::gc_reference reference,
            ::uwvm2::uwvm::runtime::storage::gc_object_store const* expected_store,
            ::std::uint_least32_t expected_index) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            return linked_gc_function_reference_type_matches_in_context(world, reference, expected_store, expected_index);
        }

        template <initialization_purpose Purpose>
        inline constexpr void validate_and_resolve_core3_tags_after_linking_in_context(initialization_context<Purpose>& world) noexcept
        {
            using tag_pointer = ::uwvm2::uwvm::runtime::storage::imported_tag_storage_t const*;
            for(auto& [module_name, module] : world.modules())
            {
                for(auto& imported : module.imported_tag_vec_storage)
                {
                    auto fail = [&](::uwvm2::utils::container::u8string_view reason) noexcept
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ", ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ", ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"initializer: tag import in module \"", module_name, u8"\": ", reason, u8".\n\n",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    };
                    auto const next = [](tag_pointer value) noexcept -> tag_pointer
                    { return value == nullptr ? nullptr : value->imported_target; };
                    check_no_import_alias_cycle_floyd(static_cast<tag_pointer>(::std::addressof(imported)), next,
                        [&]() noexcept { fail(u8"cyclic tag alias has no concrete instance"); });
                    // [live imported record] arrays were frozen before linking; the chain is now known acyclic.
                    // ^^ current
                    tag_pointer current{::std::addressof(imported)};
                    while(current->imported_target != nullptr)
                    {
                        // [current record] imported_target names another checked stable record, never a guest pointer.
                        // ^^ current advances along the proven acyclic chain; the loop keeps a live record.
                        current = current->imported_target;
                    }
                    auto const target{current->defined_target};
                    if(target == nullptr) { fail(u8"unresolved tag, wrong export kind or unsupported host tag provider"); }
                    // Current parser accepts only lossless function signatures; compare complete parameter/result types,
                    // not type indices, byte widths or arity. Rich canonical tag types must replace this comparison when enabled.
                    if(!wasm1_function_type_equal_in_context(world, imported.function_type_ptr, target->function_type_ptr))
                    { fail(u8"tag payload type mismatch"); }
                    // [concrete retained tag record] store the instance identity once, avoiding alias walks on each throw.
                    // ^^ resolved_tag; code publication happens only after this phase completes for all modules.
                    imported.resolved_tag = target;
                }
            }
        }

        inline constexpr void validate_and_resolve_core3_tags_after_linking() noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            validate_and_resolve_core3_tags_after_linking_in_context(world);
        }

        template <initialization_purpose Purpose>
        inline constexpr void validate_wasm_file_module_import_types_after_linking_in_context(initialization_context<Purpose>& world) noexcept
        {
            using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;

            for([[maybe_unused]] auto& [curr_module_name, curr_rt]: world.modules())
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_info(u8"initializer: Validate import types for module \"",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"\". ");
                }

                ::std::size_t func_checked{};
                ::std::size_t table_checked{};
                ::std::size_t table_skipped_unresolved{};
                ::std::size_t memory_checked{};
                ::std::size_t memory_skipped_unresolved{};
                ::std::size_t global_checked{};

                // func imports
                for(auto const& imp: curr_rt.imported_function_vec_storage)
                {
                    auto const import_ptr{imp.import_type_ptr};
                    if(import_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                    if(imp.link_kind == func_link_kind::unresolved) { continue; }
                    ++func_checked;

                    if(import_ptr->imports.type != external_types::func) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    auto const expected_type{import_ptr->imports.storage.function};
                    if(expected_type == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    if(imp.link_kind == func_link_kind::imported)
                    {
                        auto const imported_target{imp.target.imported_ptr};
                        if(imported_target == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const leaf{resolve_linked_imported_function_leaf_in_context(world, imported_target)};
                        if(leaf.imported == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }
                        // [authenticated alias head] the complete bounded walk proved its
                        // [safe                    ] native registry membership before this read.
                        // ^^ target_import_ptr borrows its immutable parser import declaration.
                        auto const target_import_ptr{imported_target->import_type_ptr};
                        if(target_import_ptr == nullptr || target_import_ptr->imports.type != external_types::func ||
                           target_import_ptr->imports.storage.function == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        // [authenticated provider record] its function_type_ptr belongs
                        // [safe                         ] to the provider's frozen type array.
                        // ^^ actual_type preserves the concrete Wasm function instance type;
                        //    native/local/CAPI aliases retain their existing declaration contract.
                        auto const actual_type{leaf.defined != nullptr ? leaf.defined->function_type_ptr
                                                                      : target_import_ptr->imports.storage.function};
                        if(!wasm1_function_type_matches_in_context(world, expected_type, actual_type)) [[unlikely]]
                        {
#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
                            // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", imported function \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch. expected: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*expected_type),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", got: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*actual_type),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#else
                            // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
                            {
                                auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                                ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                                    ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                                auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported function \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" has a type mismatch. expected: \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*expected_type),
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", got: \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*actual_type),
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\".\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            }
#endif
                            ::fast_io::fast_terminate();
                        }
                    }
                    else if(imp.link_kind == func_link_kind::defined)
                    {
                        auto const def{imp.target.defined_ptr};
                        if(def == nullptr || def->function_type_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const actual_type{def->function_type_ptr};
                        if(!wasm1_function_type_matches_in_context(world, expected_type, actual_type)) [[unlikely]]
                        {
#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
                            // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", imported function \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch. expected: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*expected_type),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", got: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*actual_type),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#else
                            // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
                            {
                                auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                                ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                                    ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                                auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported function \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" has a type mismatch. expected: \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*expected_type),
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", got: \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*actual_type),
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\".\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            }
#endif
                            ::fast_io::fast_terminate();
                        }
                    }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                    else if(imp.link_kind == func_link_kind::dl)
                    {
                        auto const dl_ptr{imp.target.dl_ptr};
                        if(dl_ptr == nullptr) [[unlikely]]
                        {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                            ::fast_io::fast_terminate();
                        }

                        if(!wasm1_function_type_equal_to_capi_in_context(world, expected_type, dl_ptr)) [[unlikely]]
                        {
# ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
                            // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", imported function \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch. expected: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*expected_type),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", got: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                u8"(dl) para_types=",
                                                dl_ptr->para_type_vec_size,
                                                u8", res_types=",
                                                dl_ptr->res_type_vec_size,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
# else
                            // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
                            {
                                auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                                ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                                    ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                                auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported function \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" has a type mismatch. expected: \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*expected_type),
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", got: \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    u8"(dl) para_types=",
                                                    dl_ptr->para_type_vec_size,
                                                    u8", res_types=",
                                                    dl_ptr->res_type_vec_size,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\".\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            }
# endif
                            ::fast_io::fast_terminate();
                        }
                    }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                    else if(imp.link_kind == func_link_kind::weak_symbol)
                    {
                        auto const weak_ptr{imp.target.weak_symbol_ptr};
                        if(weak_ptr == nullptr) [[unlikely]]
                        {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                            ::fast_io::fast_terminate();
                        }

                        if(!wasm1_function_type_equal_to_capi_in_context(world, expected_type, weak_ptr)) [[unlikely]]
                        {
# ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
                            // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", imported function \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch. expected: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*expected_type),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", got: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                u8"(weak_symbol) para_types=",
                                                weak_ptr->para_type_vec_size,
                                                u8", res_types=",
                                                weak_ptr->res_type_vec_size,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
# else
                            // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
                            {
                                auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                                ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                                    ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                                auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported function \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" has a type mismatch. expected: \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*expected_type),
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", got: \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    u8"(weak_symbol) para_types=",
                                                    weak_ptr->para_type_vec_size,
                                                    u8", res_types=",
                                                    weak_ptr->res_type_vec_size,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\".\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            }
# endif
                            ::fast_io::fast_terminate();
                        }
                    }
#endif
                    else if(imp.link_kind == func_link_kind::local_imported)
                    {
                        auto const& li{imp.target.local_imported};
                        if(li.module_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const info{li.module_ptr->get_function_information_from_index(li.index)};
                        if(!info.successed) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const actual_type{::std::addressof(info.function_type)};
                        if(!wasm1_function_type_matches_in_context(world, expected_type, actual_type)) [[unlikely]]
                        {
#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
                            // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", imported function \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch. expected: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*expected_type),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", got: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*actual_type),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#else
                            // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
                            {
                                auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                                ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                                    ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                                auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported function \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" has a type mismatch. expected: \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*expected_type),
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", got: \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(*actual_type),
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\".\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            }
#endif
                            ::fast_io::fast_terminate();
                        }
                    }
                    else [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                }

                // table imports
                for(auto const& imp: curr_rt.imported_table_vec_storage)
                {
                    auto const import_ptr{imp.import_type_ptr};
                    if(import_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    using table_link_kind = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                    if(imp.link_kind == table_link_kind::unresolved) { continue; }

                    if(import_ptr->imports.type != external_types::table) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t* resolved_table{};
                    if(!maybe_resolve_wasm1_imported_table_defined_in_context(world, ::std::addressof(imp), resolved_table))
                    {
                        ++table_skipped_unresolved;
                        continue;
                    }
                    ++table_checked;

                    if(resolved_table == nullptr || resolved_table->table_type_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    auto const& expected_table{import_ptr->imports.storage.table};
                    auto const& actual_table{*resolved_table->table_type_ptr};
                    // Core 3 table imports require invariant reference types. A one-byte exnref
                    // and an explicit non-null (ref exn) share the 0x69 carrier but cannot alias.
                    bool exn_type_mismatch{};
                    bool expected_exn_nullable{}, actual_exn_nullable{};
                    bool expected_noexn_heap{}, actual_noexn_heap{};
                    if(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(expected_table.reftype) == 0x69u &&
                       static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(actual_table.reftype) == 0x69u)
                    {
                        namespace w3 = ::uwvm2::validation::standard::wasm3;
                        auto const expected_core{w3::core3_declaration_effective_type(
                            expected_table, ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(expected_table.reftype))};
                        auto const actual_core{w3::core3_declaration_effective_type(
                            actual_table, ::uwvm2::parser::wasm::standard::wasm1p1::features::to_value_type(actual_table.reftype))};
                        exn_type_mismatch = expected_core.kind != actual_core.kind ||
                            expected_core.heap.code != actual_core.heap.code || expected_core.nullable != actual_core.nullable;
                        expected_exn_nullable = expected_core.nullable;
                        actual_exn_nullable = actual_core.nullable;
                        using exn_heap = ::uwvm2::parser::wasm::standard::wasm3::type::abstract_heap_type;
                        expected_noexn_heap = expected_core.heap.code == static_cast<::std::int_least64_t>(exn_heap::noexn);
                        actual_noexn_heap = actual_core.heap.code == static_cast<::std::int_least64_t>(exn_heap::noexn);
                    }
                    if(expected_table.address64 != actual_table.address64 || expected_table.reftype != actual_table.reftype ||
                       exn_type_mismatch || !wasm1_limits_match(expected_table.limits, actual_table.limits)) [[unlikely]]
                    {
                        // The legacy table formatter has no name for Core 3's 0x69 carrier.
                        // Keep the exact heap and nullability visible in this cold link error.
                        auto const exn_type_name{[](bool nullable, bool noexn) noexcept -> char8_t const*
                        {
                            if(noexn) { return nullable ? u8"(ref null noexn)" : u8"(ref noexn)"; }
                            return nullable ? u8"exnref (ref null exn)" : u8"(ref exn)";
                        }};
                        auto const expected_exn_name{exn_type_name(expected_exn_nullable, expected_noexn_heap)};
                        auto const actual_exn_name{exn_type_name(actual_exn_nullable, actual_noexn_heap)};
#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
                        // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In module \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            curr_module_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\", imported table \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            import_ptr->module_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8".",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            import_ptr->extern_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\" has a type mismatch. expected: \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(expected_table),
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\", got: \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(actual_table),
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\".\n\n",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        if(exn_type_mismatch)
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                u8"Core 3 table reference type: expected ", ::fast_io::mnp::os_c_str(expected_exn_name),
                                                u8", got ", ::fast_io::mnp::os_c_str(actual_exn_name), u8".\n\n");
                        }
#else
                        // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
                        {
                            auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                            ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                                ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                            auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                            ::fast_io::io::perr(u8log_output_ul,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"");
                            ::fast_io::io::perr(u8log_output_ul,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", imported table \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8".");
                            ::fast_io::io::perr(u8log_output_ul,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch. expected: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(expected_table),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", got: \"");
                            ::fast_io::io::perr(u8log_output_ul,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(actual_table),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            if(exn_type_mismatch)
                            {
                                ::fast_io::io::perr(u8log_output_ul,
                                                    u8"Core 3 table reference type: expected ", ::fast_io::mnp::os_c_str(expected_exn_name),
                                                    u8", got ", ::fast_io::mnp::os_c_str(actual_exn_name), u8".\n\n");
                            }
                        }
#endif
                        ::fast_io::fast_terminate();
                    }
                }

                // memory imports
                for(auto const& imp: curr_rt.imported_memory_vec_storage)
                {
                    auto const import_ptr{imp.import_type_ptr};
                    if(import_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                    if(imp.link_kind == memory_link_kind::unresolved) { continue; }

                    if(import_ptr->imports.type != external_types::memory) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    wasm1_resolved_imported_memory_t resolved_memory{};
                    if(!maybe_resolve_wasm1_imported_memory_in_context(world, ::std::addressof(imp), resolved_memory))
                    {
                        ++memory_skipped_unresolved;
                        continue;
                    }
                    ++memory_checked;

                    auto const& expected_memory{import_ptr->imports.storage.memory};

                    if(resolved_memory.defined_ptr != nullptr)
                    {
                        if(resolved_memory.defined_ptr->memory_type_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const& actual_memory{*resolved_memory.defined_ptr->memory_type_ptr};
                        if(wasm_memory_is_shared(expected_memory) != wasm_memory_is_shared(actual_memory) ||
                           wasm_memory_is_address64(expected_memory) != wasm_memory_is_address64(actual_memory) ||
                           !wasm1_limits_match(expected_memory.limits, actual_memory.limits)) [[unlikely]]
                        {
#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
                            // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", imported memory \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch. expected: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(expected_memory),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", got: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(actual_memory),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#else
                            // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
                            {
                                auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                                ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                                    ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                                auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported memory \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" has a type mismatch. expected: \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(expected_memory),
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", got: \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(actual_memory),
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\".\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            }
#endif
                            ::fast_io::fast_terminate();
                        }
                    }
                    else
                    {
                        if(resolved_memory.local_imported_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const page_size_bytes{resolved_memory.local_imported_ptr->memory_page_size_from_index(resolved_memory.local_imported_index)};
                        if(page_size_bytes != 65536u) [[unlikely]]
                        {
#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
                            // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", imported memory \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\" has an unsupported host page size (page_size_bytes=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                page_size_bytes,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8").\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#else
                            // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
                            {
                                auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                                ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                                    ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                                auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported memory \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" has an unsupported host page size (page_size_bytes=",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    page_size_bytes,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8").\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            }
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const page_count_u64{resolved_memory.local_imported_ptr->memory_size_from_index(resolved_memory.local_imported_index)};
                        constexpr auto max_u32{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
                        if(page_count_u64 > max_u32) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        ::uwvm2::parser::wasm::standard::wasm1::type::memory_type actual_memory{};
                        actual_memory.limits.min = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(page_count_u64);
                        actual_memory.limits.present_max = true;
                        actual_memory.limits.max = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(page_count_u64);

                        if(wasm_memory_is_shared(expected_memory) || wasm_memory_is_address64(expected_memory) ||
                           !wasm1_limits_match(expected_memory.limits, actual_memory.limits)) [[unlikely]]
                        {
#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
                            // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", imported memory \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch. expected: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(expected_memory),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", got: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(actual_memory),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#else
                            // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
                            {
                                auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                                ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                                    ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                                auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported memory \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" has a type mismatch. expected: \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(expected_memory),
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", got: \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(actual_memory),
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\".\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            }
#endif
                            ::fast_io::fast_terminate();
                        }
                    }
                }

                // global imports
                for(auto const& imp: curr_rt.imported_global_vec_storage)
                {
                    auto const import_ptr{imp.import_type_ptr};
                    if(import_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    using global_link_kind = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t::imported_global_link_kind;
                    if(imp.link_kind == global_link_kind::unresolved) { continue; }
                    ++global_checked;

                    if(import_ptr->imports.type != external_types::global) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    auto const& expected_global{import_ptr->imports.storage.global};
                    using final_global_type = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_global_type_t;
                    using final_value_type = decltype(final_global_type{}.type);

                    final_global_type const* actual_global_ptr{};
                    final_global_type actual_global_local_imported{};

                    if(imp.link_kind == global_link_kind::imported)
                    {
                        auto const imported_target{imp.target.imported_ptr};
                        if(imported_target == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const target_import_ptr{imported_target->import_type_ptr};
                        if(target_import_ptr == nullptr || target_import_ptr->imports.type != external_types::global) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }
                        actual_global_ptr = ::std::addressof(target_import_ptr->imports.storage.global);
                    }
                    else if(imp.link_kind == global_link_kind::defined)
                    {
                        auto const def{imp.target.defined_ptr};
                        if(def == nullptr || def->global_type_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }
                        actual_global_ptr = def->global_type_ptr;
                    }
                    else
                    {
                        if(imp.link_kind != global_link_kind::local_imported) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const& li{imp.target.local_imported};
                        if(li.module_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const vt_u8{static_cast<::std::uint_least8_t>(li.module_ptr->global_value_type_from_index(li.index))};

                        switch(vt_u8)
                        {
                            case static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32): [[fallthrough]];
                            case static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64): [[fallthrough]];
                            case static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32): [[fallthrough]];
                            case static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64): [[fallthrough]];
                            case static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128): [[fallthrough]];
                            case static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref): [[fallthrough]];
                            case static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::externref): [[fallthrough]];
                            case 0x69u: // Core 3 exnref retains the exact rich declaration through imported-global aliases.
                            {
                                actual_global_local_imported.type = static_cast<final_value_type>(vt_u8);
                                break;
                            }
                            [[unlikely]] default:
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported global \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" has an unsupported host global type.\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            }
                        }

                        actual_global_local_imported.is_mutable = li.module_ptr->global_is_mutable_from_index(li.index);
                        actual_global_ptr = ::std::addressof(actual_global_local_imported);
                    }

                    if(actual_global_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    auto const& actual_global{*actual_global_ptr};
                    if(expected_global.type != actual_global.type || expected_global.is_mutable != actual_global.is_mutable) [[unlikely]]
                    {
#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
                        // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In module \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            curr_module_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\", imported global \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            import_ptr->module_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8".",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            import_ptr->extern_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\" has a type mismatch. expected: \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(expected_global),
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\", got: \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(actual_global),
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\".\n\n",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#else
                        // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
                        {
                            auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                            ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                                ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                            auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                            ::fast_io::io::perr(u8log_output_ul,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"");
                            ::fast_io::io::perr(u8log_output_ul,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", imported global \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8".");
                            ::fast_io::io::perr(u8log_output_ul,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                import_ptr->extern_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\" has a type mismatch. expected: \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(expected_global),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", got: \"");
                            ::fast_io::io::perr(u8log_output_ul,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                ::uwvm2::uwvm::wasm::section_detail::section_details_adl_caller(actual_global),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        }
#endif
                        ::fast_io::fast_terminate();
                    }
                }

                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_info(u8"initializer: Import type validation summary for module \"",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"\": checked(f/t/m/g)=",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 func_checked,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 table_checked,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 memory_checked,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 global_checked,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8", unresolved_skipped(t/m)=",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                 table_skipped_unresolved,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                 memory_skipped_unresolved,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8". ");
                }
            }
        }

        inline constexpr void validate_wasm_file_module_import_types_after_linking() noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            validate_wasm_file_module_import_types_after_linking_in_context(world);
        }

        template <initialization_purpose Purpose>
        inline constexpr void try_eval_wasm1_const_expr_offset_after_linking_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
                                                                             ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& curr_rt,
                                                                             ::std::uint_least64_t& out) noexcept
        {
            if(expr.opcodes.size() > 1uz)
            {
                auto const result{evaluate_wasm3_constant_expression_in_context(world, expr, curr_rt)};
                if(result.kind != ::uwvm2::object::global::global_type::wasm_i32) [[unlikely]] { ::fast_io::fast_terminate(); }
                out = static_cast<::std::uint_least32_t>(result.storage.i32);
                return;
            }
            if(expr.opcodes.size() != 1uz)
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"initializer: wasm1.0 const expr must contain exactly one opcode; got ",
                                    expr.opcodes.size(),
                                    u8".\n\n",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            auto const& op{expr.opcodes.front_unchecked()};

            if(op.opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const)
            {
                out = static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(op.storage.i32));
                return;
            }
            else if(op.opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get)
            {
                auto const idx{static_cast<::std::size_t>(op.storage.imported_global_idx)};
                auto const imported_global_count{curr_rt.imported_global_vec_storage.size()};
                auto const local_global_count{curr_rt.local_defined_global_vec_storage.size()};
                auto const all_global_count{imported_global_count + local_global_count};
                if(idx >= all_global_count) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"initializer: Constant expression offset global index is out of bounds: ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        idx,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8" >= ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        all_global_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8".\n\n",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }

                if(idx < imported_global_count)
                {
                    try_resolve_wasm1_imported_global_i32_value_in_context(world, ::std::addressof(curr_rt.imported_global_vec_storage.index_unchecked(idx)), out);
                }
                else
                {
                    auto const& local_global{curr_rt.local_defined_global_vec_storage.index_unchecked(idx - imported_global_count)};
                    if(local_global.init_state != ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized ||
                       local_global.global.kind != ::uwvm2::object::global::global_type::wasm_i32) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }
                    out = static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(local_global.global.storage.i32));
                }

                return;
            }

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"initializer: Constant expression offset retrieval in wasm1.0 encountered an invalid instruction: ",
                                ::fast_io::mnp::hex0x<true>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(op.opcode)),
                                u8".\n\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
            // terminate
            ::fast_io::fast_terminate();
        }

        inline constexpr void try_eval_wasm1_const_expr_offset_after_linking(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
                                                                             ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& curr_rt,
                                                                             ::std::uint_least64_t& out) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            try_eval_wasm1_const_expr_offset_after_linking_in_context(world, expr, curr_rt, out);
        }

        inline constexpr ::std::size_t safe_u32_to_size_t(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 v) noexcept;

        inline constexpr void try_eval_wasm3_memory_const_expr_offset(
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
            ::std::uint_least64_t& out, bool address64) noexcept
        {
            if(!address64) { try_eval_wasm1_const_expr_offset(expr, out); return; }
            if(expr.opcodes.size() > 1uz) { out = 0u; return; } // Evaluated after linking.
            if(expr.opcodes.size() == 1uz)
            {
                auto const& op{expr.opcodes.front_unchecked()}; // One checked, live expression entry.
                if(op.opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i64_const)
                {
                    // Wasm addresses interpret the complete i64 bit pattern as unsigned.
                    out = static_cast<::std::uint_least64_t>(op.storage.i64);
                    return;
                }
                if(op.opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get)
                { out = 0u; return; } // The imported value is not linked yet.
            }
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                u8"uwvm: [fatal] initializer: memory64 data offset requires an i64 constant expression.\n\n");
            ::fast_io::fast_terminate();
        }

        template <initialization_purpose Purpose>
        inline constexpr void try_eval_wasm3_memory_const_expr_offset_after_linking_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& module,
            ::std::uint_least64_t& out, bool address64) noexcept
        {
            if(!address64) { try_eval_wasm1_const_expr_offset_after_linking_in_context(world, expr, module, out); return; }
            auto const result{evaluate_wasm3_constant_expression_in_context(world, expr, module)};
            if(result.kind != ::uwvm2::object::global::global_type::wasm_i64) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                    u8"uwvm: [fatal] initializer: memory64 data offset requires an i64 constant expression.\n\n");
                ::fast_io::fast_terminate();
            }
            out = static_cast<::std::uint_least64_t>(result.storage.i64);
        }

        inline constexpr void try_eval_wasm3_memory_const_expr_offset_after_linking(
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& module,
            ::std::uint_least64_t& out, bool address64) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            try_eval_wasm3_memory_const_expr_offset_after_linking_in_context(world, expr, module, out, address64);
        }

        inline constexpr void try_eval_wasm3_table_const_expr_offset(
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
            ::std::uint_least64_t& out, bool address64) noexcept
        {
            if(!address64) { try_eval_wasm1_const_expr_offset(expr, out); return; }
            if(expr.opcodes.size() > 1uz) { out = 0u; return; } // Evaluated after linking.
            if(expr.opcodes.size() == 1uz)
            {
                auto const& op{expr.opcodes.front_unchecked()}; // One checked, live expression entry.
                if(op.opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i64_const)
                {
                    // Wasm addresses interpret the complete i64 bit pattern as unsigned.
                    out = static_cast<::std::uint_least64_t>(op.storage.i64);
                    return;
                }
                if(op.opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get)
                { out = 0u; return; } // The imported value is not linked yet.
            }
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                u8"uwvm: [fatal] initializer: table64 element offset requires an i64 constant expression.\n\n");
            ::fast_io::fast_terminate();
        }

        template <initialization_purpose Purpose>
        inline constexpr void try_eval_wasm3_table_const_expr_offset_after_linking_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& module,
            ::std::uint_least64_t& out, bool address64) noexcept
        {
            if(!address64) { try_eval_wasm1_const_expr_offset_after_linking_in_context(world, expr, module, out); return; }
            auto const result{evaluate_wasm3_constant_expression_in_context(world, expr, module)};
            if(result.kind != ::uwvm2::object::global::global_type::wasm_i64) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                    u8"uwvm: [fatal] initializer: table64 element offset requires an i64 constant expression.\n\n");
                ::fast_io::fast_terminate();
            }
            out = static_cast<::std::uint_least64_t>(result.storage.i64);
        }

        inline constexpr void try_eval_wasm3_table_const_expr_offset_after_linking(
            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_const_expr_t const& expr,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& module,
            ::std::uint_least64_t& out, bool address64) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            try_eval_wasm3_table_const_expr_offset_after_linking_in_context(world, expr, module, out, address64);
        }

        template <initialization_purpose Purpose, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void initialize_from_binfmt_ver1_module_storage_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& out) noexcept(Purpose == initialization_purpose::ordinary)
        {
            using type_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>;
            using import_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>;
            using table_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<Fs...>;
            using memory_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>;
            using global_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<Fs...>;
            using export_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::export_section_storage_t<Fs...>;
            using element_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<Fs...>;
            using code_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>;
            using data_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1::features::data_section_storage_t<Fs...>;
            using data_count_section_storage_t = ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...>;

            auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t>(module_storage.sections)};
            auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t>(module_storage.sections)};
            auto const& funcsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>(module_storage.sections)};
            auto const& tablesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<table_section_storage_t>(module_storage.sections)};
            auto const& memorysec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<memory_section_storage_t>(module_storage.sections)};
            auto const& globalsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<global_section_storage_t>(module_storage.sections)};
            auto const& exportsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<export_section_storage_t>(module_storage.sections)};
            auto const& elemsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<element_section_storage_t>(module_storage.sections)};
            auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<code_section_storage_t>(module_storage.sections)};
            auto const& datasec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<data_section_storage_t>(module_storage.sections)};
            auto const& datacountsec{
                ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<data_count_section_storage_t>(module_storage.sections)};

            enforce_wasm1p1_initializer_feature_parameters_in_context(world, module_storage, fs_para);

            out.data_count_section_present = datacountsec.present;
            out.data_count_section_count = datacountsec.count;

            out.type_section_storage.requires_function_references = typesec.requires_function_references;
            out.type_section_storage.requires_gc = typesec.requires_gc;
            out.type_section_storage.requires_exceptions = typesec.requires_exceptions;
            out.type_section_storage.requires_simd = typesec.requires_simd;
            out.type_section_storage.requires_reference_types = typesec.requires_reference_types;
            out.type_section_storage.requires_multi_value = typesec.requires_multi_value;
            // [actual parser-owned declaration context] retained with the module
            // [safe                                    ] no cursor or allocation changes.
            // ^^ This includes the real empty context of legacy function-only parsing.
            out.type_section_storage.core3_declaration_context_ptr = ::std::addressof(typesec.core3_context);
            // [0, parser type count) remains the exact bound for both legacy
            // function-only and recursive Core 3 defined heap references.
            out.type_section_storage.type_section_count = typesec.types.size();
            // [parser-owned Core 3 type section] remains immutable while this module executes.
            // [safe                            ] borrow only; no source or destination cursor advances.
            // ^^ compiler field validation may read it, while the GC runtime copies its own layouts below.
            out.type_section_storage.core3_recursive_types_ptr =
                typesec.core3_recursive_types.type_count == 0u ? nullptr : ::std::addressof(typesec.core3_recursive_types);
            // Every module may receive an externref bridge from another module,
            // even with GC instructions disabled. Its lightweight store/lease owner
            // pins those imported roots and canonicalizes any declared Core 3 types.
            // Only aggregate declarations allocate the GC membership index; the
            // GC opcode feature gate remains independent of this host-owned store.
            {
                out.gc_lease_roots = ::std::make_shared<::uwvm2::uwvm::runtime::storage::gc_lease_owner>();
                if(!out.gc_lease_roots) [[unlikely]] { ::fast_io::fast_terminate(); }
                out.gc_store = world.make_store(typesec.core3_recursive_types, out.gc_lease_roots,
                    linked_gc_function_reference_type_matches);
                // ^^ constructor binds trusted immutable code before publication;
                // typed funcref fields during later constexpr evaluation can use it.
                if(!out.gc_store || !out.gc_store->valid()) [[unlikely]] { ::fast_io::fast_terminate(); }
            }
            out.table_declarations_require_function_references = tablesec.requires_function_references;
            out.global_declarations_require_function_references = globalsec.requires_function_references;
            out.element_declarations_require_function_references = elemsec.requires_function_references;
            out.table_declarations_require_gc = tablesec.requires_gc;
            out.global_declarations_require_gc = globalsec.requires_gc;
            out.element_declarations_require_gc = elemsec.requires_gc;
            out.table_declarations_require_exceptions = tablesec.requires_exceptions;
            out.global_declarations_require_exceptions = globalsec.requires_exceptions;
            out.element_declarations_require_exceptions = elemsec.requires_exceptions;
            out.table_declarations_require_reference_types = tablesec.requires_reference_types;
            out.global_declarations_require_reference_types = globalsec.requires_reference_types;
            out.element_declarations_require_reference_types = elemsec.requires_reference_types;
            out.global_declarations_require_simd = globalsec.requires_simd;
            out.table_reference_types_diagnostic_value = tablesec.reference_types_diagnostic_value;
            out.global_reference_types_diagnostic_value = globalsec.reference_types_diagnostic_value;
            out.element_reference_types_diagnostic_value = elemsec.reference_types_diagnostic_value;
            out.memory_declarations_require_memory64 = memorysec.requires_memory64;
            out.table_declarations_require_table64 = tablesec.requires_table64;
            out.memory_declarations_require_threads = memorysec.requires_threads;
            out.memory_declarations_require_multi_memory = ::uwvm2::validation::standard::wasm3::core3_has_multiple_declarations(
                importsec.importdesc.index_unchecked(2uz).size(), memorysec.memories.size());
            out.table_declarations_require_table_initializer = tablesec.requires_table_initializer;
            out.constant_expressions_require_extended_const = globalsec.constant_expressions_require_extended_const;
            out.extended_const_diagnostic_value = globalsec.extended_const_diagnostic_value;
            out.extended_const_diagnostic_subject = globalsec.extended_const_diagnostic_subject;
            out.constant_expression_opcode_requirements = globalsec.constant_expression_opcode_requirements;
            out.code_declarations_require_exceptions = codesec.locals_require_exceptions;
            auto const& tagsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1p1::features::tag_section_storage_t<Fs...>>(module_storage.sections)};
            out.tag_section_present = tagsec.present;
            details::check_reserve_limit_in_context(world, u8"local_defined_tags", tagsec.type_indices.size(), world.limits().max_local_defined_tags);
            out.local_defined_tag_vec_storage.reserve(tagsec.type_indices.size());
            for(auto index : tagsec.type_indices)
            {
                if(index >= typesec.types.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const& type{typesec.types.index_unchecked(index)};
                if(type.result.begin != type.result.end) [[unlikely]] { ::fast_io::fast_terminate(); }
                // [validated type allocation] borrow this live record only after checking its index and empty results.
                // ^^ function_type_ptr: the module retains parsed types for the entire instance lifetime.
                // [new tag INSTANCE identity] ownership is published together with the checked
                // signature borrow. Imported aliases retain this same identity via resolved_tag;
                // each separately initialized local tag gets a distinct owning token. Exception
                // values retain only the token, so parser/module retirement cannot dangle identity.
                out.local_defined_tag_vec_storage.push_back({::std::addressof(type), index,
                    ::std::make_shared<::uwvm2::uwvm::runtime::storage::tag_instance_identity const>()});
            }


            // Expose type section range for runtime/compilers (e.g. call_indirect validation).
            if(typesec.types.empty())
            {
                out.type_section_storage.type_section_begin = nullptr;
                out.type_section_storage.type_section_end = nullptr;
            }
            else [[likely]]
            {
                out.type_section_storage.type_section_begin = typesec.types.cbegin();
                out.type_section_storage.type_section_end = typesec.types.cend();
                // [parser Core 3 context] retained for the entire runtime-module lifetime.
                // [safe                 ] no cursor or allocation is changed by this borrow.
                // ^^ core3_context_ptr is used by compiler validation for exact aggregate subtyping.
                out.type_section_storage.core3_context_ptr =
                    typesec.core3_context.records.empty() ? nullptr : ::std::addressof(typesec.core3_context);
            }
            // The qualified feature pack uses the same owned-signature carrier as runtime storage.
            // Other parser packs can leave this optional side table absent; never reinterpret a
            // differently instantiated template record as a runtime signature.
            if constexpr(::std::same_as<decltype(typesec.owned_signatures.cbegin()),
                ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t const*>)
            {
                if(!typesec.owned_signatures.empty() && typesec.owned_signatures.size() != typesec.types.size()) [[unlikely]]
                { ::fast_io::fast_terminate(); }
                if(typesec.owned_signatures.empty())
                {
                    out.type_section_storage.owned_signature_begin = nullptr;
                    out.type_section_storage.owned_signature_end = nullptr;
                }
                else
                {
                    // [owned signature 0 ... owned signature N] one-past
                    // [safe                               ] unsafe (not dereferenced)
                    // ^^ owned_signature_begin borrows parser storage retained by this runtime module.
                    out.type_section_storage.owned_signature_begin = typesec.owned_signatures.cbegin();
                    // [owned signature 0 ... owned signature N] one-past
                    // [safe                               ] unsafe (not dereferenced)
                    //                                       ^^ owned_signature_end is a bounded one-past pointer.
                    out.type_section_storage.owned_signature_end = typesec.owned_signatures.cend();
                }
            }
            else
            {
                if(!typesec.owned_signatures.empty()) [[unlikely]] { ::fast_io::fast_terminate(); }
                out.type_section_storage.owned_signature_begin = nullptr;
                out.type_section_storage.owned_signature_end = nullptr;
            }

            // Fail-safe validation for partially-degraded parsers:
            // - wasm1.0 MVP forbids multiple tables/memories.
            // - wasm1.0 MVP forbids multi-value results.
            if constexpr(!::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_table<Fs...>())
            {
                auto const imported_table_count{importsec.importdesc.index_unchecked(importdesc_table_index).size()};
                auto const local_table_count{tablesec.tables.size()};
                if(imported_table_count > 1uz || local_table_count > 1uz || (imported_table_count == 1uz && local_table_count == 1uz)) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"initializer: In module \"",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        world.current_module(),
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"\", wasm1.0 forbids multiple tables (imported_tables=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        imported_table_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8", local_tables=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        local_table_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8").\n\n",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }

            if(!::uwvm2::parser::wasm::standard::wasm1::features::multi_memory_enabled(fs_para))
            {
                auto const imported_memory_count{importsec.importdesc.index_unchecked(importdesc_memory_index).size()};
                auto const local_memory_count{memorysec.memories.size()};
                if(imported_memory_count > 1uz || local_memory_count > 1uz || (imported_memory_count == 1uz && local_memory_count == 1uz)) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"initializer: In module \"",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        world.current_module(),
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"\", multiple memories require --wasm-feature-enable-multi-memory (imported_memories=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        imported_memory_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8", local_memories=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        local_memory_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8").\n\n",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }

            // Core 3 limits standard-page memories to 2^(address_bits - 16) pages.
            // Validate the full declaration before imposing native resource limits.
            {
                using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;

                auto const validate_memory_limits{
                    [&]<typename MemoryType>(MemoryType const& memory, ::uwvm2::utils::container::u8string_view kind, ::std::size_t index) constexpr noexcept
                    {
                        auto const& limits{memory.limits};
                        auto const max_pages{::std::uint_least64_t{1u} << (wasm_memory_is_address64(memory) ? 48u : 16u)};
                        if(limits.present_max && limits.max < limits.min) [[unlikely]]
                        {
#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
                            // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                world.current_module(),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", invalid memory limits (",
                                                kind,
                                                u8"_idx=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                index,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", min=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                limits.min,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", max=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                limits.max,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", present_max=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                limits.present_max,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8").\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#else
                            // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
                            {
                                auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                                ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                                    ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                                auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    world.current_module(),
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", invalid memory limits (",
                                                    kind,
                                                    u8"_idx=",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    index);
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8", min=",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    limits.min,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8", max=",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    limits.max);
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8", present_max=",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    limits.present_max,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8").\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            }
#endif
                            ::fast_io::fast_terminate();
                        }

                        if(limits.min > max_pages || (limits.present_max && limits.max > max_pages)) [[unlikely]]
                        {
#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
                            // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                world.current_module(),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", memory limit exceeds ", max_pages, u8" pages (",
                                                kind,
                                                u8"_idx=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                index,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", min=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                limits.min,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", max=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                limits.max,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", present_max=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                limits.present_max,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8").\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#else
                            // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
                            {
                                auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                                ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                                    ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                                auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"");
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    world.current_module(),
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", memory limit exceeds ", max_pages, u8" pages (",
                                                    kind,
                                                    u8"_idx=",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    index);
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8", min=",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    limits.min,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8", max=",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    limits.max);
                                ::fast_io::io::perr(u8log_output_ul,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8", present_max=",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    limits.present_max,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8").\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            }
#endif
                            ::fast_io::fast_terminate();
                        }
                    }};

                // Validate imported memory limits.
                {
                    ::std::size_t idx{};
                    for(auto const import_ptr: importsec.importdesc.index_unchecked(importdesc_memory_index))
                    {
                        if(import_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        if(import_ptr->imports.type != external_types::memory) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        validate_memory_limits(import_ptr->imports.storage.memory, u8"import", idx);
                        ++idx;
                    }
                }

                // Validate local-defined memory limits.
                {
                    ::std::size_t idx{};
                    for(auto const& mem: memorysec.memories)
                    {
                        validate_memory_limits(mem, u8"memory", idx);
                        ++idx;
                    }
                }
            }

            if constexpr(!::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_result_vector<Fs...>())
            {
                ::std::size_t type_idx{};
                for(auto const& ty: typesec.types)
                {
                    auto const res_count{safe_ptr_range_size(ty.result.begin, ty.result.end)};
                    if(res_count > 1uz) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In module \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            world.current_module(),
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\", wasm1.0 forbids multi-value results (type_idx=",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            type_idx,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8", results=",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            res_count,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8").\n\n",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }
                    ++type_idx;
                }
            }

            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { verbose_module_info_in_context(world, u8"Init: imported descriptors. "); }

            auto module_memory_limit_override{world.find_memory_limit(world.current_module())};
            auto const local_defined_memory_count{memorysec.memories.size()};

            if(module_memory_limit_override != nullptr) [[unlikely]]
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_module_info_in_context(world, u8"Init: found local-defined memory limit overrides (apply_to_all=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        module_memory_limit_override->apply_to_all_memories,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8", explicit_memory_overrides=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        module_memory_limit_override->local_defined_memory_limits.size(),
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8", local_defined_memories=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        local_defined_memory_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"). ");
                }

                if(local_defined_memory_count == 0uz) [[unlikely]] { fatal_runtime_memory_limit_no_local_defined_memory_in_context(world, *module_memory_limit_override); }

                for(auto const& [memory_index, configured_limits]: module_memory_limit_override->local_defined_memory_limits)
                {
                    if(memory_index >= local_defined_memory_count) [[unlikely]]
                    {
                        fatal_runtime_memory_limit_out_of_range_in_context(world, memory_index, local_defined_memory_count, configured_limits);
                    }
                }
            }

            // imported
            {
                auto const& imported_funcs{importsec.importdesc.index_unchecked(importdesc_func_index)};
                auto import_reset_rules{world.find_import_reset(world.current_module())};
                if(import_reset_rules != nullptr) [[unlikely]]
                {
                    out.rewritten_import_vec_storage.reserve(importsec.imports.size());
                    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        verbose_module_info_in_context(world, u8"Init: found import reset rules (rules=",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            import_reset_rules->size(),
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"). ");
                    }
                }

                details::check_reserve_limit_in_context(world, u8"imported_functions", imported_funcs.size(), world.limits().max_imported_functions);
                out.imported_function_vec_storage.reserve(imported_funcs.size());
                for(auto const import_ptr: imported_funcs)
                {
                    ::uwvm2::uwvm::runtime::storage::imported_function_storage_t rec{};
                    rec.import_type_ptr = apply_configured_import_reset_to_import_type_in_context(world, import_ptr, out, import_reset_rules);
                    out.imported_function_vec_storage.push_back_unchecked(::std::move(rec));
                }

                auto const& imported_tables{importsec.importdesc.index_unchecked(importdesc_table_index)};
                details::check_reserve_limit_in_context(world, u8"imported_tables", imported_tables.size(), world.limits().max_imported_tables);
                out.imported_table_vec_storage.reserve(imported_tables.size());
                for(auto const import_ptr: imported_tables)
                {
                    ::uwvm2::uwvm::runtime::storage::imported_table_storage_t rec{};
                    rec.import_type_ptr = apply_configured_import_reset_to_import_type_in_context(world, import_ptr, out, import_reset_rules);
                    out.imported_table_vec_storage.push_back_unchecked(::std::move(rec));
                }

                auto const& imported_memories{importsec.importdesc.index_unchecked(importdesc_memory_index)};
                details::check_reserve_limit_in_context(world, u8"imported_memories", imported_memories.size(), world.limits().max_imported_memories);
                out.imported_memory_vec_storage.reserve(imported_memories.size());
                for(auto const import_ptr: imported_memories)
                {
                    ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t rec{};
                    rec.import_type_ptr = apply_configured_import_reset_to_import_type_in_context(world, import_ptr, out, import_reset_rules);
                    rec.effective_limits = parser_memory_limits_to_runtime_limits(rec.import_type_ptr->imports.storage.memory.limits);
                    out.imported_memory_vec_storage.push_back_unchecked(::std::move(rec));
                }

                auto const& imported_globals{importsec.importdesc.index_unchecked(importdesc_global_index)};
                details::check_reserve_limit_in_context(world, u8"imported_globals", imported_globals.size(), world.limits().max_imported_globals);
                out.imported_global_vec_storage.reserve(imported_globals.size());
                for(auto const import_ptr: imported_globals)
                {
                    ::uwvm2::uwvm::runtime::storage::imported_global_storage_t rec{};
                    rec.import_type_ptr = apply_configured_import_reset_to_import_type_in_context(world, import_ptr, out, import_reset_rules);
                    out.imported_global_vec_storage.push_back_unchecked(::std::move(rec));
                }
                auto const& imported_tags{importsec.importdesc.index_unchecked(importdesc_tag_index)};
                details::check_reserve_limit_in_context(world, u8"imported_tags", imported_tags.size(), world.limits().max_imported_tags);
                out.imported_tag_vec_storage.reserve(imported_tags.size());
                for(auto const original : imported_tags)
                {
                    ::uwvm2::uwvm::runtime::storage::imported_tag_storage_t tag{};
                    // [stable parsed import record] reset rules copy names into retained runtime-owned storage.
                    // ^^ import_type_ptr: no pointer into guest linear memory is retained.
                    tag.import_type_ptr = apply_configured_import_reset_to_import_type_in_context(world, original, out, import_reset_rules);
                    auto const index{tag.import_type_ptr->imports.storage.tag_type_index};
                    if(index >= typesec.types.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const& type{typesec.types.index_unchecked(index)};
                    if(type.result.begin != type.result.end) [[unlikely]] { ::fast_io::fast_terminate(); }
                    // [validated stable type allocation] index was checked before taking this address.
                    // ^^ function_type_ptr
                    tag.function_type_ptr = ::std::addressof(type);
                    out.imported_tag_vec_storage.push_back(tag);
                }
            }

            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { verbose_module_info_in_context(world, u8"Init: local functions and code. "); }

            // local defined function + code
            {
                auto const defined_func_count{funcsec.funcs.size()};
                auto const defined_code_count{codesec.codes.size()};
                auto const imported_func_count{importsec.importdesc.index_unchecked(importdesc_func_index).size()};
                if(defined_func_count != codesec.codes.size()) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"initializer: In module \"",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        world.current_module(),
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"\", function section count does not match code section count (funcs=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        defined_func_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8", codes=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        codesec.codes.size(),
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8").\n\n",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }

                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_module_info_in_context(world, u8"Init: local functions/code reserve begin (imported_funcs=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        imported_func_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8", local_funcs=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        defined_func_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8", code_bodies=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        defined_code_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8", type_count=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        typesec.types.size(),
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"). ");
                }

                details::check_reserve_limit_in_context(world, u8"local_defined_functions", defined_func_count, world.limits().max_local_defined_functions);
                details::check_reserve_limit_in_context(world, u8"local_defined_codes", defined_func_count, world.limits().max_local_defined_codes);
                out.local_defined_function_vec_storage.reserve(defined_func_count);
                out.local_defined_code_vec_storage.reserve(defined_func_count);

                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_module_info_in_context(world, u8"Init: local functions/code reserve done (function_records=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        defined_func_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8", code_records=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        defined_func_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"). ");
                }

                for(::std::size_t i{}; i != defined_func_count; ++i)
                {
                    auto const type_idx{static_cast<::std::size_t>(funcsec.funcs.index_unchecked(i))};
                    if(type_idx >= typesec.types.size()) [[unlikely]]
                    {
#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
                        // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In module \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            world.current_module(),
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\", function section references a type index that is out of bounds (func_index=",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            i,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8", type_idx=",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            type_idx,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8", type_count=",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            typesec.types.size(),
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8").\n\n",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#else
                        // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
                        {
                            auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                            ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                                ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                            auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                            ::fast_io::io::perr(u8log_output_ul,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"");
                            ::fast_io::io::perr(u8log_output_ul,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                world.current_module(),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", function section references a type index that is out of bounds (func_index=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                i,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", type_idx=");
                            ::fast_io::io::perr(u8log_output_ul,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                type_idx,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", type_count=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                typesec.types.size(),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8").\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        }
#endif
                        ::fast_io::fast_terminate();
                    }

                    auto const& function_type{typesec.types.index_unchecked(type_idx)};
                    auto const& code_type{codesec.codes.index_unchecked(i)};

                    if constexpr(false)
                    {
                        // Too verbose

                        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                        {
                            auto const param_count{safe_ptr_range_size(function_type.parameter.begin, function_type.parameter.end)};
                            auto const result_count{safe_ptr_range_size(function_type.result.begin, function_type.result.end)};
                            auto const code_body_bytes{safe_ptr_range_size(code_type.body.code_begin, code_type.body.code_end)};
                            auto const expr_bytes{safe_ptr_range_size(code_type.body.expr_begin, code_type.body.code_end)};
                            verbose_module_info_in_context(world, u8"Init: bind local function/code (local_func_idx=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                                i,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", wasm_func_idx=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                                imported_func_count + i,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", type_idx=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                                type_idx,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", params=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                                param_count,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", results=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                                result_count,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", local_decl_groups=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                                code_type.locals.size(),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", local_values=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                                code_type.all_local_count,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", code_body_bytes=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                                code_body_bytes,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", expr_bytes=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                                expr_bytes,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"). ");
                        }
                    }

                    ::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t f{};
                    f.function_type_ptr = ::std::addressof(function_type);
                    f.wasm_code_ptr = ::std::addressof(code_type);
                    out.local_defined_function_vec_storage.push_back_unchecked(f);

                    ::uwvm2::uwvm::runtime::storage::local_defined_code_storage_t c{};
                    c.code_type_ptr = ::std::addressof(code_type);
                    c.func_ptr = ::std::addressof(out.local_defined_function_vec_storage.back());
                    out.local_defined_code_vec_storage.push_back_unchecked(c);
                }
            }

            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { verbose_module_info_in_context(world, u8"Init: local tables. "); }

            // local defined table
            {
                auto const local_table_count{tablesec.tables.size()};
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_module_info_in_context(world, u8"Init: local tables reserve begin (local_tables=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        local_table_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8", imported_tables=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        out.imported_table_vec_storage.size(),
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"). ");
                }

                details::check_reserve_limit_in_context(world, u8"local_defined_tables", local_table_count, world.limits().max_local_defined_tables);
                out.local_defined_table_vec_storage.reserve(local_table_count);

                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_module_info_in_context(world, u8"Init: local tables reserve done (table_records=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        local_table_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"). ");
                }

                ::std::size_t table_idx{};
                for(auto const& table_type: tablesec.tables)
                {
                    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        verbose_module_info_in_context(world, u8"Init: resize local table begin (table_idx=",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                            table_idx,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8", table_type=\"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                            ::uwvm2::parser::wasm::standard::wasm1p1::features::section_details(table_type),
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\", initial_elems=",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                            static_cast<::std::size_t>(table_type.limits.min),
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"). ");
                    }

                    ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t rec{};
                    // table_type is the current element of the live parsed tables vector.
                    // [table_type] ... (tables.end)
                    // [safe      ]
                    //  ^^ rec.table_type_ptr borrows this object, without advancing a cursor.
                    rec.table_type_ptr = ::std::addressof(table_type);
                    if(!tablesec.initializers.empty())
                    {
                        if(tablesec.initializers.size() != tablesec.tables.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                        // [initializers[0] ... initializers[table_idx] ...] (initializers.end)
                        // [safe                                         ]
                        //                       ^^ initializer_expr; table_idx is this loop's checked local-table index.
                        rec.initializer_expr = ::std::addressof(tablesec.initializers.index_unchecked(table_idx));
                    }
                    rec.owner_module_rt_ptr = ::std::addressof(out);
                    // Preserve the declared u64 count until its complete byte allocation
                    // is representable on this host. No narrowing or resize precedes it.
                    constexpr auto max_native_elements{(::std::numeric_limits<::std::size_t>::max)() /
                        sizeof(::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t)};
                    if(table_type.limits.min > max_native_elements) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            u8"uwvm: [fatal] initializer: table minimum exceeds the host allocation limit.\n\n");
                        ::fast_io::fast_terminate();
                    }
                    if constexpr(Purpose == initialization_purpose::ordinary)
                    { rec.elems.resize(static_cast<::std::size_t>(table_type.limits.min)); }
                    // Restored extent is installed once from the checked saved state;
                    // empty private metadata is never a published non-null table filler.
                    if(table_type.reftype == ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type::externref)
                    {
                        for(auto& elem: rec.elems)
                        {
                            elem.storage.extern_ptr = nullptr;
                            elem.type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::extern_ref;
                        }
                    }
                    else if(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(table_type.reftype) == 0x69u)
                    {
                        for(auto& elem: rec.elems)
                        {
                            elem.storage.extern_ptr = nullptr;
                            elem.type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::exn_ref;
                        }
                    }
                    out.local_defined_table_vec_storage.push_back_unchecked(::std::move(rec));

                    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        verbose_module_info_in_context(world, u8"Init: resize local table done (table_idx=",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                            table_idx,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8", stored_elems=",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                            out.local_defined_table_vec_storage.back().elems.size(),
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"). ");
                    }

                    ++table_idx;
                }
            }

#if defined(UWVM_RUNTIME_LLVM_JIT)
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                verbose_module_info_in_context(world, u8"Init: resize LLVM JIT call_indirect table views begin (imported_tables=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    out.imported_table_vec_storage.size(),
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8", local_tables=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    out.local_defined_table_vec_storage.size(),
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"). ");
            }
            out.llvm_jit_call_indirect_table_views.resize(out.imported_table_vec_storage.size() + out.local_defined_table_vec_storage.size());
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                verbose_module_info_in_context(world, u8"Init: resize LLVM JIT call_indirect table views done (views=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                    out.llvm_jit_call_indirect_table_views.size(),
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"). ");
            }
#endif

            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { verbose_module_info_in_context(world, u8"Init: local memories. "); }

            // local defined memory
            {
                auto const local_memory_count{memorysec.memories.size()};
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_module_info_in_context(world, u8"Init: local memories reserve begin (local_memories=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        local_memory_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8", imported_memories=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        out.imported_memory_vec_storage.size(),
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8", backend=\"",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        ::uwvm2::object::memory::linear::native_memory_t::name,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"\", backend_can_mmap=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        ::uwvm2::object::memory::linear::native_memory_t::can_mmap,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8", backend_multi_thread=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        ::uwvm2::object::memory::linear::native_memory_t::support_multi_thread,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"). ");
                }

                details::check_reserve_limit_in_context(world, u8"local_defined_memories", local_memory_count, world.limits().max_local_defined_memories);
                out.local_defined_memory_vec_storage.reserve(local_memory_count);

                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_module_info_in_context(world, u8"Init: local memories reserve done (memory_records=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_CYAN),
                                        local_memory_count,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"). ");
                }

                ::std::size_t memory_idx{};
                for(auto const& memory_type: memorysec.memories)
                {
                    auto const local_defined_memory_limit_override{resolve_configured_local_defined_memory_limit(module_memory_limit_override, memory_idx)};
                    auto const effective_limits{
                        resolve_effective_runtime_memory_limits_in_context(world, memory_type.limits, local_defined_memory_limit_override.limits, u8"local-defined", memory_idx)};

                    emit_local_memory_init_verbose_in_context(world, u8"construct record begin",
                                                   memory_idx,
                                                   memory_type.limits,
                                                   effective_limits,
                                                   default_linear_memory_page_size_bytes());

                    out.local_defined_memory_vec_storage.emplace_back();
                    auto& rec{out.local_defined_memory_vec_storage.back()};
                    rec.memory_type_ptr = ::std::addressof(memory_type);
                    rec.effective_limits = effective_limits;

                    auto const runtime_page_size_bytes{linear_memory_page_size_bytes_from_log2(rec.memory.custom_page_size_log2)};
                    emit_local_memory_init_verbose_in_context(world, u8"construct record done", memory_idx, memory_type.limits, rec.effective_limits, runtime_page_size_bytes);

                    if(local_defined_memory_limit_override.limits != nullptr) [[unlikely]]
                    {
                        emit_runtime_memory_limit_override_verbose_in_context(world, memory_idx,
                                                                   local_defined_memory_limit_override.from_all,
                                                                   memory_type.limits,
                                                                   rec.effective_limits);
                    }

                    emit_local_memory_init_verbose_in_context(world, u8"init_by_page_count begin", memory_idx, memory_type.limits, rec.effective_limits, runtime_page_size_bytes);
                    // Memory ownership includes imported memories. Native mmap instances use this
                    // index for fault diagnostics; 32-bit reservations also honor the effective maximum.
                    if constexpr(Purpose == initialization_purpose::ordinary)
                    {
                    initialize_native_memory(rec.memory, rec.effective_limits, out.imported_memory_vec_storage.size() + memory_idx,
                                             wasm_memory_is_shared(memory_type), wasm_memory_is_address64(memory_type));
                    }
                    // Restoration batch owns the one exact saved-size allocation after quotas.
                    emit_local_memory_init_verbose_in_context(world, u8"init_by_page_count done", memory_idx, memory_type.limits, rec.effective_limits, runtime_page_size_bytes);

                    ++memory_idx;
                }
            }

            // Declared refs for wasm1.1 `ref.func` validation. This mirrors the standard validator's export/global/element collection.
            {
                out.declared_ref_funcidx_vec_storage.clear();
                auto append_declared_ref{[&](::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 func_idx)
                {
                    for(auto const declared_idx: out.declared_ref_funcidx_vec_storage)
                    {
                        if(declared_idx == func_idx) { return; }
                    }
                    out.declared_ref_funcidx_vec_storage.push_back(func_idx);
                }};
                auto collect_const_expr_refs{[&](auto const& expr)
                {
                    for(auto const& op: expr.opcodes)
                    {
                        if(op.opcode == static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xD2u))
                        {
                            append_declared_ref(op.storage.ref_func_idx);
                        }
                    }
                }};

                for(auto const& exp: exportsec.exports)
                {
                    if(exp.exports.type == ::uwvm2::parser::wasm::standard::wasm1::type::external_types::func)
                    {
                        append_declared_ref(exp.exports.storage.func_idx);
                    }
                }

                for(auto const& local_global: globalsec.local_globals) { collect_const_expr_refs(local_global.expr); }
                for(auto const& expr: tablesec.initializers) { collect_const_expr_refs(expr); }

                for(auto const& elem: elemsec.elems)
                {
                    auto const& elem_segment{elem.storage.segment};
                    for(auto const func_idx: elem_segment.vec_funcidx) { append_declared_ref(func_idx); }
                    for(auto const& expr: elem_segment.vec_expr) { collect_const_expr_refs(expr); }
                }
            }

            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { verbose_module_info_in_context(world, u8"Init: local globals. "); }

            // local defined global
            {
                details::check_reserve_limit_in_context(world, u8"local_defined_globals", globalsec.local_globals.size(), world.limits().max_local_defined_globals);
                out.local_defined_global_vec_storage.reserve(globalsec.local_globals.size());
                for(auto const& local_global: globalsec.local_globals)
                {
                    ::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t rec{};
                    rec.global_type_ptr = ::std::addressof(local_global.global);
                    rec.local_global_type_ptr = ::std::addressof(local_global);
                    rec.global.kind = details::to_object_global_type_in_context(world, local_global.global.type);
                    rec.global.is_mutable = local_global.global.is_mutable;
                    // The store is owned by `out` and remains live for every local global;
                    // numeric reads do not consult this borrowed cold reference hook.
                    rec.global.ref_lease_store = out.gc_store.get();

                    if constexpr(Purpose == initialization_purpose::unpublished_restore)
                    {
                        // [private actual declaration slot] no original expression evaluates.
                        // [safe] zero carrier remains UNINITIALIZED until exact graph fixup.
                        out.local_defined_global_vec_storage.push_back_unchecked(::std::move(rec));
                        continue;
                    }

                    if(local_global.expr.opcodes.size() > 1uz ||
                       (local_global.expr.opcodes.size() == 1uz &&
                        local_global.expr.opcodes.front_unchecked().opcode ==
                            static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xfbu)))
                    {
                        // Core 3 GC constructors and conversions may be a single
                        // opcode (for example struct.new_default). Their store is
                        // not ready until the module has reached its stable
                        // runtime address. The nonempty check above protects
                        // front_unchecked; no expression pointer is advanced.
                        // Leave the record uninitialized until dependencies have
                        // been linked and finalized.
                        out.local_defined_global_vec_storage.push_back_unchecked(::std::move(rec));
                        continue;
                    }
                    if(local_global.expr.opcodes.size() != 1uz) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"initializer: wasm1.0 global initializer const expr must contain exactly one opcode; got ",
                                            local_global.expr.opcodes.size(),
                                            u8".\n\n",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    auto const& op{local_global.expr.opcodes.front_unchecked()};
                    /// @warning Extension point: new const-expression opcodes must be evaluated here before defined globals are usable at runtime.
                    switch(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(op.opcode))
                    {
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                            ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const):
                        {
                            rec.global.storage.i32 = op.storage.i32;
                            rec.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                            ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i64_const):
                        {
                            rec.global.storage.i64 = op.storage.i64;
                            rec.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                            ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f32_const):
                        {
                            ::std::memcpy(::std::addressof(rec.global.storage.f32), ::std::addressof(op.storage.f32), sizeof(rec.global.storage.f32));
                            rec.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                            ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f64_const):
                        {
                            ::std::memcpy(::std::addressof(rec.global.storage.f64), ::std::addressof(op.storage.f64), sizeof(rec.global.storage.f64));
                            rec.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xD0u):
                        {
                            ::uwvm2::object::global::wasm_global_ref_t ref{};
                            ref.storage.ptr = nullptr;
                            ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_null;
                            rec.global.storage.ref = ref;
                            rec.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xD2u):
                        {
                            ::uwvm2::object::global::wasm_global_ref_t ref{};
                            ref.storage.func_idx = op.storage.ref_func_idx;
                            ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_func;
                            rec.global.storage.ref = ref;
                            rec.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xFDu):
                        {
                            rec.global.storage.v128 = op.storage.v128;
                            rec.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
                            break;
                        }
                        case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                            ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get):
                        {
                            // Requires import-linking; evaluated in `finalize_wasm1_globals_after_linking()`.
                            rec.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::uninitialized;
                            break;
                        }
                        [[unlikely]] default:
                        {
                            ::fast_io::io::perr(
                                ::uwvm2::uwvm::io::u8log_output,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"initializer: wasm1.0 global initializer const expr encountered an invalid instruction: ",
                                ::fast_io::mnp::hex0x<true>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(op.opcode)),
                                u8".\n\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }
                    }

                    out.local_defined_global_vec_storage.push_back_unchecked(::std::move(rec));
                }
            }

            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { verbose_module_info_in_context(world, u8"Init: element segments. "); }
            // element (wasm1: active segments)
            {
                details::check_reserve_limit_in_context(world, u8"local_defined_elements", elemsec.elems.size(), world.limits().max_local_defined_elements);
                out.local_defined_element_vec_storage.reserve(elemsec.elems.size());
                for(auto const& elem: elemsec.elems)
                {
                    auto const& elem_segment{elem.storage.segment};
                    ::uwvm2::uwvm::runtime::storage::local_defined_element_storage_t rec{};
                    rec.element_type_ptr = ::std::addressof(elem);
                    rec.element.table_idx = elem_segment.table_idx;
                    auto const funcidx_size{elem_segment.vec_funcidx.size()};
                    if(funcidx_size == 0uz)
                    {
                        rec.element.funcidx_begin = nullptr;
                        rec.element.funcidx_end = nullptr;
                    }
                    else
                    {
                        rec.element.funcidx_begin = elem_segment.vec_funcidx.data();
                        rec.element.funcidx_end = rec.element.funcidx_begin + funcidx_size;
                    }
                    rec.element.kind = elem_segment.active ? ::uwvm2::uwvm::runtime::storage::wasm_element_segment_kind::active
                                                           : ::uwvm2::uwvm::runtime::storage::wasm_element_segment_kind::passive;
                    if constexpr(Purpose == initialization_purpose::ordinary)
                    {
                        if(elem_segment.declarative) { ::uwvm2::uwvm::runtime::storage::drop_wasm_element_segment_payload(rec.element); }
                    }
                    if(elem_segment.active && Purpose == initialization_purpose::ordinary)
                    {
                        try_eval_wasm3_table_const_expr_offset(elem_segment.expr, rec.element.offset,
                            runtime_table_is_address64(out, safe_u32_to_size_t(rec.element.table_idx)));
                    }
                    out.local_defined_element_vec_storage.push_back_unchecked(::std::move(rec));
                }
            }

            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { verbose_module_info_in_context(world, u8"Init: data segments. "); }
            // data (wasm1: active segments)
            {
                details::check_reserve_limit_in_context(world, u8"local_defined_datas", datasec.datas.size(), world.limits().max_local_defined_datas);
                out.local_defined_data_vec_storage.reserve(datasec.datas.size());
                for(auto const& data: datasec.datas)
                {
                    auto const& data_segment{data.storage.segment};
                    ::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t rec{};
                    rec.data_type_ptr = ::std::addressof(data);
                    rec.data.kind = data_segment.active ? ::uwvm2::uwvm::runtime::storage::wasm_data_segment_kind::active
                                                        : ::uwvm2::uwvm::runtime::storage::wasm_data_segment_kind::passive;
                    rec.data.dropped = false;
                    rec.data.memory_idx = data_segment.memory_idx;
                    rec.data.byte_begin = reinterpret_cast<::std::byte const*>(data_segment.byte.begin);
                    rec.data.byte_end = reinterpret_cast<::std::byte const*>(data_segment.byte.end);
                    if(data_segment.active && Purpose == initialization_purpose::ordinary)
                    {
                        try_eval_wasm3_memory_const_expr_offset(data_segment.expr, rec.data.offset,
                            runtime_memory_is_address64(out, safe_u32_to_size_t(rec.data.memory_idx)));
                    }
                    out.local_defined_data_vec_storage.push_back_unchecked(::std::move(rec));
                }
            }
        }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline constexpr void initialize_from_binfmt_ver1_module_storage(
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
            ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& out) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            initialize_from_binfmt_ver1_module_storage_in_context(world, module_storage, fs_para, out);
        }

        inline void retain_wasm1_local_global_reference(::uwvm2::object::global::wasm_global_storage_t const& global) noexcept
        {
            if(global.kind != ::uwvm2::object::global::global_type::wasm_ref) { return; }
            auto const* store{static_cast<::uwvm2::uwvm::runtime::storage::gc_object_store const*>(global.ref_lease_store)};
            if(::uwvm2::uwvm::runtime::storage::uwvm2_gc_retain_reference(store,
                    ::std::addressof(global.storage.ref)) != ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                    u8"uwvm: [fatal] initializer: Invalid or expired global reference.\n\n");
                ::fast_io::fast_terminate();
            }
        }

        template <initialization_purpose Purpose>
        inline void ensure_wasm1_local_defined_global_initialized_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t& g) noexcept
        {
            using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;

            switch(g.init_state)
            {
                case ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized:
                {
                    return;
                }
                case ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initializing:
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"initializer: Global initialization encountered a circular dependency.\n\n",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
                case ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::uninitialized:
                {
                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }

            if(g.owner_module_rt_ptr == nullptr || g.local_global_type_ptr == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            g.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initializing;

            auto const& expr{g.local_global_type_ptr->expr};
            if(expr.opcodes.size() > 1uz ||
               (expr.opcodes.size() == 1uz &&
                (expr.opcodes.front_unchecked().opcode == static_cast<::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic>(0xfbu) ||
                 (expr.opcodes.front_unchecked().opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get &&
                  expr.opcodes.front_unchecked().storage.imported_global_idx >= g.owner_module_rt_ptr->imported_global_vec_storage.size()))))
            {
                auto const result{evaluate_wasm3_constant_expression_in_context(world, expr, *g.owner_module_rt_ptr)};
                if(result.kind != g.global.kind) [[unlikely]] { ::fast_io::fast_terminate(); }
                g.global.storage = result.storage; // Preserve the declared mutability of the destination global.
                retain_wasm1_local_global_reference(g.global);
                g.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
                return;
            }
            if(expr.opcodes.size() != 1uz) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"initializer: wasm1.0 global initializer const expr must contain exactly one opcode; got ",
                                    expr.opcodes.size(),
                                    u8".\n\n",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            auto const& op{expr.opcodes.front_unchecked()};
            switch(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(op.opcode))
            {
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                    ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const):
                {
                    g.global.storage.i32 = op.storage.i32;
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                    ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i64_const):
                {
                    g.global.storage.i64 = op.storage.i64;
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                    ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f32_const):
                {
                    ::std::memcpy(::std::addressof(g.global.storage.f32), ::std::addressof(op.storage.f32), sizeof(g.global.storage.f32));
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                    ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::f64_const):
                {
                    ::std::memcpy(::std::addressof(g.global.storage.f64), ::std::addressof(op.storage.f64), sizeof(g.global.storage.f64));
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xD0u):
                {
                    ::uwvm2::object::global::wasm_global_ref_t ref{};
                    ref.storage.ptr = nullptr;
                    ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_null;
                    g.global.storage.ref = ref;
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xD2u):
                {
                    ::uwvm2::object::global::wasm_global_ref_t ref{};
                    ref.storage.func_idx = op.storage.ref_func_idx;
                    ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_func;
                    g.global.storage.ref = ref;
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(0xFDu):
                {
                    g.global.storage.v128 = op.storage.v128;
                    break;
                }
                case static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(
                    ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get):
                {
                    auto const idx{static_cast<::std::size_t>(op.storage.imported_global_idx)};
                    auto const imported_count{g.owner_module_rt_ptr->imported_global_vec_storage.size()};
                    if(idx >= imported_count) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In wasm1.0, global initializer refers to an imported global index that is out of bounds: ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            idx,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8" >= ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            imported_count,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8".\n\n",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    auto const imported_global_ptr{::std::addressof(g.owner_module_rt_ptr->imported_global_vec_storage.index_unchecked(idx))};
                    if(imported_global_ptr->import_type_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    if(imported_global_ptr->import_type_ptr->imports.type != external_types::global) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    // wasm1.0: global initializers may only use `global.get` on imported immutable globals.
                    if(imported_global_ptr->import_type_ptr->imports.storage.global.is_mutable) [[unlikely]]
                    {
                        ::fast_io::io::perr(
                            ::uwvm2::uwvm::io::u8log_output,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"initializer: In wasm1.0, global initializers may only use `global.get` on imported immutable globals; got mutable global \"",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                            imported_global_ptr->import_type_ptr->module_name,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8".",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                            imported_global_ptr->import_type_ptr->extern_name,
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                            u8"\".\n\n",
                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    ::uwvm2::object::global::wasm_global_storage_t const* resolved_global{};
                    ::uwvm2::object::global::wasm_global_storage_t local_imported_scratch{};
                    try_resolve_wasm1_imported_global_value_in_context(world, imported_global_ptr, resolved_global, local_imported_scratch);

                    if(resolved_global == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    if(resolved_global->kind != g.global.kind) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In wasm1.0, global initializer type mismatch: expected ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            ::uwvm2::object::global::get_global_type_name(g.global.kind),
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8", got ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            ::uwvm2::object::global::get_global_type_name(resolved_global->kind),
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8".\n\n",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    switch(g.global.kind)
                    {
                        case ::uwvm2::object::global::global_type::wasm_i32:
                        {
                            g.global.storage.i32 = resolved_global->storage.i32;
                            break;
                        }
                        case ::uwvm2::object::global::global_type::wasm_i64:
                        {
                            g.global.storage.i64 = resolved_global->storage.i64;
                            break;
                        }
                        case ::uwvm2::object::global::global_type::wasm_f32:
                        {
                            ::std::memcpy(::std::addressof(g.global.storage.f32), ::std::addressof(resolved_global->storage.f32), sizeof(g.global.storage.f32));
                            break;
                        }
                        case ::uwvm2::object::global::global_type::wasm_f64:
                        {
                            ::std::memcpy(::std::addressof(g.global.storage.f64), ::std::addressof(resolved_global->storage.f64), sizeof(g.global.storage.f64));
                            break;
                        }
                        case ::uwvm2::object::global::global_type::wasm_v128:
                        {
                            g.global.storage.v128 = resolved_global->storage.v128;
                            break;
                        }
                        case ::uwvm2::object::global::global_type::wasm_ref:
                        {
                            g.global.storage.ref = resolved_global->storage.ref;
                            // A raw wasm_func index is meaningful only inside its defining module. All wasm-defined globals are
                            // canonicalized before dependency evaluation; an imported host value that still contains a bare index
                            // has no recoverable owner and must fail closed.
                            if(g.global.storage.ref.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_func) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        [[unlikely]] default:
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }
                    }

                    break;
                }
                [[unlikely]] default:
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"initializer: wasm1.0 global initializer const expr encountered an invalid instruction: ",
                                        ::fast_io::mnp::hex0x<true>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::op_basic_type>(op.opcode)),
                                        u8".\n\n",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }

            retain_wasm1_local_global_reference(g.global);
            g.init_state = ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized;
        }

        inline void ensure_wasm1_local_defined_global_initialized(::uwvm2::uwvm::runtime::storage::local_defined_global_storage_t& g) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            ensure_wasm1_local_defined_global_initialized_in_context(world, g);
        }

        template <initialization_purpose Purpose>
        inline constexpr void finalize_wasm1_globals_after_linking_in_context(initialization_context<Purpose>& world) noexcept
        {
            // First: attach owner pointers after modules have been moved into the runtime storage map.
            // Local tables and globals are constructed against a temporary `wasm_module_storage_t`, so their
            // back-pointers must be rebound once the final in-map module address is stable.
            for([[maybe_unused]] auto& [curr_module_name, curr_rt]: world.modules())
            {
                for(auto& table: curr_rt.local_defined_table_vec_storage) { table.owner_module_rt_ptr = ::std::addressof(curr_rt); }
                for(auto& g: curr_rt.local_defined_global_vec_storage) { g.owner_module_rt_ptr = ::std::addressof(curr_rt); }
            }

            // Direct ref.func initializers were parsed before the final runtime-module addresses existed. Convert their local
            // indices to stable imported/defined storage pointers before any other module can observe them through global.get.
            for([[maybe_unused]] auto& [curr_module_name, curr_rt]: world.modules())
            {
                for(auto& g: curr_rt.local_defined_global_vec_storage)
                {
                    if(g.init_state == ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized &&
                       g.global.kind == ::uwvm2::object::global::global_type::wasm_ref)
                    {
                        canonicalize_wasm1_funcref(curr_rt, g.global.storage.ref);
                        retain_wasm1_local_global_reference(g.global);
                    }
                }
            }

            // Second: evaluate all wasm1 global initializers (including those that use `global.get`).
            for([[maybe_unused]] auto& [curr_module_name, curr_rt]: world.modules())
            {
                ::std::size_t globals_total{};
                ::std::size_t globals_need_eval{};
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_info(u8"initializer: Finalize globals for module \"",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"\". ");
                    globals_total = curr_rt.local_defined_global_vec_storage.size();
                    for(auto const& g: curr_rt.local_defined_global_vec_storage)
                    {
                        if(g.init_state != ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized) { ++globals_need_eval; }
                    }
                }
                for(auto& g: curr_rt.local_defined_global_vec_storage) { ensure_wasm1_local_defined_global_initialized_in_context(world, g); }
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_info(u8"initializer: Finalize globals summary for module \"",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"\": total=",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 globals_total,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8", evaluated=",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 globals_need_eval,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8". ");
                }
            }
        }

        inline constexpr void finalize_wasm1_globals_after_linking() noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            finalize_wasm1_globals_after_linking_in_context(world);
        }

        template <initialization_purpose Purpose>
        inline constexpr void finalize_wasm1_offsets_after_linking_in_context(initialization_context<Purpose>& world) noexcept
        {
            for([[maybe_unused]] auto& [curr_module_name, curr_rt]: world.modules())
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_info(u8"initializer: Finalize offsets for module \"",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"\". ");
                }

                for(auto& elem: curr_rt.local_defined_element_vec_storage)
                {
                    if(elem.element_type_ptr == nullptr) [[unlikely]] { continue; }
                    auto const& expr{elem.element_type_ptr->storage.segment.expr};
                    if(expr.opcodes.size() > 1uz ||
                       (expr.opcodes.size() == 1uz &&
                        expr.opcodes.front_unchecked().opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get))
                    {
                        try_eval_wasm3_table_const_expr_offset_after_linking_in_context(world, expr, curr_rt, elem.element.offset,
                            runtime_table_is_address64(curr_rt, safe_u32_to_size_t(elem.element.table_idx)));
                    }
                }

                for(auto& data: curr_rt.local_defined_data_vec_storage)
                {
                    if(data.data_type_ptr == nullptr) [[unlikely]] { continue; }
                    auto const& expr{data.data_type_ptr->storage.segment.expr};
                    if(expr.opcodes.size() > 1uz ||
                       (expr.opcodes.size() == 1uz &&
                        expr.opcodes.front_unchecked().opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get))
                    {
                        try_eval_wasm3_memory_const_expr_offset_after_linking_in_context(world, expr, curr_rt, data.data.offset,
                            runtime_memory_is_address64(curr_rt, safe_u32_to_size_t(data.data.memory_idx)));
                    }
                }

                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_info(u8"initializer: Finalize offsets summary for module \"",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"\": segments(elem/data)=",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_rt.local_defined_element_vec_storage.size(),
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_rt.local_defined_data_vec_storage.size(),
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8". ");
                }
            }
        }

        inline constexpr void finalize_wasm1_offsets_after_linking() noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            finalize_wasm1_offsets_after_linking_in_context(world);
        }

        inline constexpr ::std::size_t safe_u32_to_size_t(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 v) noexcept
        {
            constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
            constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
            if constexpr(size_t_max < wasm_u32_max)
            {
                if(v > size_t_max) [[unlikely]]
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }

            return static_cast<::std::size_t>(v);
        }

        inline constexpr ::std::size_t safe_u64_to_size_t(::std::uint_least64_t v) noexcept
        {
            constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
            if(v > size_t_max) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            return static_cast<::std::size_t>(v);
        }

        template <typename NativeMemory>
        inline constexpr ::std::size_t get_native_memory_length_bytes(NativeMemory const& memory) noexcept
        {
            // All supported backends expose `get_page_size()` and `custom_page_size_log2`.
            auto const page_count{memory.get_page_size()};
            auto const log2{static_cast<::std::size_t>(memory.custom_page_size_log2)};

            // Prevent UB on oversized shifts and prevent wraparound on multiplication.
            if(log2 >= ::std::numeric_limits<::std::size_t>::digits) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            auto const page_size_bytes{static_cast<::std::size_t>(1uz) << log2};
            if(page_count > ::std::numeric_limits<::std::size_t>::max() / page_size_bytes) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            return page_count * page_size_bytes;
        }

        // Active-segment writes may target a memory imported from another live
        // instance. A moving allocator must remain pinned from length/base
        // acquisition through the final copy; mmap retains a stable reservation.
        // This is an instantiation-only helper, never a guest load/store path.
        template<typename Memory, typename Function>
        inline constexpr void with_native_initialization_memory(Memory& memory, Function&& function) noexcept
        {
            if constexpr(Memory::can_mmap)
            { function(memory.memory_begin, memory.memory_length_p->load(::std::memory_order_acquire)); }
            else if constexpr(Memory::support_multi_thread)
            {
#if __cpp_lib_atomic_wait >= 201907L
                ::uwvm2::object::memory::linear::memory_operation_guard_t pin{memory.growing_flag_p, memory.active_ops_p};
                function(memory.memory_begin, memory.memory_length);
#else
                static_assert(!Memory::support_multi_thread);
#endif
            }
            else { function(memory.memory_begin, memory.memory_length); }
        }

        inline constexpr void refresh_active_element_call_indirect_view(
            ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t* target_table,
            ::std::size_t offset, ::std::size_t element_count) noexcept
        {
#if defined(UWVM_RUNTIME_LLVM_JIT)
            // The initializer already resolved the actual target and proved
            // [offset, offset + element_count) inside its live element vector.
            // No pointer is advanced here. Each alias is refreshed by the
            // existing exact-range runtime bridge, without reallocating views.
            if(auto const refresh{::uwvm2::uwvm::runtime::storage::llvm_jit_table_refresh_hook}; refresh != nullptr)
            {
                refresh(target_table, ::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_mutation_kind::init,
                    offset, element_count);
            }
#else
            static_cast<void>(target_table); static_cast<void>(offset); static_cast<void>(element_count);
#endif
        }

        template <initialization_purpose Purpose>
        inline constexpr void apply_wasm1_active_element_and_data_segments_for_module_in_context(initialization_context<Purpose>& world, 
            ::uwvm2::utils::container::u8string_view curr_module_name,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& curr_rt) noexcept
        {
            ::uwvm2::runtime::gc::scoped_gc_root_graph_administration gc_administration{};
            curr_rt.gc_collection_phase.publish_active_segments(false);
            using table_elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;

            // Keep segment validation, writes and implicit drops local to one
            // instance, so its start can run before the next instance's writes.
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_info(u8"initializer: Apply active elem/data segments for module \"",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"\". ");
                }

                ::std::size_t elem_active_applied{};
                ::std::size_t data_active_applied{};

                materialize_wasm1p1_element_expr_payloads_in_context(world, curr_rt);

                // element (wasm1: active segments)
                for(auto& elem_seg: curr_rt.local_defined_element_vec_storage)
                {
                    auto& elem{elem_seg.element};
                    if(elem.kind != ::uwvm2::uwvm::runtime::storage::wasm_element_segment_kind::active || ::uwvm2::uwvm::runtime::storage::wasm_element_segment_is_dropped(elem)) { continue; }
                    ++elem_active_applied;

                    if(elem_seg.element_type_ptr != nullptr)
                    {
                        auto const& expr{elem_seg.element_type_ptr->storage.segment.expr};
                        if(expr.opcodes.size() > 1uz ||
                           (expr.opcodes.size() == 1uz &&
                            expr.opcodes.front_unchecked().opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get))
                        {
                            try_eval_wasm3_table_const_expr_offset_after_linking_in_context(world, expr, curr_rt, elem.offset,
                                runtime_table_is_address64(curr_rt, safe_u32_to_size_t(elem.table_idx)));
                        }
                    }

                    auto const table_idx{safe_u32_to_size_t(elem.table_idx)};
                    auto const imported_table_count{curr_rt.imported_table_vec_storage.size()};

                    ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t* target_table{};
                    if(table_idx < imported_table_count)
                    {
                        auto const imported_table_ptr{::std::addressof(curr_rt.imported_table_vec_storage.index_unchecked(table_idx))};
                        if(!maybe_resolve_wasm1_imported_table_defined_in_context(world, imported_table_ptr, target_table) || target_table == nullptr) [[unlikely]]
                        {
                            if(imported_table_ptr == nullptr || imported_table_ptr->import_type_ptr == nullptr) [[unlikely]]
                            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                ::fast_io::fast_terminate();
                            }

                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", element segment requires an unresolved imported table \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                imported_table_ptr->import_type_ptr->module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                imported_table_ptr->import_type_ptr->extern_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }
                    }
                    else
                    {
                        auto const local_idx{table_idx - imported_table_count};
                        if(local_idx >= curr_rt.local_defined_table_vec_storage.size()) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", element segment refers to a table index that is out of bounds (table_idx=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                table_idx,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", imported_tables=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                imported_table_count,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", local_tables=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_rt.local_defined_table_vec_storage.size(),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8").\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }

                        target_table = ::std::addressof(curr_rt.local_defined_table_vec_storage.index_unchecked(local_idx));
                    }

                    if(target_table == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                    if(target_table->table_type_ptr == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                    using reference_family = ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family;
                    // The legacy reftype carrier can be externref for a Core 3 anyref table. The complete
                    // Core 3 declaration, rather than that carrier, selects the materialized payload family.
                    auto const target_family{::uwvm2::uwvm::runtime::storage::runtime_table_family(*target_table)};
                    auto const element_family{::uwvm2::uwvm::runtime::storage::runtime_element_family(elem_seg, curr_rt)};
                    if(target_family != element_family) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In module \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            curr_module_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\", active element segment targets an incompatible table \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            ::uwvm2::parser::wasm::standard::wasm1p1::features::section_details(*target_table->table_type_ptr),
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\".\n\n",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    auto const wide_offset{elem.offset};

                    // Legacy segments retain module-local indices. Expression-form funcref segments use canonical pointer entries
                    // so global.get can preserve a reference created by another module.
                    auto const funcidx_begin{elem.funcidx_begin};
                    auto const funcidx_end{elem.funcidx_end};
                    auto const funcref_begin{elem.funcref_begin};
                    auto const funcref_end{elem.funcref_end};
                    auto const externref_begin{elem.externref_begin};
                    auto const externref_end{elem.externref_end};
                    auto const gc_ref_begin{elem.gc_ref_begin};
                    auto const gc_ref_end{elem.gc_ref_end};
                    if(((funcidx_begin == nullptr) != (funcidx_end == nullptr)) ||
                       ((funcref_begin == nullptr) != (funcref_end == nullptr)) ||
                       ((externref_begin == nullptr) != (externref_end == nullptr)) ||
                       ((gc_ref_begin == nullptr) != (gc_ref_end == nullptr))) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    if(target_family == reference_family::function)
                    {
                        if(externref_begin != nullptr || gc_ref_begin != nullptr ||
                           (funcidx_begin != nullptr && funcref_begin != nullptr)) [[unlikely]]
                        {
                            ::fast_io::fast_terminate();
                        }
                    }
                    else if(funcidx_begin != nullptr || funcref_begin != nullptr ||
                            (target_family == reference_family::gc ? externref_begin != nullptr : gc_ref_begin != nullptr)) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }

                    auto const element_count{target_family == reference_family::function
                                                 ? (funcref_begin == nullptr ? safe_ptr_range_size(funcidx_begin, funcidx_end)
                                                                             : safe_ptr_range_size(funcref_begin, funcref_end))
                                                 : (target_family == reference_family::gc
                                                        ? safe_ptr_range_size(gc_ref_begin, gc_ref_end)
                                                        : safe_ptr_range_size(externref_begin, externref_end))};

                    auto const table_size{target_table->elems.size()};
                    if(wide_offset > table_size || element_count > (table_size - wide_offset)) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In module \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            curr_module_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\", element segment initialization would write past table bounds (offset=",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            wide_offset,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8", count=",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            element_count,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8", table_size=",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            table_size,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8").\n\n",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }

                    // [table slots ... wide_offset ... wide_offset+element_count] | end
                    // [safe                                                        ]
                    // Complete full-width proof above permits native indexing below.
                    auto const offset{static_cast<::std::size_t>(wide_offset)};
                    if(target_family == reference_family::external || target_family == reference_family::exception)
                    {
                        auto const exnref_table{target_family == reference_family::exception};
                        for(::std::size_t i{}; i != element_count; ++i)
                        {
                            // [externref_begin, externref_end) | segment_end
                            // [safe                         ]
                            //                   ^^ i is bounded by element_count, derived from this exact pair.
                            auto const payload{externref_begin[i]};
                            if((exnref_table ? ::uwvm2::uwvm::runtime::storage::retain_runtime_table_exn_payload(target_table, payload)
                                             : ::uwvm2::uwvm::runtime::storage::retain_runtime_table_extern_payload(target_table, payload)) !=
                                   ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
                            { ::fast_io::fast_terminate(); }
                            auto& slot{target_table->elems.index_unchecked(offset + i)};
                            slot.storage.extern_ptr = payload;
                            slot.type = exnref_table ? table_elem_type::exn_ref : table_elem_type::extern_ref;
                        }
                    }
                    else if(target_family == reference_family::gc)
                    {
                        for(::std::size_t i{}; i != element_count; ++i)
                        {
                            // [gc_ref_begin, gc_ref_end) and [table slots, table end) stay module-owned.
                            // [safe                        ] [safe                          ]
                            //                  ^^ i < element_count and the full table range were checked above.
                            auto const& reference{gc_ref_begin[i]};
                            if(::uwvm2::uwvm::runtime::storage::retain_runtime_table_reference(target_table, reference) !=
                               ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
                            { ::fast_io::fast_terminate(); }
                            target_table->elems.index_unchecked(offset + i) =
                                ::uwvm2::uwvm::runtime::storage::runtime_table_slot_from_gc_reference(reference);
                        }
                    }
                    else
                    {
                        if(funcref_begin != nullptr)
                        {
                            for(::std::size_t i{}; i != element_count; ++i)
                            {
                                target_table->elems.index_unchecked(offset + i) = funcref_begin[i];
                            }
                            refresh_active_element_call_indirect_view(target_table, offset, element_count);
                            ::uwvm2::uwvm::runtime::storage::drop_wasm_element_segment_payload(elem);
                            continue;
                        }

                        auto const imported_func_count{curr_rt.imported_function_vec_storage.size()};
                        auto const local_func_count{curr_rt.local_defined_function_vec_storage.size()};
                        auto const all_func_count{imported_func_count + local_func_count};

                        for(::std::size_t i{}; i != element_count; ++i)
                        {
                            auto& slot{target_table->elems.index_unchecked(offset + i)};
                            if(funcidx_begin[i] == wasm_ref_null_funcidx_sentinel)
                            {
                                slot = {};
                                continue;
                            }
                            auto const func_idx{safe_u32_to_size_t(funcidx_begin[i])};
                            if(func_idx >= all_func_count) [[unlikely]]
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", element segment refers to a function index that is out of bounds (func_idx=",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    func_idx,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8" >= ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    all_func_count,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8").\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            }

                            if(func_idx < imported_func_count)
                            {
                                slot.storage.imported_ptr = ::std::addressof(curr_rt.imported_function_vec_storage.index_unchecked(func_idx));
                                slot.type = table_elem_type::func_ref_imported;
                            }
                            else
                            {
                                auto const local_idx{func_idx - imported_func_count};
                                slot.storage.defined_ptr = ::std::addressof(curr_rt.local_defined_function_vec_storage.index_unchecked(local_idx));
                                slot.type = table_elem_type::func_ref_defined;
                            }
                        }
                    }

                    // Instantiation applies an active element segment as table.init followed by an implicit elem.drop.
                    refresh_active_element_call_indirect_view(target_table, offset, element_count);
                    ::uwvm2::uwvm::runtime::storage::drop_wasm_element_segment_payload(elem);
                }

                bool initialized_shared_memory{};
                // data (wasm1: active segments)
                for(auto& data_seg: curr_rt.local_defined_data_vec_storage)
                {
                    auto& data{data_seg.data};
                    if(data.kind != ::uwvm2::uwvm::runtime::storage::wasm_data_segment_kind::active || ::uwvm2::uwvm::runtime::storage::wasm_data_segment_is_dropped(data)) { continue; }
                    ++data_active_applied;

                    if(data_seg.data_type_ptr != nullptr)
                    {
                        auto const& expr{data_seg.data_type_ptr->storage.segment.expr};
                        if(expr.opcodes.size() > 1uz ||
                           (expr.opcodes.size() == 1uz &&
                            expr.opcodes.front_unchecked().opcode == ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::global_get))
                        {
                            try_eval_wasm3_memory_const_expr_offset_after_linking_in_context(world, expr, curr_rt, data.offset,
                                runtime_memory_is_address64(curr_rt, safe_u32_to_size_t(data.memory_idx)));
                        }
                    }

                    auto const mem_idx{safe_u32_to_size_t(data.memory_idx)};
                    auto const imported_mem_count{curr_rt.imported_memory_vec_storage.size()};

                    ::uwvm2::uwvm::runtime::storage::local_defined_memory_storage_t* target_memory{};
                    ::uwvm2::uwvm::wasm::type::local_imported_t* target_local_imported_memory{};
                    ::std::size_t target_local_imported_index{};
                    ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t const* imported_memory_ptr_for_error{};
                    if(mem_idx < imported_mem_count)
                    {
                        imported_memory_ptr_for_error = ::std::addressof(curr_rt.imported_memory_vec_storage.index_unchecked(mem_idx));
                        wasm1_resolved_imported_memory_t resolved_memory{};
                        if(!maybe_resolve_wasm1_imported_memory_in_context(world, imported_memory_ptr_for_error, resolved_memory)) [[unlikely]]
                        {
                            if(imported_memory_ptr_for_error == nullptr || imported_memory_ptr_for_error->import_type_ptr == nullptr) [[unlikely]]
                            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                ::fast_io::fast_terminate();
                            }

                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", data segment requires an unresolved imported memory \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                imported_memory_ptr_for_error->import_type_ptr->module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                imported_memory_ptr_for_error->import_type_ptr->extern_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }

                        if(resolved_memory.defined_ptr) { target_memory = resolved_memory.defined_ptr; }
                        else
                        {
                            target_local_imported_memory = resolved_memory.local_imported_ptr;
                            target_local_imported_index = resolved_memory.local_imported_index;
                        }
                    }
                    else
                    {
                        auto const local_idx{mem_idx - imported_mem_count};
                        if(local_idx >= curr_rt.local_defined_memory_vec_storage.size()) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", data segment refers to a memory index that is out of bounds (memory_idx=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                mem_idx,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", imported_memories=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                imported_mem_count,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", local_memories=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_rt.local_defined_memory_vec_storage.size(),
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8").\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }

                        target_memory = ::std::addressof(curr_rt.local_defined_memory_vec_storage.index_unchecked(local_idx));
                    }

                    if(target_memory == nullptr && target_local_imported_memory == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    // Keep the complete Wasm address until the native memory bound proves it representable.
                    auto const offset{data.offset};

                    auto const byte_begin{data.byte_begin};
                    auto const byte_end{data.byte_end};
                    if((byte_begin == nullptr) != (byte_end == nullptr)) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    auto const byte_count{safe_ptr_range_size(byte_begin, byte_end)};

                    auto const copy_data{[&](::std::byte* memory_begin, ::std::size_t mem_length) noexcept
                    {
                        if(offset > mem_length || byte_count > (mem_length - offset)) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", data segment initialization would write past memory bounds (offset=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                offset,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", size=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                byte_count,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8", memory_size=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                mem_length,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8").\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }

                        if(byte_count != 0uz)
                        {
                            if(memory_begin == nullptr) [[unlikely]]
                            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                ::fast_io::fast_terminate();
                            }

                            // [pinned/committed memory ... offset: byte_count ...] end
                            // [safe                                                ]
                            //                            ^^ destination; offset <= mem_length <= SIZE_MAX, then byte_count <= mem_length - offset.
                            // Native narrowing and pointer addition occur only after the complete u64 range proof.
                            ::fast_io::freestanding::my_memcpy(memory_begin + static_cast<::std::size_t>(offset), byte_begin, byte_count);
                        }

                    }};
                    if(target_memory)
                    {
                        initialized_shared_memory |= target_memory->memory_type_ptr != nullptr &&
                            wasm_memory_is_shared(*target_memory->memory_type_ptr);
                        with_native_initialization_memory(target_memory->memory, copy_data);
                    }
                    else
                    {
                        ::std::byte* memory_begin{};
                        ::std::size_t mem_length{};
                        auto li{target_local_imported_memory};
                        if(li == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const page_size_bytes{li->memory_page_size_from_index(target_local_imported_index)};
                        if(page_size_bytes != 65536u) [[unlikely]]
                        {
                            if(imported_memory_ptr_for_error == nullptr || imported_memory_ptr_for_error->import_type_ptr == nullptr) [[unlikely]]
                            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                                ::fast_io::fast_terminate();
                            }

                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"initializer: In module \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                curr_module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\", imported memory \"",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                imported_memory_ptr_for_error->import_type_ptr->module_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8".",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                imported_memory_ptr_for_error->import_type_ptr->extern_name,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8"\" has an unsupported host page size (page_size_bytes=",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                page_size_bytes,
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                u8").\n\n",
                                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }

                        auto const page_count_u64{li->memory_size_from_index(target_local_imported_index)};
                        if(page_count_u64 > ::std::numeric_limits<::std::uint_least64_t>::max() / static_cast<::std::uint_least64_t>(page_size_bytes))
                            [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        auto const mem_length_u64{page_count_u64 * static_cast<::std::uint_least64_t>(page_size_bytes)};
                        mem_length = safe_u64_to_size_t(mem_length_u64);
                        memory_begin = li->memory_begin_from_index(target_local_imported_index);
                        copy_data(memory_begin, mem_length);
                    }

                    // Instantiation applies an active data segment as memory.init followed by an implicit data.drop.
                    ::uwvm2::uwvm::runtime::storage::drop_wasm_data_segment_payload(data);
                }

                // Threads' instantiation rule orders the complete data-section
                // initialization before subsequent synchronization operations.
                // Segment order is unchanged. Unshared initialization needs no
                // inter-agent fence, and normal guest memory accesses are unchanged.
                if(initialized_shared_memory) { ::std::atomic_thread_fence(::std::memory_order_seq_cst); }

                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_info(u8"initializer: Apply segments summary for module \"",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"\": applied(elem/data)=",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 elem_active_applied,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 data_active_applied,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8". ");
                }
            }
            // Actual writes, validation and implicit drops are complete before
            // the native phase is published. A failed initializer never reaches it.
            curr_rt.gc_collection_phase.publish_active_segments(true);
        }

        inline constexpr void apply_wasm1_active_element_and_data_segments_for_module(
            ::uwvm2::utils::container::u8string_view curr_module_name,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& curr_rt) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            apply_wasm1_active_element_and_data_segments_for_module_in_context(world, curr_module_name, curr_rt);
        }

        template <initialization_purpose Purpose>
        inline constexpr void apply_wasm1_active_element_and_data_segments_after_linking_in_context(initialization_context<Purpose>& world) noexcept
        {
            for(auto& [name, rt]: world.modules())
            {
                apply_wasm1_active_element_and_data_segments_for_module_in_context(world, name, rt);
            }
        }

        inline constexpr void apply_wasm1_active_element_and_data_segments_after_linking() noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            apply_wasm1_active_element_and_data_segments_after_linking_in_context(world);
        }

        template <initialization_purpose Purpose>
        inline constexpr void initialize_from_wasm_file_in_context(initialization_context<Purpose>& world, ::uwvm2::uwvm::wasm::type::wasm_file_t const& wf,
                                                        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& out) noexcept(Purpose == initialization_purpose::ordinary)
        {
            switch(wf.binfmt_ver)
            {
                case 1u:
                {
                    initialize_from_binfmt_ver1_module_storage_in_context(world, wf.wasm_module_storage.wasm_binfmt_ver1_storage, wf.wasm_parameter.binfmt1_para, out);
                    break;
                }

                    /// @todo support other version
                    static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");

                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::fast_io::fast_terminate();
                }
            }
        }

        inline constexpr void initialize_from_wasm_file(::uwvm2::uwvm::wasm::type::wasm_file_t const& wf,
                                                        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t& out) noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            initialize_from_wasm_file_in_context(world, wf, out);
        }

        template <initialization_purpose Purpose>
        inline constexpr void resolve_imports_for_wasm_file_modules_in_context(initialization_context<Purpose>& world) noexcept
        {
            using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
            using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;
            using local_imported_export_type_t = ::uwvm2::uwvm::wasm::type::local_imported_export_type_t;

            for([[maybe_unused]] auto& [curr_module_name, curr_rt]: world.modules())
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    verbose_info(u8"initializer: Resolve imports for module \"",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"\". ");
                }

                auto const resolve_exported_module_runtime{
                    [&](auto const& import_ptr) constexpr noexcept -> ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t*
                    {
                        if(import_ptr == nullptr) [[unlikely]] { return nullptr; }
                        auto const it{world.modules().find(import_ptr->module_name)};
                        if(it == world.modules().end()) [[unlikely]] { return nullptr; }
                        return ::std::addressof(it->second);
                    }};

                auto const resolve_export_record{[&](auto const& import_ptr) constexpr noexcept -> ::uwvm2::uwvm::wasm::type::all_module_export_t const*
                                                 {
                                                     if(import_ptr == nullptr) [[unlikely]] { return nullptr; }

                                                     auto const mod_it{world.exports().find(import_ptr->module_name)};
                                                     if(mod_it == world.exports().end()) [[unlikely]] { return nullptr; }
                                                     auto const name_it{mod_it->second.find(import_ptr->extern_name)};
                                                     if(name_it == mod_it->second.end()) [[unlikely]] { return nullptr; }
                                                     return ::std::addressof(name_it->second);
                                                 }};

                for(auto& imp : curr_rt.imported_tag_vec_storage)
                {
                    auto const record{resolve_export_record(imp.import_type_ptr)};
                    if(record == nullptr || (record->type != module_type_t::exec_wasm && record->type != module_type_t::preloaded_wasm) ||
                       record->storage.wasm_file_export_storage_ptr.binfmt_ver != 1u) { continue; }
                    auto const exported{record->storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                    if(exported == nullptr || exported->type != external_types::tag) { continue; }
                    auto const owner{resolve_exported_module_runtime(imp.import_type_ptr)};
                    if(owner == nullptr) { continue; }
                    auto const index{static_cast<::std::size_t>(exported->storage.tag_idx)};
                    auto const imports{owner->imported_tag_vec_storage.size()};
                    if(index < imports)
                    {
                        // [frozen imported-tag records ...] imports; index < imports proves a live record.
                        // ^^ imported_target: the referenced module outlives the linked execution domain.
                        imp.imported_target = ::std::addressof(owner->imported_tag_vec_storage.index_unchecked(index));
                    }
                    else if(index - imports < owner->local_defined_tag_vec_storage.size())
                    {
                        // [frozen local-tag records ...] count; subtraction is guarded by index >= imports.
                        // ^^ defined_target: address is the identity, not the signature/type index.
                        imp.defined_target = ::std::addressof(owner->local_defined_tag_vec_storage.index_unchecked(index - imports));
                    }
                }

                for(auto& imp: curr_rt.imported_function_vec_storage)
                {
                    auto const import_ptr{imp.import_type_ptr};
                    if(import_ptr != nullptr &&
                       !::uwvm2::uwvm::runtime::initializer::details::is_wasip1_import_visible_for_wasm_module_in_context(world, curr_module_name, import_ptr->module_name))
                        [[unlikely]]
                    {
                        continue;
                    }
                    auto const export_record{resolve_export_record(import_ptr)};
                    if(export_record == nullptr) [[unlikely]] { continue; }

                    switch(export_record->type)
                    {
                        case module_type_t::exec_wasm: [[fallthrough]];
                        case module_type_t::preloaded_wasm:
                        {
                            if(export_record->storage.wasm_file_export_storage_ptr.binfmt_ver != 1u) [[unlikely]] { continue; }

                            auto const export_ptr{export_record->storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                            if(export_ptr == nullptr || export_ptr->type != external_types::func) [[unlikely]] { continue; }

                            auto const exported_rt{resolve_exported_module_runtime(import_ptr)};
                            if(exported_rt == nullptr) [[unlikely]] { continue; }

                            // This conversion is reasonable, as no index exceeding size_t will occur during the parsing phase.
                            auto const exported_idx{static_cast<::std::size_t>(export_ptr->storage.func_idx)};
                            auto const imported_count{exported_rt->imported_function_vec_storage.size()};
                            using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                            if(exported_idx < imported_count)
                            {
                                imp.target.imported_ptr = ::std::addressof(exported_rt->imported_function_vec_storage.index_unchecked(exported_idx));
                                imp.link_kind = func_link_kind::imported;
                                imp.is_opposite_side_imported = true;
                            }
                            else
                            {
                                auto const local_idx{exported_idx - imported_count};
                                if(local_idx >= exported_rt->local_defined_function_vec_storage.size()) [[unlikely]] { continue; }
                                imp.target.defined_ptr = ::std::addressof(exported_rt->local_defined_function_vec_storage.index_unchecked(local_idx));
                                imp.link_kind = func_link_kind::defined;
                                imp.is_opposite_side_imported = false;
                            }

                            break;
                        }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                        case module_type_t::preloaded_dl:
                        {
                            auto const dl_ptr{export_record->storage.wasm_dl_export_storage_ptr.storage};
                            if(dl_ptr == nullptr) [[unlikely]] { continue; }
                            using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                            imp.target.dl_ptr = dl_ptr;
                            imp.link_kind = func_link_kind::dl;
                            imp.is_opposite_side_imported = false;

                            break;
                        }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                        case module_type_t::weak_symbol:
                        {
                            auto const weak_ptr{export_record->storage.wasm_weak_symbol_export_storage_ptr.storage};
                            if(weak_ptr == nullptr) [[unlikely]] { continue; }
                            using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                            imp.target.weak_symbol_ptr = weak_ptr;
                            imp.link_kind = func_link_kind::weak_symbol;
                            imp.is_opposite_side_imported = false;

                            break;
                        }
#endif
                        case module_type_t::local_import:
                        {
                            auto const& li_exp{export_record->storage.local_imported_export_storage_ptr};
                            if(li_exp.type != local_imported_export_type_t::func || li_exp.storage == nullptr) [[unlikely]] { continue; }
                            using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                            imp.target.local_imported.module_ptr = li_exp.storage;
                            imp.target.local_imported.index = li_exp.index;
                            imp.link_kind = func_link_kind::local_imported;
                            imp.is_opposite_side_imported = false;

                            break;
                        }
                        [[unlikely]] default:
                        {
                            break;
                        }
                    }
                }

                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    auto const total{curr_rt.imported_function_vec_storage.size()};
                    ::std::size_t linked_imported{};
                    ::std::size_t linked_defined{};
                    ::std::size_t linked_local_imported{};
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                    ::std::size_t linked_dl{};
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                    ::std::size_t linked_weak_symbol{};
#endif
                    for(auto const& imp: curr_rt.imported_function_vec_storage)
                    {
                        using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                        linked_imported += static_cast<::std::size_t>(imp.link_kind == func_link_kind::imported);
                        linked_defined += static_cast<::std::size_t>(imp.link_kind == func_link_kind::defined);
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                        linked_dl += static_cast<::std::size_t>(imp.link_kind == func_link_kind::dl);
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                        linked_weak_symbol += static_cast<::std::size_t>(imp.link_kind == func_link_kind::weak_symbol);
#endif
                        linked_local_imported += static_cast<::std::size_t>(imp.link_kind == func_link_kind::local_imported);
                    }

                    auto const unresolved{total - linked_imported - linked_defined
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                          - linked_dl
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                          - linked_weak_symbol
#endif
                                          - linked_local_imported};

                    verbose_info(u8"initializer: Resolve imports summary (func) for module \"",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"\": total=",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 total,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8", linked(imported/defined"
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                 u8"/dl"
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                 u8"/weak_symbol"
#endif
                                 u8"/local_imported/unresolved)=",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 linked_imported,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 linked_defined,
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 linked_dl,
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 linked_weak_symbol,
#endif
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 linked_local_imported,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                 unresolved,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8". ");
                }

                for(auto& imp: curr_rt.imported_table_vec_storage)
                {
                    auto const import_ptr{imp.import_type_ptr};
                    auto const export_record{resolve_export_record(import_ptr)};
                    if(export_record == nullptr) [[unlikely]] { continue; }
                    if(export_record->type != module_type_t::exec_wasm && export_record->type != module_type_t::preloaded_wasm) { continue; }
                    if(export_record->storage.wasm_file_export_storage_ptr.binfmt_ver != 1u) [[unlikely]] { continue; }

                    auto const export_ptr{export_record->storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                    if(export_ptr == nullptr || export_ptr->type != external_types::table) [[unlikely]] { continue; }

                    auto const exported_rt{resolve_exported_module_runtime(import_ptr)};
                    if(exported_rt == nullptr) [[unlikely]] { continue; }

                    auto const exported_idx{static_cast<::std::size_t>(export_ptr->storage.table_idx)};
                    auto const imported_count{exported_rt->imported_table_vec_storage.size()};
                    using table_link_kind = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                    if(exported_idx < imported_count)
                    {
                        imp.target.imported_ptr = ::std::addressof(exported_rt->imported_table_vec_storage.index_unchecked(exported_idx));
                        imp.link_kind = table_link_kind::imported;
                        imp.is_opposite_side_imported = true;
                    }
                    else
                    {
                        auto const local_idx{exported_idx - imported_count};
                        if(local_idx >= exported_rt->local_defined_table_vec_storage.size()) [[unlikely]] { continue; }
                        imp.target.defined_ptr = ::std::addressof(exported_rt->local_defined_table_vec_storage.index_unchecked(local_idx));
                        imp.link_kind = table_link_kind::defined;
                        imp.is_opposite_side_imported = false;
                    }
                }

                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    auto const total{curr_rt.imported_table_vec_storage.size()};

                    ::std::size_t linked_imported{};
                    ::std::size_t linked_defined{};

                    for(auto const& imp: curr_rt.imported_table_vec_storage)
                    {
                        using table_link_kind = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                        linked_imported += static_cast<::std::size_t>(imp.link_kind == table_link_kind::imported);
                        linked_defined += static_cast<::std::size_t>(imp.link_kind == table_link_kind::defined);
                    }
                    auto const unresolved{total - linked_imported - linked_defined};

                    verbose_info(u8"initializer: Resolve imports summary (table) for module \"",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"\": total=",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 total,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8", linked(imported/defined/unresolved)=",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 linked_imported,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 linked_defined,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                 unresolved,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8". ");
                }

                for(auto& imp: curr_rt.imported_memory_vec_storage)
                {
                    auto const import_ptr{imp.import_type_ptr};
                    auto const export_record{resolve_export_record(import_ptr)};
                    if(export_record == nullptr) [[unlikely]] { continue; }

                    switch(export_record->type)
                    {
                        case module_type_t::exec_wasm:
                        case module_type_t::preloaded_wasm:
                        {
                            if(export_record->storage.wasm_file_export_storage_ptr.binfmt_ver != 1u) [[unlikely]] { continue; }

                            auto const export_ptr{export_record->storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                            if(export_ptr == nullptr || export_ptr->type != external_types::memory) [[unlikely]] { continue; }

                            auto const exported_rt{resolve_exported_module_runtime(import_ptr)};
                            if(exported_rt == nullptr) [[unlikely]] { continue; }

                            auto const exported_idx{static_cast<::std::size_t>(export_ptr->storage.memory_idx)};
                            auto const imported_count{exported_rt->imported_memory_vec_storage.size()};
                            using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                            if(exported_idx < imported_count)
                            {
                                imp.target.imported_ptr = ::std::addressof(exported_rt->imported_memory_vec_storage.index_unchecked(exported_idx));
                                imp.link_kind = memory_link_kind::imported;
                                imp.is_opposite_side_imported = true;
                            }
                            else
                            {
                                auto const local_idx{exported_idx - imported_count};
                                if(local_idx >= exported_rt->local_defined_memory_vec_storage.size()) [[unlikely]] { continue; }
                                imp.target.defined_ptr = ::std::addressof(exported_rt->local_defined_memory_vec_storage.index_unchecked(local_idx));
                                imp.link_kind = memory_link_kind::defined;
                                imp.is_opposite_side_imported = false;
                            }

                            break;
                        }
                        case module_type_t::local_import:
                        {
                            auto const& li_exp{export_record->storage.local_imported_export_storage_ptr};
                            if(li_exp.type != local_imported_export_type_t::memory || li_exp.storage == nullptr) [[unlikely]] { continue; }
                            using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                            imp.target.local_imported.module_ptr = li_exp.storage;
                            imp.target.local_imported.index = li_exp.index;
                            imp.link_kind = memory_link_kind::local_imported;
                            imp.is_opposite_side_imported = false;
                            break;
                        }
                        [[unlikely]] default:
                        {
                            break;
                        }
                    }
                }

                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    auto const total{curr_rt.imported_memory_vec_storage.size()};
                    ::std::size_t linked_imported{};
                    ::std::size_t linked_defined{};
                    ::std::size_t linked_local_imported{};

                    for(auto const& imp: curr_rt.imported_memory_vec_storage)
                    {
                        using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                        linked_imported += static_cast<::std::size_t>(imp.link_kind == memory_link_kind::imported);
                        linked_defined += static_cast<::std::size_t>(imp.link_kind == memory_link_kind::defined);
                        linked_local_imported += static_cast<::std::size_t>(imp.link_kind == memory_link_kind::local_imported);
                    }
                    auto const unresolved{total - linked_imported - linked_defined - linked_local_imported};

                    verbose_info(u8"initializer: Resolve imports summary (memory) for module \"",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"\": total=",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 total,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8", linked(imported/defined/local_imported/unresolved)=",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 linked_imported,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 linked_defined,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 linked_local_imported,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                 unresolved,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8". ");
                }

                for(auto& imp: curr_rt.imported_global_vec_storage)
                {
                    auto const import_ptr{imp.import_type_ptr};
                    auto const export_record{resolve_export_record(import_ptr)};
                    if(export_record == nullptr) [[unlikely]] { continue; }
                    switch(export_record->type)
                    {
                        case module_type_t::exec_wasm:
                        case module_type_t::preloaded_wasm:
                        {
                            if(export_record->storage.wasm_file_export_storage_ptr.binfmt_ver != 1u) [[unlikely]] { continue; }

                            auto const export_ptr{export_record->storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                            if(export_ptr == nullptr || export_ptr->type != external_types::global) [[unlikely]] { continue; }

                            auto const exported_rt{resolve_exported_module_runtime(import_ptr)};
                            if(exported_rt == nullptr) [[unlikely]] { continue; }

                            auto const exported_idx{static_cast<::std::size_t>(export_ptr->storage.global_idx)};
                            auto const imported_count{exported_rt->imported_global_vec_storage.size()};
                            using global_link_kind = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t::imported_global_link_kind;
                            if(exported_idx < imported_count)
                            {
                                imp.target.imported_ptr = ::std::addressof(exported_rt->imported_global_vec_storage.index_unchecked(exported_idx));
                                imp.link_kind = global_link_kind::imported;
                                imp.is_opposite_side_imported = true;
                            }
                            else
                            {
                                auto const local_idx{exported_idx - imported_count};
                                if(local_idx >= exported_rt->local_defined_global_vec_storage.size()) [[unlikely]] { continue; }
                                imp.target.defined_ptr = ::std::addressof(exported_rt->local_defined_global_vec_storage.index_unchecked(local_idx));
                                imp.link_kind = global_link_kind::defined;
                                imp.is_opposite_side_imported = false;
                            }
                            break;
                        }
                        case module_type_t::local_import:
                        {
                            auto const& li_exp{export_record->storage.local_imported_export_storage_ptr};
                            if(li_exp.type != local_imported_export_type_t::global || li_exp.storage == nullptr) [[unlikely]] { continue; }
                            using global_link_kind = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t::imported_global_link_kind;
                            imp.target.local_imported.module_ptr = li_exp.storage;
                            imp.target.local_imported.index = li_exp.index;
                            imp.link_kind = global_link_kind::local_imported;
                            imp.is_opposite_side_imported = false;
                            break;
                        }
                        [[unlikely]] default:
                        {
                            break;
                        }
                    }
                }

                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    auto const total{curr_rt.imported_global_vec_storage.size()};
                    ::std::size_t linked_imported{};
                    ::std::size_t linked_defined{};
                    ::std::size_t linked_local_imported{};

                    for(auto const& imp: curr_rt.imported_global_vec_storage)
                    {
                        using global_link_kind = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t::imported_global_link_kind;
                        linked_imported += static_cast<::std::size_t>(imp.link_kind == global_link_kind::imported);
                        linked_defined += static_cast<::std::size_t>(imp.link_kind == global_link_kind::defined);
                        linked_local_imported += static_cast<::std::size_t>(imp.link_kind == global_link_kind::local_imported);
                    }

                    auto const unresolved{total - linked_imported - linked_defined - linked_local_imported};

                    verbose_info(u8"initializer: Resolve imports summary (global) for module \"",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 curr_module_name,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"\": total=",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 total,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8", linked(imported/defined/local_imported/unresolved)=",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 linked_imported,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 linked_defined,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                 linked_local_imported,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8"/",
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                 unresolved,
                                 ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                 u8". ");
                }
            }
        }

        inline constexpr void resolve_imports_for_wasm_file_modules() noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            resolve_imports_for_wasm_file_modules_in_context(world);
        }

        template <initialization_purpose Purpose>
        inline constexpr void error_on_unresolved_imports_after_linking_in_context(initialization_context<Purpose>& world) noexcept
        {
            bool any_unresolved{};

            for([[maybe_unused]] auto const& [curr_module_name, curr_rt]: world.modules())
            {
                ::std::size_t unresolved_func{};
                ::std::size_t unresolved_table{};
                ::std::size_t unresolved_memory{};
                ::std::size_t unresolved_global{};

                {
                    using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                    for(auto const& imp: curr_rt.imported_function_vec_storage)
                    {
                        unresolved_func += static_cast<::std::size_t>(imp.link_kind == func_link_kind::unresolved);
                    }
                }
                {
                    using table_link_kind = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                    for(auto const& imp: curr_rt.imported_table_vec_storage)
                    {
                        unresolved_table += static_cast<::std::size_t>(imp.link_kind == table_link_kind::unresolved);
                    }
                }
                {
                    using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                    for(auto const& imp: curr_rt.imported_memory_vec_storage)
                    {
                        unresolved_memory += static_cast<::std::size_t>(imp.link_kind == memory_link_kind::unresolved);
                    }
                }
                {
                    using global_link_kind = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t::imported_global_link_kind;
                    for(auto const& imp: curr_rt.imported_global_vec_storage)
                    {
                        unresolved_global += static_cast<::std::size_t>(imp.link_kind == global_link_kind::unresolved);
                    }
                }

                if(unresolved_func == 0uz && unresolved_table == 0uz && unresolved_memory == 0uz && unresolved_global == 0uz) { continue; }
                any_unresolved = true;

#ifdef UWVM2_USE_HUGE_FAST_IO_CPO_OUTPUT
                // Original wide CPO call: may improve output throughput at higher compile-time and memory cost.
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"initializer: Unresolved imports in module \"",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                    curr_module_name,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"\": unresolved(f/t/m/g)=",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                    unresolved_func,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"/",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                    unresolved_table,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"/",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                    unresolved_memory,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"/",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                    unresolved_global,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"\n",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
#else
                // Smaller CPO argument packs reduce compilation cost; one lock keeps the split record atomic.
                {
                    auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
                    ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                        ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
                    auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

                    ::fast_io::io::perr(u8log_output_ul,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"initializer: Unresolved imports in module \"");
                    ::fast_io::io::perr(u8log_output_ul,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                        curr_module_name,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"\": unresolved(f/t/m/g)=",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                        unresolved_func,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"/");
                    ::fast_io::io::perr(u8log_output_ul,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                        unresolved_table,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"/",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                        unresolved_memory,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"/");
                    ::fast_io::io::perr(u8log_output_ul,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RED),
                                        unresolved_global,
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                        u8"\n",
                                        ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                }
#endif

                auto const print_import{
                    [&curr_module_name, &world](::uwvm2::utils::container::u8string_view kind,
                                        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_import_type_t const* import_ptr) constexpr noexcept
                    {
                        if(import_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;
                        using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;

                        ::uwvm2::utils::container::u8string_view reason{u8"unknown"};
                        ::uwvm2::utils::container::u8string_view export_module_type{};
                        ::uwvm2::utils::container::u8string_view got_kind{};
                        bool has_got_kind{};
                        bool has_export_idx{};
                        ::std::size_t export_idx{};
                        ::std::size_t export_total{};
                        bool wasip1_hidden_by_module_setting{};

                        auto const module_loaded{world.declarations().find(import_ptr->module_name) !=
                                                 world.declarations().end()};
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)
                        wasip1_hidden_by_module_setting =
                            !::uwvm2::uwvm::runtime::initializer::details::is_wasip1_import_visible_for_wasm_module_in_context(world, curr_module_name, import_ptr->module_name);
# endif
#endif

                        auto const modexp_it{world.exports().find(import_ptr->module_name)};
                        if(wasip1_hidden_by_module_setting) [[unlikely]]
                        {
                            reason = ::uwvm2::utils::container::u8string_view{u8"module-specific WASI Preview 1 setting disables this import"};
                        }
                        else if(modexp_it == world.exports().end())
                        {
                            if(module_loaded) { reason = ::uwvm2::utils::container::u8string_view{u8"module has no exports"}; }
                            else
                            {
                                reason = ::uwvm2::utils::container::u8string_view{u8"module not loaded"};
                            }
                        }
                        else
                        {
                            auto const exp_it{modexp_it->second.find(import_ptr->extern_name)};
                            if(exp_it == modexp_it->second.end()) { reason = ::uwvm2::utils::container::u8string_view{u8"export not found"}; }
                            else
                            {
                                auto const& export_record{exp_it->second};
                                export_module_type = module_type_to_string(export_record.type);

                                switch(export_record.type)
                                {
                                    case module_type_t::exec_wasm: [[fallthrough]];
                                    case module_type_t::preloaded_wasm:
                                    {
                                        if(export_record.storage.wasm_file_export_storage_ptr.binfmt_ver != 1u)
                                        {
                                            reason = ::uwvm2::utils::container::u8string_view{u8"unsupported wasm export binfmt version"};
                                            break;
                                        }

                                        auto const export_ptr{export_record.storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                                        if(export_ptr == nullptr)
                                        {
                                            reason = ::uwvm2::utils::container::u8string_view{u8"invalid wasm export record"};
                                            break;
                                        }

                                        if(export_ptr->type != import_ptr->imports.type)
                                        {
                                            reason = ::uwvm2::utils::container::u8string_view{u8"export kind mismatch"};
                                            got_kind = ::uwvm2::parser::wasm::standard::wasm1::type::get_extern_kind_name<char8_t>(export_ptr->type);
                                            has_got_kind = true;
                                            break;
                                        }

                                        auto const rt_it{world.modules().find(import_ptr->module_name)};
                                        if(rt_it == world.modules().end())
                                        {
                                            reason = ::uwvm2::utils::container::u8string_view{u8"export module runtime missing"};
                                            break;
                                        }

                                        auto const& exported_rt{rt_it->second};
                                        auto const export_type{export_ptr->type};

                                        if(export_type == external_types::func)
                                        {
                                            export_idx = static_cast<::std::size_t>(export_ptr->storage.func_idx);
                                            export_total =
                                                exported_rt.imported_function_vec_storage.size() + exported_rt.local_defined_function_vec_storage.size();
                                            has_export_idx = true;
                                        }
                                        else if(export_type == external_types::table)
                                        {
                                            export_idx = static_cast<::std::size_t>(export_ptr->storage.table_idx);
                                            export_total = exported_rt.imported_table_vec_storage.size() + exported_rt.local_defined_table_vec_storage.size();
                                            has_export_idx = true;
                                        }
                                        else if(export_type == external_types::memory)
                                        {
                                            export_idx = static_cast<::std::size_t>(export_ptr->storage.memory_idx);
                                            export_total = exported_rt.imported_memory_vec_storage.size() + exported_rt.local_defined_memory_vec_storage.size();
                                            has_export_idx = true;
                                        }
                                        else if(export_type == external_types::global)
                                        {
                                            export_idx = static_cast<::std::size_t>(export_ptr->storage.global_idx);
                                            export_total = exported_rt.imported_global_vec_storage.size() + exported_rt.local_defined_global_vec_storage.size();
                                            has_export_idx = true;
                                        }
                                        else
                                        {
                                            reason = ::uwvm2::utils::container::u8string_view{u8"unsupported export kind"};
                                            break;
                                        }

                                        if(has_export_idx && export_idx >= export_total)
                                        {
                                            reason = ::uwvm2::utils::container::u8string_view{u8"export index out of bounds in exporting module"};
                                        }
                                        else
                                        {
                                            // If the export record exists, kind matches, and the index is in range, linking should have succeeded.
                                            reason = ::uwvm2::utils::container::u8string_view{u8"unexpected unresolved import (internal error)"};
                                        }
                                        break;
                                    }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                                    case module_type_t::preloaded_dl:
                                    {
                                        if(import_ptr->imports.type != external_types::func)
                                        {
                                            reason = ::uwvm2::utils::container::u8string_view{u8"export kind mismatch"};
                                            got_kind = ::uwvm2::utils::container::u8string_view{u8"func"};
                                            has_got_kind = true;
                                            break;
                                        }
                                        if(export_record.storage.wasm_dl_export_storage_ptr.storage == nullptr)
                                        {
                                            reason = ::uwvm2::utils::container::u8string_view{u8"dl export is null"};
                                            break;
                                        }
                                        reason = ::uwvm2::utils::container::u8string_view{u8"unexpected unresolved import (internal error)"};
                                        break;
                                    }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                                    case module_type_t::weak_symbol:
                                    {
                                        if(import_ptr->imports.type != external_types::func)
                                        {
                                            reason = ::uwvm2::utils::container::u8string_view{u8"export kind mismatch"};
                                            got_kind = ::uwvm2::utils::container::u8string_view{u8"func"};
                                            has_got_kind = true;
                                            break;
                                        }
                                        if(export_record.storage.wasm_weak_symbol_export_storage_ptr.storage == nullptr)
                                        {
                                            reason = ::uwvm2::utils::container::u8string_view{u8"weak_symbol export is null"};
                                            break;
                                        }
                                        reason = ::uwvm2::utils::container::u8string_view{u8"unexpected unresolved import (internal error)"};
                                        break;
                                    }
#endif
                                    case module_type_t::local_import:
                                    {
                                        auto const& li_exp{export_record.storage.local_imported_export_storage_ptr};

                                        ::uwvm2::utils::container::u8string_view li_kind{};
                                        bool match{};
                                        switch(li_exp.type)
                                        {
                                            case ::uwvm2::uwvm::wasm::type::local_imported_export_type_t::func:
                                            {
                                                li_kind = ::uwvm2::utils::container::u8string_view{u8"func"};
                                                match = (import_ptr->imports.type == external_types::func);
                                                break;
                                            }
                                            case ::uwvm2::uwvm::wasm::type::local_imported_export_type_t::memory:
                                            {
                                                li_kind = ::uwvm2::utils::container::u8string_view{u8"memory"};
                                                match = (import_ptr->imports.type == external_types::memory);
                                                break;
                                            }
                                            case ::uwvm2::uwvm::wasm::type::local_imported_export_type_t::global:
                                            {
                                                li_kind = ::uwvm2::utils::container::u8string_view{u8"global"};
                                                match = (import_ptr->imports.type == external_types::global);
                                                break;
                                            }
                                            [[unlikely]] default:
                                            {
                                                li_kind = ::uwvm2::utils::container::u8string_view{u8"unknown"};
                                                match = false;
                                                break;
                                            }
                                        }

                                        if(!match)
                                        {
                                            reason = ::uwvm2::utils::container::u8string_view{u8"export kind mismatch"};
                                            got_kind = li_kind;
                                            has_got_kind = true;
                                            break;
                                        }
                                        if(li_exp.storage == nullptr)
                                        {
                                            reason = ::uwvm2::utils::container::u8string_view{u8"local_import export storage is null"};
                                            break;
                                        }
                                        reason = ::uwvm2::utils::container::u8string_view{u8"unexpected unresolved import (internal error)"};
                                        break;
                                    }
                                    [[unlikely]] default:
                                    {
                                        reason = ::uwvm2::utils::container::u8string_view{u8"unsupported export module type"};
                                        break;
                                    }
                                }
                            }
                        }

                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"initializer: In module \"",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            curr_module_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8"\", unresolved ",
                                            kind,
                                            u8" import: ",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            import_ptr->module_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8".",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                            import_ptr->extern_name,
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                            u8" (reason: ",
                                            reason);

                        if(!export_module_type.empty()) { ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output, u8", export_module_type=", export_module_type); }
                        if(has_got_kind) { ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output, u8", got=", got_kind); }
                        if(has_export_idx)
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output, u8", export_idx=", export_idx, u8", export_total=", export_total);
                        }

                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            u8")\n",
                                            ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                    }};

                {
                    using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
                    for(auto const& imp: curr_rt.imported_function_vec_storage)
                    {
                        if(imp.link_kind != func_link_kind::unresolved) { continue; }
                        print_import(u8"function", imp.import_type_ptr);
                    }
                }
                {
                    using table_link_kind = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                    for(auto const& imp: curr_rt.imported_table_vec_storage)
                    {
                        if(imp.link_kind != table_link_kind::unresolved) { continue; }
                        print_import(u8"table", imp.import_type_ptr);
                    }
                }
                {
                    using memory_link_kind = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t::imported_memory_link_kind;
                    for(auto const& imp: curr_rt.imported_memory_vec_storage)
                    {
                        if(imp.link_kind != memory_link_kind::unresolved) { continue; }
                        print_import(u8"memory", imp.import_type_ptr);
                    }
                }
                {
                    using global_link_kind = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t::imported_global_link_kind;
                    for(auto const& imp: curr_rt.imported_global_vec_storage)
                    {
                        if(imp.link_kind != global_link_kind::unresolved) { continue; }
                        print_import(u8"global", imp.import_type_ptr);
                    }
                }

                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output, u8"\n");
            }

            if(any_unresolved) { ::fast_io::fast_terminate(); }

            // Detect circular "import -> import -> ..." alias chains for functions.
            // These can happen via re-exported imports across modules and must be rejected if they never reach a concrete target.
            {
                using imported_func_storage_t = ::uwvm2::uwvm::runtime::storage::imported_function_storage_t;
                using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;

                auto const next_imported{[](imported_func_storage_t const* curr) constexpr noexcept -> imported_func_storage_t const*
                                         {
                                             if(curr == nullptr) { return nullptr; }
                                             if(curr->link_kind != func_link_kind::imported) { return nullptr; }
                                             return curr->target.imported_ptr;
                                         }};

                for([[maybe_unused]] auto const& [curr_module_name, curr_rt]: world.modules())
                {
                    for(auto const& imp: curr_rt.imported_function_vec_storage)
                    {
                        if(imp.link_kind != func_link_kind::imported) { continue; }

                        auto const import_ptr{imp.import_type_ptr};
                        if(import_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        check_no_import_alias_cycle_floyd(
                            ::std::addressof(imp),
                            next_imported,
                            [&]() constexpr noexcept
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported function \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" encountered a circular dependency during import resolution.\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            });

                        require_import_alias_chain_resolves(
                            ::std::addressof(imp),
                            [](imported_func_storage_t const* p) constexpr noexcept -> bool
                            { return p != nullptr && p->link_kind == func_link_kind::imported; },
                            next_imported,
                            [](imported_func_storage_t const* p) constexpr noexcept -> bool
                            { return p != nullptr && p->link_kind != func_link_kind::imported; },
                            [&]() constexpr noexcept
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported function \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" is linked as an imported alias but has a null target.\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            },
                            [&]() constexpr noexcept
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported function \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" is linked as an imported alias but does not resolve to a concrete function.\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            });
                    }
                }
            }

            // Detect circular "import -> import -> ..." alias chains for tables.
            // These can happen via re-exported imports across modules and must be rejected if they never reach a concrete target.
            {
                using imported_table_storage_t = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t;
                using table_link_kind = imported_table_storage_t::imported_table_link_kind;

                auto const next_imported{[](imported_table_storage_t const* curr) constexpr noexcept -> imported_table_storage_t const*
                                         {
                                             if(curr == nullptr) { return nullptr; }
                                             if(curr->link_kind != table_link_kind::imported) { return nullptr; }
                                             return curr->target.imported_ptr;
                                         }};

                for([[maybe_unused]] auto const& [curr_module_name, curr_rt]: world.modules())
                {
                    for(auto const& imp: curr_rt.imported_table_vec_storage)
                    {
                        if(imp.link_kind != table_link_kind::imported) { continue; }

                        auto const import_ptr{imp.import_type_ptr};
                        if(import_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        check_no_import_alias_cycle_floyd(
                            ::std::addressof(imp),
                            next_imported,
                            [&]() constexpr noexcept
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported table \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" encountered a circular dependency during import resolution.\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            });

                        require_import_alias_chain_resolves(
                            ::std::addressof(imp),
                            [](imported_table_storage_t const* p) constexpr noexcept -> bool
                            { return p != nullptr && p->link_kind == table_link_kind::imported; },
                            next_imported,
                            [](imported_table_storage_t const* p) constexpr noexcept -> bool
                            { return p != nullptr && p->link_kind == table_link_kind::defined; },
                            [&]() constexpr noexcept
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported table \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" is linked as an imported alias but has a null target.\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            },
                            [&]() constexpr noexcept
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported table \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" is linked as an imported alias but does not resolve to a concrete table.\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            });
                    }
                }
            }

            // Detect circular "import -> import -> ..." alias chains for memories.
            // These can happen via re-exported imports across modules and must be rejected if they never reach a concrete target.
            {
                using imported_memory_storage_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t;
                using memory_link_kind = imported_memory_storage_t::imported_memory_link_kind;

                auto const next_imported{[](imported_memory_storage_t const* curr) constexpr noexcept -> imported_memory_storage_t const*
                                         {
                                             if(curr == nullptr) { return nullptr; }
                                             if(curr->link_kind != memory_link_kind::imported) { return nullptr; }
                                             return curr->target.imported_ptr;
                                         }};

                for([[maybe_unused]] auto const& [curr_module_name, curr_rt]: world.modules())
                {
                    for(auto const& imp: curr_rt.imported_memory_vec_storage)
                    {
                        if(imp.link_kind != memory_link_kind::imported) { continue; }

                        auto const import_ptr{imp.import_type_ptr};
                        if(import_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        check_no_import_alias_cycle_floyd(
                            ::std::addressof(imp),
                            next_imported,
                            [&]() constexpr noexcept
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported memory \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" encountered a circular dependency during import resolution.\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            });

                        require_import_alias_chain_resolves(
                            ::std::addressof(imp),
                            [](imported_memory_storage_t const* p) constexpr noexcept -> bool
                            { return p != nullptr && p->link_kind == memory_link_kind::imported; },
                            next_imported,
                            [](imported_memory_storage_t const* p) constexpr noexcept -> bool
                            { return p != nullptr && (p->link_kind == memory_link_kind::defined || p->link_kind == memory_link_kind::local_imported); },
                            [&]() constexpr noexcept
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported memory \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" is linked as an imported alias but has a null target.\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            },
                            [&]() constexpr noexcept
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported memory \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" is linked as an imported alias but does not resolve to a concrete memory.\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            });
                    }
                }
            }

            // Detect circular "import -> import -> ..." alias chains for globals.
            // These can happen via re-exported imports across modules and must be rejected if they never reach a concrete target.
            {
                using imported_global_storage_t = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t;
                using global_link_kind = imported_global_storage_t::imported_global_link_kind;

                auto const is_imported{[](imported_global_storage_t const* curr) constexpr noexcept -> bool
                                       { return curr != nullptr && curr->link_kind == global_link_kind::imported; }};
                auto const next_imported{[](imported_global_storage_t const* curr) constexpr noexcept -> imported_global_storage_t const*
                                         {
                                             if(curr == nullptr) { return nullptr; }
                                             if(curr->link_kind != global_link_kind::imported) { return nullptr; }
                                             return curr->target.imported_ptr;
                                         }};
                auto const is_concrete{[](imported_global_storage_t const* curr) constexpr noexcept -> bool
                                       {
                                           if(curr == nullptr) { return false; }
                                           return curr->link_kind == global_link_kind::defined || curr->link_kind == global_link_kind::local_imported;
                                       }};

                for([[maybe_unused]] auto const& [curr_module_name, curr_rt]: world.modules())
                {
                    for(auto const& imp: curr_rt.imported_global_vec_storage)
                    {
                        if(imp.link_kind != global_link_kind::imported) { continue; }

                        auto const import_ptr{imp.import_type_ptr};
                        if(import_ptr == nullptr) [[unlikely]]
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }

                        check_no_import_alias_cycle_floyd(
                            ::std::addressof(imp),
                            next_imported,
                            [&]() constexpr noexcept
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported global \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" encountered a circular dependency during import resolution.\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            });

                        require_import_alias_chain_resolves(
                            ::std::addressof(imp),
                            is_imported,
                            next_imported,
                            is_concrete,
                            [&]() constexpr noexcept
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported global \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" is linked as an imported alias but has a null target.\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            },
                            [&]() constexpr noexcept
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"initializer: In module \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    curr_module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\", imported global \"",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->module_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8".",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                                    import_ptr->extern_name,
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                                    u8"\" is linked as an imported alias but does not resolve to a concrete global.\n\n",
                                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();
                            });
                    }
                }
            }

            // If we reached here, all unresolved imports are rejected and all imported-alias chains are acyclic and resolve to concrete targets.
            world.alias_sanity() = true;
        }

        inline constexpr void error_on_unresolved_imports_after_linking() noexcept
        {
            initialization_context<initialization_purpose::ordinary> world{};
            error_on_unresolved_imports_after_linking_in_context(world);
        }
    }  // namespace details

    inline constexpr void apply_runtime_active_segments(::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        auto& modules{::uwvm2::uwvm::runtime::storage::active_runtime_registry()};
        auto const pos{modules.find(module_name)};
        if(pos == modules.end()) [[unlikely]] { ::fast_io::fast_terminate(); }
        // Prior preload starts can retain guest threads that access an imported
        // table. In debug-full, hold the same actual-table lock as every LLVM
        // table bridge and the live call_indirect resolver across validation,
        // writes and implicit drops. The guard is inert in ordinary modes.
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
        ::uwvm2::runtime::lib::details::llvm_jit_debug_table_guard table_guard{};
#endif
        details::apply_wasm1_active_element_and_data_segments_for_module(module_name, pos->second);
    }

    // Embedding callers retain the eager-initialization default. The CLI
    // defers active segments to its ordered per-module start dispatcher.
    inline constexpr void initialize_runtime(bool defer_active_segments = false) noexcept
    {
        if(::uwvm2::uwvm::wasm::storage::details::selected_execute_wasm_initialized)
        { ::fast_io::fast_terminate(); } // never clear a sealed owner registry in place
        // Count the whole root-graph mutation, including teardown of previous
        // external storage. Existing caller reset/loader serialization remains
        // mandatory; this is not permission to replace a live compiled module.
        ::uwvm2::runtime::gc::scoped_gc_root_graph_administration gc_administration{};
        auto const gc_initializer_serial{::uwvm2::runtime::gc::begin_owned_initializer()};
        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { details::verbose_info(u8"Initialize the runtime environment for the WASM module. "); }

        ::fast_io::unix_timestamp start_time{};
        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
        {
#ifdef UWVM_CPP_EXCEPTIONS
            try
#endif
            {
                start_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
            }
#ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
                // do nothing
            }
#endif
        }

        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { details::verbose_info(u8"initializer: Clear runtime storage. "); }
        ::uwvm2::uwvm::runtime::storage::active_runtime_registry().clear();
        details::import_alias_sanity_checked = false;
        details::reset_configured_import_reset_match_counts();
        auto const all_module_size{::uwvm2::uwvm::wasm::storage::all_module.size()};
        details::check_reserve_limit(u8"runtime_modules", all_module_size, initializer_limit.max_runtime_modules);
        ::uwvm2::uwvm::runtime::storage::active_runtime_registry().reserve(all_module_size);
        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
        {
            details::verbose_info(u8"initializer: Reserve runtime storage (modules=",
                                  ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                  all_module_size,
                                  ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                  u8"). ");
        }

        for(auto const& [module_name, mod]: ::uwvm2::uwvm::wasm::storage::all_module)
        {
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t rt{};
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                details::verbose_info(u8"initializer: Build runtime record for module \"",
                                      ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                      module_name,
                                      ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                      u8"\" (type=",
                                      ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                      details::module_type_to_string(mod.type),
                                      ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                      u8"). ");
            }
            switch(mod.type)
            {
                case ::uwvm2::uwvm::wasm::type::module_type_t::exec_wasm: [[fallthrough]];
                case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_wasm:
                {
                    if(mod.module_storage_ptr.wf == nullptr) [[unlikely]]
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }

                    details::current_initializing_module_name = module_name;
                    details::initialize_from_wasm_file(*mod.module_storage_ptr.wf, rt);
                    details::current_initializing_module_name = {};
#if defined(UWVM_RUNTIME_LLVM_JIT)
                    rt.module_name = module_name;
#endif
                    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        details::verbose_info(u8"initializer: Module \"",
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                              module_name,
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                              u8"\": Init: imported(f/t/m/g)=",
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                              rt.imported_function_vec_storage.size(),
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                              u8"/",
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                              rt.imported_table_vec_storage.size(),
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                              u8"/",
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                              rt.imported_memory_vec_storage.size(),
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                              u8"/",
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                              rt.imported_global_vec_storage.size(),
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                              u8", local(f/t/m/g)=",
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                              rt.local_defined_function_vec_storage.size(),
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                              u8"/",
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                              rt.local_defined_table_vec_storage.size(),
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                              u8"/",
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                              rt.local_defined_memory_vec_storage.size(),
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                              u8"/",
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                              rt.local_defined_global_vec_storage.size(),
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                              u8", segments(elem/data)=",
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                              rt.local_defined_element_vec_storage.size(),
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                              u8"/",
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                              rt.local_defined_data_vec_storage.size(),
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                              u8". ");
                    }

                    // no necessary to check, When constructing the all_module, duplicate names have already been excluded.
                    ::uwvm2::uwvm::runtime::storage::active_runtime_registry().try_emplace(module_name, ::std::move(rt));

                    break;
                }
                default:
                {
                    // Other module types are not yet representable by `wasm_module_storage_t`.
                    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        details::verbose_info(u8"initializer: Skip module \"",
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                              module_name,
                                              ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                              u8"\" (type not supported by runtime storage yet). ");
                    }
                    break;
                }
            }
        }

        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { details::verbose_info(u8"initializer: Validate configured memory limit override target modules. "); }
        details::validate_configured_runtime_memory_limit_target_modules();
        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { details::verbose_info(u8"initializer: Validate configured import reset target modules. "); }
        details::validate_configured_import_reset_target_modules();
        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { details::verbose_info(u8"initializer: Validate configured import reset matches. "); }
        details::validate_configured_import_reset_matches();

        // Best-effort linking between wasm file modules.
        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { details::verbose_info(u8"initializer: Resolve imports (best-effort). "); }
        details::resolve_imports_for_wasm_file_modules();
        details::error_on_unresolved_imports_after_linking();
        details::validate_and_resolve_core3_tags_after_linking();

        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { details::verbose_info(u8"initializer: Validate linked import types. "); }
        details::validate_wasm_file_module_import_types_after_linking();

        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { details::verbose_info(u8"initializer: Finalize wasm1 globals. "); }
        details::finalize_wasm1_globals_after_linking();
        details::initialize_wasm3_table_expressions_after_linking();
        for(auto& [gc_module_name, gc_module] : ::uwvm2::uwvm::runtime::storage::active_runtime_registry())
        {
            static_cast<void>(gc_module_name);
            // Only native map-owned modules, after complete globals/table
            // initialization, receive the generation/ownership stamp.
            gc_module.gc_collection_phase.publish_initialized(gc_initializer_serial);
        }

        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]] { details::verbose_info(u8"initializer: Apply wasm1 active elem/data segments. "); }
        if(!defer_active_segments) { details::apply_wasm1_active_element_and_data_segments_after_linking(); }
        ::uwvm2::runtime::gc::publish_owned_initializer(gc_initializer_serial);

        // finalize time
        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
        {
            ::fast_io::unix_timestamp end_time{};
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
#ifdef UWVM_CPP_EXCEPTIONS
                try
#endif
                {
                    end_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                }
#ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    // do nothing
                }
#endif
            }

            details::verbose_info(u8"initializer: Runtime initialization done. (time=",
                                  ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_GREEN),
                                  end_time - start_time,
                                  ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                  u8"s). ");
        }
    }
}

#include "restoration_context.h"

#ifndef UWVM_MODULE
// macro
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
