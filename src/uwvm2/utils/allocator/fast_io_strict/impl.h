/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-24
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

#include <fast_io.h>

/// @brief  This version provides a strict implementation of the fast_io allocator, which returns nullptr upon allocation failure.

#if (defined(_WIN32) || defined(__CYGWIN__)) && !defined(__WINE__)
# include "nt_rtlheapalloc.h"
# include "win32_heapalloc.h"
#endif
#if ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && !defined(_LIBCPP_FREESTANDING)) || defined(FAST_IO_ENABLE_HOSTED_FEATURES))
# include "c_malloc.h"
# if defined(_DEBUG) && defined(_MSC_VER)
#  include "wincrt_malloc_dbg.h"
# endif
#endif
#if (defined(__linux__) && defined(__KERNEL__)) || defined(FAST_IO_USE_LINUX_KERNEL_ALLOCATOR)
# include "linux_kernel.h"
#endif

#if (defined(FAST_IO_ENABLE_MIMALLOC) || defined(FAST_IO_USE_MIMALLOC)) && (!defined(_MSC_VER) || defined(__clang__))
# include "mimalloc_driver.h"
#endif

namespace uwvm2::utils::allocator::fast_io_strict
{
    using native_strict_global_allocator = ::fast_io::generic_allocator_adapter<
#if defined(FAST_IO_USE_CUSTOM_GLOBAL_ALLOCATOR)
        ::fast_io::custom_global_allocator
#elif defined(FAST_IO_USE_MIMALLOC) && (!defined(_MSC_VER) || defined(__clang__))
        ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_mimalloc_allocator
#elif (defined(__linux__) && defined(__KERNEL__)) || defined(FAST_IO_USE_LINUX_KERNEL_ALLOCATOR)
        ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_linux_kmalloc_allocator
#elif ((__STDC_HOSTED__ == 1 && (!defined(_GLIBCXX_HOSTED) || _GLIBCXX_HOSTED == 1) && !defined(_LIBCPP_FREESTANDING)) ||                                      \
       defined(FAST_IO_ENABLE_HOSTED_FEATURES))
# if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__WINE__) && !defined(FAST_IO_USE_C_MALLOC)
#  if defined(_DEBUG) && defined(_MSC_VER)
        ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_wincrt_malloc_dbg_allocator
#  else
        ::std::conditional_t<::fast_io::asan::asan_status::current == ::fast_io::asan::asan_status::none,
#   ifdef _WIN32_WINDOWS
                             ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_win32_heapalloc_allocator
#   else
                             ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_nt_rtlallocateheap_allocator
#   endif
                             ,
                             ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_c_malloc_allocator>
#  endif
# else
#  if defined(_DEBUG) && defined(_MSC_VER)
        ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_wincrt_malloc_dbg_allocator
#  else
        ::uwvm2::utils::allocator::fast_io_strict::fast_io_strict_c_malloc_allocator
#  endif
# endif
#else
        ::fast_io::custom_global_allocator
#endif
        >;

    template <typename T>
    using native_strict_typed_global_allocator = ::fast_io::typed_generic_allocator_adapter<native_strict_global_allocator, T>;

    using native_strict_thread_local_allocator = ::fast_io::generic_allocator_adapter<
#if defined(FAST_IO_USE_CUSTOM_THREAD_LOCAL_ALLOCATOR)
        ::fast_io::custom_thread_local_allocator
#else
        native_strict_global_allocator
#endif
        >;

    template <typename T>
    using native_strict_typed_thread_local_allocator = ::fast_io::typed_generic_allocator_adapter<native_strict_thread_local_allocator, T>;

}  // namespace uwvm2::utils::allocator::fast_io_strict
