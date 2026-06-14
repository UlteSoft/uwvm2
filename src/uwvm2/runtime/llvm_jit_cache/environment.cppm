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
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#if defined(__unix__) || defined(__APPLE__) || defined(__linux__) || defined(__linux)
# include <unistd.h>
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
# include <llvm/Config/llvm-config.h>
# include <llvm/ADT/StringMap.h>
# include <llvm/ADT/StringRef.h>
# include <llvm/Target/TargetMachine.h>
# include <llvm/TargetParser/Host.h>
#endif

export module uwvm2.runtime.llvm_jit_cache:environment;

import fast_io;
import uwvm2.utils.container;
import uwvm2.uwvm.runtime.runtime_mode;
import :format;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "environment.h"
