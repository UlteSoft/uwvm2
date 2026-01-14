/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-01-08
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

auto const& bttm{errout.err.err_selectable.br_table_target_type_mismatch};

auto const expected_type_name{
    ::uwvm2::parser::wasm::standard::wasm1::type::get_value_name<char_type>(::uwvm2::parser::wasm::standard::wasm1::type::section_details(bttm.expected_type))};
auto const actual_type_name{
    ::uwvm2::parser::wasm::standard::wasm1::type::get_value_name<char_type>(::uwvm2::parser::wasm::standard::wasm1::type::section_details(bttm.actual_type))};

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
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                             "uwvm: ",
                                                             UWVM_WIN32_TEXTATTR_RED,
                                                             "[error] ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             "(offset=",
                                                             ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                             ") br_table target type mismatch between label ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             bttm.expected_label_index,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             " and ",
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             bttm.mismatched_label_index,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             ": expected arity=",
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             bttm.expected_arity,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             ", actual arity=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             bttm.actual_arity,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, ", expected type="),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, UWVM_WIN32_TEXTATTR_YELLOW),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, expected_type_name),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, UWVM_WIN32_TEXTATTR_WHITE),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, ", actual type="),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, UWVM_WIN32_TEXTATTR_CYAN),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, actual_type_name),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, UWVM_WIN32_TEXTATTR_WHITE),
                                                             ".",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL UWVM_AES_WHITE),
                                                     "uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RED),
                                                     "[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                     "(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     ") br_table target type mismatch between label ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                     bttm.expected_label_index,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                     " and ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_YELLOW),
                                                     bttm.mismatched_label_index,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                     ": expected arity=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_YELLOW),
                                                     bttm.expected_arity,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                     ", actual arity=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                     bttm.actual_arity,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, ", expected type="),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_YELLOW)),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, expected_type_name),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE)),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, ", actual type="),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN)),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, actual_type_name),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE)),
                                                     ".",
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
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                             L"uwvm: ",
                                                             UWVM_WIN32_TEXTATTR_RED,
                                                             L"[error] ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             L"(offset=",
                                                             ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                             L") br_table target type mismatch between label ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             bttm.expected_label_index,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             L" and ",
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             bttm.mismatched_label_index,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             L": expected arity=",
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             bttm.expected_arity,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             L", actual arity=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             bttm.actual_arity,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, L", expected type="),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, UWVM_WIN32_TEXTATTR_YELLOW),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, expected_type_name),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, UWVM_WIN32_TEXTATTR_WHITE),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, L", actual type="),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, UWVM_WIN32_TEXTATTR_CYAN),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, actual_type_name),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, UWVM_WIN32_TEXTATTR_WHITE),
                                                             L".",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL UWVM_AES_W_WHITE),
                                                     L"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RED),
                                                     L"[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                     L"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     L") br_table target type mismatch between label ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                     bttm.expected_label_index,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                     L" and ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_YELLOW),
                                                     bttm.mismatched_label_index,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                     L": expected arity=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_YELLOW),
                                                     bttm.expected_arity,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                     L", actual arity=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                     bttm.actual_arity,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, L", expected type="),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_YELLOW)),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, expected_type_name),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE)),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, L", actual type="),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN)),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, actual_type_name),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE)),
                                                     L".",
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
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                             u8"uwvm: ",
                                                             UWVM_WIN32_TEXTATTR_RED,
                                                             u8"[error] ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u8"(offset=",
                                                             ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                             u8") br_table target type mismatch between label ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             bttm.expected_label_index,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u8" and ",
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             bttm.mismatched_label_index,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u8": expected arity=",
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             bttm.expected_arity,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u8", actual arity=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             bttm.actual_arity,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, u8", expected type="),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, UWVM_WIN32_TEXTATTR_YELLOW),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, expected_type_name),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, UWVM_WIN32_TEXTATTR_WHITE),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, u8", actual type="),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, UWVM_WIN32_TEXTATTR_CYAN),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, actual_type_name),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, UWVM_WIN32_TEXTATTR_WHITE),
                                                             u8".",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL UWVM_AES_U8_WHITE),
                                                     u8"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RED),
                                                     u8"[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                     u8"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     u8") br_table target type mismatch between label ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                     bttm.expected_label_index,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                     u8" and ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_YELLOW),
                                                     bttm.mismatched_label_index,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                     u8": expected arity=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_YELLOW),
                                                     bttm.expected_arity,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                     u8", actual arity=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                     bttm.actual_arity,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, u8", expected type="),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_YELLOW)),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, expected_type_name),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE)),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, u8", actual type="),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN)),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, actual_type_name),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE)),
                                                     u8".",
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
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                             u"uwvm: ",
                                                             UWVM_WIN32_TEXTATTR_RED,
                                                             u"[error] ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u"(offset=",
                                                             ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                             u") br_table target type mismatch between label ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             bttm.expected_label_index,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u" and ",
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             bttm.mismatched_label_index,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u": expected arity=",
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             bttm.expected_arity,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u", actual arity=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             bttm.actual_arity,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, u", expected type="),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, UWVM_WIN32_TEXTATTR_YELLOW),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, expected_type_name),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, UWVM_WIN32_TEXTATTR_WHITE),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, u", actual type="),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, UWVM_WIN32_TEXTATTR_CYAN),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, actual_type_name),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, UWVM_WIN32_TEXTATTR_WHITE),
                                                             u".",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL UWVM_AES_U16_WHITE),
                                                     u"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RED),
                                                     u"[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                     u"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     u") br_table target type mismatch between label ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                     bttm.expected_label_index,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                     u" and ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_YELLOW),
                                                     bttm.mismatched_label_index,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                     u": expected arity=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_YELLOW),
                                                     bttm.expected_arity,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                     u", actual arity=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                     bttm.actual_arity,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, u", expected type="),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_YELLOW)),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, expected_type_name),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE)),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, u", actual type="),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN)),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, actual_type_name),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE)),
                                                     u".",
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
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                             U"uwvm: ",
                                                             UWVM_WIN32_TEXTATTR_RED,
                                                             U"[error] ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             U"(offset=",
                                                             ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                             U") br_table target type mismatch between label ",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             bttm.expected_label_index,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             U" and ",
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             bttm.mismatched_label_index,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             U": expected arity=",
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             bttm.expected_arity,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             U", actual arity=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             bttm.actual_arity,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, U", expected type="),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, UWVM_WIN32_TEXTATTR_YELLOW),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, expected_type_name),
                                                             ::fast_io::mnp::cond(bttm.expected_arity == 1u, UWVM_WIN32_TEXTATTR_WHITE),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, U", actual type="),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, UWVM_WIN32_TEXTATTR_CYAN),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, actual_type_name),
                                                             ::fast_io::mnp::cond(bttm.actual_arity == 1u, UWVM_WIN32_TEXTATTR_WHITE),
                                                             U".",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL UWVM_AES_U32_WHITE),
                                                     U"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RED),
                                                     U"[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                     U"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     U") br_table target type mismatch between label ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                     bttm.expected_label_index,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                     U" and ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_YELLOW),
                                                     bttm.mismatched_label_index,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                     U": expected arity=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_YELLOW),
                                                     bttm.expected_arity,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                     U", actual arity=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                     bttm.actual_arity,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, U", expected type="),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_YELLOW)),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, expected_type_name),
                                                     ::fast_io::mnp::cond(bttm.expected_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE)),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, U", actual type="),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN)),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, actual_type_name),
                                                     ::fast_io::mnp::cond(bttm.actual_arity == 1u, ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE)),
                                                     U".",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
    return;
}

