// Actual setup then two read-only LLVM-full guests. New instance census under
// one real pause/cohort/hostclose/N/publication, not a copied debugger VIEW.
// The const graph is logical DATA; whole-instance restoration is not claimed.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/utils/control/owned_file_image.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/runtime/gc/entry_admission.h>
#include <uwvm2/uwvm/debugger/checkpoint_state.h>
#include <fast_io.h>
#include <fast_io_unit/string.h>
#include <atomic>
#include <array>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
// Derive actual target/backend capabilities in this fixture's own balanced scope.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#if !defined(UWVM_RUNTIME_LLVM_JIT) || !defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) || !defined(UWVM_CPP_EXCEPTIONS)
# error Actual checkpoint census fixture requires LLVM full/native threads/C++ EH
#endif
namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace threads = ::uwvm2::utils::thread;
namespace checkpoint = ::uwvm2::runtime::checkpoint;
namespace image = ::uwvm2::utils::control;
namespace full = ::uwvm2::uwvm::runtime::full;
using domain = threads::cooperative_pause_domain;
static void require(bool valid, unsigned line)
{
    if(valid) { return; }
    ::fast_io::print(::fast_io::err(), "debug_checkpoint_prepared_retirement_runtime FAIL line=", ::fast_io::mnp::dec(line), "\n");
    ::fast_io::fast_terminate();
}
#define REQUIRE(x) require(bool(x), __LINE__)
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct source_setup
{
    ::std::u8string path{};
    image::owned_file_image::owner immutable{};
    full::full_source_instance::mutable_owner source{};
    bool ready{};
    static bool prepare(void* pointer) noexcept
    {
        // [actual main-owned synchronous drained setup context] owner_end
        // [safe] retained until replace_full_source_after_drain callback returns.
        auto& state{*static_cast<source_setup*>(pointer)};
        try
        {
            auto candidate{full::full_source_instance::create_unparsed(state.path, u8"checkpoint-instance")};
            if(!full::select_unparsed_full_source_after_drain(candidate)) { return false; }
            // SAME exclusive source image is adopted BEFORE magic/section parse,
            // not copied from a mutable file mapping after compiler validation.
            auto const loaded{::uwvm2::uwvm::wasm::loader::load_wasm_file(candidate->file_for_native_initialization(),
                candidate->owned_file_name(), candidate->owned_rename(), ::uwvm2::uwvm::wasm::storage::wasm_parameter,
                ::std::move(state.immutable))};
            if(loaded != ::uwvm2::uwvm::wasm::loader::load_wasm_file_rtl::ok ||
               ::uwvm2::uwvm::wasm::loader::construct_all_module_and_check_duplicate_module() !=
                   ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok ||
               ::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles() !=
                   ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok) { return false; }
            // These modules have no start functions. Complete ordinary segment
            // instantiation, including passive expression payloads, BEFORE any
            // setup guest entry or source seal; true would leave it deferred.
            ::uwvm2::uwvm::runtime::initializer::initialize_runtime(false);
            if(!candidate->seal_actual_initializer()) { return false; }
            state.source = ::std::move(candidate); state.ready = true; return true;
        }
        catch(...) { return false; }
    }
};
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(2u)};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    ::std::array<::std::uint_least64_t, 2u> participants{};
    ::std::array<threads::cooperative_pause_location, 2u> locations{};
    ::std::array<lib::llvm_jit_checkpoint_capture_result, 2u> captures{};
    domain::pause_ticket ticket{}; threads::cooperative_pause_location request_location{};
    ::std::size_t seen{}, done{};
    ::std::array<::std::size_t, 2u> points{};
    bool collecting{};
    void prepare_attempt()
    {
        // Both actual guest threads have joined BEFORE state is reused.
        // No private producer, participant or ticket constructor is used.
        ::std::lock_guard lock{mutex};
        REQUIRE(done == 0u || done == 2u);
        participants = {}; locations = {}; captures = {}; ticket = {};
        request_location = {}; seen = 0u; done = 0u; points = {};
    }
    static void point(void* pointer, ::std::uint_least64_t participant,
        threads::cooperative_pause_location location) noexcept
    {
        auto& self{*static_cast<observer*>(pointer)};
        ::std::lock_guard lock{self.mutex};
        REQUIRE(participant != 0u);
        if(!self.collecting || location.function != 1u || self.ticket) { return; }
        ::std::size_t index{};
        for(; index != self.seen; ++index) { if(self.participants[index] == participant) { break; } }
        if(index == self.seen)
        {
            REQUIRE(self.seen < self.participants.size());
            self.participants[self.seen++] = participant;
        }
        ++self.points[index];
        // Each threshold counts genuine original safe-point callbacks, never
        // an invented opcode address/value. The WAT prefix initializes all
        // selected references before the twentieth callback, then runs nops.
        if(self.seen != 2u || self.points[0u] < 20u || self.points[1u] < 20u) { return; }
        // This only requests a pause. Two previously observed IDs alone are
        // NOT a census proof: a first entrant may already have returned.
        self.ticket = self.control->request_pause(); REQUIRE(self.ticket);
        self.request_location = location;
        self.changed.notify_all();
        // No observer waits, yields, reenters or blocks for the other guest.
    }
    static void before_park(void* pointer, ::std::uint_least64_t participant,
        threads::cooperative_pause_location location, lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(pointer)};
        ::std::lock_guard lock{self.mutex}; REQUIRE(self.ticket);
        ::std::size_t index{};
        for(; index != self.seen; ++index)
        { if(self.participants[index] == participant) { break; } }
        REQUIRE(index < self.participants.size() && !self.captures[index].capture);
        // Whichever actual participant requested the pause owns this location.
        // Both locations are independently matched to the actual domain below.
        // Both owners come ONLY from the real generated before-park callback.
        // The actual ticket/control/lease/source/plan checks remain in producer.
        self.locations[index] = location;
        self.captures[index] = lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket);
        self.changed.notify_all();
    }
};


