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
 * | |_| |  \ V  V /    \ V / | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

module;

// std
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <utility>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
// platform
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <llvm/Analysis/TargetTransformInfo.h>
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
# include <uwvm2/runtime/compiler/llvm_jit/section_memory_manager.h>
#endif

export module uwvm2.runtime.compiler.llvm_jit.compile_cu_from_lazy_validator:translate;

import fast_io;
import uwvm2.uwvm_predefine.io;
import uwvm2.uwvm_predefine.utils.ansies;
import uwvm2.utils.container;
import uwvm2.utils.debug;
import uwvm2.utils.thread;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.standard.wasm1;
import uwvm2.parser.wasm.binfmt.binfmt_ver1;
import uwvm2.validation.error;
import uwvm2.validation.standard.wasm1:validator;
import uwvm2.uwvm.wasm.feature;
import uwvm2.uwvm.runtime.storage;
import uwvm2.runtime.compiler.llvm_jit.compile_all_from_uwvm;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "translate.h"
