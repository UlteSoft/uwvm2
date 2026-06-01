/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
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
# include <bit>
# include <charconv>
# include <cstddef>
# include <cstdint>
# include <cmath>
# include <limits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
    template <typename ScanManipulator>
    [[nodiscard]] inline constexpr bool wasm_set_start_func_scan_exact(char8_t const* first,
                                                                       char8_t const* last,
                                                                       ScanManipulator scan_manipulator) noexcept
    {
        auto const [next, err]{::fast_io::parse_by_scan(first, last, scan_manipulator)};
        return err == ::fast_io::parse_code::ok && next == last;
    }

    template <bool allow_signed_decimal, typename Out, typename Unsigned>
    [[nodiscard]] inline constexpr bool wasm_set_start_func_parse_wasm_integer(::uwvm2::utils::container::u8string_view str,
                                                                               Out& value) noexcept
    {
        static_assert(sizeof(Out) == sizeof(Unsigned));
        auto const first{str.cbegin()};
        auto const last{str.cend()};
        if(first == last) { return false; }

        if constexpr(allow_signed_decimal)
        {
            Out signed_value; // no init necessary
            if(wasm_set_start_func_scan_exact(first, last, ::fast_io::mnp::dec_get<true, false>(signed_value)))
            {
                value = signed_value;
                return true;
            }
        }

        Unsigned unsigned_value; // no init necessary
        if(wasm_set_start_func_scan_exact(first, last, ::fast_io::mnp::dec_get<true, false>(unsigned_value)) ||
           wasm_set_start_func_scan_exact(first, last, ::fast_io::mnp::hex_get<true, false, true>(unsigned_value)) ||
           wasm_set_start_func_scan_exact(first, last, ::fast_io::mnp::bin_get<true, false, true>(unsigned_value)) ||
           wasm_set_start_func_scan_exact(first, last, ::fast_io::mnp::oct_get<true, false, true, true>(unsigned_value)))
        {
            value = ::std::bit_cast<Out>(unsigned_value);
            return true;
        }

        return false;
    }

    [[nodiscard]] inline constexpr bool wasm_set_start_func_parse_u32(::uwvm2::utils::container::u8string_view str,
                                                                      ::std::uint32_t& value) noexcept
    {
        return wasm_set_start_func_parse_wasm_integer<false, ::std::uint32_t, ::std::uint32_t>(str, value);
    }

    [[nodiscard]] inline constexpr bool wasm_set_start_func_parse_i64(::uwvm2::utils::container::u8string_view str) noexcept
    {
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_u64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;
        static_assert(sizeof(wasm_i64) == 8uz && sizeof(wasm_u64) == 8uz);
        wasm_i64 value; // no init necessary
        return wasm_set_start_func_parse_wasm_integer<true, wasm_i64, wasm_u64>(str, value);
    }

    [[nodiscard]] inline constexpr bool wasm_set_start_func_hex_digit(char8_t ch, ::std::uint_least8_t& value) noexcept
    {
        ::std::uint_least8_t parsed{};
        auto const first{::std::addressof(ch)};
        auto const [next, err]{::fast_io::parse_by_scan(first, first + 1u, ::fast_io::mnp::hex_get<true, false, false>(parsed))};
        if(err != ::fast_io::parse_code::ok || next != first + 1u) { return false; }
        value = parsed;
        return true;
    }

    [[nodiscard]] inline constexpr bool wasm_set_start_func_binary_digit(char8_t ch, ::std::uint_least8_t& value) noexcept
    {
        ::std::uint_least8_t parsed{};
        auto const first{::std::addressof(ch)};
        auto const [next, err]{::fast_io::parse_by_scan(first, first + 1u, ::fast_io::mnp::bin_get<true, false, false>(parsed))};
        if(err != ::fast_io::parse_code::ok || next != first + 1u) { return false; }
        value = parsed;
        return true;
    }

    template <typename T, ::std::uint_least8_t Base, ::std::int_least64_t BitsPerDigit>
    [[nodiscard]] inline bool wasm_set_start_func_parse_prefixed_binary_float_range(char8_t const* first,
                                                                                    char8_t const* last,
                                                                                    T& value) noexcept
    {
        if(first == last) { return false; }
        if(*first == u8'-' || *first == u8'+') { return false; }

        if constexpr(Base == 16u)
        {
            if(last - first < 2 || first[0] != u8'0' || (first[1] != u8'x' && first[1] != u8'X')) { return false; }
        }
        else
        {
            static_assert(Base == 2u);
            if(last - first < 2 || first[0] != u8'0' || (first[1] != u8'b' && first[1] != u8'B')) { return false; }
        }
        first += 2;

        long double significand{};
        bool has_digit{};
        ::std::int_least64_t fractional_digits{};

        auto parse_digit{[](char8_t ch, ::std::uint_least8_t& digit) constexpr noexcept
                         {
                             if constexpr(Base == 16u) { return wasm_set_start_func_hex_digit(ch, digit); }
                             else { return wasm_set_start_func_binary_digit(ch, digit); }
                         }};

        while(first != last)
        {
            ::std::uint_least8_t digit{};
            if(!parse_digit(*first, digit)) { break; }
            significand = significand * static_cast<long double>(Base) + static_cast<long double>(digit);
            has_digit = true;
            ++first;
        }

        if(first != last && *first == u8'.')
        {
            ++first;
            while(first != last)
            {
                ::std::uint_least8_t digit{};
                if(!parse_digit(*first, digit)) { break; }
                significand = significand * static_cast<long double>(Base) + static_cast<long double>(digit);
                has_digit = true;
                ++fractional_digits;
                ++first;
            }
        }

        ::std::int_least64_t exponent{};
        if(!has_digit || first == last || (*first != u8'p' && *first != u8'P')) { return false; }
        ++first;
        if(first == last) { return false; }
        if(*first == u8'+')
        {
            ++first;
            if(first == last) { return false; }
        }

        auto const [next, err]{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::dec_get<true, false>(exponent))};
        if(err != ::fast_io::parse_code::ok || next != last) { return false; }

        auto const adjusted_exponent{exponent - fractional_digits * BitsPerDigit};
        if(adjusted_exponent > static_cast<::std::int_least64_t>((::std::numeric_limits<int>::max)()))
        {
            value = (::std::numeric_limits<T>::infinity)();
            return true;
        }
        if(adjusted_exponent < static_cast<::std::int_least64_t>((::std::numeric_limits<int>::min)()))
        {
            value = static_cast<T>(0.0);
            return true;
        }

        auto const scaled{::std::ldexp(significand, static_cast<int>(adjusted_exponent))};
        value = static_cast<T>(scaled);
        return true;
    }

    template <typename T>
    [[nodiscard]] inline bool wasm_set_start_func_parse_prefixed_binary_float_range(char8_t const* first, char8_t const* last, T& value) noexcept
    {
        return wasm_set_start_func_parse_prefixed_binary_float_range<T, 16u, 4>(first, last, value) ||
               wasm_set_start_func_parse_prefixed_binary_float_range<T, 2u, 1>(first, last, value);
    }

    template <typename T>
    [[nodiscard]] inline bool wasm_set_start_func_parse_float_range(char8_t const* first, char8_t const* last, T& value) noexcept
    {
        if(first == last) { return false; }
#if defined(FAST_IO_NOT_USE_FAST_FLOAT)
        auto const char_first{reinterpret_cast<char const*>(first)};
        auto const char_last{reinterpret_cast<char const*>(last)};
        auto const [next, err]{::std::from_chars(char_first, char_last, value, ::std::chars_format::general)};
        if(err == ::std::errc{} && next == char_last) { return true; }
#else
        auto const [next, err]{::fast_io::parse_by_scan(first, last, value)};
        if(err == ::fast_io::parse_code::ok && next == last) { return true; }
#endif
        return wasm_set_start_func_parse_prefixed_binary_float_range(first, last, value);
    }

    template <typename T>
    [[nodiscard]] inline bool wasm_set_start_func_parse_float_full(::uwvm2::utils::container::u8string_view str, T& value) noexcept
    {
        auto first{str.cbegin()};
        auto last{str.cend()};
        if(wasm_set_start_func_parse_float_range(first, last, value)) { return true; }
        if(first != last && (last[-1] == u8'f' || last[-1] == u8'F'))
        {
            --last;
            if(wasm_set_start_func_parse_float_range(first, last, value)) { return true; }
        }
        return false;
    }

    [[nodiscard]] inline bool wasm_set_start_func_parse_f64(::uwvm2::utils::container::u8string_view str) noexcept
    {
        double value{};
        return wasm_set_start_func_parse_float_full(str, value);
    }

    [[nodiscard]] inline bool wasm_set_start_func_is_numeric_token(::uwvm2::utils::container::u8string_view str) noexcept
    {
        if(str.empty()) [[unlikely]] { return false; }
        if(wasm_set_start_func_parse_i64(str)) { return true; }
        return wasm_set_start_func_parse_f64(str);
    }

