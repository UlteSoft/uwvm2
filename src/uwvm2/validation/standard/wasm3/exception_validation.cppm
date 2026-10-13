/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <cstddef>
#include <cstdint>
#include <span>
export module uwvm2.validation.standard.wasm3.exception_validation;
import uwvm2.parser.wasm.standard.wasm3.type;
import uwvm2.validation.standard.wasm3.exception_immediate;
import uwvm2.validation.standard.wasm3.reference_validation;
import uwvm2.validation.standard.wasm3.recursive_type_validation;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "exception_validation.h"
