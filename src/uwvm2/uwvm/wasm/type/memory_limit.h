/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-04-23
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
# include <concepts>
# include <cstddef>
# include <limits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::type
{
    // Runtime-facing module memory limits are expressed in Wasm pages.
    // Unlike parser-side wasm1::limits_type, these use size_t so CLI overrides can
    // carry host policy values without rewriting the parsed module.
    struct module_memory_limit_t
    {
        inline static constexpr ::std::size_t default_max{::std::numeric_limits<::std::size_t>::max()};

        ::std::size_t min{};
        ::std::size_t max{default_max};
        bool present_max{};
    };

    struct module_memory_limit_section_details_wrapper_t
    {
        module_memory_limit_t limits{};
    };

    inline constexpr module_memory_limit_section_details_wrapper_t section_details(module_memory_limit_t limits) noexcept { return {limits}; }

    template <::std::integral char_type, typename Stm>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, module_memory_limit_section_details_wrapper_t>,
                                       Stm&& stream,
                                       module_memory_limit_section_details_wrapper_t const wrapper)
    {
        if(wrapper.limits.present_max)
        {
            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(
                    ::std::forward<Stm>(stream), "limits: {min: ", wrapper.limits.min, ", max: ", wrapper.limits.max, "}");
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(
                    ::std::forward<Stm>(stream), L"limits: {min: ", wrapper.limits.min, L", max: ", wrapper.limits.max, L"}");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(
                    ::std::forward<Stm>(stream), u8"limits: {min: ", wrapper.limits.min, u8", max: ", wrapper.limits.max, u8"}");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(
                    ::std::forward<Stm>(stream), u"limits: {min: ", wrapper.limits.min, u", max: ", wrapper.limits.max, u"}");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(
                    ::std::forward<Stm>(stream), U"limits: {min: ", wrapper.limits.min, U", max: ", wrapper.limits.max, U"}");
            }
        }
        else
        {
            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(
                    ::std::forward<Stm>(stream), "limits: {min: ", wrapper.limits.min, ", max: unbounded}");
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(
                    ::std::forward<Stm>(stream), L"limits: {min: ", wrapper.limits.min, L", max: unbounded}");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(
                    ::std::forward<Stm>(stream), u8"limits: {min: ", wrapper.limits.min, u8", max: unbounded}");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(
                    ::std::forward<Stm>(stream), u"limits: {min: ", wrapper.limits.min, u", max: unbounded}");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(
                    ::std::forward<Stm>(stream), U"limits: {min: ", wrapper.limits.min, U", max: unbounded}");
            }
        }
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
