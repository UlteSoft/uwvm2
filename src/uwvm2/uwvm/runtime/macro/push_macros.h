/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
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

// Consult the configured producer only when a JIT build is requested on
// these downstream ELF loader targets. Interpreter-only builds need no LLVM
// headers. Header capability enables compilation; the runtime target gate also
// calls the actual library capability ABI, rejecting stale linked archives.
#if !defined(UWVM_DISABLE_JIT) && (defined(UWVM_USE_DEFAULT_JIT) || defined(UWVM_USE_LLVM_JIT)) && \
    (defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC) || defined(__sparc__) || defined(__sparc))
# include <llvm/Config/llvm-config.h>
#endif

#pragma push_macro("UWVM_RUNTIME_LLVM_JIT")
#undef UWVM_RUNTIME_LLVM_JIT
#ifndef UWVM_DISABLE_JIT
# if (defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)) && \
     !(defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || defined(_ARCH_PPC64)) && \
     (!defined(LLVM_UWVM_ROS_ELF_PPC32_MCJIT) || LLVM_UWVM_ROS_ELF_PPC32_MCJIT != 1 || \
      (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
// Stock PPC32 RuntimeDyld and the little-endian ELF32 ABI remain unsupported.
# elif (defined(__sparc__) || defined(__sparc)) && \
       (!defined(__arch64__) || !defined(LLVM_UWVM_ROS_ELF_SPARCV9_MCJIT) || LLVM_UWVM_ROS_ELF_SPARCV9_MCJIT != 1)
// Only the actual patched SPARC V9 producer enables this native ELF loader.
# elif defined(UWVM_USE_DEFAULT_JIT)
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

#pragma push_macro("UWVM_RUNTIME_DEBUG_INTERPRETER")
#undef UWVM_RUNTIME_DEBUG_INTERPRETER
// debug-int and uwvm-int are different integers and do not interfere with each other
#if defined(UWVM_ENABLE_DEBUG_INT)
# define UWVM_RUNTIME_DEBUG_INTERPRETER
#endif

#pragma push_macro("UWVM_RUNTIME_HAS_BACKEND")
#undef UWVM_RUNTIME_HAS_BACKEND
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
# define UWVM_RUNTIME_HAS_BACKEND
#endif

#pragma push_macro("UWVM_RUNTIME_HAS_DEBUGGER_BACKEND")
#undef UWVM_RUNTIME_HAS_DEBUGGER_BACKEND
#if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
# define UWVM_RUNTIME_HAS_DEBUGGER_BACKEND
#endif
