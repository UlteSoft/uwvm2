// REAL current capture -> private retired-stack drain -> actual engine-resolved
// saved parent/child execution -> new real pause -> managed foreign cancellation.
// This is execution continuity only, not fresh-world or FILE restore authority.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
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
namespace cp = ::uwvm2::runtime::checkpoint;
using domain = threads::cooperative_pause_domain;
static void require(bool condition,char const* message)
{
    if(!condition) { ::fast_io::io::perrln("debug actual resumed shutdown: ",::fast_io::mnp::os_c_str(message));::fast_io::fast_terminate(); }
}
static auto deadline() { return ::std::chrono::steady_clock::now()+::std::chrono::seconds{20}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::mutex mutex{};::std::condition_variable changed{};
    domain::pause_ticket ticket{};
    lib::llvm_jit_checkpoint_thread_capture_owner capture{};
    unsigned phase{},initial_child_points{};
    bool requested{},parked{},finished{};
    static void point(void* opaque,::std::uint_least64_t,threads::cooperative_pause_location where) noexcept
    {
        // [actual installed strong native context] remains owned through BOTH
        // original and new canonical resumed entries; no guest/saved pointer.
        auto& self{*static_cast<observer*>(opaque)};if(where.function!=1u){return;}
        ::std::lock_guard lock{self.mutex};if(self.requested){return;}
        if(self.phase==0u && self.initial_child_points++!=3u){return;}
        self.ticket=self.control->request_pause();require(bool(self.ticket),"actual current pause request");
        self.requested=true;self.changed.notify_all();
    }
    static void before_park(void* opaque,::std::uint_least64_t,threads::cooperative_pause_location,
        lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};::std::lock_guard lock{self.mutex};
        require(self.requested && !self.parked,"one genuine episode");
        if(self.phase==0u)
        {
            auto actual{lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket)};
            require(actual.status==lib::llvm_jit_checkpoint_capture_status::captured && actual.capture,
                "real canonical initialized i31 child plus waiting parent typed capture");
            self.capture=::std::move(actual.capture);
        }
        self.parked=true;self.changed.notify_all();
    }
};
int main(int argc,char** argv)
{
    if(argc!=3){return 2;}
    auto strategy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]))};
    require(strategy=="instruction" || strategy=="unwind","actual stack strategy");
    require(lib::runtime_debug_shutdown_terminal_cleanup_abi_host_api()==2u,"real terminal physical cleanup ABI");
    mode::global_runtime_mode=mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler=mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack=strategy=="instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads=0u;mode::runtime_compile_threads_existed=true;
    mode::global_runtime_llvm_jit_cache_path_mode=mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    // [old owning argument storage] no source borrow yet.
    // [safe] retire its cursor BEFORE clear/reallocation.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=nullptr;arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{u8"debug-actual-resumed-shutdown",nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())},nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [complete real argument owner] no subsequent growth; last slot is bounded.
    // [safe] form actual cursor only AFTER both emplacements.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=::std::addressof(arguments.back());
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc=false;features.explicit_enable_gc=true;
    features.disable_function_references=false;features.explicit_enable_function_references=true;
    features.disable_reference_types=false;features.explicit_enable_reference_types=true;
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name=u8"debug-actual-resumed-shutdown";
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true)==static_cast<int>(::uwvm2::uwvm::run::retval::ok),"actual owned Core3 parse and initializer");
    auto state{::std::make_shared<observer>()};
    require(lib::llvm_jit_configure_debug_session_host_api(state->control,{state,observer::point,observer::before_park},lib::llvm_jit_debug_safe_point_granularity::instruction)==lib::llvm_jit_debug_configure_result::ok,"genuine debugger context");
    auto profile{cp::compilation_profile::create_for_trusted_manager()};
    require(profile && lib::llvm_jit_configure_checkpoint_recording_host_api(profile)==lib::llvm_jit_debug_configure_result::ok,"true immutable resumable profile");
    ::uwvm2::runtime::gc::scoped_cli_gc_execution compilation{};(void)lib::runtime_gc_prepare_cli_collection_host_api();
    require(lib::llvm_jit_prepare_debug_host_api(),"single fused actual compiler and ALL native resolution");
    ::std::array<::std::byte,4u> original{};original.fill(::std::byte{0xa5u});
    ::std::thread first{[&]
    {
        ::uwvm2::runtime::gc::scoped_cli_gc_execution actual{};
        lib::full_compile_run_config cfg{};cfg.entry_function_index=0u;
        cfg.entry_abi_buffers.result_buffer=original.data();cfg.entry_abi_buffers.result_bytes=original.size();
        lib::full_compile_and_run_main_module(u8"debug-actual-resumed-shutdown",cfg);
        ::std::lock_guard lock{state->mutex};state->finished=true;state->changed.notify_all();
    }};
    {
        ::std::unique_lock lock{state->mutex};
        require(state->changed.wait_until(lock,deadline(),[&]{return state->parked || state->finished;}),"actual original park");
        require(state->parked && !state->finished && state->capture,"genuine original typed two-frame capture");
    }
    require(state->control->wait_until_paused(state->ticket,deadline())==threads::cooperative_pause_result::paused,"ONE complete actual original cohort");
    lib::llvm_jit_checkpoint_thread_capture_owner captures[1u]{state->capture};
    // The keeper's finite watchdog covers this ACTUAL retirement API, not a fake
    // boolean ACK. Success requires original execution-domain lease/RAII drain.
    require(lib::llvm_jit_checkpoint_retire_saved_execution_host_api(state->ticket,captures)==lib::llvm_jit_checkpoint_execution_retirement_status::execution_retired,"actual original cleanup and execution drain");
    first.join();for(auto b:original){require(b==::std::byte{0xa5u},"old native execution publishes no result");}
    {
        ::std::lock_guard lock{state->mutex};state->phase=1u;state->requested=false;state->parked=false;state->finished=false;state->ticket={};
    }
    ::std::array<::std::byte,4u> output{};output.fill(::std::byte{0x5au});
    auto resumed{lib::llvm_jit_checkpoint_continuation_status::invalid_capture_owner};
    ::std::thread second{[&]
    {
        resumed=lib::llvm_jit_checkpoint_continue_saved_thread_host_api(state->capture,output.data(),output.size());
        ::std::lock_guard lock{state->mutex};state->finished=true;state->changed.notify_all();
    }};
    {
        ::std::unique_lock lock{state->mutex};
        require(state->changed.wait_until(lock,deadline(),[&]{return state->parked || state->finished;}),"new actual native continuation park");
        require(state->parked && !state->finished,"resumed native execution physically remains live");
    }
    require(state->control->wait_until_paused(state->ticket,deadline())==threads::cooperative_pause_result::paused,"new genuine complete resumed cohort");
    auto stop{lib::runtime_begin_llvm_jit_debug_shutdown_host_api(state->control)};
    require(stop.status==lib::llvm_jit_debug_shutdown_status::started && stop.owner,"real retained native shutdown owner");
    auto status{lib::llvm_jit_debug_shutdown_status::pending_execution};auto limit{deadline()};
    do {status=lib::runtime_poll_llvm_jit_debug_shutdown_host_api(stop.owner,50u);}
    while((status==lib::llvm_jit_debug_shutdown_status::pending_execution || status==lib::llvm_jit_debug_shutdown_status::pending_producers || status==lib::llvm_jit_debug_shutdown_status::busy) && ::std::chrono::steady_clock::now()<limit);
    require(status==lib::llvm_jit_debug_shutdown_status::resources_quiescent,"real resumed TLS/root/activation/entry drain plus actual backend native join");
    {
        ::std::unique_lock lock{state->mutex};require(state->changed.wait_until(lock,deadline(),[&]{return state->finished;}),"actual resumed host boundary returned");
    }
    second.join();require(resumed==lib::llvm_jit_checkpoint_continuation_status::execution_retired,"private resume-boundary catch accepted genuine foreign cleanup signal");
    for(auto b:output){require(b==::std::byte{0x5au},"cancelled continuation does not publish host results");}
    require(lib::runtime_release_llvm_jit_debug_shutdown_host_api(stop.owner),"true maintenance consumed only after actual native worker retired");
    lib::reset_runtime_state_host_api();
    ::fast_io::io::println("debug_actual_resumed_shutdown: PASS actual capture/retire/new-native-parent-child-pause/foreign-cleanup/drain; full_world_restore=false");
}
