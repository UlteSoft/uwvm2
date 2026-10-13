// Genuine generation-2 fused typed state after hot replacement. Both pause
// episodes/captures are produced by native JIT execution, never synthetic DATA.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/uwvm/debugger/wasm_state.h>
#include <uwvm2/utils/container/string_concat.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace threads = ::uwvm2::utils::thread;
namespace checkpoint = ::uwvm2::runtime::checkpoint;
namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
using domain = threads::cooperative_pause_domain;
static void require(bool valid, char const* message)
{
    if(!valid) { ::fast_io::io::perrln("checkpoint generation: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    domain::pause_ticket ticket{};
    lib::llvm_jit_checkpoint_thread_capture_owner capture{};
    lib::llvm_jit_checkpoint_recording_observation recording{};
    threads::cooperative_pause_location location{};
    ::std::uint_least64_t participant{};
    unsigned phase{}, worker_opcode{};
    bool requested{}, copied{}, finished{};
    static void point(void* opaque, ::std::uint_least64_t participant, threads::cooperative_pause_location where) noexcept
    {
        // [actual runtime-retained observer owner] both actual stops retain it.
        // [safe] installed by real host manager, never file/request pointer.
        auto& self{*static_cast<observer*>(opaque)};
        ::std::lock_guard lock{self.mutex};
        if(self.requested) { return; }
        if(self.phase == 0u) { if(where.function != 0u || where.offset != 0u) { return; } }
        else
        {
            if(where.function != 1u || self.worker_opcode++ != 4u) { return; }
        }
        self.ticket = self.control->request_pause(); require(bool(self.ticket), "genuine before-opcode pause");
        self.location = where; self.participant = participant; self.requested = true; self.changed.notify_all();
    }
    static void before_park(void* opaque, ::std::uint_least64_t participant,
        threads::cooperative_pause_location where, lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)}; ::std::lock_guard lock{self.mutex};
        require(self.requested && !self.copied && self.location == where && self.participant == participant,
            "same genuine stop and participant");
        auto saved{lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket)};
        require(saved.status == lib::llvm_jit_checkpoint_capture_status::captured && bool(saved.capture),
            "actual canonical current source/engine/TLS/plan typed capture");
        self.recording = lib::llvm_jit_observe_checkpoint_recording_host_api();
        require(self.recording.instrumented && self.recording.at_current_opcode &&
            self.recording.status == checkpoint::status::ok &&
            self.recording.function_generation == (self.phase == 0u ? 1u : 2u) &&
            self.recording.native_frames == (self.phase == 0u ? 1u : 2u),
            "actual generation and complete typed activation chain");
        if(self.phase != 0u)
        { require(self.recording.typed_slots == 2u, "new nondefaultable local plus live i32 operand"); }
        self.capture = ::std::move(saved.capture); self.copied = true; self.changed.notify_all();
    }
};
static domain::pause_ticket wait_for_actual(observer& state)
{
    ::std::unique_lock lock{state.mutex};
    require(state.changed.wait_until(lock, deadline(), [&] { return state.copied || state.finished; }), "finite native stop");
    require(state.copied && !state.finished && state.capture, "guest genuinely paused before completion");
    return state.ticket;
}
int main(int argc, char** argv)
{
    if(argc != 3) { return 64; }
    auto const policy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]))};
    require(policy == "instruction" || policy == "unwind", "explicit actual stack strategy");
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction :
        mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0u; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto const path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    // [old CLI array] no guest exists; retire cursor before any reallocation.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"checkpoint-generation", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())}, nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [actual owning finalized CLI array] end; no further growth before reset.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(
        ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc = false; features.explicit_enable_gc = true;
    features.disable_function_references = false; features.explicit_enable_function_references = true;
    features.disable_reference_types = false; features.explicit_enable_reference_types = true;
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"checkpoint-generation";
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true) == static_cast<int>(::uwvm2::uwvm::run::retval::ok),
        "actual immutable before-parse modern source");
    auto state{::std::make_shared<observer>()};
    require(lib::llvm_jit_configure_debug_session_host_api(state->control, {state, observer::point, observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok, "real reserved debugger");
    ::uwvm2::runtime::gc::scoped_cli_gc_execution gc_compilation_owner{};
    require(lib::runtime_gc_prepare_cli_collection_host_api() == ::uwvm2::runtime::gc::managed_gc_configure_result::requested,
        "actual managed-root producer selected before compilation");
    require(lib::llvm_jit_configure_debug_value_observation_host_api() == lib::llvm_jit_debug_configure_result::ok,
        "actual immutable observation profile selected");
    require(lib::llvm_jit_prepare_debug_host_api(), "one fused validate-and-translate original generation");
    ::std::uint32_t result{};
    ::std::thread guest{[&]
    {
        ::uwvm2::runtime::gc::scoped_cli_gc_execution actual_gc_owner{};
        lib::full_compile_run_config run{}; run.entry_function_index = 0u;
        // [main-owned exact declared result] end; main retains it through join.
        run.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(result));
        run.entry_abi_buffers.result_bytes = sizeof(result);
        lib::full_compile_and_run_main_module(u8"checkpoint-generation", run);
        ::std::lock_guard lock{state->mutex}; state->finished = true; state->changed.notify_all();
    }};
    auto first{wait_for_actual(*state)};
    require(state->control->wait_until_paused(first, deadline()) == threads::cooperative_pause_result::paused,
        "whole actual original cohort parked");
    auto const old_capture{state->capture};
    // Complete function body with one nondefaultable (ref i31) local. WAT
    // sibling identifies exact semantics; official assembler body cross-check is
    // a mandatory Linux prerequisite, not inferred from this byte constant.
    ::std::array<::std::byte, 20u> const body{::std::byte{0x01}, ::std::byte{0x01}, ::std::byte{0x64}, ::std::byte{0x6c},
        ::std::byte{0x41}, ::std::byte{0xe3}, ::std::byte{0x00}, ::std::byte{0xfb}, ::std::byte{0x1c},
        ::std::byte{0x21}, ::std::byte{0x00}, ::std::byte{0x41}, ::std::byte{0x07}, ::std::byte{0x01},
        ::std::byte{0x20}, ::std::byte{0x00}, ::std::byte{0xfb}, ::std::byte{0x1e}, ::std::byte{0x6a}, ::std::byte{0x0b}};
    require(state->location.function == 0u && state->recording.native_frames == 1u,
        "target function 1 absent from the actual complete stack");
    auto prepared{lib::llvm_jit_debug_prepare_function_replacement_host_api(0u, 1u, 1u, body.data(), body.size())};
    require(prepared.status == lib::llvm_jit_debug_replace_status::replaced && prepared.transaction,
        "replacement same ABI with new local declaration fused validate and typed-plan emission");
    lib::llvm_jit_debug_replace_result committed{};
    require(state->control->while_stopped(first, [&]
    { committed = lib::llvm_jit_debug_commit_function_replacement_host_api(prepared.transaction); }) &&
        committed.status == lib::llvm_jit_debug_replace_status::replaced && committed.generation == 2u,
        "private owner and complete typed generation committed under actual stop");
    lib::llvm_jit_debug_discard_function_replacement_host_api(prepared.transaction);
    {
        ::std::lock_guard lock{state->mutex}; state->phase = 1u; state->requested = false; state->copied = false;
        state->ticket = {}; state->capture.reset();
    }
    require(state->control->resume(first), "resume original caller into its newly published target");
    auto second{wait_for_actual(*state)};
    require(state->control->wait_until_paused(second, deadline()) == threads::cooperative_pause_result::paused,
        "new generation actual guest parked");
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 1u> captures{state->capture};
    ws::request request{}; request.participant = state->participant; request.frame = 0u; request.count = 1u;
    request.selected = ws::selection::locals;
    auto local{lib::llvm_jit_debug_query_wasm_state_host_api(second, captures, request)};
    require(local.result == ws::status::available && local.rows.size() == 1u && local.rows[0u].data.available &&
        local.rows[0u].data.type.kind == ws::value_kind::reference && local.rows[0u].data.type.heap == -20 &&
        !local.rows[0u].data.type.nullable && local.rows[0u].data.ref.kind == ws::reference_kind::i31 &&
        local.rows[0u].data.ref.i31_bits == 99u, "real generation2 new local declaration and i31 value");
    request.selected = ws::selection::operands;
    auto operand{lib::llvm_jit_debug_query_wasm_state_host_api(second, captures, request)};
    require(operand.result == ws::status::available && operand.rows.size() == 1u && operand.rows[0u].data.available &&
        operand.rows[0u].data.type.kind == ws::value_kind::i32, "real replacement typed operand");
    ::std::uint32_t bits{}; auto const& data{operand.rows[0u].data.bits};
    // [fixed canonical 16-byte copied value] end; exactly four proven bytes.
    auto const first_byte{reinterpret_cast<unsigned char const*>(data.data())};
    auto const parsed{::fast_io::parse_by_scan(first_byte, first_byte + 4u, ::fast_io::mnp::le_get<32>(bits))};
    require(parsed.code == ::fast_io::parse_code::ok && parsed.iter == first_byte + 4u && bits == 7u,
        "canonical LE value parsed by FastIO without host-endian assumptions");
    captures[0u] = old_capture;
    require(lib::llvm_jit_debug_query_wasm_state_host_api(second, captures, request).result != ws::status::available,
        "old stop snapshot cannot impersonate current generation episode");
    require(state->control->resume(second), "resume real generation2"); guest.join();
    require(result == 106u && !state->recording.executable_restore_available,
        "new behavior executed; observation remains no restore authority");
    lib::reset_runtime_state_host_api();
    ::fast_io::io::println("checkpoint_generation: PASS actual generation2 local/i31/operand/caller, stale capture rejected");
}
