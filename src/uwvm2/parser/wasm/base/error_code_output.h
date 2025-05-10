/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-27
 * @copyright   APL-2 License
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

#ifdef UWVM_MODULE
import fast_io;
import uwvm2.utils.ansies;
import :error_code;
#else
// std
# include <cstddef>
# include <cstdint>
# include <concepts>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/utils/ansies/ansi_push_macro.h>
# include <uwvm2/utils/ansies/win32_text_attr_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/ansies/impl.h>
# include "error_code.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::base
{
    /// @brief Define the flag used for output
    struct error_output_flag_t
    {
        ::std::uint_least8_t enable_ansi : 1;
        ::std::uint_least8_t win32_use_text_attr : 1;  // on win95 - win7
    };

    /// @brief Provide information for output
    struct error_output_t
    {
        ::std::byte const* module_begin{};
        ::uwvm2::parser::wasm::base::error_impl err{};
        error_output_flag_t flag{};
    };

    /// @brief Output, support: char, wchar_t, char8_t, char16_t, char32_t
    template <::std::integral char_type, typename Stm>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, error_output_t>, Stm && stream, error_output_t const& errout) noexcept
    {
        bool const enable_ansi{static_cast<bool>(errout.flag.enable_ansi)};
        switch(errout.err.err_code)
        {
            default:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Unknown Error.",
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
                                                                     ") Unknown Error.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Unknown Error.",
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
                                                                     L") Unknown Error.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Unknown Error.",
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
                                                                     u8") Unknown Error.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Unknown Error.",
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
                                                                     u") Unknown Error.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Unknown Error.",
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
                                                                     U") Unknown Error.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
                    {
                        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                                             "uwvm: [info] There are no errors.",
                                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
                            return;
                        }
                    }
#endif
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL UWVM_AES_WHITE),
                                                                     "uwvm: [info] There are no errors.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
                    {
                        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                                             L"uwvm: [info] There are no errors.",
                                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
                            return;
                        }
                    }
#endif
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL UWVM_AES_W_WHITE),
                                                                     L"uwvm: [info] There are no errors.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
                    {
                        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                                             u8"uwvm: [info] There are no errors.",
                                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
                            return;
                        }
                    }
#endif
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL UWVM_AES_U8_WHITE),
                                                                     u8"uwvm: [info] There are no errors.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
                    {
                        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                                             u"uwvm: [info] There are no errors.",
                                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
                            return;
                        }
                    }
#endif
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL UWVM_AES_U16_WHITE),
                                                                     u"uwvm: [info] There are no errors.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
                    {
                        if(static_cast<bool>(errout.flag.win32_use_text_attr) && enable_ansi)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                             UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                                             U"uwvm: [info] There are no errors.",
                                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
                            return;
                        }
                    }
#endif
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL UWVM_AES_U32_WHITE),
                                                                     U"uwvm: [info] There are no errors.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_begin_pointer:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             "Begin Pointer \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::pointervw(errout.err.err_curr),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\" > End Pointer \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::pointervw(errout.err.err_selectable.err_end),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     "Begin Pointer \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     ::fast_io::mnp::pointervw(errout.err.err_curr),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\" > End Pointer \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     ::fast_io::mnp::pointervw(errout.err.err_selectable.err_end),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_nt_io_observer<char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>, ::fast_io::basic_zw_io_observer<char_type>>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                         UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE,
                                                                         L"uwvm: ",
                                                                         UWVM_WIN32_TEXTATTR_RED,
                                                                         L"[error] ",
                                                                         UWVM_WIN32_TEXTATTR_WHITE,
                                                                         L"Begin Pointer \"",
                                                                         UWVM_WIN32_TEXTATTR_CYAN,
                                                                         ::fast_io::mnp::pointervw(errout.err.err_curr),
                                                                         UWVM_WIN32_TEXTATTR_WHITE,
                                                                         L"\" > End Pointer \"",
                                                                         UWVM_WIN32_TEXTATTR_CYAN,
                                                                         ::fast_io::mnp::pointervw(errout.err.err_selectable.err_end),
                                                                         UWVM_WIN32_TEXTATTR_WHITE,
                                                                         L"\".",
                                                                         UWVM_WIN32_TEXTATTR_RST_ALL);
                        return;
                    }
