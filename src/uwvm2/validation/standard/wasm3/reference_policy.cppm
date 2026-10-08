/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <memory>
#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2.validation.standard.wasm3.reference_policy;
import fast_io;
import uwvm2.utils.container;
import uwvm2.parser.wasm.standard.wasm3.type;
import uwvm2.parser.wasm.base;
import uwvm2.validation.error;
import uwvm2.validation.standard.wasm3.tail_call;
import uwvm2.validation.standard.wasm3.heap_immediate;
import uwvm2.validation.standard.wasm3.value_immediate;
import uwvm2.validation.standard.wasm3.recursive_type_validation;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "reference_policy.h"
