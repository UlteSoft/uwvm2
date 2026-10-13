/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <cstddef>
#include <cstdint>
#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2.validation.standard.wasm3.threads;
import fast_io;
import uwvm2.parser.wasm.base;
import uwvm2.validation.error;
import uwvm2.validation.standard.wasm3.atomic_immediate;
import uwvm2.validation.standard.wasm3.address_limits;
import uwvm2.validation.standard.wasm3.memory_immediate;
import uwvm2.validation.standard.wasm3.memory_validation;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "threads.h"
