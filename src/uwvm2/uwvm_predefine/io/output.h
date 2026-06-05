/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        output.h
 * @brief       Process-wide diagnostic output stream for UWVM.
 * @details     This header defines `u8log_output`, the UTF-8 stream used for user-facing UWVM diagnostics.  It receives
 *              command-line usage text, errors, warnings, fatal messages, verbose records, and similar runtime messages.
 *              Runtime compiler logs are intentionally routed through `u8runtime_log_output` instead.
 *
 *              The stream is configured by `--log-output`.  Hosted targets default to a duplicate of stderr so the
 *              diagnostic stream owns a stable output handle.  Targets without descriptor duplication or regular file
 *              support use standard-output/standard-error observers.  All variants are wrapped in
 *              `fast_io::basic_io_lockable_nonmovable` to support serialized writes from concurrent UWVM code paths.
 *
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
    /// @brief Process-wide UTF-8 diagnostic output stream.
    /// @details `u8log_output` is the sink for normal UWVM diagnostics and verbose messages.  It can be rebound by the
    ///          `--log-output` command-line option to stdout, stderr, or a file on hosted targets.  The lockable wrapper
    ///          allows callers to take a stream lock and emit multi-part diagnostic records without interleaving output
    ///          from other threads.
    /// @see u8runtime_log_output
    /// @see ::uwvm2::uwvm::cmdline::params::log_output
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_output_callback
# if defined(__AVR__)
    // AVR targets use C stderr because POSIX/native descriptor facilities are unavailable.
    inline ::fast_io::basic_io_lockable_nonmovable<::fast_io::u8c_io_observer> u8log_output{
        ::fast_io::u8c_stderr()};  // [global] No global variable dependencies from other translation units
# elif ((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) || (defined(__MSDOS__) || defined(__DJGPP__)) ||                                   \
     (defined(__NEWLIB__) && !defined(__CYGWIN__)) || defined(_PICOLIBC__) || defined(__wasm__)
    // These constrained targets cannot reliably duplicate stderr, so keep a native stderr observer.
    // win9x cannot dup stderr
    inline ::fast_io::basic_io_lockable_nonmovable<::fast_io::u8native_io_observer> u8log_output{
        ::fast_io::u8err()};  // [global] No global variable dependencies from other translation units
# else
    // Hosted targets own a duplicate of stderr until `--log-output` reopens the stream.
    inline ::fast_io::basic_io_lockable_nonmovable<::fast_io::u8native_file> u8log_output{
        ::fast_io::io_dup,
        ::fast_io::u8err()};  // [global] No global variable dependencies from other translation units
# endif

}  // namespace uwvm2::uwvm::io
#endif
