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
// macro
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::runtime_mode
{
    enum class runtime_mode_t : unsigned
    {
        full_compile
    };

    enum class runtime_compiler_t : unsigned
    {
#if !defined(UWVM_RUNTIME_HAS_BACKEND)
        none_backend,
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        uwvm_interpreter_only,
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
        llvm_jit_only,
#endif
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#endif
