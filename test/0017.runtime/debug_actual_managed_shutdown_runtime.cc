// Real owned source -> initializer -> fused full LLVM compiler -> nested
// guest native loop/host debugger callback -> private cancellation -> ACTUAL
// entry RAII drain. No fake host-only loop or numeric thread ACK substitutes.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/utils/container/string_concat.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace threads = ::uwvm2::utils::thread;
namespace cp = ::uwvm2::runtime::checkpoint;
using domain = threads::cooperative_pause_domain;
using status = lib::llvm_jit_debug_shutdown_status;
static void require(bool valid, char const* message)
{
    if(!valid) { ::fast_io::io::perrln("debug actual managed shutdown: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static auto deadline() { return ::std::chrono::steady_clock::now()+::std::chrono::seconds{15}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    domain::pause_ticket ticket{};
    bool block_host{}, entered{}, release_host{}, parked{}, finished{};
    static void point(void* opaque,::std::uint_least64_t,threads::cooperative_pause_location where) noexcept
    {
        // [actual runtime-retained native observer owner] guest entry is live.
        // [safe] true installed host context; no guest address or checkpoint ID.
        auto& self{*static_cast<observer*>(opaque)};
        if(where.function != 1u) { return; }
        ::std::unique_lock lock{self.mutex};
        if(self.entered) { return; }
        self.entered=true; self.changed.notify_all();
        if(self.block_host)
        {
            // A real callback INSIDE the generated guest safe-point bridge.
            // Cancellation must not throw across this noexcept host frame or
            // claim its execution lease drained before it actually returns.
            require(self.changed.wait_until(lock,deadline(),[&]{return self.release_host;}),"finite actual blocked native observer release");
        }
        else
        {
            self.ticket=self.control->request_pause();require(bool(self.ticket),"real native pause request");
        }
    }
    static void before_park(void* opaque,::std::uint_least64_t,threads::cooperative_pause_location,
        lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};::std::lock_guard lock{self.mutex};
        self.parked=true;self.changed.notify_all();
    }
};
int main(int argc,char** argv)
{
    if(argc!=5) { return 2; }
    auto const strategy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]))};
    auto const purpose{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[3]))};
    auto const scenario{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[4]))};
    require(lib::runtime_debug_shutdown_terminal_cleanup_abi_host_api()==2u,"actual terminal cleanup contract, not old WORK-only quiescence");
    require(strategy=="instruction" || strategy=="unwind","explicit actual frame strategy");
    require(purpose=="none" || purpose=="observe" || purpose=="resumable","explicit actual immutable profile purpose");
    require(scenario=="parked-loop" || scenario=="host-block","explicit real native scenario");
    mode::global_runtime_mode=mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler=mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack=strategy=="instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads=0u;mode::runtime_compile_threads_existed=true;
    mode::global_runtime_llvm_jit_cache_path_mode=mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto const path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    // [old argument owner] no source borrow exists; clear before releasing it.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=nullptr;arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{u8"debug-actual-shutdown",nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())},nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [complete owning arguments] no further growth; borrow only after publish.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=::std::addressof(arguments.back());
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_exceptions=false;features.explicit_enable_exceptions=true;
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name=u8"debug-actual-shutdown";
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true)==static_cast<int>(::uwvm2::uwvm::run::retval::ok),"actual owned source parse and initializer");
    auto state{::std::make_shared<observer>()};state->block_host=scenario=="host-block";
    require(lib::llvm_jit_configure_debug_session_host_api(state->control,{state,observer::point,observer::before_park},lib::llvm_jit_debug_safe_point_granularity::instruction)==lib::llvm_jit_debug_configure_result::ok,"real reserved debugger native context");
    if(purpose!="none")
    {
        auto profile{purpose=="observe" ? cp::compilation_profile::create_for_trusted_observer() : cp::compilation_profile::create_for_trusted_manager()};
        require(bool(profile),"actual selected immutable profile owner");
        if(purpose=="observe")
        {
            require(lib::llvm_jit_configure_checkpoint_recording_host_api(profile)==lib::llvm_jit_debug_configure_result::invalid_context,
                    "observer profile cannot acquire checkpoint restoration authority");
            require(lib::llvm_jit_configure_debug_value_observation_host_api({})==lib::llvm_jit_debug_configure_result::ok,
                    "actual dedicated observation-only configuration");
        }
        else
        { require(lib::llvm_jit_configure_checkpoint_recording_host_api(profile)==lib::llvm_jit_debug_configure_result::ok,
                  "actual resumable immutable profile"); }
    }
    ::uwvm2::runtime::gc::scoped_cli_gc_execution compilation_owner{};
    (void)lib::runtime_gc_prepare_cli_collection_host_api();
    require(lib::llvm_jit_prepare_debug_host_api(),"true one fused validation/translation and native publication");
    ::std::array<::std::byte,4u> output{};output.fill(::std::byte{0xa5u});
    ::std::thread guest{[&]
    {
        ::uwvm2::runtime::gc::scoped_cli_gc_execution actual_execution{};
        lib::full_compile_run_config cfg{};cfg.entry_function_index=0u;
        // [real owner exact result4] remains alive through ACTUAL physical join.
        cfg.entry_abi_buffers.result_buffer=output.data();cfg.entry_abi_buffers.result_bytes=output.size();
        lib::full_compile_and_run_main_module(u8"debug-actual-shutdown",cfg);
        ::std::lock_guard lock{state->mutex};state->finished=true;state->changed.notify_all();
    }};
    {
        ::std::unique_lock lock{state->mutex};
        require(state->changed.wait_until(lock,deadline(),[&]{return state->block_host ? state->entered : state->parked;}),"actual nested guest stop or actual native observer callback");
        require(!state->finished,"guest is still physically executing");
    }
    if(!state->block_host)
    { require(state->control->wait_until_paused(state->ticket,deadline())==threads::cooperative_pause_result::paused,"ONE real complete guest pause"); }
    auto const unrelated{::std::make_shared<domain>(1u)};
    require(lib::runtime_begin_llvm_jit_debug_shutdown_host_api(unrelated).status==status::stale_owner,"unrelated host domain cannot close actual guest");
    auto started{lib::runtime_begin_llvm_jit_debug_shutdown_host_api(state->control)};
    require(started.status==status::started && started.owner,"genuine actual retained maintenance/current native source owner");
    auto const retry{lib::runtime_begin_llvm_jit_debug_shutdown_host_api(state->control)};
    require(retry.status==status::started && retry.owner.get()==started.owner.get() &&
        !retry.owner.owner_before(started.owner) && !started.owner.owner_before(retry.owner),"canonical same-domain retry retains one real transaction");
    status foreign_thread_status{status::started};
    ::std::thread foreign_management{[&]
        { foreign_thread_status=lib::runtime_poll_llvm_jit_debug_shutdown_host_api(started.owner,0u); }};
    foreign_management.join();
    require(foreign_thread_status==status::reentrant,"actual maintenance mutex ownership cannot migrate to another native manager");
    // Public shared_ptr alias with a DIFFERENT control block is not authentic.
    // No supplied pointee is read until the runtime privately matches BOTH.
    lib::llvm_jit_debug_shutdown_request_owner forged{started.owner.get(),[](auto const*) noexcept {}};
    require(lib::runtime_poll_llvm_jit_debug_shutdown_host_api(forged,0u)==status::stale_owner,"forged public control block rejected");
    if(state->block_host)
    {
        require(lib::runtime_poll_llvm_jit_debug_shutdown_host_api(started.owner,50u)==status::pending_execution,"real blocking native callback remains live, not false ACK/drain");
        require(!lib::runtime_release_llvm_jit_debug_shutdown_host_api(started.owner),"cannot release actual code/maintenance while real host callback lives");
        {
            ::std::lock_guard lock{state->mutex};require(!state->finished,"timeout preserved actual worker");
            state->release_host=true;state->changed.notify_all();
        }
    }
    auto outcome{status::pending_execution};auto const stop_deadline{deadline()};
    do { outcome=lib::runtime_poll_llvm_jit_debug_shutdown_host_api(started.owner,50u); }
    while((outcome==status::pending_execution || outcome==status::pending_producers || outcome==status::busy) && ::std::chrono::steady_clock::now()<stop_deadline);
    require(outcome==status::resources_quiescent,"ACTUAL entry TLS/FP/GC/native stack RAII and accepted producer work drained");
    {
        ::std::unique_lock lock{state->mutex};require(state->changed.wait_until(lock,deadline(),[&]{return state->finished;}),"actual guest closure returned");
    }
    // Test-only true join AFTER real work drain, with keeper watchdog. Production
    // CLI uses its separately qualified finite FastIO native physical join.
    guest.join();
    for(auto byte:output) { require(byte==::std::byte{0xa5u},"private foreign cancellation bypasses catch_all and publishes no Wasm result"); }
    require(lib::runtime_release_llvm_jit_debug_shutdown_host_api(started.owner),"consume genuine maintenance only after true guest join/work drain");
    require(lib::runtime_poll_llvm_jit_debug_shutdown_host_api(started.owner,0u)==status::stale_owner,"old request loses authority although opaque handle remains owned");
    lib::reset_runtime_state_host_api();
    ::fast_io::io::println("debug_actual_managed_shutdown: PASS actual nested guest cancellation, foreign cleanup, real bounded pending host callback, entry WORK drain; backend_cache_worker_physically_retired=true");
}
