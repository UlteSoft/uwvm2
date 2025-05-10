/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-18
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
import fast_io;
#else
// std
# include <cstdint>
# include <cstddef>
# include <concepts>
# include <memory>
// sys
# if (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32) && !defined(_PICOLIBC__) && !(defined(__MSDOS__) || defined(__DJGPP__))
#  include <sys/mman.h>
# endif
// import
# include <fast_io.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::madvise
{
#if (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32) && !defined(_PICOLIBC__) && !(defined(__MSDOS__) || defined(__DJGPP__))
    namespace details::posix
    {
# if defined(__DARWIN_C_LEVEL)
        extern int posix_madvise(void* addr, ::std::size_t length, int flag) noexcept __asm__("_posix_madvise");
# else
        extern int posix_madvise(void* addr, ::std::size_t length, int flag) noexcept __asm__("posix_madvise");
# endif
    }  // namespace details::posix
#endif

    enum class madvise_flag
    {
#if defined(_WIN32) || !((!defined(__NEWLIB__) || defined(__CYGWIN__)) && !(defined(__MSDOS__) || defined(__DJGPP__)) && !defined(_PICOLIBC__))
        normal,
        random,
        sequential,
        willneed,
        dontneed,
        free,
        nosync,
        autosync,
        nocore,
        core,
        project
#else
# ifdef MADV_NORMAL
        normal = MADV_NORMAL
# else
        normal = -1
# endif
        ,
# ifdef MADV_RANDOM
        random = MADV_RANDOM
# else
        random = -1
# endif
        ,
# ifdef MADV_SEQUENTIAL
        sequential = MADV_SEQUENTIAL
# else
        sequential = -1
# endif
        ,
# ifdef MADV_WILLNEED
        willneed = MADV_WILLNEED
# else
        willneed = -1
# endif
        ,
# ifdef MADV_DONTNEED
        dontneed = MADV_DONTNEED
# else
        dontneed = -1
# endif
        ,
# ifdef MADV_FREE
        free = MADV_FREE
# else
        free = -1
# endif
        ,
# ifdef MADV_NOSYNC
        nosync = MADV_NOSYNC
# else
        nosync = -1
# endif
        ,
# ifdef MADV_AUTOSYNC
        autosync = MADV_AUTOSYNC
# else
        autosync = -1
# endif
        ,
# ifdef MADV_NOCORE
        nocore = MADV_NOCORE
# else
        nocore = -1
# endif
        ,
# ifdef MADV_CORE
        core = MADV_CORE
# else
        core = -1
# endif
        ,
# ifdef MADV_PROTECT
        project = MADV_PROTECT
# else
        project = -1
# endif
#endif
    };

    /// @brief      madvise
    /// @details    Instructs the system to mmap Instructs the system to specify memory operations for mmap
    ///             No exception is thrown because this is an instructional message, not a command.
    inline void my_madvise([[maybe_unused]] void const* addr, [[maybe_unused]] ::std::size_t length, [[maybe_unused]] madvise_flag flag) noexcept
    {
#if defined(_WIN32)
# if (!defined(_WIN32_WINNT) || _WIN32_WINNT >= 0x0A00) && !defined(_WIN32_WINDOWS)
        // win10
        switch(flag)
        {
            case madvise_flag::willneed:
            {
                // "va" will not be modified, the pass parameter requires
                void* va{const_cast<void*>(addr)};
                ::fast_io::win32::nt::memory_range_entry mre{.VirtualAddress = va, .NumberOfBytes = length};
                ::std::uint_least32_t VmInformation{0u};
                ::fast_io::win32::nt::nt_set_information_virtual_memory<false>(reinterpret_cast<void*>(static_cast<::std::ptrdiff_t>(-1)),
                                                                               ::fast_io::win32::nt::virtual_memory_information_class::VmPrefetchInformation,
                                                                               1uz,
                                                                               ::std::addressof(mre),
                                                                               ::std::addressof(VmInformation),
                                                                               sizeof(::std::uint_least32_t));
                [[fallthrough]];  // Also set the default (highest) priority
            }
            case madvise_flag::normal:
            {
                // "va" will not be modified, the pass parameter requires
                void* va{const_cast<void*>(addr)};
                ::fast_io::win32::nt::memory_range_entry mre{.VirtualAddress = va, .NumberOfBytes = length};
                ::std::uint_least32_t VmInformation{5u};  // Default priority is highest priority 5
                ::fast_io::win32::nt::nt_set_information_virtual_memory<false>(
                    reinterpret_cast<void*>(static_cast<::std::ptrdiff_t>(-1)),
                    ::fast_io::win32::nt::virtual_memory_information_class::VmPagePriorityInformation,
                    1uz,
                    ::std::addressof(mre),
                    ::std::addressof(VmInformation),
                    sizeof(::std::uint_least32_t));
                break;
            }
            case madvise_flag::dontneed:
            {
                // "va" will not be modified, the pass parameter requires
                void* va{const_cast<void*>(addr)};
                ::fast_io::win32::nt::memory_range_entry mre{.VirtualAddress = va, .NumberOfBytes = length};
                ::std::uint_least32_t VmInformation{3u};  // Default priority is highest priority 5, set to 3 to lower priority
                ::fast_io::win32::nt::nt_set_information_virtual_memory<false>(
                    reinterpret_cast<void*>(static_cast<::std::ptrdiff_t>(-1)),
                    ::fast_io::win32::nt::virtual_memory_information_class::VmPagePriorityInformation,
                    1uz,
                    ::std::addressof(mre),
                    ::std::addressof(VmInformation),
                    sizeof(::std::uint_least32_t));
                break;
            }
            default:
                [[unlikely]] { break; }
        }
# elif (!defined(_WIN32_WINNT) || _WIN32_WINNT >= 0x0602) && !defined(_WIN32_WINDOWS)
        // win8
        switch(flag)
        {
            case madvise_flag::willneed:
            {
                // "va" will not be modified, the pass parameter requires
                void* va{const_cast<void*>(addr)};
                ::fast_io::win32::win32_memory_range_entry mre{.VirtualAddress = va, .NumberOfBytes = length};
                ::fast_io::win32::PrefetchVirtualMemory(reinterpret_cast<void*>(static_cast<::std::ptrdiff_t>(-1)), 1uz, ::std::addressof(mre), 0);
                break;
            }
            default:
                [[unlikely]] { break; }
        }
# endif
#elif (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !(defined(__MSDOS__) || defined(__DJGPP__)) && !defined(_PICOLIBC__)
# if defined(__linux__) && defined(__NR_madvise)
        // linux syscall
        ::fast_io::system_call<__NR_madvise, int>(addr, length, static_cast<int>(flag));
# elif !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE >= 200112L
        // The madvise function first appeared in 4.4BSD.  The posix_madvise function
        // is part of IEEE 1003.1-2001 and was first implemented in Mac OS X 10.2.

        // "addr" will not be modified, the pass parameter requires
        details::posix::posix_madvise(const_cast<void*>(addr), length, static_cast<int>(flag));
# endif
#endif
    }
}  // namespace uwvm2::utils::ansies
