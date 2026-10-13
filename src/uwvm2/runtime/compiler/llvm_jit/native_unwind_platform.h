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

#if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && defined(__linux__) && defined(__arm__)
# include <features.h>
#endif

// This header intentionally has no include guard: each inclusion creates scoped capability macros that the includer must pop.
#pragma push_macro("UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED")
#undef UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED

#if defined(_WIN64) && !(defined(__arm64ec__) || defined(_M_ARM64EC)) && !defined(__CYGWIN__) &&                                                           \
    (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64) || defined(__aarch64__) || defined(_M_ARM64))
# define UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED 0
#endif

#pragma push_macro("UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED")
#undef UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED

// Select the unwind registration ABI, not a CPU allow-list. ELF DWARF targets
// share LLVM's EH-frame registration and the Itanium unwind cursor interface;
// Mach-O and Win64 use their respective registration implementations. Actual
// availability also requires <unwind.h> (POSIX) and a successful generated-chain
// probe: having an ELF object format does not prove that its provider works.
// GNU/Linux ARM EHABI uses the opt-in actual .ARM.exidx registry and exported
// libgcc lookup hook, with exact object extents and post-drain retirement. Its
// glibc fallback requires 2.35 or newer. ARM builds using __ARM_DWARF_EH__ use
// their separate DWARF registration. Neither route admits SjLj.
// This capability is shared by the runtime, CLI parser, help, and version output.
#if UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED ||                                                                                                 \
    (!defined(_WIN32) && !defined(__USING_SJLJ_EXCEPTIONS__) &&                                                                                            \
     (!(defined(__arm__) || defined(__thumb__)) || defined(__ARM_DWARF_EH__) || \
      (defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
       defined(__linux__) && defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 35)))) &&                                                                           \
     ((defined(__APPLE__) && defined(__MACH__)) || defined(__ELF__)))
# define UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED 0
#endif
