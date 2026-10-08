// Actual cross-architecture full-runtime native-next witness.
// Requires freshly rebuilt production runtime and qualified target LLVM owner producer.
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
#if defined(__linux__) && (defined(__powerpc__) || defined(__loongarch__))
# include <sys/auxv.h>
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
        ::fast_io::io::perrln("debug_native_linux_runtime: ", ::fast_io::mnp::os_c_str(text));
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
    if(all_numeric && ::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[3])} != "all-numeric") { return 2; }
    if(!dbg::native_step::platform_available())
    { ::fast_io::io::println("debug_native_linux_runtime: SKIP actual native backend unavailable"); return 77; }
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
    ::fast_io::io::println("debug_native_linux_runtime: SKIP host process adapter unavailable");
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
    // Real register liveness witnesses add authenticated native instructions.
    // Keep the positive SIMD checks: allow enough actual steps to reach them.
    auto const deadline{clock_type::now() + ::std::chrono::seconds{180}};
    dbg::controller_reply current{};
    while(true)
    {
        current = owner->inspect();
        if(current.execution == dbg::execution_status::stopped && current.reason == dbg::stop_reason::breakpoint) { break; }
        check(clock_type::now() < deadline && !finished.load(::std::memory_order_acquire), "actual initial breakpoint");
        ::std::this_thread::yield();
    }
    ::std::size_t traps{}, numeric{}, visible{}, hidden{}, retained{}, transitions{}, numeric64{}, fp32{}, fp64{}, vector128{};
    bool large_i64_constant{}, large_i64_result{};
    ::std::size_t real_si{}, real_ni{};
    // Demand every directly representable class in the selected target ABI.
    // RV64GC has no physical v128 class. i686 i64 uses a complete SSE2
    // bit carrier; split/bit-piece and stack locations remain unqualified.
#if defined(__i386__) || defined(__arm__) || (defined(__powerpc__) && __SIZEOF_POINTER__ == 4)
    constexpr bool require_i64{true};
#else
    constexpr bool require_i64{sizeof(::std::uintptr_t) == 8u};
#endif
#if defined(__aarch64__) || defined(__i386__) || defined(__x86_64__)
    constexpr bool require_v128{true};
#elif defined(__linux__) && defined(__powerpc__)
    bool const require_v128{(::getauxval(AT_HWCAP) & 0x10000000ul) != 0u};
#elif defined(__linux__) && defined(__loongarch__)
    bool const require_v128{(::getauxval(AT_HWCAP) & (1ul << 4u)) != 0u};
#else
    constexpr bool require_v128{false};
