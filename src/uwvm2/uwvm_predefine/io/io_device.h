/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       Set up the lowest-level implementation of uwvm.
 *              Use the C API in the minimal C host environment and the user-space lowest-level API in other environments.
 *              Automatically add file-form buffers to out and in.
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
    // io does not add meaningless namespaces.
    /// @deprecated It is no longer recommended to use u8err, use u8log_output.
# if 0
#  ifndef __AVR__

    /// @brief      native stdout
    /// @details    on non-windows (POSIX) systems (fd): (int)[0, 1, 2]
    ///             on win9x-win32 (handle): (void*) GetStdHandle([STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE])
    ///             on nt (handle): (void*) RtlGetCurrentPeb()->ProcessParameters->Standard[Input, Output, Error]
    ///             set in out err __init_priority__ to 250, set xxx_buf __init_priority__ to 260

    inline ::fast_io::u8native_io_observer u8in{::fast_io::u8in()};                  // [global] No global variable dependencies from other translation units
    inline ::fast_io::u8native_io_observer u8out{::fast_io::u8out()};                // [global] No global variable dependencies from other translation units
    inline ::fast_io::u8native_io_observer u8err{::fast_io::u8err()};                // [global] No global variable dependencies from other translation units
    // No buffer is provided to u8err
#  else
    /// @brief      C's STDOUT
    /// @details    The C API of avrlibc does not have any buffers.

    inline ::fast_io::u8c_io_observer u8in{::fast_io::u8c_stdin()};             // [global] No global variable dependencies from other translation units
    inline ::fast_io::u8c_io_observer u8out{::fast_io::u8c_stdout()};           // [global] No global variable dependencies from other translation units
    inline ::fast_io::u8c_io_observer u8err{::fast_io::u8c_stderr()};           // [global] No global variable dependencies from other translation units
    // No buffer is provided to u8err
#  endif
# endif
}  // namespace uwvm2::uwvm
#endif
