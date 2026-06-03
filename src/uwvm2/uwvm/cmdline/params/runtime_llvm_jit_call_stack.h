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
# include <uwvm2/utils/cmdline/impl.h>
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

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params
{
#if defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
    namespace details
    {
        inline constexpr ::uwvm2::utils::container::u8string_view runtime_llvm_jit_call_stack_alias{u8"-Rllvm-call-stack"};
# if defined(UWVM_MODULE)
        extern "C++"
# else
        inline constexpr
# endif
            ::uwvm2::utils::cmdline::parameter_return_type runtime_llvm_jit_call_stack_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                                ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                                ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
    }  // namespace details

# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wbraced-scalar-init"
# endif
    inline constexpr ::uwvm2::utils::cmdline::parameter runtime_llvm_jit_call_stack{
        .name{u8"--runtime-llvm-jit-call-stack"},
        .describe{u8"Select the runtime LLVM JIT call-stack tracking mode."},
# ifdef UWVM2_UWVM_CMDLINE_RUNTIME_LLVM_JIT_CALL_STACK_HAS_UNWIND
        .usage{u8"[auto|instruction|none|unwind|unwind-uncheck]"},
# else
        .usage{u8"[auto|instruction|none]"},
# endif
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::runtime_llvm_jit_call_stack_alias), 1uz}},
        .handle{::std::addressof(details::runtime_llvm_jit_call_stack_callback)},
        .is_exist{::std::addressof(::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_call_stack_existed)},
        .cate{::uwvm2::utils::cmdline::categorization::runtime}};
# if defined(__clang__)
#  pragma clang diagnostic pop
# endif
#endif
}  // namespace uwvm2::uwvm::cmdline::params

#pragma pop_macro("UWVM2_UWVM_CMDLINE_RUNTIME_LLVM_JIT_CALL_STACK_HAS_UNWIND")

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
