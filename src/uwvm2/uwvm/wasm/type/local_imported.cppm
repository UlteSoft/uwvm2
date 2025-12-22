/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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

module;

// std
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <memory>
#include <vector>
#include <limits>
#include <algorithm>
#include <type_traits>
#include <utility>
#include <bit>
// macro
#include <uwvm2/utils/macro/push_macros.h>

export module uwvm2.uwvm.wasm.type:local_imported;

import fast_io;
import uwvm2.utils.container;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard.wasm1.type;
import uwvm2.parser.wasm.standard.wasm1.features;
import uwvm2.parser.wasm.standard.wasm1p1.type;
import uwvm2.parser.wasm_custom.customs;
import uwvm2.uwvm.wasm.base;
import uwvm2.uwvm.wasm.feature;
import :para;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "local_imported.h"
