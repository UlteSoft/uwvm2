/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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
# include <cstddef>
# include <memory>
# include <utility>
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
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
    extern "C++" UWVM_GNU_COLD  ::uwvm2::utils::cmdline::parameter_return_type log_convert_warn_to_fatal_callback(
        [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
        ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
        ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        // [... curr] ...
        // [  safe  ] unsafe (could be the module_end)
        //      ^^ para_curr

        auto currp1{para_curr + 1u};

        // [... curr] ...
        // [  safe  ] unsafe (could be the module_end)
        //            ^^ currp1

        // Check for out-of-bounds and not-argument
        if(currp1 == para_end || currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            // (currp1 == para_end):
            // [... curr] (end) ...
            // [  safe  ] unsafe (could be the module_end)
            //            ^^ currp1

            // (currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg):
            // [... curr para] ...
            // [     safe    ] unsafe (could be the module_end)
            //           ^^ currp1

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::log_convert_warn_to_fatal),
                                // print_usage comes with UWVM_COLOR_U8_RST_ALL
                                u8"\n\n");

            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        // [... curr arg1] ...
        // [     safe    ] unsafe (could be the module_end)
        //           ^^ currp1

        // Setting the argument is already taken
        currp1->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;

        if(auto currp1_str{currp1->str}; currp1_str == u8"all")
        {
            ::uwvm2::uwvm::io::vm_warning_fatal = true;
            ::uwvm2::uwvm::io::parser_warning_fatal = true;
            ::uwvm2::uwvm::io::untrusted_dl_warning_fatal = true;
#ifdef UWVM_SUPPORT_PRELOAD_DL
            ::uwvm2::uwvm::io::dl_warning_fatal = true;
#endif
#ifdef UWVM_SUPPORT_WEAK_SYMBOL
            ::uwvm2::uwvm::io::weak_symbol_warning_fatal = true;
#endif
            ::uwvm2::uwvm::io::depend_warning_fatal = true;
#if defined(_WIN32) && !defined(_WIN32_WINDOWS)
            ::uwvm2::uwvm::io::nt_path_warning_fatal = true;
#endif
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
            ::uwvm2::uwvm::io::toctou_warning_fatal = true;
#endif
        }
        else if(currp1_str == u8"vm") { ::uwvm2::uwvm::io::vm_warning_fatal = true; }
        else if(currp1_str == u8"parser") { ::uwvm2::uwvm::io::parser_warning_fatal = true; }
        else if(currp1_str == u8"untrusted-dl") { ::uwvm2::uwvm::io::untrusted_dl_warning_fatal = true; }
#ifdef UWVM_SUPPORT_PRELOAD_DL
        else if(currp1_str == u8"dl") { ::uwvm2::uwvm::io::dl_warning_fatal = true; }
#endif
#ifdef UWVM_SUPPORT_WEAK_SYMBOL
        else if(currp1_str == u8"weak-symbol") { ::uwvm2::uwvm::io::weak_symbol_warning_fatal = true; }
#endif
        else if(currp1_str == u8"depend") { ::uwvm2::uwvm::io::depend_warning_fatal = true; }
#if defined(_WIN32) && !defined(_WIN32_WINDOWS)
        else if(currp1_str == u8"nt-path") { ::uwvm2::uwvm::io::nt_path_warning_fatal = true; }
#endif
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
        else if(currp1_str == u8"toctou") { ::uwvm2::uwvm::io::toctou_warning_fatal = true; }
#endif
        else
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::log_convert_warn_to_fatal),
                                // print_usage comes with UWVM_COLOR_U8_RST_ALL
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }

}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

