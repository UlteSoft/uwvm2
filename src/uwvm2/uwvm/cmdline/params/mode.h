/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-31
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
# include <cstdint>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params
{
    namespace details
    {
        inline bool mode_is_exist{};  // [global]
        // An inherited, connected host socket is an explicit launch capability.
        // It is never a guest descriptor or a pathname to be opened later.
        inline int debug_jit_control_fd{-1};
        inline bool debug_jit_control_fd_is_exist{};
        // Windows launcher passes an inherited client HANDLE value. Zero means
        // no late-control grant; guest Wasm never supplies or sees this number.
        inline ::std::uintptr_t debug_jit_control_handle{};
        inline bool debug_jit_control_handle_is_exist{};
        inline constexpr ::uwvm2::utils::container::u8string_view mode_alias{u8"-m"};
        inline constexpr ::uwvm2::utils::container::u8string_view runtime_debug_jit_alias{u8"-Rdbg"};
#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type runtime_debug_jit_callback(
                ::uwvm2::utils::cmdline::parameter_parsing_results*,
                ::uwvm2::utils::cmdline::parameter_parsing_results*,
                ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type mode_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                         ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
#if defined(UWVM_MODULE)
        extern "C++"
#endif
        ::uwvm2::utils::cmdline::parameter_return_type debug_jit_control_fd_callback(
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
#if defined(UWVM_MODULE)
        extern "C++"
#endif
        ::uwvm2::utils::cmdline::parameter_return_type debug_jit_control_handle_callback(
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

    }  // namespace details

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wbraced-scalar-init"
#endif
    inline constexpr ::uwvm2::utils::cmdline::parameter mode{.name{u8"--mode"},
                                                             .describe{u8"Select the operation mode. Default: run."},
                                                             .usage{u8"[section-details|validation|run|debug-jit]"},
                                                             .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::mode_alias), 1uz}},
                                                             .handle{::std::addressof(details::mode_callback)},
                                                             .is_exist{::std::addressof(details::mode_is_exist)}};
    // Share --mode's duplicate-selection flag: argument order must not silently
    // turn an explicit debugger launch back into a normal run. Startup applies
    // the same LLVM/full defaults and incompatible-backend fatal as -m debug-jit.
    inline constexpr ::uwvm2::utils::cmdline::parameter runtime_debug_jit{
        .name{u8"--runtime-debug"},
        .describe{u8"Shortcut for -m debug-jit; enter the built-in LLVM JIT full debugger."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::runtime_debug_jit_alias), 1uz}},
        .handle{::std::addressof(details::runtime_debug_jit_callback)},
        .is_exist{::std::addressof(details::mode_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::runtime}};
    inline constexpr ::uwvm2::utils::cmdline::parameter debug_jit_control_fd{
        .name{u8"--debug-jit-control-fd"},
        .describe{u8"Opt in to late LLVM-full debugger commands from a trusted inherited Linux packet or macOS framed Unix endpoint."},
        .usage{u8"<connected-fd>"},
        .handle{::std::addressof(details::debug_jit_control_fd_callback)},
        .is_exist{::std::addressof(details::debug_jit_control_fd_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::runtime}};
    inline constexpr ::uwvm2::utils::cmdline::parameter debug_jit_control_handle{
        .name{u8"--debug-jit-control-handle"},
        .describe{u8"Windows only: opt in to LLVM-full late debugger commands from a direct-parent inherited private pipe HANDLE."},
        .usage{u8"<connected-decimal-handle>"},
        .handle{::std::addressof(details::debug_jit_control_handle_callback)},
        .is_exist{::std::addressof(details::debug_jit_control_handle_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::runtime}};
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
