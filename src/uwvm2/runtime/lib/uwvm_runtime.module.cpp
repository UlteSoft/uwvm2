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

module;

// std
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
// Defining the object-emission coroutine requires coroutine_traits in this TU;
// importing the task type does not expose its module's standard-library headers.
#include <coroutine>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <mutex>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
// macro
#include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>

#include "uwvm_runtime_generation.h"
#include "uwvm_runtime_native_exception_host.h"
#include "uwvm_runtime_checked_size.h"
#include "uwvm_runtime_activation_cleanup.h"
#include "uwvm_runtime_tiered_entry_sampling.h"
#include "uwvm_runtime_tiered_publication.h"
#include "uwvm_runtime_checked_tiered_aliases.h"
#include "uwvm_runtime_execution_entry.h"
#include "uwvm_runtime_native_stack_guard.h"
#include "uwvm_runtime_debug_native_stack.h"
#include "uwvm_runtime_generated_wasm_bridge.h"
#include "uwvm_runtime_debug_activation.h"
#include "uwvm_runtime_imported_function_lookup.h"
#include "uwvm_runtime_logical_activation_overlap.h"
#include "uwvm_runtime_local_imported_provider_callbacks.h"
#include "uwvm_runtime_state_signature.h"
#include "uwvm_runtime_wasip1_memory_bindings.h"
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
#include <fcntl.h>
#endif
// The FP execution boundary is shared by interpreter-only and LLVM builds.
// Keep this outside UWVM_RUNTIME_LLVM_JIT, matching the non-module runtime;
// an interpreter-only module build must not silently omit host FP isolation.
#include "uwvm_runtime_wasm_fp_environment.h"
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include "uwvm_runtime_call_indirect_table_views.h"
# include "uwvm_runtime_pending_code_ranges.h"
# include <uwvm2/runtime/compiler/shared/strict_float_jit.h>
# include "uwvm_runtime_llvm_lazy_worker_policy.h"
# include "uwvm_runtime_llvm_expanded_lane_unroll_policy.h"
# include "uwvm_runtime_native_unwind_execution_gate.h"
# include "uwvm_runtime_native_function_address.h"
#endif

// platform
#if !UWVM_HAS_BUILTIN(__builtin_alloca) && (defined(_WIN32) && !defined(__WINE__) && !defined(__BIONIC__) && !defined(__CYGWIN__))
# include <malloc.h>
#elif !UWVM_HAS_BUILTIN(__builtin_alloca)
# include <alloca.h>
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
# if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
// Native Win64 unwind uses fast_io's CONTEXT and NT declarations in this
// implementation unit; an import alone does not make their header-only
// declarations reachable to Clang's module consumer.
#  include <fast_io.h>
# endif
# if defined(__linux__) && defined(__x86_64__)
#  include <csignal>
#  include <linux/futex.h>
#  include <sys/syscall.h>
#  include <ucontext.h>
#  include <unistd.h>
# endif
// Keep SDK declarations needed directly by this runtime implementation's
// native unwind code in its own global fragment. The native-step definitions
// themselves are imported from their actual debugger module below.
# if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__)) && \
     defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT == 1
#  include <windows.h>
#  if defined(_MSC_VER) && !defined(__clang__)
#   include <intrin.h>
#  endif
#  include <uwvm2/utils/control/win32_abi.h>
# endif
// Keep directly needed opt-in Mach SDK/ABI declarations in this fragment;
// its backend definitions retain debugger module ownership through import.
# if defined(__APPLE__) && defined(__x86_64__) && \
     defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT == 1
#  include <TargetConditionals.h>
#  if TARGET_OS_OSX
#   include <cerrno>
#   include <mach/mach.h>
#   include <mach/i386/thread_status.h>
#   include <mach/i386/exception.h>
#   include <mach/exc.h>
#   include <Security/SecTask.h>
#   include <CoreFoundation/CoreFoundation.h>
#   include <uwvm2/uwvm/debugger/posix_abi.h>
#  endif
# endif
# include <llvm/Analysis/TargetTransformInfo.h>
# include <llvm/ADT/StringMap.h>
# include <llvm/Bitcode/BitcodeReader.h>
# include <llvm/Bitcode/BitcodeWriter.h>
# include <llvm/ExecutionEngine/ExecutionEngine.h>
# include <llvm/ExecutionEngine/JITEventListener.h>
# include <llvm/ExecutionEngine/MCJIT.h>
# include <llvm/ExecutionEngine/SectionMemoryManager.h>
# if defined(__APPLE__) && defined(__aarch64__)
#  include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/macho_headers.h>
# endif
# include <llvm/Config/llvm-config.h>
# include <llvm/InitializePasses.h>
# include <llvm/IR/Constants.h>
# include <llvm/IR/IRBuilder.h>
# include <llvm/IR/Intrinsics.h>
# include <llvm/IR/LegacyPassManager.h>
# include <llvm/IR/Metadata.h>
# include <llvm/IR/Module.h>
# include <llvm/IR/PassManager.h>
# include <llvm/IR/Verifier.h>
# include <llvm/MC/TargetRegistry.h>
# include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
# include <llvm/Linker/Linker.h>
# include <llvm/Object/ObjectFile.h>
# include <llvm/PassRegistry.h>
# include <llvm/Passes/OptimizationLevel.h>
# include <llvm/Passes/PassBuilder.h>
# include <llvm/Support/CodeGen.h>
# include <llvm/Support/DynamicLibrary.h>
# include <llvm/Support/MemoryBuffer.h>
# include <llvm/Support/SourceMgr.h>
# include <llvm/Support/TargetSelect.h>
# include <llvm/MC/MCAsmInfo.h>
# include <llvm/Target/TargetMachine.h>
# include <llvm/TargetParser/Host.h>
# include <llvm/TargetParser/Triple.h>
# include <llvm/Transforms/InstCombine/InstCombine.h>
# include <llvm/Transforms/Scalar.h>
# include <llvm/Transforms/Scalar/GVN.h>
# include <llvm/Transforms/Utils.h>
#endif

