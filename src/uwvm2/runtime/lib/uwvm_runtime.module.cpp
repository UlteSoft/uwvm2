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
#include <functional>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>
// macro
#include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>

// platform
#if !UWVM_HAS_BUILTIN(__builtin_alloca) && (defined(_WIN32) && !defined(__WINE__) && !defined(__BIONIC__) && !defined(__CYGWIN__))
# include <malloc.h>
#elif !UWVM_HAS_BUILTIN(__builtin_alloca)
# include <alloca.h>
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <llvm/Analysis/TargetTransformInfo.h>
# include <llvm/ADT/StringMap.h>
# include <llvm/ExecutionEngine/ExecutionEngine.h>
# include <llvm/ExecutionEngine/MCJIT.h>
# include <llvm/ExecutionEngine/SectionMemoryManager.h>
# include <llvm/IR/LegacyPassManager.h>
# include <llvm/Linker/Linker.h>
# include <llvm/Support/SourceMgr.h>
# include <llvm/Support/TargetSelect.h>
# include <llvm/Target/TargetMachine.h>
# include <llvm/TargetParser/Host.h>
# include <llvm/Transforms/InstCombine/InstCombine.h>
# include <llvm/Transforms/Scalar.h>
# include <llvm/Transforms/Scalar/GVN.h>
# include <llvm/Transforms/Utils.h>
#endif

import fast_io;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard.wasm1.features;
import uwvm2.parser.wasm.standard.wasm1.type;
import uwvm2.parser.wasm.standard.wasm1p1.type;
import uwvm2.runtime.compiler.uwvm_int.compile_all_from_uwvm;
import uwvm2.runtime.compiler.uwvm_int.optable;
import uwvm2.runtime.compiler.llvm_jit.compile_all_from_uwvm;
import uwvm2.utils.container;
import uwvm2.uwvm.io;
import uwvm2.uwvm.imported.wasi.wasip1.storage;
import uwvm2.uwvm.wasm.feature;
import uwvm2.uwvm.wasm.type;
import uwvm2.uwvm.runtime.runtime_mode;
import uwvm2.uwvm.wasm.storage;
import uwvm2.runtime;

#include "uwvm_runtime.default.cpp"
