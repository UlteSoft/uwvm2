/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        impl.cppm
 * @brief       Primary C++20 module interface for UWVM diagnostic color policy helpers.
 * @details     This module exports `uwvm2.uwvm_predefine.utils.ansies` and re-exports the `no_color` partition.  After
 *              defining module-build macros, it includes the umbrella header so module and non-module builds share the
 *              same color-control declarations.
 *
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-24
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

export module uwvm2.uwvm_predefine.utils.ansies;
export import :no_color;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "impl.h"
