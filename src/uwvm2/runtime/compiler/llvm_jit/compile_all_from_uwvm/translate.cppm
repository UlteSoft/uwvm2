/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-03-30
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
# include <atomic>
# include <bit>
# include <climits>
# include <concepts>
# include <coroutine>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <exception>
# include <limits>
# include <memory>
# include <mutex>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <uwvm2/runtime/lib/uwvm_runtime_generated_wasm_bridge.h>
# include <uwvm2/runtime/lib/uwvm_runtime_local_imported_provider_callbacks.h>
// platform
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <uwvm2/runtime/compiler/llvm_jit/pinned_version.h>
#  include <uwvm2/runtime/compiler/shared/strict_float.h>
#  include <llvm/Bitcode/BitcodeReader.h>
#  include <llvm/Bitcode/BitcodeWriter.h>
#  include <llvm/IR/Attributes.h>
#  include <llvm/IR/BasicBlock.h>
#  include <llvm/IR/CallingConv.h>
#  include <llvm/IR/Constants.h>
#  include <llvm/IR/Function.h>
#  include <llvm/IR/IRBuilder.h>
#  include <llvm/IR/InlineAsm.h>
#  include <llvm/IR/Intrinsics.h>
#  include <llvm/IR/LLVMContext.h>
#  include <llvm/IR/Metadata.h>
#  include <llvm/IR/Module.h>
#  include <llvm/IR/Type.h>
#  include <llvm/IR/Value.h>
#  include <llvm/IR/Verifier.h>
#  include <llvm/Linker/Linker.h>
#  include <llvm/Support/DynamicLibrary.h>
#  include <llvm/TargetParser/Host.h>
#  include <llvm/TargetParser/Triple.h>
#  include <llvm/IR/LegacyPassManager.h>
#  include <llvm/Pass.h>
#  include <llvm/PassRegistry.h>
#  include <llvm/InitializePasses.h>
#  include <llvm/Transforms/Scalar/Scalarizer.h>
# endif

export module uwvm2.runtime.compiler.llvm_jit.compile_all_from_uwvm:translate;

import fast_io;
import uwvm2.uwvm_predefine.io;
import uwvm2.uwvm_predefine.utils.ansies;
import uwvm2.utils.container;
import uwvm2.utils.debug;
import uwvm2.utils.hash;
import uwvm2.utils.thread;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard.wasm1;
import uwvm2.parser.wasm.standard.wasm1p1.type;
import uwvm2.parser.wasm.standard.wasm1p1.opcode;
import uwvm2.parser.wasm.standard.wasm1p1.features;
import uwvm2.parser.wasm.standard.wasm2.features;
import uwvm2.parser.wasm.binfmt.binfmt_ver1;
import uwvm2.validation.error;
import uwvm2.validation.concepts;
import uwvm2.validation.standard.wasm2;
import uwvm2.object;
import uwvm2.object.memory.flags;
import uwvm2.runtime.compiler.shared.wasm1p1_simd;
import uwvm2.uwvm.io;
import uwvm2.uwvm.utils.memory;
import uwvm2.uwvm.wasm.feature;
import uwvm2.uwvm.wasm.type;
import uwvm2.uwvm.wasm.storage;
import uwvm2.uwvm.runtime.storage;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "translate.h"
