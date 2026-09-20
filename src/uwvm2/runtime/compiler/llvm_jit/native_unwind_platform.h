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

// This header intentionally has no include guard: each inclusion creates scoped capability macros that the includer must pop.
#pragma push_macro("UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED")
#undef UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED

#if defined(_WIN64) && !(defined(__arm64ec__) || defined(_M_ARM64EC)) && !defined(__CYGWIN__) &&                                                             \
    (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64) || defined(__aarch64__) || defined(_M_ARM64))
# define UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED 0
#endif

#pragma push_macro("UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED")
#undef UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED

// Native unwind is available only on the platform/ISA allow-list below. Header availability alone is insufficient:
// checked POSIX mode must recover a real generated recursive chain before JIT logical-frame emission is disabled.
// Keep it independent of LLVM enablement and unwind-header availability so the runtime and CLI can share one allow-list.
#if (defined(__APPLE__) && !defined(_WIN32)) ||                                                                                                              \
    UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED ||                                                                                                  \
    ((defined(__linux__) || defined(__FreeBSD__)) &&                                                                                                        \
     ((defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)) && !defined(__ILP32__)))
# define UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED 0
#endif
