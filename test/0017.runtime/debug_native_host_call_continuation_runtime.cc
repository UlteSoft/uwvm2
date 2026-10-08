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
        ::fast_io::io::perrln("debug_native_host_call_continuation_runtime: ", ::fast_io::mnp::os_c_str(text));
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
#if !defined(__linux__) || !defined(__x86_64__) || __SIZEOF_POINTER__ != 8 || \
    !defined(UWVM2_ENABLE_DEBUG_NATIVE_CONTINUATION_LINUX_PRODUCT) || UWVM2_ENABLE_DEBUG_NATIVE_CONTINUATION_LINUX_PRODUCT != 1
    ::fast_io::io::println("debug_native_host_call_continuation_runtime: UNQUALIFIED product/backend switch unavailable");
    return 77;
#else
    if(!dbg::native_continuation_linux::sdk_available)
    { ::fast_io::io::println("debug_native_host_call_continuation_runtime: UNQUALIFIED precise-event SDK unavailable"); return 77; }
#endif
    if(!dbg::native_step::platform_available())
    { ::fast_io::io::println("debug_native_host_call_continuation_runtime: SKIP actual native backend unavailable"); return 77; }
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
    ::fast_io::io::println("debug_native_host_call_continuation_runtime: SKIP host process adapter unavailable");
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
    ::std::size_t actual_wasm_steps{}, ordinary{}, call_next{}, prefixes{}, returns{};
    dbg::native_owned_instruction_semantics::decoder semantic{};
    check(bool(semantic), "actual conservative MC provider required");
    auto genuine_wasm_step{[&](dbg::controller_reply const& previous)
    {
        check(actual_wasm_steps != 128u && clock_type::now() < deadline && !finished.load(::std::memory_order_acquire),
              "finite genuine Wasm stop search exhausted");
        auto const next{command(*owner, ::fast_io::concat_std("step wasm ", participant))}; ++actual_wasm_steps;
        check(next.status == control::error::none && !next.timed_out && next.execution == dbg::execution_status::stopped &&
              next.stop_identifier > previous.stop_identifier && next.threads.size() == 1u &&
              next.threads[0u].identifier == participant && !next.threads[0u].native_pc,
              "real Wasm stepping only; no requested machine PC");
        return next;
    }};
    auto seek_native{[&](dbg::controller_reply current)
    {
        for(;;)
        {
            check(!finished.load(::std::memory_order_acquire) && clock_type::now() < deadline &&
                  current.threads.size() == 1u && !current.threads[0u].native_pc, "actual cooperative bootstrap");
            auto next{command(*owner, ::fast_io::concat_std("step asm ", participant))};
            if(next.status == control::error::none)
            {
                check(!next.timed_out && next.reason == dbg::stop_reason::native_step &&
                      next.stop_identifier > current.stop_identifier && next.threads.size() == 1u && next.threads[0u].native_pc &&
                      *next.threads[0u].native_pc == next.native_step_to, "actual kernel trap established");
                return next;
            }
            check(next.status == control::error::unsupported_command && !next.timed_out &&
                  next.stop_identifier == current.stop_identifier && next.threads.size() == 1u && !next.threads[0u].native_pc &&
                  next.threads[0u].location == current.threads[0u].location,
                  "unproved bootstrap instruction retains original cooperative stop");
            current = genuine_wasm_step(next);
        }
    }};
    auto current{seek_native(initial)};
    check(current.status == control::error::none && current.reason == dbg::stop_reason::native_step,
          "actual initial native trap before call-next loop");
    for(::std::size_t i{}; i != 256u; ++i)
    {
        check(clock_type::now() < deadline && !finished.load(::std::memory_order_acquire), "finite actual native call-next budget");
        auto const before{owner->inspect()};
        check(before.stop_identifier == current.stop_identifier && before.reason == dbg::stop_reason::native_step,
              "same actual current native stop, no stale request or forged cursor");
        auto const registers{command(*owner, ::fast_io::concat_std("info registers"))};
        auto const image{command(*owner, ::fast_io::concat_std("disassemble-range ", participant, " ", before.stop_identifier, " 1 0 0 1"))};
        auto const& bytes{image.disassembly_code};
        check(registers.status == control::error::none && registers.registers.pc() == bytes.pc &&
              image.status == control::error::none && bytes.size != 0u && bytes.size <= lib::llvm_jit_debug_max_native_code_bytes &&
              image.disassembly_count == 1u && image.disassembly[0u] && image.disassembly[0u].pc == bytes.pc,
              "canonical real kernel/cursor/owner code and GPR copy");
        // [actual privately authenticated owned code copy ... size<=capacity] end
        // [safe                                                          ]
        //  ^^ MC sees only copied bytes, never dereferences bytes.pc as memory.
        auto const instruction{semantic.decode(bytes.pc, {bytes.bytes, bytes.size})};
        check(bool(instruction), "actual MC decode required, unknown is FAIL");
        auto const decoded{image.disassembly[0u]};
        if(instruction.safe_for_call_continuation())
        {
            check(decoded.size != 0u && decoded.size == instruction.semantics().size &&
                  decoded.size <= UINTPTR_MAX - bytes.pc && bytes.pc + decoded.size < image.disassembly_owner_end,
                  "actual nearCALL continuation scalar bound");
            auto const expected{bytes.pc + decoded.size};
            auto const next{command(*owner, ::fast_io::concat_std("ni"))};
            ::fast_io::io::perrln("NATIVE-CALL-NEXT actualfrom=", ::fast_io::mnp::hex0x(bytes.pc),
                " expected-continuation=", ::fast_io::mnp::hex0x(expected),
                " status=", ::fast_io::mnp::dec(static_cast<unsigned>(next.status)),
                " reason=", ::fast_io::mnp::dec(static_cast<unsigned>(next.native_next_reason)));
            check(next.status == control::error::none && !next.timed_out && next.reason == dbg::stop_reason::native_step &&
                  next.stop_identifier > before.stop_identifier && next.native_step_from == bytes.pc && next.native_step_to == expected &&
                  next.threads.size() == 1u && next.threads[0u].native_pc && *next.threads[0u].native_pc == expected,
                  "true nearCALL executes TF-OFF and completes only at exact owned Wasm continuation");
            auto const after{command(*owner, ::fast_io::concat_std("info registers"))};
            auto const code{command(*owner, ::fast_io::concat_std("disassemble-range ", participant, " ", next.stop_identifier, " 1 0 0 1"))};
            check(after.status == control::error::none && after.registers.pc() == expected &&
                  after.registers.sp() == registers.registers.sp() && after.registers_stop_identifier == next.stop_identifier &&
                  code.status == control::error::none && code.disassembly_code.pc == expected &&
                  code.disassembly_code.module == bytes.module && code.disassembly_code.function == bytes.function &&
                  code.disassembly_code.function_generation == bytes.function_generation && code.disassembly_code.runtime_epoch == bytes.runtime_epoch,
                  "fresh physical origin cursor/kernel SP and actual generation remain jointly authenticated");
            ++call_next; current = next;
            if(call_next == 4u && ordinary != 0u) { break; }
            continue;
        }
        if(instruction.safe_for_single_instruction())
        {
            auto const next{command(*owner, ::fast_io::concat_std("ni"))};
            check(next.status == control::error::none && !next.timed_out && next.reason == dbg::stop_reason::native_step &&
                  next.stop_identifier > before.stop_identifier && next.native_step_from == bytes.pc &&
                  next.native_step_to == bytes.pc + instruction.semantics().size,
                  "ordinary actual machine instruction remains precise alongside real call-next");
            ++ordinary; current = next; continue;
        }
        if(instruction.semantics().kind == dbg::native_instruction_semantics::flow::branch)
        {
            auto const next{command(*owner, ::fast_io::concat_std("step asm ", participant))};
            if(next.status == control::error::none) { current = next; continue; }
            auto const after{command(*owner, ::fast_io::concat_std("info registers"))};
            check(next.status == control::error::unsupported_command && next.stop_identifier == before.stop_identifier &&
                  after.status == control::error::none && same_registers(registers.registers, after.registers),
                  "unproved branch cannot enter VM/host");
            current = seek_native(genuine_wasm_step(next)); continue;
        }
        auto const denied{command(*owner, ::fast_io::concat_std("step asm ", participant))};
        auto const retained{command(*owner, ::fast_io::concat_std("info registers"))};
        check(denied.status == control::error::unsupported_command && denied.stop_identifier == before.stop_identifier &&
              retained.status == control::error::none && same_registers(registers.registers, retained.registers),
              "unsupported prefix/return remains pre-execution refused with exact PC/GPR/stop");
        if(instruction.semantics().kind == dbg::native_instruction_semantics::flow::return_instruction)
        { ++returns; break; }
        ++prefixes; current = seek_native(genuine_wasm_step(denied));
    }
    ::fast_io::io::perrln("NATIVE-CALL-NEXT-COVERAGE ordinary=", ::fast_io::mnp::dec(ordinary),
        " actual-near-call=", ::fast_io::mnp::dec(call_next), " prefixes-retained=", ::fast_io::mnp::dec(prefixes),
        " returns-refused=", ::fast_io::mnp::dec(returns), " actual-wasm-steps=", ::fast_io::mnp::dec(actual_wasm_steps));
    check(ordinary != 0u && call_next != 0u, "missing actual nearCALL NI is FAIL, never decoder-only or SKIP success");
    lib::reset_runtime_state_host_api();
    check(domain->is_closed() && owner->detach_resume(), "real native event/session ACK before owner/runtime drain");
    auto const end{clock_type::now() + ::std::chrono::seconds{10}};
    while(!finished.load(::std::memory_order_acquire))
    { check(clock_type::now() < end, "native borrower/event did not retire"); ::std::this_thread::yield(); }
    guest.join();
    check(result == 125u, "actual Core3 execution remains correct after real NI/reset");
    owner.reset(); check(lifetime.expired(), "actual observer retires after guest ACK and join");
    ::fast_io::io::println("debug_native_host_call_continuation_runtime: PASS actual-near-call-NI=", ::fast_io::mnp::dec(call_next),
        " ordinary=", ::fast_io::mnp::dec(ordinary), " policy=", policy,
        " TF-OFF-host-call-with-real-owned-Wasm-continuation=yes finish/cross-owner/full-platform-qualified=no");
}