// Genuine host work after the guest entry and genuine OS TLS teardown. Neither
// flag is used by production as authority: its real leases and OS join decide.
struct exit_windows
{
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    ::std::size_t body_waiters{}, tls_waiters{}, tls_finished{};
    bool release_body{};
    ::std::array<bool,2u> release_tls{};
    ::std::size_t tls_entered_mask{};
};
struct tls_teardown
{
    ::std::shared_ptr<exit_windows> windows{};
    ::std::size_t index{};
    ~tls_teardown()
    {
        if(!windows) { return; }
        REQUIRE(index < 2u);
        ::std::unique_lock lock{windows->mutex};
        REQUIRE((windows->tls_entered_mask & (::std::size_t{1u} << index)) == 0u);
        windows->tls_entered_mask |= ::std::size_t{1u} << index;
        ++windows->tls_waiters; windows->changed.notify_all();
        windows->changed.wait(lock,[&] { return windows->release_tls[index]; });
        ++windows->tls_finished; windows->changed.notify_all();
    }
};
struct owned_launch
{
    ::std::shared_ptr<observer> observed{};
    ::std::shared_ptr<exit_windows> windows{};
    ::std::shared_ptr<::std::barrier<>> start{};
    ::std::uint32_t output{0xa5a5a5a5u};
    ::std::size_t index{};
    static void body(void* pointer) noexcept
    {
        auto& self{*static_cast<owned_launch*>(pointer)};
        static thread_local tls_teardown teardown{};
        teardown.windows = self.windows; teardown.index = self.index;
        self.start->arrive_and_wait();
        lib::full_compile_run_config config{}; config.entry_function_index = 1u;
        config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(self.output));
        config.entry_abi_buffers.result_bytes = sizeof(self.output);
        lib::full_compile_and_run_main_module(u8"checkpoint-instance",config);
        { ::std::lock_guard lock{self.observed->mutex}; ++self.observed->done; self.observed->changed.notify_all(); }
        ::std::unique_lock lock{self.windows->mutex}; ++self.windows->body_waiters; self.windows->changed.notify_all();
        self.windows->changed.wait(lock,[&] { return self.windows->release_body; });
    }
};
static bool unexpected_source_callback(void* pointer) noexcept
{
    auto& attempts{*static_cast<::std::size_t*>(pointer)}; ++attempts; return false;
}
static auto short_deadline()
{ return ::std::chrono::steady_clock::now() + ::std::chrono::milliseconds{500}; }
static auto paused_captures(::std::shared_ptr<observer> const& state)
{
    domain::pause_ticket ticket{};
    { ::std::unique_lock lock{state->mutex};
      REQUIRE(state->changed.wait_until(lock,deadline(),[&] { return bool(state->ticket) || state->done == 2u; }));
      ticket = state->ticket; }
    if(!ticket)
    {
        ::std::lock_guard lock{state->mutex};
        ::fast_io::print(::fast_io::err(),"NATIVE_CAPTURE_FINISHED seen=",::fast_io::mnp::dec(state->seen),
            " done=",::fast_io::mnp::dec(state->done)," first=",::fast_io::mnp::dec(state->points[0u]),
            " second=",::fast_io::mnp::dec(state->points[1u]),"\n");
    }
    REQUIRE(ticket);
    auto const stopped{state->control->wait_until_paused(ticket,deadline())};
    if(stopped!=threads::cooperative_pause_result::paused)
    {
        auto const observed{state->control->capture(ticket)};
        ::fast_io::print(::fast_io::err(),"NATIVE_CAPTURE_INCOMPLETE status=",::fast_io::mnp::dec(static_cast<unsigned>(stopped)),
            " roster=",::fast_io::mnp::dec(observed.participants.size()),"\n");
    }
    REQUIRE(stopped==threads::cooperative_pause_result::paused);
    auto const roster{state->control->capture(ticket)};
    REQUIRE(roster.result == threads::cooperative_pause_result::paused && roster.participants.size() == 2u);
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner,2u> captured{};
    ::std::lock_guard lock{state->mutex}; REQUIRE(state->seen == 2u);
    for(::std::size_t index{}; index != captured.size(); ++index)
    {
        REQUIRE(state->captures[index].status == lib::llvm_jit_checkpoint_capture_status::captured);
        captured[index] = state->captures[index].capture; REQUIRE(captured[index]);
        bool matched{}; for(auto const& actual : roster.participants)
        { matched |= actual.id == state->participants[index] && actual.location == state->locations[index]; }
        REQUIRE(matched);
    }
    return captured;
}
int main(int argc, char** argv)
{
    if(argc != 3 && argc != 4) { return 64; }
    bool const exercise_indirect{argc==4};
    if(exercise_indirect) { REQUIRE(::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[3]))=="indirect"); }
    auto const policy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]))};
    REQUIRE(policy == "instruction" || policy == "unwind");
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ?
        mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0u; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc = false; features.explicit_enable_gc = true;
    features.disable_function_references = false; features.explicit_enable_function_references = true;
    features.disable_reference_types = false; features.explicit_enable_reference_types = true;
    features.disable_memory64 = false; features.explicit_enable_memory64 = true;
    features.disable_table64 = false; features.explicit_enable_table64 = true;
    features.disable_table_instructions = false; features.explicit_enable_table_instructions = true;
    features.disable_multiple_tables = false; features.explicit_enable_multiple_tables = true;
    features.disable_table_initializer = false; features.explicit_enable_table_initializer = true;
    features.disable_exceptions = false; features.explicit_enable_exceptions = true;
    features.disable_simd = false; features.explicit_enable_simd = true;
    features.disable_bulk_memory = false; features.explicit_enable_bulk_memory = true;
    source_setup setup{};
    setup.path = ::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])));
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"debug-checkpoint-complete-instance", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(setup.path.c_str())}, nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [actual finalized global CLI argument allocation] end
    // [safe] cursor installed AFTER all vector growth, retained through reset.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    // Explicit pure Core policy: no visible WASIp1 environment to omit.
    ::uwvm2::uwvm::wasm::storage::local_preload_wasip1=false;
    auto bytes{image::owned_file_image::read(setup.path, 1048576u)}; REQUIRE(bytes);
    auto const original{bytes.image->bytes()}; REQUIRE(original.size() >= 8u && original.size() <= PTRDIFF_MAX);
    setup.immutable = ::std::move(bytes.image);
    REQUIRE(lib::replace_full_source_after_drain_host_api(source_setup::prepare, ::std::addressof(setup)) &&
        setup.ready && setup.source && setup.source->file().has_owned_source_image());
    auto state{::std::make_shared<observer>()};

    auto profile{checkpoint::compilation_profile::create_for_trusted_manager()};
    REQUIRE(profile && profile->purpose() == checkpoint::compilation_purpose::resumable);
    REQUIRE(lib::llvm_jit_configure_debug_session_host_api(state->control,
        {state, observer::point, observer::before_park}, lib::llvm_jit_debug_safe_point_granularity::instruction) ==
        lib::llvm_jit_debug_configure_result::ok);
    REQUIRE(lib::llvm_jit_configure_checkpoint_recording_host_api(profile) == lib::llvm_jit_debug_configure_result::ok);
    REQUIRE(lib::llvm_jit_prepare_debug_host_api());
    lib::full_compile_run_config initialize{}; initialize.entry_function_index = 0u;
    lib::full_compile_and_run_main_module(u8"checkpoint-instance", initialize);
    { ::std::lock_guard lock{state->mutex}; state->collecting = true; }
    using outcome = lib::llvm_jit_checkpoint_native_retirement_status;
    lib::llvm_jit_checkpoint_prepare_request prepare_request{};prepare_request.recording_label[0u]=::std::byte{34u};
    auto begin=[&](auto const& owners,auto limit)
    { return lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(state->ticket,owners,prepare_request,limit); };
    auto const epoch{lib::observe_compiler_runtime_generation_host_api()};
    REQUIRE(epoch != 0u);
    // Actual parked cohort on external std::threads has no native factory
    // owner. Sealing must cancel the unactivated request without guest wake.
    state->prepare_attempt();
    ::std::barrier external_start{3};
    ::std::array<::std::uint32_t,2u> outputs{};
    ::std::array<::std::unique_ptr<::fast_io::native_thread>,2u> external{};
    for(::std::size_t index{}; index != external.size(); ++index)
    {
        external[index].reset(new ::fast_io::native_thread{[&,index]
        {
            external_start.arrive_and_wait(); lib::full_compile_run_config config{}; config.entry_function_index = 1u;
            config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(outputs[index]));
            config.entry_abi_buffers.result_bytes = sizeof(outputs[index]);
            lib::full_compile_and_run_main_module(u8"checkpoint-instance",config);
            ::std::lock_guard lock{state->mutex}; ++state->done; state->changed.notify_all();
        }});
    }
    external_start.arrive_and_wait();
    auto unregistered{paused_captures(state)};
    auto refused{begin(unregistered,deadline())};
    REQUIRE(!refused.candidate_world_retained && refused.preparation.status!=lib::llvm_jit_checkpoint_prepare_status::prepared_and_retained);
    REQUIRE((refused.status == outcome::rejected_current_cohort || refused.status == outcome::preparation_declined) && !refused.operation);
    REQUIRE(state->control->capture(state->ticket).result == threads::cooperative_pause_result::paused);
    REQUIRE(lib::observe_compiler_runtime_generation_host_api() == epoch);
    REQUIRE(state->control->resume(state->ticket));
    for(auto& thread : external) { thread->join(); }
    REQUIRE(outputs[0u] == 42u && outputs[1u] == 42u);
    state->prepare_attempt();
    auto windows{::std::make_shared<exit_windows>()};
    auto start{::std::make_shared<::std::barrier<>>(3)};
    ::std::array<::std::shared_ptr<owned_launch>,2u> launches{};
    ::std::array<lib::llvm_jit_debug_guest_worker_owner,2u> workers{};
    for(::std::size_t index{}; index != workers.size(); ++index)
    {
        launches[index] = ::std::make_shared<owned_launch>();
        launches[index]->observed = state; launches[index]->windows = windows; launches[index]->start = start;
        launches[index]->index = index;
        auto actual{lib::runtime_launch_llvm_jit_debug_guest_worker_host_api(state->control,launches[index],owned_launch::body)};
        REQUIRE(actual.status == lib::llvm_jit_debug_guest_worker_launch_status::started && actual.worker);
        workers[index] = actual.worker;
        REQUIRE(lib::runtime_llvm_jit_debug_guest_worker_matches_control_host_api(workers[index],state->control));
    }
    start->arrive_and_wait();
    auto captured{paused_captures(state)};
    auto reject = [&](auto const& owners)
    {
        auto value{begin(owners,deadline())};
        REQUIRE((value.status == outcome::rejected_current_cohort || value.status == outcome::preparation_declined) && !value.operation);
        REQUIRE(state->control->capture(state->ticket).result == threads::cooperative_pause_result::paused);
    };
    reject(::std::span<lib::llvm_jit_checkpoint_thread_capture_owner const>{captured.data(),1u});
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner,2u> duplicate{captured[0u],captured[0u]}; reject(duplicate);
    lib::llvm_jit_checkpoint_thread_capture_owner address_alias{captured[0u],
        reinterpret_cast<lib::llvm_jit_checkpoint_thread_capture const*>(::std::uintptr_t{1u})};
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner,2u> foreign{address_alias,captured[1u]}; reject(foreign);
    lib::llvm_jit_checkpoint_thread_capture_owner block_alias{captured[0u].get(),[](auto*) noexcept {}};
    foreign = {block_alias,captured[1u]}; reject(foreign);
    { auto reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()}; REQUIRE(reader); reject(captured); }
    auto expired{begin(captured,::std::chrono::steady_clock::now())};
    REQUIRE(expired.status == outcome::invalid_deadline && !expired.operation);
    REQUIRE(lib::llvm_jit_checkpoint_retire_saved_execution_host_api(state->ticket,captured) ==
        lib::llvm_jit_checkpoint_execution_retirement_status::rejected_current_cohort);
    auto invalid{prepare_request};invalid.recording_label={};
    auto denied{lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(state->ticket,captured,invalid,deadline())};
    REQUIRE(denied.status==outcome::preparation_declined && denied.preparation.status==lib::llvm_jit_checkpoint_prepare_status::invalid_request && !denied.operation);
    invalid=prepare_request;invalid.maximum_private_dispatch_bindings=1u;
    denied=lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(state->ticket,captured,invalid,deadline());
    REQUIRE(denied.status==outcome::preparation_declined && denied.preparation.status==lib::llvm_jit_checkpoint_prepare_status::dispatch_binding_preparation_declined && !denied.operation && !denied.candidate_world_retained);
    REQUIRE(state->control->capture(state->ticket).result==threads::cooperative_pause_result::paused && lib::observe_compiler_runtime_generation_host_api()==epoch);
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
    invalid=prepare_request;invalid.maximum_private_native_endpoint_functions=1u;
    denied=lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(state->ticket,captured,invalid,deadline());
    REQUIRE(denied.status==outcome::preparation_declined && denied.preparation.status==lib::llvm_jit_checkpoint_prepare_status::engine_preparation_declined && !denied.operation && !denied.candidate_world_retained);
    REQUIRE(!denied.preparation.runtime_native_endpoint_capture_prepared && state->control->capture(state->ticket).result==threads::cooperative_pause_result::paused && lib::observe_compiler_runtime_generation_host_api()==epoch);
