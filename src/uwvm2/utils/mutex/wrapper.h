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
    /// @brief Native mutex
    using mutex_t = ::fast_io::native_mutex;
    using mutex_guard_t = ::fast_io::operations::decay::stream_ref_decay_lock_guard<mutex_t&>;
    using unlock_mutex_guard_t = ::fast_io::operations::decay::unlock_stream_ref_decay_lock_guard<mutex_t&>;

    /// @brief Write-Priority RW Lock (Turnstile + RoomEmpty + Atomic Readers Count)
    struct rwlock_t
    {
        ::std::atomic_size_t readers{};          // Active readers count
        ::std::atomic_size_t writers_waiting{};  // Writers waiting for the lock
        ::std::atomic_bool writer_active{};      // A writer currently holds the lock
    };

    inline constexpr void rwlock_pause() noexcept { ::std::atomic_signal_fence(::std::memory_order_seq_cst); }

    /// @brief Shared guard for read operations
    struct rw_shared_guard_t
    {
        rwlock_t* lock_ptr{};

        inline explicit constexpr rw_shared_guard_t(rwlock_t& lock) : lock_ptr(::std::addressof(lock))
        {
            auto& lk{*this->lock_ptr};

            for(;;)
            {
                while(lk.writer_active.load(::std::memory_order_acquire) || lk.writers_waiting.load(::std::memory_order_acquire) != 0uz) { rwlock_pause(); }

                auto curr{lk.readers.load(::std::memory_order_acquire)};

                if(lk.writer_active.load(::std::memory_order_acquire) || lk.writers_waiting.load(::std::memory_order_acquire) != 0uz) { continue; }

                if(lk.readers.compare_exchange_weak(curr, curr + 1uz, ::std::memory_order_acquire, ::std::memory_order_relaxed)) { break; }
            }
        }

        inline constexpr ~rw_shared_guard_t()
        {
            if(!this->lock_ptr) [[unlikely]] { return; }
            this->lock_ptr->readers.fetch_sub(1uz, ::std::memory_order_release);
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
            auto& lk{*this->lock_ptr};

            lk.writers_waiting.fetch_add(1uz, ::std::memory_order_acq_rel);

            for(;;)
            {
                if(!lk.writer_active.load(::std::memory_order_acquire) && lk.readers.load(::std::memory_order_acquire) == 0uz)
                {
                    bool expected{false};
                    if(lk.writer_active.compare_exchange_weak(expected, true, ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { break; }
                }

                rwlock_pause();
            }

            lk.writers_waiting.fetch_sub(1uz, ::std::memory_order_release);
        }

        inline constexpr ~rw_unique_guard_t()
        {
            if(!this->lock_ptr) [[unlikely]] { return; }
            this->lock_ptr->writer_active.store(false, ::std::memory_order_release);
        }

        rw_unique_guard_t(rw_unique_guard_t const&) = delete;
        rw_unique_guard_t& operator= (rw_unique_guard_t const&) = delete;
    };
}
