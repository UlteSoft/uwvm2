// Genuine Core3 LLVM-full before-park -> canonical capture -> actual host gate
// -> complete cohort/N/publication -> bounded WASIp1 query/edit -> resume.
// No constructor bypass, fake paused scalar, native FD injection or WASI rewrite.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/uwvm/debugger/wasip1_state.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_renumber.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close.h>
#include <uwvm2/runtime/lib/uwvm_runtime_wasip1_native_file.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_set_flags.h>
#include <fast_io.h>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace threads = ::uwvm2::utils::thread;
namespace ws = ::uwvm2::uwvm::debugger::wasip1_state;
namespace wasi_storage = ::uwvm2::uwvm::imported::wasi::wasip1::storage;
using domain = threads::cooperative_pause_domain;
// Match the ORIGINAL WASIp1 initializer's stdio ownership branch. Native
// io_dup already owns these resources; the capsule retains that genuine RC.
// Only the original fallback observer branch needs a new capsule duplicate.
#if !defined(__AVR__) && !((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) && !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(__NEWLIB__) && !defined(__CYGWIN__)) && !defined(_PICOLIBC__) && !defined(__wasm__)
inline constexpr ::std::size_t expected_stdio_capsule_duplicates{0u};
inline constexpr auto expected_stdio_kind{ws::descriptor_kind::native_file};
#else
inline constexpr ::std::size_t expected_stdio_capsule_duplicates{3u};
inline constexpr auto expected_stdio_kind{ws::descriptor_kind::native_file_observer};
#endif
inline ::std::atomic_uint checks{};
static void require(bool good, char const* message)
{
    ++checks;
    if(!good) { ::fast_io::io::perrln("debug_wasip1_prepared_retirement_runtime: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::atomic_size_t ordinal{};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    domain::pause_ticket ticket{};
    lib::llvm_jit_checkpoint_thread_capture_owner capture{};
    threads::cooperative_pause_location location{};
    bool requested{}, captured{}, finished{};
    static void point(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location where) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};
        bool const first{where.function==7u && self.ordinal.fetch_add(1u,::std::memory_order_relaxed)==3u};
        if(!first) { return; }
        auto ticket{self.control->request_pause()}; require(bool(ticket), "real pause requested");
        ::std::lock_guard lock{self.mutex}; self.ticket = ::std::move(ticket); self.location = where;
        self.requested = true; self.changed.notify_all();
    }
    static void before_park(void* opaque, ::std::uint_least64_t, threads::cooperative_pause_location where, lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};
        ::std::lock_guard lock{self.mutex}; require(self.requested && self.location == where && !self.captured, "actual same before-park episode");
        auto actual{lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket)};
        require(actual.status == lib::llvm_jit_checkpoint_capture_status::captured && actual.capture, "actual typed Core3 capture with GC root");
        self.capture = ::std::move(actual.capture); self.captured = true; self.changed.notify_all();
    }
};
struct exit_windows
{
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    ::std::size_t body_waiters{}, tls_waiters{}, tls_finished{};
    bool release_body{}, release_tls{};
};
struct tls_teardown
{
    ::std::shared_ptr<exit_windows> windows{};
    ~tls_teardown()
    {
        if(!windows) { return; }
        ::std::unique_lock lock{windows->mutex}; ++windows->tls_waiters; windows->changed.notify_all();
        windows->changed.wait(lock,[&] { return windows->release_tls; });
        ++windows->tls_finished; windows->changed.notify_all();
    }
};
struct owned_launch
{
    ::std::shared_ptr<observer> state{};
    ::std::shared_ptr<exit_windows> windows{};
    ::std::uint32_t output{0xa5a5a5a5u};
    static void body(void* opaque) noexcept
    {
        auto& self{*static_cast<owned_launch*>(opaque)};
        static thread_local tls_teardown teardown{};teardown.windows=self.windows;
        lib::full_compile_run_config run{};run.entry_function_index=7u;
        run.entry_abi_buffers.result_buffer=reinterpret_cast<::std::byte*>(::std::addressof(self.output));run.entry_abi_buffers.result_bytes=sizeof(self.output);
        lib::full_compile_and_run_main_module(u8"wasi-debug",run);
        { ::std::lock_guard lock{self.state->mutex};self.state->finished=true;self.state->changed.notify_all(); }
        ::std::unique_lock lock{self.windows->mutex};++self.windows->body_waiters;self.windows->changed.notify_all();
        self.windows->changed.wait(lock,[&] { return self.windows->release_body; });
    }
};
static auto short_deadline() { return ::std::chrono::steady_clock::now()+::std::chrono::milliseconds{500}; }
int main(int argc,char** argv)
{
    if(argc!=3 && argc!=4) { return 64; }
    bool const exercise_indirect{argc==4};
    if(exercise_indirect) { require(::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[3]))=="indirect","indirect fixture selector"); }
    auto const policy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]))};
    require(policy=="instruction" || policy=="unwind","explicit stack policy");
    mode::global_runtime_mode=mode::runtime_mode_t::full_compile;mode::global_runtime_compiler=mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack=policy=="instruction"?mode::runtime_llvm_jit_call_stack_t::instruction:mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads=0u;mode::runtime_compile_threads_existed=true;
    mode::global_runtime_llvm_jit_cache_path_mode=mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto const path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};arguments.clear();::uwvm2::uwvm::cmdline::wasm_file_ppos=nullptr;
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{u8"wasi-debug",nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())},nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=::std::addressof(arguments.back());
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name=u8"wasi-debug";
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc=false;features.explicit_enable_gc=true;features.disable_reference_types=false;features.explicit_enable_reference_types=true;
    features.disable_function_references=false;features.explicit_enable_function_references=true;
    wasi_storage::wasip1_noinherit_system_environment=true;wasi_storage::wasip1_force_args_is_set=true;
    wasi_storage::wasip1_force_argument_storage.emplace_back(u8"argv0");wasi_storage::wasip1_force_argument_storage.emplace_back(u8"retained");
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true)==static_cast<int>(::uwvm2::uwvm::run::retval::ok),"original owned WASI initializer");
    auto state{::std::make_shared<observer>()};
    require(lib::llvm_jit_configure_debug_session_host_api(state->control,{state,observer::point,observer::before_park},lib::llvm_jit_debug_safe_point_granularity::instruction)==lib::llvm_jit_debug_configure_result::ok,"real debug session");
    auto profile{::uwvm2::runtime::checkpoint::compilation_profile::create_for_trusted_manager()};
    require(profile && lib::llvm_jit_configure_checkpoint_recording_host_api(profile)==lib::llvm_jit_debug_configure_result::ok,"actual resumable profile and host gate");
    require(lib::llvm_jit_prepare_debug_host_api(),"actual native full JIT publication");
    ::uwvm2::uwvm::runtime::initializer::apply_runtime_active_segments(u8"wasi-debug");
    auto windows{::std::make_shared<exit_windows>()};auto launch{::std::make_shared<owned_launch>()};launch->state=state;launch->windows=windows;
    auto worker{lib::runtime_launch_llvm_jit_debug_guest_worker_host_api(state->control,launch,owned_launch::body)};
    require(worker.status==lib::llvm_jit_debug_guest_worker_launch_status::started && worker.worker,"genuine registered native guest");
    domain::pause_ticket ticket{};
    { ::std::unique_lock lock{state->mutex};require(state->changed.wait_until(lock,deadline(),[&] { return state->captured || state->finished; }),"real capture arrival");require(state->captured && !state->finished,"guest remains parked");ticket=state->ticket; }
    require(state->control->wait_until_paused(ticket,deadline())==threads::cooperative_pause_result::paused,"actual cohort stopped");
    lib::llvm_jit_checkpoint_thread_capture_owner owners[]{state->capture};
    using outcome=lib::llvm_jit_checkpoint_native_retirement_status;
    auto const epoch{lib::observe_compiler_runtime_generation_host_api()};
    lib::llvm_jit_checkpoint_prepare_request request{};request.recording_label[0u]=::std::byte{34u};
    auto missing{lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(ticket,owners,request,deadline())};
    require(missing.status==outcome::preparation_declined && missing.preparation.wasip1_checkpoint_required && missing.preparation.status==lib::llvm_jit_checkpoint_prepare_status::wasip1_preparation_declined && !missing.operation,"visible WASIp1 cannot be omitted before retirement");
    request.include_wasip1=true;
    auto strict{lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(ticket,owners,request,deadline())};
    require(strict.status==outcome::preparation_declined && strict.preparation.wasip1_status==lib::llvm_jit_wasip1_environment_capsule_status::unsupported_resource && !strict.operation,"strict policy refuses retained original stdio before retirement");
    require(state->control->capture(ticket).result==threads::cooperative_pause_result::paused && lib::observe_compiler_runtime_generation_host_api()==epoch,"preparation failures leave old guest paused and unchanged");
    request.require_managed_wasip1_resources=false;
    auto limited=request;limited.maximum_private_source_initializer_modules=0u;
    auto source_refused{lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(ticket,owners,limited,deadline())};
    require(source_refused.status==outcome::preparation_declined && source_refused.preparation.status==lib::llvm_jit_checkpoint_prepare_status::source_initializer_binding_preparation_declined &&
        !source_refused.operation && !source_refused.candidate_world_retained && !source_refused.preparation.runtime_source_initializer_bindings_prepared && source_refused.preparation.engines==0u,
        "source initializer quota refuses before engine allocation and old WASI retirement");
    require(state->control->capture(state->ticket).result==::uwvm2::utils::thread::cooperative_pause_result::paused && lib::observe_compiler_runtime_generation_host_api()==epoch,"source binding refusal preserves the same pause and runtime epoch");
    limited=request;limited.maximum_private_dispatch_bindings=1u;
    auto bindings_refused=lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(ticket,owners,limited,deadline());
    require(bindings_refused.status==outcome::preparation_declined && bindings_refused.preparation.status==lib::llvm_jit_checkpoint_prepare_status::dispatch_binding_preparation_declined && !bindings_refused.operation,"private dispatch budget refuses before old worker retirement");
    require(state->control->capture(ticket).result==threads::cooperative_pause_result::paused && lib::observe_compiler_runtime_generation_host_api()==epoch,"dispatch failure leaves original WASI guest unchanged");
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
    limited=request;limited.maximum_private_native_endpoint_functions=1u;
    auto endpoint_refused=lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(ticket,owners,limited,deadline());
    require(endpoint_refused.status==outcome::preparation_declined && endpoint_refused.preparation.status==lib::llvm_jit_checkpoint_prepare_status::engine_preparation_declined && !endpoint_refused.operation && !endpoint_refused.candidate_world_retained && !endpoint_refused.preparation.runtime_native_endpoint_capture_prepared,"native endpoint quota refuses before old WASI worker retirement");
    require(state->control->capture(ticket).result==threads::cooperative_pause_result::paused && lib::observe_compiler_runtime_generation_host_api()==epoch,"endpoint failure leaves old WASI guest paused and unchanged");
