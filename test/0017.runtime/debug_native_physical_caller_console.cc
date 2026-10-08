// Actual console bt -> controller -> sealed physical caller query. The API
// fixture separately checks recursive incarnations, replacement and retirement.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/uwvm/debugger/controller.h>
#include <uwvm2/uwvm/debugger/console.h>
#include <uwvm2/utils/container/string_concat.h>
#include <fast_io.h>
#include <atomic>
#include <chrono>
#include <thread>
namespace lib = uwvm2::runtime::lib;
namespace mode = uwvm2::uwvm::runtime::runtime_mode;
namespace dbg = uwvm2::uwvm::debugger;
namespace control = uwvm2::utils::control;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_native_physical_caller_console: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static dbg::controller_reply command(dbg::controller& owner, ::fast_io::string const& text)
{ return owner.execute(dbg::parse_console_command(::fast_io::string_view{text.data(), text.size()}), ::std::chrono::seconds{10}); }
int main(int argc, char** argv)
{
    if(argc != 3 || !dbg::native_step::platform_available()) { return 77; }
    auto const policy{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}};
    if(policy != "instruction" && policy != "unwind") { return 2; }
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_exception_dispatch = mode::runtime_llvm_jit_exception_dispatch_t::native_unwind;
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ?
        mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto const path{uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{uwvm2::uwvm::cmdline::parsing_result};
    uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(uwvm2::utils::cmdline::parameter_parsing_results{u8"physical-console", nullptr,
        uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(uwvm2::utils::cmdline::parameter_parsing_results{
        uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())}, nullptr,
        uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"physical-console";
    check(uwvm2::uwvm::run::prepare_owned_full_cli_source() == static_cast<int>(uwvm2::uwvm::run::retval::ok), "real initializer");
    control::launch_config config{}; config.debug_enabled = true; config.compiler = control::backend::llvm;
    config.mode = control::compile_mode::full; config.origin = control::launch_origin::console; config.instance[0u] = 1u;
    config.vm_process = static_cast<::std::uint64_t>(dbg::posix_abi::getpid_noexcept());
    auto owner{dbg::controller::create(config, 1u)}; check(owner && owner->status() == control::error::none, "real console authority");
    auto observer{owner->observer()};
    check(lib::llvm_jit_configure_debug_session_host_api(owner->domain(), ::std::move(observer),
        lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok, "real observer");
    check(lib::llvm_jit_prepare_debug_host_api() && owner->arm_initial_pause(), "actual compiled initial pause");
    check(command(*owner, ::fast_io::concat_fast_io("break 0 1 0")).status == control::error::none, "real recursive callee breakpoint");
    check(command(*owner, ::fast_io::concat_fast_io("continue")).status == control::error::none, "open launch gate");
    ::std::uint32_t result{}; ::std::atomic_bool finished{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config run{}; run.entry_function_index = 2u;
        run.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(result)); run.entry_abi_buffers.result_bytes = sizeof(result);
        lib::full_compile_and_run_main_module(u8"physical-console", run); owner->notify_guest_exit(0); finished.store(true, ::std::memory_order_release);
    }};
    auto const end{::std::chrono::steady_clock::now() + ::std::chrono::seconds{45}};
    dbg::controller_reply current{};
    for(;;)
    {
        current = owner->inspect(); if(current.execution == dbg::execution_status::stopped && current.reason == dbg::stop_reason::breakpoint) { break; }
        check(::std::chrono::steady_clock::now() < end && !finished.load(::std::memory_order_acquire), "real callee stop deadline"); ::std::this_thread::yield();
    }
    // Reach two deeper genuine recursive breakpoints through ordinary Wasm
    // execution. The console test must print multiple PHYSICAL parents, not
    // merely a one-frame UI using a constructed controller reply.
    for(unsigned depth{}; depth != 2u; ++depth)
    {
        auto const previous{current.stop_identifier};
        check(command(*owner, ::fast_io::concat_fast_io("continue")).status == control::error::none, "resume genuine recursion");
        for(;;)
        {
            current = owner->inspect();
            if(current.execution == dbg::execution_status::stopped && current.reason == dbg::stop_reason::breakpoint &&
               current.stop_identifier > previous) { break; }
            check(::std::chrono::steady_clock::now() < end && !finished.load(::std::memory_order_acquire), "deeper actual breakpoint deadline");
            ::std::this_thread::yield();
        }
    }
    check(current.threads.size() == 1u && !current.threads[0u].native_pc, "actual cooperative child");
    auto const participant{current.threads[0u].identifier}; ::std::size_t queries{};
    for(unsigned attempt{}; attempt != 64u; ++attempt)
    {
        auto native{command(*owner, ::fast_io::concat_fast_io("step asm ", participant))};
        if(native.status != control::error::none)
        {
            check(native.status == control::error::unsupported_command && native.stop_identifier == current.stop_identifier, "unproved SI keeps original stop");
            current = command(*owner, ::fast_io::concat_fast_io("step wasm ", participant));
            check(current.status == control::error::none && current.threads.size() == 1u && !current.threads[0u].native_pc, "real Wasm search"); continue;
        }
        check(native.reason == dbg::stop_reason::native_step && native.threads.size() == 1u && native.threads[0u].native_pc, "actual contained kernel trap");
        auto const text{::fast_io::concat_fast_io("bt ", participant)}; auto const parsed{dbg::parse_console_command(::fast_io::string_view{text.data(), text.size()})};
        auto const trace{owner->execute(parsed)};
        check(trace.status == control::error::none && trace.stop_identifier == native.stop_identifier &&
            trace.native_caller_stop_identifier == native.stop_identifier && trace.native_caller && trace.native_caller->valid &&
            trace.native_caller->incarnation != trace.native_caller->current_incarnation && trace.memory.empty() && trace.locals.empty() &&
            trace.source_frames.empty() && trace.native_backtrace && trace.native_backtrace->count >= 3u &&
            trace.native_backtrace->complete, "console returns a complete real multi-frame Wasm physical chain");
        for(::std::size_t i{1u}; i != trace.native_backtrace->count; ++i)
        { check(trace.native_backtrace->frames[i].current_incarnation == trace.native_backtrace->frames[i - 1u].incarnation,
            "actual console physical chain preserves distinct recursive incarnations"); }
        auto const formatted{dbg::details::format_reply(trace, parsed)};
        check(formatted.find("physical Wasm caller module=0 function=") != ::std::string::npos &&
            formatted.find("authenticated frame 3") != ::std::string::npos &&
            formatted.find("complete to Wasm root") != ::std::string::npos && formatted.find("rsp=") == ::std::string::npos &&
            formatted.find("rbp=") == ::std::string::npos && formatted.find("cfa=") == ::std::string::npos, "actual FastIO formatting exposes no native frame storage");
        auto oversized{trace}; oversized.native_backtrace->count = 33u;
        check(dbg::details::format_reply(oversized, parsed).find("error: invalid physical Wasm backtrace view") == 0u,
            "formatter rejects oversized public DATA before array access");
        auto disconnected{trace}; ++disconnected.native_backtrace->frames[1u].current_incarnation;
        check(dbg::details::format_reply(disconnected, parsed).find("error: invalid physical Wasm backtrace chain") == 0u,
            "formatter refuses disconnected physical frame DATA");
        auto forged{parsed}; forged.disassembly_stop_identifier = native.stop_identifier - 1u;
        auto const denied{owner->execute(forged)};
        check(denied.status == control::error::unsupported_command && !denied.native_caller && !denied.native_backtrace && denied.native_caller_stop_identifier == 0u &&
            owner->inspect().stop_identifier == native.stop_identifier, "stale console stop refuses before reading, current stop preserved");
        ++queries; break;
    }
    check(queries != 0u, "positive console caller witness required");
    lib::reset_runtime_state_host_api(); check(owner->detach_resume(), "actual cancellation and worker ACK"); guest.join();
    check(result == 14u, "guest result preserved after physical caller inspection/reset");
    ::fast_io::io::println("debug_native_physical_caller_console: PASS policy=", policy, " actual-bt=", queries,
        " multi-frame=yes public-native-stack-bytes=0 stale-stop-retained=yes result=", result);
}
