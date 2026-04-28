/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-16
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
    enum class runtime_log_output_target_t : unsigned
    {
        file = 0u,
        out,
        err
    };

    /// @brief Record the runtime compiler's log
    inline bool enable_runtime_log{};  // [galobal]

    inline runtime_log_output_target_t runtime_log_output_target{
# if !defined(__AVR__) && !((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) && !(defined(__MSDOS__) || defined(__DJGPP__)) &&              \
     !(defined(__NEWLIB__) && !defined(__CYGWIN__)) && !defined(_PICOLIBC__) && !defined(__wasm__)
        runtime_log_output_target_t::file
# else
        runtime_log_output_target_t::err
# endif
    };  // [global]

# if defined(__AVR__)
    // avr does not have posix
    inline ::fast_io::basic_io_lockable_nonmovable<::fast_io::u8c_io_observer> u8runtime_log_output{
        ::fast_io::u8c_stderr()};  // [global] No global variable dependencies from other translation units
# elif ((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) || (defined(__MSDOS__) || defined(__DJGPP__)) ||                                  \
     (defined(__NEWLIB__) && !defined(__CYGWIN__)) || defined(_PICOLIBC__) || defined(__wasm__)
    // win9x cannot dup stderr
    inline ::fast_io::basic_io_lockable_nonmovable<::fast_io::u8native_io_observer> u8runtime_log_output{
        ::fast_io::u8err()};  // [global] No global variable dependencies from other translation units
# else
    inline ::fast_io::basic_io_lockable_nonmovable<::fast_io::u8native_file>
        u8runtime_log_output{};  // [global] No global variable dependencies from other translation units
# endif
}  // namespace uwvm2::utils
#endif