#endif
    limited=request;limited.maximum_private_indirect_bindings=0u;
    auto indirect_refused=lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(ticket,owners,limited,deadline());
    require(indirect_refused.status==outcome::preparation_declined && indirect_refused.preparation.status==lib::llvm_jit_checkpoint_prepare_status::indirect_binding_preparation_declined && !indirect_refused.operation && !indirect_refused.candidate_world_retained && !indirect_refused.preparation.runtime_indirect_bindings_prepared,"indirect budget refuses before old WASI worker retirement");
    require(state->control->capture(ticket).result==threads::cooperative_pause_result::paused && lib::observe_compiler_runtime_generation_host_api()==epoch,"indirect failure leaves original WASI guest unchanged");
    auto publication_limited=request;publication_limited.maximum_private_full_publication_functions=0u;
    auto publication_refused=lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(ticket,owners,publication_limited,deadline());
    require(publication_refused.status==outcome::preparation_declined && publication_refused.preparation.status==lib::llvm_jit_checkpoint_prepare_status::full_publication_record_preparation_declined &&
        !publication_refused.operation && !publication_refused.candidate_world_retained && !publication_refused.preparation.runtime_full_publication_records_prepared,"publication quota refuses before WASI retirement");
    require(state->control->capture(ticket).result==threads::cooperative_pause_result::paused && lib::observe_compiler_runtime_generation_host_api()==epoch,"publication refusal preserves current stopped WASI world");
    auto pending{lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(ticket,owners,request,short_deadline())};
    ::fast_io::print(::fast_io::out(),"WASIP1_PREPARED_RETIREMENT stage=execution status=",::fast_io::mnp::dec(static_cast<unsigned>(pending.status))," prepare=",::fast_io::mnp::dec(static_cast<unsigned>(pending.preparation.status)),"\n");
    require(pending.status==outcome::pending_execution && pending.operation && pending.candidate_world_retained,"candidate survives pending execution drain");
    require(pending.preparation.status==lib::llvm_jit_checkpoint_prepare_status::prepared_and_retained && pending.preparation.data_error==::uwvm2::uwvm::debugger::checkpoint::error::none && pending.preparation.wasip1_installed_privately && pending.preparation.wasip1_prepared_together && pending.preparation.wasip1_environments==1u && pending.preparation.prepared_wasip1_memories==1u && pending.preparation.joined_private_root_workers==1u,"new WASI memory and complete private candidate retained together");
    require(pending.preparation.runtime_full_publication_records_prepared && pending.preparation.prepared_full_publication_modules==1u &&
        pending.preparation.prepared_full_publication_functions==pending.preparation.functions && !pending.full_publication_records_rechecked_after_physical_join,"private final publication record shape");
    require(pending.preparation.runtime_source_initializer_bindings_prepared &&
        pending.preparation.prepared_source_initializer_modules==pending.preparation.modules && pending.preparation.prepared_source_initializer_preloads==0u &&
        !pending.source_initializer_bindings_rechecked_after_physical_join,"source initializer bindings remain private before physical join");
    require(pending.preparation.runtime_dispatch_bindings_prepared && pending.preparation.prepared_runtime_defined_bindings==pending.preparation.functions && pending.preparation.prepared_runtime_import_bindings>0u && pending.preparation.prepared_runtime_defined_pointer_ranges>0u,"new source owns complete defined and builtin WASI dispatch bindings");
    require(pending.preparation.runtime_indirect_bindings_prepared && pending.preparation.prepared_runtime_type_bindings>0u,"private caller type bindings prepared");
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
    require(pending.preparation.runtime_native_endpoint_capture_prepared && pending.preparation.prepared_runtime_native_endpoint_functions==pending.preparation.functions*2u &&
        pending.preparation.prepared_runtime_native_helper_bodies>0u && pending.preparation.prepared_runtime_native_symbol_claims>=pending.preparation.prepared_runtime_native_endpoint_functions,"actual private typed/resume endpoints and all helper claims retained without a live ASM grant");
