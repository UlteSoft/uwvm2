/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-03-25
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
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
#if defined(UWVM_MODULE)
    extern "C++"
#else
    inline constexpr
#endif
        void runtime_compile_threads_pretreatment(char8_t const* const*& argv_curr,
                                                  char8_t const* const* argv_end,
                                                  ::uwvm2::utils::container::vector<::uwvm2::utils::cmdline::parameter_parsing_results>& pr) noexcept
    {
        auto currp1{argv_curr + 1u};

        if(currp1 != argv_end) [[likely]]
        {
            if(auto const currp1_cstr{*currp1}; currp1_cstr != nullptr)
            {
                auto const currp1_str{::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(currp1_cstr)}};

                // Pre-mark a signed integer so "-1" is not mistaken for another parameter during preprocessing.
                if(currp1_str.size() > 1uz && currp1_str.front_unchecked() == u8'-')
                {
                    auto const second_char{currp1_str.index_unchecked(1uz)};
                    if(::fast_io::char_category::is_c_digit(second_char))
                    {
                        pr.emplace_back_unchecked(currp1_str, nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg);
                        argv_curr = currp1 + 1u;
                        return;
                    }
                }
            }
        }

        argv_curr = currp1;
    }

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        ::uwvm2::utils::cmdline::parameter_return_type runtime_compile_threads_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results *
                                                                                             para_begin,
                                                                                         ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
                                                                                         ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto currp1{para_curr + 1u};

        if(currp1 == para_end ||
           (currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg &&
            currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg)) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::runtime_compile_threads),
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        currp1->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;

        auto const currp1_str{currp1->str};

        using runtime_compile_threads_type = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_type;
        using runtime_compile_threads_policy_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_policy_t;

        runtime_compile_threads_type compile_threads;  // No initialization necessary
        auto const [next, err]{::fast_io::parse_by_scan(currp1_str.cbegin(), currp1_str.cend(), compile_threads)};

        if(err == ::fast_io::parse_code::ok && next == currp1_str.cend())
        {
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads = compile_threads;
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_policy = runtime_compile_threads_policy_t::numeric;
            return ::uwvm2::utils::cmdline::parameter_return_type::def;
        }

        if(currp1_str == u8"default")
        {
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads = 0;
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_policy = runtime_compile_threads_policy_t::default_policy;
            return ::uwvm2::utils::cmdline::parameter_return_type::def;
        }

        if(currp1_str == u8"aggressive")
        {
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads = 0;
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_policy = runtime_compile_threads_policy_t::aggressive;
            return ::uwvm2::utils::cmdline::parameter_return_type::def;
        }

        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                            u8"[error] ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"Invalid runtime compile thread setting: \"",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            currp1_str,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"\". Expected ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            u8"default",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8", ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            u8"aggressive",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8", or a ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            u8"ssize_t",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8". Usage: ",
                            ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::runtime_compile_threads),
                            u8"\n\n");
        return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
    }
}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
