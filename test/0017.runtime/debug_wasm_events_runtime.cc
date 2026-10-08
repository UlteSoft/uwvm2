// Actual full-runtime Core3 Wasm event/trace/catch/script witness.
// Component opcode decoding alone is not proof of a real guest safe point.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/uwvm/run/run.h>
#include <uwvm2/uwvm/debugger/controller.h>
#include <uwvm2/uwvm/debugger/console.h>
#include <uwvm2/utils/container/string_concat.h>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <fast_io.h>
#include <fast_io_unit/string.h>

#if defined(__FreeBSD__) && (defined(__clang__) || defined(__GNUC__))
# include <sys/types.h>
// The actual FreeBSD SDK declares pid_t getpid(void). Existing FastIO
// get_process_id(posix_process_observer) only reads an already-known PID;
// it cannot discover this running host. This test-only distinct name binds
// the real libc symbol with its exact SDK return type and noexcept C ABI.
namespace uwvm_freebsd_debug_events_test_abi
{
    extern "C" ::pid_t current_process_id_noexcept() noexcept __asm__("getpid");
    static_assert(::std::numeric_limits<::pid_t>::is_signed &&
                  ::std::numeric_limits<::pid_t>::digits == 31);
    static_assert(noexcept(current_process_id_noexcept()));
}
#endif

namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace dbg = ::uwvm2::uwvm::debugger;
namespace control = ::uwvm2::utils::control;
using clock_type = ::std::chrono::steady_clock;
static void check(bool value, char const* text) noexcept
{
    if(!value)
    {
        ::fast_io::io::perrln("debug_wasm_events_runtime: ", ::fast_io::mnp::os_c_str(text));
        ::fast_io::fast_terminate();
    }
}
static dbg::controller_reply command(dbg::controller& owner, ::std::string const& text)
{
    // [owned complete command ... text.size()] command_end
    // [safe                                  ] parser borrows only until execute;
    //  ^^ no integer address or unowned input cursor is requested.
    return owner.execute(dbg::parse_console_command(::fast_io::string_view{text.data(), text.size()}),
                         ::std::chrono::seconds{10});
}
int main(int argc, char** argv)
{
    if(argc != 3) { return 2; }
    auto const policy{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}};
    if(policy != "instruction" && policy != "unwind") { return 2; }
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
        u8"uwvm-debug-wasm-events", nullptr,
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
    features.explicit_enable_tail_call = true;
    features.disable_gc = false; features.explicit_enable_gc = true;
    features.disable_function_references = false; features.explicit_enable_function_references = true;
    features.disable_memory64 = false; features.explicit_enable_memory64 = true;
    features.disable_table64 = false; features.explicit_enable_table64 = true;
    features.disable_exceptions = false; features.explicit_enable_exceptions = true;
    features.disable_simd = false; features.explicit_enable_simd = true;
    features.disable_reference_types = false; features.explicit_enable_reference_types = true;
    features.disable_table_instructions = false; features.explicit_enable_table_instructions = true;
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"wasm-events-test";
    check(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true) ==
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
#elif defined(__FreeBSD__) && (defined(__clang__) || defined(__GNUC__))
    auto const actual_pid{::uwvm_freebsd_debug_events_test_abi::current_process_id_noexcept()};
    check(actual_pid > 0, "actual FreeBSD host process adapter returned a positive PID");
    config.vm_process = static_cast<::std::uint64_t>(actual_pid);
    ::fast_io::io::println("debug_wasm_events_runtime: actual FreeBSD process=",
                          ::fast_io::mnp::dec(config.vm_process));
#else
    ::fast_io::io::println("debug_wasm_events_runtime: SKIP host process adapter unavailable");
    lib::reset_runtime_state_host_api();
    return 77;
