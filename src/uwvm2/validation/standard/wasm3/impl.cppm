/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Core 3.0
 * @details     antecedent dependency: WebAssembly Release 1.1
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-07-11
 * @copyright   APL-2.0 License
 */

module;

export module uwvm2.validation.standard.wasm3;
export import uwvm2.validation.standard.wasm3.recursive_type_binary;
export import uwvm2.validation.standard.wasm3.function_signature;
export import uwvm2.validation.standard.wasm3.heap_immediate;
export import uwvm2.validation.standard.wasm3.value_immediate;
export import uwvm2.validation.standard.wasm3.recursive_type_validation;
export import uwvm2.validation.standard.wasm3.recursive_type_registry;
export import uwvm2.validation.standard.wasm3.local_declarations;
export import uwvm2.validation.standard.wasm3.typed_stack_semantics;
export import uwvm2.validation.standard.wasm3.gc_i31_semantics;
export import uwvm2.validation.standard.wasm3.reference_constant_event;
export import uwvm2.validation.standard.wasm3.reference_unary_event;
export import uwvm2.validation.standard.wasm3.i32_numeric_event;
export import uwvm2.validation.standard.wasm3.i64_numeric_event;
export import uwvm2.validation.standard.wasm3.integer_width_event;
export import uwvm2.validation.standard.wasm3.integer_compare_event;
export import uwvm2.validation.standard.wasm3.table_access_event;
export import uwvm2.validation.standard.wasm3.typed_select_event;
export import uwvm2.validation.standard.wasm3.fused_i32_sink;
export import uwvm2.validation.standard.wasm3.reference_validation;
export import uwvm2.validation.standard.wasm3.reference_cast;
export import uwvm2.validation.standard.wasm3.gc_validation;
export import uwvm2.validation.standard.wasm3.gc_immediate;
export import uwvm2.validation.standard.wasm3.constant_expression;
export import uwvm2.validation.standard.wasm3.memory_immediate;
export import uwvm2.validation.standard.wasm3.address_limits;
export import uwvm2.validation.standard.wasm3.atomic_immediate;
export import uwvm2.validation.standard.wasm3.memory_validation;
export import uwvm2.validation.standard.wasm3.scalar_memory_event;
export import uwvm2.validation.standard.wasm3.scalar_memory_semantics;
export import uwvm2.validation.standard.wasm3.memory_page_event;
export import uwvm2.validation.standard.wasm3.memory_page_semantics;
export import uwvm2.validation.standard.wasm3.simd_event;
export import uwvm2.validation.standard.wasm3.table_validation;
export import uwvm2.validation.standard.wasm3.relaxed_simd;
export import uwvm2.validation.standard.wasm3.threads;
export import uwvm2.validation.standard.wasm3.atomic_event;
export import uwvm2.validation.standard.wasm3.atomic_semantics;
export import uwvm2.validation.standard.wasm3.bulk_memory_event;
export import uwvm2.validation.standard.wasm3.bulk_memory_semantics;
export import uwvm2.validation.standard.wasm3.tail_call;
export import uwvm2.validation.standard.wasm3.reference_policy;
export import uwvm2.validation.standard.wasm3.declaration_policy;
export import uwvm2.validation.standard.wasm3.exception_immediate;
export import uwvm2.validation.standard.wasm3.exception_policy;
export import uwvm2.validation.standard.wasm3.exception_validation;
export import uwvm2.validation.standard.wasm3.retained_plan;
export import uwvm2.validation.standard.wasm3.call_dependency_event;
export import :validator;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "impl.h"
