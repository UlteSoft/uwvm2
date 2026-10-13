// Linux-only guest wait32 -> notify latency after /proc samples prove that
// the waiter is sleeping in a futex. Qualification reads occur before the
// measured notify interval. This is separate from the rendezvous fixture.
// The host creates the worker; Core threads has no guest spawn instruction.
#if !defined(__linux__)
# error "parked wait qualification requires Linux task state and wchan"
#endif
#include "uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <fast_io.h>
#include <fast_io_unit/string.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>

// C ABI links cannot add C++ exception edges to the host measurement.
extern "C" long parked_bench_syscall_noexcept(long, ...) noexcept asm("syscall");
extern "C" pthread_t parked_bench_pthread_self_noexcept() noexcept asm("pthread_self");
extern "C" int parked_bench_pthread_setaffinity_noexcept(pthread_t, size_t, cpu_set_t const*) noexcept
    asm("pthread_setaffinity_np");

#define WAIT_BENCH_REQUIRE(condition)                                                                                       \
    do                                                                                                                      \
    {                                                                                                                       \
        if(!(condition))                                                                                                    \
        {                                                                                                                   \
            ::fast_io::io::perr("FAIL Linux parked guest wait/notify line ", ::fast_io::mnp::dec(__LINE__),                   \
                                ": ", #condition, "\n");                                                                    \
            ::std::abort();                                                                                                  \
        }                                                                                                                   \
    } while(false)

namespace
{
    namespace strict = ::uwvm2test::uwvm_int_strict;
    namespace lib = ::uwvm2::runtime::lib;
    namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
    namespace types = ::uwvm2::uwvm::wasm::type;
    using features_t = types::feature_list<::uwvm2::parser::wasm::standard::wasm1::features::wasm1>;
    using steady_clock_type = ::std::chrono::steady_clock;
    constexpr unsigned warmups{2u}, samples{9u}, rounds{128u};

    struct observation
    {
        ::std::uint64_t wake_ns{};
        ::std::uint64_t notify_call_ns{};
        ::std::uint64_t qualification_ns{};
        unsigned qualification_probes{};
        unsigned empty_notify_retries{};
        long waiter_tid{};
        ::std::string parked_wchan{};
    };

    ::std::uint64_t elapsed_ns(steady_clock_type::time_point from, steady_clock_type::time_point to)
    {
        return static_cast<::std::uint64_t>(
            ::std::chrono::duration_cast<::std::chrono::nanoseconds>(to - from).count());
    }

    void pin_current_thread(unsigned cpu)
    {
        cpu_set_t allowed{};
        CPU_ZERO(&allowed);
        CPU_SET(cpu, &allowed);
        WAIT_BENCH_REQUIRE(parked_bench_pthread_setaffinity_noexcept(
            parked_bench_pthread_self_noexcept(), sizeof(allowed), &allowed) == 0);
    }

    ::std::string read_task_proc(long tid, ::std::string_view name)
    {
        auto const path{::fast_io::concat_std("/proc/self/task/", ::fast_io::mnp::dec(tid), "/", name)};
        ::fast_io::native_file input{path, ::fast_io::open_mode::in};
        ::std::array<::std::byte, 4096uz> buffer{};
        // [buffer.begin, buffer.end) is the complete local 4096-byte array.
        // [safe                   ] fast_io may return only a cursor in this range.
        auto const end{::fast_io::operations::read_some_bytes(input, buffer.data(), buffer.data() + buffer.size())};
        WAIT_BENCH_REQUIRE(end != buffer.data() && end != buffer.data() + buffer.size());
        // [buffer.begin, end) is initialized by that read; no NUL is required.
        // [safe             ] the string copies exactly that bounded extent.
        return {reinterpret_cast<char const*>(buffer.data()), static_cast<::std::size_t>(end - buffer.data())};
    }

    char read_task_state(long tid)
    {
        auto const status{read_task_proc(tid, "stat")};
        // Linux stat field 2 is a parenthesized comm that may contain spaces.
        // Its final ')' is followed by a space and the one-character state.
        auto const close{status.rfind(')')};
        WAIT_BENCH_REQUIRE(close != ::std::string::npos && close + 2uz < status.size());
        return status[close + 2uz];
    }

    struct task_probe
    {
        bool parked{};
        ::std::uint64_t overhead_ns{};
        ::std::string wchan{};
    };

    task_probe probe_parked_waiter(long tid)
    {
        auto const begin{steady_clock_type::now()};
        auto const before{read_task_state(tid)};
        auto wchan{read_task_proc(tid, "wchan")};
        while(!wchan.empty() && (wchan.back() == '\n' || wchan.back() == '\r')) { wchan.pop_back(); }
        auto const after{read_task_state(tid)};
        bool const symbol_only{::std::all_of(wchan.begin(), wchan.end(), [](char ch) noexcept
        {
            return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                   (ch >= '0' && ch <= '9') || ch == '_' || ch == '.';
        })};
        // /proc is sampled, not an atomic task snapshot. Both surrounding
        // states must be S, wchan must name a futex, and guest notify must
        // then return one. An unavailable/zero wchan cannot qualify a sample.
        bool const parked{before == 'S' && after == 'S' && symbol_only &&
                          wchan.find("futex") != ::std::string::npos};
        return {parked, elapsed_ns(begin, steady_clock_type::now()), ::std::move(wchan)};
    }

    strict::byte_vec make_module()
    {
        strict::module_builder module{};
        module.has_memory = module.memory_has_max = module.memory_shared = true;
        module.memory_min = module.memory_max = 1;
        strict::func_body wait{}, notify{}, mismatch{};
        // Address zero contains zero for the module's entire life. wait32(0,
        // expected=0, timeout=-1) must park until the second guest call wakes
        // it; notify(0, 1) returns the number of actual parked waiters.
        for(unsigned byte : {0x41u, 0u, 0x41u, 0u, 0x42u, 0x7fu,
                             0xfeu, 1u, 2u, 0u, 0x0bu})
        { strict::append_u8(wait.code, byte); }
        for(unsigned byte : {0x41u, 0u, 0x41u, 1u, 0xfeu, 0u, 2u, 0u, 0x0bu})
        { strict::append_u8(notify.code, byte); }
        for(unsigned byte : {0x41u, 0u, 0x41u, 1u, 0x42u, 0x7fu,
                             0xfeu, 1u, 2u, 0u, 0x0bu})
        { strict::append_u8(mismatch.code, byte); }
        module.add_func({{}, {strict::k_val_i32}}, ::std::move(wait));
        module.add_func({{}, {strict::k_val_i32}}, ::std::move(notify));
        module.add_func({{}, {strict::k_val_i32}}, ::std::move(mismatch));
        module.add_export_func(0, "wait32");
        module.add_export_func(1, "notify");
        module.add_export_func(2, "mismatch");
        return module.build();
    }

    ::std::uint32_t enter(unsigned function)
    {
        ::std::uint32_t value{};
        lib::full_compile_run_config config{};
        config.entry_function_index = function;
        // [safe: result storage remains live for the entire synchronous VM entry]
        config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(&value);
        config.entry_abi_buffers.result_bytes = sizeof(value);
        lib::full_compile_and_run_main_module(u8"parked-wait-notify-performance", config);
        return value;
    }
}

int main(int argc, char** argv)
{
    if(argc == 3 && ::std::strcmp(argv[1], "--dump-wasm") == 0)
    {
        auto const wasm{make_module()};
        ::fast_io::native_file output{::std::string{argv[2]}, ::fast_io::open_mode::out |
                                                 ::fast_io::open_mode::creat |
                                                 ::fast_io::open_mode::trunc};
        ::fast_io::operations::write_all_bytes(output, wasm.data(), wasm.data() + wasm.size());
        return 0;
    }
    if(argc != 2 || (::std::strcmp(argv[1], "instruction") != 0 &&
                       ::std::strcmp(argv[1], "unwind") != 0))
    { return 64; }
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_llvm_jit_call_stack = ::std::strcmp(argv[1], "unwind") == 0 ?
        mode::runtime_llvm_jit_call_stack_t::unwind :
        mode::runtime_llvm_jit_call_stack_t::instruction;
    mode::global_runtime_compile_threads_resolved = 1;

    auto wasm{make_module()};
    auto features{strict::make_wasm1p1_feature_parameter()};
    ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_threads = false;
    auto prepared{strict::prepare_runtime_from_wasm(wasm, u8"parked-wait-notify-performance", {}, features)};
    WAIT_BENCH_REQUIRE(prepared.mod != nullptr);
    WAIT_BENCH_REQUIRE(enter(2) == 1u); // Compile and check the mismatch path.
    WAIT_BENCH_REQUIRE(enter(1) == 0u); // Compile notify before timing.

    pin_current_thread(0u);
    for(unsigned sample{}; sample != warmups + samples; ++sample)
    {
        ::std::atomic<unsigned> entering{}, awoken{};
        ::std::atomic<long> waiter_tid{};
        ::std::vector<observation> rows(rounds);
        ::std::thread waiter{[&]
        {
            pin_current_thread(2u);
            auto const tid{parked_bench_syscall_noexcept(SYS_gettid)};
            WAIT_BENCH_REQUIRE(tid > 0);
            waiter_tid.store(tid, ::std::memory_order_release);
            for(unsigned round{}; round != rounds; ++round)
            {
                entering.store(round + 1u, ::std::memory_order_release);
                WAIT_BENCH_REQUIRE(enter(0) == 0u);
                awoken.store(round + 1u, ::std::memory_order_release);
            }
        }};
        for(unsigned round{}; round != rounds; ++round)
        {
            auto const deadline{steady_clock_type::now() + ::std::chrono::seconds(10)};
            while(entering.load(::std::memory_order_acquire) <= round)
            {
                WAIT_BENCH_REQUIRE(steady_clock_type::now() < deadline);
                ::std::this_thread::yield();
            }
            auto const tid{waiter_tid.load(::std::memory_order_acquire)};
            WAIT_BENCH_REQUIRE(tid > 0);
            unsigned probes{}, empty_retries{};
            ::std::uint64_t qualification_ns{};
            for(;;)
            {
                WAIT_BENCH_REQUIRE(steady_clock_type::now() < deadline);
                auto probe{probe_parked_waiter(tid)};
                ++probes;
                qualification_ns += probe.overhead_ns;
                if(!probe.parked) { ::std::this_thread::yield(); continue; }
                // The stat/wchan qualification has finished. Its reads and
                // retries are reported separately, outside [begin, awoken].
                auto const begin{steady_clock_type::now()};
                auto const notified{enter(1)};
                auto const end{steady_clock_type::now()};
                WAIT_BENCH_REQUIRE(notified <= 1u);
                if(notified == 1u)
                {
                    while(awoken.load(::std::memory_order_acquire) <= round)
                    {
                        WAIT_BENCH_REQUIRE(steady_clock_type::now() < deadline);
                        ::std::this_thread::yield();
                    }
                    rows[round] = {elapsed_ns(begin, steady_clock_type::now()),
                                   elapsed_ns(begin, end), qualification_ns, probes,
                                   empty_retries, tid, ::std::move(probe.wchan)};
                    break;
                }
                ++empty_retries;
                ::std::this_thread::yield();
            }
        }
        waiter.join();
        WAIT_BENCH_REQUIRE(awoken.load(::std::memory_order_acquire) == rounds);
        if(sample < warmups) { continue; }
        for(unsigned round{}; round != rounds; ++round)
        {
            auto const& row{rows[round]};
            ::fast_io::io::println("{\"policy\":\"", ::fast_io::mnp::os_c_str(argv[1]), "\",\"sample\":",
                                   ::fast_io::mnp::dec(sample - warmups),
                                   ",\"round\":", ::fast_io::mnp::dec(round),
                                   ",\"wake_ns\":", ::fast_io::mnp::dec(row.wake_ns),
                                   ",\"notify_call_ns\":", ::fast_io::mnp::dec(row.notify_call_ns),
                                   ",\"qualification_ns\":", ::fast_io::mnp::dec(row.qualification_ns),
                                   ",\"qualification_probes\":", ::fast_io::mnp::dec(row.qualification_probes),
                                   ",\"empty_notify_retries\":", ::fast_io::mnp::dec(row.empty_notify_retries),
                                   ",\"waiter_tid\":", ::fast_io::mnp::dec(row.waiter_tid),
                                   ",\"parked_task_state\":\"S\",\"notify_return\":1,\"parked_wchan\":\"",
                                   row.parked_wchan, "\"}");
        }
    }
    ::fast_io::io::println("PASS Linux parked guest wait32 -> guest notify; "
                        "stat S/futex wchan before notify=1; 9 samples x 128 wakeups, 2 warmups");
}