#endif
    invalid=prepare_request;invalid.maximum_private_indirect_bindings=0u;
    denied=lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(state->ticket,captured,invalid,deadline());
    REQUIRE(denied.status==outcome::preparation_declined && denied.preparation.status==lib::llvm_jit_checkpoint_prepare_status::indirect_binding_preparation_declined && !denied.operation && !denied.candidate_world_retained && !denied.preparation.runtime_indirect_bindings_prepared);
    REQUIRE(state->control->capture(state->ticket).result==threads::cooperative_pause_result::paused && lib::observe_compiler_runtime_generation_host_api()==epoch);
    invalid=prepare_request;invalid.maximum_private_root_workers=0u;
    denied=lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(state->ticket,captured,invalid,deadline());
    ::fast_io::print(::fast_io::err(),"PREPARE_PREFLIGHT denied status=",::fast_io::mnp::dec(static_cast<unsigned>(denied.status)),
        " prepare=",::fast_io::mnp::dec(static_cast<unsigned>(denied.preparation.status))," cohort=",::fast_io::mnp::dec(denied.cohort_diagnostic),
        " data=",::fast_io::mnp::dec(static_cast<unsigned>(denied.preparation.data_error))," resource=",::fast_io::mnp::dec(denied.preparation.resource_diagnostic),
        " engine=",::fast_io::mnp::dec(denied.preparation.engine_diagnostic)," frame=",::fast_io::mnp::dec(denied.preparation.frame_diagnostic),"\n");
    REQUIRE(denied.status==outcome::preparation_declined && denied.preparation.status==lib::llvm_jit_checkpoint_prepare_status::worker_root_preparation_declined && !denied.operation && !denied.candidate_world_retained);
    REQUIRE(state->control->capture(state->ticket).result==threads::cooperative_pause_result::paused && lib::observe_compiler_runtime_generation_host_api()==epoch);
    auto pending{begin(captured,short_deadline())};
    ::fast_io::print(::fast_io::out(),"NATIVE_COHORT stage=execution status=",::fast_io::mnp::dec(static_cast<unsigned>(pending.status)),
        " cohort=",::fast_io::mnp::dec(pending.cohort_diagnostic),"\n");
    REQUIRE(pending.status == outcome::pending_execution && pending.operation && pending.workers == 2u &&
        pending.runtime_epoch == epoch && !pending.execution_drained && !pending.native_workers_joined);
    REQUIRE(pending.candidate_world_retained && pending.preparation.status==lib::llvm_jit_checkpoint_prepare_status::prepared_and_retained &&
        pending.preparation.data_error==::uwvm2::uwvm::debugger::checkpoint::error::none &&
        pending.preparation.modules==1u && pending.preparation.engines==1u && pending.preparation.prepared_threads==2u &&
        pending.preparation.started_private_root_workers==2u && pending.preparation.joined_private_root_workers==2u);
    REQUIRE(pending.preparation.runtime_dispatch_bindings_prepared &&
        pending.preparation.prepared_runtime_defined_bindings==pending.preparation.functions &&
        pending.preparation.prepared_runtime_defined_bindings==3u && pending.preparation.prepared_runtime_import_bindings==0u &&
        pending.preparation.prepared_runtime_defined_pointer_ranges==1u);
    REQUIRE(pending.preparation.runtime_indirect_bindings_prepared && pending.preparation.prepared_runtime_type_bindings>0u);
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
    REQUIRE(pending.preparation.runtime_native_endpoint_capture_prepared && pending.preparation.prepared_runtime_native_endpoint_functions==pending.preparation.functions*2u &&
        pending.preparation.prepared_runtime_native_helper_bodies>0u && pending.preparation.prepared_runtime_native_symbol_claims>=pending.preparation.prepared_runtime_native_endpoint_functions);
