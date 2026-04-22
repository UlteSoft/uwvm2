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
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/mutex/impl.h>
# include "base.h"
# include "allocator.h"
# include "single_thread_allocator.h"
# include "mmap.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::object::memory::linear
{
    /// @brief Execute `fn(memory_begin, byte_length)` against a consistent linear-memory snapshot.
    /// @note  For allocator-backed multi-threaded memories, the callback runs under exclusive access: grow and other in-flight memory operations must drain first.
    template <typename MemoryT, typename Fn>
    [[nodiscard]] inline constexpr bool with_memory_access_snapshot(MemoryT const& memory, Fn&& fn) noexcept
    {
        if constexpr(MemoryT::can_mmap)
        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(memory.memory_length_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

            auto const byte_length{memory.memory_length_p->load(::std::memory_order_acquire)};
            return static_cast<bool>(::std::forward<Fn>(fn)(memory.memory_begin, byte_length));
        }
        else if constexpr(MemoryT::support_multi_thread)
        {
#if __cpp_lib_atomic_wait >= 201907L
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(memory.growing_flag_p == nullptr || memory.active_ops_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

            growing_flag_guard_t growing_flag_guard{memory.growing_flag_p};

            unsigned spin_count{};
            for(auto v{memory.active_ops_p->load(::std::memory_order_acquire)}; v != 0uz; v = memory.active_ops_p->load(::std::memory_order_acquire))
            {
                if(++spin_count > 1000u)
                {
                    memory.active_ops_p->wait(v, ::std::memory_order_acquire);
                    spin_count = 0u;
                }
                else
                {
                    ::uwvm2::utils::mutex::rwlock_pause();
                }
            }

            return static_cast<bool>(::std::forward<Fn>(fn)(memory.memory_begin, memory.memory_length));
#else
            return static_cast<bool>(::std::forward<Fn>(fn)(memory.memory_begin, memory.memory_length));
#endif
        }
        else
        {
            return static_cast<bool>(::std::forward<Fn>(fn)(memory.memory_begin, memory.memory_length));
        }
    }

}  // namespace uwvm2::object::memory::linear

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
