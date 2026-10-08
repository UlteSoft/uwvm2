// Actual host-created threads enter a warmed VM through its public execution
// APIs. Same-thread VM execution is the matched memory-work baseline; native
// std::thread work is a separate creation/scheduling reference, not equivalent
// generated-code throughput. Compile and execute only in the remote cgroup.
#include "uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <fast_io.h>
#include <atomic>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>

#define BENCH_REQUIRE(condition) do { if(!(condition)) { ::fast_io::io::perr("FAIL VM thread benchmark ", \
    ::fast_io::mnp::dec(__LINE__), ": ", #condition, "\n"); ::std::abort(); } } while(false)

#ifdef UWVM_THREAD_BENCH_QUALIFY_CREATION
# include <dlfcn.h>
# include <pthread.h>
extern "C" void* thread_bench_dlsym_noexcept(void*, char const*) noexcept asm("dlsym");
namespace { std::atomic_size_t successful_native_creations{}; }
// Executable interposition, unlike --wrap, also sees libc++'s shared-library
// pthread_create call. This instrumentation exists only in the untimed binary.
extern "C" int pthread_create(pthread_t* id, pthread_attr_t const* attr, void* (*start)(void*), void* argument) noexcept
{
    using function = int (*)(pthread_t*, pthread_attr_t const*, void* (*)(void*), void*) noexcept;
    static function const real = []
    {
        auto address = thread_bench_dlsym_noexcept(RTLD_NEXT, "pthread_create");
        if(address == nullptr) { std::abort(); }
        function result{};
        static_assert(sizeof(result) == sizeof(address));
        std::memcpy(&result, &address, sizeof(result));
        return result;
    }();
    auto result = real(id, attr, start, argument);
    if(result == 0) { successful_native_creations.fetch_add(1, std::memory_order_relaxed); }
    return result;
}
#endif

