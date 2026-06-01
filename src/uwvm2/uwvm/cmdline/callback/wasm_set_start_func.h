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
# include <cstdint>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
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
    [[nodiscard]] inline constexpr bool wasm_set_start_func_parse_u32(::uwvm2::utils::container::u8string_view str, ::std::uint32_t& value) noexcept
    {
        auto const first{str.cbegin()};
        auto const last{str.cend()};
        if(first == last) { return false; }
        auto const [next, err]{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::dec_get<true, false>(value))};
        return err == ::fast_io::parse_code::ok && next == last;
    }

    [[nodiscard]] inline constexpr bool wasm_set_start_func_is_parameter_token(::uwvm2::utils::container::u8string_view str) noexcept
    {
        auto curr{str.cbegin()};
        auto const last{str.cend()};
        if(curr == last || *curr != u8'-') { return false; }
        ++curr;
        return curr == last || !::fast_io::char_category::is_c_digit(*curr);
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
            if(wasm_set_start_func_is_parameter_token(arg_str)) { break; }
            pr.emplace_back_unchecked(arg_str, nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg);
        }

        argv_curr = curr;
    }

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        ::uwvm2::utils::cmdline::parameter_return_type wasm_set_start_func_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results *
                                                                                        para_begin,
                                                                                    ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
                                                                                    ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto print_usage_error{[]() constexpr noexcept
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
