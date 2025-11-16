/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-07-03
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

constexpr auto get_exceeding_imported_type_name{
    []<::std::integral char_type2>(::std::uint_least8_t type) constexpr noexcept -> ::uwvm2::utils::container::basic_string_view<char_type2>
    {
        switch(type)
        {
            case 0u:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"func"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"func"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"func"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"func"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"func"}; }
            }
            case 1u:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"table"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"table"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"table"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"table"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"table"}; }
            }
            case 2u:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"mem"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"mem"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"mem"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"mem"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"mem"}; }
            }
            case 3u:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"global"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"global"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"global"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"global"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"global"}; }
            }
            case 4u:
            {
                if constexpr(::std::same_as<char_type2, char>) { return {"tag"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"tag"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"tag"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"tag"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"tag"}; }
            }
            [[unlikely]] default:
            {
            /// @warning Maybe I forgot to realize it.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif

                if constexpr(::std::same_as<char_type2, char>) { return {"unknown"}; }
                else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"unknown"}; }
                else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"unknown"}; }
                else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"unknown"}; }
                else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"unknown"}; }
            }
        }
    }};

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
                ") Importing (",
                UWVM_WIN32_TEXTATTR_CYAN,
                errout.err.err_selectable.imp_def_num_exceed_u32max.imported,
                UWVM_WIN32_TEXTATTR_WHITE,
                ") and defining (",
                UWVM_WIN32_TEXTATTR_CYAN,
                errout.err.err_selectable.imp_def_num_exceed_u32max.defined,
                UWVM_WIN32_TEXTATTR_WHITE,
                ") the number of ",
                get_exceeding_imported_type_name.template operator()<char>(errout.err.err_selectable.imp_def_num_exceed_u32max.type),
                "'s and exceeding u32 max.",
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
        ") Importing (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
        errout.err.err_selectable.imp_def_num_exceed_u32max.imported,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
        ") and defining (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_CYAN),
        errout.err.err_selectable.imp_def_num_exceed_u32max.defined,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
        ") the number of ",
        get_exceeding_imported_type_name.template operator()<char>(errout.err.err_selectable.imp_def_num_exceed_u32max.type),
        "'s and exceeding u32 max.",
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
                L") Importing (",
                UWVM_WIN32_TEXTATTR_CYAN,
                errout.err.err_selectable.imp_def_num_exceed_u32max.imported,
                UWVM_WIN32_TEXTATTR_WHITE,
                L") and defining (",
                UWVM_WIN32_TEXTATTR_CYAN,
                errout.err.err_selectable.imp_def_num_exceed_u32max.defined,
                UWVM_WIN32_TEXTATTR_WHITE,
                L") the number of ",
                get_exceeding_imported_type_name.template operator()<wchar_t>(errout.err.err_selectable.imp_def_num_exceed_u32max.type),
                L"'s and exceeding u32 max.",
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
        L") Importing (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
        errout.err.err_selectable.imp_def_num_exceed_u32max.imported,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
        L") and defining (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_CYAN),
        errout.err.err_selectable.imp_def_num_exceed_u32max.defined,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
        L") the number of ",
        get_exceeding_imported_type_name.template operator()<wchar_t>(errout.err.err_selectable.imp_def_num_exceed_u32max.type),
        L"'s and exceeding u32 max.",
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
                u8") Importing (",
                UWVM_WIN32_TEXTATTR_CYAN,
                errout.err.err_selectable.imp_def_num_exceed_u32max.imported,
                UWVM_WIN32_TEXTATTR_WHITE,
                u8") and defining (",
                UWVM_WIN32_TEXTATTR_CYAN,
                errout.err.err_selectable.imp_def_num_exceed_u32max.defined,
                UWVM_WIN32_TEXTATTR_WHITE,
                u8") the number of ",
                get_exceeding_imported_type_name.template operator()<char8_t>(errout.err.err_selectable.imp_def_num_exceed_u32max.type),
                u8"'s and exceeding u32 max.",
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
        u8") Importing (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
        errout.err.err_selectable.imp_def_num_exceed_u32max.imported,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
        u8") and defining (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_CYAN),
        errout.err.err_selectable.imp_def_num_exceed_u32max.defined,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
        u8") the number of ",
        get_exceeding_imported_type_name.template operator()<char8_t>(errout.err.err_selectable.imp_def_num_exceed_u32max.type),
        u8"'s and exceeding u32 max.",
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
                u") Importing (",
                UWVM_WIN32_TEXTATTR_CYAN,
                errout.err.err_selectable.imp_def_num_exceed_u32max.imported,
                UWVM_WIN32_TEXTATTR_WHITE,
                u") and defining (",
                UWVM_WIN32_TEXTATTR_CYAN,
                errout.err.err_selectable.imp_def_num_exceed_u32max.defined,
                UWVM_WIN32_TEXTATTR_WHITE,
                u") the number of ",
                get_exceeding_imported_type_name.template operator()<char16_t>(errout.err.err_selectable.imp_def_num_exceed_u32max.type),
                u"'s and exceeding u32 max.",
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
        u") Importing (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
        errout.err.err_selectable.imp_def_num_exceed_u32max.imported,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
        u") and defining (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_CYAN),
        errout.err.err_selectable.imp_def_num_exceed_u32max.defined,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
        u") the number of ",
        get_exceeding_imported_type_name.template operator()<char16_t>(errout.err.err_selectable.imp_def_num_exceed_u32max.type),
        u"'s and exceeding u32 max.",
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
                U") Importing (",
                UWVM_WIN32_TEXTATTR_CYAN,
                errout.err.err_selectable.imp_def_num_exceed_u32max.imported,
                UWVM_WIN32_TEXTATTR_WHITE,
                U") and defining (",
                UWVM_WIN32_TEXTATTR_CYAN,
                errout.err.err_selectable.imp_def_num_exceed_u32max.defined,
                UWVM_WIN32_TEXTATTR_WHITE,
                U") the number of ",
                get_exceeding_imported_type_name.template operator()<char32_t>(errout.err.err_selectable.imp_def_num_exceed_u32max.type),
                U"'s and exceeding u32 max.",
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
        U") Importing (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
        errout.err.err_selectable.imp_def_num_exceed_u32max.imported,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
        U") and defining (",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_CYAN),
        errout.err.err_selectable.imp_def_num_exceed_u32max.defined,
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
        U") the number of ",
        get_exceeding_imported_type_name.template operator()<char32_t>(errout.err.err_selectable.imp_def_num_exceed_u32max.type),
        U"'s and exceeding u32 max.",
        ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
    return;
}
