// Actual LLVM-full opt-in entry producer/activation qualification. This does
// not accept whole-instance restore, replay, GC graph export or native-PC jump.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <uwvm2/utils/container/string_concat.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace threads = ::uwvm2::utils::thread;
namespace checkpoint = ::uwvm2::runtime::checkpoint;
using domain = threads::cooperative_pause_domain;
static void require(bool valid, char const* text)
{
    if(!valid) { ::fast_io::io::perrln("debug_checkpoint_entry_runtime: ", ::fast_io::mnp::os_c_str(text)); ::fast_io::fast_terminate(); }
}
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::atomic_bool requested{};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    domain::pause_ticket ticket{}; threads::cooperative_pause_location location{};
    lib::llvm_jit_checkpoint_recording_observation recording{};
    bool captured{}, done{};
    static void point(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location where) noexcept
    {
        // [actual runtime-retained native observer owner] owner_end
        // [safe                                        ] callback-only cast;
        // pointer derives from owned host context, never guest memory/IDs.
        auto& self{*static_cast<observer*>(opaque)};
        if(self.requested.exchange(true, ::std::memory_order_relaxed)) { return; }
        auto ticket{self.control->request_pause()}; require(static_cast<bool>(ticket), "actual first-entry pause request");
        ::std::lock_guard lock{self.mutex}; self.ticket = ::std::move(ticket); self.location = where;
        self.changed.notify_all();
    }
    static void before_park(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location where,
        lib::llvm_jit_debug_local_view local) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};
        auto const recording{lib::llvm_jit_observe_checkpoint_recording_host_api()};
        require(local.captured_count == 2u && local.total_count == 2u && local.availability != nullptr,
            "real new Core3 fixture preserves two original local indices");
        // [actual captured availability[2]] exclusive_end
        // [safe                         ] captured_count==2 before index reads;
        // no payload of the unset nondefaultable local is ever interpreted.
        require(local.availability[0u] == 0u && local.availability[1u] == 1u,
            "nonnull struct local is unavailable before first set; nullable local is initialized");
        ::std::lock_guard lock{self.mutex};
        require(self.location == where, "recording belongs to same real cooperative stop");
        self.recording = recording; self.captured = true; self.changed.notify_all();
    }
};
int main(int argc, char** argv)
{
    if(argc != 3) { return 2; }
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    auto const policy{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}};
    require(policy == "instruction" || policy == "unwind", "explicit instruction/unwind policy");
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction :
        mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0u; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto const path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    // [old argument allocation] no running guest or borrowed cursor
    // [safe                   ] retire cursor before clear/reallocation.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"debug-checkpoint-entry", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())}, nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [actual complete global argument array] end
    // [safe                                ] no later growth through reset;
    // borrow only after both emplacements, backed by main-owned UTF8 path.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(
        ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc = false; features.explicit_enable_gc = true;
    features.disable_function_references = false; features.explicit_enable_function_references = true;
    features.disable_reference_types = false; features.explicit_enable_reference_types = true;
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"checkpoint-entry";
    // Checkpoint materialization binds the immutable image retained BEFORE
    // parsing; a plain mapped input cannot satisfy that source identity.
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true) == static_cast<int>(::uwvm2::uwvm::run::retval::ok),
        "actual owned source parse/initializer with new GC/nondefaultable-local syntax");
    auto state{::std::make_shared<observer>()};
    auto const profile{checkpoint::compilation_profile::create_for_trusted_manager()}; require(bool(profile), "owned immutable profile");
    require(lib::llvm_jit_configure_debug_session_host_api(state->control, {state, observer::point, observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok, "actual full-only session");
    require(lib::llvm_jit_configure_checkpoint_recording_host_api(profile) == lib::llvm_jit_debug_configure_result::ok,
        "actual prepublication opt-in retains immutable compile policy");
    require(!lib::llvm_jit_observe_checkpoint_recording_host_api().instrumented, "external scalar IDs cannot query an activation");
    require(lib::llvm_jit_prepare_debug_host_api(), "same fused validate+emit full compilation");
    require(lib::llvm_jit_configure_checkpoint_recording_host_api(profile) == lib::llvm_jit_debug_configure_result::already_published,
        "published engine cannot mutate checkpoint compilation policy");
    ::std::uint32_t output{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config config{}; config.entry_function_index = 0u;
        // [main-owned real result slot] output_end
        // [safe                      ] normal host ABI exact sizeof extent.
        config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(output));
        config.entry_abi_buffers.result_bytes = sizeof(output);
        lib::full_compile_and_run_main_module(u8"checkpoint-entry", config);
        ::std::lock_guard lock{state->mutex}; state->done = true; state->changed.notify_all();
    }};
    domain::pause_ticket ticket{};
    {
        ::std::unique_lock lock{state->mutex};
        require(state->changed.wait_until(lock, deadline(), [&] { return static_cast<bool>(state->ticket) || state->done; }), "actual pause event");
        require(!state->done && static_cast<bool>(state->ticket), "guest really stops at first opcode"); ticket = state->ticket;
    }
    require(state->control->wait_until_paused(ticket, deadline()) == threads::cooperative_pause_result::paused, "genuine enrolled participant parked");
    {
        ::std::lock_guard lock{state->mutex}; auto const& record{state->recording};
        require(state->captured && state->location.function == 0u && state->location.offset == 0u, "actual first opcode entry");
        require(record.instrumented && record.at_current_opcode && record.status == checkpoint::status::ok && record.incarnation != 0u &&
            record.runtime_epoch == state->location.code_generation && record.function_generation == 1u &&
            record.native_frames == 1u && record.site == 1u && record.typed_slots == 2u, "actual canonical entry typed capture matches activation/profile/generation");
        require(!record.executable_restore_available, "entry capture does not fabricate whole-instance executable restore");
    }
    require(state->control->resume(ticket), "actual first entry resumes"); guest.join();
    require(output == 42u, "new Core3 nondefaultable assignment/struct access executes after capture");
    require(!lib::llvm_jit_observe_checkpoint_recording_host_api().instrumented, "retired native observer context has no activation authority");
    lib::reset_runtime_state_host_api(); require(state->control->is_closed(), "actual reset retires session/profile/code generation");
    ::fast_io::io::println("debug_checkpoint_entry_runtime: PASS actual opt-in typed entry only; whole-state restore/reverse/replay acceptance=false");
}
