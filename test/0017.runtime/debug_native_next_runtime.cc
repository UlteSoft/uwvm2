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
        ::fast_io::io::perrln("debug_native_next_runtime: ", ::fast_io::mnp::os_c_str(text));
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
static bool same_registers(dbg::native_registers::snapshot const& a, dbg::native_registers::snapshot const& b) noexcept
{ return a.machine == b.machine && a.size() != 0u && a.values == b.values; }
int main(int argc, char** argv)
{
    if(argc != 3) { return 2; }
    if(!dbg::native_step::platform_available())
    { ::fast_io::io::println("debug_native_next_runtime: SKIP actual native backend unavailable"); return 77; }
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
    ::fast_io::io::println("debug_native_next_runtime: SKIP host process adapter unavailable");
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
    auto added{command(*owner, ::fast_io::concat_std("break 0 1 0"))};
    check(added.status == control::error::none && added.breakpoint_identifier != 0u,
          "real emitted instruction breakpoint admitted");
    check(command(*owner, ::fast_io::concat_std("continue")).status == control::error::none,
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
    auto const deadline{clock_type::now() + ::std::chrono::seconds{45}};
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
    auto rejected{command(*owner, ::fast_io::concat_std("ni"))};
    check(rejected.status == control::error::unsupported_command &&
          rejected.native_next_reason == dbg::native_next_policy::reason::current_native_trap_required &&
          rejected.stop_identifier == initial.stop_identifier && rejected.threads.size() == 1u && !rejected.threads[0u].native_pc,
          "ni cannot invent physical next from cooperative bridge return");
    auto const notice{dbg::details::format_reply(rejected, dbg::parse_console_command("ni"))};
    check(notice.find("first use step asm THREAD") != ::std::string::npos, "precise native trap requirement is visible");
    ::std::size_t actual_wasm_steps{};
    auto genuine_wasm_step{[&](dbg::controller_reply const& previous)
    {
        check(actual_wasm_steps != 128u && clock_type::now() < deadline && !finished.load(::std::memory_order_acquire),
              "finite real Wasm stop search exhausted before required branch witness");
        auto const next{command(*owner, ::fast_io::concat_std("step wasm ", participant))}; ++actual_wasm_steps;
        check(next.status == control::error::none && !next.timed_out && next.execution == dbg::execution_status::stopped &&
              next.stop_identifier > previous.stop_identifier && next.threads.size() == 1u &&
              next.threads[0u].identifier == participant && !next.threads[0u].native_pc,
              "actual Wasm stepping restores cooperative stop without permitting a VM native stop");
        return next;
    }};
    auto seek_native{[&](dbg::controller_reply cooperative)
    {
        for(;;)
        {
            check(!finished.load(::std::memory_order_acquire) && clock_type::now() < deadline &&
                  cooperative.threads.size() == 1u && !cooperative.threads[0u].native_pc, "real cooperative bootstrap only");
            auto next{command(*owner, ::fast_io::concat_std("step asm ", participant))};
            if(next.status == control::error::none)
            {
                check(!next.timed_out && next.reason == dbg::stop_reason::native_step &&
                      next.stop_identifier > cooperative.stop_identifier && next.threads.size() == 1u && next.threads[0u].native_pc &&
                      *next.threads[0u].native_pc == next.native_step_to, "new actual kernel trap established");
                return next;
            }
            check(next.status == control::error::unsupported_command && !next.timed_out &&
                  next.stop_identifier == cooperative.stop_identifier && next.threads.size() == 1u && !next.threads[0u].native_pc &&
                  next.threads[0u].location == cooperative.threads[0u].location,
                  "unproved bootstrap refuses before crossing with cooperative state unchanged");
            cooperative = genuine_wasm_step(next);
        }
    }};
    auto current{seek_native(initial)};
    check(current.status == control::error::none && !current.timed_out && current.reason == dbg::stop_reason::native_step &&
          current.execution == dbg::execution_status::stopped && current.native_instruction && current.threads.size() == 1u &&
          current.threads[0u].native_pc && *current.threads[0u].native_pc == current.native_step_to,
          "actual single-instruction native trap established by existing backend");
    auto registers{command(*owner, ::fast_io::concat_std("info registers"))};
    check(registers.status == control::error::none && registers.registers_stop_identifier == current.stop_identifier &&
          registers.registers.pc() == current.native_step_to, "actual trap registers/private capture authentication");
    auto const wrong{participant == UINT64_MAX ? participant - 1u : participant + 1u};
    rejected = command(*owner, ::fast_io::concat_std("ni ", wrong));
    auto unchanged{command(*owner, ::fast_io::concat_std("info registers"))};
    check(rejected.status != control::error::none && rejected.stop_identifier == current.stop_identifier &&
          unchanged.status == control::error::none && same_registers(registers.registers, unchanged.registers),
          "foreign requested thread cannot alter current native PC/GPR/stop");
    auto invalid_script{command(*owner, ::fast_io::concat_std("wasm-script trace wasm on; ni"))};
    auto trace_state{command(*owner, ::fast_io::concat_std("trace wasm read"))};
    check(invalid_script.status == control::error::malformed && trace_state.status == control::error::none &&
          !trace_state.wasm_trace_enabled && trace_state.stop_identifier == current.stop_identifier,
          "script rejects native execution before its earlier trace-policy child runs");
    ::std::size_t ordinary_executed{}, conditional_branch_executed{}, branch_current_proved{},
        branch_stale_proved{}, ordinary_stale_proved{}, control_refused{};
    auto stale_stop_refusal{[&](::std::uint64_t stale_stop, dbg::controller_reply const& actual_stop,
        dbg::native_registers::snapshot const& actual_registers)
    {
        auto stale{dbg::parse_console_command("ni")};
        stale.disassembly_stop_identifier = stale_stop;
        auto const rejected{owner->execute(stale)};
        auto const retained{command(*owner, ::fast_io::concat_std("info registers"))};
        check(rejected.status == control::error::unsupported_command &&
              rejected.native_next_reason == dbg::native_next_policy::reason::current_native_trap_required &&
              rejected.stop_identifier == actual_stop.stop_identifier && retained.status == control::error::none &&
              retained.registers_stop_identifier == actual_stop.stop_identifier && same_registers(actual_registers, retained.registers),
              "old-stop NI cannot release current physical gate or change any current kernel GPR");
    }};
    for(::std::size_t attempt{}; attempt != 1024u; ++attempt)
    {
        check(clock_type::now() < deadline && !finished.load(::std::memory_order_acquire),
              "finite actual NI witness budget exhausted");
        auto const before{owner->inspect()};
        registers = command(*owner, ::fast_io::concat_std("info registers"));
        auto image{command(*owner, ::fast_io::concat_std("disassemble-range ", participant, " ", before.stop_identifier, " 1 0 0 1"))};
        auto const& bytes{image.disassembly_code};
        // [fixed complete reply row array0 ... capacity32) end
        // [safe] row0 is in the array; valid count/length below must hold
        // before its OWNED bytes can become an MC span.
        auto const& copied_instruction{image.disassembly[0u]};
        check(before.execution == dbg::execution_status::stopped && before.reason == dbg::stop_reason::native_step &&
              registers.status == control::error::none && registers.registers.size() != 0u &&
              registers.registers_stop_identifier == before.stop_identifier &&
              image.status == control::error::none && image.disassembly_count == 1u &&
              image.disassembly_stop_identifier == before.stop_identifier && bytes.native_instruction_stop &&
              copied_instruction && copied_instruction.pc == bytes.pc && bytes.pc == registers.registers.pc() &&
              copied_instruction.size <= copied_instruction.bytes.size() &&
              image.disassembly_owner_begin <= bytes.pc && bytes.pc < image.disassembly_owner_end &&
              copied_instruction.size <= image.disassembly_owner_end - bytes.pc &&
              bytes.function_generation != 0u && bytes.runtime_epoch != 0u,
              "actual private runtime copy authorizes this current complete native instruction");
        // [owned authenticated RANGE row bytes ... size<=fixed32] row_end
        // [safe] RANGE metadata contains no raw code bytes. Both fixed array
        //  ^^ and true owner bounds above precede this owned-row span; no native
        // address is dereferenced. The result remains DATA, not permission.
        dbg::native_owned_instruction_semantics::decoder decoder{bytes.target};
        check(bool(decoder), "same actual current engine target semantic MC context");
        auto const decoded{decoder.decode(bytes.pc, {copied_instruction.bytes.data(), copied_instruction.size})};
        auto const selection{dbg::native_next_policy::choose(decoded, copied_instruction.size)};
        auto next{command(*owner, attempt % 2u == 0u ? ::fast_io::concat_std("ni") : ::fast_io::concat_std("nexti ", participant))};
        if(!selection && decoded.safe_for_call_continuation() && next.status == control::error::none)
        {
            // A canonical runtime-issued returning call is a valid NI result.
            // This OPTIONAL compatibility witness increments NONE of the six
            // mandatory ordinary/conditional/current/stale/refusal counters.
            auto const size{decoded.semantics().size};
            check(size != 0u && size <= UINTPTR_MAX - bytes.pc && bytes.pc + size < image.disassembly_owner_end,
                  "bounded authentic caller continuation arithmetic");
            auto const expected{bytes.pc + size};
            check(!next.timed_out && next.execution == dbg::execution_status::stopped &&
                  next.reason == dbg::stop_reason::native_step && next.stop_identifier > before.stop_identifier &&
                  next.native_step_from == bytes.pc && next.native_step_to == expected &&
                  next.native_next_reason == dbg::native_next_policy::reason::none,
                  "actual permitted typed CALL NI stops at original caller continuation");
            auto const actual{command(*owner, ::fast_io::concat_std("info registers"))};
            auto const proof{command(*owner, ::fast_io::concat_std("disassemble-range ", participant, " ", next.stop_identifier, " 1 0 0 1"))};
            check(actual.status == control::error::none && actual.registers_stop_identifier == next.stop_identifier &&
                  actual.registers.pc() == expected && actual.registers.sp() == registers.registers.sp() &&
                  proof.status == control::error::none && proof.disassembly_stop_identifier == next.stop_identifier &&
                  proof.disassembly_code.native_instruction_stop && proof.disassembly_code.pc == expected &&
                  proof.disassembly_code.module == bytes.module && proof.disassembly_code.function == bytes.function &&
                  proof.disassembly_code.function_generation == bytes.function_generation && proof.disassembly_code.runtime_epoch == bytes.runtime_epoch &&
                  proof.disassembly_owner_begin == image.disassembly_owner_begin && proof.disassembly_owner_end == image.disassembly_owner_end,
                  "returned typed call reauthenticates actual SP/caller/owner/current source generation/epoch");
            stale_stop_refusal(before.stop_identifier, next, actual.registers);
            current = next; continue;
        }
        if(!selection && decoded.safe_for_same_owner_direct_branch() && next.status == control::error::none)
        {
            // Native policy DATA alone could not authorize this branch. The
            // actual controller proved BOTH successors in its copied complete
            // current owner BEFORE opening the physical trap gate.
            auto const destination{decoded.destination()};
            check(destination && decoded.semantics().size <= UINTPTR_MAX - bytes.pc,
                  "bounded exact decoded branch successor arithmetic");
            auto const fallthrough{bytes.pc + decoded.semantics().size};
            check(destination.display_pc >= image.disassembly_owner_begin &&
                  destination.display_pc < image.disassembly_owner_end &&
                  (!destination.conditional || (fallthrough >= image.disassembly_owner_begin && fallthrough < image.disassembly_owner_end)),
                  "actual NI branch cannot leave published Wasm owner");
            check(!next.timed_out && next.reason == dbg::stop_reason::native_step &&
                  next.stop_identifier > before.stop_identifier && next.native_step_from == bytes.pc &&
                  (next.native_step_to == destination.display_pc || (destination.conditional && next.native_step_to == fallthrough)) &&
                  next.native_next_reason == dbg::native_next_policy::reason::none,
                  "one actual NI branch stops at exactly one of pre-proven successors");
            auto const after{command(*owner, ::fast_io::concat_std("info registers"))};
            check(after.status == control::error::none && after.registers.pc() == next.native_step_to &&
                  after.registers_stop_identifier == next.stop_identifier,
                  "actual kernel registers witness selected NI branch successor");
            auto const proof{command(*owner, ::fast_io::concat_std("disassemble-range ", participant, " ", next.stop_identifier, " 1 0 0 1"))};
            check(proof.status == control::error::none && proof.disassembly_stop_identifier == next.stop_identifier &&
                  proof.disassembly_code.pc == next.native_step_to && proof.disassembly_code.module == bytes.module &&
                  proof.disassembly_code.function == bytes.function && proof.disassembly_code.function_generation == bytes.function_generation &&
                  proof.disassembly_code.runtime_epoch == bytes.runtime_epoch &&
                  proof.disassembly_owner_begin == image.disassembly_owner_begin && proof.disassembly_owner_end == image.disassembly_owner_end,
                  "fresh true runtime copy authenticates branch successor owner/current generation/epoch");
            if(destination.conditional)
            {
                ++conditional_branch_executed; ++branch_current_proved;
                stale_stop_refusal(before.stop_identifier, next, after.registers); ++branch_stale_proved;
            }
            current = next; continue;
        }
        if(selection)
        {
            // [authenticated owner pc][complete size<=copied remaining] end
            // [safe                 ] check scalar addition before deriving
            //  ^^ the expected ordinary fallthrough; no pointer is formed.
            check(selection.instruction_size <= UINTPTR_MAX - bytes.pc, "bounded native fallthrough arithmetic");
            check(next.status == control::error::none && !next.timed_out && next.reason == dbg::stop_reason::native_step &&
                  next.execution == dbg::execution_status::stopped && next.stop_identifier > before.stop_identifier &&
                  next.native_step_from == bytes.pc && next.native_step_to == bytes.pc + selection.instruction_size &&
                  next.native_instruction && next.native_instruction.size == selection.instruction_size &&
                  next.native_next_reason == dbg::native_next_policy::reason::none,
                  "one ordinary native instruction executed, fresh real trap exactly at actual decoded fallthrough");
            auto actual{command(*owner, ::fast_io::concat_std("info registers"))};
            check(actual.status == control::error::none && actual.registers.pc() == next.native_step_to &&
                  actual.registers_stop_identifier == next.stop_identifier, "fresh actual trap GPR confirms native next");
            auto const executed_text{dbg::details::format_reply(next, dbg::parse_console_command("ni"))};
            check(executed_text.find("native instruction 0x") != ::std::string::npos,
                  "successful next displays the actual owned decoded instruction");
            if(ordinary_stale_proved == 0u)
            { stale_stop_refusal(before.stop_identifier, next, actual.registers); ++ordinary_stale_proved; }
            ++ordinary_executed; current = next;
        }
        else
        {
            using enum dbg::native_next_policy::reason;
            check(selection.unavailable_reason == call_continuation_unavailable ||
                  selection.unavailable_reason == caller_unwind_unavailable ||
                  selection.unavailable_reason == branch_continuation_unavailable,
                  "fixture must witness an actual call/return/branch refusal, not merely an undecodable instruction");
            auto after{owner->inspect()};
            auto actual{command(*owner, ::fast_io::concat_std("info registers"))};
            check(next.status == control::error::unsupported_command && !next.timed_out &&
                  (next.native_next_reason == selection.unavailable_reason ||
                   (selection.unavailable_reason == call_continuation_unavailable && next.native_next_reason == call_target_unavailable)) &&
                  next.stop_identifier == before.stop_identifier &&
                  after.stop_identifier == before.stop_identifier && after.reason == dbg::stop_reason::native_step &&
                  actual.status == control::error::none && same_registers(registers.registers, actual.registers) &&
                  actual.registers_stop_identifier == registers.registers_stop_identifier,
                  "control-flow next refuses before unpark/release and retains actual PC/all GPR/stop identity");
            ++control_refused;
            if(ordinary_executed != 0u && conditional_branch_executed != 0u &&
               branch_current_proved != 0u && branch_stale_proved != 0u && ordinary_stale_proved != 0u) { break; }
            current = seek_native(genuine_wasm_step(next));
        }
    }
    check(ordinary_executed >= 1u && conditional_branch_executed >= 1u &&
          branch_current_proved == conditional_branch_executed && branch_stale_proved == conditional_branch_executed &&
          ordinary_stale_proved >= 1u && control_refused >= 1u,
          "missing actual ordinary/conditional NI/current-owner/stale-stop/refusal witness is FAIL, never PASS");
    // Test actual observer retirement from the retained trapped instruction;
    // reset must retire this native borrower before execution-lease drain.
    lib::reset_runtime_state_host_api();
    rejected = command(*owner, ::fast_io::concat_std("ni"));
    check(domain->is_closed() && rejected.status != control::error::none && owner->detach_resume(),
          "closed/reset native owner grants no further next capability");
    auto const end{clock_type::now() + ::std::chrono::seconds{10}};
    while(!finished.load(::std::memory_order_acquire))
    { check(clock_type::now() < end, "actual guest remained in retired native gate"); ::std::this_thread::yield(); }
    guest.join();
    check(result == 125u, "Core3 input-dependent arithmetic and return_call preserved after next/refusal/reset");
    owner.reset(); check(lifetime.expired(), "actual observer/controller retired after reset and guest join");
    ::fast_io::io::println("debug_native_next_runtime: PASS actual native next policy=", policy,
        " ordinary-executed=", ::fast_io::mnp::dec(ordinary_executed),
        " conditional-branch-executed=", ::fast_io::mnp::dec(conditional_branch_executed),
        " branch-current-proved=", ::fast_io::mnp::dec(branch_current_proved),
        " branch-stale-proved=", ::fast_io::mnp::dec(branch_stale_proved),
        " ordinary-stale-proved=", ::fast_io::mnp::dec(ordinary_stale_proved),
        " control-flow-refused=", ::fast_io::mnp::dec(control_refused),
        " genuine-native-trap=yes Core3-return-call-and-nondefaultable-gc-local=yes");
}
