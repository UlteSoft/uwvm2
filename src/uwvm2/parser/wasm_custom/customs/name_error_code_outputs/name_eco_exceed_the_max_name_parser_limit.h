/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 */

// Without pragma once, this header file will be included in a specific code segment

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
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             "[warn]  ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             "(offset=",
                                                             ::fast_io::mnp::addrvw(errout.name_err.curr - errout.name_begin),
                                                             ") Exceed The Max Name Parser Limit: name=\"",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             ::fast_io::mnp::code_cvt(errout.name_err.err.exceed_the_max_name_parser_limit.name),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             "\", value=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             errout.name_err.err.exceed_the_max_name_parser_limit.value,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             " > limit=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             errout.name_err.err.exceed_the_max_name_parser_limit.maxval,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             ".",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL UWVM_AES_WHITE),
                                                     "uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_YELLOW),
                                                     "[warn]  ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                     "(offset=",
                                                     ::fast_io::mnp::addrvw(errout.name_err.curr - errout.name_begin),
                                                     ") Exceed The Max Name Parser Limit: name=\"",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                     ::fast_io::mnp::code_cvt(errout.name_err.err.exceed_the_max_name_parser_limit.name),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                     "\", value=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                     errout.name_err.err.exceed_the_max_name_parser_limit.value,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                     " > limit=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
                                                     errout.name_err.err.exceed_the_max_name_parser_limit.maxval,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
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
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             L"[warn]  ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             L"(offset=",
                                                             ::fast_io::mnp::addrvw(errout.name_err.curr - errout.name_begin),
                                                             L") Exceed The Max Name Parser Limit: name=\"",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             ::fast_io::mnp::code_cvt(errout.name_err.err.exceed_the_max_name_parser_limit.name),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             L"\", value=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             errout.name_err.err.exceed_the_max_name_parser_limit.value,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             L" > limit=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             errout.name_err.err.exceed_the_max_name_parser_limit.maxval,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             L".",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL UWVM_AES_W_WHITE),
                                                     L"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_YELLOW),
                                                     L"[warn]  ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                     L"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.name_err.curr - errout.name_begin),
                                                     L") Exceed The Max Name Parser Limit: name=\"",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                     ::fast_io::mnp::code_cvt(errout.name_err.err.exceed_the_max_name_parser_limit.name),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                     L"\", value=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                     errout.name_err.err.exceed_the_max_name_parser_limit.value,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                     L" > limit=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
                                                     errout.name_err.err.exceed_the_max_name_parser_limit.maxval,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
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
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             u8"[warn]  ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u8"(offset=",
                                                             ::fast_io::mnp::addrvw(errout.name_err.curr - errout.name_begin),
                                                             u8") Exceed The Max Name Parser Limit: name=\"",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             // Same type, no conversion required
                                                             errout.name_err.err.exceed_the_max_name_parser_limit.name,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u8"\", value=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             errout.name_err.err.exceed_the_max_name_parser_limit.value,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u8" > limit=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             errout.name_err.err.exceed_the_max_name_parser_limit.maxval,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u8".",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL UWVM_AES_U8_WHITE),
                                                     u8"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_YELLOW),
                                                     u8"[warn]  ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                     u8"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.name_err.curr - errout.name_begin),
                                                     u8") Exceed The Max Name Parser Limit: name=\"",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                     // Same type, no conversion required
                                                     errout.name_err.err.exceed_the_max_name_parser_limit.name,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                     u8"\", value=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                     errout.name_err.err.exceed_the_max_name_parser_limit.value,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                     u8" > limit=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
                                                     errout.name_err.err.exceed_the_max_name_parser_limit.maxval,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
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
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             u"[warn]  ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u"(offset=",
                                                             ::fast_io::mnp::addrvw(errout.name_err.curr - errout.name_begin),
                                                             u") Exceed The Max Name Parser Limit: name=\"",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             ::fast_io::mnp::code_cvt(errout.name_err.err.exceed_the_max_name_parser_limit.name),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u"\", value=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             errout.name_err.err.exceed_the_max_name_parser_limit.value,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u" > limit=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             errout.name_err.err.exceed_the_max_name_parser_limit.maxval,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             u".",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL UWVM_AES_U16_WHITE),
                                                     u"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_YELLOW),
                                                     u"[warn]  ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                     u"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.name_err.curr - errout.name_begin),
                                                     u") Exceed The Max Name Parser Limit: name=\"",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                     ::fast_io::mnp::code_cvt(errout.name_err.err.exceed_the_max_name_parser_limit.name),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                     u"\", value=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                     errout.name_err.err.exceed_the_max_name_parser_limit.value,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                     u" > limit=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
                                                     errout.name_err.err.exceed_the_max_name_parser_limit.maxval,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
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
                                                             UWVM_WIN32_TEXTATTR_YELLOW,
                                                             U"[warn]  ",
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             U"(offset=",
                                                             ::fast_io::mnp::addrvw(errout.name_err.curr - errout.name_begin),
                                                             U") Exceed The Max Name Parser Limit: name=\"",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             ::fast_io::mnp::code_cvt(errout.name_err.err.exceed_the_max_name_parser_limit.name),
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             U"\", value=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             errout.name_err.err.exceed_the_max_name_parser_limit.value,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             U" > limit=",
                                                             UWVM_WIN32_TEXTATTR_CYAN,
                                                             errout.name_err.err.exceed_the_max_name_parser_limit.maxval,
                                                             UWVM_WIN32_TEXTATTR_WHITE,
                                                             U".",
                                                             UWVM_WIN32_TEXTATTR_RST_ALL);
            return;
        }
    }
#endif
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL UWVM_AES_U32_WHITE),
                                                     U"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_YELLOW),
                                                     U"[warn]  ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                     U"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.name_err.curr - errout.name_begin),
                                                     U") Exceed The Max Name Parser Limit: name=\"",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                     ::fast_io::mnp::code_cvt(errout.name_err.err.exceed_the_max_name_parser_limit.name),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                     U"\", value=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                     errout.name_err.err.exceed_the_max_name_parser_limit.value,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                     U" > limit=",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
                                                     errout.name_err.err.exceed_the_max_name_parser_limit.maxval,
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                     U".",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
    return;
}

