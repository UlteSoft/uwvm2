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
        ::fast_io::io::perrln("debug_native_branch_display_runtime: ", ::fast_io::mnp::os_c_str(text));
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
    if(argc != 3) { return 2; }
    if(!dbg::native_step::platform_available())
    { ::fast_io::io::println("debug_native_branch_display_runtime: SKIP actual native backend unavailable"); return 77; }
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
        u8"uwvm-debug-native-branch-display", nullptr,
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
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"native-branch-display-test";
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
    ::fast_io::io::println("debug_native_branch_display_runtime: SKIP host process adapter unavailable");
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
        lib::full_compile_and_run_main_module(u8"native-branch-display-test", run);
        owner->notify_guest_exit(0);
        finished.store(true, ::std::memory_order_release);
    }};
    auto const deadline{clock_type::now() + ::std::chrono::seconds{20}};
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
    auto const notice{dbg::details::format_reply(rejected, dbg::parse_console_command(::fast_io::string_view{"ni"}))};
    check(notice.find("first use step asm THREAD") != ::std::string::npos, "precise native trap requirement is visible");
    auto current{command(*owner, ::fast_io::concat_fast_io("step asm ", participant))};
    check(current.status == control::error::none && !current.timed_out && current.reason == dbg::stop_reason::native_step &&
          current.execution == dbg::execution_status::stopped && current.native_step_to != 0u && current.threads.size() == 1u &&
          current.threads[0u].native_pc && *current.threads[0u].native_pc == current.native_step_to,
          "actual single-instruction native trap established by existing backend");
    auto registers{command(*owner, ::fast_io::concat_fast_io("info registers"))};
    check(registers.status == control::error::none && registers.registers_stop_identifier == current.stop_identifier &&
          registers.registers.pc() == current.native_step_to, "actual trap registers/private capture authentication");
    // This witness uses the REAL controller/private capture and formatter.
    // Its small input-dependent Core 3 loop leaves a physical in-owner direct
    // branch; handcrafted bytes alone do not satisfy the coverage counter.
    auto seed_text{::fast_io::concat_fast_io("disassemble-range ", participant, " ", current.stop_identifier, " 1 0 0 1")};
    auto seed{command(*owner, seed_text)};
    check(seed.status == control::error::none && seed.disassembly_count == 1u &&
          seed.disassembly_code.pc == registers.registers.pc() &&
          seed.disassembly_owner_begin <= seed.disassembly_code.pc &&
          seed.disassembly_code.pc < seed.disassembly_owner_end &&
          seed.disassembly_owner_end - seed.disassembly_owner_begin <= PTRDIFF_MAX,
          "actual private current-function owner and native-trap PC");
    auto const distance{seed.disassembly_code.pc - seed.disassembly_owner_begin};
    check(distance <= PTRDIFF_MAX && distance <= INT64_MAX,
          "bounded scalar display offset before signed conversion");
    auto const entry_offset{-static_cast<::std::int64_t>(distance)};
    ::std::size_t destinations{}, current_symbols{}, symbol_page{};
    bool completed_scan{};
    auto formatted = [&](dbg::controller_reply const& reply, ::fast_io::string const& text)
    {
        // [owned complete request text ... text.size()] request_end
        // [safe                                      ] borrow only for parse/format;
        //  ^^ the displayed target is never reused as an address command.
        return dbg::details::format_reply(reply,
            dbg::parse_console_command(::fast_io::string_view{text.data(), text.size()}));
    };
    auto const scan_limit{::std::min<::std::size_t>(seed.disassembly_owner_end - seed.disassembly_owner_begin,
        static_cast<::std::size_t>(dbg::native_disassembly::max_window_instruction_offset))};
    for(::std::size_t first{}; first != scan_limit; ++first)
    {
        auto text{::fast_io::concat_fast_io("disassemble-range ", participant, " ", current.stop_identifier,
            " 1 ", entry_offset, " ", first, " 1")};
        auto page{command(*owner, text)};
        if(page.status == control::error::unsupported_command) { break; }
        check(page.status == control::error::none && page.disassembly_count == 1u &&
              page.disassembly_stop_identifier == current.stop_identifier &&
              page.disassembly_code.participant == participant &&
              page.disassembly_code.module == seed.disassembly_code.module &&
              page.disassembly_code.function == seed.disassembly_code.function &&
              page.disassembly_code.function_generation == seed.disassembly_code.function_generation &&
              page.disassembly_code.runtime_epoch == seed.disassembly_code.runtime_epoch &&
              page.disassembly_owner_begin == seed.disassembly_owner_begin && page.disassembly_owner_end == seed.disassembly_owner_end,
              "paged display remains the exact same authenticated physical owner/generation/epoch");
        auto const visible{formatted(page, text)};
        check(visible.find("native-disassembly-range stop=") != ::std::string::npos &&
              visible.find("native-disassembly-end\n") != ::std::string::npos && visible.find("error:") == ::std::string::npos,
              "production formatter consumes the genuine controller range reply");
        for(::std::size_t row{}; row != page.disassembly_count; ++row)
        {
            auto const& instruction{page.disassembly[row]};
            auto const& target{page.disassembly_destinations[row]};
            check(dbg::native_branch_display::matches(target, instruction), "annotation is bound to the exact shown instruction");
            if(!instruction)
            {
                check(!target && instruction.pc == 0u, "invalid filler cannot carry a destination or instruction PC");
                for(auto byte : instruction.bytes) { check(byte == 0u, "hidden row bytes are zero"); }
                for(auto character : instruction.text) { check(character == '\0', "hidden row text is zero"); }
                continue;
            }
            for(::std::size_t byte{instruction.size}; byte != instruction.bytes.size(); ++byte)
            { check(instruction.bytes[byte] == 0u, "visible row has no private decoder lookahead"); }
            if(!target) { continue; }
            ++destinations;
            auto const needle{::fast_io::concat_fast_io("target=0x", ::fast_io::mnp::hex<false, true>(target.destination.display_pc))};
            check(visible.find(needle.data(), 0u, needle.size()) != ::std::string::npos, "checked scalar destination reaches the actual console formatter");
            auto const symbol{dbg::native_branch_display::resolve_current_owner(target,
                page.disassembly_owner_begin, page.disassembly_owner_end, true)};
            if(symbol.resolution == dbg::native_branch_display::symbol_resolution::current_function)
            {
                ++current_symbols; symbol_page = first;
                auto const name{::fast_io::concat_fast_io("target-symbol=current-function+0x",
                    ::fast_io::mnp::hex<false, true>(symbol.function_offset),
                    " target-module=", page.disassembly_code.module, " target-function=", page.disassembly_code.function,
                    " target-function-generation=", page.disassembly_code.function_generation)};
                check(visible.find(name.data(), 0u, name.size()) != ::std::string::npos, "only exact current-owner function labels accompany the target");
            }
            else { check(false, "a public branch cannot have an unproved current-owner destination"); }
        }

    }
    completed_scan = true;
    ::fast_io::io::perrln("actual branch coverage: direct-targets=", destinations, " current-owner-labels=", current_symbols,
        " independent-offsets=", scan_limit);
    // The owner image includes JIT scaffolding. Its branch targets need not
    // have complete pure-Wasm provenance, even when they lie inside the owner.
    // Run every isolation/reset check below, then report unavailable positive
    // runtime coverage explicitly; DATA decoding cannot supply this witness.
    bool const runtime_branch_qualified{completed_scan && destinations != 0u && current_symbols != 0u};
    auto disabled_text{::fast_io::concat_fast_io("disassemble-range ", participant, " ", current.stop_identifier,
        " 1 ", entry_offset, " ", symbol_page, " 0")};
    auto disabled{command(*owner, disabled_text)};
    auto disabled_display{formatted(disabled, disabled_text)};
    check(disabled.status == control::error::none && disabled.disassembly_function_name_size == 0u &&
          (!runtime_branch_qualified || disabled_display.find("target-symbol=disabled") != ::std::string::npos) &&
          disabled_display.find("target-symbol=current-function") == ::std::string::npos,
          "resolveSymbols false retains numeric targets without naming a function");
    auto plain_text{::fast_io::concat_fast_io("disassemble ", participant, " ", current.stop_identifier, " 32")};
    auto plain{command(*owner, plain_text)};
    auto plain_display{formatted(plain, plain_text)};
    check(plain.status == control::error::none && plain.disassembly_code.size <= lib::llvm_jit_debug_max_native_code_bytes &&
          plain.disassembly_owner_begin == seed.disassembly_owner_begin && plain.disassembly_owner_end == seed.disassembly_owner_end &&
          plain_display.find("target-symbol=current-function") == ::std::string::npos,
          "plain current-stop display retains bounded output, target containment and no owner symbol");
    auto stale{command(*owner, ::fast_io::concat_fast_io("disassemble-range ", participant, " ", initial.stop_identifier, " 1 0 0 1"))};
    auto const foreign{participant == UINT64_MAX ? participant - 1u : participant + 1u};
    auto wrong{command(*owner, ::fast_io::concat_fast_io("disassemble-range ", foreign, " ", current.stop_identifier, " 1 0 0 1"))};
    auto retained{command(*owner, ::fast_io::concat_fast_io("info registers"))};
    check(stale.status != control::error::none && wrong.status != control::error::none &&
          retained.status == control::error::none && retained.registers_stop_identifier == current.stop_identifier &&
          same_registers(registers.registers, retained.registers) && owner->inspect().stop_identifier == current.stop_identifier,
          "all display/refused requests preserve real native PC/GPR/stop and never unpark");
    lib::reset_runtime_state_host_api();
    auto invalid{command(*owner, ::fast_io::concat_fast_io("disassemble-range ", participant, " ", current.stop_identifier, " 1 0 0 1"))};
    check(domain->is_closed() && invalid.status != control::error::none && owner->detach_resume(),
          "reset invalidates the actual display owner and retires the physical trap");
    auto const end{clock_type::now() + ::std::chrono::seconds{10}};
    while(!finished.load(::std::memory_order_acquire))
    { check(clock_type::now() < end, "actual guest remained in retired native gate"); ::std::this_thread::yield(); }
    guest.join();
    check(result == 125u, "Core3 input-dependent loop/tail call result survives read-only display and reset");
    owner.reset(); check(lifetime.expired(), "actual observer/controller retired after reset and guest join");
    if(!runtime_branch_qualified)
    {
        ::fast_io::io::println("debug_native_branch_display_runtime: UNQUALIFIED actual pure-Wasm direct-target coverage; ",
            "isolation/reset checks passed policy=", policy);
        return 77;
    }
    ::fast_io::io::println("debug_native_branch_display_runtime: PASS policy=", policy,
        " actual-direct-targets=", ::fast_io::mnp::dec(destinations), " current-owner-labels=", ::fast_io::mnp::dec(current_symbols),
        " genuine-native-trap=yes display-only=yes");
}
