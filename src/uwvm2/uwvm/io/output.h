/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-16
 * @copyright   APL-2 License
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

#ifdef UWVM_MODULE
// import
import fast_io;
#else
// import
# include <fast_io.h>
# include <fast_io_device.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm
{
    /// @brief      Control VM output via virtual functions, can be set via option in `--log-output`, not supported by avr
    /// @details    multi-threaded security
#if defined(__AVR__)
    // avr does not have posix
    inline ::fast_io::basic_io_lockable_nonmovable<::fast_io::u8c_io_observer> u8log_output{
        ::fast_io::u8c_stderr()};  // [global] No global variable dependencies from other translation units
#elif ((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) || (defined(__MSDOS__) || defined(__DJGPP__)) ||                                    \
    (defined(__NEWLIB__) && !defined(__CYGWIN__))
    // win9x cannot dup stderr
    inline ::fast_io::basic_io_lockable_nonmovable<::fast_io::u8native_io_observer> u8log_output{
        ::fast_io::u8err()};  // [global] No global variable dependencies from other translation units
#else
    inline ::fast_io::basic_io_lockable_nonmovable<::fast_io::u8native_file> u8log_output{
        ::fast_io::io_dup,
        ::fast_io::u8err()};  // [global] No global variable dependencies from other translation units
#endif

}  // namespace uwvm2::utils
