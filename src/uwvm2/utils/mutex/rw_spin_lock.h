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

    inline constexpr void rwlock_pause() noexcept { ::std::atomic_signal_fence(::std::memory_order_seq_cst); }

    inline consteval unsigned rwlock_reader_mask() noexcept { return 4u; }  // READER
    inline consteval unsigned rwlock_writer_mask() noexcept { return 1u; }  // WRITER

    /// @brief Shared guard for read operations
    struct rw_shared_guard_t
    {
        rwlock_t* lock_ptr{};

        inline explicit constexpr rw_shared_guard_t(rwlock_t& lock) : lock_ptr(::std::addressof(lock))
        {
            auto& bits{this->lock_ptr->bits};
            constexpr auto reader_mask{rwlock_reader_mask()};
            constexpr auto writer_mask{rwlock_writer_mask()};

            for(;;)
            {
                auto const old{bits.fetch_add(reader_mask, ::std::memory_order_acquire)};
                if((old & writer_mask) == 0u) { break; }

                bits.fetch_sub(reader_mask, ::std::memory_order_release);
                rwlock_pause();
            }
        }

        inline constexpr ~rw_shared_guard_t()
        {
            if(!this->lock_ptr) [[unlikely]] { return; }
            constexpr auto reader_mask{rwlock_reader_mask()};
            this->lock_ptr->bits.fetch_sub(reader_mask, ::std::memory_order_release);
        }

        rw_shared_guard_t(rw_shared_guard_t const&) = delete;
        rw_shared_guard_t& operator= (rw_shared_guard_t const&) = delete;
    };

    /// @brief Unique guard for write operations
    struct rw_unique_guard_t
    {
        rwlock_t* lock_ptr{};

        inline explicit constexpr rw_unique_guard_t(rwlock_t& lock) : lock_ptr(::std::addressof(lock))
        {
            auto& bits{this->lock_ptr->bits};
            constexpr auto writer_mask{rwlock_writer_mask()};

            for(;;)
            {
                unsigned expected{};
                if(bits.compare_exchange_weak(expected, writer_mask, ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { break; }

                rwlock_pause();
            }
        }

        inline constexpr ~rw_unique_guard_t()
        {
            if(!this->lock_ptr) [[unlikely]] { return; }
            constexpr auto writer_mask{rwlock_writer_mask()};
            this->lock_ptr->bits.fetch_and(~writer_mask, ::std::memory_order_release);
        }

        rw_unique_guard_t(rw_unique_guard_t const&) = delete;
        rw_unique_guard_t& operator= (rw_unique_guard_t const&) = delete;
    };
}
