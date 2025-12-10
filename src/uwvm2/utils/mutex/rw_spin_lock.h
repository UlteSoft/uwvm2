/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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
# include <climits>
# include <concepts>
# include <memory>
# include <utility>
# include <type_traits>
# include <atomic>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::mutex
{
    /// @brief Write-Priority RW Lock (Turnstile + RoomEmpty + Atomic Readers Count)
    struct rwlock_t
    {
        // Bit layout (similar to folly::RWSpinLock):
        // bit0: WRITER, bit1: reserved/UPGRADED, bits[2..]: READER count (each reader adds READER).
        ::std::atomic_uint bits{};
    };

    inline constexpr void rwlock_pause() noexcept
    {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        // x86 / x64: use PAUSE if available
# if UWVM_HAS_BUILTIN(__builtin_ia32_pause)
        __builtin_ia32_pause();
# else
        ::std::atomic_signal_fence(::std::memory_order_seq_cst);
# endif
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__arm__) || defined(_M_ARM)
        // ARM / AArch64: use YIELD-style builtin if available
# if UWVM_HAS_BUILTIN(__builtin_aarch64_yield)
        __builtin_aarch64_yield();
# elif UWVM_HAS_BUILTIN(__builtin_arm_yield)
        __builtin_arm_yield();
# else
        ::std::atomic_signal_fence(::std::memory_order_seq_cst);
# endif
#else
        // Fallback: strong compiler fence
        ::std::atomic_signal_fence(::std::memory_order_seq_cst);
#endif
    }

    inline consteval unsigned rwlock_reader_mask() noexcept { return 4u; }  // READER
    inline consteval unsigned rwlock_writer_mask() noexcept { return 1u; }  // WRITER

    /// @brief Shared guard for read operations
    struct rw_shared_guard_t
    {
        rwlock_t* lock_ptr{};

        inline explicit constexpr rw_shared_guard_t(rwlock_t& lock) noexcept : lock_ptr(::std::addressof(lock))
        {
            auto& bits{this->lock_ptr->bits};
            constexpr auto reader_mask{rwlock_reader_mask()};
            constexpr auto writer_mask{rwlock_writer_mask()};

            unsigned spin_count{};

            for(;;)
            {
                // Acquire on success to synchronize with writer's release-unlock.
                auto const old{bits.fetch_add(reader_mask, ::std::memory_order_acquire)};
                if((old & writer_mask) == 0u) { break; }

                // On failure we only roll back our own increment; no ordering needed.
                bits.fetch_sub(reader_mask, ::std::memory_order_relaxed);

                if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                else
                {
                    rwlock_pause();
                }
            }
        }

        inline constexpr ~rw_shared_guard_t()
        {
            // no necessary to check lock_ptr, because the guard is always valid
            constexpr auto reader_mask{rwlock_reader_mask()};
            this->lock_ptr->bits.fetch_sub(reader_mask, ::std::memory_order_release);
        }

        rw_shared_guard_t() = delete;
        rw_shared_guard_t(rw_shared_guard_t const&) = delete;
        rw_shared_guard_t& operator= (rw_shared_guard_t const&) = delete;
        rw_shared_guard_t(rw_shared_guard_t&&) = delete;
        rw_shared_guard_t& operator= (rw_shared_guard_t&&) = delete;
    };

    /// @brief Unique guard for write operations
    struct rw_unique_guard_t
    {
        rwlock_t* lock_ptr{};

        inline explicit constexpr rw_unique_guard_t(rwlock_t& lock) noexcept : lock_ptr(::std::addressof(lock))
        {
            auto& bits{this->lock_ptr->bits};
            constexpr auto writer_mask{rwlock_writer_mask()};

            unsigned spin_count{};

            for(;;)
            {
                unsigned expected{};
                // Acquire on success to see prior writer's critical section;
                // failure path can be relaxed.
                if(bits.compare_exchange_weak(expected, writer_mask, ::std::memory_order_acquire, ::std::memory_order_relaxed)) { break; }

                if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                else
                {
                    rwlock_pause();
                }
            }
        }

        inline constexpr ~rw_unique_guard_t()
        {
            // no necessary to check lock_ptr, because the guard is always valid
            constexpr auto writer_mask{rwlock_writer_mask()};
            this->lock_ptr->bits.fetch_and(~writer_mask, ::std::memory_order_release);
        }

        inline constexpr rw_unique_guard_t() = delete;
        inline constexpr rw_unique_guard_t(rw_unique_guard_t const&) = delete;
        inline constexpr rw_unique_guard_t& operator= (rw_unique_guard_t const&) = delete;
        inline constexpr rw_unique_guard_t(rw_unique_guard_t&&) = delete;
        inline constexpr rw_unique_guard_t& operator= (rw_unique_guard_t&&) = delete;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
