/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-05-07
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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <concepts>
#include <type_traits>
#include <utility>
#include <memory>
#include <limits>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#endif

export module uwvm2.compiler.validation.concepts:code_validation;

import fast_io;
import uwvm2.utils.container;
import uwvm2.utils.debug;
import uwvm2.utils.intrinsics;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.utils;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard;
import uwvm2.compiler.validation.error;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "code_validation.h"
