/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <cstddef>
#include <cstdint>
#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2.validation.standard.wasm3.tail_call;
import fast_io;
import uwvm2.utils.container;
import uwvm2.parser.wasm.standard.wasm1;
import uwvm2.parser.wasm.base;
import uwvm2.validation.error;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "tail_call.h"
