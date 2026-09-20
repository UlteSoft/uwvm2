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
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <bit>
#include <limits>
#include <memory>
#include <type_traits>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>

export module uwvm2.runtime.compiler.uwvm_int.optable:memory;

import fast_io;
import uwvm2.utils.container;
import uwvm2.utils.debug;
// The memory optable directly uses rwlock_pause and grow_strict. Imports of
// their users do not re-export these implementation dependencies.
import uwvm2.utils.mutex;
import uwvm2.parser.wasm.standard.wasm1;
import uwvm2.object;
import uwvm2.object.memory.flags;
import :define;
import :storage;
import :register_ring;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "memory.h"