#endif
    auto owner{dbg::controller::create(config, 1u,
        lib::llvm_jit_capture_debug_stack_host_api, lib::llvm_jit_debug_read_memory_host_api)};
    check(owner && owner->status() == control::error::none, "real host-only console authority");
    ::std::weak_ptr<dbg::controller> lifetime{owner};
    auto domain{owner->domain()};
    auto actual_observer{owner->observer()};
    check(lib::llvm_jit_configure_debug_session_host_api(domain, ::std::move(actual_observer),
              lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok,
          "actual runtime observer owns controller until execution drain");
    check(lib::llvm_jit_prepare_debug_host_api(), "actual full fused validation/native publication");
    ::uwvm2::uwvm::runtime::initializer::apply_runtime_active_segments(u8"wasm-events-test");
    check(::uwvm2::uwvm::run::install_debug_source_maps(*owner), "install the actual published Wasm function spans before launch");
    check(owner->arm_initial_pause(), "empty-domain launch pause");
    auto const invalid{command(*owner, ::fast_io::concat_std("wasm-script trace wasm on all; continue"))};
    check(invalid.status == control::error::malformed && !command(*owner, ::fast_io::concat_std("trace wasm read")).wasm_trace_enabled,
          "complete script preparse rejects execution before earlier trace child has a side effect");
    auto setup{command(*owner, ::fast_io::concat_std("wasm-script trace wasm on all;catch wasm gc 0 2;catch wasm throw 0 all;info wasm-events"))};
    check(setup.status == control::error::none && setup.script_replies.size() == 4u && setup.script_commands.size() == 4u,
          "every bounded setup child independently authenticates the actual host session");
    for(auto const& child : setup.script_replies) { check(child.status == control::error::none, "setup child succeeded"); }
    auto const gc_catch{setup.script_replies[1u].wasm_catchpoint_identifier};
    auto const throw_catch{setup.script_replies[2u].wasm_catchpoint_identifier};
    check(gc_catch != 0u && throw_catch != 0u && gc_catch != throw_catch &&
          setup.script_replies[3u].wasm_catchpoints.size() == 2u, "actual emitted Core3 catch policies admitted");
    check(command(*owner, ::fast_io::concat_std("continue")).status == control::error::none, "open actual launch pause");
    ::std::uint32_t result{};
    ::std::atomic_bool finished{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config run{};
        run.entry_function_index = 2u;
        // [host-owned numeric result ... sizeof(result)] result_end
        // [safe                                        ] exact ordinary typed
        //  ^^ entry result slot remains owned until guest join, never a debug address.
        run.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(result));
        run.entry_abi_buffers.result_bytes = sizeof(result);
        lib::full_compile_and_run_main_module(u8"wasm-events-test", run);
        owner->notify_guest_exit(0);
        finished.store(true, ::std::memory_order_release);
    }};
    auto wait_catch{[&](::std::uint64_t identifier)
    {
        auto const deadline{clock_type::now() + ::std::chrono::seconds{20}};
        for(;;)
        {
            auto stopped{owner->inspect()};
            if(stopped.execution == dbg::execution_status::stopped)
            {
                check(stopped.reason == dbg::stop_reason::wasm_catchpoint && stopped.wasm_catchpoint_identifier == identifier &&
                      stopped.threads.size() == 1u && stopped.stop_identifier != 0u && !stopped.threads[0u].native_pc,
                      "genuine runtime callback parked at the requested Wasm catchpoint");
                return stopped;
            }
            check(clock_type::now() < deadline && !finished.load(::std::memory_order_acquire), "bounded actual Wasm catchpoint wait");
            ::std::this_thread::yield();
        }
    }};
    auto memory_value{[&]()
    {
        auto const observed{command(*owner, ::fast_io::concat_std("memory 0 0 0 4"))};
        check(observed.status == control::error::none && observed.memory.size() == 4u,
              "real stopped memory64 exposes only the authorized bounded guest bytes");
        // [owned response memory ... exact size=4] memory_end
        // [safe                                 ] the proved extent precedes
        //  ^^ both endpoint formation and the canonical little-endian parse.
        auto const* const first{reinterpret_cast<unsigned char const*>(observed.memory.data())};
        auto const* const end{first + observed.memory.size()};
        ::std::uint32_t value{};
        auto const parsed{::fast_io::parse_by_scan(first, end, ::fast_io::mnp::le_get<32>(value))};
        check(parsed.code == ::fast_io::parse_code::ok && parsed.iter == end, "bounded memory value parse consumed exactly four bytes");
        return value;
    }};
    auto check_stop_record{[&](dbg::controller_reply const& stopped, ::std::uint8_t primary, ::std::uint32_t extended)
    {
        auto trace{command(*owner, ::fast_io::concat_std("trace wasm read"))};
        check(trace.status == control::error::none && !trace.wasm_trace.empty() && trace.wasm_trace_remaining == 0u,
              "bounded workload trace includes its genuinely paused latest safe point");
        auto const& event{trace.wasm_trace.back()};
        auto const& thread{stopped.threads[0u]}; // wait_catch proved exactly one actual participant.
        check(event.participant == thread.identifier && event.module == thread.location.code_unit &&
              event.function == thread.location.function && event.offset == thread.location.offset &&
              event.runtime_epoch == thread.location.code_generation && event.function_generation == 1u &&
              event.instruction.available && event.instruction.primary == primary &&
              (primary != 0xfbu || (event.instruction.prefixed && event.instruction.extended == extended)),
              "trace opcode belongs to the same actual participant/location/epoch/generation");
        auto const text{dbg::details::format_reply(trace, dbg::parse_console_command("trace wasm read"))};
        check(text.find("before-instruction opcode=") != ::std::string::npos && text.find("opcode=unavailable") == ::std::string::npos,
              "actual trace formatter states before-instruction and real opcode metadata");
    }};
    auto gc_stop{wait_catch(gc_catch)};
    check(gc_stop.threads[0u].location.function == 2u && memory_value() == 0u,
          "GC struct.new catchpoint occurs before later memory64 store commits");
    check_stop_record(gc_stop, 0xfbu, 0u);
    check(command(*owner, ::fast_io::concat_std("disable wasm-event ", gc_catch)).status == control::error::none,
          "disable the real GC catch before subsequent GC instructions");
    check(command(*owner, ::fast_io::concat_std("continue")).status == control::error::none, "resume GC-before-instruction stop");
    auto throw_stop{wait_catch(throw_catch)};
    check(throw_stop.stop_identifier > gc_stop.stop_identifier && throw_stop.threads[0u].location.function == 1u && memory_value() == 31u,
          "callee throw stop observes completed memory64 work and a new real stop");
    check_stop_record(throw_stop, 0x08u, 0u);
    check(command(*owner, ::fast_io::concat_std("disable wasm-event ", throw_catch)).status == control::error::none,
          "disable before actual throw/catch execution continues");
    check(command(*owner, ::fast_io::concat_std("continue")).status == control::error::none, "resume actual throw into try_table catcher");
    auto const end{clock_type::now() + ::std::chrono::seconds{20}};
    while(!finished.load(::std::memory_order_acquire))
    { check(clock_type::now() < end, "actual EH/table64/SIMD/tail-call completion timeout"); ::std::this_thread::yield(); }
    guest.join(); check(result == 49u, "actual cross-function throw/catch payload 42 and tail-call result +7");

    ::std::uint64_t after{}, epoch{};
    ::std::size_t count{}, pages{};
    ::std::uint32_t categories{};
    bool struct_new{}, struct_get{}, try_table{}, throw_instruction{}, tail_call{}, memory64_load{}, memory64_store{}, table64_get{}, simd{};
    for(::std::size_t page{}; page != 128u; ++page)
    {
        auto const reply{command(*owner, ::fast_io::concat_std("trace wasm read ", after, " 3"))};
        check(reply.status == control::error::none && reply.wasm_trace_page_available && reply.wasm_trace.size() <= 3u &&
              reply.wasm_trace_overwritten == 0u && !reply.wasm_trace_cursor_gap && !reply.wasm_trace.empty(),
              "actual bounded trace pagination has no fabricated cursor, overwrite or gap");
        ++pages;
        for(auto const& event : reply.wasm_trace)
        {
            check(event.sequence == after + 1u && event.participant == gc_stop.threads[0u].identifier && event.module == 0u &&
                  event.runtime_epoch != 0u && event.function_generation == 1u && event.instruction.available,
                  "each actual record retains complete generation and monotonic identity");
            if(epoch == 0u) { epoch = event.runtime_epoch; }
            check(event.runtime_epoch == epoch, "one genuine runtime epoch spans direct calls, EH and tail call");
            after = event.sequence; ++count; categories |= event.instruction.categories;
            auto const primary{event.instruction.primary};
            struct_new |= primary == 0xfbu && event.instruction.prefixed && event.instruction.extended == 0u;
            struct_get |= primary == 0xfbu && event.instruction.prefixed && event.instruction.extended == 2u;
            try_table |= primary == 0x1fu; throw_instruction |= primary == 0x08u; tail_call |= primary == 0x12u;
            memory64_load |= primary == 0x28u; memory64_store |= primary == 0x36u; table64_get |= primary == 0x25u;
            simd |= primary == 0xfdu && event.instruction.prefixed;
        }
        check(reply.wasm_trace_next_sequence == after, "next cursor names the actual last returned record");
        if(reply.wasm_trace_remaining == 0u) { break; }
        check(page != 127u, "bounded actual pagination failed to drain its retained trace");
    }
    using cat = dbg::wasm_events::category;
    auto const required{static_cast<::std::uint32_t>(cat::gc_instruction) | static_cast<::std::uint32_t>(cat::memory_instruction) |
        static_cast<::std::uint32_t>(cat::table_instruction) | static_cast<::std::uint32_t>(cat::exception_instruction) |
        static_cast<::std::uint32_t>(cat::throw_instruction) | static_cast<::std::uint32_t>(cat::call_instruction) |
        static_cast<::std::uint32_t>(cat::reference_instruction) | static_cast<::std::uint32_t>(cat::simd_instruction) |
        static_cast<::std::uint32_t>(cat::control_instruction)};
    check(count > 3u && pages >= 2u && (categories & required) == required && struct_new && struct_get && try_table &&
          throw_instruction && tail_call && memory64_load && memory64_store && table64_get && simd,
          "missing actual Core3 opcode/category/pagination coverage is FAIL");
    check(command(*owner, ::fast_io::concat_std("trace wasm clear")).status == control::error::none &&
          command(*owner, ::fast_io::concat_std("trace wasm read")).wasm_trace.empty(), "actual bounded trace can be cleared");
    lib::reset_runtime_state_host_api();
    check(domain->is_closed() && command(*owner, ::fast_io::concat_std("memory 0 0 0 4")).status != control::error::none,
          "retired runtime history cannot grant old guest-memory read authority");
    owner.reset(); check(lifetime.expired(), "actual observer retired after completed guest and reset");
    ::fast_io::io::println("debug_wasm_events_runtime: PASS actual Core3 Wasm events policy=", policy,
        " records=", ::fast_io::mnp::dec(count), " pages=", ::fast_io::mnp::dec(pages),
        " real-gc-and-throw-catchpoints=2 before-instruction=yes mem64-table64-gc-EH-tailcall-simd=yes");
}
