/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      OpenAI
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

// std
#include <array>
#include <atomic>
#include <chrono>
#include <coroutine>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

// macro
#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
// import
# include <uwvm2/utils/thread/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    namespace thread_utils = ::uwvm2::utils::thread;
    using lazy_compile_state_t = thread_utils::lazy_compile_state;

    constexpr ::std::size_t native_task_count{64uz};
    constexpr ::std::size_t scheduler_unit_count{32uz};
    constexpr ::std::size_t submitter_count{8uz};
    constexpr ::std::size_t refill_unit_count{24uz};

    [[nodiscard]] thread_utils::scheduled_task make_counter_task(::std::atomic_size_t* hits,
                                                                 ::std::atomic_size_t& total,
                                                                 ::std::size_t index) noexcept
    {
        hits[index].fetch_add(1uz, ::std::memory_order_relaxed);
        total.fetch_add(1uz, ::std::memory_order_relaxed);
        co_return;
    }

    [[nodiscard]] int run_native_thread_pool_case()
    {
        ::std::array<::std::atomic_size_t, native_task_count> hits{};
        ::std::atomic_size_t total{};
        thread_utils::scheduled_task_batch task_batch{native_task_count};

        for(::std::size_t i{}; i != native_task_count; ++i)
        {
            auto task{make_counter_task(hits.data(), total, i)};
            ::std::construct_at(task_batch.handles.buffer + task_batch.handle_count, task.release());
            ++task_batch.handle_count;
        }

        thread_utils::native_thread_pool thread_pool{};
        thread_pool.run(task_batch, 8uz);

        if(total.load(::std::memory_order_relaxed) != native_task_count) [[unlikely]] { return 1; }
        for(auto const& hit: hits)
        {
            if(hit.load(::std::memory_order_relaxed) != 1uz) [[unlikely]] { return 2; }
        }

        return 0;
    }

    struct compile_payload
    {
        thread_utils::lazy_compile_unit_state unit{};
        ::std::atomic_uint compile_count{};
    };

    void compile_payload_callback(void* opaque) noexcept
    {
        auto& payload{*static_cast<compile_payload*>(opaque)};
        payload.compile_count.fetch_add(1u, ::std::memory_order_acq_rel);

        for(::std::size_t spin{}; spin != 256uz; ++spin)
        {
            ::std::atomic_signal_fence(::std::memory_order_seq_cst);
            if((spin & 63uz) == 0uz) { thread_utils::lazy_compile_thread_yield(); }
        }
    }

    [[nodiscard]] thread_utils::lazy_compile_request make_compile_request(compile_payload& payload, unsigned priority) noexcept
    {
        return {.unit = ::std::addressof(payload.unit),
                .compile = compile_payload_callback,
                .user_data = ::std::addressof(payload),
                .priority = priority};
    }

    [[nodiscard]] bool all_payloads_compiled(compile_payload const* payloads, ::std::size_t payload_count) noexcept
    {
        for(::std::size_t i{}; i != payload_count; ++i)
        {
            auto const state{payloads[i].unit.state.load(::std::memory_order_acquire)};
            if(state != lazy_compile_state_t::compiled) { return false; }
            if(payloads[i].compile_count.load(::std::memory_order_acquire) != 1u) { return false; }
        }
        return true;
    }

    [[nodiscard]] int run_lazy_scheduler_duplicate_case()
    {
        ::std::array<compile_payload, scheduler_unit_count> payloads{};
        thread_utils::lazy_compile_scheduler scheduler{};
        scheduler.start({.worker_count = thread_utils::has_fast_io_native_thread ? 4uz : 0uz, .queue_capacity = 16uz});

        ::std::atomic_bool start{};
        ::std::vector<::std::thread> submitters{};
        submitters.reserve(submitter_count);

        for(::std::size_t tid{}; tid != submitter_count; ++tid)
        {
            submitters.emplace_back([&scheduler, &payloads, &start, tid]() noexcept
                                    {
                                        while(!start.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }

                                        for(::std::size_t round{}; round != 4uz; ++round)
                                        {
                                            for(::std::size_t i{}; i != scheduler_unit_count; ++i)
                                            {
                                                auto& payload{payloads[(i + tid) % scheduler_unit_count]};
                                                auto const priority{((round + i + tid) % 7uz) == 0uz ? 1u : 0u};
                                                (void)scheduler.ensure_ready(make_compile_request(payload, priority));
                                            }
                                        }
                                    });
        }

        start.store(true, ::std::memory_order_release);
        for(auto& submitter: submitters) { submitter.join(); }

        auto const stats{scheduler.snapshot_stats()};
        scheduler.stop();

        if(!all_payloads_compiled(payloads.data(), payloads.size())) [[unlikely]] { return 1; }
        if(stats.duplicate_requests == 0uz) [[unlikely]] { return 2; }
        if(stats.inline_compiles + stats.worker_compiles + stats.helper_compiles != scheduler_unit_count) [[unlikely]] { return 3; }

        return 0;
    }

    struct refill_context
    {
        ::std::array<compile_payload, refill_unit_count>* payloads{};
        ::std::atomic_size_t next_index{};
    };

    [[nodiscard]] bool refill_callback(void* opaque, thread_utils::lazy_compile_scheduler& scheduler) noexcept
    {
        auto& context{*static_cast<refill_context*>(opaque)};
        auto const index{context.next_index.fetch_add(1uz, ::std::memory_order_acq_rel)};
        if(index >= refill_unit_count) { return false; }

        auto& payload{(*context.payloads)[index]};
        return scheduler.try_request(make_compile_request(payload, static_cast<unsigned>(index & 1uz)));
    }

    [[nodiscard]] int run_lazy_scheduler_refill_case()
    {
        ::std::array<compile_payload, refill_unit_count> payloads{};
        refill_context context{.payloads = ::std::addressof(payloads)};

        thread_utils::lazy_compile_scheduler scheduler{};
        scheduler.start({.worker_count = thread_utils::has_fast_io_native_thread ? 2uz : 0uz,
                         .queue_capacity = refill_unit_count + 4uz,
                         .refill_callback = refill_callback,
                         .refill_user_data = ::std::addressof(context)});

        if constexpr(thread_utils::has_fast_io_native_thread)
        {
            auto const deadline{::std::chrono::steady_clock::now() + ::std::chrono::seconds{10}};
            while(::std::chrono::steady_clock::now() < deadline)
            {
                if(context.next_index.load(::std::memory_order_acquire) >= refill_unit_count &&
                   all_payloads_compiled(payloads.data(), payloads.size()))
                {
                    break;
                }
                ::std::this_thread::yield();
            }
        }
        else
        {
            while(scheduler.try_refill_background_work()) {}
            for(auto& payload: payloads) { (void)scheduler.ensure_ready(make_compile_request(payload, 0u)); }
        }

        auto const stats{scheduler.snapshot_stats()};
        scheduler.stop();

        if(context.next_index.load(::std::memory_order_acquire) < refill_unit_count) [[unlikely]] { return 1; }
        if(!all_payloads_compiled(payloads.data(), payloads.size())) [[unlikely]] { return 2; }
        if(stats.refill_successes != refill_unit_count) [[unlikely]] { return 3; }

        return 0;
    }
}  // namespace

int main()
{
    if(run_native_thread_pool_case() != 0) [[unlikely]] { return 1; }
    if(run_lazy_scheduler_duplicate_case() != 0) [[unlikely]] { return 2; }
    if(run_lazy_scheduler_refill_case() != 0) [[unlikely]] { return 3; }

    return 0;
}
