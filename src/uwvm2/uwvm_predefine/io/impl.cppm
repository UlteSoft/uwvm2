/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        impl.cppm
 * @brief       Primary C++20 module interface for UWVM predefined I/O facilities.
 * @details     This module exports the `uwvm2.uwvm_predefine.io` module and re-exports all I/O partitions used by UWVM:
 *              standard-device compatibility, diagnostic output routing, runtime compiler logging, warning controls,
 *              verbose controls, and diagnostic timestamp helpers.  After defining module-build macros, it includes the
 *              umbrella header so module and non-module builds share the same declarations.
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

module;

export module uwvm2.uwvm_predefine.io;
export import :io_device;
export import :output;
export import :runtime_log;
export import :warn_control;
export import :verbose_control;
export import :time;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "impl.h"
