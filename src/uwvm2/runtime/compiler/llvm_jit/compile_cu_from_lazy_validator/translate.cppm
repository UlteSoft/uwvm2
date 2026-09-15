/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
// The global module fragment must also make the shared helpers for strict LLVM lowering
// visible. Updating only the non-module header path would leave module builds
// with missing declarations or inconsistent floating-point behavior.

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V / | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

module;

// std
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
// platform
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <llvm/Analysis/TargetTransformInfo.h>
# include <uwvm2/runtime/compiler/shared/strict_float_jit.h>
# include <llvm/Config/llvm-config.h>
# include <llvm/ExecutionEngine/ExecutionEngine.h>
# include <llvm/ExecutionEngine/MCJIT.h>
# include <llvm/ExecutionEngine/SectionMemoryManager.h>
# include <llvm/InitializePasses.h>
# include <llvm/IR/LegacyPassManager.h>
# include <llvm/IR/Verifier.h>
# include <llvm/PassRegistry.h>
# include <llvm/Support/TargetSelect.h>
# include <llvm/Target/TargetMachine.h>
# include <llvm/TargetParser/Host.h>
# include <llvm/Transforms/InstCombine/InstCombine.h>
# include <llvm/Transforms/Scalar.h>
# include <llvm/Transforms/Scalar/GVN.h>
# include <llvm/Transforms/Utils.h>
# if defined(__APPLE__) && !defined(_WIN32) && __has_include(<unwind.h>)
#  include <unwind.h>
# endif
#endif

export module uwvm2.runtime.compiler.llvm_jit.compile_cu_from_lazy_validator:translate;

import fast_io;
import fast_io_crypto;
import uwvm2.uwvm_predefine.io;
import uwvm2.uwvm_predefine.utils.ansies;
import uwvm2.utils.container;
import uwvm2.utils.debug;
import uwvm2.utils.hash;
import uwvm2.utils.thread;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.standard.wasm1;
import uwvm2.parser.wasm.standard.wasm1p1.type;
import uwvm2.parser.wasm.standard.wasm1p1.opcode;
import uwvm2.parser.wasm.standard.wasm1p1.features;
import uwvm2.parser.wasm.binfmt.binfmt_ver1;
import uwvm2.validation.error;
import uwvm2.validation.concepts;
import uwvm2.validation.standard.wasm1;
import uwvm2.validation.standard.wasm1p1;
import uwvm2.validation.standard.wasm2;
import uwvm2.uwvm.wasm.feature;
import uwvm2.uwvm.runtime.storage;
// The lazy validator invokes the shared SIMD visitor directly; importing the
// eager LLVM translator does not make that visitor reachable here.
import uwvm2.runtime.compiler.shared.wasm1p1_simd;
import uwvm2.runtime.compiler.llvm_jit.compile_all_from_uwvm;
import uwvm2.runtime.llvm_jit_cache;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "translate.h"
