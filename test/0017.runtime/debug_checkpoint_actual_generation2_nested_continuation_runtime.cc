// Genuine original Wasm parse+initializer+single fused compiler+actual pause
// capture+retirement+actual engine-resolved logical continuation execution.
// This qualifies the execution slice, not complete instance/file/GC rollback.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/runtime/gc/frame_roots.h>
#include <uwvm2/utils/container/string_concat.h>
#include <fast_io.h>
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
using domain = threads::cooperative_pause_domain;
static void require(bool valid, char const* message)
{
    if(!valid) { ::fast_io::io::perrln("checkpoint actual generation2 nested continuation: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::atomic_size_t op_count{}, target_count{8u};
    ::std::atomic_bool armed{true}, initial_root{true};
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
        if(self.initial_root.load(::std::memory_order_acquire))
        {
            if(where.function != 0u || where.offset != 0u) { return; }
            self.initial_root.store(false,::std::memory_order_release);
        }
        else
        {
            if(where.function != 1u || !self.armed.load(::std::memory_order_acquire) ||
               self.op_count.fetch_add(1u, ::std::memory_order_relaxed) != self.target_count.load(::std::memory_order_relaxed)) { return; }
            self.armed.store(false, ::std::memory_order_release);
        }
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
            self.recording.status == checkpoint::status::ok, "actual materialized typed current opcode");
        if(where.function == 0u)
        {
            require(self.recording.native_frames == 1u && self.recording.function_generation == 1u,
                "original target absent from real root-only replacement stop");
        }
        else
        {
            require(self.recording.native_frames == 2u && self.recording.function_generation == 2u &&
                self.recording.typed_slots == 3u && self.recording.site == 9u,
                "genuine replacement generation2 local/prefix/leaf operand and two native frames");
        }
        self.capture = ::std::move(saved.capture); self.copied = true; self.changed.notify_all();
    }
};
int main(int argc, char** argv)
{
    if(argc != 4) { return 2; }
    // Official assembler-extracted full replacement body (local declaration
    // prefix + expression), supplied by the sole Linux test keeper. Native file
    // loader owns the exact byte extent through actual fused compilation.
    ::fast_io::native_file_loader replacement_body{::fast_io::mnp::os_c_str(argv[3])};
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
    require(lib::llvm_jit_prepare_debug_host_api(), "actual single fused validation+translation and ALL native entry resolution");
    ::std::uint32_t original{0xa5a5a5a5u};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config run{}; run.entry_function_index = 0u;
        // [main-owned original result of declared exact width] end
        // [safe] owner remains live until this real guest thread joins.
        run.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(original));
        run.entry_abi_buffers.result_bytes = sizeof(original);
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
    auto const old_root_capture{state->capture};
    require(replacement_body.size() != 0u,"official actual generation2 body supplied");
    auto replacement{lib::llvm_jit_debug_prepare_function_replacement_host_api(0u,1u,1u,
        reinterpret_cast<::std::byte const*>(replacement_body.data()),replacement_body.size())};
    require(replacement.status == lib::llvm_jit_debug_replace_status::replaced && replacement.transaction,
        "genuine generation2 same ABI fused validation/translation and native resume symbol resolved");
    lib::llvm_jit_debug_replace_result committed{};
    require(state->control->while_stopped(ticket,[&]
    { committed = lib::llvm_jit_debug_commit_function_replacement_host_api(replacement.transaction); }) &&
        committed.status == lib::llvm_jit_debug_replace_status::replaced && committed.generation == 2u,
        "actual all-park commit publishes current plan/raw resume/owner generation2 together");
    lib::llvm_jit_debug_discard_function_replacement_host_api(replacement.transaction);
    {
        ::std::lock_guard lock{state->mutex}; state->requested = state->copied = false;
        state->ticket = {}; state->capture.reset();
    }
    require(state->control->resume(ticket),"actual original caller enters newly published generation2");
    {
        ::std::unique_lock lock{state->mutex};
        require(state->changed.wait_until(lock,deadline(),[&]{return state->copied || state->finished;}),
            "finite real generation2 original pause");
        require(state->copied && !state->finished && state->capture && state->recording.function_generation == 2u,
            "real generation2 target owns current canonical capture");
        ticket = state->ticket;
    }
    require(state->control->wait_until_paused(ticket,deadline()) == threads::cooperative_pause_result::paused,
        "replacement generation2 current participant fully parked");
    lib::llvm_jit_checkpoint_thread_capture_owner old_generation_episode[1u]{old_root_capture};
    require(lib::llvm_jit_checkpoint_retire_saved_execution_host_api(ticket,old_generation_episode) ==
        lib::llvm_jit_checkpoint_execution_retirement_status::rejected_current_cohort,
        "old root-only episode cannot authorize retirement of actual new generation2 stop");
    ::std::uint32_t output{0xdeadbeefu};
    require(lib::llvm_jit_checkpoint_continue_saved_thread_host_api(state->capture, ::std::addressof(output), sizeof(output)) ==
        lib::llvm_jit_checkpoint_continuation_status::original_execution_still_live && output == 0xdeadbeefu,
        "genuine pause is insufficient while old native execution still exists");
    auto original_capture{state->capture};
    lib::llvm_jit_checkpoint_thread_capture_owner owners[1u]{original_capture};
    auto prepare_generation2=[](domain::pause_ticket const& stopped,
        ::std::span<lib::llvm_jit_checkpoint_thread_capture_owner const> cohort)
    {
        namespace gc=::uwvm2::runtime::gc;
        gc::root_reference inherited_value{};gc::scoped_root_frame inherited_scope{{&inherited_value,1u}};
        require(inherited_scope && inherited_scope.publish(1u),"real inherited native root scope");
        auto const* inherited{gc::current_root_frames()};
        lib::llvm_jit_checkpoint_prepare_request request{};request.recording_label[0u]=::std::byte{0x72u};
        request.graph_budget.max_file_bytes=2u*1024u*1024u;
        auto prepared=lib::llvm_jit_checkpoint_prepare_instance_host_api(stopped,cohort,request);
        ::fast_io::print(::fast_io::out(),"WORLD_GENERATION2_PREPARATION status=",::fast_io::mnp::dec(static_cast<unsigned>(prepared.status)),
            " resource=",::fast_io::mnp::dec(prepared.resource_diagnostic)," engine=",::fast_io::mnp::dec(prepared.engine_diagnostic),
            " functions=",::fast_io::mnp::dec(prepared.functions),
            " threads=",::fast_io::mnp::dec(prepared.prepared_threads)," frames=",::fast_io::mnp::dec(prepared.prepared_frames),
            " roots=",::fast_io::mnp::dec(prepared.prepared_root_carriers)," payload=",::fast_io::mnp::dec(prepared.native_payload_bytes),
            " root_scopes=",::fast_io::mnp::dec(prepared.validated_private_root_scopes),
            " root_frames=",::fast_io::mnp::dec(prepared.installed_private_root_frames),
            " roots_restored=",prepared.private_root_chain_restored,
            " workers_started=",::fast_io::mnp::dec(prepared.started_private_root_workers),
            " workers_enrolled=",::fast_io::mnp::dec(prepared.enrolled_private_root_workers),
            " workers_joined=",::fast_io::mnp::dec(prepared.joined_private_root_workers),
            " workers_held=",prepared.private_worker_cohort_held,
            " worker_roots_restored=",prepared.private_worker_roots_restored,
                        " debug_prepared=",::fast_io::mnp::dec(prepared.prepared_private_debug_workers),
                        " debug_enrolled=",::fast_io::mnp::dec(prepared.enrolled_private_debug_workers),
                        " debug_held=",prepared.private_debug_cohort_held,
                        " debug_tls_restored=",prepared.private_debug_tls_restored,"\n");
        require(prepared.status==lib::llvm_jit_checkpoint_prepare_status::prepared_and_discarded &&
            prepared.data_error==::uwvm2::uwvm::debugger::checkpoint::error::none &&
            prepared.modules==1u && prepared.engines==1u && prepared.functions!=0u &&
            prepared.prepared_threads==1u && prepared.prepared_frames==2u,
            "fresh resources, engines and both relocated frames use actual committed generation2 bodies and roll back");
        require(prepared.validated_private_root_scopes==1u && prepared.installed_private_root_frames==5u &&
            prepared.private_root_chain_restored && gc::current_root_frames()==inherited,
            "actual candidate root prefix installed and removed without touching inherited TLS");
        require(prepared.started_private_root_workers==1u && prepared.enrolled_private_root_workers==1u &&
            prepared.joined_private_root_workers==1u && prepared.private_worker_cohort_held && prepared.private_worker_roots_restored,
            "actual generation2 nested packet remains rooted on its own native worker until complete cohort release and OS join");
        require(prepared.prepared_private_debug_workers==1u && prepared.enrolled_private_debug_workers==1u &&
            prepared.private_debug_cohort_held && prepared.private_debug_tls_restored,
            "genuine generation2 worker owns distinct healthy empty debug ledgers and a real private participant, then clears TLS before join");
        auto no_worker=request;no_worker.maximum_private_root_workers=0u;
        auto worker_refused=lib::llvm_jit_checkpoint_prepare_instance_host_api(stopped,cohort,no_worker);
        require(worker_refused.status==lib::llvm_jit_checkpoint_prepare_status::worker_root_preparation_declined &&
            worker_refused.started_private_root_workers==0u && worker_refused.enrolled_private_root_workers==0u &&
            worker_refused.joined_private_root_workers==0u && !worker_refused.private_worker_cohort_held &&
            worker_refused.private_root_chain_restored && gc::current_root_frames()==inherited,
            "zero native worker quota refuses before creation and preserves real caller roots");
        require(worker_refused.prepared_private_debug_workers==0u && worker_refused.enrolled_private_debug_workers==0u &&
            !worker_refused.private_debug_cohort_held,"zero worker quota declines before allocating any private debug controller/ledger");
        auto small=request;small.maximum_private_root_frames=4u;
        auto refused=lib::llvm_jit_checkpoint_prepare_instance_host_api(stopped,cohort,small);
        require(refused.status==lib::llvm_jit_checkpoint_prepare_status::root_preparation_declined &&
            refused.prepared_frames==2u && refused.validated_private_root_scopes==0u && !refused.private_root_chain_restored &&
            gc::current_root_frames()==inherited,"bounded complete root preflight declines before native TLS publication");
    };
    prepare_generation2(ticket,owners);

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
    require(original == 0xa5a5a5a5u, "retired original never publishes its normal function result");
    require(lib::llvm_jit_checkpoint_retire_saved_execution_host_api(ticket, owners) ==
        lib::llvm_jit_checkpoint_execution_retirement_status::rejected_current_cohort,
        "old retired pause cannot authorize a second active execution");
    auto const run{[&](::std::size_t function, bool result) -> ::std::uint32_t
    {
        ::std::uint32_t value{}; lib::full_compile_run_config config{}; config.entry_function_index = function;
        if(result)
        {
            config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(value));
            config.entry_abi_buffers.result_bytes = sizeof(value);
        }
        lib::full_compile_and_run_main_module(u8"checkpoint-call-continuation", config); return value;
    }};
    require(run(2u, true) == 1u && run(3u, true) == 1u && run(4u, true) == 1234u && run(7u, true) == 0u,
        "actual original prefixes happen once and no postcapture guest effect runs before retirement");
    run(5u, false); require(run(4u, true) == 4321u, "real guest changes memory after capture");
    require(lib::llvm_jit_checkpoint_continue_saved_leaf_host_api(state->capture, ::std::addressof(output), sizeof(output)) ==
        lib::llvm_jit_checkpoint_continuation_status::requires_complete_call_dispatch && output == 0xdeadbeefu,
        "leaf adapter cannot silently discard real parent continuations");
    auto foreign{lib::llvm_jit_checkpoint_thread_capture_owner{state->capture.get(), [](auto*) noexcept {}}};
    require(lib::llvm_jit_checkpoint_continue_saved_thread_host_api(foreign, ::std::addressof(output), sizeof(output)) ==
        lib::llvm_jit_checkpoint_continuation_status::invalid_capture_owner && output == 0xdeadbeefu,
        "same pointer foreign control block does not mint resume authority");
    require(lib::llvm_jit_checkpoint_continue_saved_thread_host_api(state->capture, ::std::addressof(output), sizeof(output)-1u) ==
        lib::llvm_jit_checkpoint_continuation_status::invalid_result_buffer && output == 0xdeadbeefu,
        "invalid output extent leaves owning result untouched");
    // Start a genuine new admitted native execution from the saved ROOT
    // waiting site. Its FIRST child nop must have both actual parent/child
    // activation + precise-root + fully typed shadow frames when it stops.
    {
        ::std::lock_guard lock{state->mutex};
        state->op_count.store(0u, ::std::memory_order_relaxed);
        state->target_count.store(0u, ::std::memory_order_relaxed);
        state->requested = state->copied = state->finished = false;
        state->capture.reset(); state->ticket = {};
        state->armed.store(true, ::std::memory_order_release);
    }
    ::std::uint32_t resumed_output{0xdeadbeefu};
    auto resumed_status{lib::llvm_jit_checkpoint_continuation_status::allocation_failed};
    ::std::thread resumed{[&]
    {
        resumed_status = lib::llvm_jit_checkpoint_continue_saved_thread_host_api(
            original_capture, ::std::addressof(resumed_output), sizeof(resumed_output));
        ::std::lock_guard lock{state->mutex}; state->finished = true; state->changed.notify_all();
    }};
    domain::pause_ticket next_ticket{};
    lib::llvm_jit_checkpoint_thread_capture_owner next_capture{};
    {
        ::std::unique_lock lock{state->mutex};
        require(state->changed.wait_until(lock, deadline(), [&] { return state->copied || state->finished; }),
            "finite actual recheckpoint inside restored child");
        require(state->copied && !state->finished && state->capture,
            "restored child paused with actual parent still on native/root/shadow chain");
        require(state->recording.native_frames == 2u && state->recording.function_generation == 2u &&
            state->recording.typed_slots == 3u && state->recording.site == 9u,
            "new actual generation2 execution captured both genuine restored frames");
        next_ticket = state->ticket; next_capture = state->capture;
    }
    require(state->control->wait_until_paused(next_ticket, deadline()) == threads::cooperative_pause_result::paused,
        "actual newly admitted restored participant really parked");
    lib::llvm_jit_checkpoint_thread_capture_owner next_owners[1u]{next_capture};
    prepare_generation2(next_ticket,next_owners);
    {
        ::std::lock_guard lock{watchdog_mutex}; drained = false;
    }
    ::std::thread second_watchdog{[&]
    {
        ::std::unique_lock lock{watchdog_mutex};
        require(watchdog_changed.wait_until(lock, deadline(), [&] { return drained; }),
            "finite actual nested resumed cleanup and execution drain");
    }};
    require(lib::llvm_jit_checkpoint_retire_saved_execution_host_api(next_ticket, next_owners) ==
        lib::llvm_jit_checkpoint_execution_retirement_status::execution_retired,
        "private signal unwinds real child through real resumed parent cleanup and drains all old leases");
    { ::std::lock_guard lock{watchdog_mutex}; drained = true; watchdog_changed.notify_all(); }
    second_watchdog.join(); resumed.join();
    require(resumed_status == lib::llvm_jit_checkpoint_continuation_status::execution_retired && resumed_output == 0xdeadbeefu,
        "retired nested resumed entry publishes no caller output");
    require(run(7u, true) == 0u, "neither retired execution performs the saved child's next guest effect");
    output = 0xdeadbeefu;
    require(lib::llvm_jit_checkpoint_continue_saved_thread_host_api(next_capture, ::std::addressof(output), sizeof(output)) ==
        lib::llvm_jit_checkpoint_continuation_status::continued && output == 1190u,
        "fresh canonical capture of resumed two-frame chain resumes again from real ROOT waiting site");
    require(run(2u, true) == 1u && run(3u, true) == 1u && run(4u, true) == 4321u && run(7u, true) == 1u,
        "new nested native continuation preserves both original prefixes and performs next child effect once");
    lib::reset_runtime_state_host_api(); output = 0xdeadbeefu;
    require(lib::llvm_jit_checkpoint_continue_saved_thread_host_api(state->capture, ::std::addressof(output), sizeof(output)) !=
        lib::llvm_jit_checkpoint_continuation_status::continued && output == 0xdeadbeefu,
        "retired code generation cannot execute old continuation");
    ::fast_io::io::println("checkpoint_actual_generation2_nested_continuation: PASS actual restored native parent+child, recheckpoint, double cleanup/drain and second resume; complete_instance_restore=false");
}
