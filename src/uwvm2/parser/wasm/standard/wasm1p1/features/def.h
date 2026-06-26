/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     antecedent dependency: WebAssembly Release 1.0 (2019-07-20)
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
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
# include <cstddef>
# include <cstdint>
# include <concepts>
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include "parser_limit.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
    struct wasm1p1;

    /// @brief Runtime switches for independent WebAssembly 1.1 feature groups.
    /// @details The explicit_* fields record CLI ownership so the feature collection and its subfeatures can report conflicts deterministically.
    /// @warning Extension point: every new wasm1.1 subfeature flag needs CLI ownership, feature conflict handling, parser gating, and ECO output.
    struct wasm_binfmt1p1_feature_parameter
    {
        bool enable_multi_value{};
        bool enable_reference_types{};
        bool enable_bulk_memory{};
        bool enable_sign_extension{};
        bool enable_nontrapping_float_to_int{};
        bool enable_simd{};

        bool explicit_feature_1p1{};
        bool explicit_enable_multi_value{};
        bool explicit_enable_reference_types{};
        bool explicit_enable_bulk_memory{};
        bool explicit_enable_sign_extension{};
        bool explicit_enable_nontrapping_float_to_int{};
        bool explicit_enable_simd{};

        wasm1p1_parser_limit_t parser_limit{};

        /// @brief Preserve wasm1 validation when wasm1p1 is compiled in but the runtime flags are still disabled.
        bool controllable_allow_multi_result_vector{true};
        bool controllable_allow_multi_table{true};
    };

    /// @brief Get the const wasm1.1 feature parameter from a parser feature-parameter tuple.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr wasm_binfmt1p1_feature_parameter const& get_wasm1p1_parameter(
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) noexcept
    {
        static_assert((::std::same_as<wasm1p1, Fs> || ...), "wasm1p1 parameter requested without wasm1p1 in the feature set");
        return ::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(fs_para);
    }

    /// @brief Get the mutable wasm1.1 feature parameter from a parser feature-parameter tuple.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr wasm_binfmt1p1_feature_parameter& get_wasm1p1_parameter(::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> & fs_para) noexcept
    {
        static_assert((::std::same_as<wasm1p1, Fs> || ...), "wasm1p1 parameter requested without wasm1p1 in the feature set");
        return ::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(fs_para);
    }

    /// @brief Return whether a value type is legal under the currently enabled wasm1.1 subfeatures.
    /// @warning Extension point: new value types must be mapped to the feature flag that enables them here.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr bool value_type_enabled(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type vt,
                                             ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) noexcept
    {
        auto const& para{get_wasm1p1_parameter(fs_para)};
        switch(vt)
        {
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::i32: [[fallthrough]];
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::i64: [[fallthrough]];
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::f32: [[fallthrough]];
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::f64: return true;
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128: return para.enable_simd;
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref: [[fallthrough]];
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::externref: return para.enable_reference_types;
            default: return false;
        }
    }

    /// @brief Return whether a reference type is legal under the currently enabled wasm1.1 subfeatures.
    /// @warning Extension point: new reference types must be gated here and kept consistent with table/global/element parsing.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr bool reference_type_enabled(::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type rt,
                                                 ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) noexcept
    {
        auto const& para{get_wasm1p1_parameter(fs_para)};
        switch(rt)
        {
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type::funcref:
            {
                // funcref remains the MVP table element type. Reference-typed locals/globals are gated by value_type_enabled instead.
                return true;
            }
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type::externref: return para.enable_reference_types;
            default: return false;
        }
    }

    /// @brief Convert a reference type byte into the matching value-type enum used by shared parser storage.
    /// @warning Extension point: new reference_type enumerators must be converted explicitly or validated before reaching shared storage.
    inline constexpr ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type to_value_type(
        ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type rt) noexcept
    {
        using value_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type;
        using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;
        switch(rt)
        {
            case reference_type::funcref: return value_type::funcref;
            case reference_type::externref: return value_type::externref;
            default: return static_cast<value_type>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(rt));
        }
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
