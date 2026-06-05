/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        no_color.cppm
 * @brief       C++20 module partition for UWVM `NO_COLOR` detection and color-output state.
 * @details     This partition exports `uwvm2.uwvm_predefine.utils.ansies:no_color` and includes `no_color.h` after
 *              setting the module-build macros.  The exported declarations provide the cached color-output flag and
 *              old-Windows console color backend selector used by diagnostic formatting code.
 *
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-15
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
#include <concepts>
#include <cstdlib>
#include <memory>

export module uwvm2.uwvm_predefine.utils.ansies:no_color;

import fast_io;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "no_color.h"