#endif
    if(exercise_indirect)
    {
        REQUIRE(pending.preparation.prepared_runtime_table_views==3u && pending.preparation.prepared_runtime_function_table_views==2u &&
            pending.preparation.prepared_runtime_indirect_targets==3u && pending.preparation.prepared_runtime_indirect_defined_targets==2u &&
            pending.preparation.prepared_runtime_indirect_import_targets==0u && pending.preparation.prepared_runtime_indirect_null_targets==1u &&
            pending.preparation.prepared_runtime_indirect_incompatible_targets==0u);
    }
    auto operation{pending.operation};
    { ::std::unique_lock lock{windows->mutex};
      auto const bodies_ready{windows->changed.wait_until(lock,deadline(),[&] { return windows->body_waiters == 2u; })};
      if(!bodies_ready) { ::fast_io::print(::fast_io::err(),"NATIVE_BODY_TIMEOUT waiters=",::fast_io::mnp::dec(windows->body_waiters),
          " tls_waiters=",::fast_io::mnp::dec(windows->tls_waiters)," tls_finished=",::fast_io::mnp::dec(windows->tls_finished),"\n"); }
      REQUIRE(bodies_ready);
      REQUIRE(windows->tls_waiters == 0u && windows->tls_finished == 0u); }
    auto blocked_launch = [&]
    {
        auto blocked{lib::runtime_launch_llvm_jit_debug_guest_worker_host_api(state->control,launches[0u],owned_launch::body)};
        REQUIRE(blocked.status == lib::llvm_jit_debug_guest_worker_launch_status::admission_closed && !blocked.worker);
    };
    auto legacy_refused = [&]
    {
        ::std::size_t callback_attempts{};
        lib::runtime_request_execution_stop_host_api();
        lib::runtime_stop_and_drain_host_api();
        lib::reset_runtime_state_host_api();
        REQUIRE(!lib::replace_full_source_after_drain_host_api(unexpected_source_callback,::std::addressof(callback_attempts)));
        REQUIRE(callback_attempts == 0u && lib::observe_compiler_runtime_generation_host_api() == epoch &&
            !state->control->is_closed());
        blocked_launch();
    };
    legacy_refused(); // actual whole-body execution leases remain active
    auto busy{begin(captured,deadline())};
    REQUIRE(busy.status == outcome::busy && busy.operation == operation);
    lib::llvm_jit_checkpoint_native_retirement_owner wrong_address{operation,
        reinterpret_cast<lib::llvm_jit_checkpoint_native_retirement const*>(::std::uintptr_t{1u})};
    lib::llvm_jit_checkpoint_native_retirement_owner wrong_block{operation.get(),[](auto*) noexcept {}};
    REQUIRE(lib::llvm_jit_checkpoint_continue_native_retirement_host_api(wrong_address,deadline()).status == outcome::stale_owner);
    REQUIRE(lib::llvm_jit_checkpoint_continue_native_retirement_host_api(wrong_block,deadline()).status == outcome::stale_owner);
    REQUIRE(lib::llvm_jit_checkpoint_continue_native_retirement_host_api(
        operation,::std::chrono::steady_clock::now()).status == outcome::invalid_deadline);
    { ::std::lock_guard lock{windows->mutex}; windows->release_body = true; windows->changed.notify_all(); }
    { ::std::unique_lock lock{windows->mutex};
      // Windows serializes thread-detach callbacks under the loader lock.
      // One blocked TLS callback can keep its peer from entering TLS cleanup.
      auto const entered{windows->changed.wait_until(lock,deadline(),[&] { return windows->tls_waiters != 0u; })};
      if(!entered) { ::fast_io::print(::fast_io::err(),"NATIVE_TLS_TIMEOUT entered=",::fast_io::mnp::dec(windows->tls_waiters),
          " finished=",::fast_io::mnp::dec(windows->tls_finished),"\n"); }
      REQUIRE(entered && windows->tls_entered_mask != 0u && windows->tls_waiters <= 2u);
      REQUIRE(windows->tls_finished == 0u); }
    auto tls_pending{lib::llvm_jit_checkpoint_continue_native_retirement_host_api(operation,short_deadline())};
    ::fast_io::print(::fast_io::out(),"NATIVE_COHORT stage=tls status=",::fast_io::mnp::dec(static_cast<unsigned>(tls_pending.status)),"\n");
    REQUIRE(tls_pending.status == outcome::pending_native_join && tls_pending.operation == operation &&
        tls_pending.execution_drained && !tls_pending.native_workers_joined && tls_pending.workers == 2u);
    REQUIRE(tls_pending.candidate_world_retained && tls_pending.preparation.native_payload_bytes==pending.preparation.native_payload_bytes);
    auto cannot_discard{lib::llvm_jit_checkpoint_discard_prepared_instance_host_api(operation,short_deadline())};
    REQUIRE(cannot_discard.status==outcome::pending_native_join && cannot_discard.candidate_world_retained && !cannot_discard.native_workers_joined);
    legacy_refused(); // execution drained, actual TLS destructors unfinished
    REQUIRE(lib::observe_compiler_runtime_generation_host_api() == epoch);
    auto repeated{lib::llvm_jit_checkpoint_continue_native_retirement_host_api(operation,short_deadline())};
    REQUIRE(repeated.status == outcome::pending_native_join && repeated.operation == operation && !repeated.native_workers_joined);
    // Release one real TLS destructor, then require its peer to enter and
    // remain blocked. This exercises both physical owners without requiring
    // concurrent Windows loader callbacks, and proves partial exit is insufficient.
    ::std::size_t first_tls{};
    { ::std::lock_guard lock{windows->mutex};
      first_tls = (windows->tls_entered_mask & 1u) != 0u ? 0u : 1u;
      REQUIRE(windows->tls_entered_mask != 0u);
      windows->release_tls[first_tls] = true; windows->changed.notify_all(); }
    { ::std::unique_lock lock{windows->mutex};
      REQUIRE(windows->changed.wait_until(lock,deadline(),[&] { return windows->tls_waiters == 2u && windows->tls_finished == 1u; }));
      REQUIRE(windows->tls_entered_mask == 3u && !windows->release_tls[1u-first_tls]); }
    auto partial_join{lib::llvm_jit_checkpoint_continue_native_retirement_host_api(operation,short_deadline())};
    REQUIRE(partial_join.status == outcome::pending_native_join && partial_join.execution_drained && !partial_join.native_workers_joined);
    legacy_refused();
    REQUIRE(partial_join.candidate_world_retained);
    { ::std::lock_guard lock{windows->mutex}; windows->release_tls[1u-first_tls] = true; windows->changed.notify_all(); }
    auto joined{lib::llvm_jit_checkpoint_continue_native_retirement_host_api(operation,deadline())};
    ::fast_io::print(::fast_io::out(),"NATIVE_COHORT stage=join status=",::fast_io::mnp::dec(static_cast<unsigned>(joined.status)),"\n");
    REQUIRE(joined.status==outcome::prepared_world_ready_closed && joined.operation==operation &&
        joined.execution_drained && joined.native_workers_joined && joined.candidate_world_retained && joined.workers==2u);
    legacy_refused(); // physical join alone must NOT reopen the retained original world.
    REQUIRE(lib::llvm_jit_checkpoint_discard_prepared_instance_host_api(wrong_address,deadline()).status==outcome::stale_owner);
    REQUIRE(lib::llvm_jit_checkpoint_discard_prepared_instance_host_api(wrong_block,deadline()).status==outcome::stale_owner);
    auto ready_again{lib::llvm_jit_checkpoint_continue_native_retirement_host_api(operation,deadline())};
    REQUIRE(ready_again.status==outcome::prepared_world_ready_closed && ready_again.candidate_world_retained);
    auto discarded{lib::llvm_jit_checkpoint_discard_prepared_instance_host_api(operation,deadline())};
    REQUIRE(discarded.status==outcome::retired_and_joined && discarded.all_frames_signalled && !discarded.candidate_world_retained &&
        discarded.preparation.status==lib::llvm_jit_checkpoint_prepare_status::prepared_and_discarded);
    REQUIRE(lib::observe_compiler_runtime_generation_host_api() == epoch);
    { ::std::lock_guard lock{windows->mutex}; REQUIRE(windows->tls_finished == 2u); }
    for(::std::size_t index{}; index != workers.size(); ++index)
    {
        REQUIRE(launches[index]->output == 0xa5a5a5a5u);
        auto physically_joined{lib::runtime_join_llvm_jit_debug_guest_worker_until_host_api(workers[index],deadline())};
        REQUIRE(physically_joined.actual.status == ::fast_io::thread_join_status::joined);
    }
    auto again{lib::llvm_jit_checkpoint_continue_native_retirement_host_api(operation,deadline())};
    REQUIRE(again.status == outcome::retired_and_joined && again.native_workers_joined && again.operation == operation);
    REQUIRE(lib::llvm_jit_checkpoint_continue_native_retirement_host_api(wrong_address,deadline()).status == outcome::stale_owner);
    { ::std::lock_guard lock{state->mutex}; state->collecting = false; }
    ::std::uint32_t after{};
    lib::full_compile_run_config ordinary{}; ordinary.entry_function_index = 1u;
    ordinary.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(after));
    ordinary.entry_abi_buffers.result_bytes = sizeof(after);
    lib::full_compile_and_run_main_module(u8"checkpoint-instance",ordinary); REQUIRE(after == 42u);
    REQUIRE(state->control->capture(state->ticket).result != threads::cooperative_pause_result::paused);
    lib::reset_runtime_state_host_api();
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr;
    ::fast_io::print(::fast_io::out(),"CHECKPOINT_PREPARED_RETIREMENT policy=",policy,
        " actual_workers=2 execution_pending=1 tls_pending=2 partial_tls_exit_refused=1 physical_join=1 admission_closed_until_join=1",
        " unregistered_refused=1 alias_refused=1 aggregate_live_cleanup=1 unchanged_outputs=1 same_epoch=1",
        " legacy_reset_stop_source_refused=3 prepared_before_retire=1 retained_until_join=1 closed_after_join=1 explicit_discard=1 retained_instance=42 whole_restore=0 PASS\n");
}
#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <uwvm2/utils/macro/pop_macros.h>
