/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-17
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
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) && defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
# if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
# else
    UWVM_GNU_COLD inline constexpr
# endif
        ::uwvm2::utils::cmdline::parameter_return_type runtime_uwvm_int_set_opcode_conbination_level_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto const print_usage_error{
            []() constexpr noexcept
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Usage: ",
                                    ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::runtime_uwvm_int_set_opcode_conbination_level),
                                    u8"\n\n");
            }};

        auto const print_invalid_level_error{
            [](auto const currp1_str) constexpr noexcept
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Invalid uwvm-int opcode conbination level: \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    currp1_str,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\". Usage: ",
                                    ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::runtime_uwvm_int_set_opcode_conbination_level),
                                    u8"\n\n");
            }};

        auto currp1{para_curr + 1u};
        if(currp1 == para_end || currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            print_usage_error();
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        currp1->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        auto const currp1_str{currp1->str};

        using level_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_uwvm_int_opcode_conbination_level_t;
        if(currp1_str == u8"disable")
        {
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_uwvm_int_opcode_conbination_level = level_t::disable;
            return ::uwvm2::utils::cmdline::parameter_return_type::def;
        }

        if(currp1_str == u8"soft")
        {
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_uwvm_int_opcode_conbination_level = level_t::soft;
            return ::uwvm2::utils::cmdline::parameter_return_type::def;
        }

# if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if(currp1_str == u8"heavy")
        {
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_uwvm_int_opcode_conbination_level = level_t::heavy;
            return ::uwvm2::utils::cmdline::parameter_return_type::def;
        }
# endif

# if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        if(currp1_str == u8"extra")
        {
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_uwvm_int_opcode_conbination_level = level_t::extra;
            return ::uwvm2::utils::cmdline::parameter_return_type::def;
        }
# endif

        print_invalid_level_error(currp1_str);
        return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
    }
#endif
}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
