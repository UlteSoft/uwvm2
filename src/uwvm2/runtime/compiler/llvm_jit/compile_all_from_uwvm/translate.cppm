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
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// platform
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <llvm/Bitcode/BitcodeReader.h>
#  include <llvm/Bitcode/BitcodeWriter.h>
#  include <llvm/IR/Attributes.h>
#  include <llvm/IR/BasicBlock.h>
#  include <llvm/IR/CallingConv.h>
#  include <llvm/IR/Constants.h>
#  include <llvm/IR/Function.h>
#  include <llvm/IR/IRBuilder.h>
#  include <llvm/IR/LLVMContext.h>
#  include <llvm/IR/Module.h>
#  include <llvm/IR/Type.h>
#  include <llvm/IR/Value.h>
#  include <llvm/IR/Verifier.h>
#  include <llvm/Linker/Linker.h>
#  include <llvm/Support/raw_ostream.h>
# endif

export module uwvm2.runtime.compiler.llvm_jit.compile_all_from_uwvm:translate;

import fast_io;
import uwvm2.uwvm_predefine.io;
import uwvm2.uwvm_predefine.utils.ansies;
import uwvm2.utils.container;
import uwvm2.utils.debug;
import uwvm2.utils.thread;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.binfmt.binfmt_ver1;
import uwvm2.validation.error;
import uwvm2.uwvm.runtime.storage;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "translate.h"
