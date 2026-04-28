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
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
#if defined(UWVM_RUNTIME_HAS_BACKEND) || defined(UWVM_RUNTIME_HAS_DEBUGGER_BACKEND)
#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        ::uwvm2::utils::cmdline::parameter_return_type runtime_compiler_log_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results *
                                                                                         para_begin,
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
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::runtime_compiler_log),
                                // print_usage comes with UWVM_COLOR_U8_RST_ALL
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        // [... curr arg1] ...
        // [     safe    ] unsafe (could be the module_end)
        //           ^^ currp1

        // Setting the argument is already taken
        currp1->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        auto const log_output_arg{currp1->str};

#if !defined(__AVR__) && !((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) && !(defined(__MSDOS__) || defined(__DJGPP__)) &&               \
    !(defined(__NEWLIB__) && !defined(__CYGWIN__)) && !defined(_PICOLIBC__) && !defined(__wasm__)
        auto log_output_path{log_output_arg};

        if(log_output_arg == u8"out")
        {
            ::uwvm2::uwvm::io::u8runtime_log_output.reopen(::fast_io::io_dup, ::fast_io::u8out());
            ::uwvm2::uwvm::io::runtime_log_output_target = ::uwvm2::uwvm::io::runtime_log_output_target_t::out;
            return ::uwvm2::utils::cmdline::parameter_return_type::def;
        }
        if(log_output_arg == u8"err")
        {
            ::uwvm2::uwvm::io::u8runtime_log_output.reopen(::fast_io::io_dup, ::fast_io::u8err());
            ::uwvm2::uwvm::io::runtime_log_output_target = ::uwvm2::uwvm::io::runtime_log_output_target_t::err;
            return ::uwvm2::utils::cmdline::parameter_return_type::def;
        }
        if(log_output_arg != u8"file")
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::runtime_compiler_log),
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }
        {
            auto currp2{para_curr + 2u};
            if(currp2 == para_end || currp2->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Usage: ",
                                    ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::runtime_compiler_log),
                                    u8"\n\n");
                return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
            }

            currp2->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
            log_output_path = currp2->str;
        }

#if defined(_WIN32) && !defined(_WIN32_WINDOWS)
        if(log_output_path.starts_with(u8"::NT::"))
        {
            // nt path

            ::fast_io::u8cstring_view const log_output_path_nt_subview{::fast_io::containers::null_terminated, log_output_path.subview(6uz)};

            if(::uwvm2::uwvm::io::show_nt_path_warning)
            {
                // Output the main information and memory indication
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    // 1
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    u8"[warn]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Resolve to NT path: \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    log_output_path_nt_subview,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\".",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8" (nt-path)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                if(::uwvm2::uwvm::io::nt_path_warning_fatal) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Convert warnings to fatal errors. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(nt-path)\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }

# if defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
            try
# endif
            {
                // allow symlink
                ::uwvm2::uwvm::io::u8runtime_log_output.reopen(::fast_io::io_kernel, log_output_path_nt_subview, ::fast_io::open_mode::out | ::fast_io::open_mode::follow);
            }
# if defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
            catch(::fast_io::error e)
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Unable to open log output file \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    log_output_path_nt_subview,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\": ",
                                    e,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                    u8"\n");
                return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
            }
# endif
        }
        else
        {
            // win32 path

# if defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
            try
# endif
            {
                // allow symlink
                ::uwvm2::uwvm::io::u8runtime_log_output.reopen(log_output_path, ::fast_io::open_mode::out | ::fast_io::open_mode::follow);
            }
# if defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
            catch(::fast_io::error e)
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Unable to open log output file \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    log_output_path,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\": ",
                                    e,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                    u8"\n");
                return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
            }
# endif
        }
#else
        // posix

# if defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
            try
# endif
            {
                // allow symlink
                ::uwvm2::uwvm::io::u8runtime_log_output.reopen(log_output_path, ::fast_io::open_mode::out | ::fast_io::open_mode::follow);
            }
# if defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
        catch(::fast_io::error e)
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Unable to open log output file \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                log_output_path,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\": ",
                                e,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                u8"\n"
#  ifndef _WIN32
                                u8"\n"
#  endif
            );
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }
# endif
# endif

        ::uwvm2::uwvm::io::runtime_log_output_target = ::uwvm2::uwvm::io::runtime_log_output_target_t::file;
        return ::uwvm2::utils::cmdline::parameter_return_type::def;
#else
        // on AVR only support cstdout and cstderr
        if(log_output_arg == u8"out")
        {
# if defined(__AVR__)
            ::uwvm2::uwvm::io::u8runtime_log_output.reopen(::fast_io::u8c_stdout());
# else
            ::uwvm2::uwvm::io::u8runtime_log_output.reopen(::fast_io::u8out());
# endif
            ::uwvm2::uwvm::io::runtime_log_output_target = ::uwvm2::uwvm::io::runtime_log_output_target_t::out;
            return ::uwvm2::utils::cmdline::parameter_return_type::def;
        }
        if(log_output_arg == u8"err")
        {
# if defined(__AVR__)
            ::uwvm2::uwvm::io::u8runtime_log_output.reopen(::fast_io::u8c_stderr());
# else
            ::uwvm2::uwvm::io::u8runtime_log_output.reopen(::fast_io::u8err());
# endif
            ::uwvm2::uwvm::io::runtime_log_output_target = ::uwvm2::uwvm::io::runtime_log_output_target_t::err;
            return ::uwvm2::utils::cmdline::parameter_return_type::def;
        }

        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                            u8"[error] ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"Usage: ",
                            ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::runtime_compiler_log),
                            u8"\n\n");
        return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
#endif
    }
#endif
}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
