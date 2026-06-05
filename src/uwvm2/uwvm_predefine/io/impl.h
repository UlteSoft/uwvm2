/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        impl.h
 * @brief       Umbrella include for UWVM predefined I/O facilities.
 * @details     This header aggregates the I/O predefine components for non-module builds.  It includes the standard
 *              device partition surface, diagnostic output routing, runtime compiler logging, warning controls, verbose
 *              controls, and timestamp helper in the same order exported by `uwvm2.uwvm_predefine.io`.
 *
 * @author      24bit-xjkp
 * @version     2.0.0
 * @date        2025-03-21
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
# include "io_device.h"
# include "output.h"
# include "runtime_log.h"
# include "warn_control.h"
# include "verbose_control.h"
# include "time.h"
#endif
