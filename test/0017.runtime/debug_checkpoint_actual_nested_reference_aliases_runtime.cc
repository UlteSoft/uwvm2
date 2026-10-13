// Genuine original Wasm parse+initializer+single fused compiler+actual pause
// capture+retirement+actual engine-resolved logical continuation execution.
// This qualifies the execution slice, not complete instance/file/GC rollback.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/uwvm/debugger/wasm_state.h>
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
namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
using domain = threads::cooperative_pause_domain;
static void require(bool valid, char const* message)
{
    if(!valid) { ::fast_io::io::perrln("checkpoint actual nested reference aliases: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::atomic_size_t op_count{};
    ::std::atomic_uint phase{};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    domain::pause_ticket ticket{};
    lib::llvm_jit_checkpoint_thread_capture_owner capture{};
    lib::llvm_jit_checkpoint_recording_observation recording{};
    threads::cooperative_pause_location location{};
    ::std::uint_least64_t participant{};
    bool requested{}, copied{}, finished{};
    static void point(void* opaque, ::std::uint_least64_t participant, threads::cooperative_pause_location where) noexcept
    {
        // [actual runtime-retained observer owner] lifetime covers both runs.
        // [safe] this native context was installed by the real host manager.
        auto& self{*static_cast<observer*>(opaque)};
        auto const phase{self.phase.load(::std::memory_order_acquire)};
        if(where.function != (phase == 0u ? 1u : 2u) ||
           self.op_count.fetch_add(1u, ::std::memory_order_relaxed) != (phase == 0u ? 1u : 3u)) { return; }
        auto ticket{self.control->request_pause()}; require(static_cast<bool>(ticket), "real pre-nop pause requested");
        ::std::lock_guard lock{self.mutex};
        require(!self.requested, "one actual capture episode");
        self.ticket = ::std::move(ticket); self.location = where; self.participant = participant; self.requested = true; self.changed.notify_all();
    }
    static void before_park(void* opaque, ::std::uint_least64_t participant, threads::cooperative_pause_location where,
        lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)}; ::std::lock_guard lock{self.mutex};
        require(self.requested && !self.copied && self.location == where && self.participant == participant, "same genuine before-park episode");
        auto saved{lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket)};
        require(saved.status == lib::llvm_jit_checkpoint_capture_status::captured && saved.capture,
            "actual canonical source/engine/plan/TLS/frame typed capture");
        self.recording = lib::llvm_jit_observe_checkpoint_recording_host_api();
        require(self.recording.instrumented && self.recording.at_current_opcode &&
            self.recording.status == checkpoint::status::ok && self.recording.native_frames == 2u &&
            self.recording.typed_slots == (self.phase.load(::std::memory_order_relaxed) == 0u ? 1u : 3u) &&
            self.recording.site == (self.phase.load(::std::memory_order_relaxed) == 0u ? 2u : 4u),
            "actual two-frame typed chain before allocation or with three true aggregate aliases");
        self.capture = ::std::move(saved.capture); self.copied = true; self.changed.notify_all();
    }
};
int main(int argc, char** argv)
{
    if(argc != 3) { return 2; }
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
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"checkpoint-call-continuation";
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true) == static_cast<int>(::uwvm2::uwvm::run::retval::ok),
        "real full-source parse and initializer of modern nondefaultable i31 syntax");
    auto state{::std::make_shared<observer>()};
    auto profile{checkpoint::compilation_profile::create_for_trusted_manager()}; require(bool(profile), "immutable actual profile");
    require(lib::llvm_jit_configure_debug_session_host_api(state->control, {state, observer::point, observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction) == lib::llvm_jit_debug_configure_result::ok, "real reserved debugger");
    require(lib::llvm_jit_configure_checkpoint_recording_host_api(profile) == lib::llvm_jit_debug_configure_result::ok, "actual prepublication checkpoint selection");
    ::uwvm2::runtime::gc::scoped_cli_gc_execution gc_compilation_owner{};
    require(lib::runtime_gc_prepare_cli_collection_host_api() == ::uwvm2::runtime::gc::managed_gc_configure_result::requested,
        "real managed collector/root context reserved before actual native compilation");
    require(lib::llvm_jit_prepare_debug_host_api(), "actual single fused validation+translation and ALL native entry resolution");
    ::std::array<::std::byte, sizeof(::std::uint32_t)> original{}; original.fill(::std::byte{0xa5u});
    ::std::thread guest{[&]
    {
        ::uwvm2::runtime::gc::scoped_cli_gc_execution actual_original_vm_execution{};
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
    auto const first_capture{state->capture};
    {
        ::std::lock_guard lock{state->mutex};
        state->phase.store(1u,::std::memory_order_release); state->op_count.store(0u,::std::memory_order_relaxed);
        state->requested = state->copied = state->finished = false; state->ticket = {}; state->capture.reset();
    }
    ::std::array<::std::byte, sizeof(::std::uint32_t)> output{}; output.fill(::std::byte{0x5au});
    auto resumed_status{lib::llvm_jit_checkpoint_continuation_status::allocation_failed};
    ::std::thread resumed{[&]
    {
        // Deliberately no CLI classification on this native management thread.
        // The canonical dispatcher classifies only after ALL real input/code
        // checks; a valid internal GC return is not an untracked native escape.
        resumed_status = lib::llvm_jit_checkpoint_continue_saved_thread_host_api(first_capture,output.data(),output.size());
        ::std::lock_guard lock{state->mutex}; state->finished = true; state->changed.notify_all();
    }};
    domain::pause_ticket second{};
    {
        ::std::unique_lock lock{state->mutex};
        require(state->changed.wait_until(lock,deadline(),[&]{return state->copied || state->finished;}),
            "finite actual post-GC-return second pause");
        require(state->copied && !state->finished && state->capture,
            "real native parent stays live with consumer child and aggregate aliases");
        second = state->ticket;
    }
    require(state->control->wait_until_paused(second,deadline()) == threads::cooperative_pause_result::paused,
        "actual aggregate-return child/parent cohort fully stopped");
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner,1u> current{state->capture};
    ws::request request{}; request.participant = state->participant; request.frame = 0u;
    request.selected = ws::selection::locals; request.count = 2u;
    auto aliases{lib::llvm_jit_debug_query_wasm_state_host_api(second,current,request)};
    require(aliases.result == ws::status::available && aliases.rows.size() == 2u &&
        aliases.rows[0u].data.available && aliases.rows[1u].data.available &&
        aliases.rows[0u].data.ref.kind == ws::reference_kind::structure &&
        aliases.rows[1u].data.ref.kind == ws::reference_kind::structure &&
        aliases.rows[0u].data.ref.object != 0u &&
        aliases.rows[0u].data.ref.object == aliases.rows[1u].data.ref.object,
        "two consumer local aliases resolve to ONE genuine N-owned GC object");
    request.count = 1u; request.member_count = 1u;
    auto field{lib::llvm_jit_debug_query_wasm_state_host_api(second,current,request)};
    require(field.result == ws::status::available && field.selected_object != 0u && field.objects.size() == 1u &&
        field.objects[0u].kind == ws::object_kind::structure && field.objects[0u].members.size() == 1u &&
        field.objects[0u].members[0u].data.available && field.objects[0u].members[0u].data.type.kind == ws::value_kind::i32,
        "root-derived current aggregate field is actually readable, not an opaque address");
    ::std::uint32_t field_bits{}; auto const& bytes{field.objects[0u].members[0u].data.bits};
    // [complete canonical LE copied numeric bytes16] end
    // [safe] four-byte parse range proved BEFORE its bounded pointer advance.
    auto const first_byte{reinterpret_cast<unsigned char const*>(bytes.data())};
    auto const parsed{::fast_io::parse_by_scan(first_byte,first_byte+4u,::fast_io::mnp::le_get<32>(field_bits))};
    require(parsed.code == ::fast_io::parse_code::ok && parsed.iter == first_byte+4u && field_bits == 42u,
        "actual shared aggregate value42 is canonical across host byte orders");
    request.member_count = 0u; request.frame = 1u;
    auto parent{lib::llvm_jit_debug_query_wasm_state_host_api(second,current,request)};
    require(parent.result == ws::status::available && parent.rows.size() == 1u && parent.rows[0u].data.available &&
        parent.rows[0u].data.ref.kind == ws::reference_kind::structure,
        "actual parent saved local keeps its returned GC object rooted during new child stop");
    request.frame = 0u; request.selected = ws::selection::operands;
    auto operand{lib::llvm_jit_debug_query_wasm_state_host_api(second,current,request)};
    require(operand.result == ws::status::available && operand.rows.size() == 1u && operand.rows[0u].data.available &&
        operand.rows[0u].data.ref.kind == ws::reference_kind::structure,
        "same consumer live operand retains a real typed aggregate reference");
    require(state->control->resume(second),"resume actual aggregate aliases without graph rollback"); resumed.join();
    require(resumed_status == lib::llvm_jit_checkpoint_continuation_status::continued,
        "actual root continuation returns normally after legal GC return and new alias pause");
    ::std::uint32_t final_bits{};
    // [real owning exact native scalar result] end
    // [safe] fixed complete width BEFORE constructing numeric carrier.
    ::fast_io::freestanding::my_memcpy(::std::addressof(final_bits),output.data(),sizeof(final_bits));
    require(final_bits == 84u,"consumer reads42 and real parent reads same rooted returned object42");
    lib::reset_runtime_state_host_api();
    ::fast_io::io::println("checkpoint_actual_nested_reference_aliases: PASS actual GC child return, two real frames, shared locals/operand/rooted member and parent read; graph_rollback=false");
}