#include "uwvm_runtime_native_unwind.h"

// Native endpoint parser/listener uses real SDK object types in the global
// module fragment. Its own private runtime types are defined after imports;
// imported full/checkpoint owners must never be redefined in another module.
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && \
    defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
# include <string_view>
# include <llvm/BinaryFormat/COFF.h>
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/elf_headers.h>
# include <llvm/BinaryFormat/MachO.h>
# include <llvm/Object/COFF.h>
# include <llvm/Object/ELFObjectFile.h>
# include <llvm/Object/MachO.h>
# include <llvm/ExecutionEngine/RuntimeDyld.h>
# include <llvm/Support/Casting.h>
# include <llvm/Support/Error.h>
#endif

module uwvm2.runtime;

#if defined(UWVM_RUNTIME_LLVM_JIT)
// The primary runtime interface does not depend on this aggregate. Import the
// debugger's ACTUAL module-owned backend in the IMPLEMENTATION, after the
// runtime module declaration. Textual backend definitions in the global module
// fragment cannot use the runtime-owned sealed activation provider and would
// attach duplicate backend/provider/register types to different modules.
import uwvm2.uwvm.debugger;
#endif

import fast_io;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.binfmt.binfmt_ver1;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard.wasm1.features;
import uwvm2.parser.wasm.standard.wasm1.type;
import uwvm2.parser.wasm.standard.wasm1p1.type;
import uwvm2.parser.wasm.standard.wasm3.type.recursive_type;
import uwvm2.object.memory;
// Reference ABI types and the trap memory printer are used directly below.
// Their declarations are not made visible by importing runtime storage alone.
import uwvm2.object.global;
import uwvm2.uwvm.utils.memory;
import uwvm2.validation.error;
import uwvm2.validation.standard.wasm3.reference_policy;
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
import uwvm2.runtime.exception;
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
import uwvm2.runtime.compiler.uwvm_int.compile_all_from_uwvm;
import uwvm2.runtime.compiler.uwvm_int.compile_cu_from_lazy_validator;
import uwvm2.runtime.compiler.uwvm_int.utils;
import uwvm2.runtime.compiler.uwvm_int.optable;
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
// Cache-key hashing uses this provider directly, not just the cache API.
import fast_io_crypto;
import uwvm2.runtime.compiler.llvm_jit.compile_all_from_uwvm;
import uwvm2.runtime.exception.pending_numeric_outer;
import uwvm2.runtime.exception.pending_numeric_bridge;
import uwvm2.runtime.compiler.llvm_jit.native_exception_landingpad;
import uwvm2.runtime.compiler.llvm_jit.native_exception_symbols;
import uwvm2.runtime.compiler.llvm_jit.compile_cu_from_lazy_validator;
import uwvm2.runtime.llvm_jit_cache;
#endif
import uwvm2.utils.container;
import uwvm2.utils.debug;
import uwvm2.utils.hash;
import uwvm2.utils.thread;
import uwvm2.runtime.gc;
import uwvm2.runtime.checkpoint.materialization;
import uwvm2.runtime.checkpoint.shadow_ledger;
import uwvm2.runtime.checkpoint.dynamic_native_packet;
import uwvm2.runtime.checkpoint.caller_return_projection;
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
// Portable checkpoint DATA only; importing the debugger controller would cycle.
import uwvm2.uwvm.debugger.checkpoint_state;
import uwvm2.uwvm.debugger.checkpoint_binding;
import uwvm2.uwvm.debugger.checkpoint_state_identity;
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && \
    defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
import uwvm2.runtime.gc.source_exception_publisher;
#endif
import uwvm2.runtime.wasm_threads;
import uwvm2.uwvm_predefine.utils.ansies;
import uwvm2.uwvm.crtmain.global;
import uwvm2.uwvm.io;
import uwvm2.uwvm.imported.wasi.wasip1.storage;
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
import uwvm2.uwvm.imported.wasi.wasip1.init;
#endif
import uwvm2.uwvm.runtime.storage;
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && \
    !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
// Import the actual exported definition used by the private by-value staged
// compiler owner. Do not duplicate it in the runtime primary interface or
// textually attach initializer definitions to this implementation's module.
import uwvm2.uwvm.runtime.initializer;
#endif
import uwvm2.uwvm.wasm.feature;
import uwvm2.uwvm.wasm.type;
import uwvm2.uwvm.runtime.runtime_mode;
import uwvm2.uwvm.wasm.storage;
#include "uwvm_runtime_wasip1_native_file.h"

#include "uwvm_runtime.default.cpp"
