/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Core 3.0
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
#include <span>
#include <type_traits>
#include <utility>
// macro
#include <uwvm2/utils/macro/push_macros.h>

export module uwvm2.validation.standard.wasm3:validator;

import uwvm2.validation.standard.wasm3.relaxed_simd;
import fast_io;
import uwvm2.validation.standard.wasm3.retained_plan;
import uwvm2.validation.standard.wasm3.memory_validation;
import uwvm2.validation.standard.wasm3.scalar_memory_semantics;
import uwvm2.validation.standard.wasm3.memory_page_semantics;
import uwvm2.validation.standard.wasm3.address_limits;
import uwvm2.validation.standard.wasm3.atomic_immediate;
import uwvm2.validation.standard.wasm3.table_validation;
import uwvm2.validation.standard.wasm3.threads;
import uwvm2.validation.standard.wasm3.atomic_semantics;
import uwvm2.validation.standard.wasm3.bulk_memory_semantics;
import uwvm2.validation.standard.wasm3.tail_call;
import uwvm2.validation.standard.wasm3.reference_policy;
import uwvm2.validation.standard.wasm3.typed_stack_semantics;
import uwvm2.validation.standard.wasm3.i32_numeric_event;
import uwvm2.validation.standard.wasm3.i64_numeric_event;
import uwvm2.validation.standard.wasm3.integer_width_event;
import uwvm2.validation.standard.wasm3.integer_compare_event;
import uwvm2.validation.standard.wasm3.table_access_event;
import uwvm2.validation.standard.wasm3.typed_select_event;
import uwvm2.validation.standard.wasm3.reference_validation;
import uwvm2.validation.standard.wasm3.recursive_type_validation;
import uwvm2.validation.standard.wasm3.local_declarations;
import uwvm2.validation.standard.wasm3.declaration_policy;
import uwvm2.validation.standard.wasm3.exception_policy;
import uwvm2.validation.standard.wasm3.exception_immediate;
import uwvm2.validation.standard.wasm3.exception_validation;
import uwvm2.validation.standard.wasm3.gc_validation;
import uwvm2.validation.standard.wasm3.gc_i31_semantics;
import uwvm2.validation.standard.wasm3.reference_constant_event;
import uwvm2.validation.standard.wasm3.reference_unary_event;
import uwvm2.validation.standard.wasm3.reference_cast;
import uwvm2.validation.standard.wasm3.heap_immediate;
import uwvm2.validation.standard.wasm3.gc_immediate;
import uwvm2.validation.standard.wasm2;
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
