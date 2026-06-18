/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-14
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
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <llvm/ADT/StringRef.h>
# include <llvm/Bitcode/BitcodeWriter.h>
# include <llvm/ExecutionEngine/ObjectCache.h>
# include <llvm/IR/Module.h>
# include <llvm/Support/MemoryBuffer.h>
# include <llvm/Support/raw_ostream.h>
#endif

export module uwvm2.runtime.llvm_jit_cache:llvm_object_cache;

import fast_io;
import fast_io_crypto;
import uwvm2.utils.container;
import uwvm2.uwvm.io;
import uwvm2.uwvm.utils.ansies;
import :format;
import :environment;
import :store;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

// The LLVM ObjectCache adapter is included from the header to keep callback behavior identical across build modes.
#include "llvm_object_cache.h"
