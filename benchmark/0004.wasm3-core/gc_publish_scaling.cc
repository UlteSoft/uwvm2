// Native diagnostic for GC publication, local field access, and teardown.
// Run only inside the 64 GiB swap-free test cgroup, pinned to P cores 0,2,4,6.
// This measures a runtime mechanism, not the throughput of a Wasm program.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <fast_io.h>
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

// The explicit assembly symbol and noexcept declaration keep C ABI calls from
// adding an artificial C++ exception edge to this diagnostic's worker path.
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

    void allocate_worker(gc::gc_object_store& store, unsigned cpu,
                         ::std::uint32_t count, ::std::atomic<unsigned>& ready,
                         ::std::atomic<bool>& start, ::std::atomic<unsigned>& failures) noexcept
    {
#if defined(__linux__)
        cpu_set_t set{};
        CPU_ZERO(&set);
        CPU_SET(cpu, &set);
        if(bench_pthread_setaffinity_np_noexcept(bench_pthread_self_noexcept(), sizeof(set), &set) != 0)
        { failures.fetch_add(1u, ::std::memory_order_relaxed); }
#else
        // macOS uses this path only for the bounded syntax preflight; all
        // measured samples run on Linux with the affinity contract above.
        (void)cpu;
#endif
        ready.fetch_add(1u, ::std::memory_order_release);
        while(!start.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
        for(::std::uint32_t index{}; index != count; ++index)
        {
            gc::gc_object_value value[]{gc::gc_object_value::i32(index)};
            gc::gc_reference reference{};
            if(store.struct_new(0u, value, 1u, reference) != gc::gc_object_status::ok)
            {
                failures.fetch_add(1u, ::std::memory_order_relaxed);
                break;
            }
        }
    }
}

int main(int argc, char** argv)
{
    constexpr ::std::uint32_t total_allocations{4000000u};
    if(argc != 2)
    { ::fast_io::io::println("usage: gc_publish_scaling one|two|four"); return 2; }
    // argv[1] is the OS-provided NUL-terminated argument; no pointer advances.
    ::std::string_view const mode{argv[1]};
    unsigned threads{};
    if(mode == "one") { threads = 1u; }
    else if(mode == "two") { threads = 2u; }
    else if(mode == "four") { threads = 4u; }
    else { ::fast_io::io::println("unknown worker mode"); return 2; }
    auto const types{make_types()};
    auto store{::std::make_unique<gc::gc_object_store>(types)};
    if(!store->valid()) { ::fast_io::io::println("GC store setup failed"); return 2; }
    ::std::atomic<unsigned> ready{};
    ::std::atomic<bool> start{};
    ::std::atomic<unsigned> failures{};
    ::std::vector<::std::thread> workers{};
    workers.reserve(threads);
    for(unsigned worker{}; worker != threads; ++worker)
    {
        workers.emplace_back(allocate_worker, ::std::ref(*store), p_cores[worker],
                             total_allocations / threads,
                             ::std::ref(ready), ::std::ref(start), ::std::ref(failures));
    }
    while(ready.load(::std::memory_order_acquire) != threads) { ::std::this_thread::yield(); }
    auto const alloc_start{::std::chrono::steady_clock::now()};
    start.store(true, ::std::memory_order_release);
    for(auto& worker : workers) { worker.join(); }
    auto const alloc_end{::std::chrono::steady_clock::now()};
    // Keep 1,024 newly published local objects. A state-derived index makes
    // each set/get use a real, varying guest reference and field address;
    // the loop measures the checked local path independently of allocation.
    constexpr ::std::uint32_t field_iterations{10000000u};
    gc::gc_object_value initial[]{gc::gc_object_value::i32(0u)};
    ::std::array<gc::gc_reference, 1024uz> hot{};
    for(auto& reference : hot)
    {
        if(store->struct_new(0u, initial, 1u, reference) != gc::gc_object_status::ok)
        { ::fast_io::io::println("GC field fixture creation failed"); return 3; }
    }
    ::std::uint64_t observed_checksum{};
    ::std::uint32_t field_state{123456789u};
    auto const field_start{::std::chrono::steady_clock::now()};
    for(::std::uint32_t i{}; i != field_iterations; ++i)
    {
        field_state = field_state * 1664525u + 1013904223u;
        auto const index{(field_state >> 20u) & 1023u};
        // [0, 1024) index selects one complete live reference in hot.
        if(store->struct_set(hot[index], 0u, gc::gc_object_value::i32(field_state)) != gc::gc_object_status::ok)
        { ::fast_io::io::println("GC local field set failed"); return 3; }
        gc::gc_object_value readback{};
        if(store->struct_get(hot[index], 0u, false, readback) != gc::gc_object_status::ok)
        { ::fast_io::io::println("GC local field get failed"); return 3; }
        observed_checksum += readback.as<::std::uint32_t>();
    }
    auto const field_end{::std::chrono::steady_clock::now()};
    ::std::uint64_t expected_checksum{};
    ::std::uint32_t expected_state{123456789u};
    for(::std::uint32_t i{}; i != field_iterations; ++i)
    {
        expected_state = expected_state * 1664525u + 1013904223u;
        expected_checksum += expected_state;
    }
    if(observed_checksum != expected_checksum)
    { ::fast_io::io::println("GC local field checksum mismatch"); return 3; }
    auto const teardown_start{::std::chrono::steady_clock::now()};
    store.reset();
    auto const teardown_end{::std::chrono::steady_clock::now()};
    if(failures.load(::std::memory_order_relaxed) != 0u)
    { ::fast_io::io::println("GC publication/affinity failed"); return 3; }
    auto const allocation_ns{::std::chrono::duration_cast<::std::chrono::nanoseconds>(
        alloc_end - alloc_start).count()};
    auto const teardown_ns{::std::chrono::duration_cast<::std::chrono::nanoseconds>(
        teardown_end - teardown_start).count()};
    auto const field_ns{::std::chrono::duration_cast<::std::chrono::nanoseconds>(
        field_end - field_start).count()};
    ::fast_io::io::println("{\"threads\":", ::fast_io::mnp::dec(threads),
                           ",\"allocations\":", ::fast_io::mnp::dec(total_allocations),
                           ",\"allocation_ns\":", ::fast_io::mnp::dec(allocation_ns),
                           ",\"field_iterations\":", ::fast_io::mnp::dec(field_iterations),
                           ",\"field_roots\":1024",
                           ",\"field_ns\":", ::fast_io::mnp::dec(field_ns),
                           ",\"field_checksum\":", ::fast_io::mnp::dec(observed_checksum),
                           ",\"teardown_ns\":", ::fast_io::mnp::dec(teardown_ns), "}");
}
