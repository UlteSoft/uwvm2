/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

module;

// std
#include <memory>
#include <type_traits>
// macro
#include <uwvm2/utils/macro/push_macros.h>

export module uwvm2.uwvm.cmdline.params:wasm_feature;

import fast_io;
import uwvm2.utils.container;
import uwvm2.utils.cmdline;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "wasm_feature.h"
