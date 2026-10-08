// Real console NI through the genuine runtime issuer and actual Wasm recursion.
// Requires the optional private backend witness in every compiled translation unit.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/uwvm/debugger/controller.h>
#include <uwvm2/uwvm/debugger/console.h>
#include <uwvm2/utils/container/string_concat.h>
#include <fast_io.h>
#include <atomic>
#include <chrono>
#include <thread>
#if !defined(UWVM2_TEST_NATIVE_CALL_CONTINUATION_WITNESS) || !UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
# error Genuine software call witness build required
#endif
namespace lib = uwvm2::runtime::lib;
namespace mode = uwvm2::uwvm::runtime::runtime_mode;
namespace dbg = uwvm2::uwvm::debugger;
namespace control = uwvm2::utils::control;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_native_call_console: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static dbg::controller_reply command(dbg::controller& owner, ::fast_io::string const& text)
{ return owner.execute(dbg::parse_console_command(::fast_io::string_view{text.data(), text.size()}), ::std::chrono::seconds{10}); }
int main(int argc, char** argv)
{
    if(argc != 4 || !dbg::native_step::platform_available()) { return 77; }
    auto const policy{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}};
    if(policy != "instruction" && policy != "unwind") { return 2; }
    auto const scenario{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[3])}};
    bool const mixed{scenario == "cancel-with-call"};
    bool const cancel{scenario == "cancel" || mixed};
    bool const unsupported{scenario == "unsupported-finish"};
    bool const recursive_running{scenario == "recursive-running"};
    if(!cancel && !unsupported && !recursive_running && scenario != "normal") { return 2; }
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
    check(uwvm2::uwvm::run::prepare_owned_full_cli_source(true) == static_cast<int>(uwvm2::uwvm::run::retval::ok), "real initializer");
    control::launch_config config{}; config.debug_enabled = true; config.compiler = control::backend::llvm;
    config.mode = control::compile_mode::full; config.origin = control::launch_origin::console; config.instance[0u] = 1u;
    config.vm_process = static_cast<::std::uint64_t>(dbg::posix_abi::getpid_noexcept());
    auto owner{dbg::controller::create(config, 1u)}; check(owner && owner->status() == control::error::none, "real console authority");
    auto observer{owner->observer()};
    check(lib::llvm_jit_configure_debug_session_host_api(owner->domain(), ::std::move(observer),
        lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok, "real observer");
    check(lib::llvm_jit_configure_debug_value_observation_host_api() ==
        lib::llvm_jit_debug_configure_result::ok,"real precompile typed Wasm observer selection");
    check(lib::llvm_jit_prepare_debug_host_api() && owner->arm_initial_pause(), "actual compiled initial pause");
    check(command(*owner, ::fast_io::concat_fast_io(::fast_io::mnp::os_c_str(cancel ? "break 0 3 0" : "break 0 0 31"))).status == control::error::none, "real before-call Wasm breakpoint");
    check(command(*owner, ::fast_io::concat_fast_io("continue")).status == control::error::none, "open launch gate");
    ::std::uint32_t result{}; ::std::atomic_bool finished{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config run{}; run.entry_function_index = cancel ? 3u : 1u;
        run.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(result)); run.entry_abi_buffers.result_bytes = sizeof(result);
        lib::full_compile_and_run_main_module(u8"physical-console", run); owner->notify_guest_exit(0); finished.store(true, ::std::memory_order_release);
    }};
    auto const end{::std::chrono::steady_clock::now() + ::std::chrono::seconds{300}};
    dbg::controller_reply current{};
    for(;;)
    {
        current = owner->inspect(); if(current.execution == dbg::execution_status::stopped && current.reason == dbg::stop_reason::breakpoint) { break; }
        check(::std::chrono::steady_clock::now() < end && !finished.load(::std::memory_order_acquire), "real callee stop deadline"); ::std::this_thread::yield();
    }
    check(current.threads.size()==1u && !current.threads[0u].native_pc,"genuine initial Wasm pause");
    check(command(*owner,::fast_io::concat_fast_io("delete 1")).status==control::error::none,"retire recursive breakpoint");
    auto const participant{current.threads[0u].identifier};unsigned calls{};bool cancelled{};
    for(unsigned attempt{};attempt!=8192u && ::std::chrono::steady_clock::now()<end;++attempt)
    {
        auto const before{current};
        if(attempt<8u || attempt%64u==0u)
        { ::fast_io::io::perrln("call search attempt=",attempt," function=",current.threads[0u].location.function,
            " Wasm offset=",current.threads[0u].location.offset," native=",bool(current.threads[0u].native_pc)); }
        // SI can now enter an authenticated typed Wasm callee. Running SI
        // first would consume the recursive call whose NI rearming this test
        // must witness. Search with NI at each real native stop instead; only
        // the actual signal-side return counter distinguishes a call from an
        // ordinary native instruction, and descendant/escape remain mandatory.
        dbg::controller_reply next{};
        if(before.threads.size()==1u && before.threads[0u].native_pc)
        {
            auto const returns_before{dbg::native_step::details::call_return_hits.load()};
            auto const text{::fast_io::concat_fast_io("ni ",participant)};
            auto const parsed{dbg::parse_console_command(::fast_io::string_view{text.data(),text.size()})};
            ::std::atomic_bool interrupt_requested{},execution_returned{},observed_wake{};
            ::std::thread input;
            if(cancel)
            {
                input=::std::thread{[&]
                {
                    auto const deadline{::std::chrono::steady_clock::now()+::std::chrono::seconds{2}};
                    while(!execution_returned.load(::std::memory_order_acquire))
                    {
                        if(owner->inspect().execution==dbg::execution_status::stopping)
                        { observed_wake=true;break; }
                        if(::std::chrono::steady_clock::now()>=deadline) { interrupt_requested=true;return; }
                        ::std::this_thread::yield();
                    }
                    if(!execution_returned.load(::std::memory_order_acquire))
                    { ::std::this_thread::sleep_for(::std::chrono::milliseconds{50});interrupt_requested=true; }
                }};
            }
            dbg::management_wait_interrupt interruption{::std::addressof(interrupt_requested),[](void* p) noexcept
            { return static_cast<::std::atomic_bool*>(p)->load(::std::memory_order_acquire); }};
            next=owner->execute(parsed,::std::chrono::seconds{10},interruption);execution_returned=true;
            if(input.joinable()) { input.join(); }
            ::fast_io::io::perrln("call NI status=",static_cast<unsigned>(next.status)," reason=",static_cast<unsigned>(next.reason),
                " next=",static_cast<unsigned>(next.native_next_reason)," from=",next.native_step_from," to=",next.native_step_to);
            if(next.status!=control::error::none)
            {
                ::fast_io::io::perrln("call failure old-stop=",before.stop_identifier," new-stop=",next.stop_identifier,
                    " execution=",static_cast<unsigned>(next.execution)," timeout=",next.timed_out,
                    " thread-count=",next.threads.size()," native-pc-present=",next.threads.size()==1u && bool(next.threads[0u].native_pc),
                    " return-hits=",dbg::native_step::details::call_return_hits.load(),
                    " descendant-hits=",dbg::native_step::details::call_descendant_hits.load(),
                    " escape-hits=",dbg::native_step::details::call_escape_hits.load());
            }
            if(cancel && observed_wake.load() && next.status==control::error::none && next.reason==dbg::stop_reason::requested &&
               next.stop_identifier>before.stop_identifier && next.threads.size()==1u && !next.threads[0u].native_pc)
            {
                check(!next.native_step_from && !next.native_step_to && !next.native_instruction,
                    "cancelled native call publishes only real cooperative Wasm pause after worker ACK");
                check(next.threads[0u].location.function==2u,"actual nonreturning Wasm callee ran before interruption");
                cancelled=true;current=::std::move(next);break;
            }
            if(next.status==control::error::none && next.stop_identifier>before.stop_identifier)
            {
                bool const actual_call{dbg::native_step::details::call_return_hits.load()>returns_before};
                if(!actual_call)
                {
                    check(next.execution==dbg::execution_status::stopped && next.threads.size()==1u,
                        "NI search retains a genuine stopped participant");
                    current=::std::move(next);continue;
                }
                check(next.reason==dbg::stop_reason::native_step && next.threads.size()==1u && next.threads[0u].native_pc &&
                    next.native_step_from==before.threads[0u].native_pc && next.native_step_to>next.native_step_from &&
                    next.threads[0u].location.function==before.threads[0u].location.function &&
                    next.memory.empty() && next.locals.empty() && next.source_frames.empty(),
                    "genuine call returns to original Wasm caller without callee or host state");
                auto const registers{command(*owner,::fast_io::concat_fast_io("info all-registers"))};
                check(registers.status==control::error::none && registers.registers_stop_identifier==next.stop_identifier,
                    "returned native registers bind to new genuine stop");
                auto const& raw{registers.registers};
                for(auto index:{dbg::native_registers::sp_index(raw.machine),dbg::native_registers::fp_index(raw.machine)})
                { if(index<raw.size()) { check(raw.values[index]==0u && raw.known_bits[index]==0u,"native stack/frame registers stay hidden"); } }
                for(::std::size_t i{};i!=raw.size();++i)
                {
                    if(raw.known_bits[i]==0u) { check(raw.values[i]==0u,"unproved native register bytes stay cleared"); }
                    else if(i!=dbg::native_registers::pc_index(raw.machine))
                    {
                        // This Wasm fixture contains only i32 values in [0,77].
                        // A masked VM address is not a valid fixture value.
                        check(raw.known_bits[i]<=32u && raw.values[i]<=77u,
                            "proved returned register is a real fixture i32 value");
                    }
                }
                for(auto const& value:raw.floating.values)
                {
                    check(value.width==0u,"i32-only Wasm cannot authorize native FP registers");
                    for(auto byte:value.bytes) { check(byte==0u,"unproved native FP bytes stay cleared"); }
                }
                auto stale=parsed;stale.disassembly_stop_identifier=before.stop_identifier;
                auto const denied{owner->execute(stale)};
                check(denied.status==control::error::unsupported_command && denied.stop_identifier==next.stop_identifier &&
                    !denied.native_step_from && !denied.native_step_to,"old NI stop cannot move returned caller");
                ++calls;current=::std::move(next);
                if(!cancel && dbg::native_step::details::call_descendant_hits.load()!=0u) { break; }
                continue;
            }
            check(next.stop_identifier==before.stop_identifier && next.threads.size()==1u &&
                next.threads[0u].native_pc==before.threads[0u].native_pc,
                "refused or pre-wake interrupted NI retains original native stop");
        }
        else
        {
            next=command(*owner,::fast_io::concat_fast_io("step asm ",participant));
            if(next.status==control::error::none)
            { current=::std::move(next);continue; }
            check(next.status==control::error::unsupported_command && next.stop_identifier==before.stop_identifier,
                "unproved SI keeps actual paused state");
        }
        current=command(*owner,::fast_io::concat_fast_io("step wasm ",participant));
        check(current.status==control::error::none && current.execution==dbg::execution_status::stopped,
            "explicit fixture Wasm search remains a genuine stopped activation");
    }
    auto const descendants{dbg::native_step::details::call_descendant_hits.load()};
    auto const escapes{dbg::native_step::details::call_escape_hits.load()};
    if(cancel)
    {
        check(cancelled,"actual nonreturning NI must wake, interrupt and drain");
        check(command(*owner,::fast_io::concat_fast_io("set wasm global 0 0 ",participant," bits i32 0")).status==control::error::none,
            "only actual Wasm global terminates cancelled fixture");
    }
    else { check(calls!=0u && descendants!=0u && descendants==escapes,"genuine Wasm recursive same-PC traps require real descendant chain and paired escape"); }
    check(command(*owner,::fast_io::concat_fast_io("continue")).status==control::error::none,"release retained NI owners after worker ACK");
    auto const done{::std::chrono::steady_clock::now()+::std::chrono::seconds{20}};
    while(!finished.load(::std::memory_order_acquire))
    { check(::std::chrono::steady_clock::now()<done,"real guest exit");::std::this_thread::yield(); }
    guest.join();check(result==(cancel?77u:48u),"actual Wasm result preserved");
    lib::reset_runtime_state_host_api();owner.reset();
    ::fast_io::io::println("PASS actual Wasm NI calls=",calls," recursive_descendants=",descendants," owned_escapes=",escapes,
        " policy=",policy," cancellation=",cancelled," native_stack_and_unproved_registers_hidden=true");
}
