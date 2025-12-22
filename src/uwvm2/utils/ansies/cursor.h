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
    struct crs
    {
        ::std::uint_least32_t feat{};
        enum class cursor_variables : char8_t
        {
            up = u8'A',
            down = u8'B',
            right = u8'C',
            left = u8'D',
            csc = u8'J',
            cln = u8'K',
        } cursor_var{};
    };

    template <::std::integral char_type>
        requires (sizeof(char_type) == sizeof(char8_t))
    inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, crs>) noexcept
    {
        constexpr ::std::size_t ul32_real_size{::fast_io::pr_rsv_size<char_type, ::std::uint_least32_t>};
        constexpr ::std::size_t max_size{3uz + ul32_real_size};
        return max_size;
    }

    namespace details
    {
        template <::std::integral char_type>
            requires (sizeof(char_type) == sizeof(char8_t))
        inline constexpr char_type* crs_print_reserve_impl(char_type* iter, crs::cursor_variables cv, ::std::uint_least32_t f) noexcept
        {
            ::std::memcpy(iter, u8"\033[", 2uz);
            // memory copy
            char_type* curr_pos{iter + 2uz};
            curr_pos = ::fast_io::pr_rsv_to_iterator_unchecked(curr_pos, f);
            *curr_pos = static_cast<char_type>(cv);
            return ++curr_pos;
        }
    }  // namespace details

    template <::std::integral char_type>
        requires (sizeof(char_type) == sizeof(char8_t))
    inline constexpr char_type* print_reserve_define(::fast_io::io_reserve_type_t<char_type, crs>, char_type * iter, crs c) noexcept
    { return details::crs_print_reserve_impl(iter, c.cursor_var, c.feat); }
}
