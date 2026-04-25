/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

// std
#include <cstddef>
#include <cstdint>
#include <memory>
// macro
#include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>

import uwvm2.imported.wasi.wasip1;
import uwvm2.runtime;
import uwvm2.uwvm.imported.wasi.wasip1.storage;
import uwvm2.uwvm.wasm.storage;
import uwvm2.uwvm.wasm.type;

#include "host_api.default.cpp"
