/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        runtime_log.h
 * @brief       Runtime compiler logging state and output routing for UWVM.
 * @details     This header defines the process-wide switch and output stream used by runtime compiler log sites.
 *              Runtime compiler logs are separate from normal UWVM diagnostics: `u8log_output` receives user-facing
 *              errors, warnings, help text, and verbose diagnostics, while `u8runtime_log_output` receives compiler
 *              backend records when `enable_runtime_log` is set by `--runtime-compiler-log`.
 *
 *              Hosted targets can route runtime compiler logs to stdout, stderr, or a dedicated file.  Constrained
 *              targets that cannot duplicate standard streams or open regular files fall back to stdout/stderr observer
 *              streams.  The stream object is lockable so concurrent compiler workers can serialize their log writes.
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
    /// @brief Identifies the selected runtime compiler log sink.
    /// @details The value mirrors the target accepted by `--runtime-compiler-log` and is updated together with
    ///          `u8runtime_log_output` when command-line parsing reopens the stream.
    /// @see runtime_log_output_target
    /// @see ::uwvm2::uwvm::cmdline::params::details::runtime_compiler_log_callback
    enum class runtime_log_output_target_t : unsigned
    {
        file = 0u,  ///< Dedicated log file target selected by `--runtime-compiler-log file <path>`.
        out,        ///< Standard-output log target selected by `--runtime-compiler-log out`.
        err         ///< Standard-error log target selected by `--runtime-compiler-log err`.
    };

    /// @brief Enables runtime compiler log emission.
    /// @details This flag is set by the `--runtime-compiler-log` command-line parameter.  Runtime backends check it
    ///          before printing lazy/full compilation records, tiered compilation summaries, and related compiler
    ///          counters.  It does not affect general verbose diagnostics controlled by `show_verbose`.
    /// @see runtime_log_output_target
    /// @see u8runtime_log_output
    /// @see ::uwvm2::uwvm::cmdline::params::runtime_compiler_log
    inline bool enable_runtime_log{};  // [global]

    /// @brief Stores the currently selected runtime compiler log target.
    /// @details The initial value reflects the preferred platform target: hosted targets default to `file` so command
    ///          parsing can open the requested path, while constrained targets default to `err` because regular files
    ///          may be unavailable.  The callback for `--runtime-compiler-log` keeps this state synchronized with
    ///          `u8runtime_log_output`.
    /// @see runtime_log_output_target_t
    /// @see u8runtime_log_output
    /// @see ::uwvm2::uwvm::cmdline::params::details::runtime_compiler_log_callback
    inline runtime_log_output_target_t runtime_log_output_target{
# if !defined(__AVR__) && !((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) && !(defined(__MSDOS__) || defined(__DJGPP__)) &&              \
     !(defined(__NEWLIB__) && !defined(__CYGWIN__)) && !defined(_PICOLIBC__) && !defined(__wasm__)
        runtime_log_output_target_t::file
# else
        runtime_log_output_target_t::err
# endif
    };  // [global]

    /// @brief Process-wide output stream for runtime compiler log records.
    /// @details The stream is written only by runtime compiler log sites and only when `enable_runtime_log` is true.
    ///          Hosted builds use an owning `u8native_file`, which is opened or rebound by the runtime compiler log
    ///          command-line callback.  Targets without the required file or descriptor support use UTF-8 stdout/stderr
    ///          observers instead.  The lockable wrapper is required because JIT and tiered compilation can emit records
    ///          from multiple worker threads.
    /// @see enable_runtime_log
    /// @see runtime_log_output_target
    /// @see ::uwvm2::uwvm::cmdline::params::details::runtime_compiler_log_callback
# if defined(__AVR__)
    // AVR targets use C stderr because POSIX/native descriptor facilities are unavailable.
    inline ::fast_io::basic_io_lockable_nonmovable<::fast_io::u8c_io_observer> u8runtime_log_output{
        ::fast_io::u8c_stderr()};  // [global] No global variable dependencies from other translation units
# elif ((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) || (defined(__MSDOS__) || defined(__DJGPP__)) ||                                   \
     (defined(__NEWLIB__) && !defined(__CYGWIN__)) || defined(_PICOLIBC__) || defined(__wasm__)
    // These constrained targets cannot reliably duplicate stderr or create regular native files.
    // win9x cannot dup stderr
    inline ::fast_io::basic_io_lockable_nonmovable<::fast_io::u8native_io_observer> u8runtime_log_output{
        ::fast_io::u8err()};  // [global] No global variable dependencies from other translation units
# else
    // Hosted targets reopen this file object after command-line parsing chooses the concrete sink.
    inline ::fast_io::basic_io_lockable_nonmovable<::fast_io::u8native_file>
        u8runtime_log_output{};  // [global] No global variable dependencies from other translation units
# endif
}  // namespace uwvm2::uwvm::io
#endif
