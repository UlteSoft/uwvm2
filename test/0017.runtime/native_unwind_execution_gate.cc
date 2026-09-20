#include <uwvm2/runtime/lib/uwvm_runtime_native_unwind_execution_gate.h>

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

int main()
{
    using gate_type = ::uwvm2::runtime::lib::details::native_unwind_execution_gate;

    gate_type gate{};
    {
        auto disabled{gate.enter_if(false)};
        if(disabled.owns_lock()) { return 1; }
    }
    {
        // Imported host code may re-enter generated code on the same thread.
        auto outer{gate.enter_if(true)};
        auto inner{gate.enter_if(true)};
        if(!outer.owns_lock() || !inner.owns_lock()) { return 2; }
    }

    constexpr ::std::size_t thread_count{4uz};
    constexpr ::std::size_t iterations{4096uz};
    ::std::atomic_size_t ready{};
    ::std::atomic_bool start{};
    ::std::atomic_size_t active{};
    ::std::atomic_size_t overlaps{};
    ::std::vector<::std::thread> workers{};
    workers.reserve(thread_count);

    for(::std::size_t thread_index{}; thread_index != thread_count; ++thread_index)
    {
        workers.emplace_back([&]
                             {
                                 ready.fetch_add(1uz, ::std::memory_order_release);
                                 while(!start.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }

                                 for(::std::size_t iteration{}; iteration != iterations; ++iteration)
                                 {
                                     auto exclusive{gate.enter_if(true)};
                                     if(active.fetch_add(1uz, ::std::memory_order_acq_rel) != 0uz)
                                     {
                                         overlaps.fetch_add(1uz, ::std::memory_order_relaxed);
                                     }
                                     ::std::this_thread::yield();
                                     active.fetch_sub(1uz, ::std::memory_order_acq_rel);
                                 }
                             });
    }

    while(ready.load(::std::memory_order_acquire) != thread_count) { ::std::this_thread::yield(); }
    start.store(true, ::std::memory_order_release);
    for(auto& worker: workers) { worker.join(); }

    return overlaps.load(::std::memory_order_relaxed) == 0uz && active.load(::std::memory_order_relaxed) == 0uz ? 0 : 3;
}