#endif
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL UWVM_AES_W_WHITE),
                                                                     L"uwvm: ",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RED),
                                                                     L"[error] ",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"Begin Pointer \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     ::fast_io::mnp::pointervw(errout.err.err_curr),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\" > End Pointer \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     ::fast_io::mnp::pointervw(errout.err.err_selectable.err_end),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8"Begin Pointer \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::pointervw(errout.err.err_curr),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\" > End Pointer \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::pointervw(errout.err.err_selectable.err_end),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8"Begin Pointer \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     ::fast_io::mnp::pointervw(errout.err.err_curr),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\" > End Pointer \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     ::fast_io::mnp::pointervw(errout.err.err_selectable.err_end),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u"Begin Pointer \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::pointervw(errout.err.err_curr),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\" > End Pointer \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::pointervw(errout.err.err_selectable.err_end),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u"Begin Pointer \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     ::fast_io::mnp::pointervw(errout.err.err_curr),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\" > End Pointer \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     ::fast_io::mnp::pointervw(errout.err.err_selectable.err_end),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U"Begin Pointer \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::pointervw(errout.err.err_curr),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\" > End Pointer \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::pointervw(errout.err.err_selectable.err_end),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U"Begin Pointer \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     ::fast_io::mnp::pointervw(errout.err.err_curr),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\" > End Pointer \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     ::fast_io::mnp::pointervw(errout.err.err_selectable.err_end),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_wasm_file_format:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Illegal WebAssembly file format.",
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
                                                                     ") Illegal WebAssembly file format.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Illegal WebAssembly file format.",
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
                                                                     L") Illegal WebAssembly file format.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Illegal WebAssembly file format.",
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
                                                                     u8") Illegal WebAssembly file format.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Illegal WebAssembly file format.",
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
                                                                     u") Illegal WebAssembly file format.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Illegal WebAssembly file format.",
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
                                                                     U") Illegal WebAssembly file format.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::no_wasm_section_found:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") No WebAssembly sections found.",
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
                                                                     ") No WebAssembly sections found.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") No WebAssembly sections found.",
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
                                                                     L") No WebAssembly sections found.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") No WebAssembly sections found.",
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
                                                                     u8") No WebAssembly sections found.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") No WebAssembly sections found.",
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
                                                                     u") No WebAssembly sections found.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") No WebAssembly sections found.",
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
                                                                     U") No WebAssembly sections found.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_section_length:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Invalid Section Length.",
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
                                                                     ") Invalid Section Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Invalid Section Length.",
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
                                                                     L") Invalid Section Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Invalid Section Length.",
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
                                                                     u8") Invalid Section Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Invalid Section Length.",
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
                                                                     u") Invalid Section Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Invalid Section Length.",
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
                                                                     U") Invalid Section Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_section_length:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Illegal Section Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Illegal Section Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Illegal Section Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Illegal Section Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Illegal Section Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Illegal Section Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Illegal Section Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Illegal Section Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Illegal Section Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Illegal Section Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::not_enough_space_unmarked:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Not enough space left.",
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
                                                                     ") Not enough space left.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Not enough space left.",
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
                                                                     L") Not enough space left.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Not enough space left.",
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
                                                                     u8") Not enough space left.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Not enough space left.",
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
                                                                     u") Not enough space left.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Not enough space left.",
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
                                                                     U") Not enough space left.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_section_id:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Illegal Section ID: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Illegal Section ID: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u8,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Illegal Section ID: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Illegal Section ID: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u8,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Illegal Section ID: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Illegal Section ID: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u8,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Illegal Section ID: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Illegal Section ID: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u8,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Illegal Section ID: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Illegal Section ID: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u8,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_custom_name_length:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Invalid Custom Name Length.",
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
                                                                     ") Invalid Custom Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Invalid Custom Name Length.",
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
                                                                     L") Invalid Custom Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Invalid Custom Name Length.",
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
                                                                     u8") Invalid Custom Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Invalid Custom Name Length.",
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
                                                                     u") Invalid Custom Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Invalid Custom Name Length.",
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
                                                                     U") Invalid Custom Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_custom_name_length:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Illegal Custom Name Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Illegal Custom Name Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Illegal Custom Name Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Illegal Custom Name Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Illegal Custom Name Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Illegal Custom Name Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Illegal Custom Name Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Illegal Custom Name Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Illegal Custom Name Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Illegal Custom Name Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_parameter_length:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Invalid Parameter Length.",
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
                                                                     ") Invalid Parameter Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Invalid Parameter Length.",
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
                                                                     L") Invalid Parameter Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Invalid Parameter Length.",
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
                                                                     u8") Invalid Parameter Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Invalid Parameter Length.",
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
                                                                     u") Invalid Parameter Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Invalid Parameter Length.",
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
                                                                     U") Invalid Parameter Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }

                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_parameter_length:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Illegal Parameter Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Illegal Parameter Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Illegal Parameter Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Illegal Parameter Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Illegal Parameter Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Illegal Parameter Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Illegal Parameter Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Illegal Parameter Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Illegal Parameter Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Illegal Parameter Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_value_type:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Illegal Value Type: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Illegal Value Type: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Illegal Value Type: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Illegal Value Type: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Illegal Value Type: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Illegal Value Type: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Illegal Value Type: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Illegal Value Type: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Illegal Value Type: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Illegal Value Type: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_result_length:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Invalid Result Length.",
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
                                                                     ") Invalid Result Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Invalid Result Length.",
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
                                                                     L") Invalid Result Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Invalid Result Length.",
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
                                                                     u8") Invalid Result Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Invalid Result Length.",
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
                                                                     u") Invalid Result Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Invalid Result Length.",
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
                                                                     U") Invalid Result Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }

                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_result_length:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Illegal Result Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Illegal Result Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Illegal Result Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Illegal Result Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Illegal Result Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Illegal Result Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Illegal Result Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Illegal Result Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Illegal Result Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Illegal Result Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1_not_allow_multi_value:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                ") In the WebAssembly Release 1.0 (2019-07-20), the length of the result type vector of a valid function type may be at most 1. This restriction may be removed in future versions.",
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
                        ") In the WebAssembly Release 1.0 (2019-07-20), the length of the result type vector of a valid function type may be at most 1. This restriction may be removed in future versions.",
                        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                L") In the WebAssembly Release 1.0 (2019-07-20), the length of the result type vector of a valid function type may be at most 1. This restriction may be removed in future versions.",
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
                        L") In the WebAssembly Release 1.0 (2019-07-20), the length of the result type vector of a valid function type may be at most 1. This restriction may be removed in future versions.",
                        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                u8") In the WebAssembly Release 1.0 (2019-07-20), the length of the result type vector of a valid function type may be at most 1. This restriction may be removed in future versions.",
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
                        u8") In the WebAssembly Release 1.0 (2019-07-20), the length of the result type vector of a valid function type may be at most 1. This restriction may be removed in future versions.",
                        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                u") In the WebAssembly Release 1.0 (2019-07-20), the length of the result type vector of a valid function type may be at most 1. This restriction may be removed in future versions.",
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
                        u") In the WebAssembly Release 1.0 (2019-07-20), the length of the result type vector of a valid function type may be at most 1. This restriction may be removed in future versions.",
                        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                U") In the WebAssembly Release 1.0 (2019-07-20), the length of the result type vector of a valid function type may be at most 1. This restriction may be removed in future versions.",
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
                        U") In the WebAssembly Release 1.0 (2019-07-20), the length of the result type vector of a valid function type may be at most 1. This restriction may be removed in future versions.",
                        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_prefix:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Illegal Type Prefix: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Illegal Type Prefix: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Illegal Type Prefix: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Illegal Type Prefix: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Illegal Type Prefix: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Illegal Type Prefix: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Illegal Type Prefix: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Illegal Type Prefix: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Illegal Type Prefix: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Illegal Type Prefix: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_section:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Duplicate WebAssembly Section: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Duplicate WebAssembly Section: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u8,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Duplicate WebAssembly Section: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Duplicate WebAssembly Section: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u8,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Duplicate WebAssembly Section: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Duplicate WebAssembly Section: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u8,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Duplicate WebAssembly Section: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Duplicate WebAssembly Section: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u8,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Duplicate WebAssembly Section: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Duplicate WebAssembly Section: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u8,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_count:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Invalid Type Count.",
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
                                                                     ") Invalid Type Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Invalid Type Count.",
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
                                                                     L") Invalid Type Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Invalid Type Count.",
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
                                                                     u8") Invalid Type Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Invalid Type Count.",
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
                                                                     u") Invalid Type Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Invalid Type Count.",
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
                                                                     U") Invalid Type Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::type_section_resolved_exceeded_the_actual_number:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") The number of types resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") The number of types resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") The number of types resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") The number of types resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") The number of types resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") The number of types resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") The number of types resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") The number of types resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") The number of types resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") The number of types resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::type_section_resolved_not_match_the_actual_number:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") The number of types resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") The number of types resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") The number of types resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") The number of types resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") The number of types resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") The number of types resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") The number of types resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") The number of types resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") The number of types resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") The number of types resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::forward_dependency_missing:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") The \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\" section must appear before \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\" section.",
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
                                                                     ") The \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u8arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\" section must appear before \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u8arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\" section.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") The \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\" section must appear before \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\" section.",
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
                                                                     L") The \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u8arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\" section must appear before \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u8arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\" section.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") The \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\" section must appear before \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\" section.",
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
                                                                     u8") The \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u8arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\" section must appear before \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u8arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\" section.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") The \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\" section must appear before \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\" section.",
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
                                                                     u") The \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u8arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\" section must appear before \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u8arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\" section.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") The \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\" section must appear before \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u8arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\" section.",
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
                                                                     U") The \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u8arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\" section must appear before \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u8arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\" section.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_import_count:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Invalid Import Count.",
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
                                                                     ") Invalid Import Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Invalid Import Count.",
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
                                                                     L") Invalid Import Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Invalid Import Count.",
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
                                                                     u8") Invalid Import Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Invalid Import Count.",
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
                                                                     u") Invalid Import Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Invalid Import Count.",
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
                                                                     U") Invalid Import Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_section_resolved_exceeded_the_actual_number:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") The number of imports resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") The number of imports resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") The number of imports resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") The number of imports resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") The number of imports resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") The number of imports resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") The number of imports resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") The number of imports resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") The number of imports resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") The number of imports resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_section_resolved_not_match_the_actual_number:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") The number of imports resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") The number of imports resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") The number of imports resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") The number of imports resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") The number of imports resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") The number of imports resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") The number of imports resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") The number of imports resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") The number of imports resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") The number of imports resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_import_module_name_length:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Invalid Module Name Length.",
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
                                                                     ") Invalid Module Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Invalid Module Name Length.",
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
                                                                     L") Invalid Module Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Invalid Module Name Length.",
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
                                                                     u8") Invalid Module Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Invalid Module Name Length.",
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
                                                                     u") Invalid Module Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Invalid Module Name Length.",
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
                                                                     U") Invalid Module Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_module_name_length_cannot_be_zero:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Imported module name should not have strings of length 0.",
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
                                                                     ") Imported module name should not have strings of length 0.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Imported module name should not have strings of length 0.",
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
                                                                     L") Imported module name should not have strings of length 0.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Imported module name should not have strings of length 0.",
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
                                                                     u8") Imported module name should not have strings of length 0.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Imported module name should not have strings of length 0.",
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
                                                                     u") Imported module name should not have strings of length 0.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Imported module name should not have strings of length 0.",
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
                                                                     U") Imported module name should not have strings of length 0.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_module_name_too_length:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Imported Module Name Too Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Imported Module Name Too Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Imported Module Name Too Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Imported Module Name Too Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Imported Module Name Too Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Imported Module Name Too Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Imported Module Name Too Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Imported Module Name Too Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Imported Module Name Too Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Imported Module Name Too Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_import_extern_name_length:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Invalid Imported Extern Name Length.",
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
                                                                     ") Invalid Imported Extern Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Invalid Imported Extern Name Length.",
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
                                                                     L") Invalid Imported Extern Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Invalid Imported Extern Name Length.",
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
                                                                     u8") Invalid Imported Extern Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Invalid Imported Extern Name Length.",
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
                                                                     u") Invalid Imported Extern Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Invalid Imported Extern Name Length.",
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
                                                                     U") Invalid Imported Extern Name Length.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_extern_name_length_cannot_be_zero:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Imported extern name should not have strings of length 0.",
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
                                                                     ") Imported extern name should not have strings of length 0.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Imported extern name should not have strings of length 0.",
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
                                                                     L") Imported extern name should not have strings of length 0.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Imported extern name should not have strings of length 0.",
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
                                                                     u8") Imported extern name should not have strings of length 0.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Imported extern name should not have strings of length 0.",
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
                                                                     u") Imported extern name should not have strings of length 0.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Imported extern name should not have strings of length 0.",
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
                                                                     U") Imported extern name should not have strings of length 0.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_extern_name_too_length:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Imported Extern Name Too Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Imported Extern Name Too Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Imported Extern Name Too Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Imported Extern Name Too Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Imported Extern Name Too Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Imported Extern Name Too Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Imported Extern Name Too Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Imported Extern Name Too Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Imported Extern Name Too Length: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Imported Extern Name Too Length: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::import_missing_import_type:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Cannot Find Import Type.",
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
                                                                     ") Cannot Find Import Type.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Cannot Find Import Type.",
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
                                                                     L") Cannot Find Import Type.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Cannot Find Import Type.",
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
                                                                     u8") Cannot Find Import Type.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Cannot Find Import Type.",
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
                                                                     u") Cannot Find Import Type.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Cannot Find Import Type.",
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
                                                                     U") Cannot Find Import Type.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_importdesc_prefix:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Illegal Importdesc Prefix: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Illegal Importdesc Prefix: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Illegal Importdesc Prefix: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Illegal Importdesc Prefix: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Illegal Importdesc Prefix: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Illegal Importdesc Prefix: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Illegal Importdesc Prefix: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Illegal Importdesc Prefix: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Illegal Importdesc Prefix: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Illegal Importdesc Prefix: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_type_index:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Invalid Type Index.",
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
                                                                     ") Invalid Type Index.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Invalid Type Index.",
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
                                                                     L") Invalid Type Index.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Invalid Type Index.",
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
                                                                     u8") Invalid Type Index.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Invalid Type Index.",
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
                                                                     u") Invalid Type Index.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Invalid Type Index.",
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
                                                                     U") Invalid Type Index.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::illegal_type_index:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Illegal Type Index: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Illegal Type Index: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Illegal Type Index: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Illegal Type Index: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Illegal Type Index: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Illegal Type Index: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Illegal Type Index: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Illegal Type Index: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Illegal Type Index: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Illegal Type Index: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::not_enough_space:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Not Enough Space, remaining: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::addrvw(errout.err.err_selectable.err_end - errout.err.err_curr),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Not Enough Space, remaining: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     ::fast_io::mnp::addrvw(errout.err.err_selectable.err_end - errout.err.err_curr),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Not Enough Space, remaining: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::addrvw(errout.err.err_selectable.err_end - errout.err.err_curr),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Not Enough Space, remaining: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     ::fast_io::mnp::addrvw(errout.err.err_selectable.err_end - errout.err.err_curr),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Not Enough Space, remaining: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::addrvw(errout.err.err_selectable.err_end - errout.err.err_curr),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Not Enough Space, remaining: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     ::fast_io::mnp::addrvw(errout.err.err_selectable.err_end - errout.err.err_curr),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Not Enough Space, remaining: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::addrvw(errout.err.err_selectable.err_end - errout.err.err_curr),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Not Enough Space, remaining: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     ::fast_io::mnp::addrvw(errout.err.err_selectable.err_end - errout.err.err_curr),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Not Enough Space, remaining: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::addrvw(errout.err.err_selectable.err_end - errout.err.err_curr),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Not Enough Space, remaining: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     ::fast_io::mnp::addrvw(errout.err.err_selectable.err_end - errout.err.err_curr),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::table_type_cannot_find_element:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Cannot find element.",
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
                                                                     ") Cannot find element.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Cannot find element.",
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
                                                                     L") Cannot find element.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Cannot find element.",
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
                                                                     u8") Cannot find element.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Cannot find element.",
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
                                                                     u") Cannot find element.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Cannot find element.",
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
                                                                     U") Cannot find element.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }

                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::table_type_illegal_element:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Illegal Element: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Illegal Element: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Illegal Element: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Illegal Element: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Illegal Element: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Illegal Element: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Illegal Element: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Illegal Element: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Illegal Element: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Illegal Element: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::limit_type_cannot_find_flag:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Cannot Find Flags.",
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
                                                                     ") Cannot Find Flags.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Cannot Find Flags.",
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
                                                                     L") Cannot Find Flags.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Cannot Find Flags.",
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
                                                                     u8") Cannot Find Flags.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Cannot Find Flags.",
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
                                                                     u") Cannot Find Flags.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Cannot Find Flags.",
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
                                                                     U") Cannot Find Flags.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }

                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::limit_type_illegal_flag:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Illegal Flag: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Illegal Flag: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Illegal Flag: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Illegal Flag: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Illegal Flag: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Illegal Flag: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Illegal Flag: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Illegal Flag: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Illegal Flag: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Illegal Flag: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::limit_type_invalid_min:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Invalid Minimum Limit.",
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
                                                                     ") Invalid Minimum Limit.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Invalid Minimum Limit.",
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
                                                                     L") Invalid Minimum Limit.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Invalid Minimum Limit.",
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
                                                                     u8") Invalid Minimum Limit.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Invalid Minimum Limit.",
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
                                                                     u") Invalid Minimum Limit.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Invalid Minimum Limit.",
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
                                                                     U") Invalid Minimum Limit.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::limit_type_invalid_max:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Invalid Maximum Limit.",
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
                                                                     ") Invalid Maximum Limit.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Invalid Maximum Limit.",
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
                                                                     L") Invalid Maximum Limit.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Invalid Maximum Limit.",
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
                                                                     u8") Invalid Maximum Limit.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Invalid Maximum Limit.",
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
                                                                     u") Invalid Maximum Limit.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Invalid Maximum Limit.",
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
                                                                     U") Invalid Maximum Limit.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_cannot_find_valtype:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Cannot Find Valtype.",
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
                                                                     ") Cannot Find Valtype.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Cannot Find Valtype.",
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
                                                                     L") Cannot Find Valtype.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Cannot Find Valtype.",
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
                                                                     u8") Cannot Find Valtype.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Cannot Find Valtype.",
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
                                                                     u") Cannot Find Valtype.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Cannot Find Valtype.",
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
                                                                     U") Cannot Find Valtype.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_illegal_valtype:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Illegal Value Type In Global: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Illegal Value Type In Global: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Illegal Value Type In Global: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Illegal Value Type In Global: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Illegal Value Type In Global: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Illegal Value Type In Global: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Illegal Value Type In Global: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Illegal Value Type In Global: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Illegal Value Type In Global: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Illegal Value Type In Global: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_cannot_find_mut:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Cannot Find Mut.",
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
                                                                     ") Cannot Find Mut.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Cannot Find Mut.",
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
                                                                     L") Cannot Find Mut.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Cannot Find Mut.",
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
                                                                     u8") Cannot Find Mut.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Cannot Find Mut.",
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
                                                                     u") Cannot Find Mut.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Cannot Find Mut.",
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
                                                                     U") Cannot Find Mut.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::global_type_illegal_mut:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Illegal Mut In Global: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") Illegal Mut In Global: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Illegal Mut In Global: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") Illegal Mut In Global: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Illegal Mut In Global: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") Illegal Mut In Global: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Illegal Mut In Global: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") Illegal Mut In Global: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Illegal Mut In Global: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") Illegal Mut In Global: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     ::fast_io::mnp::hex0x<true>(errout.err.err_selectable.u8),
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_func_count:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") Invalid Function Count.",
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
                                                                     ") Invalid Function Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") Invalid Function Count.",
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
                                                                     L") Invalid Function Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") Invalid Function Count.",
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
                                                                     u8") Invalid Function Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") Invalid Function Count.",
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
                                                                     u") Invalid Function Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") Invalid Function Count.",
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
                                                                     U") Invalid Function Count.",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_exceeded_the_actual_number:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") The number of functions resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") The number of functions resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") The number of functions resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") The number of functions resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") The number of functions resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") The number of functions resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") The number of functions resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") The number of functions resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") The number of functions resolved exceeded the actual number: \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32,
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") The number of functions resolved exceeded the actual number: \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u32,
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
            case ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_section_resolved_not_match_the_actual_number:
            {
                if constexpr(::std::same_as<char_type, char>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             ") The number of functions resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             "\".",
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
                                                                     ") The number of functions resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                                     "\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             L") The number of functions resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             L"\".",
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
                                                                     L") The number of functions resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                                     L"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u8") The number of functions resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u8"\".",
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
                                                                     u8") The number of functions resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                                     u8"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             u") The number of functions resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             u"\".",
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
                                                                     u") The number of functions resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                                     u"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
                    return;
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
                    if constexpr(::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::wide_nt, char_type>> ||
                                 ::std::same_as<::std::remove_cvref_t<Stm>,
                                                ::fast_io::basic_win32_family_io_observer<::fast_io::win32_family::ansi_9x, char_type>> ||
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
                                                                             U") The number of functions resolved \"",
                                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                                             errout.err.err_selectable.u32arr[0],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\" does not match the actual number \"",
                                                                             UWVM_WIN32_TEXTATTR_LT_GREEN,
                                                                             errout.err.err_selectable.u32arr[1],
                                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                                             U"\".",
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
                                                                     U") The number of functions resolved \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                                     errout.err.err_selectable.u32arr[0],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\" does not match the actual number \"",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_LT_GREEN),
                                                                     errout.err.err_selectable.u32arr[1],
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                                     U"\".",
                                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
                    return;
                }
                return;
            }
        }
    }

}  // namespace uwvm2::parser::wasm::base

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/ansies/ansi_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/utils/ansies/win32_text_attr_pop_macro.h>
#endif
