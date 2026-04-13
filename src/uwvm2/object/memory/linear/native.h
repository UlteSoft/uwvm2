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
# include <version>
# include <limits>
# include <memory>
# include <new>
# include <atomic>
# include <bit>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/mutex/impl.h>
# include <uwvm2/object/memory/wasm_page/impl.h>
# include "allocator.h"
# include "single_thread_allocator.h"
# include "mmap.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::object::memory::linear
{
#if defined(UWVM_SUPPORT_MMAP)
    using native_memory_t = mmap_memory_t;
#else
# if defined(UWVM_USE_MULTITHREAD_ALLOCATOR)
    // Enable on platforms that do not support mmap but support multithreading.

    // check support atomic wait, notify_*
#  if !(__cpp_lib_atomic_wait >= 201907L)
#   error "The platform does not support atomic wait"
#  else
    using native_memory_t = allocator_memory_t;
#  endif

# else
    using native_memory_t = single_thread_allocator_memory_t;
# endif
#endif

}  // namespace uwvm2::object::memory::wasm_page

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
