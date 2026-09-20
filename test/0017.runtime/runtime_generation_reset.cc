/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#include <atomic>
#include <cstdint>
#include <thread>

#include <uwvm2/runtime/lib/uwvm_runtime_generation.h>

int main()
{
    using ::uwvm2::runtime::lib::details::call_indirect_cache_generation_matches;
    using ::uwvm2::runtime::lib::details::runtime_generation_epoch;

    runtime_generation_epoch epoch{};
    ::std::atomic_uint phase{};
    runtime_generation_epoch::value_type cached_generation{};
    bool stale_cache_rejected{};

    ::std::thread cache_owner{[&]
    {
        // This remains the same live thread across the simulated reset/reload,
        // matching the TLS lifetime that originally allowed a stale cache hit.
        thread_local runtime_generation_epoch::value_type tls_cached_generation{};
        tls_cached_generation = epoch.current();
        cached_generation = tls_cached_generation;
        phase.store(1u, ::std::memory_order_release);
        while(phase.load(::std::memory_order_acquire) != 2u) { ::std::this_thread::yield(); }
        stale_cache_rejected = !call_indirect_cache_generation_matches(tls_cached_generation, epoch.current());
    }};

    while(phase.load(::std::memory_order_acquire) != 1u) { ::std::this_thread::yield(); }
    int result{};
    auto const before_reset{epoch.current()};
    if(cached_generation != before_reset || !call_indirect_cache_generation_matches(cached_generation, before_reset)) { result = 1; }

    // reset_runtime_state_host_api uses this exact operation before dropping
    // registry storage, so any subsequently reused address belongs to a new epoch.
    if(!epoch.advance()) { result = 2; }
    auto const after_reset{epoch.current()};
    if(after_reset == before_reset || call_indirect_cache_generation_matches(cached_generation, after_reset)) { result = 3; }

    phase.store(2u, ::std::memory_order_release);
    cache_owner.join();
    if(!stale_cache_rejected) { result = 4; }
    if(!call_indirect_cache_generation_matches(after_reset, after_reset)) { result = 5; }
    if(call_indirect_cache_generation_matches(0u, after_reset)) { result = 6; }
    return result;
}