#endif
    if(exercise_indirect)
    {
        require(pending.preparation.prepared_runtime_table_views==1u && pending.preparation.prepared_runtime_function_table_views==1u &&
            pending.preparation.prepared_runtime_indirect_targets==3u && pending.preparation.prepared_runtime_indirect_defined_targets==1u &&
            pending.preparation.prepared_runtime_indirect_import_targets==1u && pending.preparation.prepared_runtime_indirect_null_targets==1u &&
            pending.preparation.prepared_runtime_indirect_incompatible_targets==0u,"private WASI table preserves defined, builtin and null target types");
    }
    auto operation{pending.operation};
    { ::std::unique_lock lock{windows->mutex};require(windows->changed.wait_until(lock,deadline(),[&] { return windows->body_waiters==1u; }),"old native body waits outside guest entry");windows->release_body=true;windows->changed.notify_all();require(windows->changed.wait_until(lock,deadline(),[&] { return windows->tls_waiters==1u; }),"real old TLS destructor delays physical death"); }
    auto tls{lib::llvm_jit_checkpoint_continue_native_retirement_host_api(operation,short_deadline())};
    require(tls.status==outcome::pending_native_join && tls.execution_drained && !tls.native_workers_joined && tls.candidate_world_retained,"execution drain cannot replace physical join");
    auto blocked=[&] { auto v{lib::runtime_launch_llvm_jit_debug_guest_worker_host_api(state->control,launch,owned_launch::body)};require(v.status==lib::llvm_jit_debug_guest_worker_launch_status::admission_closed && !v.worker,"ordinary execution remains closed"); };
    blocked();
    auto early{lib::llvm_jit_checkpoint_discard_prepared_instance_host_api(operation,short_deadline())};require(early.status==outcome::pending_native_join && early.candidate_world_retained,"discard cannot bypass unfinished TLS death");
    { ::std::lock_guard lock{windows->mutex};windows->release_tls=true;windows->changed.notify_all(); }
    auto ready{lib::llvm_jit_checkpoint_continue_native_retirement_host_api(operation,deadline())};
    require(ready.status==outcome::prepared_world_ready_closed && ready.native_workers_joined && ready.candidate_world_retained && ready.source_initializer_bindings_rechecked_after_physical_join && ready.full_publication_records_rechecked_after_physical_join,"private candidate ready only after physical join");blocked();
    require(lib::observe_compiler_runtime_generation_host_api()==epoch && launch->output==0xa5a5a5a5u,"no candidate publication or original return output");
    auto discarded{lib::llvm_jit_checkpoint_discard_prepared_instance_host_api(operation,deadline())};
    require(discarded.status==outcome::retired_and_joined && !discarded.candidate_world_retained && discarded.all_frames_signalled,"explicit discard releases private candidate and reopens same instance");
    auto joined{lib::runtime_join_llvm_jit_debug_guest_worker_until_host_api(worker.worker,deadline())};require(joined.actual.status==::fast_io::thread_join_status::joined,"actual OS owner reaped");
    lib::reset_runtime_state_host_api();::uwvm2::uwvm::cmdline::wasm_file_ppos=nullptr;
    ::fast_io::print(::fast_io::out(),"CHECKPOINT_WASIP1_PREPARED_RETIREMENT policy=",policy," checks=",::fast_io::mnp::dec(checks.load())," missing_wasi_refused=1 strict_refused=1 execution_pending=1 tls_pending=1 physical_join=1 closed_after_join=1 explicit_discard=1 whole_restore=0 PASS\n");
}
#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <uwvm2/utils/macro/pop_macros.h>
