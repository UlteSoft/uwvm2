/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

module;

#include <cstddef>
#include <cstdint>

#include <uwvm2/utils/macro/push_macros.h>
export module uwvm2.parser.wasm.standard.wasm1p1.features:call_indirect_immediate;

import fast_io;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "call_indirect_immediate.h"
