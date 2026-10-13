/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <atomic>
# include <chrono>
# include <cstddef>
# include <memory>
# include <utility>
# include <uwvm2/utils/macro/push_macros.h>
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#  include <condition_variable>
#  include <mutex>
#  include <stop_token>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::utils::thread
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // Explicitly visible in the exported lifetime partition before its private
    // friend declaration; the domain definition belongs to this same module.
    class execution_domain;
    // Tracks execution on host-created threads without taking ownership of the
    // OS thread. Admission is bounded and irrevocably closed on stop. An owner
    // requests stop, wakes its blocking services, then drains admitted work
    // before destroying resources. This does not forcibly terminate a thread.
    // Callers must stop entering before destroying this object. Destruction on
    // a thread that still owns one of its leases would deadlock and is forbidden.
    class execution_lifetime
    {
        friend class execution_domain;
        // Sole owning domain, under actual maintenance. Publish cooperative
        // stop without invoking a foreign/blocked host callback synchronously.
        // Only stopping changes: outstanding stop_token callbacks stay live
        // until their genuine admitted call returns. No lease is released here.
        void mark_stopping_without_callbacks()
        {
            ::std::lock_guard lock{mutex};
            stopping.store(true, ::std::memory_order_release);
        }
        // Only execution_domain calls this while holding its REAL admission
        // mutex. New leases cannot enter; after observing zero, no existing
        // lease can revive it. Drop this short lifetime lock before publication.
        [[nodiscard]] bool quiescent_configuration_ready() noexcept
        {
            ::std::lock_guard lock{mutex};
            return active == 0uz && !stopping.load(::std::memory_order_relaxed);
        }
        ::std::mutex mutex{};
        ::std::condition_variable idle{};
        ::std::atomic_bool stopping{};
        ::std::stop_source cancellation{};
        ::std::size_t active{};
        ::std::size_t const capacity;
        void leave()
        {
            ::std::lock_guard lock{mutex};
            --active;
            if(active == 0uz) { idle.notify_all(); }
        }

    public:
        class lease
        {
            friend class execution_lifetime;
            execution_lifetime* owner{};
            explicit lease(execution_lifetime* value) noexcept : owner{value} {}
        public:
            lease() noexcept = default;
            lease(lease const&) = delete;
            lease& operator=(lease const&) = delete;
            lease(lease&& other) noexcept : owner{::std::exchange(other.owner, nullptr)} {}
            lease& operator=(lease&& other) noexcept
            {
                if(this != ::std::addressof(other))
                {
                    reset();
                    owner = ::std::exchange(other.owner, nullptr);
                }
                return *this;
            }
            ~lease() { reset(); }
            void reset() noexcept
            {
                if(owner != nullptr)
                {
                    // [live owner] remains alive until this final admission is released.
                    // ^^ current borrows the owner; clear this token before decrementing.
                    auto* current{::std::exchange(owner, nullptr)};
                    current->leave();
                    // No owner access after leave(): the draining owner may now be destroyed.
                }
            }
            [[nodiscard]] explicit operator bool() const noexcept { return owner != nullptr; }
            [[nodiscard]] ::std::stop_token cancellation_token() const noexcept
            { return owner == nullptr ? ::std::stop_token{} : owner->cancellation.get_token(); }
            [[nodiscard]] bool stop_requested() const noexcept
            { return owner == nullptr || owner->stop_requested(); }
        };

        explicit execution_lifetime(::std::size_t max_concurrent) : capacity{max_concurrent} {}
        execution_lifetime(execution_lifetime const&) = delete;
        execution_lifetime& operator=(execution_lifetime const&) = delete;
        ~execution_lifetime() { request_stop(); drain(); }

        [[nodiscard]] lease try_enter()
        {
            ::std::lock_guard lock{mutex};
            if(stopping.load(::std::memory_order_relaxed) || active == capacity) { return {}; }
            ++active;
            // [live execution_lifetime]
            // [safe                   ] held alive by the owner's drain protocol
            // ^^ returned lease borrows this; only the lease releases its admission.
            return lease{this};
        }
        void request_stop()
        {
            {
                ::std::lock_guard lock{mutex};
                stopping.store(true, ::std::memory_order_release);
            }
            // Callbacks may acquire wait-shard locks or reenter admission. Never
            // invoke them while holding the execution-lifetime mutex.
            (void)cancellation.request_stop();
        }
        [[nodiscard]] bool stop_requested() const noexcept
        { return stopping.load(::std::memory_order_acquire); }
        // Cold bounded observation of ACTUAL admitted work. A timeout does
        // not release a lease, cancel a host frame or make this owner quiescent.
        [[nodiscard]] bool drain_until(::std::chrono::steady_clock::time_point deadline)
        {
            ::std::unique_lock lock{mutex};
            return idle.wait_until(lock, deadline, [&] { return active == 0uz; });
        }
        void drain()
        {
            ::std::unique_lock lock{mutex};
            idle.wait(lock, [&] { return active == 0uz; });
        }
    };
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