#if defined(UWVM_MODULE)
    extern "C++"
#else
    inline
#endif
        void wasm_set_start_func_pretreatment(char8_t const* const*& argv_curr,
                                              char8_t const* const* argv_end,
                                              ::uwvm2::utils::container::vector<::uwvm2::utils::cmdline::parameter_parsing_results>& pr) noexcept
    {
        auto curr{argv_curr + 1u};
        if(curr == argv_end || *curr == nullptr) [[unlikely]]
        {
            argv_curr = curr;
            return;
        }

        auto const local_func_str{::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(*curr)}};
        ::std::uint32_t local_func_index{};
        if(!wasm_set_start_func_parse_u32(local_func_str, local_func_index))
        {
            argv_curr = curr;
            return;
        }

        // The outer command-line parser reserves `argc` result slots before calling any pretreatment. This
        // pretreatment consumes at most one argv token per appended result, so total appends can never exceed that
        // parser-level reservation.
        pr.emplace_back_unchecked(local_func_str, nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg);
        for(++curr; curr != argv_end; ++curr)
        {
            if(*curr == nullptr) [[unlikely]] { break; }
            auto const arg_str{::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(*curr)}};
            if(!wasm_set_start_func_is_numeric_token(arg_str)) { break; }
            pr.emplace_back_unchecked(arg_str, nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg);
        }

        argv_curr = curr;
    }

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        ::uwvm2::utils::cmdline::parameter_return_type wasm_set_start_func_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto print_usage_error{
            []() constexpr noexcept
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Usage: ",
                                    ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_start_func),
                                    u8"\n\n");
            }};

        auto currp1{para_curr + 1u};
        if(currp1 == para_end || (currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg &&
                                  currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg)) [[unlikely]]
        {
            print_usage_error();
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        ::std::uint32_t local_func_index{};
        auto const currp1_str{currp1->str};
        if(!wasm_set_start_func_parse_u32(currp1_str, local_func_index)) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid local function index for ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"--wasm-set-start-func",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8": \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                currp1_str,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". Expected u32. Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_start_func),
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        currp1->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;

        auto& cfg{::uwvm2::uwvm::wasm::storage::start_func_call};
        cfg.enabled = true;
        cfg.local_function_index = local_func_index;
        cfg.argument_tokens.clear();

        for(auto curr_arg{currp1 + 1u}; curr_arg != para_end; ++curr_arg)
        {
            if(curr_arg->para != nullptr || curr_arg->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg) { break; }
            cfg.argument_tokens.emplace_back(::uwvm2::utils::container::u8string_view{curr_arg->str});
        }

        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }
}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
