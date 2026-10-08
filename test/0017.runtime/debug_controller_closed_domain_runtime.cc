// Real runtime controller/native-step lifecycle witness, not a component state
// simulation. Run only against the SAME fresh producer/runtime/host TU closure.
// Existing platform qualification gates stay intact; unavailable is a SKIP,
// never an emulated native trap or a claimed platform pass.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/uwvm/debugger/controller.h>
#include <uwvm2/utils/container/string_concat.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <fast_io.h>
#include <fast_io_unit/string.h>

namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace dbg = ::uwvm2::uwvm::debugger;
namespace control = ::uwvm2::utils::control;
using clock_type = ::std::chrono::steady_clock;

static void check(bool value, char const* text) noexcept
{
    if(!value)
    {
        ::fast_io::io::perrln("debug_controller_closed_domain_runtime: ", ::fast_io::mnp::os_c_str(text));
        ::fast_io::fast_terminate();
    }
}
static dbg::controller_reply command(dbg::controller& owner, ::std::string const& text)
{
    // [owned complete command ... text.size()] command_end
    // [safe                                  ] parse borrows only until execute
    //  ^^ has copied its bounded command; no guest/host address is encoded.
    return owner.execute(dbg::parse_console_command(::fast_io::string_view{text.data(), text.size()}),
                         ::std::chrono::seconds{10});
}
// Deliberately invalid TRUSTED host callbacks, only for bounded death cases.
// The runtime has already closed the actual domain and retained the actual
// observer owner. No pointer is read or forged by this negative control.
static bool recursive_close_reset(void*, ::uwvm2::utils::thread::cooperative_pause_domain const*) noexcept
{
    ::fast_io::io::perrln("actual-native-close-callback-recursive-reset");
    lib::reset_runtime_state_host_api();
    ::fast_io::io::perrln("UNEXPECTED close callback recursive reset returned");
    return false;
}
static bool recursive_close_stop(void*, ::uwvm2::utils::thread::cooperative_pause_domain const*) noexcept
{
    ::fast_io::io::perrln("actual-native-close-callback-recursive-stop");
    lib::runtime_stop_and_drain_host_api();
    ::fast_io::io::perrln("UNEXPECTED close callback recursive stop returned");
    return false;
}
int main(int argc, char** argv)
{
    if(argc != 3 && argc != 4) { return 2; }
    if(!dbg::native_step::platform_available())
    {
        ::fast_io::io::println("debug_controller_closed_domain_runtime: SKIP native backend unavailable");
        return 77;
    }
    // [host-owned terminated argv[2]] argv2_end
    // [safe                        ] a process launch argument, never guest IO.
    auto const policy{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}};
    if(policy != "instruction" && policy != "unwind") { return 2; }
    bool racing_close{};
    bool direct_reset{}, direct_stop{}, recursive_reset{}, recursive_stop{};
    ::std::uint32_t close_delay_us{};
    if(argc == 4)
    {
        // [actual terminated host argv[3]][bounded view ... size] arg_end
        // [safe                       ] form the end only within this owned
        //  ^^ launch argument. Public fast_io decimal parser handles overflow.
        auto const delay{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[3])}};
        recursive_reset = delay == "recursive-reset"; recursive_stop = delay == "recursive-stop";
        direct_reset = delay == "reset" || recursive_reset; direct_stop = delay == "stop" || recursive_stop;
        if(!direct_reset && !direct_stop)
        {
            racing_close = true;
            auto const* first{delay.data()}; auto const* last{first + delay.size()};
            auto const parsed{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::dec(close_delay_us))};
            if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last || close_delay_us > 10000u) { return 2; }
        }
    }
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_exception_dispatch = mode::runtime_llvm_jit_exception_dispatch_t::native_unwind;
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ?
        mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0;
    mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;

    // [host-owned terminated argv[1]] argv1_end
    // [safe                        ] copied to a stable owning UTF-8 string
    //  ^^ before publishing any argv cursor to the real initializer.
    auto const owned_path{::uwvm2::utils::container::u8concat_uwvm(
        ::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    // [old parsing_result storage] old_end
    // [safe                     ] retire its cursor BEFORE clear/growth.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr;
    arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"uwvm-debug-controller-lifecycle", nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(owned_path.c_str())}, nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [actual parsing_result array][last live argument] arguments_end
    // [safe                       ] both insertions preceded this borrow;
    //                              no later vector mutation before reset/join.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(
        ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_tail_call = false;
    features.explicit_enable_tail_call = true; // same explicit CLI feature gate
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"native-close-test";
    check(::uwvm2::uwvm::run::prepare_owned_full_cli_source() ==
        static_cast<int>(::uwvm2::uwvm::run::retval::ok), "actual CLI owning initializer");

    control::launch_config config{};
    config.debug_enabled = true;
    config.compiler = control::backend::llvm;
    config.mode = control::compile_mode::full;
    config.origin = control::launch_origin::console;
    config.instance[0u] = 1u; // host routing label, never authentication secret
#if defined(_WIN32) && !defined(__CYGWIN__)
    config.vm_process = ::fast_io::win32::GetCurrentProcessId();
#elif defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
    config.vm_process = static_cast<::std::uint64_t>(dbg::posix_abi::getpid_noexcept());
#else
    ::fast_io::io::println("debug_controller_closed_domain_runtime: SKIP host process adapter unavailable");
    lib::reset_runtime_state_host_api();
    return 77;
#endif
    auto owner{dbg::controller::create(config, 1u)};
    check(owner && owner->status() == control::error::none, "real host-only console authority");
    ::std::weak_ptr<dbg::controller> lifetime{owner};
    auto domain{owner->domain()};
    auto actual_observer{owner->observer()};
    // Original owning context and genuine safe-point callbacks are preserved.
    // Only the negative-case HOST close callback intentionally violates its
    // no-recursive-maintenance contract after a genuine native trap is proven.
    if(recursive_reset) { actual_observer.on_close = recursive_close_reset; }
    else if(recursive_stop) { actual_observer.on_close = recursive_close_stop; }
    check(lib::llvm_jit_configure_debug_session_host_api(domain, ::std::move(actual_observer),
              lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok,
          "actual runtime observer owns controller until execution drain");
    check(lib::llvm_jit_prepare_debug_host_api(), "actual full fused validation/native publication");
    check(owner->arm_initial_pause(), "empty-domain launch pause");
    auto added{command(*owner, ::fast_io::concat_std("break 0 1 0"))};
    check(added.status == control::error::none && added.breakpoint_identifier != 0u,
          "real emitted instruction breakpoint admitted");
    check(command(*owner, ::fast_io::concat_std("continue")).status == control::error::none,
          "open launch pause before starting guest");

    ::std::uint32_t result{};
    ::std::atomic_bool finished{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config run{};
        run.entry_function_index = 1u;
        // [host-owned exact result slot] result_end
        // [safe                        ] the ordinary typed ABI owns this slot
        //  ^^ through guest join; it is not a debugger/native address request.
        run.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(result));
        run.entry_abi_buffers.result_bytes = sizeof(result);
        lib::full_compile_and_run_main_module(u8"native-close-test", run);
        owner->notify_guest_exit(0);
        finished.store(true, ::std::memory_order_release);
    }};
    auto const deadline{clock_type::now() + ::std::chrono::seconds{20}};
    dbg::controller_reply stopped{};
    for(;;)
    {
        stopped = owner->inspect();
        if(stopped.execution == dbg::execution_status::stopped && stopped.reason == dbg::stop_reason::breakpoint) { break; }
        check(clock_type::now() < deadline && !finished.load(::std::memory_order_acquire), "bounded real breakpoint wait");
        ::std::this_thread::yield();
    }
    check(stopped.threads.size() == 1u && stopped.threads[0u].location.code_unit == 0u &&
          stopped.threads[0u].location.function == 1u && stopped.threads[0u].location.offset == 0u &&
          stopped.threads[0u].location.code_generation != 0u,
          "actual published participant/module/function/epoch");
    auto const participant{stopped.threads[0u].identifier};
    ::std::optional<::std::thread> closer{};
    ::std::atomic_bool begin_close{};
    if(racing_close)
    {
        closer.emplace([&]
        {
            while(!begin_close.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
            if(close_delay_us != 0u) { ::std::this_thread::sleep_for(::std::chrono::microseconds{close_delay_us}); }
            domain->close();
        });
    }
    begin_close.store(true, ::std::memory_order_release);
    auto stepped{command(*owner, ::fast_io::concat_std("step asm ", participant))};
    bool const genuine_native_stop{stepped.status == control::error::none && !stepped.timed_out &&
          stepped.reason == dbg::stop_reason::native_step && stepped.execution == dbg::execution_status::stopped &&
          stepped.native_instruction && stepped.native_step_from != 0u && stepped.native_step_to != 0u &&
          stepped.threads.size() == 1u && stepped.threads[0u].native_pc &&
          *stepped.threads[0u].native_pc == stepped.native_step_to};
    if(!racing_close)
    {
        check(genuine_native_stop, "genuine one-instruction native trap, not cooperative simulation");
        auto registers{command(*owner, ::fast_io::concat_std("info registers"))};
        check(registers.status == control::error::none && registers.registers.size() != 0u &&
              registers.registers.pc() == stepped.native_step_to &&
              registers.registers_stop_identifier == stepped.stop_identifier,
              "real trap GPR snapshot authenticated against private capture/publication");
    }
    else
    {
        // Close can win before request, while the genuine bridge arms, or after
        // a successful trap. A rejected operation must never lose its active
        // native borrower. Do not claim which uninstrumented phase won.
        closer->join();
        check(genuine_native_stop || stepped.status != control::error::none,
              "raced step either reports the real trap or explicitly rejects");
    }

    if(direct_reset || direct_stop)
    {
        // NO detach/close/release before this host maintenance operation. The
        // genuine native trap still owns the guest's execution lease. Old
        // domain-close-only maintenance deadlocks here; the new retained close
        // observer must retire the actual native borrower before lease drain.
        check(genuine_native_stop, "maintenance starts at a genuine native trap");
        if(recursive_reset || recursive_stop)
        { ::fast_io::io::perrln("genuine-native-trap-for-recursive-close=yes"); }
        if(direct_reset) { lib::reset_runtime_state_host_api(); }
        else { lib::runtime_stop_and_drain_host_api(); }
        ::fast_io::io::println("direct-maintenance-retired-native=", ::fast_io::mnp::os_c_str(direct_reset ? "reset" : "stop"));
    }
    else
    {
        // A caller's plain domain close removes authorization but cannot
        // release the trapped backend. Its explicit detach must drain it.
        domain->close();
    }
    check(domain->is_closed(), "actual domain closed");
    auto const closed_registers{command(*owner, ::fast_io::concat_std("info registers"))};
    check(closed_registers.status != control::error::none && closed_registers.registers.size() == 0u,
          "closed domain never grants register/native read authority");
    check(owner->detach_resume(), "closed-domain detach drains real native borrower");
    check(owner->detach_resume(), "second detach remains idempotent after actual clear");
    auto const join_deadline{clock_type::now() + ::std::chrono::seconds{10}};
    while(!finished.load(::std::memory_order_acquire))
    {
        check(clock_type::now() < join_deadline, "guest did not remain in a closed native gate");
        ::std::this_thread::yield();
    }
    guest.join();
    check(result == 13u, "Core 3 return_call executed correctly after actual release");
    check(owner->detach_resume(), "post-exit detach remains safe");
    owner.reset();
    if(direct_reset)
    { check(lifetime.expired(), "direct reset already retired the actual observer after native drain"); }
    else
    {
        check(!lifetime.expired(), "runtime still owns its published observer before reset");
        lib::reset_runtime_state_host_api();
    }
    check(lifetime.expired(), "real reset retires observer and drained controller/native owners");
    ::fast_io::io::println("debug_controller_closed_domain_runtime: PASS actual closed-domain detach/idempotence/Core3 return_call/reset ", policy);
    if(racing_close)
    { ::fast_io::io::println("close-race delay-us=", ::fast_io::mnp::dec(close_delay_us), " genuine-native-result=", genuine_native_stop); }
    else { ::fast_io::io::println("genuine-native-trap=yes"); }
}
