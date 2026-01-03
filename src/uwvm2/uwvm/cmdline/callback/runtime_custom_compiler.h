/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-01-03
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
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        ::uwvm2::utils::cmdline::parameter_return_type runtime_custom_compiler_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results *
                                                                                            para_begin,
                                                                                        ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
                                                                                        ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        // [... curr] ...
        // [  safe  ] unsafe (could be the module_end)
        //      ^^ para_curr

        if(
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            ::uwvm2::uwvm::runtime::runtime_mode::is_runtime_mode_code_int_existed ||
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
            ::uwvm2::uwvm::runtime::runtime_mode::is_runtime_mode_code_jit_existed ||
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            ::uwvm2::uwvm::runtime::runtime_mode::is_runtime_mode_code_tiered_existed ||
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
            ::uwvm2::uwvm::runtime::runtime_mode::is_runtime_mode_code_aot_existed ||
#endif
            false) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Conflicting runtime parameters: \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"--runtime-custom-compiler",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" conflicts with shortcut runtime mode parameters "
                                u8"("
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                                u8"--runtime-int"
# if defined(UWVM_RUNTIME_LLVM_JIT)
                                ,
                                u8"|"
# endif
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
                                u8"--runtime-jit"
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                                ,
                                u8"|",
                                u8"--runtime-tiered"
# endif
                                ,
                                u8"|",
                                u8"--runtime-aot"
#endif
                                u8").\n" u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Use \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"--help runtime",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" for details.\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        auto currp1{para_curr + 1u};

        // [... curr] ...
        // [  safe  ] unsafe (could be the module_end)
        //            ^^ currp1

        // Check for out-of-bounds and not-argument
        if(currp1 == para_end || currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::runtime_custom_compiler),
                                // print_usage comes with UWVM_COLOR_U8_RST_ALL
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        // [... curr arg] ...
        // [     safe   ] unsafe (could be the module_end)
        //           ^^ currp1

        // Setting the argument is already taken
        currp1->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;

        auto const currp1_str{currp1->str};

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        if(currp1_str == u8"int")
        {
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only;
        }
        else
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            if(currp1_str == u8"tiered")
        {
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler =
                ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered;
        }
        else
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
            if(currp1_str == u8"jit")
        {
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only;
        }
        else
#endif
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid runtime compiler \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                currp1_str,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::runtime_custom_compiler),
                                // print_usage comes with UWVM_COLOR_U8_RST_ALL
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }
}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
