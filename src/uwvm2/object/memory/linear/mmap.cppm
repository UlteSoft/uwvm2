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
#include <limits>
#include <memory>
#include <new>
#include <atomic>
#include <bit>
#include <utility>
// macro
#include <uwvm2/utils/macro/push_macros.h>
// platform
#if defined(UWVM_SUPPORT_MMAP) && !(defined(_WIN32) || defined(__CYGWIN__))  // posix mmap
# include <sys/mman.h>
#endif

export module uwvm2.object.memory.linear:mmap;

import fast_io;
import uwvm2.utils.container;
import uwvm2.utils.debug;
import uwvm2.utils.mutex;
import uwvm2.object.memory.wasm_page;
import uwvm2.object.memory.platform_page;
import uwvm2.object.memory.signal;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "mmap.h"
