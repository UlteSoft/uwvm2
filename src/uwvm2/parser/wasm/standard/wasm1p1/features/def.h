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

    /// @brief Parser-owned COP tag selecting the WebAssembly 1.1 validation strategy.
    struct wasm1p1_code_version
    {};

    /// @brief Records which mutually-exclusive family of Wasm feature switches owns the command line.
    /// @details Each of the three complete feature-set switches (MVP, WebAssembly 1.1, and WebAssembly 2.0) conflicts with
    ///          either of the other two complete switches and with every scoped feature switch.
    enum class wasm_feature_cli_mode : ::std::uint_least8_t
    {
        unspecified,
        direct_mvp,
        direct_wasm1p1,
        direct_wasm2,
        scoped
    };

    /// @brief Runtime feature-policy storage shared by the WebAssembly 1.1 and WebAssembly 2.0 parser paths.
    /// @details The disable_* fields are false by default; the selected complete mode and scoped switches determine the effective policy.
    /// @details The explicit_* fields record CLI ownership so shared feature collection can report conflicts deterministically and emit version-correct ECO output.
    /// @warning Extension point: every new shared WebAssembly 1.1/2.0 subfeature flag needs CLI ownership, feature conflict handling, parser gating, and version-correct ECO output.
    struct wasm_binfmt1p1_feature_parameter
    {
        wasm_feature_cli_mode cli_mode{};

        bool disable_multi_value{};
        bool disable_reference_types{};
        bool disable_table_instructions{};
        bool disable_multiple_tables{};
        bool disable_bulk_memory{};
        bool disable_sign_extension{};
        bool disable_nontrapping_float_to_int{};
        bool disable_simd{};

        bool explicit_feature_mvp{};
        bool explicit_feature_wasm1p1{};
        bool explicit_feature_wasm2{};
        bool explicit_enable_multi_value{};
        bool explicit_enable_reference_types{};
        bool explicit_enable_table_instructions{};
        bool explicit_enable_multiple_tables{};
        bool explicit_enable_bulk_memory{};
        bool explicit_enable_sign_extension{};
        bool explicit_enable_nontrapping_float_to_int{};
        bool explicit_enable_simd{};
        bool explicit_disable_multi_value{};
        bool explicit_disable_reference_types{};
        bool explicit_disable_table_instructions{};
        bool explicit_disable_multiple_tables{};
        bool explicit_disable_bulk_memory{};
        bool explicit_disable_sign_extension{};
        bool explicit_disable_nontrapping_float_to_int{};
        bool explicit_disable_simd{};

        wasm1p1_parser_limit_t parser_limit{};

        /// @brief Re-enable wasm1 validation when a wasm1p1 feature that relaxes MVP syntax is explicitly disabled.
        bool controllable_allow_multi_result_vector{};
        bool controllable_allow_multi_table{};
    };

    /// @brief Select the legacy MVP encoding of `call_indirect`'s trailing immediate.
    /// @details WebAssembly Core 1.0 section 5.4.1 encodes `call_indirect` as
    ///          `0x11 typeidx 0x00`; that final token is a literal byte, not a
    ///          `tableidx`.  Core 1.0 section 2.5.1's abstract `tableidx ::= u32`
    ///          definition applies only where a grammar production actually
    ///          references `tableidx`; MVP `call_indirect` does not.  The
    ///          Reference Types/Multiple Tables grammar and
    ///          WebAssembly Core 2.0 section 5.4.1 instead encode a real
    ///          `tableidx ::= u32`.  Disabling multiple tables constrains that
    ///          decoded index to zero, but does not change its binary type.
    /// @details For a real `tableidx`, Core section 5.2.2 permits trailing-zero
    ///          ULEB128 encodings within the u32 width bound.  Thus `0x80 0x00`
    ///          is a valid encoding of index zero on the newer grammar path.
    [[nodiscard]] inline constexpr bool uses_mvp_call_indirect_reserved_byte(
        wasm_binfmt1p1_feature_parameter const& parameter) noexcept
    {
        return parameter.cli_mode == wasm_feature_cli_mode::direct_mvp;
    }

    /// Core 1 and Core 2 differ even for some primary opcodes: Core 1 br_table
    /// requires identical label types, whereas Core 2 can unify them through
    /// bottom operands. Disabling individual proposals does not select Core 1;
    /// only the explicit MVP policy does. Keep this decision shared by validators.
    [[nodiscard]] inline constexpr bool uses_mvp_validation_rules(
        wasm_binfmt1p1_feature_parameter const& parameter) noexcept
    {
        return parameter.cli_mode == wasm_feature_cli_mode::direct_mvp;
    }

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
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128: return !para.disable_simd;
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref: [[fallthrough]];
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::externref: return !para.disable_reference_types;
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
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type::externref: return !para.disable_reference_types;
            default: return false;
        }
    }

    /// @brief Return whether a reference type is legal specifically as a table element type.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr bool table_reference_type_enabled(::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type rt,
                                                       ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) noexcept
    {
        auto const& para{get_wasm1p1_parameter(fs_para)};
        switch(rt)
        {
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type::funcref: return true;
            case ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type::externref:
                return !para.disable_reference_types && !para.disable_table_instructions;
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
