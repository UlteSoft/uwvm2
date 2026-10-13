/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

// Small paired measurements of actual thread-management facilities. This is
// neither a Wasm memory benchmark nor a claim about VM host-entry throughput.
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>
#include <uwvm2/utils/thread/impl.h>

static_assert(::uwvm2::utils::thread::has_fast_io_native_thread, "This benchmark requires native worker threads");

#ifdef UWVM_THREAD_BENCH_QUALIFY_CREATION
# include <pthread.h>
namespace { ::std::atomic_size_t successful_native_creations{}; }
extern "C" int __real_pthread_create(pthread_t*, pthread_attr_t const*, void* (*)(void*), void*);
extern "C" int __wrap_pthread_create(pthread_t* id, pthread_attr_t const* attr, void* (*start)(void*), void* argument)
{
    auto const error{__real_pthread_create(id, attr, start, argument)};
    if(error == 0) { successful_native_creations.fetch_add(1uz, ::std::memory_order_relaxed); }
    return error;
}
#endif

namespace
{
    namespace threads = ::uwvm2::utils::thread;
    using clock_type = ::std::chrono::steady_clock;
    constexpr ::std::size_t task_count{64uz};
    constexpr ::std::size_t memory_words{1024uz};
    constexpr ::std::size_t memory_passes{64uz};
    using memory_buffer = ::std::array<::std::uint64_t, memory_words>;

    [[noreturn]] void fail(char const* reason)
    {
        ::std::fprintf(stderr, "FAIL thread creation benchmark: %s\n", reason);
        ::std::abort();
    }

    [[nodiscard]] double elapsed_ns(clock_type::time_point start, clock_type::time_point end)
    {
        return ::std::chrono::duration<double, ::std::nano>{end - start}.count();
    }

    [[nodiscard]] ::std::uint64_t tiny_work(::std::uint64_t value) noexcept
    {
        for(unsigned i{}; i != 32u; ++i)
        {
            value ^= value >> 13u;
            value *= 0x9e3779b97f4a7c15ull;
            value += i;
        }
        return value;
    }

    [[nodiscard]] threads::scheduled_task task_body(::std::array<::std::uint64_t, task_count>& outputs, ::std::size_t index) noexcept
    {
        outputs[index] = tiny_work(index + 1uz);
        co_return;
    }

    void run_reference_batch(threads::scheduled_task_batch& batch, ::std::size_t worker_count)
    {
        ::std::atomic_size_t next{};
        auto work{[&]() noexcept
        {
            for(;;)
            {
                auto const index{next.fetch_add(1uz, ::std::memory_order_acq_rel)};
                if(index >= batch.size()) { break; }
                batch.resume_and_destroy(index);
            }
        }};
        ::std::vector<::std::thread> workers;
        workers.reserve(worker_count);
        for(::std::size_t index{}; index != worker_count; ++index) { workers.emplace_back(work); }
        work(); // The product also includes the calling thread in batch execution.
        for(auto& worker: workers) { worker.join(); }
    }

    struct measurement
    {
        double wall_ns{};
        double body_ns{};
        ::std::uint64_t checksum{};
    };

    [[nodiscard]] measurement measure_batch(bool managed, ::std::size_t worker_count, ::std::size_t rounds)
    {
        measurement result{};
#ifdef UWVM_THREAD_BENCH_QUALIFY_CREATION
        auto const created_before{successful_native_creations.load(::std::memory_order_relaxed)};
#endif
        for(::std::size_t round{}; round != rounds; ++round)
        {
            ::std::array<::std::uint64_t, task_count> outputs{};
            threads::scheduled_task_batch batch{task_count};
            for(::std::size_t index{}; index != task_count; ++index)
            {
                auto task{task_body(outputs, index)};
                // [safe: task_count allocated handle slots] [one-past]
                //        ^^ handle_count is strictly below task_count here.
                ::std::construct_at(batch.handles.buffer + batch.handle_count, task.release());
                ++batch.handle_count;
            }
            // Identical coroutine frames and output buffers exist before either
            // timer. Worker allocation, creation, work, join and pool/vector
            // destruction are all included; serial fixture setup is excluded.
            auto const start{clock_type::now()};
            if(managed)
            {
                threads::native_thread_pool pool;
                pool.run(batch, worker_count);
            }
            else { run_reference_batch(batch, worker_count); }
            auto const end{clock_type::now()};
            result.wall_ns += elapsed_ns(start, end);
            for(::std::size_t index{}; index != task_count; ++index)
            {
                if(outputs[index] != tiny_work(index + 1uz)) { fail("prepared batch result mismatch"); }
                result.checksum += outputs[index];
            }
        }
#ifdef UWVM_THREAD_BENCH_QUALIFY_CREATION
        // This separate untimed binary wraps direct calls made by fast_io.
        // libc++'s std::thread calls are inside its shared library and unwrapped.
        // The timed binary has neither wrapper nor creation-counter overhead.
        if(managed && successful_native_creations.load(::std::memory_order_relaxed) - created_before != worker_count * rounds)
        { fail("native worker creation fell back to serial execution"); }
#endif
        result.wall_ns /= static_cast<double>(rounds);
        return result;
    }
}

