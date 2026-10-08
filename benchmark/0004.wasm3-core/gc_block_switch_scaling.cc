// Native A/B workload for GC token-block registration when a worker switches
// between many stores on every allocation. This is not a Wasm or GC-collector
// throughput result. Formal measurements run in the Linux 64 GiB cgroup.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <fast_io.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#if defined(__linux__)
extern "C" pthread_t bench_pthread_self_noexcept() noexcept asm("pthread_self");
extern "C" int bench_pthread_setaffinity_np_noexcept(pthread_t, size_t, cpu_set_t const*) noexcept
    asm("pthread_setaffinity_np");
#endif

namespace gc_type = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace gc = ::uwvm2::uwvm::runtime::storage;

namespace
{
    constexpr unsigned p_cores[]{0u, 2u, 4u, 6u};
    constexpr ::std::uint32_t total_allocations{1000000u};
    constexpr ::std::size_t stores_per_worker{64uz};

    gc_type::recursive_type_section make_types()
    {
        gc_type::recursive_type_section section{};
        section.type_count = 1u;
        gc_type::recursive_group group{};
        group.first_type_index = 0u;
        gc_type::sub_type structure{};
        structure.kind = gc_type::composite_kind::struct_;
        gc_type::field_type field{};
        field.storage.value.kind = gc_type::value_kind::i32;
        field.mutable_ = true;
        structure.fields.push_back(field);
        group.types.push_back(::std::move(structure));
        section.groups.push_back(::std::move(group));
        return section;
    }

    struct token_range
    {
        ::std::uintptr_t first{};
        ::std::uintptr_t last{};
    };

