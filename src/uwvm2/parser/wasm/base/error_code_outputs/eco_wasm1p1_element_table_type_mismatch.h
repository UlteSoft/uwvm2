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

// Without pragma once, this header file will be included in a specific code segment

#include "eco_wasm1p1_type_name.h"

if constexpr(::std::same_as<char_type, char>)
{
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
    {
        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                "uwvm: ",
                UWVM_WIN32_TEXTATTR_RED,
                "[error] ",
                UWVM_WIN32_TEXTATTR_WHITE,
                "(offset=",
                ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                ") ",
                "Element segment reference type ",
                UWVM_WIN32_TEXTATTR_CYAN,
                get_wasm1p1_type_name.template operator()<char>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                " (",
                UWVM_WIN32_TEXTATTR_CYAN,
                ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                ") does not match table ",
                UWVM_WIN32_TEXTATTR_LT_GREEN,
                errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_idx,
                UWVM_WIN32_TEXTATTR_WHITE,
                " reference type ",
                UWVM_WIN32_TEXTATTR_CYAN,
                get_wasm1p1_type_name.template operator()<char>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                " (",
                UWVM_WIN32_TEXTATTR_CYAN,
                ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                ").",
                UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(
        ::std::forward<Stm>(stream),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL UWVM_AES_WHITE),
        "uwvm: ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RED),
        "[error] ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
        "(offset=",
        ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
        ") ",
        "Element segment reference type ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
        get_wasm1p1_type_name.template operator()<char>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
        " (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
        ") does not match table ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_LT_GREEN),
        errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_idx,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
        " reference type ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
        get_wasm1p1_type_name.template operator()<char>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
        " (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
        ").",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
    return;
}
else if constexpr(::std::same_as<char_type, wchar_t>)
{
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
    {
        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                L"uwvm: ",
                UWVM_WIN32_TEXTATTR_RED,
                L"[error] ",
                UWVM_WIN32_TEXTATTR_WHITE,
                L"(offset=",
                ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                L") ",
                L"Element segment reference type ",
                UWVM_WIN32_TEXTATTR_CYAN,
                get_wasm1p1_type_name.template operator()<wchar_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                L" (",
                UWVM_WIN32_TEXTATTR_CYAN,
                ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                L") does not match table ",
                UWVM_WIN32_TEXTATTR_LT_GREEN,
                errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_idx,
                UWVM_WIN32_TEXTATTR_WHITE,
                L" reference type ",
                UWVM_WIN32_TEXTATTR_CYAN,
                get_wasm1p1_type_name.template operator()<wchar_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                L" (",
                UWVM_WIN32_TEXTATTR_CYAN,
                ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                L").",
                UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(
        ::std::forward<Stm>(stream),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL UWVM_AES_W_WHITE),
        L"uwvm: ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RED),
        L"[error] ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
        L"(offset=",
        ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
        L") ",
        L"Element segment reference type ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
        get_wasm1p1_type_name.template operator()<wchar_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
        L" (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
        L") does not match table ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_LT_GREEN),
        errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_idx,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
        L" reference type ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
        get_wasm1p1_type_name.template operator()<wchar_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
        L" (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
        L").",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
    return;
}
else if constexpr(::std::same_as<char_type, char8_t>)
{
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
    {
        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                u8"uwvm: ",
                UWVM_WIN32_TEXTATTR_RED,
                u8"[error] ",
                UWVM_WIN32_TEXTATTR_WHITE,
                u8"(offset=",
                ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                u8") ",
                u8"Element segment reference type ",
                UWVM_WIN32_TEXTATTR_CYAN,
                get_wasm1p1_type_name.template operator()<char8_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                u8" (",
                UWVM_WIN32_TEXTATTR_CYAN,
                ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                u8") does not match table ",
                UWVM_WIN32_TEXTATTR_LT_GREEN,
                errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_idx,
                UWVM_WIN32_TEXTATTR_WHITE,
                u8" reference type ",
                UWVM_WIN32_TEXTATTR_CYAN,
                get_wasm1p1_type_name.template operator()<char8_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                u8" (",
                UWVM_WIN32_TEXTATTR_CYAN,
                ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                u8").",
                UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(
        ::std::forward<Stm>(stream),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL UWVM_AES_U8_WHITE),
        u8"uwvm: ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RED),
        u8"[error] ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
        u8"(offset=",
        ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
        u8") ",
        u8"Element segment reference type ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
        get_wasm1p1_type_name.template operator()<char8_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
        u8" (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
        u8") does not match table ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_LT_GREEN),
        errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_idx,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
        u8" reference type ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
        get_wasm1p1_type_name.template operator()<char8_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
        u8" (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
        u8").",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
    return;
}
else if constexpr(::std::same_as<char_type, char16_t>)
{
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
    {
        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                u"uwvm: ",
                UWVM_WIN32_TEXTATTR_RED,
                u"[error] ",
                UWVM_WIN32_TEXTATTR_WHITE,
                u"(offset=",
                ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                u") ",
                u"Element segment reference type ",
                UWVM_WIN32_TEXTATTR_CYAN,
                get_wasm1p1_type_name.template operator()<char16_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                u" (",
                UWVM_WIN32_TEXTATTR_CYAN,
                ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                u") does not match table ",
                UWVM_WIN32_TEXTATTR_LT_GREEN,
                errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_idx,
                UWVM_WIN32_TEXTATTR_WHITE,
                u" reference type ",
                UWVM_WIN32_TEXTATTR_CYAN,
                get_wasm1p1_type_name.template operator()<char16_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                u" (",
                UWVM_WIN32_TEXTATTR_CYAN,
                ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                u").",
                UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(
        ::std::forward<Stm>(stream),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL UWVM_AES_U16_WHITE),
        u"uwvm: ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RED),
        u"[error] ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
        u"(offset=",
        ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
        u") ",
        u"Element segment reference type ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
        get_wasm1p1_type_name.template operator()<char16_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
        u" (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
        u") does not match table ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_LT_GREEN),
        errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_idx,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
        u" reference type ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
        get_wasm1p1_type_name.template operator()<char16_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
        u" (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
        u").",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
    return;
}
else if constexpr(::std::same_as<char_type, char32_t>)
{
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
    {
        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                U"uwvm: ",
                UWVM_WIN32_TEXTATTR_RED,
                U"[error] ",
                UWVM_WIN32_TEXTATTR_WHITE,
                U"(offset=",
                ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                U") ",
                U"Element segment reference type ",
                UWVM_WIN32_TEXTATTR_CYAN,
                get_wasm1p1_type_name.template operator()<char32_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                U" (",
                UWVM_WIN32_TEXTATTR_CYAN,
                ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                U") does not match table ",
                UWVM_WIN32_TEXTATTR_LT_GREEN,
                errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_idx,
                UWVM_WIN32_TEXTATTR_WHITE,
                U" reference type ",
                UWVM_WIN32_TEXTATTR_CYAN,
                get_wasm1p1_type_name.template operator()<char32_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                U" (",
                UWVM_WIN32_TEXTATTR_CYAN,
                ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
                UWVM_WIN32_TEXTATTR_WHITE,
                U").",
                UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(
        ::std::forward<Stm>(stream),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL UWVM_AES_U32_WHITE),
        U"uwvm: ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RED),
        U"[error] ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
        U"(offset=",
        ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
        U") ",
        U"Element segment reference type ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
        get_wasm1p1_type_name.template operator()<char32_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
        U" (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.segment_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
        U") does not match table ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_LT_GREEN),
        errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_idx,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
        U" reference type ",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
        get_wasm1p1_type_name.template operator()<char32_t>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
        U" (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
        ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.wasm1p1_element_table_type_mismatch.table_type),
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
        U").",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
    return;
}
