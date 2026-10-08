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

#pragma once

#ifndef UWVM_MODULE
# include "recursive_type_binary.h"
# include "function_signature.h"
# include "heap_immediate.h"
# include "value_immediate.h"
# include "recursive_type_validation.h"
# include "recursive_type_registry.h"
# include "local_declarations.h"
# include "typed_stack_semantics.h"
# include "gc_i31_semantics.h"
# include "reference_constant_event.h"
# include "reference_unary_event.h"
# include "i32_numeric_event.h"
# include "i64_numeric_event.h"
# include "integer_width_event.h"
# include "integer_compare_event.h"
# include "table_access_event.h"
# include "typed_select_event.h"
# include "fused_i32_sink.h"
# include "reference_validation.h"
# include "reference_cast.h"
# include "gc_validation.h"
# include "gc_immediate.h"
# include "constant_expression.h"
# include "memory_immediate.h"
# include "address_limits.h"
# include "atomic_immediate.h"
# include "memory_validation.h"
# include "scalar_memory_event.h"
# include "scalar_memory_semantics.h"
# include "memory_page_event.h"
# include "memory_page_semantics.h"
# include "simd_event.h"
# include "table_validation.h"
# include "relaxed_simd.h"
# include "threads.h"
# include "atomic_event.h"
# include "atomic_semantics.h"
# include "bulk_memory_event.h"
# include "bulk_memory_semantics.h"
# include "tail_call.h"
# include "reference_policy.h"
# include "declaration_policy.h"
# include "exception_immediate.h"
# include "exception_policy.h"
# include "exception_validation.h"
# include "retained_plan.h"
# include "call_dependency_event.h"
# include "validator.h"
#endif
