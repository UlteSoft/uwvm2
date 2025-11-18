
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
#include <cstddef>
#include <cstdint>
#include <climits>
#include <type_traits>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>

export module uwvm2.uwvm.wasm.warning:element_section;

import fast_io;
import uwvm2.utils.container;
import uwvm2.utils.ansies;
import uwvm2.utils.debug;
import uwvm2.utils.madvise;
import uwvm2.utils.utf;
import uwvm2.parser.wasm.base;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard;
import uwvm2.parser.wasm.binfmt.base;
import uwvm2.uwvm.io;
import uwvm2.uwvm.utils.ansies;
import uwvm2.uwvm.utils.memory;
import uwvm2.uwvm.cmdline;
import uwvm2.uwvm.wasm.base;
import uwvm2.uwvm.wasm.type;
import uwvm2.uwvm.wasm.storage;
import uwvm2.uwvm.wasm.feature;
import uwvm2.uwvm.wasm.custom;
import :warn_storage;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "element_section.h"
