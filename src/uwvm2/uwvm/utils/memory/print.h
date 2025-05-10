/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-29
 * @copyright   APL-2 License
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

#ifdef UWVM_MODULE
import fast_io;
import uwvm2.uwvm.io;
#else
// std
# include <cstddef>
# include <cstdint>
# include <concepts>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <fast_io_device.h>
# include <uwvm2/uwvm/io/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::utils::memory
{
    struct print_memory
    {
        ::std::byte const* err_begin{};
        ::std::byte const* err_curr{};
        ::std::byte const* err_end{};
        ::std::size_t front{4uz};
        ::std::size_t back{12uz};
    };

    template <::std::integral char_type, typename Stm>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, print_memory>, Stm && stream, print_memory const& mem) noexcept
    {
        auto err_begin{mem.err_begin};
        auto err_curr{mem.err_curr};
        auto err_end{mem.err_end};

        bool print_front_ext{};
        bool print_end_ext{};

        if(static_cast<::std::size_t>(err_curr - err_begin) > mem.front)
        {
            err_begin = err_curr - mem.front;
            print_front_ext = true;
        }

        if(static_cast<::std::size_t>(err_end - err_curr) > mem.back)
        {
            err_end = err_curr + mem.back;
            print_end_ext = true;
        }

        if(err_begin == err_end) [[unlikely]]
        {
            if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "(null) "); }
            else if constexpr(::std::same_as<char_type, wchar_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"(null) "); }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"(null) ");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"(null) ");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"(null) ");
            }

            return;
        }

        if(print_front_ext)
        {
            if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "... "); }
            else if constexpr(::std::same_as<char_type, wchar_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"... "); }
            else if constexpr(::std::same_as<char_type, char8_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"... "); }
            else if constexpr(::std::same_as<char_type, char16_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"... "); }
            else if constexpr(::std::same_as<char_type, char32_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"... "); }
        }

        for(auto acc_curr{err_begin}; acc_curr != err_end; ++acc_curr)
        {
            if constexpr(::std::same_as<char_type, char>)
            {
                if(acc_curr != err_curr) [[likely]]
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     ::fast_io::mnp::hex<false, true>(static_cast<::std::uint_least8_t>(*acc_curr)),
                                                                     " ");
                }
                else
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     "[",
                                                                     ::fast_io::mnp::hex<false, true>(static_cast<::std::uint_least8_t>(*acc_curr)),
                                                                     "] ");
                }
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                if(acc_curr != err_curr) [[likely]]
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     ::fast_io::mnp::hex<false, true>(static_cast<::std::uint_least8_t>(*acc_curr)),
                                                                     L" ");
                }
                else
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     L"[",
                                                                     ::fast_io::mnp::hex<false, true>(static_cast<::std::uint_least8_t>(*acc_curr)),
                                                                     L"] ");
                }
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                if(acc_curr != err_curr) [[likely]]
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     ::fast_io::mnp::hex<false, true>(static_cast<::std::uint_least8_t>(*acc_curr)),
                                                                     u8" ");
                }
                else
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u8"[",
                                                                     ::fast_io::mnp::hex<false, true>(static_cast<::std::uint_least8_t>(*acc_curr)),
                                                                     u8"] ");
                }
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                if(acc_curr != err_curr) [[likely]]
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     ::fast_io::mnp::hex<false, true>(static_cast<::std::uint_least8_t>(*acc_curr)),
                                                                     u" ");
                }
                else
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u"[",
                                                                     ::fast_io::mnp::hex<false, true>(static_cast<::std::uint_least8_t>(*acc_curr)),
                                                                     u"] ");
                }
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                if(acc_curr != err_curr) [[likely]]
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     ::fast_io::mnp::hex<false, true>(static_cast<::std::uint_least8_t>(*acc_curr)),
                                                                     U" ");
                }
                else
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     U"[",
                                                                     ::fast_io::mnp::hex<false, true>(static_cast<::std::uint_least8_t>(*acc_curr)),
                                                                     U"] ");
                }
            }
        }

        if(print_end_ext)
        {
            if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "... "); }
            else if constexpr(::std::same_as<char_type, wchar_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"... "); }
            else if constexpr(::std::same_as<char_type, char8_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"... "); }
            else if constexpr(::std::same_as<char_type, char16_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"... "); }
            else if constexpr(::std::same_as<char_type, char32_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"... "); }
        }
    }

}  // namespace uwvm2::utils::install_path

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
