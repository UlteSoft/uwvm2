// Genuine original Wasm parse+initializer+single fused compiler+actual pause
// capture+retirement+actual engine-resolved logical continuation execution.
// This qualifies the execution slice, not complete instance/file/GC rollback.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/utils/container/string_concat.h>
#include <fast_io.h>
#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace threads = ::uwvm2::utils::thread;
namespace checkpoint = ::uwvm2::runtime::checkpoint;
using domain = threads::cooperative_pause_domain;
static void require(bool valid, char const* message)
{
    if(!valid) { ::fast_io::io::perrln("checkpoint actual nested reference result: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::atomic_size_t op_count{};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    domain::pause_ticket ticket{};
    lib::llvm_jit_checkpoint_thread_capture_owner capture{};
    lib::llvm_jit_checkpoint_recording_observation recording{};
    threads::cooperative_pause_location location{};
    bool requested{}, copied{}, finished{};
    static void point(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location where) noexcept
    {
        // [actual runtime-retained observer owner] lifetime covers both runs.
        // [safe] this native context was installed by the real host manager.
        auto& self{*static_cast<observer*>(opaque)};
        if(where.function != 1u || self.op_count.fetch_add(1u, ::std::memory_order_relaxed) != 1u) { return; }
        auto ticket{self.control->request_pause()}; require(static_cast<bool>(ticket), "real pre-nop pause requested");
        ::std::lock_guard lock{self.mutex};
        require(!self.requested, "one actual capture episode");
        self.ticket = ::std::move(ticket); self.location = where; self.requested = true; self.changed.notify_all();
    }
    static void before_park(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location where,
        lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)}; ::std::lock_guard lock{self.mutex};
        require(self.requested && !self.copied && self.location == where, "same genuine before-park episode");
        auto saved{lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket)};
        require(saved.status == lib::llvm_jit_checkpoint_capture_status::captured && saved.capture,
            "actual canonical source/engine/plan/TLS/frame typed capture");
        self.recording = lib::llvm_jit_observe_checkpoint_recording_host_api();
        require(self.recording.instrumented && self.recording.at_current_opcode &&
            self.recording.status == checkpoint::status::ok && self.recording.native_frames == 2u &&
            self.recording.typed_slots == 1u && self.recording.site == 2u,
            "same-walk GC-result function live scalar operand before allocation");
        self.capture = ::std::move(saved.capture); self.copied = true; self.changed.notify_all();
    }
};
int main(int argc, char** argv)
{
    if(argc != 4) { return 2; }
    auto const expected{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[3]))};
    require(expected == "struct" || expected == "array" || expected == "function" || expected == "wrapper" ||
        expected == "i31" || expected == "null" || expected == "exception", "explicit actual reference result class");
    auto const policy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]))};
    require(policy == "instruction" || policy == "unwind", "explicit stack strategy");
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction :
        mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0u; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto const path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    // [old argument allocation] no source/guest borrows it yet.
    // [safe] retire its cursor BEFORE clear/reallocation.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"checkpoint-call-continuation", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())}, nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [complete owning argument array] end; no subsequent vector growth.
    // [safe] borrow the real last argument only after both emplacements.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(
        ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc = false; features.explicit_enable_gc = true;
    features.disable_function_references = false; features.explicit_enable_function_references = true;
    features.disable_reference_types = false; features.explicit_enable_reference_types = true;
    features.disable_exceptions = false; features.explicit_enable_exceptions = true;
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"checkpoint-call-continuation";
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true) == static_cast<int>(::uwvm2::uwvm::run::retval::ok),
        "real full-source parse and initializer of modern nondefaultable i31 syntax");
    auto state{::std::make_shared<observer>()};
    auto profile{checkpoint::compilation_profile::create_for_trusted_manager()}; require(bool(profile), "immutable actual profile");
    require(lib::llvm_jit_configure_debug_session_host_api(state->control, {state, observer::point, observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok, "real reserved debugger");
    require(lib::llvm_jit_configure_checkpoint_recording_host_api(profile) == lib::llvm_jit_debug_configure_result::ok, "actual prepublication checkpoint selection");
    require(lib::llvm_jit_prepare_debug_host_api(), "actual single fused validation+translation and ALL native entry resolution");
    ::std::array<::std::byte, sizeof(checkpoint::native_reference)> original{}; original.fill(::std::byte{0xa5u});
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config run{}; run.entry_function_index = 0u;
        // [main-owned original result of declared exact width] end
        // [safe] owner remains live until this real guest thread joins.
        run.entry_abi_buffers.result_buffer = original.data();
        run.entry_abi_buffers.result_bytes = original.size();
        lib::full_compile_and_run_main_module(u8"checkpoint-call-continuation", run);
        ::std::lock_guard lock{state->mutex}; state->finished = true; state->changed.notify_all();
    }};
    domain::pause_ticket ticket{};
    {
        ::std::unique_lock lock{state->mutex};
        require(state->changed.wait_until(lock, deadline(), [&] { return state->copied || state->finished; }), "finite real capture event");
        require(state->copied && !state->finished && state->capture, "original native execution really parked");
        ticket = state->ticket;
    }
    require(state->control->wait_until_paused(ticket, deadline()) == threads::cooperative_pause_result::paused,
        "current actual participant fully parked");
    lib::llvm_jit_checkpoint_thread_capture_owner owners[1u]{state->capture};
    auto foreign_retire{lib::llvm_jit_checkpoint_thread_capture_owner{state->capture.get(), [](auto*) noexcept {}}};
    lib::llvm_jit_checkpoint_thread_capture_owner foreign_owners[1u]{foreign_retire};
    require(lib::llvm_jit_checkpoint_retire_saved_execution_host_api(ticket, foreign_owners) ==
        lib::llvm_jit_checkpoint_execution_retirement_status::rejected_current_cohort,
        "foreign shared_ptr control block does not authorize original retirement");
    ::std::mutex watchdog_mutex; ::std::condition_variable watchdog_changed; bool drained{};
    ::std::thread watchdog{[&]
    {
        ::std::unique_lock lock{watchdog_mutex};
        require(watchdog_changed.wait_until(lock, deadline(), [&] { return drained; }),
            "finite real execution retirement and native/GC/host admission drain");
    }};
    require(lib::llvm_jit_checkpoint_retire_saved_execution_host_api(ticket, owners) ==
        lib::llvm_jit_checkpoint_execution_retirement_status::execution_retired,
        "real ONEcohort transfer, private cleanup signal and actual execution-domain drain");
    { ::std::lock_guard lock{watchdog_mutex}; drained = true; watchdog_changed.notify_all(); }
    watchdog.join(); guest.join();
    for(auto const byte : original) { require(byte == ::std::byte{0xa5u}, "retired original reference result remains untouched"); }
    ::std::array<::std::byte, sizeof(checkpoint::native_reference)> output{}; output.fill(::std::byte{0x5au});
    require(lib::llvm_jit_checkpoint_continue_saved_thread_host_api(state->capture, output.data(), output.size()) ==
        lib::llvm_jit_checkpoint_continuation_status::continued,
        "actual restored native parent receives legal registered GC child result");
    checkpoint::native_reference actual{};
    static_assert(sizeof(actual) == output.size());
    // [one complete actual output owner] end
    // [safe] exact native carrier extent BEFORE copying constructed tag/token;
    // host never dereferences a payload or follows a token obtained from DATA.
    ::fast_io::freestanding::my_memcpy(::std::addressof(actual), output.data(), sizeof(actual));
    using kind = ::uwvm2::object::global::wasm_ref_kind;
    bool valid{};
    if(expected == "null") { valid = actual.kind == kind::wasm_null; }
    else if(expected == "i31") { valid = actual.kind == kind::wasm_i31 && actual.storage.wasm_i31.get_u() == 7u; }
    else
    {
        // [constructed actual complete carrier] only its active pointer member
        // [safe] numeric/i31/null cases were separated BEFORE pointer selection.
        valid = actual.storage.ptr != nullptr &&
            ((expected == "struct" && actual.kind == kind::wasm_struct) ||
             (expected == "array" && actual.kind == kind::wasm_array) ||
             (expected == "function" && actual.kind == kind::wasm_func_defined) ||
             (expected == "wrapper" && actual.kind == kind::wasm_extern) ||
             (expected == "exception" && actual.kind == kind::wasm_exn));
    }
    require(valid,"actual legal reference tag exported only after genuine complete normal return");
    lib::reset_runtime_state_host_api();
    ::fast_io::io::println("checkpoint_actual_nested_reference: PASS kind=", expected, " actual native return/root handoff; graph_rollback=false");
}