namespace
{
    namespace strict = uwvm2test::uwvm_int_strict;
    namespace lib = uwvm2::runtime::lib;
    namespace mode = uwvm2::uwvm::runtime::runtime_mode;
    namespace types = uwvm2::uwvm::wasm::type;
    using features_t = types::feature_list<uwvm2::parser::wasm::standard::wasm1::features::wasm1>;
    using result_t = types::import_function_result_tuple_t<features_t>;
    using params_t = types::import_function_parameter_tuple_t<features_t>;
    using host_call_t = types::local_imported_function_type_t<result_t, params_t>;
    using clock_type = std::chrono::steady_clock;
    constexpr unsigned words = 1024, passes = 32;
    struct alignas(64) worker_state
    {
        std::array<std::uint32_t, words> native_memory{};
        clock_type::time_point launch{}, started{}, call{}, body_begin{}, body_end{}, returned{};
        std::uint32_t result{}, expected{};
        unsigned begins{}, ends{};
    };
    thread_local worker_state* active{};
    double ns(clock_type::time_point a, clock_type::time_point b)
    { return std::chrono::duration<double, std::nano>(b - a).count(); }
    struct begin
    {
        inline static constexpr uwvm2::utils::container::u8string_view function_name{u8"begin"};
        using result_tuple = result_t;
        using parameter_tuple = params_t;
        using local_imported_function_type = host_call_t;
        static void call(host_call_t&) noexcept
        {
            BENCH_REQUIRE(active != nullptr && !lib::runtime_execution_stop_requested_host_api());
            ++active->begins;
            active->body_begin = clock_type::now();
        }
    };
    struct end
    {
        inline static constexpr uwvm2::utils::container::u8string_view function_name{u8"end"};
        using result_tuple = result_t;
        using parameter_tuple = params_t;
        using local_imported_function_type = host_call_t;
        static void call(host_call_t&) noexcept
        {
            BENCH_REQUIRE(active != nullptr);
            active->body_end = clock_type::now();
            ++active->ends;
        }
    };
    struct host
    {
        uwvm2::utils::container::u8string_view module_name{u8"thread-bench-host"};
        using local_function_tuple = uwvm2::utils::container::tuple<begin, end>;
    };
    strict::byte_vec make_module()
    {
        strict::module_builder module{};
        module.has_memory = module.memory_has_max = module.memory_shared = true;
        module.memory_min = module.memory_max = 1;
        module.types.push_back({{}, {}});
        module.add_import_func("thread-bench-host", "begin", 0);
        module.add_import_func("thread-bench-host", "end", 0);
        strict::func_body body{};
        body.locals.push_back({5, strict::k_val_i32}); // i, pass, address, value, checksum
        auto bytes = [&](std::initializer_list<unsigned> code)
        { for(auto byte : code) { strict::append_u8(body.code, byte); } };
        auto constant = [&](std::int32_t value)
        { bytes({0x41}); strict::append_i32_leb(body.code, value); };
        auto advance = [&](unsigned local, unsigned limit)
        {
            bytes({0x20, local}); constant(1); bytes({0x6a, 0x22, local});
            constant(limit); bytes({0x49, 0x0d, 0});
        };
        auto address = [&] { bytes({0x20, 0, 0x20, 2}); constant(4); bytes({0x6c, 0x6a}); };
        // New threads syntax is executed once per call. Each worker owns a
        // disjoint 4 KiB slice of this shared Wasm memory, so ordinary accesses
        // are race-free and the memory loop does not need atomic operations.
        bytes({0x10, 0, 0xfe, 0x03, 0x00});
        constant(0); bytes({0x21, 2, 0x03, 0x40});
        address(); bytes({0x20, 2, 0x20, 1, 0x6a, 0x36, 2, 0});
        advance(2, words); bytes({0x0b});
        constant(0); bytes({0x21, 3, 0x03, 0x40});
        constant(0); bytes({0x21, 2, 0x03, 0x40});
        address(); bytes({0x22, 4, 0x20, 4, 0x28, 2, 0});
        constant(7); bytes({0x77, 0x20, 1, 0x20, 3, 0x6a, 0x73});
        constant(static_cast<std::int32_t>(0x9e3779b9u)); bytes({0x6a, 0x36, 2, 0});
        advance(2, words); bytes({0x0b});
        advance(3, passes); bytes({0x0b});
        constant(0); bytes({0x21, 2});
        constant(0); bytes({0x21, 6, 0x03, 0x40, 0x20, 6});
        constant(5); bytes({0x77});
        address(); bytes({0x28, 2, 0, 0x6a, 0x21, 6});
        advance(2, words); bytes({0x0b, 0x10, 1, 0x20, 6, 0x0b});
        module.add_func({{strict::k_val_i32, strict::k_val_i32}, {strict::k_val_i32}}, std::move(body));
        module.add_export_func(2, "kernel");
        return module.build();
    }
}

extern "C" [[gnu::noinline]] std::uint32_t uwvm_vm_thread_native_kernel(std::uint32_t* memory, std::uint32_t seed) noexcept
{
    std::atomic_thread_fence(std::memory_order_seq_cst);
    // [safe: one worker owns exactly words initialized uint32_t slots]
    for(unsigned i{}; i != words; ++i) { memory[i] = i + seed; }
    for(unsigned pass{}; pass != passes; ++pass)
    {
        for(unsigned i{}; i != words; ++i)
        { memory[i] = (std::rotl(memory[i], 7) ^ (seed + pass)) + 0x9e3779b9u; }
    }
    std::uint32_t checksum{};
    for(unsigned i{}; i != words; ++i) { checksum = std::rotl(checksum, 5) + memory[i]; }
    return checksum;
}

