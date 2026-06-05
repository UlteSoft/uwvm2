/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        verbose_control.cppm
 * @brief       C++20 module partition for the UWVM verbose diagnostic switch.
 * @details     This partition exports `uwvm2.uwvm_predefine.io:verbose_control` and includes `verbose_control.h` after
 *              setting the module-build macros.  The exported flag gates detailed progress records emitted by loader,
 *              runtime, WASI, validation, and compiler paths.
 *
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-07-07
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

export module uwvm2.uwvm_predefine.io:verbose_control;

import fast_io;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "verbose_control.h"
