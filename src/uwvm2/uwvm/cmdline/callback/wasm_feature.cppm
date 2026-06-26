/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

module;

// std
#include <cstddef>
#include <cstdint>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>

export module uwvm2.uwvm.cmdline.callback:wasm_feature;

import fast_io;
import uwvm2.utils.container;
import uwvm2.utils.ansies;
import uwvm2.utils.cmdline;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard.wasm1p1.features;
import uwvm2.uwvm.io;
import uwvm2.uwvm.utils.ansies;
import uwvm2.uwvm.cmdline.params;
import uwvm2.uwvm.wasm.storage;
import uwvm2.uwvm.wasm.feature;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "wasm_feature.h"