#endif
    auto const numeric_classes_seen{[&]() noexcept
    { return (!require_i64 || (numeric64 != 0u && large_i64_constant && large_i64_result)) &&
        fp32 != 0u && fp64 != 0u && (!require_v128 || vector128 != 0u); }};
    auto const participant{current.threads[0u].identifier};
    for(::std::size_t attempt{}; attempt != 4096u && clock_type::now() < deadline; ++attempt)
    {
        check(current.threads.size() == 1u && current.execution == dbg::execution_status::stopped,
              "actual stopped participant retained");
        auto const parsed{dbg::parse_console_command("info all-registers")};
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
            auto const sp{dbg::native_registers::sp_index(view.machine)};
            auto const fp{dbg::native_registers::fp_index(view.machine)};
            for(auto index : {sp,fp})
            { if(index < view.size()) { check(view.known_bits[index] == 0u && view.values[index] == 0u,"no host stack/frame pointer bits"); } }
            for(::std::size_t i{}; i != view.size(); ++i)
            {
                if(i == dbg::native_registers::pc_index(view.machine)) { continue; }
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
                    bool const expected{all_numeric && view.known_bits[i] == 64u && (value == 17u || value == 12345678901234567u ||
                          value == 12345678901234584u || value == 0x4031000000000000u || value == 0x4002000000000000u ||
                          value == 0x4043200000000000u)};
                    if(!expected)
                    {
                        ::fast_io::io::perrln("unexpected qualified GPR index=", i, " bits=", unsigned(view.known_bits[i]),
                            " value=0x", ::fast_io::mnp::hex<false,true>(value), " stop=", current.stop_identifier);
                    }
                    check(expected, "i64/f64 bits come from this exact Wasm numeric fixture");
                    if(value == 17u || value == 12345678901234567u || value == 12345678901234584u) { ++numeric64; }
                    large_i64_constant |= value == 12345678901234567u;
                    large_i64_result |= value == 12345678901234584u;
                }
                ++numeric;
            }
            if(all_numeric)
            {
                for(::std::size_t i{}; i != view.floating.values.size(); ++i)
                {
                    auto const& value{view.floating.values[i]};
                    if(value.width == 0u)
                    { for(auto byte:value.bytes) { check(byte == 0u, "unqualified FP/vector storage cleared"); } continue; }
                    ::std::uint64_t low{};
                    for(unsigned byte{}; byte != (::std::min)(unsigned(value.width),8u); ++byte)
                    { low |= ::std::uint64_t{value.bytes[byte]} << (byte*8u); }
                    for(unsigned byte{value.width}; byte != value.bytes.size(); ++byte)
                    { check(value.bytes[byte] == 0u, "unqualified upper XMM bits cleared"); }
                    if(value.width == 4u)
                    {
                        bool const fp{low == 0x41880000u || low == 0x3fc00000u || low == 0x41cc0000u};
                        check(fp || low == 17u || low == 41u || low == 56u || low == 112u || low == 125u || low == 0u ||
                              low == 1u || low == 2u || low == 3u || low == 4u || low == 13u, "actual qualified 32-bit Wasm bits in FP/vector registers");
                        if(fp) { ++fp32; }
                    }
                    else if(value.width == 8u)
                    {
                        bool const fp{low == 0x4031000000000000u || low == 0x4002000000000000u || low == 0x4043200000000000u};
                        check(fp || low == 17u || low == 12345678901234567u || low == 12345678901234584u,
                              "actual qualified 64-bit Wasm bits in FP/vector registers");
                        if(fp) { ++fp64; }
                        else { ++numeric64; } // A complete i64 may use an XMM bit carrier.
                        large_i64_constant |= low == 12345678901234567u;
                        large_i64_result |= low == 12345678901234584u;
                    }
                    else
                    {
                        check(value.width == 16u, "only real v128 can qualify a complete FP/vector register");
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
            if(!all_numeric) for(auto const& value: view.floating.values)
            {
                check(value.width == 0u,"integer-only fixture cannot qualify host floating state");
                for(auto byte:value.bytes) { check(byte == 0u,"unqualified floating bytes cleared"); }
            }
            auto const rendered{dbg::details::format_reply(registers, parsed)};
            auto const hidden_sp{::fast_io::concat_fast_io(
                ::fast_io::mnp::os_c_str(reinterpret_cast<char const*>(dbg::native_registers::name(view.machine,sp))),"=unavailable")};
            check(rendered.find(hidden_sp.c_str()) != ::std::string::npos,"console renders host stack register unavailable");
#if defined(__i386__)
            check(rendered.find("architecture=i686 word-bits=32") != ::std::string::npos,"console identifies actual i686 ABI");
#elif defined(__riscv)
            check(rendered.find("architecture=riscv64 word-bits=64") != ::std::string::npos,"console identifies actual RV64 ABI");
#elif defined(__aarch64__)
            check(rendered.find("architecture=aarch64 word-bits=64") != ::std::string::npos,"console identifies actual AArch64 ABI");
#endif
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
            " wasm-function=",current.threads[0u].location.function,
            " native-offset=",current.threads[0u].native_pc && current.threads[0u].native_wasm_position ?
                *current.threads[0u].native_pc-current.threads[0u].native_wasm_position->owner_begin:SIZE_MAX,
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
        auto const display{dbg::details::format_reply(image, disassembly_command)};
        check(image.status != control::error::none || display.find("error:") == ::std::string::npos, "plain hidden filler keeps authenticated boundary label");
        auto const before{current.stop_identifier};
        // Alternate actual SI and NI at physical Wasm-owned stops. A
        // cooperative checkpoint first needs SI to acquire a real trap.
        bool const use_ni{current.threads[0u].native_pc && attempt % 2u != 0u};
        auto next{command(*owner, (use_ni ? ::fast_io::concat_fast_io("ni ",participant) : ::fast_io::concat_fast_io("step asm ",participant)))};
        ::fast_io::io::perrln("PUBLIC-STEP status=",static_cast<unsigned>(next.status)," reason=",static_cast<unsigned>(next.native_next_reason)," stop=",next.stop_identifier);
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
            ++transitions; if(use_ni) { ++real_ni; } else { ++real_si; } current = next;
        }
        else
        {
            check(next.stop_identifier == before && next.execution == dbg::execution_status::stopped,
                  "unproved VM/control transfer refuses before unpark");
            ++retained;
            if(traps != 0u && numeric != 0u && visible != 0u && hidden != 0u &&
               (!all_numeric || numeric_classes_seen())) { current = next; break; }
            current = command(*owner, ::fast_io::concat_fast_io("step wasm ", participant));
            if(current.execution == dbg::execution_status::exited) { break; } // Guest completion/result are checked after joining below.
            check(current.status == control::error::none && current.stop_identifier > before,
                  "real Wasm checkpoint advances after retained native refusal");
        }
        auto const old{command(*owner, ::fast_io::concat_fast_io("info registers ",participant," ",before," all"))};
        check(old.status != control::error::none, "retired stop IDs cannot read old registers");
    }
    ::fast_io::io::perrln("native-public coverage traps=",traps," numeric=",numeric," visible=",visible,
                          " hidden=",hidden," retained=",retained," transitions=",transitions,
                          " i64=",numeric64," f32=",fp32," f64=",fp64," v128=",vector128,
                          " large-i64-constant=",large_i64_constant," large-i64-result=",large_i64_result,
                          " real-si=",real_si," real-ni=",real_ni);
    check(traps != 0u && numeric != 0u && visible != 0u && hidden != 0u && retained != 0u && transitions != 0u,
          "real target Wasm JIT native stops, numeric/code values, and retained boundary refusal are required");
    check(real_si != 0u && real_ni != 0u,"both SI and NI execute genuine target instructions");
    check(!all_numeric || numeric_classes_seen(),
          "every directly representable physical numeric class requires positive actual Wasm values");
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
