/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <cstddef>
export module uwvm2.uwvm.debugger:native_next_policy;
import :native_instruction_semantics;
import :native_owned_instruction_semantics;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "native_next_policy.h"
