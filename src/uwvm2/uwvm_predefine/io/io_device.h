/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        io_device.h
 * @brief       Legacy low-level standard-device declarations for the UWVM I/O predefine layer.
 * @details     This header is retained as the `io_device` partition surface for compatibility with the existing
 *              `uwvm_predefine.io` module layout.  The old direct standard-device observers (`u8in`, `u8out`, and
 *              `u8err`) are preserved only in a disabled compatibility block.  Current diagnostic code should use
 *              `u8log_output` from `output.h`, and runtime compiler logging should use `u8runtime_log_output` from
 *              `runtime_log.h`.
 *
 * @author      MacroModel
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
    // The `io` namespace intentionally owns the stream objects directly; no extra implementation namespace is needed.
    /// @deprecated Direct standard-device globals are superseded by `u8log_output` and `u8runtime_log_output`.
    /// @details The block remains disabled because the modern logging layer centralizes synchronization and
    ///          command-line output routing in the dedicated log streams.
# if 0
#  ifndef __AVR__

    /// @brief Native UTF-8 standard-device observers for non-AVR targets.
    /// @details On POSIX-like systems these observers refer to file descriptors 0, 1, and 2.  On Win32-family targets
    ///          they refer to the process standard handles.  The declarations are kept only as historical context.
    ///             on non-windows (POSIX) systems (fd): (int)[0, 1, 2]
    ///             on win9x-win32 (handle): (void*) GetStdHandle([STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE])
    ///             on nt (handle): (void*) RtlGetCurrentPeb()->ProcessParameters->Standard[Input, Output, Error]
    ///             set in out err __init_priority__ to 250, set xxx_buf __init_priority__ to 260


    inline ::fast_io::u8native_io_observer u8in{::fast_io::u8in()};                  // [global] No global variable dependencies from other translation units
    inline ::fast_io::u8native_io_observer u8out{::fast_io::u8out()};                // [global] No global variable dependencies from other translation units
    inline ::fast_io::u8native_io_observer u8err{::fast_io::u8err()};                // [global] No global variable dependencies from other translation units
    // No buffer is provided to u8err
#  else
    /// @brief C UTF-8 standard-device observers for AVR targets.
    /// @details AVR targets use the C standard streams exposed by avr-libc, which do not provide the native descriptor
    ///          facilities used by hosted UWVM builds.  The declarations are kept only as historical context.

    inline ::fast_io::u8c_io_observer u8in{::fast_io::u8c_stdin()};             // [global] No global variable dependencies from other translation units
    inline ::fast_io::u8c_io_observer u8out{::fast_io::u8c_stdout()};           // [global] No global variable dependencies from other translation units
    inline ::fast_io::u8c_io_observer u8err{::fast_io::u8c_stderr()};           // [global] No global variable dependencies from other translation units
    // No buffer is provided to u8err
#  endif
# endif
}  // namespace uwvm2::uwvm::io
#endif
