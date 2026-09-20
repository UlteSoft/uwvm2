/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 2.0 (2024-08-09)
 * @details     Code-validation strategy selected by the wasm2 parser COP tag
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-07-11
 * @copyright   APL-2.0 License
 */

module;

// std
#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <concepts>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
// macro
#include <uwvm2/utils/macro/push_macros.h>

export module uwvm2.validation.standard.wasm2:validator;

import fast_io;
import uwvm2.utils.container;
import uwvm2.utils.debug;
import uwvm2.utils.intrinsics;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.binfmt.binfmt_ver1;
import uwvm2.parser.wasm.utils;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard;
import uwvm2.validation.error;
import uwvm2.validation.concepts;
export import uwvm2.validation.standard.wasm1;
export import uwvm2.validation.standard.wasm1p1;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "validator.h"
