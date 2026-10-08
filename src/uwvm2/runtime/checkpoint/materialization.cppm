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
#include <utility>
#include <vector>
export module uwvm2.runtime.checkpoint.materialization;
import fast_io;
import uwvm2.parser.wasm.standard.wasm3.type;
#define UWVM_MODULE
#define UWVM_MODULE_EXPORT export
#include "materialization.h"
