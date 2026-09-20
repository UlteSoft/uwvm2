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
#include <coroutine>
#include <cstddef>
#include <memory>

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

    constexpr ::std::size_t native_task_count{64uz};

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
}  // namespace

int main()
{
    if(run_native_thread_pool_case() != 0) [[unlikely]] { return 1; }
    return 0;
}
