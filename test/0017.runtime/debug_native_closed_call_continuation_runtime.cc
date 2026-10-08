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
        ::fast_io::io::perrln("debug_native_closed_call_continuation_runtime: ", ::fast_io::mnp::os_c_str(text));
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
{ return a.machine == b.machine && a.size() != 0u && a.values == b.values; }
int main(int argc, char** argv)
{
    if(argc != 4) { return 2; }
#if !defined(__linux__) || (!defined(__x86_64__) && !UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE) || \
    !defined(UWVM2_ENABLE_DEBUG_NATIVE_CONTINUATION_LINUX_PRODUCT) || UWVM2_ENABLE_DEBUG_NATIVE_CONTINUATION_LINUX_PRODUCT != 1
    ::fast_io::io::println("debug_native_closed_call_continuation_runtime: UNQUALIFIED product/backend switch unavailable");
    return 77;
#elif !UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
    if(!dbg::native_continuation_linux::sdk_available)
    { ::fast_io::io::println("debug_native_closed_call_continuation_runtime: UNQUALIFIED precise-event SDK unavailable"); return 77; }
#endif
    if(!dbg::native_step::platform_available())
    { ::fast_io::io::println("debug_native_closed_call_continuation_runtime: SKIP actual native backend unavailable"); return 77; }
    auto const policy{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}};
    if(policy != "instruction" && policy != "unwind") { return 2; }
    auto const scenario{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[3])}};
    if(scenario != "return" && scenario != "exception") { return 2; }
    bool const throwing{scenario == "exception"};
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
    features.disable_memory64 = false; features.explicit_enable_memory64 = true;
    features.disable_exceptions = false; features.explicit_enable_exceptions = true;
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
    ::fast_io::io::println("debug_native_closed_call_continuation_runtime: SKIP host process adapter unavailable");
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

    ::std::uint32_t argument{throwing ? UINT32_MAX : 17u}, result{};
    // Unsigned Wasm i32 arithmetic wraps modulo2^32. Both actual GC field5
    // and memory64 store/load participate in the same final result. The modern
    // exception case transfers tag payload9 across the real called function.
    auto const expected_result{static_cast<::std::uint32_t>((argument ^ 41u) + 31u + (throwing ? 9u : argument + 2u))};
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
    auto const deadline{clock_type::now() + ::std::chrono::seconds{300}};
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
    ::std::size_t actual_wasm_steps{}, ordinary{}, calls{}, hidden{}, retained_si{}, unavailable_ni{},
        call_current_proved{}, call_stale_proved{}, call_abandoned{}, abandon_stale_proved{}, integer_fp_carriers{};
    auto genuine_wasm_step{[&](dbg::controller_reply const& previous)
    {
        check(actual_wasm_steps != 256u && clock_type::now() < deadline && !finished.load(::std::memory_order_acquire),
              "finite genuine Wasm stop search exhausted");
        auto const next{command(*owner, ::fast_io::concat_fast_io("step wasm ", participant))}; ++actual_wasm_steps;
        check(next.status == control::error::none && !next.timed_out && next.execution == dbg::execution_status::stopped &&
              next.stop_identifier > previous.stop_identifier && next.threads.size() == 1u &&
              next.threads[0u].identifier == participant && !next.threads[0u].native_pc,
              "only true Wasm stepping advances cooperative search; no requested machine PC");
        return next;
    }};
    auto seek_native{[&](dbg::controller_reply current)
    {
        for(;;)
        {
            check(!finished.load(::std::memory_order_acquire) && clock_type::now() < deadline &&
                  current.threads.size() == 1u && !current.threads[0u].native_pc,
                  "finite actual cooperative bootstrap");
            auto next{command(*owner, ::fast_io::concat_fast_io("step asm ", participant))};
            if(next.status == control::error::none)
            {
                check(!next.timed_out && next.reason == dbg::stop_reason::native_step &&
                      next.stop_identifier > current.stop_identifier && next.threads.size() == 1u && next.threads[0u].native_pc &&
                      *next.threads[0u].native_pc == next.native_step_to,
                      "genuine selected kernel trap established");
                return next;
            }
            check(next.status == control::error::unsupported_command && !next.timed_out &&
                  next.stop_identifier == current.stop_identifier && next.threads.size() == 1u && !next.threads[0u].native_pc &&
                  next.threads[0u].location == current.threads[0u].location,
                  "unproved bootstrap instruction refuses BEFORE execution; original stop intact");
            current = genuine_wasm_step(next);
        }
    }};
    auto current{seek_native(initial)};
    for(::std::size_t attempt{}; attempt != 8192u; ++attempt)
    {
        check(clock_type::now() < deadline && !finished.load(::std::memory_order_acquire), "finite actual native control budget");
        auto const before{owner->inspect()};
        check(before.stop_identifier == current.stop_identifier && before.reason == dbg::stop_reason::native_step &&
              before.threads.size() == 1u && before.threads[0u].native_pc,
              "one exact current physical stop; no copied trap identity");
        auto const registers{command(*owner, ::fast_io::concat_fast_io("info registers"))};
        auto const image{command(*owner, ::fast_io::concat_fast_io("disassemble-range ", participant, " ", before.stop_identifier, " 1 0 0 1"))};
        auto const& identity{image.disassembly_code};
        check(registers.status == control::error::none && image.status == control::error::none &&
              registers.registers_stop_identifier == before.stop_identifier && image.disassembly_stop_identifier == before.stop_identifier &&
              identity.native_instruction_stop && registers.registers.pc() == identity.pc &&
              identity.pc == *before.threads[0u].native_pc && image.disassembly_count == 1u &&
              image.disassembly_owner_begin <= identity.pc && identity.pc < image.disassembly_owner_end &&
              identity.module == before.threads[0u].location.code_unit && identity.function == before.threads[0u].location.function &&
              identity.runtime_epoch != 0u && identity.function_generation != 0u,
              "actual current physical stop and caller publication authenticate the public view");
        auto const& raw{registers.registers};
        for(auto index:{dbg::native_registers::sp_index(raw.machine),dbg::native_registers::fp_index(raw.machine)})
        { check(index<raw.size() && raw.known_bits[index]==0u && raw.values[index]==0u,
                "native SP/FP remain private on the actual target"); }
        for(::std::size_t index{};index!=raw.size();++index)
        { if(raw.known_bits[index]==0u) { check(raw.values[index]==0u,"unproved native register payload cleared"); } }
        ::std::size_t fp_index{};
        for(auto const& value:raw.floating.values)
        {
            if(value.width == 0u)
            {
                for(auto byte:value.bytes) { check(byte==0u,"unproved floating/vector register payload cleared"); }
                ++fp_index; continue;
            }
            // i686 uses SSE2 registers for complete integer bit carriers.
            // This fixture has i32 values and the literal memory64 offset
            // i64.const0, but no f32/f64/v128 values. A nonzero public width
            // therefore needs BOTH the same authenticated numeric location
            // and exact fixture integer bits, never a blanket FP exception.
            check(raw.machine == dbg::native_registers::architecture::i686 && fp_index < 8u &&
                  (value.width == 4u || value.width == 8u),
                  "only actual i686 integer carriers are admitted by this fixture");
            // The same-stop checks above authenticate the public value-only
            // projection. The controller joins each width to its private
            // compiler numeric-location table; that table is not a public
            // display DTO or a native address/read capability.
            ::std::uint64_t bits{};
            for(unsigned byte{}; byte != value.width; ++byte)
            { bits |= ::std::uint64_t{value.bytes[byte]} << (byte*8u); }
            if(value.width == 8u)
            { check(bits == 0u,"only the actual Wasm i64 memory64 offset0 can qualify an eight-byte integer carrier"); }
            else
            {
                ::std::uint32_t const input{argument}, numeric{input ^ 41u};
                ::std::uint32_t const expected[]{0u,1u,2u,5u,9u,13u,41u,input,numeric,
                    numeric+5u,numeric+18u,numeric+31u,input+2u,expected_result};
                bool actual{};
                for(auto const wanted:expected) { if(bits == wanted) { actual = true; break; } }
                check(actual,"four-byte FP carrier contains an actual Wasm i32 fixture value");
            }
            for(::std::size_t byte{value.width}; byte != value.bytes.size(); ++byte)
            { check(value.bytes[byte] == 0u,"integer carrier never exposes residual upper FP bytes"); }
            ++integer_fp_carriers; ++fp_index;
        }
        // A current Wasm owner can contain VM bridge instructions. Their public
        // row is deliberately empty, even at a genuine physical stop. Never
        // demand hidden bytes or use this display DTO as execution authority.
        if(!image.disassembly[0u])
        {
            auto const& row{image.disassembly[0u]};
            check(row.size == 0u && row.text[0u] == '\0' && !image.disassembly_destinations[0u],
                  "hidden bridge has no public bytes, text or target");
            for(auto byte: row.bytes) { check(byte == 0u, "hidden bridge byte storage cleared"); }
            ++hidden;
        }
        auto next{command(*owner, ::fast_io::concat_fast_io("step asm ", participant))};
        if(next.status == control::error::none)
        {
            check(!next.timed_out && next.reason == dbg::stop_reason::native_step && next.stop_identifier > before.stop_identifier &&
                  next.native_step_from == identity.pc && next.threads.size() == 1u && next.threads[0u].native_pc &&
                  *next.threads[0u].native_pc == next.native_step_to &&
                  next.native_step_to >= image.disassembly_owner_begin && next.native_step_to < image.disassembly_owner_end,
                  "real SI remains inside the authenticated Wasm owner");
            ++ordinary; current = next; continue;
        }
        auto retained{command(*owner, ::fast_io::concat_fast_io("info registers"))};
        check(next.status == control::error::unsupported_command && !next.timed_out &&
              next.stop_identifier == before.stop_identifier && next.execution == dbg::execution_status::stopped &&
              retained.status == control::error::none && retained.registers_stop_identifier == before.stop_identifier &&
              same_registers(registers.registers, retained.registers),
              "unproved SI retains the actual stop before a separate NI request");
        ++retained_si;
        next = command(*owner, attempt % 2u == 0u ? ::fast_io::concat_fast_io("ni") : ::fast_io::concat_fast_io("nexti ", participant));
        ::fast_io::io::perrln("NATIVE-PUBLIC-NI status=", static_cast<unsigned>(next.status),
            " reason=", static_cast<unsigned>(next.native_next_reason), " stop=", next.stop_identifier);
        if(next.status == control::error::none && next.reason == dbg::stop_reason::native_step)
        {
            check(!next.timed_out && next.stop_identifier > before.stop_identifier && next.native_step_from == identity.pc &&
                  next.threads.size() == 1u && next.threads[0u].native_pc && *next.threads[0u].native_pc == next.native_step_to &&
                  next.native_step_to > identity.pc && next.native_step_to < image.disassembly_owner_end &&
                  next.source_frames.empty() && next.memory.empty() && next.locals.empty() &&
                  !next.threads[0u].trace && next.threads[0u].source_inline_frames.empty(),
                  "SI-refused real call completes only in the original Wasm caller without callee stack or memory authority");
            auto const after{command(*owner, ::fast_io::concat_fast_io("info registers"))};
            auto const returned{command(*owner, ::fast_io::concat_fast_io("disassemble-range ", participant, " ", next.stop_identifier, " 1 0 0 1"))};
            check(after.status == control::error::none && after.registers_stop_identifier == next.stop_identifier &&
                  after.registers.pc() == next.native_step_to && returned.status == control::error::none &&
                  returned.disassembly_stop_identifier == next.stop_identifier && returned.disassembly_code.native_instruction_stop &&
                  returned.disassembly_code.pc == next.native_step_to && returned.disassembly_owner_begin == image.disassembly_owner_begin &&
                  returned.disassembly_owner_end == image.disassembly_owner_end && returned.disassembly_code.module == identity.module &&
                  returned.disassembly_code.function == identity.function &&
                  returned.disassembly_code.function_generation == identity.function_generation &&
                  returned.disassembly_code.runtime_epoch == identity.runtime_epoch,
                  "fresh kernel stop and original caller generation/epoch jointly reauthenticated");
            ++calls; ++call_current_proved;
        }
        else if(next.status == control::error::none && next.reason == dbg::stop_reason::requested)
        {
            check(throwing && !next.timed_out && next.execution == dbg::execution_status::stopped &&
                  next.stop_identifier > before.stop_identifier && next.threads.size() == 1u && !next.threads[0u].native_pc &&
                  next.threads[0u].location.code_unit == 0u && next.threads[0u].location.function == 1u &&
                  next.native_step_from == 0u && next.native_step_to == 0u && !next.native_instruction,
                  "NI abandonment gives an authentic caller Wasm pause; its cause is not inferred from the fixture");
            ++call_abandoned;
        }
        else
        {
            retained = command(*owner, ::fast_io::concat_fast_io("info registers"));
            check(next.status == control::error::unsupported_command && !next.timed_out &&
                  next.stop_identifier == before.stop_identifier && next.execution == dbg::execution_status::stopped &&
                  retained.status == control::error::none && retained.registers_stop_identifier == before.stop_identifier &&
                  same_registers(registers.registers, retained.registers),
                  "unavailable target/event/return keeps the authentic trap; denial is not positive NI coverage");
            ++unavailable_ni;
            current = seek_native(genuine_wasm_step(next)); continue;
        }
        auto stale{dbg::parse_console_command("ni")};
        stale.disassembly_stop_identifier = before.stop_identifier;
        auto const rejected{owner->execute(stale)};
        auto const still{owner->inspect()};
        check(rejected.status == control::error::unsupported_command &&
              rejected.native_next_reason == dbg::native_next_policy::reason::current_native_trap_required &&
              rejected.stop_identifier == next.stop_identifier && still.stop_identifier == next.stop_identifier &&
              still.reason == next.reason && still.threads.size() == 1u && still.threads[0u].location == next.threads[0u].location &&
              still.threads[0u].native_pc == next.threads[0u].native_pc,
              "old physical stop cannot resume a returned or abandoned continuation");
        if(next.reason == dbg::stop_reason::native_step) { ++call_stale_proved; }
        else { ++abandon_stale_proved; }
        current = next;
        if(calls >= (throwing ? 1u : 2u) && ordinary != 0u && hidden != 0u &&
           (!throwing || call_abandoned != 0u)) { break; }
        if(!current.threads[0u].native_pc) { current = seek_native(current); }
    }
    check(calls >= (throwing ? 1u : 2u) && ordinary != 0u && hidden != 0u && retained_si != 0u &&
          call_current_proved == calls && call_stale_proved == calls &&
          (!throwing || (call_abandoned != 0u && abandon_stale_proved == call_abandoned)),
          "required actual NI, hidden-bridge, stale-stop and abandonment coverage is missing; denial is not PASS");
