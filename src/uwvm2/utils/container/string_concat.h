/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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
    template <::std::integral char_type, typename... Args>
    inline constexpr ::uwvm2::utils::container::basic_string<char_type> basic_concat_uwvm(Args && ... args)
    {
        constexpr bool type_error{::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char_type>, Args...>};
        if constexpr(type_error)
        {
            return ::fast_io::basic_general_concat<false, char_type, ::uwvm2::utils::container::basic_string<char_type>>(
                ::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(args))...);
        }
        else
        {
            static_assert(type_error, "some types are not printable, so we cannot concat ::fast_io::basic_string");
            return {};
        }
    }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::string concat_uwvm(Args && ... args)
    {
        constexpr bool type_error{::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char>, Args...>};
        if constexpr(type_error)
        {
            return ::fast_io::basic_general_concat<false, char, ::uwvm2::utils::container::string>(
                ::fast_io::io_print_forward<char>(::fast_io::io_print_alias(args))...);
        }
        else
        {
            static_assert(type_error, "some types are not printable, so we cannot concat ::fast_io::string");
            return {};
        }
    }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::wstring wconcat_uwvm(Args && ... args)
    {
        constexpr bool type_error{::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<wchar_t>, Args...>};
        if constexpr(type_error)
        {
            return ::fast_io::basic_general_concat<false, wchar_t, ::uwvm2::utils::container::wstring>(
                ::fast_io::io_print_forward<wchar_t>(::fast_io::io_print_alias(args))...);
        }
        else
        {
            static_assert(type_error, "some types are not printable, so we cannot concat ::fast_io::wstring");
            return {};
        }
    }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::u8string u8concat_uwvm(Args && ... args)
    {
        constexpr bool type_error{::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char8_t>, Args...>};
        if constexpr(type_error)
        {
            return ::fast_io::basic_general_concat<false, char8_t, ::uwvm2::utils::container::u8string>(
                ::fast_io::io_print_forward<char8_t>(::fast_io::io_print_alias(args))...);
        }
        else
        {
            static_assert(type_error, "some types are not printable, so we cannot concat ::fast_io::u8string");
            return {};
        }
    }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::u16string u16concat_uwvm(Args && ... args)
    {
        constexpr bool type_error{::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char16_t>, Args...>};
        if constexpr(type_error)
        {
            return ::fast_io::basic_general_concat<false, char16_t, ::uwvm2::utils::container::u16string>(
                ::fast_io::io_print_forward<char16_t>(::fast_io::io_print_alias(args))...);
        }
        else
        {
            static_assert(type_error, "some types are not printable, so we cannot concat ::fast_io::u16string");
            return {};
        }
    }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::u32string u32concat_uwvm(Args && ... args)
    {
        constexpr bool type_error{::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char32_t>, Args...>};
        if constexpr(type_error)
        {
            return ::fast_io::basic_general_concat<false, char32_t, ::uwvm2::utils::container::u32string>(
                ::fast_io::io_print_forward<char32_t>(::fast_io::io_print_alias(args))...);
        }
        else
        {
            static_assert(type_error, "some types are not printable, so we cannot concat ::fast_io::u32string");
            return {};
        }
    }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::string concatln_uwvm(Args && ... args)
    {
        constexpr bool type_error{::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char>, Args...>};
        if constexpr(type_error)
        {
            return ::fast_io::basic_general_concat<true, char, ::uwvm2::utils::container::string>(
                ::fast_io::io_print_forward<char>(::fast_io::io_print_alias(args))...);
        }
        else
        {
            static_assert(type_error, "some types are not printable, so we cannot concatln ::fast_io::string");
            return {};
        }
    }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::wstring wconcatln_uwvm(Args && ... args)
    {
        constexpr bool type_error{::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<wchar_t>, Args...>};
        if constexpr(type_error)
        {
            return ::fast_io::basic_general_concat<true, wchar_t, ::uwvm2::utils::container::wstring>(
                ::fast_io::io_print_forward<wchar_t>(::fast_io::io_print_alias(args))...);
        }
        else
        {
            static_assert(type_error, "some types are not printable, so we cannot concatln ::fast_io::wstring");
            return {};
        }
    }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::u8string u8concatln_uwvm(Args && ... args)
    {
        constexpr bool type_error{::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char8_t>, Args...>};
        if constexpr(type_error)
        {
            return ::fast_io::basic_general_concat<true, char8_t, ::uwvm2::utils::container::u8string>(
                ::fast_io::io_print_forward<char8_t>(::fast_io::io_print_alias(args))...);
        }
        else
        {
            static_assert(type_error, "some types are not printable, so we cannot concatln ::fast_io::u8string");
            return {};
        }
    }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::u16string u16concatln_uwvm(Args && ... args)
    {
        constexpr bool type_error{::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char16_t>, Args...>};
        if constexpr(type_error)
        {
            return ::fast_io::basic_general_concat<true, char16_t, ::uwvm2::utils::container::u16string>(
                ::fast_io::io_print_forward<char16_t>(::fast_io::io_print_alias(args))...);
        }
        else
        {
            static_assert(type_error, "some types are not printable, so we cannot concatln ::fast_io::u16string");
            return {};
        }
    }

    template <typename... Args>
    inline constexpr ::uwvm2::utils::container::u32string u32concatln_uwvm(Args && ... args)
    {
        constexpr bool type_error{::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char32_t>, Args...>};
        if constexpr(type_error)
        {
            return ::fast_io::basic_general_concat<true, char32_t, ::uwvm2::utils::container::u32string>(
                ::fast_io::io_print_forward<char32_t>(::fast_io::io_print_alias(args))...);
        }
        else
        {
            static_assert(type_error, "some types are not printable, so we cannot concatln ::fast_io::u32string");
            return {};
        }
    }

    namespace tlc
    {

        template <::std::integral char_type, typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::basic_string<char_type> basic_concat_uwvm_tlc(Args&&... args)
        {
            constexpr bool type_error{
                ::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char_type>, Args...>};
            if constexpr(type_error)
            {
                return ::fast_io::basic_general_concat<false, char_type, ::uwvm2::utils::container::tlc::basic_string<char_type>>(
                    ::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(args))...);
            }
            else
            {
                static_assert(type_error, "some types are not printable, so we cannot concat ::fast_io::tlc::basic_string");
                return {};
            }
        }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::string concat_uwvm_tlc(Args&&... args)
        {
            constexpr bool type_error{::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char>, Args...>};
            if constexpr(type_error)
            {
                return ::fast_io::basic_general_concat<false, char, ::uwvm2::utils::container::tlc::string>(
                    ::fast_io::io_print_forward<char>(::fast_io::io_print_alias(args))...);
            }
            else
            {
                static_assert(type_error, "some types are not printable, so we cannot concat ::fast_io::tlc::string");
                return {};
            }
        }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::wstring wconcat_uwvm_tlc(Args&&... args)
        {
            constexpr bool type_error{
                ::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<wchar_t>, Args...>};
            if constexpr(type_error)
            {
                return ::fast_io::basic_general_concat<false, wchar_t, ::uwvm2::utils::container::tlc::wstring>(
                    ::fast_io::io_print_forward<wchar_t>(::fast_io::io_print_alias(args))...);
            }
            else
            {
                static_assert(type_error, "some types are not printable, so we cannot concat ::fast_io::tlc::wstring");
                return {};
            }
        }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::u8string u8concat_uwvm_tlc(Args&&... args)
        {
            constexpr bool type_error{
                ::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char8_t>, Args...>};
            if constexpr(type_error)
            {
                return ::fast_io::basic_general_concat<false, char8_t, ::uwvm2::utils::container::tlc::u8string>(
                    ::fast_io::io_print_forward<char8_t>(::fast_io::io_print_alias(args))...);
            }
            else
            {
                static_assert(type_error, "some types are not printable, so we cannot concat ::fast_io::tlc::u8string");
                return {};
            }
        }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::u16string u16concat_uwvm_tlc(Args&&... args)
        {
            constexpr bool type_error{
                ::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char16_t>, Args...>};
            if constexpr(type_error)
            {
                return ::fast_io::basic_general_concat<false, char16_t, ::uwvm2::utils::container::tlc::u16string>(
                    ::fast_io::io_print_forward<char16_t>(::fast_io::io_print_alias(args))...);
            }
            else
            {
                static_assert(type_error, "some types are not printable, so we cannot concat ::fast_io::tlc::u16string");
                return {};
            }
        }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::u32string u32concat_uwvm_tlc(Args&&... args)
        {
            constexpr bool type_error{
                ::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char32_t>, Args...>};
            if constexpr(type_error)
            {
                return ::fast_io::basic_general_concat<false, char32_t, ::uwvm2::utils::container::tlc::u32string>(
                    ::fast_io::io_print_forward<char32_t>(::fast_io::io_print_alias(args))...);
            }
            else
            {
                static_assert(type_error, "some types are not printable, so we cannot concat ::fast_io::tlc::u32string");
                return {};
            }
        }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::string concatln_uwvm_tlc(Args&&... args)
        {
            constexpr bool type_error{::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char>, Args...>};
            if constexpr(type_error)
            {
                return ::fast_io::basic_general_concat<true, char, ::uwvm2::utils::container::tlc::string>(
                    ::fast_io::io_print_forward<char>(::fast_io::io_print_alias(args))...);
            }
            else
            {
                static_assert(type_error, "some types are not printable, so we cannot concatln ::fast_io::tlc::string");
                return {};
            }
        }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::wstring wconcatln_uwvm_tlc(Args&&... args)
        {
            constexpr bool type_error{
                ::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<wchar_t>, Args...>};
            if constexpr(type_error)
            {
                return ::fast_io::basic_general_concat<true, wchar_t, ::uwvm2::utils::container::tlc::wstring>(
                    ::fast_io::io_print_forward<wchar_t>(::fast_io::io_print_alias(args))...);
            }
            else
            {
                static_assert(type_error, "some types are not printable, so we cannot concatln ::fast_io::tlc::wstring");
                return {};
            }
        }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::u8string u8concatln_uwvm_tlc(Args&&... args)
        {
            constexpr bool type_error{
                ::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char8_t>, Args...>};
            if constexpr(type_error)
            {
                return ::fast_io::basic_general_concat<true, char8_t, ::uwvm2::utils::container::tlc::u8string>(
                    ::fast_io::io_print_forward<char8_t>(::fast_io::io_print_alias(args))...);
            }
            else
            {
                static_assert(type_error, "some types are not printable, so we cannot concatln ::fast_io::tlc::u8string");
                return {};
            }
        }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::u16string u16concatln_uwvm_tlc(Args&&... args)
        {
            constexpr bool type_error{
                ::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char16_t>, Args...>};
            if constexpr(type_error)
            {
                return ::fast_io::basic_general_concat<true, char16_t, ::uwvm2::utils::container::tlc::u16string>(
                    ::fast_io::io_print_forward<char16_t>(::fast_io::io_print_alias(args))...);
            }
            else
            {
                static_assert(type_error, "some types are not printable, so we cannot concatln ::fast_io::tlc::u16string");
                return {};
            }
        }

        template <typename... Args>
        inline constexpr ::uwvm2::utils::container::tlc::u32string u32concatln_uwvm_tlc(Args&&... args)
        {
            constexpr bool type_error{
                ::fast_io::operations::defines::print_freestanding_okay<::fast_io::details::dummy_buffer_output_stream<char32_t>, Args...>};
            if constexpr(type_error)
            {
                return ::fast_io::basic_general_concat<true, char32_t, ::uwvm2::utils::container::tlc::u32string>(
                    ::fast_io::io_print_forward<char32_t>(::fast_io::io_print_alias(args))...);
            }
            else
            {
                static_assert(type_error, "some types are not printable, so we cannot concatln ::fast_io::tlc::u32string");
                return {};
            }
        }

    }  // namespace tlc
}
