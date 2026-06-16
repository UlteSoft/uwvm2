/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        impl.h
 * @brief       Umbrella include for UWVM diagnostic color policy helpers.
 * @details     This header aggregates the ANSI/color predefine utilities for non-module builds.  At present it exposes
 *              the `NO_COLOR` detection and color-output control state from `no_color.h`; the paired macro headers are
 *              included explicitly at use sites because their `push_macro`/`pop_macro` ordering must remain local.
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

#pragma once

#ifndef UWVM_MODULE
# include "no_color.h"

#endif
