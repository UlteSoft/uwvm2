/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

module;

// macro
#include <uwvm2/utils/macro/push_macros.h>

export module uwvm2.uwvm.wasm.warning:data_count_section;

import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard.wasm1p1.features;
import uwvm2.uwvm.wasm.type;
import :warn_storage;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "data_count_section.h"
