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
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if !defined(UWVM_RUNTIME_UWVM_INTERPRETER) && !defined(UWVM_RUNTIME_LLVM_JIT)
// The debug interpreter is never used as a regular interpreter.
# error "No runtime backend selected. Include <uwvm2/uwvm/runtime/macro/push_macros.h> before this header (or enable interpreter/JIT in build flags)."
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::runtime_mode
{
    enum class runtime_mode_t : unsigned
    {
        lazy_compile,
        lazy_compile_with_full_code_verification,
        full_compile
    };

    enum class runtime_compiler_t : unsigned
    {
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        uwvm_interpreter_only,
#endif
#if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
        debug_interpreter,
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        uwvm_interpreter_llvm_jit_tiered,
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
        llvm_jit_only,
#endif
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
