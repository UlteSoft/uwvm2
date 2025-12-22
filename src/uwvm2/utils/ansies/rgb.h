/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-24
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
# include <cstdint>
# include <cstddef>
# include <cstring>
# include <concepts>
// import
# include <fast_io.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::ansies
{
    struct rgb
    {
        ::std::uint_least32_t r{};
        ::std::uint_least32_t g{};
        ::std::uint_least32_t b{};
    };

    template <::std::integral char_type>
        requires (sizeof(char_type) == sizeof(char8_t))
    inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, rgb>) noexcept
    {
        constexpr ::std::size_t ul32_real_size{::fast_io::pr_rsv_size<char_type, ::std::uint_least32_t>};
        constexpr ::std::size_t max_size{10uz + 3uz * ul32_real_size};
        return max_size;
    }

    namespace details
    {
        template <::std::integral char_type>
            requires (sizeof(char_type) == sizeof(char8_t))
        inline constexpr char_type* rgb_print_reserve_impl(char_type* iter, ::std::uint_least32_t r, ::std::uint_least32_t g, ::std::uint_least32_t b) noexcept
        {
            // Multiple copies of one char will not exceed the boundary, and the efficiency will become higher.
            ::std::memcpy(iter, u8"\033[38;2;\0", 8uz);
            // memory copy
            char_type* curr_pos{iter + 7uz};  // 7uz: Replace the written '\0'
            curr_pos = ::fast_io::pr_rsv_to_iterator_unchecked(curr_pos, r);
            *curr_pos = static_cast<char_type>(u8';');
            curr_pos = ::fast_io::pr_rsv_to_iterator_unchecked(++curr_pos, g);
            *curr_pos = static_cast<char_type>(u8';');
            curr_pos = ::fast_io::pr_rsv_to_iterator_unchecked(++curr_pos, b);
            *curr_pos = static_cast<char_type>(u8'm');
            return ++curr_pos;
        }
    }  // namespace details

    template <::std::integral char_type>
        requires (sizeof(char_type) == sizeof(char8_t))
    inline constexpr char_type* print_reserve_define(::fast_io::io_reserve_type_t<char_type, rgb>, char_type * iter, rgb color) noexcept
    { return details::rgb_print_reserve_impl(iter, color.r, color.g, color.b); }
}
