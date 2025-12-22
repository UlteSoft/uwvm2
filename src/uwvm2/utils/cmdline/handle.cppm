/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-25
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
#include <memory>
#include <new>
#include <algorithm>
#include <limits>
#include <utility>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/imported/wasi/feature/feature_push_macro.h>  // wasi

export module uwvm2.utils.cmdline:handle;

import fast_io;
import fast_io_crypto;
import uwvm2.utils.container;
import uwvm2.utils.hash;
import uwvm2.utils.debug;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "handle.h"
