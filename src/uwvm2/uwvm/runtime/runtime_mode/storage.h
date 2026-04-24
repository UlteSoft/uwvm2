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
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm3/type/impl.h>
# include <uwvm2/object/impl.h>
# include <uwvm2/uwvm/wasm/impl.h>
# include "mode.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::runtime_mode
{
    using ssize_t = ::std::make_signed_t<::std::size_t>;
    using runtime_compile_threads_type = ssize_t;

    enum class runtime_compile_threads_policy_t : unsigned
    {
        numeric,
        default_policy,
        aggressive
    };

    enum class runtime_scheduling_policy_t : unsigned
    {
        function_count,
        code_size
    };

    inline bool custom_runtime_mode_existed{};      // [global]
    inline bool custom_runtime_compiler_existed{};  // [global]

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
    /// @brief   Whether the runtime mode is code interpreted.
    /// @details lazy_compile + uwvm_interpreter_only
    inline bool is_runtime_mode_code_int_existed{};  // [global]
#endif

#if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
    /// @brief   Whether the runtime mode is debug interpreter.
    /// @details full_compile + debug_interpreter
    inline bool is_runtime_mode_code_debug_existed{};  // [global]
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
    /// @brief   Whether the runtime mode is code jit.
    /// @details lazy_compile + llvm_jit_only
    inline bool is_runtime_mode_code_jit_existed{};  // [global]
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
    /// @brief   Whether the runtime mode is code jit.
    /// @details lazy_compile + uwvm_interpreter_llvm_jit_tiered
    inline bool is_runtime_mode_code_tiered_existed{};  // [global]
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
    /// @brief   Whether the runtime mode is code aot.
    /// @details full_compile + llvm_jit_only
    inline bool is_runtime_mode_code_aot_existed{};  // [global]
#endif

    /// @brief Whether the runtime compile thread count was explicitly configured.
    /// @details When this is false, the runtime will use its default thread selection policy.
    inline bool runtime_compile_threads_existed{};  // [global]

    /// @brief Raw runtime compile thread count from the command line.
    /// @details Only used when the policy is `numeric`. The execution meaning of 0 / positive / negative values is handled later by the runtime scheduler.
    inline runtime_compile_threads_type global_runtime_compile_threads{};  // [global]

    /// @brief Runtime compile thread selection policy from the command line.
    /// @details When the option is omitted, the runtime still falls back to the default adaptive policy.
    inline runtime_compile_threads_policy_t global_runtime_compile_threads_policy{
        ::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_policy_t::default_policy};  // [global]

    /// @brief Effective runtime compile thread count after defaulting / negative-offset resolution.
    /// @details For adaptive policies, this is the resolved upper bound before per-module full-compile adjustment. `0` means no extra compile thread.
    inline ::std::size_t global_runtime_compile_threads_resolved{};  // [global]

    inline constexpr ::std::size_t default_runtime_scheduling_size{4096uz};

    /// @brief Whether the runtime scheduling policy was explicitly configured.
    inline bool runtime_scheduling_policy_existed{};  // [global]

    /// @brief Runtime scheduling policy.
    /// @details The default policy groups local functions by cumulative wasm code-body size.
    inline runtime_scheduling_policy_t global_runtime_scheduling_policy{
        ::uwvm2::uwvm::runtime::runtime_mode::runtime_scheduling_policy_t::code_size};  // [global]

    /// @brief Runtime scheduling granularity.
    /// @details Interpreted either as `functions per task` or `cumulative wasm code-body bytes per task` depending on the selected policy.
    inline ::std::size_t global_runtime_scheduling_size{default_runtime_scheduling_size};  // [global]

    /// @brief   The global runtime mode.
    /// @details default = full_compile
    /// @todo    set default to lazy_compile
    inline ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t global_runtime_mode{
        ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile
    };  // [global]

     /// @todo    set default to uwvm_interpreter_llvm_jit_tiered
    inline ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t global_runtime_compiler{
#if defined(UWVM_RUNTIME_LLVM_JIT)
        ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only
#elif defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only
#else
        ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::none_backend
#endif
    };  // [global]
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
