/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      YexuanXiao
 * @version     2.0.0
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
# include <version>
# include <cstdint>
# include <cstddef>
# include <cstring>
# include <concepts>
# include <memory>
// import
# include <fast_io.h>
# include "wrapper.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::container
{
    // fast_io::basic_general_concat owns aliasing and character-dependent status forwarding.
    // Pass the original value categories through exactly once: pre-normalizing here repeats
    // user CPOs, and testing a dummy stream can reject a valid destination-specific printer.
    // The same contract applies to the thread-local allocator wrappers below.
    template <::std::integral char_type, typename... Args>
    inline constexpr ::uwvm2::utils::container::basic_string<char_type> basic_concat_uwvm(Args && ... args)
    { return ::fast_io::basic_general_concat<false, char_type, ::uwvm2::utils::container::basic_string<char_type>>(static_cast<Args&&>(args)...); }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::string concat_uwvm(Args && ... args)
    { return ::fast_io::basic_general_concat<false, char, ::uwvm2::utils::container::string>(static_cast<Args&&>(args)...); }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::wstring wconcat_uwvm(Args && ... args)
    { return ::fast_io::basic_general_concat<false, wchar_t, ::uwvm2::utils::container::wstring>(static_cast<Args&&>(args)...); }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::u8string u8concat_uwvm(Args && ... args)
    { return ::fast_io::basic_general_concat<false, char8_t, ::uwvm2::utils::container::u8string>(static_cast<Args&&>(args)...); }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::u16string u16concat_uwvm(Args && ... args)
    { return ::fast_io::basic_general_concat<false, char16_t, ::uwvm2::utils::container::u16string>(static_cast<Args&&>(args)...); }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::u32string u32concat_uwvm(Args && ... args)
    { return ::fast_io::basic_general_concat<false, char32_t, ::uwvm2::utils::container::u32string>(static_cast<Args&&>(args)...); }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::string concatln_uwvm(Args && ... args)
    { return ::fast_io::basic_general_concat<true, char, ::uwvm2::utils::container::string>(static_cast<Args&&>(args)...); }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::wstring wconcatln_uwvm(Args && ... args)
    { return ::fast_io::basic_general_concat<true, wchar_t, ::uwvm2::utils::container::wstring>(static_cast<Args&&>(args)...); }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::u8string u8concatln_uwvm(Args && ... args)
    { return ::fast_io::basic_general_concat<true, char8_t, ::uwvm2::utils::container::u8string>(static_cast<Args&&>(args)...); }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::u16string u16concatln_uwvm(Args && ... args)
    { return ::fast_io::basic_general_concat<true, char16_t, ::uwvm2::utils::container::u16string>(static_cast<Args&&>(args)...); }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::u32string u32concatln_uwvm(Args && ... args)
    { return ::fast_io::basic_general_concat<true, char32_t, ::uwvm2::utils::container::u32string>(static_cast<Args&&>(args)...); }

    namespace tlc
    {

        template <::std::integral char_type, typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::basic_string<char_type> basic_concat_uwvm_tlc(Args&&... args)
        { return ::fast_io::basic_general_concat<false, char_type, ::uwvm2::utils::container::tlc::basic_string<char_type>>(static_cast<Args&&>(args)...); }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::string concat_uwvm_tlc(Args&&... args)
        { return ::fast_io::basic_general_concat<false, char, ::uwvm2::utils::container::tlc::string>(static_cast<Args&&>(args)...); }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::wstring wconcat_uwvm_tlc(Args&&... args)
        { return ::fast_io::basic_general_concat<false, wchar_t, ::uwvm2::utils::container::tlc::wstring>(static_cast<Args&&>(args)...); }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::u8string u8concat_uwvm_tlc(Args&&... args)
        { return ::fast_io::basic_general_concat<false, char8_t, ::uwvm2::utils::container::tlc::u8string>(static_cast<Args&&>(args)...); }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::u16string u16concat_uwvm_tlc(Args&&... args)
        { return ::fast_io::basic_general_concat<false, char16_t, ::uwvm2::utils::container::tlc::u16string>(static_cast<Args&&>(args)...); }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::u32string u32concat_uwvm_tlc(Args&&... args)
        { return ::fast_io::basic_general_concat<false, char32_t, ::uwvm2::utils::container::tlc::u32string>(static_cast<Args&&>(args)...); }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::string concatln_uwvm_tlc(Args&&... args)
        { return ::fast_io::basic_general_concat<true, char, ::uwvm2::utils::container::tlc::string>(static_cast<Args&&>(args)...); }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::wstring wconcatln_uwvm_tlc(Args&&... args)
        { return ::fast_io::basic_general_concat<true, wchar_t, ::uwvm2::utils::container::tlc::wstring>(static_cast<Args&&>(args)...); }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::u8string u8concatln_uwvm_tlc(Args&&... args)
        { return ::fast_io::basic_general_concat<true, char8_t, ::uwvm2::utils::container::tlc::u8string>(static_cast<Args&&>(args)...); }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::u16string u16concatln_uwvm_tlc(Args&&... args)
        { return ::fast_io::basic_general_concat<true, char16_t, ::uwvm2::utils::container::tlc::u16string>(static_cast<Args&&>(args)...); }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::u32string u32concatln_uwvm_tlc(Args&&... args)
        { return ::fast_io::basic_general_concat<true, char32_t, ::uwvm2::utils::container::tlc::u32string>(static_cast<Args&&>(args)...); }

    }  // namespace tlc
}
