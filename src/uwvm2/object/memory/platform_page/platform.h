/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
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
// std
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// platform
# if defined(UWVM_SUPPORT_MMAP) && !(defined(_WIN32) || defined(__CYGWIN__))  // posix mmap
#  include <unistd.h>
# endif
// import
# include <fast_io.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::object::memory::platform_page
{
    struct get_platform_page_size_result_t
    {
        ::std::size_t page_size;
        bool success;
    };

    UWVM_GNU_PURE inline get_platform_page_size_result_t get_platform_page_size() noexcept
    {
#if defined(_WIN32) || defined(__CYGWIN__)                                                          // Windows
# if !defined(__CYGWIN__) && !defined(__WINE__) && !defined(__BIONIC__) && defined(_WIN32_WINDOWS)  // WIN32

        ::fast_io::win32::system_info si;  // No initlization is required

        ::fast_io::win32::GetSystemInfo(::std::addressof(si));

        return {static_cast<::std::size_t>(si.dwPageSize), true};

# else  // NT

        ::fast_io::win32::nt::system_basic_information si;  // No initlization is required

        constexpr bool zw{false};
        auto const status{::fast_io::win32::nt::nt_query_system_information<zw>(::fast_io::win32::nt::system_information_class::SystemBasicInformation,
                                                                                ::std::addressof(si),
                                                                                sizeof(si),
                                                                                nullptr)};
        if(status) [[unlikely]]
        {
            // Systems running in protected mode (with paging support) crash immediately upon a failed call.
            return {0uz, false};
        }

        return {static_cast<::std::size_t>(si.PageSize), true};

# endif
#elif !defined(__NEWLIB__) && !(defined(__MSDOS__) || defined(__DJGPP__)) && (!defined(__wasm__) || (defined(__wasi__) && defined(_WASI_EMULATED_MMAN))) &&    \
    __has_include(<sys/mman.h>)  // POSIX

        int const preferred_virtual_page_size{::fast_io::noexcept_call(::getpagesize)};

        // getpagesize() never fails
        // Guarantees a positive integer return value under POSIX semantics
        [[assume(preferred_virtual_page_size > 0)]];

        // Do not use sysconf(_SC_PAGESIZE) because it may set errno (though it will almost certainly succeed in most cases), violating gnu::pure semantics.
        return {static_cast<::std::size_t>(preferred_virtual_page_size), true};

#else  // None (e.g. i586-msdosdjgpp)

        return {0uz, true};

#endif
    }

}  // namespace uwvm2::object::memory::wasm_page

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
