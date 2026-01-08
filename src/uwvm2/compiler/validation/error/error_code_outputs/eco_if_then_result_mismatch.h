/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-01-07
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

auto const& itr{errout.err.err_selectable.if_then_result_mismatch};

auto const expected_is_single{itr.expected_count == 1uz};
auto const actual_is_single{itr.actual_count == 1uz};

auto const expected_type_name{::uwvm2::parser::wasm::standard::wasm1::type::get_value_name<char_type>(
    ::uwvm2::parser::wasm::standard::wasm1::type::section_details(itr.expected_type))};
auto const actual_type_name{::uwvm2::parser::wasm::standard::wasm1::type::get_value_name<char_type>(
    ::uwvm2::parser::wasm::standard::wasm1::type::section_details(itr.actual_type))};

if constexpr(::std::same_as<char_type, char>)
{
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL UWVM_AES_WHITE),
                                                     "uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RED),
                                                     "[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_WHITE),
                                                     "(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     ") If-then branch result mismatch: expected ",
                                                     itr.expected_count);
    if(expected_is_single)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), " (", expected_type_name, ")");
    }
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ", got ", itr.actual_count);
    if(actual_is_single)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), " (", actual_type_name, ")");
    }
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ".",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
    return;
}
else if constexpr(::std::same_as<char_type, wchar_t>)
{
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL UWVM_AES_W_WHITE),
                                                     L"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RED),
                                                     L"[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_WHITE),
                                                     L"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     L") If-then branch result mismatch: expected ",
                                                     itr.expected_count);
    if(expected_is_single)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L" (", expected_type_name, L")");
    }
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L", got ", itr.actual_count);
    if(actual_is_single)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L" (", actual_type_name, L")");
    }
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     L".",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
    return;
}
else if constexpr(::std::same_as<char_type, char8_t>)
{
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL UWVM_AES_U8_WHITE),
                                                     u8"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RED),
                                                     u8"[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_WHITE),
                                                     u8"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     u8") If-then branch result mismatch: expected ",
                                                     itr.expected_count);
    if(expected_is_single)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8" (", expected_type_name, u8")");
    }
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8", got ", itr.actual_count);
    if(actual_is_single)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8" (", actual_type_name, u8")");
    }
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     u8".",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
    return;
}
else if constexpr(::std::same_as<char_type, char16_t>)
{
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL UWVM_AES_U16_WHITE),
                                                     u"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RED),
                                                     u"[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_WHITE),
                                                     u"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     u") If-then branch result mismatch: expected ",
                                                     itr.expected_count);
    if(expected_is_single)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u" (", expected_type_name, u")");
    }
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u", got ", itr.actual_count);
    if(actual_is_single)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u" (", actual_type_name, u")");
    }
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     u".",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
    return;
}
else if constexpr(::std::same_as<char_type, char32_t>)
{
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL UWVM_AES_U32_WHITE),
                                                     U"uwvm: ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RED),
                                                     U"[error] ",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_WHITE),
                                                     U"(offset=",
                                                     ::fast_io::mnp::addrvw(errout.err.err_curr - errout.module_begin),
                                                     U") If-then branch result mismatch: expected ",
                                                     itr.expected_count);
    if(expected_is_single)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U" (", expected_type_name, U")");
    }
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U", got ", itr.actual_count);
    if(actual_is_single)
    {
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U" (", actual_type_name, U")");
    }
    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                     U".",
                                                     ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
    return;
}
