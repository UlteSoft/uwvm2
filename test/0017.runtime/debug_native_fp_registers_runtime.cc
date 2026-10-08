// Actual full-runtime native-next witness; fresh production producer/runtime/CLI+host closure only.
// LLVM DATA component success is not proof of an executed native instruction.
#include <uwvm2/uwvm/run/owned_source.h>
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

namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace dbg = ::uwvm2::uwvm::debugger;
namespace control = ::uwvm2::utils::control;
using clock_type = ::std::chrono::steady_clock;
static void check(bool value, char const* text) noexcept
{
    if(!value)
    {
        ::fast_io::io::perrln("debug_native_fp_registers_runtime: ", ::fast_io::mnp::os_c_str(text));
        ::fast_io::fast_terminate();
    }
}
static dbg::controller_reply command(dbg::controller& owner, ::fast_io::string const& text)
{
    // [owned complete command ... text.size()] command_end
    // [safe                                  ] parser borrows only until execute;
    //  ^^ no integer address or unowned input cursor is requested.
    return owner.execute(dbg::parse_console_command(::fast_io::string_view{text.data(), text.size()}),
                         ::std::chrono::seconds{10});
}
static bool same_registers(dbg::native_registers::snapshot const& a, dbg::native_registers::snapshot const& b) noexcept
{ return a.machine == b.machine && a.size() != 0u && a.values == b.values && a.floating == b.floating; }
int main(int argc, char** argv)
{
    if(argc != 3) { return 2; }
    if(!dbg::native_step::platform_available())
    { ::fast_io::io::println("debug_native_fp_registers_runtime: SKIP actual native backend unavailable"); return 77; }
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
        u8"uwvm-debug-native-next", nullptr,
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
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"native-next-test";
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
    ::fast_io::io::println("debug_native_fp_registers_runtime: SKIP host process adapter unavailable");
    lib::reset_runtime_state_host_api();
    return 77;
#endif
    auto owner{dbg::controller::create(config, 1u)};
    check(owner && owner->status() == control::error::none, "real host-only console authority");
    ::std::weak_ptr<dbg::controller> lifetime{owner};
    auto domain{owner->domain()};
    auto actual_observer{owner->observer()};
    check(lib::llvm_jit_configure_debug_session_host_api(domain, ::std::move(actual_observer),
              lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok,
          "actual runtime observer owns controller until execution drain");
    check(lib::llvm_jit_prepare_debug_host_api(), "actual full fused validation/native publication");
    check(owner->arm_initial_pause(), "empty-domain launch pause");
    auto added{command(*owner, ::fast_io::concat_fast_io("break 0 1 0"))};
    check(added.status == control::error::none && added.breakpoint_identifier != 0u,
          "real emitted instruction breakpoint admitted");
    check(command(*owner, ::fast_io::concat_fast_io("continue")).status == control::error::none,
          "open launch pause before starting guest");

    ::std::uint32_t argument{17u}, result{};
    ::std::atomic_bool finished{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config run{};
        run.entry_function_index = 1u;
        // [host-owned exact result slot] result_end
        // [safe                        ] the ordinary typed ABI owns this slot
        //  ^^ through guest join; it is not a debugger/native address request.
        // [host-owned exact numeric argument] parameter_end
        // [safe                             ] immutable through guest join;
        //  ^^ the actual typed entry ABI consumes precisely these four bytes.
        run.entry_abi_buffers.param_buffer = reinterpret_cast<::std::byte const*>(::std::addressof(argument));
        run.entry_abi_buffers.param_bytes = sizeof(argument);
        run.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(result));
        run.entry_abi_buffers.result_bytes = sizeof(result);
        lib::full_compile_and_run_main_module(u8"native-next-test", run);
        owner->notify_guest_exit(0);
        finished.store(true, ::std::memory_order_release);
    }};
    auto const deadline{clock_type::now() + ::std::chrono::seconds{60}};
    dbg::controller_reply initial{};
    for(;;)
    {
        initial = owner->inspect();
        if(initial.execution == dbg::execution_status::stopped && initial.reason == dbg::stop_reason::breakpoint) { break; }
        check(clock_type::now() < deadline && !finished.load(::std::memory_order_acquire), "bounded actual breakpoint wait");
        ::std::this_thread::yield();
    }
    check(initial.threads.size() == 1u && !initial.threads[0u].native_pc && initial.stop_identifier != 0u,
          "genuine cooperative stop precedes physical trap");
    auto const participant{initial.threads[0u].identifier};
    auto rejected{command(*owner, ::fast_io::concat_fast_io("ni"))};
    check(rejected.status == control::error::unsupported_command &&
          rejected.native_next_reason == dbg::native_next_policy::reason::current_native_trap_required &&
          rejected.stop_identifier == initial.stop_identifier && rejected.threads.size() == 1u && !rejected.threads[0u].native_pc,
          "ni cannot invent physical next from cooperative bridge return");
    auto const notice{dbg::details::format_reply(rejected, dbg::parse_console_command("ni"))};
    check(notice.find("first use step asm THREAD") != ::std::string::npos, "precise native trap requirement is visible");
    ::std::size_t cooperative_refused{}, prefix_retained{}, actual_wasm_steps{};
    auto current{initial};
    while(!current.threads[0u].native_pc)
    {
        check(clock_type::now() < deadline && actual_wasm_steps < 128u, "bounded genuine bootstrap");
        auto const attempted{command(*owner, ::fast_io::concat_fast_io("step asm ", participant))};
        if(attempted.status == control::error::none)
        {
            check(attempted.stop_identifier > current.stop_identifier && attempted.reason == dbg::stop_reason::native_step &&
                  attempted.threads.size() == 1u && attempted.threads[0u].native_pc &&
                  *attempted.threads[0u].native_pc == attempted.native_step_to, "real kernel trap establishes FP query authority");
            current = attempted; break;
        }
        check(attempted.status == control::error::unsupported_command && attempted.stop_identifier == current.stop_identifier &&
              !attempted.threads[0u].native_pc, "unproved cooperative bootstrap retains its exact stop");
        ++cooperative_refused;
        auto const unavailable{command(*owner, ::fast_io::concat_fast_io("info registers"))};
        check(unavailable.status != control::error::none, "cooperative stop grants no physical register values");
        current = command(*owner, ::fast_io::concat_fast_io("step wasm ", participant)); ++actual_wasm_steps;
        check(current.status == control::error::none && current.execution == dbg::execution_status::stopped &&
              current.threads.size() == 1u && !current.threads[0u].native_pc, "bootstrap search uses only actual Wasm checkpoints");
    }
    auto registers{command(*owner, ::fast_io::concat_fast_io("info registers"))};
    check(registers.status == control::error::none && registers.registers_stop_identifier == current.stop_identifier &&
          registers.registers.pc() == current.native_step_to, "actual trap registers/private capture authentication");
    check(registers.registers.floating.available,
          "real current Wasm-owned kernel trap supplied the complete baseline floating register values");
    for(::std::size_t i{}; i != dbg::native_registers::max_fp_registers; ++i)
    {
        auto const width{registers.registers.floating.values[i].width};
        check(i < 16u ? (width == 0u || width == 4u || width == 8u || width == 16u) : width == 0u,
              "public FP bits require numeric locations; x87/control values remain private");
    }
    for(auto const* spelling : {"$xmm0", "xmm15", "$st(0)", "st7", "fcw", "mxcsr"})
    {
        auto const text{::fast_io::concat_fast_io("info registers ", ::fast_io::mnp::os_c_str(spelling))};
        auto const parsed{dbg::parse_console_command(::fast_io::string_view{text.data(), text.size()})};
        auto const queried{owner->execute(parsed)};
        auto const display{dbg::details::format_reply(queried, parsed)};
        check(queried.status == control::error::none && queried.registers_stop_identifier == current.stop_identifier &&
              same_registers(registers.registers, queried.registers) && display.find("error:") == ::std::string::npos &&
              (display.find("=0x") != ::std::string::npos || display.find("=unavailable") != ::std::string::npos),
              "CLI and DAP-debugConsole common formatter select actual immutable fp values at the same stop");
    }
    auto const all_parsed{dbg::parse_console_command("info all-registers")};
    auto const all_queried{owner->execute(all_parsed)};
    auto const all_display{dbg::details::format_reply(all_queried, all_parsed)};
    check(all_queried.status == control::error::none && same_registers(registers.registers, all_queried.registers) &&
          all_display.find("  xmm15=") != ::std::string::npos && all_display.find("  st7=unavailable") != ::std::string::npos &&
          all_display.find("  rflags=unavailable") != ::std::string::npos && all_display.find("  rsp=unavailable") != ::std::string::npos,
          "explicit all-registers preserves names while hiding unqualified runtime/control values");
    auto const default_display{dbg::details::format_reply(registers, dbg::parse_console_command("info registers"))};
    check(default_display.find("  xmm0=") == ::std::string::npos && default_display.find("  rip=0x") != ::std::string::npos,
          "default GPR display remains compatible with existing CLI/DAP boundary witnesses");
    for(auto const* spelling : {"fip", "fdp", "ymm0", "zmm0"})
    {
        auto const text{::fast_io::concat_fast_io("info registers ", ::fast_io::mnp::os_c_str(spelling))};
        auto const parsed{dbg::parse_console_command(::fast_io::string_view{text.data(), text.size()})};
        auto const queried{owner->execute(parsed)};
        auto const display{dbg::details::format_reply(queried, parsed)};
        check(queried.stop_identifier == current.stop_identifier && same_registers(registers.registers, queried.registers) &&
              display.find("error: register is unavailable") != ::std::string::npos,
              "no x87 host address or unqualified XSAVE extension can be read or mutate the genuine stop");
    }
    auto const wrong{participant == UINT64_MAX ? participant - 1u : participant + 1u};
    rejected = command(*owner, ::fast_io::concat_fast_io("ni ", wrong));
    auto unchanged{command(*owner, ::fast_io::concat_fast_io("info registers"))};
    check(rejected.status != control::error::none && rejected.stop_identifier == current.stop_identifier &&
          unchanged.status == control::error::none && same_registers(registers.registers, unchanged.registers),
          "foreign requested thread cannot alter current native PC/GPR/stop");
    auto invalid_script{command(*owner, ::fast_io::concat_fast_io("wasm-script trace wasm on; ni"))};
    auto trace_state{command(*owner, ::fast_io::concat_fast_io("trace wasm read"))};
    check(invalid_script.status == control::error::malformed && trace_state.status == control::error::none &&
          !trace_state.wasm_trace_enabled && trace_state.stop_identifier == current.stop_identifier,
          "script rejects native execution before its earlier trace-policy child runs");
    // Complete execution proof stays private. Public rows may be hidden; their
    // absence must not be replaced by reading the raw function or native PC.
    ::std::size_t positive{}, control_refused{}, same_owner_branches{}, qualified{}, hidden{};
    for(::std::size_t attempt{}; attempt != 1024u && clock_type::now() < deadline; ++attempt)
    {
        auto const before{owner->inspect()};
        registers = command(*owner, ::fast_io::concat_fast_io("info registers"));
        auto const image{command(*owner, ::fast_io::concat_fast_io("disassemble ", participant, " ", before.stop_identifier, " 1"))};
        check(before.execution == dbg::execution_status::stopped && before.reason == dbg::stop_reason::native_step &&
              before.threads.size() == 1u && before.threads[0u].native_pc &&
              registers.status == control::error::none && registers.registers_stop_identifier == before.stop_identifier &&
              registers.registers.pc() == *before.threads[0u].native_pc && image.status == control::error::none &&
              image.disassembly_count == 1u && image.disassembly_stop_identifier == before.stop_identifier &&
              image.disassembly_code.pc == registers.registers.pc() && image.disassembly_code.size == 0u &&
              image.disassembly_code.function_generation != 0u && image.disassembly_code.runtime_epoch != 0u,
              "authentic current owner/stop identity never exports raw code storage");
        auto const& row{image.disassembly[0u]};
        if(!row)
        {
            ++hidden;
            check(row.size == 0u && row.text[0u] == '\0' && !image.disassembly_destinations[0u], "hidden runtime row has no text or target");
            for(auto byte:row.bytes) { check(byte == 0u, "hidden runtime byte storage is zero"); }
        }
        auto next{command(*owner, attempt % 2u == 0u ? ::fast_io::concat_fast_io("ni") : ::fast_io::concat_fast_io("nexti ", participant))};
        if(next.status == control::error::none)
        {
            check(!next.timed_out && next.execution == dbg::execution_status::stopped &&
                  next.reason == dbg::stop_reason::native_step && next.stop_identifier > before.stop_identifier &&
                  next.native_step_from == registers.registers.pc() && next.threads.size() == 1u && next.threads[0u].native_pc &&
                  *next.threads[0u].native_pc == next.native_step_to,
                  "NI/nexti establishes a new genuine physical trap");
            if(row)
            {
                check(next.native_instruction && next.native_instruction.pc == row.pc && next.native_instruction.size == row.size &&
                      next.native_instruction.bytes == row.bytes, "executed public bytes equal the prior qualified Wasm instruction");
                ++qualified;
            }
            else { check(!next.native_instruction, "hidden executed scaffolding remains hidden in NI replies"); }
            auto const actual{command(*owner, ::fast_io::concat_fast_io("info registers"))};
            check(actual.status == control::error::none && actual.registers.pc() == next.native_step_to &&
                  actual.registers_stop_identifier == next.stop_identifier, "actual hardware PC confirms NI");
            if(positive == 0u)
            {
                auto stale{dbg::parse_console_command("ni")}; stale.disassembly_stop_identifier = before.stop_identifier;
                auto const denied{owner->execute(stale)};
                auto const retained{command(*owner, ::fast_io::concat_fast_io("info registers"))};
                check(denied.status == control::error::unsupported_command &&
                      denied.native_next_reason == dbg::native_next_policy::reason::current_native_trap_required &&
                      denied.stop_identifier == next.stop_identifier && retained.status == control::error::none &&
                      same_registers(actual.registers, retained.registers), "stale stop cannot open the new native gate");
            }
            ++positive; current = next; continue;
        }
        check(next.status == control::error::unsupported_command && !next.timed_out &&
              next.stop_identifier == before.stop_identifier && next.execution == dbg::execution_status::stopped &&
              next.native_next_reason != dbg::native_next_policy::reason::none, "NI refusal retains the physical stop before unpark");
        auto actual{command(*owner, ::fast_io::concat_fast_io("info registers"))};
        check(actual.status == control::error::none && same_registers(registers.registers, actual.registers), "NI refusal preserves public numeric bits/PC");
        auto const single{command(*owner, ::fast_io::concat_fast_io("step asm ", participant))};
        if(single.status == control::error::none)
        {
            check(single.reason == dbg::stop_reason::native_step && single.stop_identifier > before.stop_identifier &&
                  single.native_step_from == registers.registers.pc() && single.threads.size() == 1u && single.threads[0u].native_pc &&
                  *single.threads[0u].native_pc == single.native_step_to, "SI successor is a genuine owned Wasm trap");
            ++same_owner_branches; current = single; continue;
        }
        actual = command(*owner, ::fast_io::concat_fast_io("info registers"));
        check(single.status == control::error::unsupported_command && !single.timed_out &&
              single.stop_identifier == before.stop_identifier && actual.status == control::error::none &&
              same_registers(registers.registers, actual.registers), "SI refuses before any VM/caller instruction can execute");
        using enum dbg::native_next_policy::reason;
        if(single.native_next_reason == call_continuation_unavailable || single.native_next_reason == caller_unwind_unavailable)
        { ++control_refused; }
        auto const display{dbg::details::format_reply(single, dbg::parse_console_command("step asm 1"))};
        check(display.find("native step unavailable:") != ::std::string::npos && display.find("current stop retained") != ::std::string::npos,
              "precise retained-stop reason is visible");
        current = single;
        if(positive != 0u && qualified != 0u && hidden != 0u && control_refused != 0u) { break; }
        current = command(*owner, ::fast_io::concat_fast_io("step wasm ", participant));
        check(current.status == control::error::none && current.execution == dbg::execution_status::stopped &&
              current.stop_identifier > before.stop_identifier && current.threads.size() == 1u && !current.threads[0u].native_pc,
              "only actual Wasm stepping advances past a retained host/caller boundary");
        auto const boot{command(*owner, ::fast_io::concat_fast_io("step asm ", participant))};
        check(boot.status == control::error::none && boot.reason == dbg::stop_reason::native_step &&
              boot.stop_identifier > current.stop_identifier && boot.threads.size() == 1u && boot.threads[0u].native_pc,
              "new native bootstrap uses a real emitted Wasm stop");
        current = boot;
    }
    check(positive != 0u && qualified != 0u && hidden != 0u && control_refused != 0u,
          "real NI execution, qualified Wasm bytes, hidden runtime bytes and SI call/caller refusal all required");
    // Test actual observer retirement from the retained trapped instruction;
    // reset must retire this native borrower before execution-lease drain.
    lib::reset_runtime_state_host_api();
    rejected = command(*owner, ::fast_io::concat_fast_io("ni"));
    check(domain->is_closed() && rejected.status != control::error::none && owner->detach_resume(),
          "closed/reset native owner grants no further next capability");
    auto const end{clock_type::now() + ::std::chrono::seconds{10}};
    while(!finished.load(::std::memory_order_acquire))
    { check(clock_type::now() < end, "actual guest remained in retired native gate"); ::std::this_thread::yield(); }
    guest.join();
    check(result == 125u, "Core3 input-dependent arithmetic and return_call preserved after next/refusal/reset");
    owner.reset(); check(lifetime.expired(), "actual observer/controller retired after reset and guest join");
    ::fast_io::io::println("debug_native_fp_registers_runtime: PASS actual native next policy=", policy,
        " ordinary-executed=", ::fast_io::mnp::dec(positive), " same-owner-branch-executed=", ::fast_io::mnp::dec(same_owner_branches),
        " call-or-return-SI-refused=", ::fast_io::mnp::dec(control_refused),
        " cooperative-refused=", ::fast_io::mnp::dec(cooperative_refused),
        " native-prefix-retained=", ::fast_io::mnp::dec(prefix_retained),
        " actual-wasm-steps=", ::fast_io::mnp::dec(actual_wasm_steps),
        " genuine-native-trap=yes Core3-return-call-and-nondefaultable-gc-local=yes");
}
