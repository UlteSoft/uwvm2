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
# include <uwvm2/utils/debug/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::mutex
{
    /// @brief RW spin lock state word.
    /// @details
    ///   Shared by several variants:
    ///   - A non-fair, reader-preferred RW spin lock (`rw_shared_guard_t` / `rw_unique_guard_t`), where writers
    ///     may starve under heavy read load, but readers are extremely cheap.
    ///   - A fair, phase-fair writer-preferred RW spin lock (`rw_fair_shared_guard_t` / `rw_fair_unique_guard_t`),
    ///     which guarantees that once a writer starts waiting, no new readers can enter (writer starvation-free).
    ///   - A non-fair, writer-preferred RW spin lock (`rw_writer_pref_shared_guard_t` / `rw_writer_pref_unique_guard_t`),
    ///     where waiting writers block new readers, and readers can starve under heavy write load.
    ///   - A read-write fair but writer-biased RW spin lock (`rw_fair_writer_pref_shared_guard_t` / `rw_fair_writer_pref_unique_guard_t`),
    ///     which behaves like the fair variant but, when both readers and writers are ready, tends to let writers go first.
    struct rwlock_t
    {
        // Bit layout (similar to folly::RWSpinLock):
        //   bit0: WRITER         (exclusive owner)
        //   bit1: PENDING        (ticketed waiting writer; only used by the fair variant)
        //   bit2: WRITER_WAITING (there exists at least one waiting writer; only used by the writer-preferred variant)
        //   bits[3..]: READER count (each reader adds READER_MASK).
        ::std::atomic_uint bits{};
    };

    UWVM_ALWAYS_INLINE inline constexpr void rwlock_pause() noexcept
    {
#if (defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
        // x86 / x64: use PAUSE if available
# if UWVM_HAS_BUILTIN(__builtin_ia32_pause)
        __builtin_ia32_pause();
# else
        ::std::atomic_signal_fence(::std::memory_order_seq_cst);
# endif
#elif (defined(__aarch64__) || defined(_M_ARM64)) || (defined(__arm__) || defined(_M_ARM)) || (defined(__arm64ec__) || defined(_M_ARM64EC))
        // ARM / AArch64: use YIELD-style builtin if available
# if UWVM_HAS_BUILTIN(__builtin_aarch64_yield)
        __builtin_aarch64_yield();
# elif UWVM_HAS_BUILTIN(__builtin_arm_yield)
        __builtin_arm_yield();
# else
        ::std::atomic_signal_fence(::std::memory_order_seq_cst);
# endif
#elif defined(__riscv)
# if UWVM_HAS_BUILTIN(__builtin_riscv_pause)
        __builtin_riscv_pause();
# else
        ::std::atomic_signal_fence(::std::memory_order_seq_cst);
# endif
#else
        // Fallback: strong compiler fence
        ::std::atomic_signal_fence(::std::memory_order_seq_cst);
#endif
    }

    inline consteval unsigned rwlock_writer_mask() noexcept { return 1u; }          // WRITER (bit0)
    inline consteval unsigned rwlock_pending_mask() noexcept { return 2u; }         // PENDING (bit1, fair variant)
    inline consteval unsigned rwlock_writer_waiting_mask() noexcept { return 4u; }  // WRITER_WAITING (bit2, writer-preferred variant)
    inline consteval unsigned rwlock_reader_waiting_mask() noexcept
    {
        // Backward-compatible alias (no external users in this project).
        return rwlock_writer_waiting_mask();
    }
    inline consteval unsigned rwlock_reader_mask() noexcept { return 8u; }  // READER increment (bits[3..])
    inline consteval unsigned rwlock_reader_count_mask() noexcept
    {
        // All bits used for the reader count (bits[3..]).
        return static_cast<unsigned>(~(rwlock_reader_mask() - 1u));
    }

    UWVM_ALWAYS_INLINE inline constexpr void rwlock_assert_no_reader_overflow([[maybe_unused]] unsigned old) noexcept
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        constexpr auto reader_count_mask{rwlock_reader_count_mask()};
        // Theoretically unreachable: would require an astronomically large
        // number of concurrent readers; if this ever triggers, something
        // elsewhere is badly wrong.
        if((old & reader_count_mask) == reader_count_mask) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
    }

    /// @brief Non-fair shared guard for read operations.
    /// @details
    ///   Reader-preferred (non-fair) variant:
    ///   - Readers can always enter as long as no writer currently owns the lock (WRITER bit is 0).
    ///   - Writers only proceed when there are no readers, and can be starved by a continuous stream of readers.
    ///   - When readers and writers contend at the same time, readers are statistically more likely to win,
    ///     because they only require WRITER==0 while writers require the whole state word to be zero.
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
                rwlock_assert_no_reader_overflow(old);
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

    /// @brief Non-fair unique guard for write operations.
    /// @details
    ///   Non-fair writer side of the reader-preferred RW spin lock:
    ///   - Tries to acquire exclusive ownership only when the state word is entirely zero (no readers, no writer).
    ///   - Under heavy read contention, writers may wait for a long time or be starved; in head-to-head races,
    ///     readers tend to win because they have a weaker acquisition condition.
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

    /// @brief Writer-preferred shared guard for read operations.
    /// @details
    ///   Writer-preferred (non-fair) variant:
    ///   - Once a writer has indicated interest in the lock (WRITER_WAITING bit set),
    ///     new readers are blocked until writers have made progress.
    ///   - Existing readers are allowed to finish, but if writers keep arriving,
    ///     readers may be starved in the long run.
    struct rw_writer_pref_shared_guard_t
    {
        rwlock_t* lock_ptr{};

        inline explicit constexpr rw_writer_pref_shared_guard_t(rwlock_t& lock) noexcept : lock_ptr(::std::addressof(lock))
        {
            auto& bits{this->lock_ptr->bits};
            constexpr auto reader_mask{rwlock_reader_mask()};
            constexpr auto writer_mask{rwlock_writer_mask()};
            constexpr auto writer_waiting_mask{rwlock_writer_waiting_mask()};

            unsigned spin_count{};

            for(;;)
            {
                auto old{bits.load(::std::memory_order_relaxed)};

                // Writer-preferred: if there is a writer owning or waiting,
                // new readers must not enter.
                if((old & (writer_mask | writer_waiting_mask)) != 0u)
                {
                    if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                    else
                    {
                        rwlock_pause();
                    }
                    continue;
                }

                rwlock_assert_no_reader_overflow(old);
                auto const desired{old + reader_mask};
                // Acquire on success to synchronize with writer's release-unlock.
                if(bits.compare_exchange_weak(old, desired, ::std::memory_order_acquire, ::std::memory_order_relaxed)) { break; }

                if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                else
                {
                    rwlock_pause();
                }
            }
        }

        inline constexpr ~rw_writer_pref_shared_guard_t()
        {
            // no necessary to check lock_ptr, because the guard is always valid
            constexpr auto reader_mask{rwlock_reader_mask()};
            this->lock_ptr->bits.fetch_sub(reader_mask, ::std::memory_order_release);
        }

        rw_writer_pref_shared_guard_t() = delete;
        rw_writer_pref_shared_guard_t(rw_writer_pref_shared_guard_t const&) = delete;
        rw_writer_pref_shared_guard_t& operator= (rw_writer_pref_shared_guard_t const&) = delete;
        rw_writer_pref_shared_guard_t(rw_writer_pref_shared_guard_t&&) = delete;
        rw_writer_pref_shared_guard_t& operator= (rw_writer_pref_shared_guard_t&&) = delete;
    };

    /// @brief Writer-preferred unique guard for write operations.
    /// @details
    ///   Writer-preferred (non-fair) variant:
    ///   - Writers set WRITER_WAITING to block new readers while they are waiting.
    ///   - Once no readers and no writer own the lock, a waiting writer acquires WRITER.
    ///   - If writers are continuously arriving, readers can be starved.
    struct rw_writer_pref_unique_guard_t
    {
        rwlock_t* lock_ptr{};

        inline explicit constexpr rw_writer_pref_unique_guard_t(rwlock_t& lock) noexcept : lock_ptr(::std::addressof(lock))
        {
            auto& bits{this->lock_ptr->bits};
            constexpr auto writer_mask{rwlock_writer_mask()};
            constexpr auto writer_waiting_mask{rwlock_writer_waiting_mask()};
            constexpr auto reader_count_mask{rwlock_reader_count_mask()};

            unsigned spin_count{};

            for(;;)
            {
                auto old{bits.load(::std::memory_order_relaxed)};

                // Phase 1: announce that a writer is waiting.
                if((old & writer_waiting_mask) == 0u)
                {
                    auto const desired{old | writer_waiting_mask};
                    if(!bits.compare_exchange_weak(old, desired, ::std::memory_order_acq_rel, ::std::memory_order_relaxed))
                    {
                        if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                        else
                        {
                            rwlock_pause();
                        }
                        continue;
                    }
                    // Successfully set WRITER_WAITING; reload state in next iteration
                    // to wait for readers to drain and then grab WRITER.
                    if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                    else
                    {
                        rwlock_pause();
                    }
                    continue;
                }

                // Phase 2: WRITER_WAITING is set. Once no readers and no writer
                // own the lock, acquire exclusive ownership.
                if((old & writer_mask) == 0u && (old & reader_count_mask) == 0u)
                {
                    auto const desired{old | writer_mask};
                    // Acquire on success to see prior critical sections.
                    if(bits.compare_exchange_weak(old, desired, ::std::memory_order_acquire, ::std::memory_order_relaxed)) { break; }
                }

                if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                else
                {
                    rwlock_pause();
                }
            }
        }

        inline constexpr ~rw_writer_pref_unique_guard_t()
        {
            // no necessary to check lock_ptr, because the guard is always valid
            constexpr auto writer_mask{rwlock_writer_mask()};
            constexpr auto writer_waiting_mask{rwlock_writer_waiting_mask()};
            this->lock_ptr->bits.fetch_and(~(writer_mask | writer_waiting_mask), ::std::memory_order_release);
        }

        inline constexpr rw_writer_pref_unique_guard_t() = delete;
        inline constexpr rw_writer_pref_unique_guard_t(rw_writer_pref_unique_guard_t const&) = delete;
        inline constexpr rw_writer_pref_unique_guard_t& operator= (rw_writer_pref_unique_guard_t const&) = delete;
        inline constexpr rw_writer_pref_unique_guard_t(rw_writer_pref_unique_guard_t&&) = delete;
        inline constexpr rw_writer_pref_unique_guard_t& operator= (rw_writer_pref_unique_guard_t&&) = delete;
    };

    /// @brief Writer-preferred, writer-starvation-free shared guard.
    /// @details
    ///   Phase-fair writer-preferred variant, specialized for "no writer starvation":
    ///   - Once a writer acquires the PENDING ticket, new readers are blocked until that writer has finished.
    ///   - Guarantees that writers are not starved by an unbounded stream of new readers.
    struct rw_writer_pref_no_starvation_shared_guard_t
    {
        rwlock_t* lock_ptr{};

        inline explicit constexpr rw_writer_pref_no_starvation_shared_guard_t(rwlock_t& lock) noexcept : lock_ptr(::std::addressof(lock))
        {
            auto& bits{this->lock_ptr->bits};
            constexpr auto reader_mask{rwlock_reader_mask()};
            constexpr auto writer_mask{rwlock_writer_mask()};
            constexpr auto pending_mask{rwlock_pending_mask()};

            unsigned spin_count{};

            for(;;)
            {
                auto old{bits.load(::std::memory_order_relaxed)};

                // Writer-preferred, no-starvation: if there is a writer owning
                // or holding the ticket, new readers must not enter.
                if((old & (writer_mask | pending_mask)) != 0u)
                {
                    if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                    else
                    {
                        rwlock_pause();
                    }
                    continue;
                }

                rwlock_assert_no_reader_overflow(old);
                auto const desired{old + reader_mask};
                // Acquire on success to synchronize with writer's release-unlock.
                if(bits.compare_exchange_weak(old, desired, ::std::memory_order_acquire, ::std::memory_order_relaxed)) { break; }

                if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                else
                {
                    rwlock_pause();
                }
            }
        }

        inline constexpr ~rw_writer_pref_no_starvation_shared_guard_t()
        {
            // no necessary to check lock_ptr, because the guard is always valid
            constexpr auto reader_mask{rwlock_reader_mask()};

            this->lock_ptr->bits.fetch_sub(reader_mask, ::std::memory_order_release);
        }

        rw_writer_pref_no_starvation_shared_guard_t() = delete;
        rw_writer_pref_no_starvation_shared_guard_t(rw_writer_pref_no_starvation_shared_guard_t const&) = delete;
        rw_writer_pref_no_starvation_shared_guard_t& operator= (rw_writer_pref_no_starvation_shared_guard_t const&) = delete;
        rw_writer_pref_no_starvation_shared_guard_t(rw_writer_pref_no_starvation_shared_guard_t&&) = delete;
        rw_writer_pref_no_starvation_shared_guard_t& operator= (rw_writer_pref_no_starvation_shared_guard_t&&) = delete;
    };

    /// @brief Writer-preferred, writer-starvation-free unique guard.
    /// @details
    ///   Phase-fair writer-preferred variant:
    ///   - Writers acquire a serialized PENDING ticket and then wait for readers to drain.
    ///   - Ensures every writer holding a ticket eventually acquires exclusive ownership (no writer starvation).
    struct rw_writer_pref_no_starvation_unique_guard_t
    {
        rwlock_t* lock_ptr{};

        inline explicit constexpr rw_writer_pref_no_starvation_unique_guard_t(rwlock_t& lock) noexcept : lock_ptr(::std::addressof(lock))
        {
            auto& bits{this->lock_ptr->bits};
            constexpr auto writer_mask{rwlock_writer_mask()};
            constexpr auto pending_mask{rwlock_pending_mask()};
            constexpr auto reader_count_mask{rwlock_reader_count_mask()};

            unsigned spin_count{};
            bool have_pending_ticket{};

            for(;;)
            {
                auto old{bits.load(::std::memory_order_relaxed)};

                // Phase 1: acquire a "pending" ticket if we do not
                // already own it. Only the ticketed writer is allowed
                // to proceed to acquire WRITER; other writers spin.
                if(!have_pending_ticket)
                {
                    // If another writer already holds the pending ticket,
                    // wait for it to complete.
                    if((old & pending_mask) != 0u)
                    {
                        if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                        else
                        {
                            rwlock_pause();
                        }
                        continue;
                    }

                    auto const desired{old | pending_mask};
                    if(!bits.compare_exchange_weak(old, desired, ::std::memory_order_release, ::std::memory_order_relaxed))
                    {
                        // CAS failed, retry with updated 'old'.
                        if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                        else
                        {
                            rwlock_pause();
                        }
                        continue;
                    }

                    // Successfully acquired the pending ticket; reload
                    // current state in the next iteration and proceed to
                    // phase 2 (waiting for readers to drain).
                    have_pending_ticket = true;
                    if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                    else
                    {
                        rwlock_pause();
                    }
                    continue;
                }

                // Phase 2: we own the pending ticket. Once no readers and
                // no writer own the lock, acquire exclusive ownership.
                //
                // We deliberately ignore pending_mask here: PENDING is our
                // ticket, not ownership. We only care that:
                //   - WRITER bit is 0, and
                //   - reader count bits are 0.
                if((old & writer_mask) == 0u && (old & reader_count_mask) == 0u)
                {
                    auto const desired{old | writer_mask};
                    // Acquire on success to see prior critical sections.
                    if(bits.compare_exchange_weak(old, desired, ::std::memory_order_acquire, ::std::memory_order_relaxed)) { break; }
                }

                if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                else
                {
                    rwlock_pause();
                }
            }
        }

        inline constexpr ~rw_writer_pref_no_starvation_unique_guard_t()
        {
            // no necessary to check lock_ptr, because the guard is always valid
            constexpr auto writer_mask{rwlock_writer_mask()};
            constexpr auto pending_mask{rwlock_pending_mask()};
            this->lock_ptr->bits.fetch_and(~(writer_mask | pending_mask), ::std::memory_order_release);
        }

        inline constexpr rw_writer_pref_no_starvation_unique_guard_t() = delete;
        inline constexpr rw_writer_pref_no_starvation_unique_guard_t(rw_writer_pref_no_starvation_unique_guard_t const&) = delete;
        inline constexpr rw_writer_pref_no_starvation_unique_guard_t& operator= (rw_writer_pref_no_starvation_unique_guard_t const&) = delete;
        inline constexpr rw_writer_pref_no_starvation_unique_guard_t(rw_writer_pref_no_starvation_unique_guard_t&&) = delete;
        inline constexpr rw_writer_pref_no_starvation_unique_guard_t& operator= (rw_writer_pref_no_starvation_unique_guard_t&&) = delete;
    };

    /// @brief Fair shared guard for read operations.
    /// @details
    ///   Fair writer-preferred variant:
    ///   - Once a writer acquires the PENDING ticket, new readers are blocked until that writer has finished.
    ///   - Prevents writers from being starved by an unbounded stream of new readers.
    ///   - After a writer releases the lock, readers and writers race for the next phase; because there can be many
    ///     readers but usually few writers, readers are statistically more likely to acquire the lock first.
    struct rw_fair_shared_guard_t
    {
        rwlock_t* lock_ptr{};

        inline explicit constexpr rw_fair_shared_guard_t(rwlock_t& lock) noexcept : lock_ptr(::std::addressof(lock))
        {
            auto& bits{this->lock_ptr->bits};
            constexpr auto reader_mask{rwlock_reader_mask()};
            constexpr auto writer_mask{rwlock_writer_mask()};
            constexpr auto pending_mask{rwlock_pending_mask()};

            unsigned spin_count{};

            for(;;)
            {
                auto old{bits.load(::std::memory_order_relaxed)};

                // Write-priority: if there is a writer owning or waiting,
                // new readers must not enter.
                if((old & (writer_mask | pending_mask)) != 0u)
                {
                    if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                    else
                    {
                        rwlock_pause();
                    }
                    continue;
                }

                rwlock_assert_no_reader_overflow(old);
                auto const desired{old + reader_mask};
                // Acquire on success to synchronize with writer's release-unlock.
                if(bits.compare_exchange_weak(old, desired, ::std::memory_order_acquire, ::std::memory_order_relaxed)) { break; }

                if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                else
                {
                    rwlock_pause();
                }
            }
        }

        inline constexpr ~rw_fair_shared_guard_t()
        {
            // no necessary to check lock_ptr, because the guard is always valid
            constexpr auto reader_mask{rwlock_reader_mask()};

            this->lock_ptr->bits.fetch_sub(reader_mask, ::std::memory_order_release);
        }

        rw_fair_shared_guard_t() = delete;
        rw_fair_shared_guard_t(rw_fair_shared_guard_t const&) = delete;
        rw_fair_shared_guard_t& operator= (rw_fair_shared_guard_t const&) = delete;
        rw_fair_shared_guard_t(rw_fair_shared_guard_t&&) = delete;
        rw_fair_shared_guard_t& operator= (rw_fair_shared_guard_t&&) = delete;
    };

    /// @brief Fair unique guard for write operations.
    /// @details
    ///   Phase-fair writer-preferred variant:
    ///   - Uses a PENDING ticket to serialize writers and block new readers while a writer is waiting.
    ///   - Acquires WRITER only after all current readers have left, ensuring writers eventually make progress.
    ///   - At phase boundaries (when WRITER and PENDING are cleared) both readers and writers may be waiting;
    ///     there is no deliberate bias, but in practice new readers often win this race (slight read tilt).
    struct rw_fair_unique_guard_t
    {
        rwlock_t* lock_ptr{};

        inline explicit constexpr rw_fair_unique_guard_t(rwlock_t& lock) noexcept : lock_ptr(::std::addressof(lock))
        {
            auto& bits{this->lock_ptr->bits};
            constexpr auto writer_mask{rwlock_writer_mask()};
            constexpr auto pending_mask{rwlock_pending_mask()};
            constexpr auto reader_count_mask{rwlock_reader_count_mask()};

            unsigned spin_count{};
            bool have_pending_ticket{};

            for(;;)
            {
                auto old{bits.load(::std::memory_order_relaxed)};

                // Phase 1: acquire a "pending" ticket if we do not
                // already own it. Only the ticketed writer is allowed
                // to proceed to acquire WRITER; other writers spin.
                if(!have_pending_ticket)
                {
                    // If another writer already holds the pending ticket,
                    // wait for it to complete.
                    if((old & pending_mask) != 0u)
                    {
                        if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                        else
                        {
                            rwlock_pause();
                        }
                        continue;
                    }

                    auto const desired{old | pending_mask};
                    if(!bits.compare_exchange_weak(old, desired, ::std::memory_order_release, ::std::memory_order_relaxed))
                    {
                        // CAS failed, retry with updated 'old'.
                        if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                        else
                        {
                            rwlock_pause();
                        }
                        continue;
                    }

                    // Successfully acquired the pending ticket; reload
                    // current state in the next iteration and proceed to
                    // phase 2 (waiting for readers to drain).
                    have_pending_ticket = true;
                    if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                    else
                    {
                        rwlock_pause();
                    }
                    continue;
                }

                // Phase 2: we own the pending ticket. Once no readers and
                // no writer own the lock, acquire exclusive ownership.
                //
                // We deliberately ignore pending_mask here: PENDING is our
                // ticket, not ownership. We only care that:
                //   - WRITER bit is 0, and
                //   - reader count bits are 0.
                if((old & writer_mask) == 0u && (old & reader_count_mask) == 0u)
                {
                    auto const desired{old | writer_mask};
                    // Acquire on success to see prior critical sections.
                    if(bits.compare_exchange_weak(old, desired, ::std::memory_order_acquire, ::std::memory_order_relaxed)) { break; }
                }

                if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                else
                {
                    rwlock_pause();
                }
            }
        }

        inline constexpr ~rw_fair_unique_guard_t()
        {
            // no necessary to check lock_ptr, because the guard is always valid
            constexpr auto writer_mask{rwlock_writer_mask()};
            constexpr auto pending_mask{rwlock_pending_mask()};
            this->lock_ptr->bits.fetch_and(~(writer_mask | pending_mask), ::std::memory_order_release);
        }

        inline constexpr rw_fair_unique_guard_t() = delete;
        inline constexpr rw_fair_unique_guard_t(rw_fair_unique_guard_t const&) = delete;
        inline constexpr rw_fair_unique_guard_t& operator= (rw_fair_unique_guard_t const&) = delete;
        inline constexpr rw_fair_unique_guard_t(rw_fair_unique_guard_t&&) = delete;
        inline constexpr rw_fair_unique_guard_t& operator= (rw_fair_unique_guard_t&&) = delete;
    };

    /// @brief Fair, writer-biased shared guard for read operations.
    /// @details
    ///   Read-write fair but writer-tilted variant:
    ///   - Behaves like `rw_fair_shared_guard_t`, but also observes the WRITER_WAITING bit.
    ///   - Once a writer has indicated interest in the lock, new readers are blocked until writers
    ///     holding the ticket have had a chance to acquire WRITER.
    struct rw_fair_writer_pref_shared_guard_t
    {
        rwlock_t* lock_ptr{};

        inline explicit constexpr rw_fair_writer_pref_shared_guard_t(rwlock_t& lock) noexcept : lock_ptr(::std::addressof(lock))
        {
            auto& bits{this->lock_ptr->bits};
            constexpr auto reader_mask{rwlock_reader_mask()};
            constexpr auto writer_mask{rwlock_writer_mask()};
            constexpr auto pending_mask{rwlock_pending_mask()};
            constexpr auto writer_waiting_mask{rwlock_writer_waiting_mask()};

            unsigned spin_count{};

            for(;;)
            {
                auto old{bits.load(::std::memory_order_relaxed)};

                // Writer-biased fair: if there is a writer owning, ticketed, or
                // explicitly waiting, new readers must not enter.
                if((old & (writer_mask | pending_mask | writer_waiting_mask)) != 0u)
                {
                    if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                    else
                    {
                        rwlock_pause();
                    }
                    continue;
                }

                rwlock_assert_no_reader_overflow(old);
                auto const desired{old + reader_mask};
                // Acquire on success to synchronize with writer's release-unlock.
                if(bits.compare_exchange_weak(old, desired, ::std::memory_order_acquire, ::std::memory_order_relaxed)) { break; }

                if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                else
                {
                    rwlock_pause();
                }
            }
        }

        inline constexpr ~rw_fair_writer_pref_shared_guard_t()
        {
            // no necessary to check lock_ptr, because the guard is always valid
            constexpr auto reader_mask{rwlock_reader_mask()};

            this->lock_ptr->bits.fetch_sub(reader_mask, ::std::memory_order_release);
        }

        rw_fair_writer_pref_shared_guard_t() = delete;
        rw_fair_writer_pref_shared_guard_t(rw_fair_writer_pref_shared_guard_t const&) = delete;
        rw_fair_writer_pref_shared_guard_t& operator= (rw_fair_writer_pref_shared_guard_t const&) = delete;
        rw_fair_writer_pref_shared_guard_t(rw_fair_writer_pref_shared_guard_t&&) = delete;
        rw_fair_writer_pref_shared_guard_t& operator= (rw_fair_writer_pref_shared_guard_t&&) = delete;
    };

    /// @brief Fair, writer-biased unique guard for write operations.
    /// @details
    ///   Read-write fair but writer-tilted variant:
    ///   - Writers first advertise interest via WRITER_WAITING, then use PENDING to serialize among themselves.
    ///   - New readers observe WRITER_WAITING and are blocked, so when both readers and writers are ready,
    ///     writers tend to acquire the lock first.
    struct rw_fair_writer_pref_unique_guard_t
    {
        rwlock_t* lock_ptr{};

        inline explicit constexpr rw_fair_writer_pref_unique_guard_t(rwlock_t& lock) noexcept : lock_ptr(::std::addressof(lock))
        {
            auto& bits{this->lock_ptr->bits};
            constexpr auto writer_mask{rwlock_writer_mask()};
            constexpr auto pending_mask{rwlock_pending_mask()};
            constexpr auto writer_waiting_mask{rwlock_writer_waiting_mask()};
            constexpr auto reader_count_mask{rwlock_reader_count_mask()};

            unsigned spin_count{};
            bool have_pending_ticket{};

            for(;;)
            {
                auto old{bits.load(::std::memory_order_relaxed)};

                // Phase 0: announce that some writer is waiting.
                if((old & writer_waiting_mask) == 0u)
                {
                    auto const desired0{old | writer_waiting_mask};
                    if(!bits.compare_exchange_weak(old, desired0, ::std::memory_order_release, ::std::memory_order_relaxed))
                    {
                        if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                        else
                        {
                            rwlock_pause();
                        }
                        continue;
                    }
                    // Successfully set WRITER_WAITING; reload state in next iteration.
                    if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                    else
                    {
                        rwlock_pause();
                    }
                    continue;
                }

                // Phase 1: acquire a "pending" ticket if we do not
                // already own it. Only the ticketed writer is allowed
                // to proceed to acquire WRITER; other writers spin.
                if(!have_pending_ticket)
                {
                    // If another writer already holds the pending ticket,
                    // wait for it to complete.
                    if((old & pending_mask) != 0u)
                    {
                        if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                        else
                        {
                            rwlock_pause();
                        }
                        continue;
                    }

                    auto const desired{old | pending_mask};
                    if(!bits.compare_exchange_weak(old, desired, ::std::memory_order_release, ::std::memory_order_relaxed))
                    {
                        // CAS failed, retry with updated 'old'.
                        if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                        else
                        {
                            rwlock_pause();
                        }
                        continue;
                    }

                    // Successfully acquired the pending ticket; reload
                    // current state in the next iteration and proceed to
                    // phase 2 (waiting for readers to drain).
                    have_pending_ticket = true;
                    if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                    else
                    {
                        rwlock_pause();
                    }
                    continue;
                }

                // Phase 2: we own the pending ticket. Once no readers and
                // no writer own the lock, acquire exclusive ownership.
                //
                // We deliberately ignore pending_mask and writer_waiting_mask here:
                // they express intent to acquire, not ownership. We only care that:
                //   - WRITER bit is 0, and
                //   - reader count bits are 0.
                if((old & writer_mask) == 0u && (old & reader_count_mask) == 0u)
                {
                    auto const desired{old | writer_mask};
                    // Acquire on success to see prior critical sections.
                    if(bits.compare_exchange_weak(old, desired, ::std::memory_order_acquire, ::std::memory_order_relaxed)) { break; }
                }

                if(++spin_count > 1000u) { ::fast_io::this_thread::yield(); }
                else
                {
                    rwlock_pause();
                }
            }
        }

        inline constexpr ~rw_fair_writer_pref_unique_guard_t()
        {
            // no necessary to check lock_ptr, because the guard is always valid
            constexpr auto writer_mask{rwlock_writer_mask()};
            constexpr auto pending_mask{rwlock_pending_mask()};
            constexpr auto writer_waiting_mask{rwlock_writer_waiting_mask()};
            this->lock_ptr->bits.fetch_and(~(writer_mask | pending_mask | writer_waiting_mask), ::std::memory_order_release);
        }

        inline constexpr rw_fair_writer_pref_unique_guard_t() = delete;
        inline constexpr rw_fair_writer_pref_unique_guard_t(rw_fair_writer_pref_unique_guard_t const&) = delete;
        inline constexpr rw_fair_writer_pref_unique_guard_t& operator= (rw_fair_writer_pref_unique_guard_t const&) = delete;
        inline constexpr rw_fair_writer_pref_unique_guard_t(rw_fair_writer_pref_unique_guard_t&&) = delete;
        inline constexpr rw_fair_writer_pref_unique_guard_t& operator= (rw_fair_writer_pref_unique_guard_t&&) = delete;
    };

    /// @brief Fair, writer-biased shared guard (no writer starvation).
    using rw_fair_writer_pref_no_starvation_shared_guard_t = rw_fair_writer_pref_shared_guard_t;

    /// @brief Fair, writer-biased unique guard (no writer starvation).
    using rw_fair_writer_pref_no_starvation_unique_guard_t = rw_fair_writer_pref_unique_guard_t;
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