    void allocate_worker(::std::vector<::std::shared_ptr<gc::gc_object_store>> const& stores,
                         ::std::vector<gc::gc_reference>& first_refs,
                         ::std::vector<gc::gc_reference>& last_refs,
                         token_range& range, unsigned worker, unsigned workers,
                         ::std::atomic<unsigned>& ready, ::std::atomic<bool>& start,
                         ::std::atomic<unsigned>& failures) noexcept
    {
#if defined(__linux__)
        cpu_set_t set{};
        CPU_ZERO(&set);
        CPU_SET(p_cores[worker], &set);
        if(bench_pthread_setaffinity_np_noexcept(bench_pthread_self_noexcept(), sizeof(set), &set) != 0)
        { failures.fetch_add(1u, ::std::memory_order_relaxed); }
#endif
        ready.fetch_add(1u, ::std::memory_order_release);
        while(!start.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
        auto const base{static_cast<::std::size_t>(worker) * stores_per_worker};
        auto const iterations{total_allocations / workers};
        for(::std::uint32_t i{}; i != iterations; ++i)
        {
            auto const index{base + (i & (stores_per_worker - 1uz))};
            gc::gc_reference reference{};
            if(stores[index]->struct_new_default(0u, reference) != gc::gc_object_status::ok)
            {
                failures.fetch_add(1u, ::std::memory_order_relaxed);
                return;
            }
            // [0, stores.size()) this worker alone writes its 64 reference slots.
            if(i < stores_per_worker) { first_refs[index] = reference; }
            last_refs[index] = reference;
            auto const token{reinterpret_cast<::std::uintptr_t>(reference.storage.ptr)};
            if(i == 0u) { range.first = token; }
            range.last = token;
        }
    }
}

int main(int argc, char** argv)
{
    if(argc != 2) { ::fast_io::io::println("usage: gc_block_switch_scaling one|four"); return 2; }
    // argv[1] is an OS-provided NUL-terminated string; this code does not advance its pointer.
    ::std::string_view const mode{argv[1]};
    unsigned workers{};
    if(mode == "one") { workers = 1u; }
    else if(mode == "four") { workers = 4u; }
    else { ::fast_io::io::println("unknown worker mode"); return 2; }
    auto const types{make_types()};
    auto const store_count{static_cast<::std::size_t>(workers) * stores_per_worker};
    ::std::vector<::std::shared_ptr<gc::gc_object_store>> stores{};
    stores.reserve(store_count);
    for(::std::size_t i{}; i != store_count; ++i)
    {
        stores.push_back(::std::make_shared<gc::gc_object_store>(types));
        if(!stores.back()->valid()) { ::fast_io::io::println("store setup failed"); return 3; }
    }
    ::std::vector<gc::gc_reference> first_refs(store_count);
    ::std::vector<gc::gc_reference> last_refs(store_count);
    ::std::array<token_range, 4uz> ranges{};
    ::std::atomic<unsigned> ready{};
    ::std::atomic<bool> start{};
    ::std::atomic<unsigned> failures{};
    ::std::vector<::std::thread> threads{};
    threads.reserve(workers);
    for(unsigned worker{}; worker != workers; ++worker)
    {
        threads.emplace_back(allocate_worker, ::std::cref(stores), ::std::ref(first_refs), ::std::ref(last_refs),
                             ::std::ref(ranges[worker]), worker, workers,
                             ::std::ref(ready), ::std::ref(start), ::std::ref(failures));
    }
    while(ready.load(::std::memory_order_acquire) != workers) { ::std::this_thread::yield(); }
    auto const alloc_start{::std::chrono::steady_clock::now()};
    start.store(true, ::std::memory_order_release);
    for(auto& thread : threads) { thread.join(); }
    auto const alloc_end{::std::chrono::steady_clock::now()};
    if(failures.load(::std::memory_order_relaxed) != 0u)
    { ::fast_io::io::println("allocation or affinity failed"); return 3; }
    // All 64 stores per worker must retain an observable final object. The
    // field check occurs outside the allocation timing interval.
    ::std::uint64_t checksum{};
    auto lowest{~::std::uintptr_t{}};
    ::std::uintptr_t highest{};
    for(::std::size_t i{}; i != store_count; ++i)
    {
        auto const first{reinterpret_cast<::std::uintptr_t>(first_refs[i].storage.ptr)};
        auto const last{reinterpret_cast<::std::uintptr_t>(last_refs[i].storage.ptr)};
        if(first == 0u || last < first || last_refs[i].storage.ptr == nullptr ||
           stores[i]->struct_set(last_refs[i], 0u,
                                  gc::gc_object_value::i32(static_cast<::std::uint32_t>(i))) !=
               gc::gc_object_status::ok)
        { ::fast_io::io::println("final reference invalid"); return 3; }
        gc::gc_object_value observed{};
        if(stores[i]->struct_get(last_refs[i], 0u, false, observed) != gc::gc_object_status::ok ||
           observed.as<::std::uint32_t>() != i)
        { ::fast_io::io::println("wrong store or field value"); return 3; }
        checksum += observed.as<::std::uint32_t>();
        // Each store has exactly one writer, and its issued tokens increase.
        // A TLS cache may restore older blocks when that worker changes stores;
        // the worker's final token therefore need not be its largest token.
        // Reduce all store first/last tokens after join, outside allocation time.
        lowest = ::std::min(lowest, first);
        highest = ::std::max(highest, last);
    }
    auto endpoint_lowest{ranges[0].first};
    auto endpoint_highest{ranges[0].last};
    for(unsigned worker{1u}; worker != workers; ++worker)
    {
        endpoint_lowest = ::std::min(endpoint_lowest, ranges[worker].first);
        endpoint_highest = ::std::max(endpoint_highest, ranges[worker].last);
    }
    if(lowest == 0u || highest < lowest || highest - lowest + 1u < total_allocations)
    { ::fast_io::io::println("bad all-store token range"); return 3; }
    auto const teardown_start{::std::chrono::steady_clock::now()};
    stores.clear();
    auto const teardown_end{::std::chrono::steady_clock::now()};
    auto const allocation_ns{::std::chrono::duration_cast<::std::chrono::nanoseconds>(
        alloc_end - alloc_start).count()};
    auto const teardown_ns{::std::chrono::duration_cast<::std::chrono::nanoseconds>(
        teardown_end - teardown_start).count()};
    ::fast_io::io::println("{\"workers\":", ::fast_io::mnp::dec(workers),
                           ",\"stores_per_worker\":", ::fast_io::mnp::dec(stores_per_worker),
                           ",\"allocations\":", ::fast_io::mnp::dec(total_allocations),
                           ",\"allocation_ns\":", ::fast_io::mnp::dec(allocation_ns),
                           ",\"teardown_ns\":", ::fast_io::mnp::dec(teardown_ns),
                           ",\"token_span\":", ::fast_io::mnp::dec(highest - lowest + 1u),
                           ",\"token_span_method\":\"all_store_first_last_single_writer\"",
                           ",\"token_endpoint_span\":", ::fast_io::mnp::dec(endpoint_highest - endpoint_lowest + 1u),
                           ",\"final_refs\":", ::fast_io::mnp::dec(store_count),
                           ",\"field_checksum\":", ::fast_io::mnp::dec(checksum), "}");
}
