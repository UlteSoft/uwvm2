// Actual full-runtime COOPERATIVE owned native-code view witness.
// No trap adapter, GPR/native-step, arbitrary address or VM restore qualification.
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
        ::fast_io::io::perrln("debug_native_actual_target_view_runtime: ", ::fast_io::mnp::os_c_str(text));
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
        u8"uwvm-debug-native-actual-target-view", nullptr,
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
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"native-actual-target-view-test";
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
    ::fast_io::io::println("debug_native_actual_target_view_runtime: SKIP host process adapter unavailable");
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
        lib::full_compile_and_run_main_module(u8"native-actual-target-view-test", run);
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
    auto range_text{::fast_io::concat_std("disassemble-range ", participant, " ", initial.stop_identifier, " 4 0 0 0")};
    auto range{command(*owner, range_text)};
    check(range.status == control::error::none && range.disassembly_count == 4u && range.disassembly[0u] &&
          !range.disassembly_code.native_instruction_stop && !range.native_instruction &&
          range.disassembly_code.pc == range.disassembly[0u].pc &&
          range.disassembly_code.target.description_version == 1u &&
          dbg::native_target_metadata::valid(range.disassembly_code.target),
          "one actual private code/target image yields an exact cooperative boundary");
    auto const& actual_target{range.disassembly_code.target};
    auto const native_entry{range.disassembly_owner_begin};
    check(native_entry != 0u && range.disassembly_code.pc >= native_entry &&
          range.disassembly_code.pc < range.disassembly_owner_end &&
          range.disassembly_owner_end - native_entry <= PTRDIFF_MAX,
          "genuine same-generation function range remains bounded");
    auto parsed{dbg::parse_console_command(::fast_io::string_view{range_text.data(), range_text.size()})};
    auto visible{dbg::details::format_reply(range, parsed)};
    check(visible.find("origin=safepoint-code-view") != ::std::string::npos &&
          visible.find("origin=native-instruction-stop") == ::std::string::npos,
          "actual formatter explicitly labels a safepoint code VIEW");
    auto plain_text{::fast_io::concat_std("disassemble ", participant, " ", initial.stop_identifier, " 4")};
    auto plain{command(*owner, plain_text)};
    check(plain.status == control::error::none && plain.disassembly_count == 4u &&
          !plain.disassembly_code.native_instruction_stop && plain.disassembly_code.pc == range.disassembly_code.pc &&
          plain.disassembly_code.target.triple == actual_target.triple &&
          plain.disassembly_code.target.cpu == actual_target.cpu &&
          plain.disassembly_code.target.features == actual_target.features &&
          plain.disassembly_code.target.little_endian == actual_target.little_endian &&
          plain.disassembly_code.target.pointer_bits == sizeof(::std::uintptr_t) * 8u,
          "plain cooperative display uses real current engine target DATA");
    for(::std::size_t index{}; index != range.disassembly_count; ++index)
    {
        // [same two OWNED decoded pages ... count<=32] end
        // [safe                                      ] index<count;
        //  ^^ comparing copied values cannot read a requested native address.
        auto const& a{range.disassembly[index]}; auto const& b{plain.disassembly[index]};
        check(a.pc == b.pc && a.size == b.size && a.bytes == b.bytes, "two actual view commands show the same boundaries");
    }
    auto regs{command(*owner, ::fast_io::concat_std("info registers"))};
    auto next{command(*owner, ::fast_io::concat_std("ni"))};
    check(regs.status != control::error::none && next.status != control::error::none &&
          owner->inspect().stop_identifier == initial.stop_identifier &&
          !owner->inspect().threads[0u].native_pc,
          "read-only cooperative view cannot manufacture real trap registers or NI authority");
    auto wrong{command(*owner, ::fast_io::concat_std("disassemble-range ", participant, " ",
        initial.stop_identifier + 1u, " 1 0 0 0"))};
    check(wrong.status != control::error::none, "stale stop request refuses without following a PC");
    lib::reset_runtime_state_host_api();
    auto retired{command(*owner, range_text)};
    check(domain->is_closed() && retired.status != control::error::none && owner->detach_resume(),
          "reset invalidates private capture and retires actual stopped execution");
    auto const end{clock_type::now() + ::std::chrono::seconds{10}};
    while(!finished.load(::std::memory_order_acquire))
    { check(clock_type::now() < end, "cooperative guest drain timeout"); ::std::this_thread::yield(); }
    guest.join();
    check(result == 125u, "Core3 GC declaration/tail-call input-dependent guest result remains correct");
    owner.reset(); check(lifetime.expired(), "actual observer ownership drains before destruction");
    ::fast_io::io::println("debug_native_actual_target_view_runtime: PASS policy=", policy,
        " actual-typed-engine-description=yes cooperative-boundary=yes real-controller=yes",
        " hardware-step-qualified=no native-registers-qualified=no");
}
