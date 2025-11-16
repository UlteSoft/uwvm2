/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       Imported local dynamic libraries
 * @details     "--wasm-load-dl" or "-Wld"
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-28
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

// macro
#include <uwvm2/utils/macro/push_macros.h>

export module uwvm2.uwvm.wasm.storage:weak_symbol;

import fast_io;
import uwvm2.utils.container;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard.wasm1.type;
import uwvm2.uwvm.wasm.base;
import uwvm2.uwvm.wasm.feature;
import uwvm2.uwvm.wasm.type;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "weak_symbol.h"
