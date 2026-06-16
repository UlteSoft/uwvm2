/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        verbose_control.h
 * @brief       Global verbose diagnostic switch for UWVM.
 * @details     This header defines the process-wide flag used by UWVM verbose logging sites.  The flag is set by the
 *              `--log-verbose` command-line parameter and gates detailed progress records from the loader, WASI setup,
 *              runtime initialization, validation, compiler scheduling, and other diagnostic paths.
 *
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

#pragma once

#ifndef UWVM_MODULE
// import
# include <fast_io.h>
# include <fast_io_device.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#ifdef UWVM
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::io
{
    /// @brief Controls emission of verbose UWVM diagnostics.
    /// @details When enabled, UWVM emits detailed progress and state messages to `u8log_output`.  Verbose records are
    ///          advisory diagnostics rather than warning/fatal controls, and many of them include timestamps from
    ///          `get_local_realtime()`.  This flag is disabled by default and is set through `--log-verbose`.
    /// @see u8log_output
    /// @see get_local_realtime
    /// @see ::uwvm2::uwvm::cmdline::params::log_verbose
    inline bool show_verbose{};  // [global] No global variable dependencies from other translation units
}  // namespace uwvm2::uwvm::io
#endif
