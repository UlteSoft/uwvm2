// Real two-thread guest wait32 -> notify rendezvous latency. The host creates and
// joins the worker, as required by the Core threads proposal: there is no
// guest spawn instruction. Build only from the frozen O3 runtime in the test
// cgroup; the runner pins the process to two assigned P cores. notify=1 proves
// an enrolled Wasm waiter, but this fixture does not prove an OS-sleep state.
#include "uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <fast_io.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#define WAIT_BENCH_REQUIRE(condition)                                                                                       \
    do                                                                                                                      \
    {                                                                                                                       \
        if(!(condition))                                                                                                    \
        {                                                                                                                   \
            ::fast_io::io::perr("FAIL guest wait/notify latency line ", ::fast_io::mnp::dec(__LINE__),                    \
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
        unsigned empty_polls{};
    };

    ::std::uint64_t elapsed_ns(steady_clock_type::time_point from, steady_clock_type::time_point to)
    {
        return static_cast<::std::uint64_t>(
            ::std::chrono::duration_cast<::std::chrono::nanoseconds>(to - from).count());
    }

    strict::byte_vec make_module()
    {
        strict::module_builder module{};
        module.has_memory = module.memory_has_max = module.memory_shared = true;
        module.memory_min = module.memory_max = 1;
        strict::func_body wait{}, notify{}, mismatch{};
        // Address zero contains zero for the module's entire life. wait32(0,
        // expected=0, timeout=-1) waits until the second guest call wakes it;
        // notify(0, 1) reports a registered waiter, not kernel-sleep status.
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
        lib::full_compile_and_run_main_module(u8"wait-notify-performance", config);
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
    auto prepared{strict::prepare_runtime_from_wasm(wasm, u8"wait-notify-performance", {}, features)};
    WAIT_BENCH_REQUIRE(prepared.mod != nullptr);
    WAIT_BENCH_REQUIRE(enter(2) == 1u); // Compile and check the mismatch path.
    WAIT_BENCH_REQUIRE(enter(1) == 0u); // Compile notify before timing.

    for(unsigned sample{}; sample != warmups + samples; ++sample)
    {
        ::std::atomic<unsigned> entering{}, awoken{};
        ::std::vector<observation> rows(rounds);
        ::std::thread waiter{[&]
        {
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
            unsigned polls{};
            for(;;)
            {
                WAIT_BENCH_REQUIRE(steady_clock_type::now() < deadline);
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
                                   elapsed_ns(begin, end), polls};
                    break;
                }
                ++polls;
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
                                   ",\"empty_polls\":", ::fast_io::mnp::dec(row.empty_polls), "}");
        }
    }
    ::fast_io::io::println("PASS real two-thread guest wait32 -> guest notify; "
                        "9 samples x 128 wakeups, 2 warmup samples");
}
