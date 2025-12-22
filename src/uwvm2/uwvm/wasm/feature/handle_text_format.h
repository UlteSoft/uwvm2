/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-09
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
# include <type_traits>
# include <concepts>
// import
# include <fast_io.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/parser/wasm/text_format/impl.h>
# include "feature.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::feature
{
    // Content used to check for errors
    inline constexpr ::uwvm2::utils::utf::u8result handle_text_format(
        ::uwvm2::parser::wasm::concepts::text_format_wapper<::uwvm2::parser::wasm::text_format::text_format::null>,  // [adl]
        char8_t const*,
        char8_t const* end) noexcept
    { return ::uwvm2::utils::utf::u8result{end, ::uwvm2::utils::utf::utf_error_code::success}; }

    inline constexpr ::uwvm2::utils::utf::u8result handle_text_format(
        ::uwvm2::parser::wasm::concepts::text_format_wapper<::uwvm2::parser::wasm::text_format::text_format::zero_illegal>,  // [adl]
        char8_t const* begin,
        char8_t const* end) noexcept
    { return ::uwvm2::utils::utf::check_has_zero_illegal_unchecked(begin, end); }

    inline constexpr ::uwvm2::utils::utf::u8result handle_text_format(
        ::uwvm2::parser::wasm::concepts::text_format_wapper<::uwvm2::parser::wasm::text_format::text_format::utf8_rfc3629>,  // [adl]
        char8_t const* begin,
        char8_t const* end) noexcept
    { return ::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629>(begin, end); }

    inline constexpr ::uwvm2::utils::utf::u8result handle_text_format(
        ::uwvm2::parser::wasm::concepts::text_format_wapper<::uwvm2::parser::wasm::text_format::text_format::utf8_rfc3629_with_zero_illegal>,  // [adl]
        char8_t const* begin,
        char8_t const* end) noexcept
    { return ::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629_and_zero_illegal>(begin, end); }

    // Used to warn about content that has not been checked for errors.
    inline constexpr ::uwvm2::utils::utf::u8result warn_unchecked_text_format_error(
        ::uwvm2::parser::wasm::concepts::text_format_wapper<::uwvm2::parser::wasm::text_format::text_format::null>,  // [adl]
        char8_t const* begin,
        char8_t const* end) noexcept
    { return ::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629_and_zero_illegal>(begin, end); }

    inline constexpr ::uwvm2::utils::utf::u8result warn_unchecked_text_format_error(
        ::uwvm2::parser::wasm::concepts::text_format_wapper<::uwvm2::parser::wasm::text_format::text_format::zero_illegal>,  // [adl]
        char8_t const* begin,
        char8_t const* end) noexcept
    { return ::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629>(begin, end); }

    inline constexpr ::uwvm2::utils::utf::u8result warn_unchecked_text_format_error(
        ::uwvm2::parser::wasm::concepts::text_format_wapper<::uwvm2::parser::wasm::text_format::text_format::utf8_rfc3629>,  // [adl]
        char8_t const* begin,
        char8_t const* end) noexcept
    { return ::uwvm2::utils::utf::check_has_zero_illegal_unchecked(begin, end); }

    inline constexpr ::uwvm2::utils::utf::u8result warn_unchecked_text_format_error(
        ::uwvm2::parser::wasm::concepts::text_format_wapper<::uwvm2::parser::wasm::text_format::text_format::utf8_rfc3629_with_zero_illegal>,  // [adl]
        [[maybe_unused]] char8_t const* begin,
        char8_t const* end) noexcept
    { return ::uwvm2::utils::utf::u8result{end, ::uwvm2::utils::utf::utf_error_code::success}; }

    template <typename Wrp>
    concept can_handle_text_format = requires(Wrp wrapper_adl, char8_t const* begin, char8_t const* end) {
        { handle_text_format(wrapper_adl, begin, end) } -> ::std::same_as<::uwvm2::utils::utf::u8result>;
    };

    template <typename Wrp>
    concept can_warn_unchecked_text_format_error = requires(Wrp wrapper_adl, char8_t const* begin, char8_t const* end) {
        { warn_unchecked_text_format_error(wrapper_adl, begin, end) } -> ::std::same_as<::uwvm2::utils::utf::u8result>;
    };

    static_assert(can_handle_text_format<::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_text_format_wapper_t>);
    static_assert(can_warn_unchecked_text_format_error<::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_text_format_wapper_t>);
}  // namespace uwvm2::uwvm::wasm::feature
