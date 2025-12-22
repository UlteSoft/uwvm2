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
# include <compare>
# include <concepts>
// import
# include <fast_io.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils
{
    // version does not add meaningless namespaces.
    struct version
    {
        ::std::uint_least32_t x{};
        ::std::uint_least32_t y{};
        ::std::uint_least32_t z{};
        ::std::uint_least32_t state{};
    };

    inline constexpr bool operator== (version v1, version v2) noexcept { return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z && v1.state == v2.state; }

    inline constexpr ::std::strong_ordering operator<=> (version v1, version v2) noexcept
    {
        auto const cx{v1.x <=> v2.x};
        if(cx == ::std::strong_ordering::equal)
        {
            auto const cy{v1.y <=> v2.y};
            if(cy == ::std::strong_ordering::equal)
            {
                auto const cz{v1.z <=> v2.z};
                if(cz == ::std::strong_ordering::equal) { return v1.state <=> v2.state; }
                return cz;
            }
            return cy;
        }
        return cx;
    }

    template <::std::integral char_type>
    inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, version>) noexcept
    {
        constexpr ::std::size_t ul32_real_size{::fast_io::pr_rsv_size<char_type, ::std::uint_least32_t>};
        constexpr ::std::size_t max_size{3uz + 4uz * ul32_real_size};
        return max_size;
    }

    namespace details
    {
        template <::std::integral char_type>
        inline constexpr char_type* version_print_reserve_impl(char_type* iter,
                                                               ::std::uint_least32_t x,
                                                               ::std::uint_least32_t y,
                                                               ::std::uint_least32_t z,
                                                               ::std::uint_least32_t state) noexcept
        {
            constexpr auto point{::fast_io::char_literal_v<u8'.', char_type>};
            // [x ... '.' y ... '.' z ... '.' state ...] (end)
            // [                 safe                  ] unsafe
            // ^^ curr_pos
            char_type* curr_pos{::fast_io::pr_rsv_to_iterator_unchecked(iter, x)};
            // [x ... '.' y ... '.' z ... '.' state ...] (end)
            // [                 safe                  ] unsafe
            //        ^^ curr_pos
            *curr_pos = point;
            // [x ... '.' y ... '.' z ... '.' state ...] (end)
            // [                 safe                  ] unsafe
            //        ^^ curr_pos
            curr_pos = ::fast_io::pr_rsv_to_iterator_unchecked(++curr_pos, y);
            // [x ... '.' y ... '.' z ... '.' state ...] (end)
            // [                 safe                  ] unsafe
            //            ^^ ++curr_pos
            //                  ^^ (ret) curr_pos
            *curr_pos = point;
            // [x ... '.' y ... '.' z ... '.' state ...] (end)
            // [                 safe                  ] unsafe
            //                  ^^ curr_pos
            curr_pos = ::fast_io::pr_rsv_to_iterator_unchecked(++curr_pos, z);
            // [x ... '.' y ... '.' z ... '.' state ...] (end)
            // [                 safe                  ] unsafe
            //                      ^^ ++curr_pos
            //                            ^^ (ret) curr_pos
            *curr_pos = point;
            curr_pos = ::fast_io::pr_rsv_to_iterator_unchecked(++curr_pos, state);
            // [x ... '.' y ... '.' z ... '.' state ...] (end)
            // [                 safe                  ] unsafe
            //                                ^^ ++curr_pos
            //                                           ^^ (ret) curr_pos
            return curr_pos;
        }
    }  // namespace details

    template <::std::integral char_type>
    inline constexpr char_type* print_reserve_define(::fast_io::io_reserve_type_t<char_type, version>, char_type * iter, version ver) noexcept
    { return details::version_print_reserve_impl(iter, ver.x, ver.y, ver.z, ver.state); }
}
