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

// std
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
// macro
#include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>

#include "uwvm_runtime_generation.h"
#include "uwvm_runtime_execution_entry.h"
#include "uwvm_runtime_generated_wasm_bridge.h"
#include "uwvm_runtime_imported_function_lookup.h"
#include "uwvm_runtime_local_imported_provider_callbacks.h"
#include "uwvm_runtime_state_signature.h"
#include "uwvm_runtime_wasip1_memory_bindings.h"
// The FP execution boundary is shared by interpreter-only and LLVM builds.
// Keep this outside UWVM_RUNTIME_LLVM_JIT, matching the non-module runtime;
// an interpreter-only module build must not silently omit host FP isolation.
#include "uwvm_runtime_wasm_fp_environment.h"
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include "uwvm_runtime_call_indirect_table_views.h"
# include "uwvm_runtime_llvm_expanded_lane_unroll_policy.h"
# include "uwvm_runtime_native_unwind_execution_gate.h"
#endif

// platform
#if !UWVM_HAS_BUILTIN(__builtin_alloca) && (defined(_WIN32) && !defined(__WINE__) && !defined(__BIONIC__) && !defined(__CYGWIN__))
# include <malloc.h>
#elif !UWVM_HAS_BUILTIN(__builtin_alloca)
# include <alloca.h>
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
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
# include <llvm/IR/LegacyPassManager.h>
# include <llvm/IR/Module.h>
# include <llvm/IR/PassManager.h>
# include <llvm/IR/Verifier.h>
# include <llvm/Object/ObjectFile.h>
# include <llvm/PassRegistry.h>
# include <llvm/Passes/OptimizationLevel.h>
# include <llvm/Passes/PassBuilder.h>
# include <llvm/Support/CodeGen.h>
# include <llvm/Support/MemoryBuffer.h>
# include <llvm/Support/TargetSelect.h>
# include <llvm/Target/TargetMachine.h>
# include <llvm/TargetParser/Host.h>
# include <llvm/Transforms/InstCombine/InstCombine.h>
# include <llvm/Transforms/Scalar.h>
# include <llvm/Transforms/Scalar/GVN.h>
# include <llvm/Transforms/Utils.h>
#endif

#include "uwvm_runtime_native_unwind.h"

import fast_io;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard.wasm1.features;
import uwvm2.parser.wasm.standard.wasm1.type;
import uwvm2.parser.wasm.standard.wasm1p1.type;
import uwvm2.object.memory;
import uwvm2.validation.error;
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
import uwvm2.runtime.compiler.uwvm_int.compile_all_from_uwvm;
import uwvm2.runtime.compiler.uwvm_int.optable;
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
import uwvm2.runtime.compiler.llvm_jit.compile_all_from_uwvm;
import uwvm2.runtime.llvm_jit_cache;
#endif
import uwvm2.utils.container;
import uwvm2.utils.debug;
import uwvm2.utils.hash;
import uwvm2.utils.thread;
import uwvm2.uwvm_predefine.utils.ansies;
import uwvm2.uwvm.crtmain.global;
import uwvm2.uwvm.io;
import uwvm2.uwvm.imported.wasi.wasip1.storage;
import uwvm2.uwvm.runtime.storage;
import uwvm2.uwvm.wasm.feature;
import uwvm2.uwvm.wasm.type;
import uwvm2.uwvm.runtime.runtime_mode;
import uwvm2.uwvm.wasm.storage;
import uwvm2.runtime;

#include "uwvm_runtime.default.cpp"
