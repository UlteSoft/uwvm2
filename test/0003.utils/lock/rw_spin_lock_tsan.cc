/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
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
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>
#include <atomic>

// macro
#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
// import
# include <uwvm2/utils/mutex/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{

    namespace mylock
    {
        using rwlock_t = ::uwvm2::utils::mutex::rwlock_t;

        using rw_shared_guard_t = ::uwvm2::utils::mutex::rw_shared_guard_t;
        using rw_unique_guard_t = ::uwvm2::utils::mutex::rw_unique_guard_t;

        using rw_writer_pref_shared_guard_t = ::uwvm2::utils::mutex::rw_writer_pref_shared_guard_t;
        using rw_writer_pref_unique_guard_t = ::uwvm2::utils::mutex::rw_writer_pref_unique_guard_t;

        using rw_writer_pref_no_starvation_shared_guard_t = ::uwvm2::utils::mutex::rw_writer_pref_no_starvation_shared_guard_t;
        using rw_writer_pref_no_starvation_unique_guard_t = ::uwvm2::utils::mutex::rw_writer_pref_no_starvation_unique_guard_t;

        using rw_fair_shared_guard_t = ::uwvm2::utils::mutex::rw_fair_shared_guard_t;
        using rw_fair_unique_guard_t = ::uwvm2::utils::mutex::rw_fair_unique_guard_t;

        using rw_fair_writer_pref_shared_guard_t = ::uwvm2::utils::mutex::rw_fair_writer_pref_shared_guard_t;
        using rw_fair_writer_pref_unique_guard_t = ::uwvm2::utils::mutex::rw_fair_writer_pref_unique_guard_t;
    }  // namespace mylock

    template <typename SharedGuard, typename UniqueGuard>
    void run_rw_lock_scenario()
    {
        mylock::rwlock_t lock{};
        std::atomic<std::uint64_t> counter{0};

        constexpr std::size_t thread_count{4};
        constexpr std::size_t iters_per_thread{1000};

        auto worker = [&](unsigned tid)
        {
            for(std::size_t i{}; i < iters_per_thread; ++i)
            {
                // 0.5 / 0.5 read/write ratio per thread.
                if(((i + tid) & 1u) == 0u)
                {
                    SharedGuard g{lock};
                    (void)counter.load(std::memory_order_relaxed);
                }
                else
                {
                    UniqueGuard g{lock};
                    counter.fetch_add(1u, std::memory_order_relaxed);
                }
            }
        };

        std::vector<std::thread> threads;
        threads.reserve(thread_count);

        for(std::size_t i{}; i < thread_count; ++i) { threads.emplace_back(worker, static_cast<unsigned>(i)); }
        for(auto& t: threads) { t.join(); }
    }

}  // namespace

int main()
{
    // Exercise all rwlock_t variants (excluding folly) under a 50/50 read/write mix.

    // Reader-preferred (non-fair).
    run_rw_lock_scenario<mylock::rw_shared_guard_t, mylock::rw_unique_guard_t>();

    // Writer-preferred (non-fair).
    run_rw_lock_scenario<mylock::rw_writer_pref_shared_guard_t, mylock::rw_writer_pref_unique_guard_t>();

    // Writer-preferred, writer-starvation-free.
    run_rw_lock_scenario<mylock::rw_writer_pref_no_starvation_shared_guard_t, mylock::rw_writer_pref_no_starvation_unique_guard_t>();

    // Fair (phase-fair, writer-preferred).
    run_rw_lock_scenario<mylock::rw_fair_shared_guard_t, mylock::rw_fair_unique_guard_t>();

    // Fair, writer-biased.
    run_rw_lock_scenario<mylock::rw_fair_writer_pref_shared_guard_t, mylock::rw_fair_writer_pref_unique_guard_t>();

    return 0;
}

