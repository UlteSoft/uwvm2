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
#include <array>
#include <atomic>
#include <thread>
#include <cstddef>
#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>

export module uwvm2.runtime;

import uwvm2.utils.container;
import uwvm2.utils.thread;
import uwvm2.runtime.exception;
import uwvm2.runtime.gc;
import uwvm2.runtime.checkpoint.materialization;
import uwvm2.uwvm.debugger.wasm_state;
import uwvm2.uwvm.debugger.wasm_mutation;
import uwvm2.uwvm.debugger.wasip1_state;
import uwvm2.uwvm.debugger.wasip1_portable_checkpoint;
import uwvm2.uwvm.debugger.checkpoint_state;
// The preload descriptor belongs to uwvm2.uwvm.wasm.type, not this API module.
// Import its owner before declaring functions that accept the descriptor; a
// fresh exported forward declaration here creates a conflicting module owner.
import uwvm2.uwvm.wasm.type;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "uwvm_runtime.h"