// Both lease and baseline workers call precisely the same out-of-line body.
// Inspect this symbol in the optimized binary: no admission/stop/guard operation
// belongs inside the memory loop. The private buffer is exactly 8 KiB.
extern "C" [[gnu::noinline]] ::std::uint64_t uwvm_thread_bench_memory_kernel(memory_buffer& data, ::std::uint64_t seed) noexcept
{
    for(::std::size_t pass{}; pass != memory_passes; ++pass)
    {
        for(auto& value: data)
        {
            value = ((value << 7u) | (value >> 57u)) ^ seed;
            value += 0x9e3779b97f4a7c15ull;
        }
        seed += pass + 1uz;
    }
    ::std::uint64_t checksum{};
    for(auto const value: data) { checksum ^= value; }
    return checksum;
}

namespace
{
    struct alignas(64) worker_state
    {
        memory_buffer data{};
        double body_ns{};
        ::std::uint64_t checksum{};
    };

    void initialize_buffer(memory_buffer& data, ::std::size_t worker)
    {
        for(::std::size_t index{}; index != data.size(); ++index) { data[index] = (index + 1uz) * (worker + 3uz); }
    }

    [[nodiscard]] measurement measure_domain(bool managed, ::std::size_t worker_count, ::std::size_t rounds)
    {
        threads::execution_domain domain{worker_count};
        ::std::vector<worker_state> states(worker_count);
        ::std::vector<::std::uint64_t> expected(worker_count);
        for(::std::size_t index{}; index != worker_count; ++index)
        {
            memory_buffer reference{};
            initialize_buffer(reference, index);
            expected[index] = uwvm_thread_bench_memory_kernel(reference, index + 1uz);
        }
        measurement result{};
        for(::std::size_t round{}; round != rounds; ++round)
        {
            for(::std::size_t index{}; index != worker_count; ++index) { initialize_buffer(states[index].data, index); }
            auto const start{clock_type::now()};
            {
                ::std::vector<::std::thread> workers;
                workers.reserve(worker_count);
                for(::std::size_t index{}; index != worker_count; ++index)
                {
                    workers.emplace_back([&, index]
                    {
                        // The host-entry lease is acquired exactly once, before
                        // body timing, and destroyed after body timing. Neither
                        // branch performs management inside the memory kernel.
                        auto lease{managed ? domain.try_enter() : threads::execution_domain::lease{}};
                        if(managed && !lease) { fail("host admission unexpectedly rejected"); }
                        auto& state{states[index]};
                        auto const body_start{clock_type::now()};
                        state.checksum = uwvm_thread_bench_memory_kernel(state.data, index + 1uz);
                        state.body_ns = elapsed_ns(body_start, clock_type::now());
                    });
                }
                for(auto& worker: workers) { worker.join(); }
            }
            result.wall_ns += elapsed_ns(start, clock_type::now());
            for(::std::size_t index{}; index != worker_count; ++index)
            {
                if(states[index].checksum != expected[index]) { fail("memory kernel result mismatch"); }
                result.body_ns += states[index].body_ns;
                result.checksum += states[index].checksum;
            }
        }
        // Parallel body times are reported per worker, never subtracted from the
        // elapsed create/join wall time. Domain construction/destruction is cold
        // VM-generation setup and intentionally outside every measured interval.
        result.wall_ns /= static_cast<double>(rounds);
        result.body_ns /= static_cast<double>(rounds * worker_count);
        return result;
    }

    using measure_fn = measurement (*)(bool, ::std::size_t, ::std::size_t);

    void profile(char const* name, measure_fn measure, ::std::size_t workers, bool qualify)
    {
        ::std::size_t const rounds{qualify ? 1uz : 32uz};
        ::std::size_t const samples{qualify ? 1uz : 9uz};
        if(!qualify)
        {
            for(unsigned warmup{}; warmup != 2u; ++warmup)
            {
                (void)measure(false, workers, 4uz);
                (void)measure(true, workers, 4uz);
            }
        }
        for(::std::size_t sample{}; sample != samples; ++sample)
        {
            ::std::array<measurement, 2> pair{};
            for(::std::size_t order{}; order != 2uz; ++order)
            {
                auto const managed{static_cast<bool>((sample + order) & 1uz)};
                auto const value{measure(managed, workers, rounds)};
                pair[managed] = value;
                ::std::printf("%s,%zu,%zu,%zu,%s,%zu,%.3f,%.3f,%llu\n", name, workers, sample, order,
                              managed ? "managed" : "reference", rounds, value.wall_ns, value.body_ns,
                              static_cast<unsigned long long>(value.checksum));
            }
            if(pair[0].checksum != pair[1].checksum) { fail("paired workloads differed"); }
        }
    }
}

int main(int argc, char** argv)
{
    bool const qualify{argc == 2 && ::std::strcmp(argv[1], "--qualify") == 0};
    if(argc != 1 && !qualify) { fail("expected no argument or --qualify"); }
    ::std::puts("profile,workers,sample,order,variant,rounds,wall_ns_per_round,body_ns_per_worker,checksum");
    for(auto const workers: {1uz, 4uz})
    {
        profile("prepared_batch_create_run_join", measure_batch, workers, qualify);
        profile("host_thread_domain_create_run_join", measure_domain, workers, qualify);
    }
    ::std::fputs("PASS thread management paired workload checks\n", stderr);
}
