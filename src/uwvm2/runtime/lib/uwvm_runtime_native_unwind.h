/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <uwvm2/runtime/compiler/llvm_jit/native_unwind_platform.h>

// Keep native-unwind detection identical for the traditional aggregation translation unit and the C++ module consumer.
// Native mode owns JIT call-stack reporting: it uses registered asynchronous unwind tables and does not emit logical
// push/pop calls. POSIX checked mode also executes a generated recursive-chain probe before omitting instruction frames.
#if defined(UWVM_RUNTIME_LLVM_JIT) && UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED
# define UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE 0
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT) && UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED
# define UWVM2_RUNTIME_LLVM_JIT_ENABLE_NATIVE_UNWIND_BACKTRACE 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_ENABLE_NATIVE_UNWIND_BACKTRACE 0
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT) && UWVM2_RUNTIME_LLVM_JIT_ENABLE_NATIVE_UNWIND_BACKTRACE && !defined(_WIN32) && __has_include(<unwind.h>)
# include <unwind.h>
# define UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE 1
# if defined(__APPLE__)
extern "C" void __register_frame(void const*);
extern "C" void __deregister_frame(void const*);
# endif
#else
# define UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE 0
#endif

#if UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE || UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
# define UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE 0
#endif

#if UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE
# define UWVM2_RUNTIME_LLVM_JIT_UNWIND_REPLACES_INSTRUCTION_FRAMES 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_UNWIND_REPLACES_INSTRUCTION_FRAMES 0
#endif

// POSIX walks registered CFI through the real signal trampoline, without inspecting unregistered machine state.
#if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
# define UWVM2_RUNTIME_LLVM_JIT_HAS_TRAP_FRAME_POINTER_CHAIN 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_HAS_TRAP_FRAME_POINTER_CHAIN 0
#endif

#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED")
#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED")
