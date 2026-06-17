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

#if defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
    enum class runtime_llvm_jit_policy_t : unsigned
    {
        debug,
        default_policy,
        fast_compile,
        balanced,
        max
    };

    enum class runtime_llvm_jit_lazy_policy_t : unsigned
    {
        auto_policy,
        debug,
        light,
        balanced
    };

    enum class runtime_llvm_jit_full_policy_t : unsigned
    {
        auto_policy,
        debug,
        legacy_light,
        passbuilder_o1,
        passbuilder_o2,
        passbuilder_o3
    };

    enum class runtime_llvm_jit_call_stack_t : unsigned
    {
        auto_policy,
        instruction,
        none,
        unwind,
        unwind_uncheck
    };

    enum class runtime_llvm_jit_cache_path_mode_t : unsigned
    {
        default_path,
        disabled,
        custom_path
    };
#endif

    inline bool custom_runtime_mode_existed{};      // [global]
    inline bool custom_runtime_compiler_existed{};  // [global]

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
    /// @brief   Whether the runtime mode is code interpreted.
    /// @details auto_compile + uwvm_interpreter_only
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

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
    inline constexpr ::std::size_t default_runtime_uwvm_int_loop_unwind_max_size{4096uz};

    /// @brief Whether uwvm-int loop unwind is disabled at runtime.
    inline bool runtime_uwvm_int_disable_loop_unwind{};  // [global]

    /// @brief Whether uwvm-int opcode conbination peepholes are disabled at runtime.
    inline bool runtime_uwvm_int_disable_opcode_conbination{};  // [global]

    /// @brief Whether uwvm-int delay-local peepholes are disabled at runtime.
    inline bool runtime_uwvm_int_disable_delay_local{};  // [global]

    /// @brief Whether the uwvm-int loop unwind byte-size limit was explicitly configured.
    inline bool runtime_uwvm_int_loop_unwind_max_size_existed{};  // [global]

    /// @brief Maximum Wasm body bytes considered for one loop-unwind decision.
    inline ::std::size_t global_runtime_uwvm_int_loop_unwind_max_size{default_runtime_uwvm_int_loop_unwind_max_size};  // [global]
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT) || defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
    /// @brief Whether the high-level runtime LLVM JIT policy was explicitly configured.
    inline bool runtime_llvm_jit_policy_existed{};  // [global]

    /// @brief High-level LLVM JIT policy preset.
    inline runtime_llvm_jit_policy_t global_runtime_llvm_jit_policy{runtime_llvm_jit_policy_t::default_policy};  // [global]

    /// @brief Whether the lazy/tier-1 runtime LLVM JIT policy was explicitly configured.
    inline bool runtime_llvm_jit_lazy_policy_existed{};  // [global]

    /// @brief Lazy/tier-1 LLVM JIT policy override.
    inline runtime_llvm_jit_lazy_policy_t global_runtime_llvm_jit_lazy_policy{runtime_llvm_jit_lazy_policy_t::auto_policy};  // [global]

    /// @brief Whether the full/tier-2 runtime LLVM JIT policy was explicitly configured.
    inline bool runtime_llvm_jit_full_policy_existed{};  // [global]

    /// @brief Full/tier-2 LLVM JIT policy override.
    inline runtime_llvm_jit_full_policy_t global_runtime_llvm_jit_full_policy{runtime_llvm_jit_full_policy_t::auto_policy};  // [global]

    /// @brief Whether the runtime LLVM JIT call-stack tracking mode was explicitly configured.
    inline bool runtime_llvm_jit_call_stack_existed{};  // [global]

    /// @brief Runtime LLVM JIT call-stack tracking mode.
    inline runtime_llvm_jit_call_stack_t global_runtime_llvm_jit_call_stack{runtime_llvm_jit_call_stack_t::auto_policy};  // [global]

    /// @brief Whether runtime LLVM JIT IR verification is disabled by command line.
    inline bool runtime_llvm_jit_disable_ir_verifaction{};  // [global]

    /// @brief Whether runtime LLVM JIT cache signature generation is disabled by command line.
    inline bool runtime_llvm_jit_cache_no_sign{};  // [global]

    /// @brief Whether runtime LLVM JIT cache signature verification is disabled by command line.
    inline bool runtime_llvm_jit_cache_no_verify{};  // [global]

    /// @brief Whether runtime LLVM JIT cache path mode was explicitly configured.
    inline bool runtime_llvm_jit_cache_path_existed{};  // [global]

    /// @brief Runtime LLVM JIT cache path mode.
    inline runtime_llvm_jit_cache_path_mode_t global_runtime_llvm_jit_cache_path_mode{
        runtime_llvm_jit_cache_path_mode_t::default_path};  // [global]

    /// @brief Runtime LLVM JIT custom cache directory path.
    inline ::uwvm2::utils::container::u8string global_runtime_llvm_jit_cache_path{};  // [global]
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
    /// @brief Whether Tier 0 uwvm-int lazy interpreter fallback is disabled in tiered mode.
    inline bool runtime_tiered_disable_uwvm_int_lazy_interpreter{};  // [global]

    /// @brief Whether Tier 2 background full-module LLVM JIT is disabled in tiered mode.
    inline bool runtime_tiered_disable_llvm_full_jit{};  // [global]
#endif

    /// @brief   The global runtime mode.
    /// @details default = lazy_compile
    inline ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t global_runtime_mode{
        ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::lazy_compile};  // [global]

    /// @brief   The global runtime compiler backend.
    /// @details default = uwvm_interpreter_llvm_jit_tiered when the tiered backend is available
    inline ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t global_runtime_compiler{
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered
#elif defined(UWVM_RUNTIME_LLVM_JIT)
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
