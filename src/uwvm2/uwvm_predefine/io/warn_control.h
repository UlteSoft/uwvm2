/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        warn_control.h
 * @brief       Global warning visibility and warning-to-fatal controls for UWVM.
 * @details     This header defines the process-wide switches used by UWVM warning sites.  Every warning category is
 *              represented by two related flags:
 *
 *              - `show_*_warning`: whether warnings in that category are emitted.
 *              - `*_warning_fatal`: whether an emitted warning in that category is converted to a fatal error.
 *
 *              Command-line handling maps category names such as `vm`, `parser`, `depend`, and
 *              `runtime-compile-threads` onto these variables.  Most categories are enabled by default and can be
 *              disabled through `--log-disable-warning`.  Noisy advisory categories, currently `parser` and `wasip1`,
 *              are disabled by default and can be enabled through `--log-enable-warning`.  Fatal conversion is controlled
 *              separately by `--log-convert-warn-to-fatal`.
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
// macro
# include <uwvm2/utils/macro/push_macros.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#ifdef UWVM
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::io
{
    /// @brief Controls emission of generic VM infrastructure warnings.
    /// @details Category: `vm`.  Covers UWVM host/process infrastructure diagnostics that are not tied to a more
    ///          specific subsystem, such as timer failures, install-path lookup failures, console/ANSI setup issues,
    ///          and similar VM-level advisory messages.  Enabled by default.
    /// @see vm_warning_fatal
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_disable_warning_callback
    inline bool show_vm_warning{true};  // [global]

    /// @brief Controls emission of parser-level WebAssembly advisory warnings.
    /// @details Category: `parser`.  Covers valid-but-suspicious wasm structure diagnostics produced by the wasm warning
    ///          pass, including duplicate function types, unused types, and section-level import/export/memory
    ///          advisories.  Disabled by default because these checks can be very noisy.  Use
    ///          `--log-enable-warning parser` to enable this category.
    /// @see parser_warning_fatal
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_enable_warning_callback
    /// @see ::uwvm2::uwvm::wasm::warning::show_wasm_binfmt_ver1_warning
    inline bool show_parser_warning{false};  // [global]

    /// @brief Controls emission of the native-code trust warning for preloaded dynamic libraries.
    /// @details Category: `untrusted-dl`.  Covers the one-time security warning that a preloaded native dynamic library
    ///          runs in the UWVM process and may inspect or mutate process memory.  Enabled by default.
    /// @see untrusted_dl_warning_fatal
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_disable_warning_callback
    /// @see ::uwvm2::uwvm::wasm::loader::load_dl
    inline bool show_untrusted_dl_warning{true};  // [global]

# ifdef UWVM_SUPPORT_PRELOAD_DL
    /// @brief Controls emission of preload-DL metadata and ABI warnings.
    /// @details Category: `dl`.  Covers recoverable diagnostics while loading UWVM preloaded dynamic-library modules,
    ///          such as inconsistent module-name metadata and invalid callback-provided metadata.  Enabled by default
    ///          when preload-DL support is compiled in.
    /// @see dl_warning_fatal
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_disable_warning_callback
    /// @see ::uwvm2::uwvm::wasm::loader::load_dl
    inline bool show_dl_warning{true};  // [global]
# endif

# ifdef UWVM_SUPPORT_WEAK_SYMBOL
    /// @brief Controls emission of weak-symbol module warnings.
    /// @details Category: `weak-symbol`.  Covers weak-symbol table diagnostics such as malformed entries, invalid UTF-8
    ///          function names, duplicate weak function names, and null weak function pointers.  These issues are
    ///          generally skipped as recoverable weak-symbol entries rather than treated as hard module-load failures.
    ///          Enabled by default when weak-symbol support is compiled in.
    /// @see weak_symbol_warning_fatal
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_disable_warning_callback
    /// @see ::uwvm2::uwvm::wasm::loader::load_weak_symbol
    inline bool show_weak_symbol_warning{true};  // [global]
# endif

    /// @brief Controls emission of module dependency-analysis warnings.
    /// @details Category: `depend`.  Covers dependency graph size/recursion-limit warnings and detected module
    ///          dependency cycles.  Import-existence failures are still hard loader errors; this flag controls the
    ///          warning-level dependency diagnostics and optional cycle-reporting path.  Enabled by default.
    /// @see depend_warning_fatal
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_disable_warning_callback
    /// @see ::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles
    inline bool show_depend_warning{true};  // [global]

    /// @brief Controls emission of WASI Preview 1 semantic warnings.
    /// @details Category: `wasip1`.  Covers WASI Preview 1 advisories such as out-of-range `proc_exit` exit codes.
    ///          Disabled by default; use `--log-enable-warning wasip1` to enable this category.
    /// @see wasip1_warning_fatal
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_enable_warning_callback
    /// @see ::uwvm2::uwvm::imported::wasi::wasip1::uwvm_wasip1_proc_exit_func_ptr_overload
    inline bool show_wasip1_warning{};  // [global]

# if defined(_WIN32) && !defined(_WIN32_WINDOWS)
    /// @brief Controls emission of Windows NT native-path warnings.
    /// @details Category: `nt-path`.  Covers diagnostics emitted when a path resolves to the internal `::NT::` namespace
    ///          on Windows NT-family builds.  Enabled by default on the matching platform.
    /// @see nt_path_warning_fatal
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_disable_warning_callback
    /// @see ::uwvm2::uwvm::imported::wasi::wasip1::details::path_u8str_to_file
    inline bool show_nt_path_warning{true};  // [global]
# endif

# if defined(_WIN32) && defined(_WIN32_WINDOWS)
    /// @brief Controls emission of Windows 9x TOCTOU sandbox warnings.
    /// @details Category: `toctou`.  Covers warnings that directory operations on Windows 9x can be time-of-check to
    ///          time-of-use sensitive because that platform lacks the directory-handle semantics used by safer targets.
    ///          Enabled by default on the matching platform.
    /// @see toctou_warning_fatal
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_disable_warning_callback
    /// @see ::uwvm2::uwvm::cmdline::params::details::wasip1_global_mount_dir_callback
    inline bool show_toctou_warning{true};  // [global]
# endif

    /// @brief Controls emission of general runtime initialization and backend warnings.
    /// @details Category: `runtime`.  Covers runtime-stage advisories such as memory-limit overrides that widen declared
    ///          limits, debug-interpreter mode normalization, and LLVM JIT call-stack unwind fallback.  Compile-thread
    ///          policy warnings are controlled by `show_runtime_compile_threads_warning` instead.  Enabled by default.
    /// @see runtime_warning_fatal
    /// @see show_runtime_compile_threads_warning
    /// @see ::uwvm2::uwvm::runtime::initializer::details::emit_runtime_memory_limit_widen_warning
    inline bool show_runtime_warning{true};  // [global]

    /// @brief Controls emission of runtime compile-thread policy and scheduling warnings.
    /// @details Category: `runtime-compile-threads`.  Covers warnings for unsupported compile-thread requests,
    ///          native-thread-unavailable fallbacks, requested counts above detected concurrency, and adaptive
    ///          full-translation scheduling adjustments.  Kept separate from `runtime` so performance-policy warnings can
    ///          be tuned independently.  Enabled by default.
    /// @see runtime_compile_threads_warning_fatal
    /// @see show_runtime_warning
    /// @see ::uwvm2::uwvm::run::resolve_runtime_compile_threads
    inline bool show_runtime_compile_threads_warning{true};  // [global]

    /// @brief Converts emitted `vm` warnings into fatal errors.
    /// @details Fatal control paired with `show_vm_warning`.  When this flag is set, a `vm` warning site that emits a
    ///          warning also emits the standard warning-to-fatal message and terminates.
    /// @see show_vm_warning
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_convert_warn_to_fatal_callback
    inline bool vm_warning_fatal{};  // [global]

    /// @brief Converts emitted `parser` warnings into fatal errors.
    /// @details Fatal control paired with `show_parser_warning`.  Setting this flag does not enable parser warnings by
    ///          itself; the parser warning category must still be emitted by its normal warning path.
    /// @see show_parser_warning
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_convert_warn_to_fatal_callback
    /// @see ::uwvm2::uwvm::wasm::warning::show_wasm_binfmt_ver1_warning
    inline bool parser_warning_fatal{};  // [global]

    /// @brief Converts emitted `untrusted-dl` warnings into fatal errors.
    /// @details Fatal control paired with `show_untrusted_dl_warning` for the native-code trust warning.
    /// @see show_untrusted_dl_warning
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_convert_warn_to_fatal_callback
    /// @see ::uwvm2::uwvm::wasm::loader::load_dl
    inline bool untrusted_dl_warning_fatal{};  // [global]

# ifdef UWVM_SUPPORT_PRELOAD_DL
    /// @brief Converts emitted `dl` warnings into fatal errors.
    /// @details Fatal control paired with `show_dl_warning` for preload-DL metadata and ABI diagnostics.
    /// @see show_dl_warning
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_convert_warn_to_fatal_callback
    /// @see ::uwvm2::uwvm::wasm::loader::load_dl
    inline bool dl_warning_fatal{};  // [global]
# endif

# ifdef UWVM_SUPPORT_WEAK_SYMBOL
    /// @brief Converts emitted `weak-symbol` warnings into fatal errors.
    /// @details Fatal control paired with `show_weak_symbol_warning` for weak-symbol parsing/loading diagnostics.
    /// @see show_weak_symbol_warning
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_convert_warn_to_fatal_callback
    /// @see ::uwvm2::uwvm::wasm::loader::load_weak_symbol
    inline bool weak_symbol_warning_fatal{};  // [global]
# endif

    /// @brief Converts emitted `depend` warnings into fatal errors.
    /// @details Fatal control paired with `show_depend_warning` for dependency-analysis warnings, including skipped
    ///          cycle checks and detected dependency cycles.
    /// @see show_depend_warning
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_convert_warn_to_fatal_callback
    /// @see ::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles
    inline bool depend_warning_fatal{};  // [global]

    /// @brief Converts emitted `wasip1` warnings into fatal errors.
    /// @details Fatal control paired with `show_wasip1_warning`.  Setting this flag does not enable WASI Preview 1
    ///          warnings by itself.
    /// @see show_wasip1_warning
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_convert_warn_to_fatal_callback
    /// @see ::uwvm2::uwvm::imported::wasi::wasip1::uwvm_wasip1_proc_exit_func_ptr_overload
    inline bool wasip1_warning_fatal{};  // [global]

# if defined(_WIN32) && !defined(_WIN32_WINDOWS)
    /// @brief Converts emitted `nt-path` warnings into fatal errors.
    /// @details Fatal control paired with `show_nt_path_warning` for Windows NT native-path diagnostics.
    /// @see show_nt_path_warning
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_convert_warn_to_fatal_callback
    /// @see ::uwvm2::uwvm::imported::wasi::wasip1::details::path_u8str_to_file
    inline bool nt_path_warning_fatal{};  // [global]
# endif

# if defined(_WIN32) && defined(_WIN32_WINDOWS)
    /// @brief Converts emitted `toctou` warnings into fatal errors.
    /// @details Fatal control paired with `show_toctou_warning` for Windows 9x TOCTOU sandbox diagnostics.
    /// @see show_toctou_warning
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_convert_warn_to_fatal_callback
    /// @see ::uwvm2::uwvm::cmdline::params::details::wasip1_global_mount_dir_callback
    inline bool toctou_warning_fatal{};  // [global]
# endif

    /// @brief Converts emitted `runtime` warnings into fatal errors.
    /// @details Fatal control paired with `show_runtime_warning`.  Compile-thread policy diagnostics use
    ///          `runtime_compile_threads_warning_fatal` instead.
    /// @see show_runtime_warning
    /// @see runtime_compile_threads_warning_fatal
    /// @see ::uwvm2::uwvm::cmdline::params::details::log_convert_warn_to_fatal_callback
    inline bool runtime_warning_fatal{};  // [global]

    /// @brief Converts emitted `runtime-compile-threads` warnings into fatal errors.
    /// @details Fatal control paired with `show_runtime_compile_threads_warning` for compile-thread policy and
    ///          full-translation scheduling diagnostics.
    /// @see show_runtime_compile_threads_warning
    /// @see runtime_warning_fatal
    /// @see ::uwvm2::uwvm::run::resolve_runtime_compile_threads
    inline bool runtime_compile_threads_warning_fatal{};  // [global]

}  // namespace uwvm2::uwvm::io
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
