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
# include <cstdint>
# include <limits>
# include <memory>
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

#if !defined(UWVM_RUNTIME_UWVM_INTERPRETER) && !defined(UWVM_RUNTIME_LLVM_JIT) && !defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
# error "No runtime backend selected. Include <uwvm2/uwvm/runtime/macro/push_macros.h> before this header (or enable interpreter/JIT in build flags)."
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::runtime_mode
{
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

    /// @brief   The global runtime mode.
    /// @details default = runtime-tiered
    inline ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t global_runtime_mode{
        ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::lazy_compile};  // [global]

    // The debug interpreter is never used as a regular interpreter.
    inline ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t global_runtime_compiler{
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered
#elif defined(UWVM_RUNTIME_LLVM_JIT)
        ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only
#elif defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only
#endif
    };  // [global]
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
