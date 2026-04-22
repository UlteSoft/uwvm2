/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
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
#  if defined(__DARWIN_C_LEVEL) || defined(__DJGPP__)
        extern char* libc_getenv(char const*) noexcept __asm__("_getenv");
#  else
        extern char* libc_getenv(char const*) noexcept __asm__("getenv");
#  endif
# endif
    }  // namespace details::posix

    /// @brief Determine if there is NO_COLOR by environment variable
    inline bool check_has_no_color() noexcept
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

    inline bool const put_color{!check_has_no_color()};  // [global] No global variable dependencies from other translation units

# if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
    /// @brief Used to determine whether or not to output colors via win32_text_attr
    inline bool log_win32_use_ansi_b{false};  // [global] No global variable dependencies from other translation units
# endif
}  // namespace uwvm2::utils::ansies
#endif
