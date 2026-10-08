// Actual full-runtime native-next witness; fresh production producer/runtime/CLI+host closure only.
// LLVM DATA component success is not proof of an executed native instruction.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/uwvm/debugger/controller.h>
#include <uwvm2/uwvm/debugger/console.h>
#include <uwvm2/utils/container/string_concat.h>
#include <algorithm>
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
        ::fast_io::io::perrln("debug_native_public_registers_runtime: ", ::fast_io::mnp::os_c_str(text));
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
    if(argc != 3 && argc != 4) { return 2; }
    bool const all_numeric{argc == 4};
    auto const coverage{all_numeric ? ::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[3])} : ::fast_io::cstring_view{}};
    bool const require_ni{coverage == "all-numeric-ni"};
    if(all_numeric && coverage != "all-numeric" && !require_ni) { return 2; }
    if(!dbg::native_step::platform_available())
    { ::fast_io::io::println("debug_native_public_registers_runtime: SKIP actual native backend unavailable"); return 77; }
    auto const policy{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}};
    if(policy != "instruction" && policy != "unwind") { return 2; }
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_exception_dispatch = mode::runtime_llvm_jit_exception_dispatch_t::native_unwind;
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ?
        mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    // This all-type witness needs an explicit debug code-generation plan;
    // optimized-away values are not evidence of physical register coverage.
    if(all_numeric)
    {
        mode::global_runtime_llvm_jit_full_policy = mode::runtime_llvm_jit_full_policy_t::debug;
        mode::runtime_llvm_jit_full_policy_existed = true;
    }
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
    ::fast_io::io::println("debug_native_public_registers_runtime: SKIP host process adapter unavailable");
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
    dbg::controller_reply current{};
    while(true)
    {
        current = owner->inspect();
        if(current.execution == dbg::execution_status::stopped && current.reason == dbg::stop_reason::breakpoint) { break; }
        check(clock_type::now() < deadline && !finished.load(::std::memory_order_acquire), "actual initial breakpoint");
        ::std::this_thread::yield();
    }
    ::std::size_t traps{}, numeric{}, visible{}, hidden{}, retained{}, transitions{}, numeric64{}, fp32{}, fp64{}, vector128{}, call_continuations{};
    auto const participant{current.threads[0u].identifier};
    for(::std::size_t attempt{}; attempt != 4096u && clock_type::now() < deadline; ++attempt)
    {
        check(current.threads.size() == 1u && current.execution == dbg::execution_status::stopped,
              "actual stopped participant retained");
        auto const parsed{dbg::parse_console_command(::fast_io::string_view{"info all-registers"})};
        auto const registers{owner->execute(parsed)};
        if(current.threads[0u].native_pc)
        {
            ++traps;
            check(!current.threads[0u].trace && !current.threads[0u].source_inline_available &&
                  current.threads[0u].source_inline_frames.empty() && current.source_frames.empty() &&
                  current.memory.empty() && current.locals.empty(),
                  "native stop exposes no cached caller stack, native stack bytes or borrowed memory");
            check(registers.threads.size() == 1u && !registers.threads[0u].trace &&
                  registers.memory.empty() && registers.locals.empty() && registers.source_frames.empty(),
                  "register reply carries no stack or memory read capability");
            check(registers.status == control::error::none && registers.registers_stop_identifier == current.stop_identifier &&
                  registers.registers.pc() == *current.threads[0u].native_pc, "authentic native register stop identity");
            auto const& view{registers.registers};
            check(view.known_bits[6u] == 0u && view.known_bits[7u] == 0u && view.known_bits[17u] == 0u &&
                  view.values[6u] == 0u && view.values[7u] == 0u && view.values[17u] == 0u,
                  "no VM frame/stack/flags bits in public copy");
            for(::std::size_t i{}; i != 16u; ++i)
            {
                if(view.known_bits[i] == 0u) { check(view.values[i] == 0u, "unqualified GPR storage cleared"); continue; }
                auto const value{view.values[i]};
                if(view.known_bits[i] == 32u)
                {
                    check((value >> 32u) == 0u, "i32/f32 never release residual upper GPR bits");
                    check(value == 17u || value == 41u || value == 56u || value == 1u || value == 112u ||
                          value == 0u || value == 13u || value == 125u ||
                          (all_numeric && (value == 2u || value == 3u || value == 4u || value == 0x41880000u ||
                                           value == 0x3fc00000u || value == 0x41cc0000u)),
                          "qualified low bits are actual Wasm numeric values");
                }
                else
                {
                    check(all_numeric && view.known_bits[i] == 64u && (value == 17u || value == 12345678901234567u ||
                          value == 12345678901234584u || value == 0x4031000000000000u || value == 0x4002000000000000u ||
                          value == 0x4043200000000000u), "i64/f64 bits come from this exact Wasm numeric fixture");
                    if(value == 17u || value == 12345678901234567u || value == 12345678901234584u) { ++numeric64; }
                }
                ++numeric;
            }
            if(all_numeric)
            {
                for(::std::size_t i{}; i != 16u; ++i)
                {
                    auto const& value{view.floating.values[i]};
                    if(value.width == 0u)
                    { for(auto byte:value.bytes) { check(byte == 0u, "unqualified XMM storage cleared"); } continue; }
                    ::std::uint64_t low{};
                    for(unsigned byte{}; byte != (::std::min)(unsigned(value.width),8u); ++byte)
                    { low |= ::std::uint64_t{value.bytes[byte]} << (byte*8u); }
                    for(unsigned byte{value.width}; byte != value.bytes.size(); ++byte)
                    { check(value.bytes[byte] == 0u, "unqualified upper XMM bits cleared"); }
                    if(value.width == 4u)
                    {
                        bool const fp{low == 0x41880000u || low == 0x3fc00000u || low == 0x41cc0000u};
                        check(fp || low == 17u || low == 41u || low == 56u || low == 112u || low == 125u || low == 0u ||
                              low == 1u || low == 2u || low == 3u || low == 4u || low == 13u, "actual qualified 32-bit Wasm bits in XMM");
                        if(fp) { ++fp32; }
                    }
                    else if(value.width == 8u)
                    {
                        bool const fp{low == 0x4031000000000000u || low == 0x4002000000000000u || low == 0x4043200000000000u};
                        check(fp || low == 17u || low == 12345678901234567u || low == 12345678901234584u,
                              "actual qualified 64-bit Wasm bits in XMM");
                        if(fp) { ++fp64; }
                    }
                    else
                    {
                        check(value.width == 16u, "only real v128 can qualify a complete XMM");
                        bool input{true}, constant{true}, output{true};
                        for(unsigned lane{}; lane != 4u; ++lane)
                        {
                            ::std::uint32_t bits{};
                            for(unsigned byte{}; byte != 4u; ++byte)
                            { bits |= ::std::uint32_t{value.bytes[lane*4u+byte]} << (byte*8u); }
                            input &= bits == 17u; constant &= bits == lane+1u; output &= bits == lane+18u;
                        }
                        check(input || constant || output, "complete v128 contains actual Wasm lanes"); ++vector128;
                    }
                }
            }
            for(::std::size_t i{16u}; i != dbg::native_registers::max_fp_registers; ++i)
            { check(view.floating.values[i].width == 0u, "x87/control state remains private"); }
            auto const rendered{dbg::details::format_reply(registers, parsed)};
            check(rendered.find("rsp=unavailable") != ::std::string::npos &&
                  rendered.find("rbp=unavailable") != ::std::string::npos &&
                  rendered.find("rflags=unavailable") != ::std::string::npos,
                  "console clearly renders forbidden registers unavailable");
            auto const stale{command(*owner, ::fast_io::concat_fast_io("info registers ", participant, " ", current.stop_identifier+1u, " all"))};
            check(stale.status != control::error::none, "forged stop identity rejected");
            auto const foreign{command(*owner, ::fast_io::concat_fast_io("info registers ", participant+1u, " ", current.stop_identifier, " all"))};
            check(foreign.status != control::error::none, "foreign participant rejected");
        }
        else { check(registers.status != control::error::none, "cooperative stop cannot expose physical registers"); }
        auto const text{::fast_io::concat_fast_io("disassemble ", participant, " ", current.stop_identifier, " 4")};
        auto const disassembly_command{dbg::parse_console_command(::fast_io::string_view{text.data(),text.size()})};
        auto const image{owner->execute(disassembly_command)};
        ::fast_io::io::perrln("PUBLIC-STOP stop=",current.stop_identifier," native=",bool(current.threads[0u].native_pc),
            " view-status=",static_cast<unsigned>(image.status)," count=",image.disassembly_count,
            " numeric=",numeric," i64=",numeric64," f32=",fp32," f64=",fp64," v128=",vector128,
            " wasm-offset=",current.threads[0u].location.offset);
        check(current.threads[0u].native_pc ? image.status == control::error::none && image.disassembly_count == 4u :
              (image.status == control::error::none && image.disassembly_count == 4u) ||
              (image.status == control::error::unsupported_command && image.disassembly_count == 0u),
              "native view requires genuine current owner; cooperative view may remain unavailable");
        for(::std::size_t row{}; row != image.disassembly_count; ++row)
        {
            if(image.disassembly[row]) { ++visible; }
            else
            {
                ++hidden;
                check(image.disassembly[row].size == 0u && image.disassembly[row].text[0u] == '\0' &&
                      !image.disassembly_destinations[row], "hidden scaffolding has no bytes/text/branch destination");
                for(auto const byte: image.disassembly[row].bytes) { check(byte == 0u, "hidden row byte storage cleared"); }
            }
        }
        if(visible == 0u && current.threads[0u].native_pc && image.status == control::error::none &&
           image.disassembly_owner_end > image.disassembly_owner_begin)
        {
            // Query exact instruction offsets in the SAME authentic paused image.
            // A hidden row in one 32-row page cannot stand in for the whole owner.
            auto const distance{image.disassembly_code.pc - image.disassembly_owner_begin};
            check(distance <= INT64_MAX, "bounded owner scan distance");
            auto const extent{image.disassembly_owner_end - image.disassembly_owner_begin};
            auto const count{::std::min<::std::size_t>(extent, static_cast<::std::size_t>(dbg::native_disassembly::max_window_instruction_offset))};
            for(::std::size_t index{}; index != count; ++index)
            {
                auto query{command(*owner, ::fast_io::concat_fast_io("disassemble-range ", participant, " ", current.stop_identifier,
                    " 1 ", -static_cast<::std::int64_t>(distance), " ", index, " 0"))};
                check(query.status == control::error::none && query.disassembly_count == 1u &&
                      query.disassembly_stop_identifier == current.stop_identifier &&
                      query.disassembly_owner_begin == image.disassembly_owner_begin &&
                      query.disassembly_owner_end == image.disassembly_owner_end,
                      "whole owner scan remains the real same stop and image");
                if(query.disassembly[0u]) { ++visible; break; }
            }
            ::fast_io::io::perrln("PUBLIC-OWNER-SCAN visible=",visible," bounded-instruction-offsets=",count);
        }
        auto const display{dbg::details::format_reply(image, disassembly_command)};
        check(image.status != control::error::none || display.find("error:") == ::std::string::npos, "plain hidden filler keeps authenticated boundary label");
        auto const before{current.stop_identifier};
        auto next{command(*owner, ::fast_io::concat_fast_io("step asm ", participant))};
        ::fast_io::io::perrln("PUBLIC-STEP status=",static_cast<unsigned>(next.status)," reason=",static_cast<unsigned>(next.native_next_reason)," stop=",next.stop_identifier);
        if(next.status != control::error::none && current.threads[0u].native_pc)
        {
            check(next.stop_identifier == before && next.execution == dbg::execution_status::stopped,
                  "refused SI keeps the original physical stop before a separate NI request");
            auto continued{command(*owner, ::fast_io::concat_fast_io("ni ", participant))};
            ::fast_io::io::perrln("PUBLIC-NEXT status=", static_cast<unsigned>(continued.status),
                " reason=", static_cast<unsigned>(continued.native_next_reason), " stop=", continued.stop_identifier);
            if(continued.status == control::error::none)
            {
                check(continued.stop_identifier > before && continued.threads.size() == 1u &&
                      continued.threads[0u].native_pc &&
                      *continued.threads[0u].native_pc >= image.disassembly_owner_begin &&
                      *continued.threads[0u].native_pc < image.disassembly_owner_end,
                      "NI returns to the same authentic Wasm caller; no host/VM native stop");
                check(continued.source_frames.empty() && continued.memory.empty() && continued.locals.empty() &&
                      !continued.threads[0u].trace && continued.threads[0u].source_inline_frames.empty(),
                      "opaque call continuation gives no callee/VM stack or memory capability");
                auto const returned{command(*owner, ::fast_io::concat_fast_io("disassemble-range ", participant,
                    " ", continued.stop_identifier, " 1 0 0 1"))};
                check(returned.status == control::error::none && returned.disassembly_code.native_instruction_stop &&
                      returned.disassembly_code.pc == *continued.threads[0u].native_pc &&
                      returned.disassembly_owner_begin == image.disassembly_owner_begin &&
                      returned.disassembly_owner_end == image.disassembly_owner_end &&
                      returned.disassembly_code.module == image.disassembly_code.module &&
                      returned.disassembly_code.function == image.disassembly_code.function &&
                      returned.disassembly_code.function_generation == image.disassembly_code.function_generation &&
                      returned.disassembly_code.runtime_epoch == image.disassembly_code.runtime_epoch,
                      "NI reauthenticates the same actual Wasm caller and publication generation");
                auto stale{dbg::parse_console_command("ni")};
                stale.disassembly_stop_identifier = before;
                auto const rejected{owner->execute(stale)};
                check(rejected.status == control::error::unsupported_command &&
                      rejected.native_next_reason == dbg::native_next_policy::reason::current_native_trap_required &&
                      rejected.execution == dbg::execution_status::stopped &&
                      rejected.stop_identifier == continued.stop_identifier &&
                      owner->inspect().stop_identifier == continued.stop_identifier,
                      "the previous physical stop cannot resume a returned NI continuation");
                ++call_continuations; next = ::std::move(continued);
            }
            else
            {
                check(continued.stop_identifier == before && continued.execution == dbg::execution_status::stopped,
                      "unavailable NI leaves the same original physical stop and execution state");
            }
        }
        if(next.status == control::error::none)
        {
            check(next.stop_identifier > before && next.threads[0u].native_pc,
                  "actual new native stop has an authentic physical PC");
            if(next.native_instruction)
            {
                check(image.disassembly_count != 0u && image.disassembly[0u] &&
                      next.native_instruction.pc == image.disassembly[0u].pc &&
                      next.native_instruction.size == image.disassembly[0u].size &&
                      next.native_instruction.bytes == image.disassembly[0u].bytes,
                      "executed instruction bytes match the prior compiler-qualified Wasm row");
            }
            ++transitions; current = next;
        }
        else
        {
            check(next.stop_identifier == before && next.execution == dbg::execution_status::stopped,
                  "unproved VM/control transfer refuses before unpark");
            ++retained;
            if(traps != 0u && numeric != 0u && visible != 0u && hidden != 0u &&
               (!all_numeric || (numeric64 != 0u && fp32 != 0u && fp64 != 0u && vector128 != 0u)) &&
               (!require_ni || call_continuations != 0u)) { current = next; break; }
            current = command(*owner, ::fast_io::concat_fast_io("step wasm ", participant));
            if(current.execution == dbg::execution_status::exited && finished.load(::std::memory_order_acquire)) { break; }
            check(current.status == control::error::none && current.stop_identifier > before,
                  "real Wasm checkpoint advances after retained native refusal");
        }
        auto const old{command(*owner, ::fast_io::concat_fast_io("info registers ",participant," ",before," all"))};
        check(old.status != control::error::none, "retired stop IDs cannot read old registers");
    }
    ::fast_io::io::println("native-public coverage traps=",traps," numeric=",numeric," visible=",visible,
                          " hidden=",hidden," retained=",retained," transitions=",transitions,
                          " i64=",numeric64," f32=",fp32," f64=",fp64," v128=",vector128," ni-continuations=",call_continuations);
    check(traps != 0u && numeric != 0u && visible != 0u && hidden != 0u && retained != 0u && transitions != 0u,
          "real positive numeric/code behavior and retained refusal are required");
    check(!all_numeric || (numeric64 != 0u && fp32 != 0u && fp64 != 0u && vector128 != 0u),
          "actual physical i64/f32/f64/v128 positive values are required");
    check(!require_ni || call_continuations != 0u,
          "explicit NI coverage requires a real caller continuation; DATA-only or fallback is not PASS");
    lib::reset_runtime_state_host_api();
    check(domain->is_closed() && owner->detach_resume(), "private hardware context retires during actual reset");
    auto const end{clock_type::now() + ::std::chrono::seconds{10}};
    while(!finished.load(::std::memory_order_acquire))
    { check(clock_type::now() < end,"actual guest joins after retirement"); ::std::this_thread::yield(); }
    guest.join(); check(result == 125u,"guest result preserved by public projection");
    check(command(*owner, ::fast_io::concat_fast_io("info all-registers")).status != control::error::none,
          "reset revokes native display authority");
    owner.reset(); check(lifetime.expired(),"observer retires after reset");
    ::fast_io::io::println("PASS actual Wasm JIT numeric registers, masked upper bits, hidden runtime code, forged/retired identity rejection policy=",policy);
}