namespace
{
    enum class path { native, vm_entry, vm_raw };
    void enter(path selected, void const* module, worker_state& state, unsigned worker)
    {
        active = &state; // [safe: round-owned state outlives this worker and join]
        state.call = clock_type::now();
        auto const seed = 0x12345u + worker;
        if(selected == path::native)
        {
            state.body_begin = clock_type::now();
            state.result = uwvm_vm_thread_native_kernel(state.native_memory.data(), seed);
            state.body_end = clock_type::now();
        }
        else
        {
            std::uint32_t parameters[]{worker * words * 4u, seed};
            if(selected == path::vm_raw)
            { lib::llvm_jit_call_raw_host_api(module, 2, &state.result, sizeof(state.result), parameters, sizeof(parameters)); }
            else
            {
                lib::full_compile_run_config config{};
                config.entry_function_index = 2;
                // [safe: initialized parameter tuple and writable result owned by this live worker]
                config.entry_abi_buffers.param_buffer = reinterpret_cast<std::byte const*>(parameters);
                config.entry_abi_buffers.param_bytes = sizeof(parameters);
                config.entry_abi_buffers.result_buffer = reinterpret_cast<std::byte*>(&state.result);
                config.entry_abi_buffers.result_bytes = sizeof(state.result);
                lib::full_compile_and_run_main_module(u8"thread-performance", config);
            }
        }
        state.returned = clock_type::now();
        active = nullptr; // worker no longer borrows the round-owned timing state
    }
    struct measurement
    {
        double wall{}, constructors{}, joins{}, start_latency{}, admission{}, interval{}, release{};
        std::uint64_t checksum{};
        std::size_t creations{};
    };
    measurement measure(path selected, bool threaded, unsigned workers, unsigned rounds, void const* module)
    {
        measurement total{};
        for(unsigned round{}; round != rounds; ++round)
        {
            std::array<worker_state, 4> states{};
            for(unsigned i{}; i != workers; ++i)
            { states[i].expected = uwvm_vm_thread_native_kernel(states[i].native_memory.data(), 0x12345u + i); BENCH_REQUIRE(states[i].expected != 0); }
#ifdef UWVM_THREAD_BENCH_QUALIFY_CREATION
            auto const before = successful_native_creations.load(std::memory_order_relaxed);
#endif
            auto const started = clock_type::now();
            {
                std::array<std::thread, 4> threads;
                for(unsigned i{}; i != workers; ++i)
                {
                    states[i].launch = clock_type::now();
                    auto work = [&, i]
                    {
                        states[i].started = clock_type::now();
                        enter(selected, module, states[i], i);
                    };
                    if(threaded)
                    {
                        threads[i] = std::thread{work};
                        total.constructors += ns(states[i].launch, clock_type::now());
                    }
                    else { work(); }
                }
                if(threaded)
                {
                    for(unsigned i{}; i != workers; ++i)
                    {
                        auto const join_start = clock_type::now();
                        threads[i].join();
                        total.joins += ns(join_start, clock_type::now());
                    }
                }
            } // all native thread objects and each worker's VM TLS have drained
            total.wall += ns(started, clock_type::now());
#ifdef UWVM_THREAD_BENCH_QUALIFY_CREATION
            auto const created = successful_native_creations.load(std::memory_order_relaxed) - before;
            BENCH_REQUIRE(created == (threaded ? workers : 0u));
            total.creations += created;
#endif
            for(unsigned i{}; i != workers; ++i)
            {
                auto const& state = states[i];
                BENCH_REQUIRE(state.result == state.expected);
                BENCH_REQUIRE(state.begins == (selected == path::native ? 0u : 1u) && state.ends == state.begins);
                total.checksum += state.result;
                total.start_latency += ns(state.launch, state.started);
                total.admission += ns(state.call, state.body_begin);
                total.interval += ns(state.body_begin, state.body_end);
                total.release += ns(state.body_end, state.returned);
            }
        }
        total.wall /= rounds; total.constructors /= rounds; total.joins /= rounds;
        auto const calls = static_cast<double>(rounds) * workers;
        total.start_latency /= calls; total.admission /= calls; total.interval /= calls; total.release /= calls;
        return total;
    }
}

