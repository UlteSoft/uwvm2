// Actual LLVM-full managed collection with and without selected debug.
// No synthetic root/capture/cohort owner, no collector invoked by a test bool.
#include <uwvm2/uwvm/run/impl.h>
#include <uwvm2/runtime/gc/entry_admission.h>
#include <fast_io.h>
#include <fast_io_unit/string.h>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
// Runtime headers restore macros; this fixture needs its own balanced scope.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#if !defined(UWVM_RUNTIME_LLVM_JIT) || !defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) || !defined(UWVM_CPP_EXCEPTIONS)
# error Genuine managed GC debug fixture requires LLVM full/native threads/C++ EH
#endif
namespace lib = ::uwvm2::runtime::lib;
namespace gc = ::uwvm2::runtime::gc;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace threads = ::uwvm2::utils::thread;
using domain = threads::cooperative_pause_domain;
static void require(bool ok, unsigned line)
{
    if(ok) { return; }
    ::fast_io::io::perrln("debug managed GC runtime FAIL line=", line);
    ::fast_io::fast_terminate();
}
#define REQUIRE(x) require(bool(x), __LINE__)
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{60}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::atomic_size_t phase{};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    ::std::array<domain::pause_ticket, 2u> tickets{};
    ::std::array<lib::llvm_jit_debug_activation_capture_owner, 2u> captures{};
    ::std::size_t requested{}; bool done{};
    static void point(void* pointer, ::std::uint_least64_t, threads::cooperative_pause_location) noexcept
    {
        // [actual retained provider observer] end; no guest pointer or mutation.
        auto& self{*static_cast<observer*>(pointer)};
        auto const phase{self.phase.load(::std::memory_order_relaxed)};
        if(phase >= 2u) { return; }
        if(phase == 1u && lib::runtime_gc_collection_metrics_host_api().attempts == 0u) { return; }
        auto ticket{self.control->request_pause()}; REQUIRE(ticket);
        ::std::lock_guard lock{self.mutex};
        REQUIRE(self.requested == phase && phase < self.tickets.size());
        self.tickets[phase] = ::std::move(ticket); ++self.requested;
        self.phase.store(phase + 1u, ::std::memory_order_relaxed);
        self.changed.notify_all();
    }
    static void before_park(void* pointer, ::std::uint_least64_t participant,
        threads::cooperative_pause_location, lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(pointer)};
        ::std::lock_guard lock{self.mutex};
        REQUIRE(participant && self.requested && self.requested <= self.tickets.size());
        auto const episode{self.requested - 1u};
        self.captures[episode] = lib::llvm_jit_debug_capture_activation_host_api(self.tickets[episode]);
        REQUIRE(self.captures[episode]); // only genuine runtime before-park producer
        self.changed.notify_all();
    }
};
int main(int argc, char** argv)
{
    if(argc != 4) { return 64; }
    auto const policy{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}};
    auto const selection{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[3])}};
    REQUIRE(policy == "instruction" || policy == "unwind");
    REQUIRE(selection == "debug" || selection == "normal");
    bool const debug{selection == "debug"};
    gc::scoped_cli_gc_execution actual_cli{};
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_exception_dispatch = mode::runtime_llvm_jit_exception_dispatch_t::native_unwind;
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ?
        mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0u; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto const owned_path{::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"debug-managed-gc", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(owned_path.c_str())}, nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [actual final global argument allocation] end; no subsequent growth until
    // guest join/reset. The owned UTF8 string retains the borrowed path.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(
        ::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc = false; features.explicit_enable_gc = true;
    features.disable_function_references = false; features.explicit_enable_function_references = true;
    features.disable_reference_types = false; features.explicit_enable_reference_types = true;
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name = u8"debug-managed-gc";
    REQUIRE(::uwvm2::uwvm::run::prepare_owned_full_cli_source(debug) == static_cast<int>(::uwvm2::uwvm::run::retval::ok));
    auto const source{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
    REQUIRE(::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(source) &&
        source->initialized_main_module() && source->initialized_main_module()->type_section_storage.requires_gc);
    REQUIRE(!debug || source->file().has_owned_source_image());
    REQUIRE(lib::runtime_gc_prepare_cli_collection_host_api() == gc::managed_gc_configure_result::requested);
    REQUIRE(lib::runtime_gc_collection_metrics_host_api().precise_roots_requested);
    auto state{::std::make_shared<observer>()};
    if(debug)
    {
        REQUIRE(lib::llvm_jit_configure_debug_session_host_api(state->control,
            {state, observer::point, observer::before_park}, lib::llvm_jit_debug_safe_point_granularity::instruction) ==
            lib::llvm_jit_debug_configure_result::ok);
        REQUIRE(lib::llvm_jit_prepare_debug_host_api()); // roots requested BEFORE same fused compilation
        auto const module{source->bound_initialized_main_module_id()};
        {
            auto actual_exclusion{gc::runtime_gc_entry_admission.try_exclusive(0u)};
            REQUIRE(actual_exclusion);
            REQUIRE(!lib::llvm_jit_debug_bind_source_host_api(module));
        }
        REQUIRE(lib::llvm_jit_debug_bind_source_host_api(module));
    }
    ::std::thread guest{[&]
    {
        gc::scoped_cli_gc_execution actual_guest_cli{};
        lib::full_compile_run_config config{}; config.entry_function_index = 0u;
        // Unchanged ring WAT carries its actual checksum oracle/traps.
        ::uwvm2::uwvm::run::run_full_module_graph(u8"debug-managed-gc", config);
        ::std::lock_guard lock{state->mutex}; state->done = true; state->changed.notify_all();
    }};
    gc::managed_entry_admission::shared_lease reader{};
    lib::llvm_jit_debug_activation_capture_owner previous{};
    if(debug)
    {
        for(::std::size_t episode{}; episode != 2u; ++episode)
        {
            domain::pause_ticket ticket{};
            {
                ::std::unique_lock lock{state->mutex};
                REQUIRE(state->changed.wait_until(lock, deadline(), [&] { return state->done || state->requested > episode; }));
                REQUIRE(!state->done); ticket = state->tickets[episode];
            }
            REQUIRE(state->control->wait_until_paused(ticket, deadline()) == threads::cooperative_pause_result::paused);
            lib::llvm_jit_debug_activation_capture_owner capture{};
            { ::std::lock_guard lock{state->mutex}; capture = state->captures[episode]; }
            REQUIRE(capture);
            if(episode == 0u) { reader = gc::runtime_gc_entry_admission.try_enter(); REQUIRE(reader); }
            lib::llvm_jit_debug_activation_snapshot actual{};
            REQUIRE(lib::llvm_jit_debug_query_activation_host_api(capture, actual) && !actual.frames.empty());
            if(previous)
            {
                lib::llvm_jit_debug_activation_snapshot stale{};
                REQUIRE(!lib::llvm_jit_debug_query_activation_host_api(previous, stale));
            }
            auto const metrics{lib::runtime_gc_collection_metrics_host_api()};
            REQUIRE(!metrics.disabled && metrics.precise_roots_requested);
            if(episode == 1u)
            {
                REQUIRE(metrics.attempts != 0u && metrics.rejected_multiple != 0u && metrics.collections == 0u);
                reader.reset(); // real reader retirement, before guest resumes/next poll
            }
            previous = capture;
            REQUIRE(state->control->resume(ticket));
        }
    }
    {
        ::std::unique_lock lock{state->mutex};
        REQUIRE(state->changed.wait_until(lock, deadline(), [&] { return state->done; }));
    }
    guest.join();
    REQUIRE(!reader && gc::runtime_gc_entry_admission.active_count() == 0u);
    auto const metrics{lib::runtime_gc_collection_metrics_host_api()};
    REQUIRE(metrics.precise_roots_requested && !metrics.disabled && metrics.accounted_allocations == 65536u);
    REQUIRE(metrics.attempts == 16u && metrics.collections != 0u && metrics.reclaimed != 0u &&
        metrics.rejected_heap == 0u && metrics.rejected_population == 0u);
    if(debug) { REQUIRE(metrics.rejected_multiple != 0u); }
    else { REQUIRE(metrics.rejected_multiple == 0u); }
    REQUIRE(metrics.rejection == gc::managed_gc_rejection::none);
    lib::reset_runtime_state_host_api();
    if(debug) { REQUIRE(state->control->is_closed()); }
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr;
    ::fast_io::io::println("debug managed GC runtime: ", policy, " ", selection,
        " allocations=", metrics.accounted_allocations, " collections=", metrics.collections,
        " reclaimed=", metrics.reclaimed, " counted_reader_skips=", metrics.rejected_multiple);
}

#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <uwvm2/utils/macro/pop_macros.h>
