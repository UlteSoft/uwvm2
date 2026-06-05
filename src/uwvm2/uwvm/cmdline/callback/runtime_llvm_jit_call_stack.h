/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-03
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

#pragma push_macro("UWVM2_UWVM_CMDLINE_RUNTIME_LLVM_JIT_CALL_STACK_HAS_UNWIND")
#undef UWVM2_UWVM_CMDLINE_RUNTIME_LLVM_JIT_CALL_STACK_HAS_UNWIND
#if (defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)) &&                                                              \
    ((!defined(_WIN32) && (__has_include(<libunwind.h>) || __has_include(<unwind.h>))) ||                                                                  \
     (defined(_WIN64) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&              \
      !defined(__CYGWIN__) && __has_include(<windows.h>)))
# define UWVM2_UWVM_CMDLINE_RUNTIME_LLVM_JIT_CALL_STACK_HAS_UNWIND
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
#if defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
# if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
# else
    UWVM_GNU_COLD inline constexpr
# endif
        ::uwvm2::utils::cmdline::parameter_return_type runtime_llvm_jit_call_stack_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        constexpr auto print_usage_error{[]() constexpr noexcept
                               {
                                   ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                       ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                       u8"uwvm: ",
                                                       ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                                       u8"[error] ",
                                                       ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                       u8"Usage: ",
                                                       ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::runtime_llvm_jit_call_stack),
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

        using runtime_llvm_jit_call_stack_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_call_stack_t;
        if(currp1_str == u8"auto") { ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_llvm_jit_call_stack = runtime_llvm_jit_call_stack_t::auto_policy; }
        else if(currp1_str == u8"instruction")
        {
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_llvm_jit_call_stack = runtime_llvm_jit_call_stack_t::instruction;
        }
        else if(currp1_str == u8"none") { ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_llvm_jit_call_stack = runtime_llvm_jit_call_stack_t::none; }
# ifdef UWVM2_UWVM_CMDLINE_RUNTIME_LLVM_JIT_CALL_STACK_HAS_UNWIND
        else if(currp1_str == u8"unwind") { ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_llvm_jit_call_stack = runtime_llvm_jit_call_stack_t::unwind; }
        else if(currp1_str == u8"unwind-uncheck" || currp1_str == u8"unwind-unchecked")
        {
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_llvm_jit_call_stack = runtime_llvm_jit_call_stack_t::unwind_uncheck;
        }
# endif
        else [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid runtime LLVM JIT call-stack mode: \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                currp1_str,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". Expected ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"auto",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"instruction",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"none",
# ifdef UWVM2_UWVM_CMDLINE_RUNTIME_LLVM_JIT_CALL_STACK_HAS_UNWIND
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", or ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"unwind",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", or ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"unwind-uncheck",
# endif
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8". Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::runtime_llvm_jit_call_stack),
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }
#endif
}  // namespace uwvm2::uwvm::cmdline::params::details

#pragma pop_macro("UWVM2_UWVM_CMDLINE_RUNTIME_LLVM_JIT_CALL_STACK_HAS_UNWIND")

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