int main(int argc, char** argv)
{
    if(argc != 5) { return 64; }
    unsigned samples{}, rounds{};
    auto const parse_count{[](::std::string_view text, unsigned& result) noexcept
    {
        // [text.data(), text.data()+text.size()) is the complete argument.
        // [safe                                ] parse_by_scan stays in that extent.
        auto const end{text.data() + text.size()};
        auto const parsed{::fast_io::parse_by_scan(text.data(), end,
                                                   ::fast_io::mnp::dec_get<true, true>(result))};
        return parsed.code == ::fast_io::parse_code::ok && parsed.iter == end;
    }};
    if(!parse_count(argv[2], samples) || !parse_count(argv[3], rounds)) { return 64; }
    if(samples == 0 || samples > 99 || rounds == 0 || rounds > 1024) { return 64; }
    if(std::strcmp(argv[1], "unwind") != 0 && std::strcmp(argv[1], "instruction") != 0) { return 64; }
    mode::global_runtime_llvm_jit_call_stack = std::strcmp(argv[1], "unwind") == 0 ?
        mode::runtime_llvm_jit_call_stack_t::unwind : mode::runtime_llvm_jit_call_stack_t::instruction;
    auto wasm = make_module();
    {
        ::fast_io::native_file wasm_output{::std::string{argv[4]}, ::fast_io::open_mode::out |
            ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
        // [wasm.data(), wasm.data()+wasm.size()) is the complete generated module.
        // [safe                                ] write_all_bytes copies that extent.
        ::fast_io::operations::write_all_bytes(wasm_output, wasm.data(), wasm.data() + wasm.size());
    }
    auto features = strict::make_wasm1p1_feature_parameter();
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_threads = false;
    types::local_imported_t imported{host{}};
    auto prepared = strict::prepare_runtime_from_wasm(wasm, u8"thread-performance", {}, features, {imported});
    mode::global_runtime_compile_threads_resolved = 1;
    worker_state warm{};
    enter(path::vm_entry, prepared.mod, warm, 0); // parsing/JIT/materialization excluded from all samples
    for(auto selected : {path::native, path::vm_entry, path::vm_raw})
    {
        for(unsigned workers : {1u, 4u})
        {
            for(unsigned sample{}; sample != samples + 2; ++sample)
            {
                for(bool threaded : {bool(sample % 2), !bool(sample % 2)})
                {
                    auto value = measure(selected, threaded, workers, rounds, prepared.mod);
                    if(sample < 2) { continue; }
                    auto const path_name{selected == path::native ? "std_native" :
                        selected == path::vm_entry ? "vm_full_entry" : "vm_raw_entry"};
                    ::fast_io::io::println("{\"path\":\"", ::fast_io::mnp::os_c_str(path_name),
                        "\",\"workers\":", ::fast_io::mnp::dec(workers),
                        ",\"threaded\":", ::fast_io::mnp::os_c_str(threaded ? "true" : "false"),
                        ",\"sample\":", ::fast_io::mnp::dec(sample - 2), ",\"rounds\":", ::fast_io::mnp::dec(rounds),
                        ",\"wall_ns\":", ::fast_io::mnp::fixed(value.wall, 3u),
                        ",\"thread_constructor_sum_ns\":", ::fast_io::mnp::fixed(value.constructors, 3u),
                        ",\"join_sum_ns\":", ::fast_io::mnp::fixed(value.joins, 3u),
                        ",\"start_latency_ns\":", ::fast_io::mnp::fixed(value.start_latency, 3u),
                        ",\"admission_ns\":", ::fast_io::mnp::fixed(value.admission, 3u),
                        ",\"guest_interval_ns\":", ::fast_io::mnp::fixed(value.interval, 3u),
                        ",\"release_ns\":", ::fast_io::mnp::fixed(value.release, 3u),
                        ",\"checksum\":", ::fast_io::mnp::dec(value.checksum),
                        ",\"native_creations\":", ::fast_io::mnp::dec(value.creations), "}");
                }
            }
        }
    }
    // Keep successful records and their marker on the same stream: stdout
    // may be buffered, so a stderr marker could split a redirected JSON line.
    ::fast_io::io::println("PASS real VM thread entry/checksum/markers; shared memory + atomic.fence; creation counter ",
#ifdef UWVM_THREAD_BENCH_QUALIFY_CREATION
        "verified"
#else
        "not instrumented"
#endif
        );
}
