/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-03-26
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
# include <coroutine>
# include <atomic>
# include <cstddef>
# include <memory>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#pragma push_macro("UWVM_UTILS_THREAD_HAS_STD_ATOMIC_WAIT")
#undef UWVM_UTILS_THREAD_HAS_STD_ATOMIC_WAIT
#ifndef UWVM_UTILS_THREAD_HAS_STD_ATOMIC_WAIT
# if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L && !(defined(__APPLE__) && defined(_LIBCPP_VERSION))
#  define UWVM_UTILS_THREAD_HAS_STD_ATOMIC_WAIT 1
# endif
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::thread
{
#ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
    inline constexpr bool has_fast_io_native_thread{true};

    [[nodiscard]] inline constexpr ::std::size_t hardware_concurrency() noexcept
    {
        auto const hc{static_cast<::std::size_t>(::fast_io::native_thread::hardware_concurrency())};
        return hc == 0uz ? 1uz : hc;
    }
#else
    inline constexpr bool has_fast_io_native_thread{};
#endif

    [[nodiscard]] inline constexpr ::std::size_t clamp_extra_worker_count(::std::size_t task_count, ::std::size_t requested_extra_worker_count) noexcept
    {
        if(task_count <= 1uz || requested_extra_worker_count == 0uz) { return 0uz; }
        auto const max_useful_extra_worker_count{task_count - 1uz};
        return requested_extra_worker_count < max_useful_extra_worker_count ? requested_extra_worker_count : max_useful_extra_worker_count;
    }

    template <typename T>
    struct native_global_typed_allocator_buffer
    {
        using allocator_type = ::fast_io::native_typed_global_allocator<T>;

        T* buffer{};
        ::std::size_t buffer_count{};

        inline constexpr native_global_typed_allocator_buffer() noexcept = default;

        inline constexpr explicit native_global_typed_allocator_buffer(::std::size_t n) noexcept :
            buffer{n == 0uz ? nullptr : allocator_type::allocate(n)}, buffer_count{n}
        {
        }

        inline constexpr native_global_typed_allocator_buffer(native_global_typed_allocator_buffer const&) noexcept = delete;
        inline constexpr native_global_typed_allocator_buffer& operator= (native_global_typed_allocator_buffer const&) noexcept = delete;

        inline constexpr native_global_typed_allocator_buffer(native_global_typed_allocator_buffer&& other) noexcept :
            buffer{::std::exchange(other.buffer, nullptr)}, buffer_count{::std::exchange(other.buffer_count, 0uz)}
        {
        }

        inline constexpr native_global_typed_allocator_buffer& operator= (native_global_typed_allocator_buffer&& other) noexcept
        {
            if(this == ::std::addressof(other)) [[unlikely]] { return *this; }

            if(this->buffer) { allocator_type::deallocate_n(this->buffer, this->buffer_count); }
            this->buffer = ::std::exchange(other.buffer, nullptr);
            this->buffer_count = ::std::exchange(other.buffer_count, 0uz);

            return *this;
        }

        inline constexpr ~native_global_typed_allocator_buffer() noexcept
        {
            if(this->buffer) { allocator_type::deallocate_n(this->buffer, this->buffer_count); }
        }
    };

    struct scheduled_task
    {
        struct promise_type;
        using handle_type = ::std::coroutine_handle<promise_type>;

        struct promise_type
        {
            [[nodiscard]] inline constexpr scheduled_task get_return_object() noexcept { return scheduled_task{handle_type::from_promise(*this)}; }

            [[nodiscard]] inline static constexpr scheduled_task get_return_object_on_allocation_failure() noexcept { return {}; }

            [[nodiscard]] inline constexpr ::std::suspend_always initial_suspend() const noexcept { return {}; }

            [[nodiscard]] inline constexpr ::std::suspend_always final_suspend() const noexcept { return {}; }

            inline constexpr void return_void() const noexcept {}

            [[noreturn]] inline constexpr void unhandled_exception() const noexcept { ::fast_io::fast_terminate(); }

            [[nodiscard]] inline static void* operator new (::std::size_t n) noexcept { return ::fast_io::native_global_allocator::allocate(n); }

            inline static void operator delete (void* p) noexcept { ::fast_io::native_global_allocator::deallocate(p); }

            inline static void operator delete (void* p, ::std::size_t n) noexcept { ::fast_io::native_global_allocator::deallocate_n(p, n); }
        };

        handle_type handle{};

        inline constexpr scheduled_task() noexcept = default;

        inline constexpr explicit scheduled_task(handle_type h) noexcept : handle{h} {}

        inline constexpr scheduled_task(scheduled_task const&) noexcept = delete;
        inline constexpr scheduled_task& operator= (scheduled_task const&) noexcept = delete;

        inline constexpr scheduled_task(scheduled_task&& other) noexcept : handle{other.handle} { other.handle = {}; }

        inline constexpr scheduled_task& operator= (scheduled_task&& other) noexcept
        {
            if(this == ::std::addressof(other)) [[unlikely]] { return *this; }
            if(this->handle) { this->handle.destroy(); }
            this->handle = other.handle;
            other.handle = {};
            return *this;
        }

        inline constexpr ~scheduled_task() noexcept
        {
            if(this->handle) { this->handle.destroy(); }
        }

        [[nodiscard]] inline constexpr handle_type release() noexcept
        {
            auto const h{this->handle};
            this->handle = {};
            return h;
        }
    };

    enum class lazy_compile_state : unsigned
    {
        uncompiled,
        queued,
        compiling,
        compiled,
        failed
    };

    [[nodiscard]] inline constexpr bool lazy_compile_state_is_terminal(lazy_compile_state st) noexcept
    { return st == lazy_compile_state::compiled || st == lazy_compile_state::failed; }

    inline constexpr void lazy_compile_thread_yield() noexcept
    {
#ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
        ::fast_io::this_thread::yield();
#endif
    }

    struct lazy_compile_unit_state
    {
        ::std::atomic<lazy_compile_state> state{lazy_compile_state::uncompiled};

        inline constexpr lazy_compile_unit_state() noexcept = default;

        inline constexpr lazy_compile_unit_state(lazy_compile_unit_state const&) noexcept : state{lazy_compile_state::uncompiled} {}

        inline constexpr lazy_compile_unit_state& operator= (lazy_compile_unit_state const&) noexcept
        {
            this->state.store(lazy_compile_state::uncompiled, ::std::memory_order_relaxed);
            return *this;
        }
    };

    inline constexpr void lazy_compile_notify_unit(lazy_compile_unit_state & unit) noexcept
    {
#if defined(UWVM_UTILS_THREAD_HAS_STD_ATOMIC_WAIT)
        unit.state.notify_all();
#else
        (void)unit;
#endif
    }

    struct lazy_compile_request
    {
        using compile_callback_type = void (*)(void*) noexcept;

        lazy_compile_unit_state* unit{};
        compile_callback_type compile{};
        void* user_data{};
        unsigned priority{};
        bool owns_queued_state{true};
    };

    struct lazy_compile_scheduler;
    using lazy_compile_refill_callback_type = bool (*)(void*, lazy_compile_scheduler&) noexcept;

    struct lazy_compile_scheduler_config
    {
        ::std::size_t worker_count{};
        ::std::size_t queue_capacity{};
        lazy_compile_refill_callback_type refill_callback{};
        void* refill_user_data{};
    };

    struct lazy_compile_scheduler_stats_snapshot
    {
        ::std::size_t enqueued_requests{};
        ::std::size_t enqueue_failures{};
        ::std::size_t duplicate_requests{};
        ::std::size_t inline_compiles{};
        ::std::size_t worker_compiles{};
        ::std::size_t helper_compiles{};
        ::std::size_t passive_waits{};
        ::std::size_t worker_queue_waits{};
        ::std::size_t refill_calls{};
        ::std::size_t refill_successes{};
    };

    struct lazy_compile_scheduler
    {
        struct worker_task;

        struct ring_slot
        { lazy_compile_request request{}; };

#ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
        using native_thread_type = ::fast_io::native_thread;
#endif

        native_global_typed_allocator_buffer<ring_slot> queue{};
        ::std::size_t queue_capacity{};
        ::std::size_t queue_head{};
        ::std::atomic_flag queue_lock = ATOMIC_FLAG_INIT;
        ::std::atomic_size_t queued_count{};
        ::std::atomic_bool stop_requested{};
        ::std::atomic<unsigned> queue_epoch{};
        lazy_compile_refill_callback_type refill_callback{};
        void* refill_user_data{};

        ::std::atomic_size_t enqueued_request_count{};
        ::std::atomic_size_t enqueue_failure_count{};
        ::std::atomic_size_t duplicate_request_count{};
        ::std::atomic_size_t inline_compile_count{};
        ::std::atomic_size_t worker_compile_count{};
        ::std::atomic_size_t helper_compile_count{};
        ::std::atomic_size_t passive_wait_count{};
        ::std::atomic_size_t worker_queue_wait_count{};
        ::std::atomic_size_t refill_call_count{};
        ::std::atomic_size_t refill_success_count{};

#ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
        native_global_typed_allocator_buffer<native_thread_type> workers{};
        native_global_typed_allocator_buffer<::std::coroutine_handle<>> worker_handles{};
        ::std::size_t worker_count{};
#endif

        inline constexpr lazy_compile_scheduler() noexcept = default;
        inline constexpr lazy_compile_scheduler(lazy_compile_scheduler const&) noexcept = delete;
        inline constexpr lazy_compile_scheduler& operator= (lazy_compile_scheduler const&) noexcept = delete;

        inline constexpr lazy_compile_scheduler(lazy_compile_scheduler&&) noexcept = delete;
        inline constexpr lazy_compile_scheduler& operator= (lazy_compile_scheduler&&) noexcept = delete;

        inline constexpr ~lazy_compile_scheduler() noexcept { this->stop(); }

        [[nodiscard]] inline constexpr bool running() const noexcept
        {
#ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
            return this->worker_count != 0uz;
#else
            return false;
#endif
        }

        [[nodiscard]] inline static constexpr ::std::size_t default_queue_capacity(::std::size_t worker_count) noexcept
        {
            auto const scaled{worker_count * 64uz};
            return scaled < 256uz ? 256uz : scaled;
        }

        inline constexpr void start(lazy_compile_scheduler_config config) noexcept;

        inline constexpr void reset_stats() noexcept
        {
            this->enqueued_request_count.store(0uz, ::std::memory_order_relaxed);
            this->enqueue_failure_count.store(0uz, ::std::memory_order_relaxed);
            this->duplicate_request_count.store(0uz, ::std::memory_order_relaxed);
            this->inline_compile_count.store(0uz, ::std::memory_order_relaxed);
            this->worker_compile_count.store(0uz, ::std::memory_order_relaxed);
            this->helper_compile_count.store(0uz, ::std::memory_order_relaxed);
            this->passive_wait_count.store(0uz, ::std::memory_order_relaxed);
            this->worker_queue_wait_count.store(0uz, ::std::memory_order_relaxed);
            this->refill_call_count.store(0uz, ::std::memory_order_relaxed);
            this->refill_success_count.store(0uz, ::std::memory_order_relaxed);
        }

        [[nodiscard]] inline constexpr lazy_compile_scheduler_stats_snapshot snapshot_stats() const noexcept
        {
            return {.enqueued_requests = this->enqueued_request_count.load(::std::memory_order_relaxed),
                    .enqueue_failures = this->enqueue_failure_count.load(::std::memory_order_relaxed),
                    .duplicate_requests = this->duplicate_request_count.load(::std::memory_order_relaxed),
                    .inline_compiles = this->inline_compile_count.load(::std::memory_order_relaxed),
                    .worker_compiles = this->worker_compile_count.load(::std::memory_order_relaxed),
                    .helper_compiles = this->helper_compile_count.load(::std::memory_order_relaxed),
                    .passive_waits = this->passive_wait_count.load(::std::memory_order_relaxed),
                    .worker_queue_waits = this->worker_queue_wait_count.load(::std::memory_order_relaxed),
                    .refill_calls = this->refill_call_count.load(::std::memory_order_relaxed),
                    .refill_successes = this->refill_success_count.load(::std::memory_order_relaxed)};
        }

        inline constexpr void stop() noexcept
        {
            this->stop_requested.store(true, ::std::memory_order_release);
            this->wake_all_workers();

#ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
            for(::std::size_t i{}; i != this->worker_count; ++i)
            {
                auto& worker{this->workers.buffer[i]};
                if(worker.joinable()) { worker.join(); }
                ::std::destroy_at(this->workers.buffer + i);

                auto& handle{this->worker_handles.buffer[i]};
                if(handle)
                {
                    if(!handle.done()) [[unlikely]] { ::fast_io::fast_terminate(); }
                    handle.destroy();
                    handle = {};
                }
            }
            this->worker_count = 0uz;
            this->workers = {};
            this->worker_handles = {};
#endif

            this->drain_abandoned_requests();

            if(this->queue.buffer)
            {
                for(::std::size_t i{}; i != this->queue_capacity; ++i) { ::std::destroy_at(this->queue.buffer + i); }
            }
            this->queue = {};
            this->queue_capacity = 0uz;
            this->queued_count.store(0uz, ::std::memory_order_relaxed);
            this->refill_callback = {};
            this->refill_user_data = {};
        }

        [[nodiscard]] inline constexpr bool try_request(lazy_compile_request request) noexcept
        {
            if(request.unit == nullptr || request.compile == nullptr) [[unlikely]] { return false; }

            request.owns_queued_state = true;
            auto expected{lazy_compile_state::uncompiled};
            if(!request.unit->state.compare_exchange_strong(expected, lazy_compile_state::queued, ::std::memory_order_acq_rel, ::std::memory_order_acquire))
            {
                this->duplicate_request_count.fetch_add(1uz, ::std::memory_order_relaxed);
                return expected == lazy_compile_state::queued || expected == lazy_compile_state::compiling || expected == lazy_compile_state::compiled;
            }

            if(!this->try_enqueue(request))
            {
                this->enqueue_failure_count.fetch_add(1uz, ::std::memory_order_relaxed);
                expected = lazy_compile_state::queued;
                (void)request.unit->state.compare_exchange_strong(expected,
                                                                  lazy_compile_state::uncompiled,
                                                                  ::std::memory_order_acq_rel,
                                                                  ::std::memory_order_acquire);
                this->notify_unit(*request.unit);
                return false;
            }

            this->enqueued_request_count.fetch_add(1uz, ::std::memory_order_relaxed);
            return true;
        }

        [[nodiscard]] inline constexpr bool try_request_or_shadow_queued(lazy_compile_request request) noexcept
        {
            if(request.unit == nullptr || request.compile == nullptr) [[unlikely]] { return false; }

            auto expected{lazy_compile_state::uncompiled};
            if(request.unit->state.compare_exchange_strong(expected, lazy_compile_state::queued, ::std::memory_order_acq_rel, ::std::memory_order_acquire))
            {
                request.owns_queued_state = true;
                if(!this->try_enqueue(request))
                {
                    this->enqueue_failure_count.fetch_add(1uz, ::std::memory_order_relaxed);
                    expected = lazy_compile_state::queued;
                    (void)request.unit->state.compare_exchange_strong(expected,
                                                                      lazy_compile_state::uncompiled,
                                                                      ::std::memory_order_acq_rel,
                                                                      ::std::memory_order_acquire);
                    this->notify_unit(*request.unit);
                    return false;
                }

                this->enqueued_request_count.fetch_add(1uz, ::std::memory_order_relaxed);
                return true;
            }

            this->duplicate_request_count.fetch_add(1uz, ::std::memory_order_relaxed);
            if(expected != lazy_compile_state::queued) { return expected == lazy_compile_state::compiling || expected == lazy_compile_state::compiled; }

            request.owns_queued_state = false;
            if(!this->try_enqueue(request, true))
            {
                this->enqueue_failure_count.fetch_add(1uz, ::std::memory_order_relaxed);
                return false;
            }

            this->enqueued_request_count.fetch_add(1uz, ::std::memory_order_relaxed);
            return true;
        }

        inline constexpr void request_or_compile_inline(lazy_compile_request request) noexcept
        {
            if(this->try_request(request))
            {
                if(request.unit != nullptr) { this->wait_until_ready_passive(*request.unit); }
                return;
            }
            if(request.unit == nullptr || request.compile == nullptr) [[unlikely]] { return; }

            auto expected{lazy_compile_state::uncompiled};
            if(!request.unit->state.compare_exchange_strong(expected, lazy_compile_state::compiling, ::std::memory_order_acq_rel, ::std::memory_order_acquire))
            {
                this->wait_until_ready_passive(*request.unit);
                return;
            }

            this->inline_compile_count.fetch_add(1uz, ::std::memory_order_relaxed);
            request.compile(request.user_data);
            this->complete_request(*request.unit);
        }

        inline constexpr bool wait_until_ready_passive(lazy_compile_unit_state& unit) noexcept
        {
            bool counted_wait{};
            for(;;)
            {
                auto const st{unit.state.load(::std::memory_order_acquire)};
                if(lazy_compile_state_is_terminal(st)) { return st == lazy_compile_state::compiled; }
                if(st == lazy_compile_state::uncompiled) { return false; }
                if(!counted_wait)
                {
                    this->passive_wait_count.fetch_add(1uz, ::std::memory_order_relaxed);
                    counted_wait = true;
                }
                this->wait_for_unit_event(unit, st);
            }
        }

        [[nodiscard]] inline constexpr bool ensure_ready(lazy_compile_request request) noexcept
        {
            if(request.unit == nullptr || request.compile == nullptr) [[unlikely]] { return false; }

            if(request.priority != 0u)
            {
                for(;;)
                {
                    auto const st{request.unit->state.load(::std::memory_order_acquire)};
                    if(lazy_compile_state_is_terminal(st)) { return st == lazy_compile_state::compiled; }

                    if(st == lazy_compile_state::uncompiled)
                    {
                        auto expected{lazy_compile_state::uncompiled};
                        if(request.unit->state.compare_exchange_strong(expected,
                                                                       lazy_compile_state::compiling,
                                                                       ::std::memory_order_acq_rel,
                                                                       ::std::memory_order_acquire))
                        {
                            this->inline_compile_count.fetch_add(1uz, ::std::memory_order_relaxed);
                            request.compile(request.user_data);
                            this->complete_request(*request.unit);
                            return request.unit->state.load(::std::memory_order_acquire) == lazy_compile_state::compiled;
                        }
                        continue;
                    }

                    if(st == lazy_compile_state::queued)
                    {
                        auto expected{lazy_compile_state::queued};
                        if(request.unit->state.compare_exchange_strong(expected,
                                                                       lazy_compile_state::compiling,
                                                                       ::std::memory_order_acq_rel,
                                                                       ::std::memory_order_acquire))
                        {
                            this->inline_compile_count.fetch_add(1uz, ::std::memory_order_relaxed);
                            request.compile(request.user_data);
                            this->complete_request(*request.unit);
                            return request.unit->state.load(::std::memory_order_acquire) == lazy_compile_state::compiled;
                        }
                        continue;
                    }

                    this->wait_for_unit_event(*request.unit, st);
                }
            }

            if(this->try_request(request))
            {
                if(request.unit != nullptr && !this->wait_until_ready_passive(*request.unit)) [[unlikely]] { return false; }
                return request.unit->state.load(::std::memory_order_acquire) == lazy_compile_state::compiled;
            }

            for(;;)
            {
                auto const st{request.unit->state.load(::std::memory_order_acquire)};
                if(lazy_compile_state_is_terminal(st)) { return st == lazy_compile_state::compiled; }

                if(st == lazy_compile_state::uncompiled)
                {
                    auto expected{lazy_compile_state::uncompiled};
                    if(request.unit->state.compare_exchange_strong(expected,
                                                                   lazy_compile_state::compiling,
                                                                   ::std::memory_order_acq_rel,
                                                                   ::std::memory_order_acquire))
                    {
                        this->inline_compile_count.fetch_add(1uz, ::std::memory_order_relaxed);
                        request.compile(request.user_data);
                        this->complete_request(*request.unit);
                        return request.unit->state.load(::std::memory_order_acquire) == lazy_compile_state::compiled;
                    }
                    continue;
                }

                if(!this->wait_until_ready_passive(*request.unit)) [[unlikely]] { return false; }
            }
        }

        inline constexpr void wait_until_ready(lazy_compile_unit_state& unit) noexcept
        {
            for(;;)
            {
                auto const st{unit.state.load(::std::memory_order_acquire)};
                if(lazy_compile_state_is_terminal(st) || st == lazy_compile_state::uncompiled) { return; }

                if(st == lazy_compile_state::queued)
                {
                    this->help_or_yield_once();
                    continue;
                }

                this->wait_for_unit_event(unit, st);
            }
        }

        inline constexpr void notify_unit(lazy_compile_unit_state& unit) noexcept { lazy_compile_notify_unit(unit); }

        inline constexpr void mark_failed(lazy_compile_unit_state& unit) noexcept
        {
            unit.state.store(lazy_compile_state::failed, ::std::memory_order_release);
            this->notify_unit(unit);
        }

        inline constexpr void wake_all_workers() noexcept
        {
            this->queue_epoch.fetch_add(1u, ::std::memory_order_release);
#if defined(UWVM_UTILS_THREAD_HAS_STD_ATOMIC_WAIT)
            this->queue_epoch.notify_all();
#endif
        }

        [[nodiscard]] inline constexpr bool try_enqueue(lazy_compile_request request, bool deduplicate_unit = false) noexcept
        {
            if(this->queue_capacity == 0uz || this->stop_requested.load(::std::memory_order_acquire)) { return false; }

            this->lock_queue();

            auto const current_count{this->queued_count.load(::std::memory_order_relaxed)};
            if(deduplicate_unit && request.unit != nullptr)
            {
                for(::std::size_t i{}; i != current_count; ++i)
                {
                    auto const probe_index{(this->queue_head + i) % this->queue_capacity};
                    if(this->queue.buffer[probe_index].request.unit == request.unit)
                    {
                        this->unlock_queue();
                        return true;
                    }
                }
            }

            if(current_count >= this->queue_capacity)
            {
                this->unlock_queue();
                return false;
            }

            ::std::size_t write_index{};
            if(request.priority == 0u) { write_index = (this->queue_head + current_count) % this->queue_capacity; }
            else
            {
                ::std::size_t insert_offset{current_count};
                for(::std::size_t i{}; i != current_count; ++i)
                {
                    auto const probe_index{(this->queue_head + i) % this->queue_capacity};
                    if(this->queue.buffer[probe_index].request.priority < request.priority)
                    {
                        insert_offset = i;
                        break;
                    }
                }

                for(::std::size_t i{current_count}; i != insert_offset; --i)
                {
                    auto const dst_index{(this->queue_head + i) % this->queue_capacity};
                    auto const src_index{(this->queue_head + i - 1uz) % this->queue_capacity};
                    this->queue.buffer[dst_index].request = this->queue.buffer[src_index].request;
                }

                write_index = (this->queue_head + insert_offset) % this->queue_capacity;
            }

            this->queue.buffer[write_index].request = request;
            this->queued_count.store(current_count + 1uz, ::std::memory_order_release);
            this->unlock_queue();
            this->wake_one_worker();
            return true;
        }

        [[nodiscard]] inline constexpr bool try_dequeue(lazy_compile_request& out) noexcept
        {
            if(this->queue_capacity == 0uz) { return false; }

            this->lock_queue();

            auto const current_count{this->queued_count.load(::std::memory_order_relaxed)};
            if(current_count == 0uz)
            {
                this->unlock_queue();
                return false;
            }

            auto& slot{this->queue.buffer[this->queue_head]};
            out = slot.request;
            slot.request = {};
            this->queue_head = (this->queue_head + 1uz) % this->queue_capacity;
            this->queued_count.store(current_count - 1uz, ::std::memory_order_release);
            this->unlock_queue();
            return true;
        }

        inline constexpr void wake_one_worker() noexcept
        {
            this->queue_epoch.fetch_add(1u, ::std::memory_order_release);
#if defined(UWVM_UTILS_THREAD_HAS_STD_ATOMIC_WAIT)
            this->queue_epoch.notify_one();
#endif
        }

        inline constexpr void wait_for_queue_event(unsigned observed_epoch) noexcept
        {
#if defined(UWVM_UTILS_THREAD_HAS_STD_ATOMIC_WAIT)
            while(this->queue_epoch.load(::std::memory_order_acquire) == observed_epoch && !this->stop_requested.load(::std::memory_order_acquire))
            {
                this->queue_epoch.wait(observed_epoch, ::std::memory_order_acquire);
            }
#else
            while(this->queue_epoch.load(::std::memory_order_acquire) == observed_epoch && !this->stop_requested.load(::std::memory_order_acquire))
            {
                lazy_compile_thread_yield();
            }
#endif
        }

        inline constexpr void wait_for_unit_event(lazy_compile_unit_state& unit, lazy_compile_state observed_state) noexcept
        {
#if defined(UWVM_UTILS_THREAD_HAS_STD_ATOMIC_WAIT)
            while(unit.state.load(::std::memory_order_acquire) == observed_state) { unit.state.wait(observed_state, ::std::memory_order_acquire); }
#else
            while(unit.state.load(::std::memory_order_acquire) == observed_state) { lazy_compile_thread_yield(); }
#endif
        }

        inline constexpr void help_or_yield_once() noexcept
        {
            lazy_compile_request request{};
            if(this->try_dequeue(request))
            {
                this->execute_request(request, false);
                return;
            }
            lazy_compile_thread_yield();
        }

        inline constexpr void complete_request(lazy_compile_unit_state& unit) noexcept
        {
            auto expected{lazy_compile_state::compiling};
            if(!unit.state.compare_exchange_strong(expected, lazy_compile_state::compiled, ::std::memory_order_release, ::std::memory_order_acquire))
            {
                if(expected != lazy_compile_state::failed) { unit.state.store(lazy_compile_state::compiled, ::std::memory_order_release); }
            }
            this->notify_unit(unit);
        }

        inline constexpr void execute_request(lazy_compile_request request, bool worker_thread) noexcept
        {
            if(request.unit == nullptr || request.compile == nullptr) [[unlikely]] { return; }

            auto expected{lazy_compile_state::queued};
            if(!request.unit->state.compare_exchange_strong(expected, lazy_compile_state::compiling, ::std::memory_order_acq_rel, ::std::memory_order_acquire))
            {
                return;
            }

            if(worker_thread) { this->worker_compile_count.fetch_add(1uz, ::std::memory_order_relaxed); }
            else
            {
                this->helper_compile_count.fetch_add(1uz, ::std::memory_order_relaxed);
            }
            request.compile(request.user_data);
            this->complete_request(*request.unit);
        }

        [[nodiscard]] inline constexpr bool try_refill_background_work() noexcept
        {
            if(this->refill_callback == nullptr || this->stop_requested.load(::std::memory_order_acquire)) { return false; }
            this->refill_call_count.fetch_add(1uz, ::std::memory_order_relaxed);
            if(this->refill_callback(this->refill_user_data, *this))
            {
                this->refill_success_count.fetch_add(1uz, ::std::memory_order_relaxed);
                return true;
            }
            return false;
        }

        inline constexpr void drain_abandoned_requests() noexcept
        {
            lazy_compile_request request{};
            while(this->try_dequeue(request))
            {
                if(request.unit == nullptr) { continue; }
                if(!request.owns_queued_state) { continue; }
                auto expected{lazy_compile_state::queued};
                (void)request.unit->state.compare_exchange_strong(expected,
                                                                  lazy_compile_state::uncompiled,
                                                                  ::std::memory_order_acq_rel,
                                                                  ::std::memory_order_acquire);
                this->notify_unit(*request.unit);
            }
        }

        inline constexpr void lock_queue() noexcept
        {
            while(this->queue_lock.test_and_set(::std::memory_order_acquire))
            {
#if defined(UWVM_UTILS_THREAD_HAS_STD_ATOMIC_WAIT)
                this->queue_lock.wait(true, ::std::memory_order_acquire);
#else
                lazy_compile_thread_yield();
#endif
            }
        }

        inline constexpr void unlock_queue() noexcept
        {
            this->queue_lock.clear(::std::memory_order_release);
#if defined(UWVM_UTILS_THREAD_HAS_STD_ATOMIC_WAIT)
            this->queue_lock.notify_one();
#endif
        }

        inline worker_task make_worker_task() noexcept;
    };

    struct lazy_compile_scheduler::worker_task
    {
        struct promise_type;
        using handle_type = ::std::coroutine_handle<promise_type>;

        struct promise_type
        {
            [[nodiscard]] inline constexpr worker_task get_return_object() noexcept { return worker_task{handle_type::from_promise(*this)}; }

            [[nodiscard]] inline static constexpr worker_task get_return_object_on_allocation_failure() noexcept { return {}; }

            [[nodiscard]] inline constexpr ::std::suspend_always initial_suspend() const noexcept { return {}; }

            [[nodiscard]] inline constexpr ::std::suspend_always final_suspend() const noexcept { return {}; }

            inline constexpr void return_void() const noexcept {}

            [[noreturn]] inline constexpr void unhandled_exception() const noexcept { ::fast_io::fast_terminate(); }

            [[nodiscard]] inline static void* operator new (::std::size_t n) noexcept { return ::fast_io::native_global_allocator::allocate(n); }

            inline static void operator delete (void* p) noexcept { ::fast_io::native_global_allocator::deallocate(p); }

            inline static void operator delete (void* p, ::std::size_t n) noexcept { ::fast_io::native_global_allocator::deallocate_n(p, n); }
        };

        handle_type handle{};

        inline constexpr worker_task() noexcept = default;

        inline constexpr explicit worker_task(handle_type h) noexcept : handle{h} {}

        inline constexpr worker_task(worker_task const&) noexcept = delete;
        inline constexpr worker_task& operator= (worker_task const&) noexcept = delete;

        inline constexpr worker_task(worker_task&& other) noexcept : handle{other.handle} { other.handle = {}; }

        inline constexpr worker_task& operator= (worker_task&& other) noexcept
        {
            if(this == ::std::addressof(other)) [[unlikely]] { return *this; }
            if(this->handle) { this->handle.destroy(); }
            this->handle = other.handle;
            other.handle = {};
            return *this;
        }

        inline constexpr ~worker_task() noexcept
        {
            if(this->handle) { this->handle.destroy(); }
        }

        [[nodiscard]] inline constexpr handle_type release() noexcept
        {
            auto const h{this->handle};
            this->handle = {};
            return h;
        }
    };

    inline lazy_compile_scheduler::worker_task lazy_compile_scheduler::make_worker_task() noexcept
    {
        for(;;)
        {
            if(this->stop_requested.load(::std::memory_order_acquire)) { break; }

            lazy_compile_request request{};
            if(this->try_dequeue(request))
            {
                this->execute_request(request, true);
                continue;
            }

            if(this->try_refill_background_work()) { continue; }

            auto const observed_epoch{this->queue_epoch.load(::std::memory_order_acquire)};
            if(this->queued_count.load(::std::memory_order_acquire) == 0uz && !this->stop_requested.load(::std::memory_order_acquire))
            {
                this->worker_queue_wait_count.fetch_add(1uz, ::std::memory_order_relaxed);
                this->wait_for_queue_event(observed_epoch);
            }
        }
        co_return;
    }

    inline constexpr void lazy_compile_scheduler::start(lazy_compile_scheduler_config config) noexcept
    {
        this->stop();

#ifndef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
        this->reset_stats();
        this->refill_callback = config.refill_callback;
        this->refill_user_data = config.refill_user_data;
        return;
#else
        if(config.worker_count == 0uz)
        {
            this->reset_stats();
            this->refill_callback = config.refill_callback;
            this->refill_user_data = config.refill_user_data;
            return;
        }
        if(config.queue_capacity == 0uz) { config.queue_capacity = default_queue_capacity(config.worker_count); }

        this->queue = native_global_typed_allocator_buffer<ring_slot>{config.queue_capacity};
        for(::std::size_t i{}; i != config.queue_capacity; ++i) { ::std::construct_at(this->queue.buffer + i); }

        this->queue_capacity = config.queue_capacity;
        this->queue_head = 0uz;
        this->queue_lock.clear(::std::memory_order_release);
        this->queued_count.store(0uz, ::std::memory_order_relaxed);
        this->stop_requested.store(false, ::std::memory_order_relaxed);
        this->queue_epoch.store(0u, ::std::memory_order_relaxed);
        this->refill_callback = config.refill_callback;
        this->refill_user_data = config.refill_user_data;
        this->reset_stats();

        this->workers = native_global_typed_allocator_buffer<native_thread_type>{config.worker_count};
        this->worker_handles = native_global_typed_allocator_buffer<::std::coroutine_handle<>>{config.worker_count};

# ifdef UWVM_CPP_EXCEPTIONS
        try
# endif
        {
            while(this->worker_count != config.worker_count)
            {
                auto task{this->make_worker_task()};
                auto handle{task.release()};
                if(!handle) [[unlikely]] { ::fast_io::fast_terminate(); }

# ifdef UWVM_CPP_EXCEPTIONS
                try
# endif
                {
                    ::std::construct_at(this->workers.buffer + this->worker_count, native_thread_type{[handle]() noexcept { handle.resume(); }});
                }
# ifdef UWVM_CPP_EXCEPTIONS
                catch(...)
                {
                    handle.destroy();
                    throw;
                }
# endif

                ::std::construct_at(this->worker_handles.buffer + this->worker_count, handle);
                ++this->worker_count;
            }
        }
# ifdef UWVM_CPP_EXCEPTIONS
        catch(...)
        {
            this->stop();
        }
# endif
#endif
    }

    struct scheduled_task_batch
    {
        using handle_type = ::std::coroutine_handle<>;

        native_global_typed_allocator_buffer<handle_type> handles{};
        ::std::size_t handle_count{};

        inline constexpr scheduled_task_batch() noexcept = default;

        inline constexpr explicit scheduled_task_batch(::std::size_t n) noexcept : handles{n} {}

        inline constexpr scheduled_task_batch(scheduled_task_batch const&) noexcept = delete;
        inline constexpr scheduled_task_batch& operator= (scheduled_task_batch const&) noexcept = delete;

        inline constexpr scheduled_task_batch(scheduled_task_batch&& other) noexcept :
            handles{::std::move(other.handles)}, handle_count{::std::exchange(other.handle_count, 0uz)}
        {
        }

        inline constexpr scheduled_task_batch& operator= (scheduled_task_batch&& other) noexcept
        {
            if(this == ::std::addressof(other)) [[unlikely]] { return *this; }
            this->clear();
            this->handles = ::std::move(other.handles);
            this->handle_count = ::std::exchange(other.handle_count, 0uz);
            return *this;
        }

        inline constexpr ~scheduled_task_batch() noexcept { this->clear(); }

        [[nodiscard]] inline constexpr bool empty() const noexcept { return this->handle_count == 0uz; }

        [[nodiscard]] inline constexpr ::std::size_t size() const noexcept { return this->handle_count; }

        inline constexpr void resume_and_destroy(::std::size_t index) noexcept
        {
            auto& handle{this->handles.buffer[index]};
            if(!handle) [[unlikely]] { return; }
            handle.resume();
            // This scheduler only supports one-shot tasks: the first resume must reach final_suspend().
            if(!handle.done()) [[unlikely]] { ::fast_io::fast_terminate(); }
            handle.destroy();
            handle = {};
        }

        inline constexpr void resume_all_serial() noexcept
        {
            for(::std::size_t i{}; i != this->handle_count; ++i) { this->resume_and_destroy(i); }
        }

        inline constexpr void clear() noexcept
        {
            for(::std::size_t i{}; i != this->handle_count; ++i)
            {
                auto& handle{this->handles.buffer[i]};
                if(handle) { handle.destroy(); }
            }
            this->handle_count = 0uz;
        }
    };

    struct native_thread_pool
    {
#ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
        using native_thread_type = ::fast_io::native_thread;
        native_global_typed_allocator_buffer<native_thread_type> workers{};
        ::std::size_t worker_count{};
#endif

        inline constexpr native_thread_pool() noexcept = default;
        inline constexpr native_thread_pool(native_thread_pool const&) noexcept = delete;
        inline constexpr native_thread_pool& operator= (native_thread_pool const&) noexcept = delete;

        inline constexpr native_thread_pool([[maybe_unused]] native_thread_pool&& other) noexcept
#ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
            : workers{::std::move(other.workers)}, worker_count{::std::exchange(other.worker_count, 0uz)}
#endif
        {
        }

        inline constexpr native_thread_pool& operator= (native_thread_pool&& other) noexcept
        {
            if(this == ::std::addressof(other)) [[unlikely]] { return *this; }
            this->join_all();
#ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
            this->workers = ::std::move(other.workers);
            this->worker_count = ::std::exchange(other.worker_count, 0uz);
#endif
            return *this;
        }

        inline constexpr ~native_thread_pool() noexcept { this->join_all(); }

        inline constexpr void join_all() noexcept
        {
#ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
            for(::std::size_t i{}; i != this->worker_count; ++i)
            {
                auto& worker{this->workers.buffer[i]};
                if(worker.joinable()) { worker.join(); }
                ::std::destroy_at(this->workers.buffer + i);
            }
            this->worker_count = 0uz;
#endif
        }

        inline constexpr void run(scheduled_task_batch& task_batch, ::std::size_t extra_worker_count) UWVM_THROWS
        {
            if(task_batch.empty()) { return; }

#ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
            extra_worker_count = clamp_extra_worker_count(task_batch.size(), extra_worker_count);
            if(extra_worker_count == 0uz)
            {
                task_batch.resume_all_serial();
                return;
            }

            ::std::atomic_size_t next_task_index{};

            auto const run_worker{[&task_batch, &next_task_index]() noexcept
                                  {
                                      for(;;)
                                      {
                                          auto const task_index{next_task_index.fetch_add(1uz, ::std::memory_order_acq_rel)};
                                          if(task_index >= task_batch.size()) { break; }
                                          task_batch.resume_and_destroy(task_index);
                                      }
                                  }};

            this->join_all();
            this->workers = native_global_typed_allocator_buffer<native_thread_type>{extra_worker_count};

# ifdef UWVM_CPP_EXCEPTIONS
            try
# endif
            {
                for(; this->worker_count != extra_worker_count; ++this->worker_count)
                {
                    ::std::construct_at(this->workers.buffer + this->worker_count, native_thread_type{run_worker});
                }
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(...)
            {
                run_worker();
                this->join_all();
                task_batch.resume_all_serial();
                return;
            }
# endif

            run_worker();
            this->join_all();
#else
            (void)extra_worker_count;
            task_batch.resume_all_serial();
#endif
        }
    };
}

#pragma pop_macro("UWVM_UTILS_THREAD_HAS_STD_ATOMIC_WAIT")

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
