/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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
        inline explicit native_global_typed_allocator_buffer(::std::size_t n) noexcept : buffer{n == 0uz ? nullptr : allocator_type::allocate(n)}, buffer_count{n} {}
        inline constexpr native_global_typed_allocator_buffer(native_global_typed_allocator_buffer const&) noexcept = delete;
        inline constexpr native_global_typed_allocator_buffer& operator= (native_global_typed_allocator_buffer const&) noexcept = delete;

        inline constexpr native_global_typed_allocator_buffer(native_global_typed_allocator_buffer&& other) noexcept :
            buffer{::std::exchange(other.buffer, nullptr)},
            buffer_count{::std::exchange(other.buffer_count, 0uz)}
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

            [[nodiscard]] inline constexpr ::std::suspend_always initial_suspend() const noexcept { return {}; }

            [[nodiscard]] inline constexpr ::std::suspend_always final_suspend() const noexcept { return {}; }

            inline constexpr void return_void() const noexcept {}

            [[noreturn]] inline void unhandled_exception() const noexcept { ::fast_io::fast_terminate(); }

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

    struct scheduled_task_batch
    {
        using handle_type = ::std::coroutine_handle<>;

        native_global_typed_allocator_buffer<handle_type> handles{};
        ::std::size_t handle_count{};

        inline constexpr scheduled_task_batch() noexcept = default;
        inline explicit scheduled_task_batch(::std::size_t n) noexcept : handles{n} {}
        inline constexpr scheduled_task_batch(scheduled_task_batch const&) noexcept = delete;
        inline constexpr scheduled_task_batch& operator= (scheduled_task_batch const&) noexcept = delete;

        inline constexpr scheduled_task_batch(scheduled_task_batch&& other) noexcept :
            handles{::std::move(other.handles)},
            handle_count{::std::exchange(other.handle_count, 0uz)}
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

        inline constexpr native_thread_pool(native_thread_pool&& other) noexcept
#ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
            : workers{::std::move(other.workers)},
              worker_count{::std::exchange(other.worker_count, 0uz)}
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

        inline void join_all() noexcept
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

        inline void run(scheduled_task_batch& task_batch, ::std::size_t extra_worker_count) UWVM_THROWS
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

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
