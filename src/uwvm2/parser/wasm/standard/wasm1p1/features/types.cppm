/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     Extended table, global, value type, and constant-expression parsers
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
 * @copyright   APL-2.0 License
 */

module;

// std
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>
// macro
#include <uwvm2/utils/macro/push_macros.h>

export module uwvm2.parser.wasm.standard.wasm1p1.features:types;

import fast_io;
import uwvm2.utils.container;
import uwvm2.utils.debug;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.binfmt.binfmt_ver1;
import uwvm2.parser.wasm.standard.wasm1;
import uwvm2.parser.wasm.standard.wasm1p1.type;
import uwvm2.parser.wasm.standard.wasm3.type.function_signature;
import uwvm2.validation.standard.wasm3.constant_expression;
import uwvm2.validation.standard.wasm3.address_limits;
import uwvm2.validation.standard.wasm3.function_signature;
import uwvm2.validation.standard.wasm3.heap_immediate;
import uwvm2.validation.standard.wasm3.value_immediate;
import uwvm2.validation.standard.wasm3.recursive_type_binary;
import uwvm2.validation.standard.wasm3.recursive_type_validation;
import :def;
import :feature_def;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "types.h"
