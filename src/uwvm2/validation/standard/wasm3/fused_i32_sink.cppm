/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <cstddef>
#include <cstdint>
#include <limits>
export module uwvm2.validation.standard.wasm3.fused_i32_sink;
import uwvm2.validation.standard.wasm3.i32_numeric_event;
import uwvm2.validation.standard.wasm3.i64_numeric_event;
import uwvm2.validation.standard.wasm3.integer_width_event;
import uwvm2.validation.standard.wasm3.integer_compare_event;
import uwvm2.validation.standard.wasm3.table_access_event;
import uwvm2.validation.standard.wasm3.typed_select_event;
import uwvm2.validation.standard.wasm3.scalar_memory_event;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "fused_i32_sink.h"
