/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-27
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
#include <type_traits>
#include <concepts>
#include <utility>
#include <limits>
// macro
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/utils/ansies/ansi_push_macro.h>
#include <uwvm2/utils/ansies/win32_text_attr_push_macro.h>

export module uwvm2.validation.error:error_code_output;

import fast_io;
import uwvm2.utils.ansies;
import uwvm2.utils.utf;
import uwvm2.utils.debug;
import :error;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "error_code_output.h"
