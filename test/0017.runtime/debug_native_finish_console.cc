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
{ if(!value) { ::fast_io::io::perrln("debug_native_finish_console: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
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
    check(command(*owner, ::fast_io::concat_fast_io(cancel ? "break 0 4 0" : "break 0 1 0")).status == control::error::none, "real recursive callee breakpoint");
    check(command(*owner, ::fast_io::concat_fast_io("continue")).status == control::error::none, "open launch gate");
    ::std::uint32_t result{}; ::std::atomic_bool finished{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config run{}; run.entry_function_index = cancel ? 3u : 2u;
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
    for(unsigned depth{}; depth != (cancel ? 0u : recursive_running ? 1u : 2u); ++depth)
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
    auto const participant{current.threads[0u].identifier}; ::std::size_t queries{}, returns{};
    auto unavailable{command(*owner, ::fast_io::concat_fast_io("finish asm ",participant))};
    check(unavailable.status == control::error::unsupported_command && unavailable.stop_identifier == current.stop_identifier &&
          !unavailable.threads[0u].native_pc,"cooperative finish refusal opens no native gate");
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

        if(mixed)
        {
            bool called{};
            for(unsigned search{}; search != 256u; ++search)
            {
                auto const before{native};
                auto next{command(*owner,::fast_io::concat_fast_io("step asm ",participant))};
                if(next.status == control::error::none)
                {
                    check(next.reason == dbg::stop_reason::native_step && next.stop_identifier > before.stop_identifier &&
                          next.threads.size() == 1u && next.threads[0u].native_pc &&
                          next.threads[0u].location.function == 4u,"mixed search executes only genuine same-Wasm-owner SI");
                    native = ::std::move(next); continue;
                }
                check(next.status == control::error::unsupported_command && next.stop_identifier == before.stop_identifier &&
                      next.threads.size() == 1u && next.threads[0u].native_pc == before.threads[0u].native_pc,
                      "mixed search SI refusal retains exact native stop");
                next = command(*owner,::fast_io::concat_fast_io("ni ",participant));
                if(next.status == control::error::none)
                {
                    // NI and SI use the same ordinary/branch admission. Only
                    // an actual sealed call continuation permits NI after this
                    // retained SI refusal. No text/constructed DTO proves it.
                    check(next.reason == dbg::stop_reason::native_step && next.stop_identifier > before.stop_identifier &&
                          next.threads.size() == 1u && next.threads[0u].native_pc &&
                          next.threads[0u].location.function == 4u && next.native_step_from == before.threads[0u].native_pc &&
                          next.native_step_to > next.native_step_from && next.memory.empty() && next.locals.empty() &&
                          next.source_frames.empty(),"actual NI call returns to the genuine spin caller before finish cancellation");
                    native = ::std::move(next); called = true; break;
                }
                check(next.status == control::error::unsupported_command && next.stop_identifier == before.stop_identifier &&
                      next.threads.size() == 1u && next.threads[0u].native_pc == before.threads[0u].native_pc,
                      "mixed unavailable NI opens no native gate");
                // No call event was installed. Only this fixture performs an
                // explicit Wasm search; production NI/finish has no fallback.
                current = command(*owner,::fast_io::concat_fast_io("step wasm ",participant));
                check(current.status == control::error::none && current.threads.size() == 1u &&
                      !current.threads[0u].native_pc && current.threads[0u].location.function == 4u,"mixed explicit Wasm search");
                bool found{};
                for(unsigned bootstrap{}; bootstrap != 64u; ++bootstrap)
                {
                    next = command(*owner,::fast_io::concat_fast_io("step asm ",participant));
                    if(next.status == control::error::none)
                    {
                        check(next.reason == dbg::stop_reason::native_step && next.threads.size() == 1u &&
                              next.threads[0u].native_pc && next.threads[0u].location.function == 4u,"mixed genuine native reentry");
                        native = ::std::move(next); found = true; break;
                    }
                    check(next.status == control::error::unsupported_command && next.stop_identifier == current.stop_identifier,
                          "mixed bootstrap refusal retains actual cooperative stop");
                    current = command(*owner,::fast_io::concat_fast_io("step wasm ",participant));
                    check(current.status == control::error::none && current.threads.size() == 1u && !current.threads[0u].native_pc &&
                          current.threads[0u].location.function == 4u,"mixed finite cooperative search");
                }
                check(found,"mixed actual native stop required");
            }
            check(called,"positive retained NI call required before a nonreturning return event");
        }

        auto const first_id{native.stop_identifier};
        auto const finishtext{::fast_io::concat_fast_io("finish asm ",participant)};
        auto parsed{dbg::parse_console_command(::fast_io::string_view{finishtext.data(),finishtext.size()})};
        bool pending{true};
        dbg::management_wait_interrupt early{::std::addressof(pending),[](void* p) noexcept { return *static_cast<bool*>(p); }};
        if(unsupported)
        {
            // A separate negative capability test. Never call this a successful
            // finish: it requires a real SI trap and an unchanged denied stop.
            for(bool interrupted : {true, false})
            {
                pending = interrupted;
                auto const denied{owner->execute(parsed,::std::chrono::seconds{10},early)};
                check(denied.status == control::error::unsupported_command &&
                      denied.native_next_reason == dbg::native_next_policy::reason::caller_unwind_unavailable &&
                      denied.stop_identifier == first_id && denied.threads.size() == 1u &&
                      denied.threads[0u].native_pc == native.threads[0u].native_pc &&
                      denied.threads[0u].location.function == native.threads[0u].location.function &&
                      !denied.native_step_from && !denied.native_step_to && !denied.native_instruction,
                      "unimplemented finish cannot wake or move a real native trap");
            }
            check(command(*owner,::fast_io::concat_fast_io("delete 1")).status == control::error::none,"retire actual breakpoint after denied finish");
            check(command(*owner,::fast_io::concat_fast_io("continue")).status == control::error::none,"release real SI owner after denied finish");
            ++queries; break;
        }
        auto const kept{owner->execute(parsed,::std::chrono::seconds{10},early)};
        check(kept.status == control::error::none && kept.stop_identifier == first_id &&
              kept.threads.size() == 1u && kept.threads[0u].native_pc == native.threads[0u].native_pc &&
              !kept.native_step_from && !kept.native_step_to,"pre-wake interrupt retains the actual native trap");
        if(cancel)
        {
            check(command(*owner,::fast_io::concat_fast_io("delete 1")).status == control::error::none,"retire actual spin breakpoint before native wake");
            ::std::atomic_bool flag{}, execution_returned{}, observed_wake{};
            ::std::thread input{[&]
            {
                auto const wake_deadline{::std::chrono::steady_clock::now() + ::std::chrono::seconds{2}};
                while(!execution_returned.load(::std::memory_order_acquire))
                {
                    if(owner->inspect().execution == dbg::execution_status::stopping)
                    { observed_wake.store(true,::std::memory_order_release); break; }
                    if(::std::chrono::steady_clock::now() >= wake_deadline)
                    { flag.store(true,::std::memory_order_release); return; }
                    ::std::this_thread::yield();
                }
                if(!execution_returned.load(::std::memory_order_acquire))
                {
                    ::std::this_thread::sleep_for(::std::chrono::milliseconds{50});
                    flag.store(true,::std::memory_order_release);
                }
            }};
            dbg::management_wait_interrupt interrupt{::std::addressof(flag),[](void* p) noexcept
            { return static_cast<::std::atomic_bool*>(p)->load(::std::memory_order_acquire); }};
            auto const stopped{owner->execute(parsed,::std::chrono::seconds{10},interrupt)}; execution_returned.store(true,::std::memory_order_release); input.join();
            check(observed_wake.load(::std::memory_order_acquire),"actual return event wakes the genuine callee before cancellation");
            ::fast_io::io::perrln("cancel state: status=",static_cast<unsigned>(stopped.status),
                " execution=",static_cast<unsigned>(stopped.execution)," reason=",static_cast<unsigned>(stopped.reason),
                " old-stop=",first_id," stop=",stopped.stop_identifier," threads=",stopped.threads.size(),
                " native-pc-present=",!stopped.threads.empty() && stopped.threads[0u].native_pc.has_value(),
                " function=",stopped.threads.empty() ? SIZE_MAX : stopped.threads[0u].location.function,
                " from=",stopped.native_step_from," to=",stopped.native_step_to,
                " next-reason=",static_cast<unsigned>(stopped.native_next_reason)," timeout=",stopped.timed_out);
            check(stopped.status == control::error::none && stopped.execution == dbg::execution_status::stopped &&
                  stopped.reason == dbg::stop_reason::requested && stopped.stop_identifier > first_id &&
                  stopped.threads.size() == 1u && !stopped.threads[0u].native_pc && stopped.threads[0u].location.code_unit == 0u &&
                  (stopped.threads[0u].location.function == 4u || stopped.threads[0u].location.function == 0u) &&
                  !stopped.native_step_from && !stopped.native_step_to && !stopped.native_instruction,
                  "interrupted nonreturning finish drains actual event ACK and preserves a genuine Wasm pause");
            auto const globals{command(*owner,::fast_io::concat_fast_io("globals ",participant," 0 0 2"))};
            ::fast_io::io::perrln("cancel globals: ",dbg::details::format_reply(globals,dbg::parse_console_command("globals")));
            check(globals.status == control::error::none && globals.wasm_state_values.result == dbg::wasm_state::status::available &&
                  globals.wasm_state_values.rows.size() == 2u && globals.wasm_state_values.rows[1u].data.available &&
                  globals.wasm_state_values.rows[1u].data.type.kind == dbg::wasm_state::value_kind::i32 &&
                  (globals.wasm_state_values.rows[1u].data.bits[0u] != ::std::byte{} || globals.wasm_state_values.rows[1u].data.bits[1u] != ::std::byte{} || globals.wasm_state_values.rows[1u].data.bits[2u] != ::std::byte{} || globals.wasm_state_values.rows[1u].data.bits[3u] != ::std::byte{}),
                  "real spin executed between native wake and cooperative cancellation");
            auto const exit{command(*owner,::fast_io::concat_fast_io("set wasm global 0 0 ",participant," bits i32 0"))};
            check(exit.status == control::error::none && exit.wasm_mutation_value.applied,"only real Wasm global exits the fixture loop");
            check(command(*owner,::fast_io::concat_fast_io("continue")).status == control::error::none,"resume actual cooperative exit");
        }
        else
        {
            check(command(*owner,::fast_io::concat_fast_io("delete 1")).status == control::error::none,"retire actual recursive breakpoint");
#if ((defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))
            auto const other_before{dbg::native_step::details::return_other_stack_hits.load(::std::memory_order_relaxed)};
            auto const skip_before{dbg::native_step::details::return_skip_hits.load(::std::memory_order_relaxed)};
#endif
            for(unsigned hop{}; hop != (recursive_running ? 2u : 3u); ++hop)
            {
                auto const old{native.stop_identifier};
                auto const trace{command(*owner,::fast_io::concat_fast_io("bt ",participant))};
                ::fast_io::io::perrln("physical trace: hop=",hop," status=",static_cast<unsigned>(trace.status),
                    " caller=",trace.native_caller.has_value()," frames=",trace.native_backtrace ? trace.native_backtrace->count : 0u,
                    " complete=",trace.native_backtrace && trace.native_backtrace->complete);
                check(trace.status == control::error::none && trace.native_caller && trace.native_caller->valid &&
                      trace.native_backtrace && trace.native_backtrace->complete,"fresh physical parent identity");
                auto const parent{*trace.native_caller};
                native = command(*owner,hop == 1u ? ::fast_io::concat_fast_io("fin asm") :
                    ::fast_io::concat_fast_io("finish asm ",participant));
                ::fast_io::io::perrln("physical finish: hop=",hop," status=",static_cast<unsigned>(native.status),
                    " execution=",static_cast<unsigned>(native.execution)," reason=",static_cast<unsigned>(native.reason),
                    " next-reason=",static_cast<unsigned>(native.native_next_reason)," timeout=",native.timed_out,
                    " old-stop=",old," stop=",native.stop_identifier," threads=",native.threads.size(),
                    " native-pc-present=",!native.threads.empty() && native.threads[0u].native_pc.has_value(),
                    " expected-parent=",parent.function," function=",native.threads.empty() ? SIZE_MAX : native.threads[0u].location.function,
                    " expected-epoch=",parent.runtime_epoch," epoch=",native.threads.empty() ? 0u : native.threads[0u].location.code_generation,
                    " from-present=",native.native_step_from!=0u," to-present=",native.native_step_to!=0u);
                check(native.status == control::error::none && native.execution == dbg::execution_status::stopped &&
                      native.reason == dbg::stop_reason::native_step && native.stop_identifier > old &&
                      native.threads.size() == 1u && native.threads[0u].native_pc && native.native_step_from && native.native_step_to &&
                      native.threads[0u].location.code_unit == parent.module && native.threads[0u].location.function == parent.function &&
                      native.threads[0u].location.code_generation == parent.runtime_epoch &&
                      native.memory.empty() && native.locals.empty() && native.source_frames.empty(),
                      "genuine recursive parent replaces displayed child and never promotes child locals/source");
                check(dbg::details::format_reply(native,parsed).find("native finish 0x") == 0u,"FastIO finish output names actual native movement");
                auto stale{parsed}; stale.disassembly_stop_identifier = old;
                auto const denied{owner->execute(stale)};
                check(denied.status == control::error::unsupported_command && denied.stop_identifier == native.stop_identifier &&
                      !denied.native_step_from && !denied.native_step_to,"old finish stop cannot move the new parent");
                auto const disasm{command(*owner,::fast_io::concat_fast_io("disassemble ",participant," ",native.stop_identifier," 8"))};
                check(disasm.status == control::error::none && disasm.disassembly_count != 0u && disasm.disassembly_code.pc == native.threads[0u].native_pc,
                      "new parent code query uses a current authenticated capture");
                auto const regtext{::fast_io::concat_fast_io("info registers ",participant," ",native.stop_identifier," all")};
                auto const regcmd{dbg::parse_console_command(::fast_io::string_view{regtext.data(),regtext.size()})};
                auto const regs{owner->execute(regcmd)}; auto const formatted{dbg::details::format_reply(regs,regcmd)};
                auto const sp_index{dbg::native_registers::sp_index(regs.registers.machine)};
                auto const fp_index{dbg::native_registers::fp_index(regs.registers.machine)};
                check(regs.status == control::error::none && sp_index < regs.registers.size() && fp_index < regs.registers.size() &&
                      regs.registers.values[sp_index] == 0u && regs.registers.known_bits[sp_index] == 0u &&
                      regs.registers.values[fp_index] == 0u && regs.registers.known_bits[fp_index] == 0u &&
                      formatted.find("rsp=0x") == ::std::string::npos && formatted.find("sp=0x") == ::std::string::npos &&
                      (regs.registers.machine != dbg::native_registers::architecture::riscv64 ||
                       formatted.find("s0=0x") == ::std::string::npos) &&
                      (regs.registers.machine != dbg::native_registers::architecture::aarch64 ||
                       formatted.find("x29=0x") == ::std::string::npos) && formatted.find("fp=0x") == ::std::string::npos &&
                      formatted.find("rbp=0x") == ::std::string::npos && formatted.find("esp=0x") == ::std::string::npos &&
                      formatted.find("ebp=0x") == ::std::string::npos && formatted.find("cfa=") == ::std::string::npos,
                      "parent registers never expose host/native frame storage");
                ++returns;
            }
#if ((defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))
            if(recursive_running)
            {
                auto const other{dbg::native_step::details::return_other_stack_hits.load(::std::memory_order_relaxed)-other_before};
                auto const skip{dbg::native_step::details::return_skip_hits.load(::std::memory_order_relaxed)-skip_before};
                check(other!=0u && other==skip,"actual recursive same-PC/wrong-SP traps execute the original instruction and re-arm");
                ::fast_io::io::perrln("physical recursive escape: wrong-sp=",other," real-instruction-successor=",skip);
            }
#endif
            auto const root_id{native.stop_identifier};
            auto const root{command(*owner,::fast_io::concat_fast_io("finish asm ",participant))};
            check(root.status == control::error::unsupported_command && root.native_next_reason == dbg::native_next_policy::reason::caller_unwind_unavailable &&
                  root.stop_identifier == root_id && root.threads[0u].native_pc == native.threads[0u].native_pc &&
                  !root.native_step_from && !root.native_step_to,"Wasm root finish cannot expose or execute a host caller stop");
            check(command(*owner,::fast_io::concat_fast_io("continue")).status == control::error::none,"release all retained return owners after real ACK");
        }
        ++queries; break;
    }
    check(queries != 0u, "actual native console inspection required");
    auto const exit{command(*owner,::fast_io::concat_fast_io("wait"))};
    check(exit.status == control::error::none && exit.execution == dbg::execution_status::exited,"actual public wait observes guest exit");
    guest.join(); lib::reset_runtime_state_host_api(); check(owner->detach_resume(),"actual reset after return owner retirement");
    check(result == (cancel ? 77u : 14u), "guest result preserved after physical caller inspection/reset");
    ::fast_io::io::println("debug_native_finish_console: PASS policy=", policy, " actual-native-console=", queries,
        " scenario=",scenario," real-native-finish=",returns," public-native-stack-bytes=0 stale-stop-retained=yes cancel-qualified=",cancel,
        " finish-refusal-qualified=",unsupported,
        " retained-ni-call-before-finish=",mixed," result=", result);
}
