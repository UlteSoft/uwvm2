/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
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

// Without pragma once, this header file will be included in a specific code segment.

/// @warning Extension point: keep this helper synchronized with wasm1p1::type::value_type so ECO output names every supported type.
constexpr auto get_wasm1p1_type_name{
    []<::std::integral char_type2>(::std::uint_least8_t value) constexpr noexcept -> ::uwvm2::utils::container::basic_string_view<char_type2>
    {
        switch(value)
        {
            case 0x7Fu:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"i32"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"i32"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"i32"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"i32"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"i32"}; }
            }
            case 0x7Eu:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"i64"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"i64"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"i64"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"i64"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"i64"}; }
            }
            case 0x7Du:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"f32"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"f32"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"f32"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"f32"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"f32"}; }
            }
            case 0x7Cu:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"f64"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"f64"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"f64"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"f64"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"f64"}; }
            }
            case 0x7Bu:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"v128"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"v128"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"v128"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"v128"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"v128"}; }
            }
            case 0x70u:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"funcref"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"funcref"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"funcref"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"funcref"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"funcref"}; }
            }
            case 0x6Fu:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"externref"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"externref"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"externref"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"externref"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"externref"}; }
            }
            [[unlikely]] default:
            {
                /// @warning Extension point: reaching "unknown" here usually means a new wasm type was not added to ECO type-name output.
                if constexpr(::std::same_as<char_type2, char>) { return {"unknown"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"unknown"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"unknown"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"unknown"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"unknown"}; }
            }
        }
    }};
