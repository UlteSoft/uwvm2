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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
// macro
#include <uwvm2/utils/macro/push_macros.h>

export module uwvm2.runtime.compiler.uwvm_int.compile_cu_from_lazy_validator:translate;

import fast_io;
import uwvm2.utils.container;
import uwvm2.utils.debug;
import uwvm2.utils.thread;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.standard.wasm1;
import uwvm2.validation.error;
import uwvm2.validation.standard.wasm1:validator;
import uwvm2.uwvm.wasm.feature;
import uwvm2.uwvm.runtime.storage;
import uwvm2.runtime.compiler.uwvm_int.optable;
import uwvm2.runtime.compiler.uwvm_int.utils;
import uwvm2.runtime.compiler.uwvm_int.compile_all_from_uwvm;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "translate.h"
