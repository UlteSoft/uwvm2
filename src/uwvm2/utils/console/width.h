/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-06-30
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
# include <cstring>
# include <concepts>
# include <memory>
// platform
# if (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32) && !defined(_PICOLIBC__) && !(defined(__MSDOS__) || defined(__DJGPP__)) &&              \
     !defined(__wasm__)
#  include <unistd.h>
#  include <sys/ioctl.h>
# endif
// import
# include <fast_io.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::console
{
    inline constexpr ::std::size_t get_console_width() noexcept
    {
#if defined(_WIN32)
        auto hStdout{::fast_io::win32::GetStdHandle(::fast_io::win32_stdout_number)};
        if(hStdout == nullptr) [[unlikely]] { return 0uz; }

        ::fast_io::win32::console_screen_buffer_info csbi;
        if(::fast_io::win32::GetConsoleScreenBufferInfo(hStdout, ::std::addressof(csbi))) [[likely]]
        {
            return static_cast<::std::size_t>(csbi.Window.Right - csbi.Window.Left);
        }

        return 0uz;

#elif (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32) && !defined(_PICOLIBC__) && !(defined(__MSDOS__) || defined(__DJGPP__)) &&             \
    !defined(__wasm__)
        struct winsize size;
# if defined(__linux__) && defined(__NR_ioctl)
        if(::fast_io::system_call<__NR_ioctl, int>(::fast_io::posix_stdout_number, TIOCGWINSZ, ::std::addressof(size)) != 0) [[unlikely]] { return 0uz; }
# else
        if(::fast_io::posix::libc_ioctl(::fast_io::posix_stdout_number, TIOCGWINSZ, ::std::addressof(size)) != 0) [[unlikely]] { return 0uz; }
# endif
        return static_cast<::std::size_t>(size.ws_col);

#else
        return 0uz;

#endif
    }
}
