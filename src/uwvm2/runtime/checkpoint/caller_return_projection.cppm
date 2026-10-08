/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>
export module uwvm2.runtime.checkpoint.caller_return_projection;
import uwvm2.runtime.checkpoint.materialization;
import uwvm2.runtime.checkpoint.shadow_ledger;
import uwvm2.object.global;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "caller_return_projection.h"
