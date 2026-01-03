/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author MacroModel
 * @version 2.0.0
 * @date 2025-03-23
 * @copyright APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

/// @brief The following are the macros used by uwvm.
/// @details Use `push_macro` to avoid side effects on existing macros. Please use `pop_macro` in conjunction.

// #pragma once

// macro checkers
#if defined(UWVM_DISABLE_INT) && defined(UWVM_DISABLE_JIT)
# error "Both interpreter and JIT are disabled. Please enable at least one of them."
#endif

// macro definitions
#pragma push_macro("UWVM_RUNTIME_UWVM_INTERPRETER")
#undef UWVM_RUNTIME_UWVM_INTERPRETER
#ifndef UWVM_DISABLE_INT
# if defined(UWVM_USE_DEFAULT_INT)
#  define UWVM_RUNTIME_UWVM_INTERPRETER
# elif defined(UWVM_USE_UWVM_INT)
#  define UWVM_RUNTIME_UWVM_INTERPRETER
# else
#  error "Invalid interpreter mode. Please check the UWVM_USE_DEFAULT_INT or UWVM_USE_UWVM_INT macro."
# endif
#endif

#pragma push_macro("UWVM_RUNTIME_LLVM_JIT")
#undef UWVM_RUNTIME_LLVM_JIT
#ifndef UWVM_DISABLE_JIT
# if defined(UWVM_USE_DEFAULT_JIT)
#  define UWVM_RUNTIME_LLVM_JIT
# elif defined(UWVM_USE_LLVM_JIT)
#  define UWVM_RUNTIME_LLVM_JIT
# else
#  error "Invalid JIT mode. Please check the UWVM_USE_DEFAULT_JIT or UWVM_USE_LLVM_JIT macro."
# endif
#endif

#pragma push_macro("UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED")
#undef UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) && defined(UWVM_RUNTIME_LLVM_JIT)
# define UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED
#endif
