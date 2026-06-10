/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        no_color.h
 * @brief       NO_COLOR detection and process-wide color-output controls for UWVM diagnostics.
 * @details     This header defines the color policy used by UWVM diagnostic output.  `check_has_no_color()` detects the
 *              presence of the `NO_COLOR` environment variable without relying on C++ library state that may not be
 *              available in all target environments.  `put_color` caches the inverse result and is used by diagnostic
 *              formatting code to decide whether ANSI/color manipulators should be emitted.
 *
 *              On old Windows targets, UWVM may need to choose between ANSI escape sequences and Win32 console text
 *              attributes at runtime.  `log_win32_use_ansi_b` records that choice for the color macro layer.
 *
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-15
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
// std
# include <cstdint>
# include <cstddef>
# include <concepts>
# include <cstdlib>
# include <memory>
// import
# include <fast_io.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#ifdef UWVM
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::utils::ansies
{
    namespace details::posix
    {
# if !(defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__))
        /// @brief Direct C-library `getenv` entry point used for POSIX-style `NO_COLOR` probing.
        /// @details The declaration binds to the platform symbol explicitly so the diagnostic color switch can be
        ///          queried in low-level startup paths without depending on higher-level C++ wrappers.
#  if defined(__DARWIN_C_LEVEL) || defined(__DJGPP__)
        extern char* libc_getenv(char const*) noexcept __asm__("_getenv");
#  else
        extern char* libc_getenv(char const*) noexcept __asm__("getenv");
#  endif
# endif
    }  // namespace details::posix

    /// @brief Checks whether the process environment contains `NO_COLOR`.
    /// @details The function implements platform-specific environment lookup for the color policy:
    ///
    ///          - Windows NT-family builds query the process environment through `RtlQueryEnvironmentVariable_U`.
    ///          - Windows 9x builds use `GetEnvironmentVariableA`.
    ///          - POSIX-style builds call the bound C-library `getenv` symbol.
    ///
    ///          Only the presence of `NO_COLOR` matters; its value is intentionally ignored.
    /// @return `true` when `NO_COLOR` is present, otherwise `false`.
    /// @see put_color
    inline constexpr bool check_has_no_color() noexcept
    {
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__)
#  ifndef _WIN32_WINDOWS
        auto const curr_peb{::fast_io::win32::nt::nt_get_current_peb()};
        constexpr decltype(auto) env_str{u"NO_COLOR"};
        ::fast_io::win32::nt::unicode_string env_us{.Length = sizeof(env_str) - sizeof(char16_t),
                                                    .MaximumLength = sizeof(env_str),
                                                    .Buffer = const_cast<char16_t*>(env_str)};
        ::fast_io::win32::nt::unicode_string out_us{};
        auto const status{
            ::fast_io::win32::nt::RtlQueryEnvironmentVariable_U(curr_peb->ProcessParameters->Environment, ::std::addressof(env_us), ::std::addressof(out_us))};
        return status == 0xC000'0023u || status == 0x0000'0000u;
#  else
        auto const no_color_env{::fast_io::win32::GetEnvironmentVariableA(reinterpret_cast<char const*>(u8"NO_COLOR"), nullptr, 0)};
        return static_cast<bool>(no_color_env);
#  endif
# else
        auto const no_color_env{details::posix::libc_getenv(reinterpret_cast<char const*>(u8"NO_COLOR"))};
        return static_cast<bool>(no_color_env);
# endif
    }

    /// @brief Process-wide switch controlling whether UWVM diagnostics emit color sequences.
    /// @details The flag is initialized once from `check_has_no_color()`.  Diagnostic sites wrap color tokens with
    ///          `fast_io::mnp::cond(put_color, ...)`, so setting `NO_COLOR` disables color output while preserving the
    ///          same message text.
    /// @see check_has_no_color
    /// @see UWVM_COLOR_U8_RST_ALL
    inline bool const put_color{!check_has_no_color()};  // [global] No global variable dependencies from other translation units

# if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
    /// @brief Selects ANSI escape output instead of Win32 text attributes on old Windows console targets.
    /// @details This flag is used by the `UWVM_COLOR_*` macro layer when both ANSI escape sequences and Win32 console
    ///          text attributes are viable formatting mechanisms.  It is disabled by default and can be enabled by
    ///          console setup code after detecting ANSI support.
    /// @see UWVM_COLOR_RST_ALL
    /// @see uwvm_color_push_macro.h
    inline bool log_win32_use_ansi_b{false};  // [global] No global variable dependencies from other translation units
# endif
}  // namespace uwvm2::uwvm::utils::ansies
#endif