#if defined(__i386__)
    check(!throwing || integer_fp_carriers != 0u,"exception walk exercises a genuine compiler-qualified i686 integer FP carrier");
#endif
    // Actual reset closes native admission and waits for real observer retirement
    // before the execution lease drains. Every retained callcap and perf event
    // belongs to that exact native ACK; a successful label cannot replace it.
    lib::reset_runtime_state_host_api();
    check(domain->is_closed() && owner->detach_resume(), "real native event/session ACK precedes actual runtime drain");
    auto const end{clock_type::now() + ::std::chrono::seconds{10}};
    while(!finished.load(::std::memory_order_acquire))
    { check(clock_type::now() < end, "actual native event/cursor/cap borrower did not retire"); ::std::this_thread::yield(); }
    guest.join();
    check(result == expected_result, "actual GC/memory64/return_call/try_table execution preserved after native NI/cancel/reset");
    owner.reset(); check(lifetime.expired(), "actual observer/controller released after guest ACK and physical join");
    ::fast_io::io::println("debug_native_closed_call_continuation_runtime: PASS policy=", policy, " scenario=", scenario,
        " returning-NI=", ::fast_io::mnp::dec(calls),
        " call-current-proved=", ::fast_io::mnp::dec(call_current_proved), " call-stale-proved=", ::fast_io::mnp::dec(call_stale_proved),
        " ordinary=", ::fast_io::mnp::dec(ordinary), " hidden-bridge=", ::fast_io::mnp::dec(hidden),
        " SI-retained=", ::fast_io::mnp::dec(retained_si), " NI-unavailable=", ::fast_io::mnp::dec(unavailable_ni),
        " integer-FP-carriers=", ::fast_io::mnp::dec(integer_fp_carriers),
        " actual-NI-Wasm-abandon=", ::fast_io::mnp::dec(call_abandoned),
        " abandon-stale-proved=", ::fast_io::mnp::dec(abandon_stale_proved), " result=", ::fast_io::mnp::dec(result),
        " Core3-GC-memory64-return_call-try_table=yes call-operand-class-qualified=no exact-EH-cause-qualified=no finish/all-platform-qualified=no");
}
