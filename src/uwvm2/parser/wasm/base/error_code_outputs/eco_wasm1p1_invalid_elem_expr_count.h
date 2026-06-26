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

// Without pragma once, this header file will be included in a specific code segment

auto const wasm1p1_print_error = [&]<typename... Args>(Args&&... args)
{
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
                                                         ") ",
                                                         ::std::forward<Args>(args)...,
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_RST_ALL));
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
                                                         L") ",
                                                         ::std::forward<Args>(args)...,
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_W_RST_ALL));
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
                                                         u8") ",
                                                         ::std::forward<Args>(args)...,
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U8_RST_ALL));
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
                                                         u") ",
                                                         ::std::forward<Args>(args)...,
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U16_RST_ALL));
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
                                                         U") ",
                                                         ::std::forward<Args>(args)...,
                                                         ::fast_io::mnp::cond(enable_ansi, UWVM_AES_U32_RST_ALL));
    }
};

if constexpr(::std::same_as<char_type, char>) { wasm1p1_print_error("Invalid WebAssembly 1.1 element expression count."); }
else if constexpr(::std::same_as<char_type, wchar_t>) { wasm1p1_print_error(L"Invalid WebAssembly 1.1 element expression count."); }
else if constexpr(::std::same_as<char_type, char8_t>) { wasm1p1_print_error(u8"Invalid WebAssembly 1.1 element expression count."); }
else if constexpr(::std::same_as<char_type, char16_t>) { wasm1p1_print_error(u"Invalid WebAssembly 1.1 element expression count."); }
else if constexpr(::std::same_as<char_type, char32_t>) { wasm1p1_print_error(U"Invalid WebAssembly 1.1 element expression count."); }
return;
