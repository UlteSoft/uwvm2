// Genuine two ordinary LLVM-full guest entries -> two actual before-park
// producers -> current ONE-domain complete census -> immutable input DATA.
// No synthetic participant/capture owner, host wait in observer or restore permission.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/utils/control/owned_file_image.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/runtime/gc/entry_admission.h>
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
#if !defined(UWVM_RUNTIME_LLVM_JIT) || !defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) || !defined(UWVM_CPP_EXCEPTIONS)
# error Actual checkpoint census fixture requires LLVM full/native threads/C++ EH
#endif
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
    ::fast_io::print(::fast_io::err(), "debug_checkpoint_gc_two_participants_runtime FAIL line=", ::fast_io::mnp::dec(line), "\n");
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
            auto candidate{full::full_source_instance::create_unparsed(state.path, u8"checkpoint-census")};
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
            ::uwvm2::uwvm::runtime::initializer::initialize_runtime(true);
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
    void prepare_attempt()
    {
        // Both actual guest threads have joined BEFORE state is reused.
        // No private producer, participant or ticket constructor is used.
        ::std::lock_guard lock{mutex};
        REQUIRE(done == 0u || done == 2u);
        participants = {}; locations = {}; captures = {}; ticket = {};
        request_location = {}; seen = 0u; done = 0u;
    }
    static void point(void* pointer, ::std::uint_least64_t participant,
        threads::cooperative_pause_location location) noexcept
    {
        auto& self{*static_cast<observer*>(pointer)};
        ::std::lock_guard lock{self.mutex};
        REQUIRE(participant != 0u);
        if(self.ticket) { return; }
        for(::std::size_t i{}; i != self.seen; ++i)
        { if(self.participants[i] == participant) { return; } }
        REQUIRE(self.seen < self.participants.size());
        // [two main-owned observation cells] end
        // [safe] seen<2 checked BEFORE index and then bounded increment.
        self.participants[self.seen++] = participant;
        if(self.seen != 2u) { return; }
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
        if(index == 1u) { REQUIRE(location == self.request_location); }
        // Both owners come ONLY from the real generated before-park callback.
        // The actual ticket/control/lease/source/plan checks remain in producer.
        self.locations[index] = location;
        self.captures[index] = lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket);
        self.changed.notify_all();
    }
};
int main(int argc, char** argv)
{
    if(argc != 3) { return 64; }
    auto const policy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]))};
    REQUIRE(policy == "instruction" || policy == "unwind");
    mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack = policy == "instruction" ?
        mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads = 0u; mode::runtime_compile_threads_existed = true;
    mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    source_setup setup{};
    setup.path = ::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])));
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"debug-checkpoint-two-participants", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(setup.path.c_str())}, nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [actual finalized global CLI argument allocation] end
    // [safe] cursor installed AFTER all vector growth, retained through reset.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    auto bytes{image::owned_file_image::read(setup.path, 1048576u)}; REQUIRE(bytes);
    auto const original{bytes.image->bytes()}; REQUIRE(original.size() >= 8u && original.size() <= PTRDIFF_MAX);
    ::std::vector<::std::byte> expected{original.begin(), original.end()}; // SAME bounded actual owner
    setup.immutable = ::std::move(bytes.image);
    REQUIRE(lib::replace_full_source_after_drain_host_api(source_setup::prepare, ::std::addressof(setup)) &&
        setup.ready && setup.source && setup.source->file().has_owned_source_image());
    auto state{::std::make_shared<observer>()};
    auto profile{checkpoint::compilation_profile::create_for_trusted_manager()}; REQUIRE(profile);
    REQUIRE(lib::llvm_jit_configure_debug_session_host_api(state->control,
        {state, observer::point, observer::before_park}, lib::llvm_jit_debug_safe_point_granularity::instruction) ==
        lib::llvm_jit_debug_configure_result::ok);
    REQUIRE(lib::llvm_jit_configure_checkpoint_recording_host_api(profile) == lib::llvm_jit_debug_configure_result::ok);
    REQUIRE(!lib::llvm_jit_checkpoint_capture_thread_host_api({}).capture);
    REQUIRE(lib::llvm_jit_prepare_debug_host_api()); // SAME authoritative fused validation/lowering, once
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 2u> first{};
    domain::pause_ticket first_ticket{};
    threads::cooperative_pause_location first_request{};
    ::std::uint_least64_t epoch{};
    ::std::size_t positives{}, attempts{};
    // Scheduler misses are bounded: no selected owner is fabricated and a
    // one-participant actual census is never counted as a positive.
    for(; attempts != 8u && positives != 2u; ++attempts)
    {
        state->prepare_attempt();
        ::std::barrier start{3};
        ::std::array<::std::uint32_t, 2u> outputs{};
        ::std::array<::std::thread, 2u> guests{};
        for(::std::size_t i{}; i != guests.size(); ++i)
        {
            // [two main-owned independent result cells] end
            // [safe] i<2; each normal guest owns exactly its own result buffer.
            guests[i] = ::std::thread{[&, i]
            {
                start.arrive_and_wait(); // native setup BEFORE any execution lease/guest enrollment
                lib::full_compile_run_config config{}; config.entry_function_index = 0u;
                config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(outputs[i]));
                config.entry_abi_buffers.result_bytes = sizeof(outputs[i]);
                lib::full_compile_and_run_main_module(u8"checkpoint-census", config);
                ::std::lock_guard lock{state->mutex}; ++state->done; state->changed.notify_all();
            }};
        }
        start.arrive_and_wait();
        domain::pause_ticket ticket{};
        {
            ::std::unique_lock lock{state->mutex};
            REQUIRE(state->changed.wait_until(lock, deadline(), [&] { return bool(state->ticket) || state->done == 2u; }));
            ticket = state->ticket;
        }
        if(ticket)
        {
            REQUIRE(state->control->wait_until_paused(ticket, deadline()) == threads::cooperative_pause_result::paused);
            auto const census{state->control->capture(ticket)};
            REQUIRE(census.result == threads::cooperative_pause_result::paused);
            REQUIRE(census.participants.size() <= 2u);
            if(census.participants.size() == 2u)
            {
                ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 2u> captured{};
                threads::cooperative_pause_location request{};
                {
                    ::std::lock_guard lock{state->mutex}; request = state->request_location;
                    REQUIRE(state->seen == 2u && state->participants[0u] != state->participants[1u]);
                    for(::std::size_t i{}; i != captured.size(); ++i)
                    {
                        REQUIRE(state->captures[i].status == lib::llvm_jit_checkpoint_capture_status::captured);
                        captured[i] = state->captures[i].capture; REQUIRE(captured[i]);
                        bool matched{};
                        for(auto const& actual : census.participants)
                        {
                            if(actual.id == state->participants[i] && actual.location == state->locations[i]) { matched = true; }
                        }
                        REQUIRE(matched); // actual domain result matched, never a numeric ID credential
                    }
                }
                {
                    auto real_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()};
                    REQUIRE(real_reader);
                    auto refused{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, captured)};
                    REQUIRE(refused.status == lib::llvm_jit_checkpoint_query_status::gc_admission_denied &&
                        refused.threads.empty() && refused.modules.empty());
                } // actual extra native lease retired BEFORE a true N exclusion
                auto observed{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, captured)};
                REQUIRE(observed.status == lib::llvm_jit_checkpoint_query_status::coherent_typed_data &&
                    observed.threads.size() == 2u && observed.threads[0u].participant != observed.threads[1u].participant);
                REQUIRE(!observed.complete_instance && !observed.executable_restore_available && !observed.snapshot_or_restore_authority());
                REQUIRE(observed.resource_status == lib::llvm_jit_checkpoint_resource_status::immutable_inputs_only &&
                    observed.modules.size() == 1u && observed.modules[0u].original_module == expected &&
                    observed.modules[0u].data.empty() &&
                    observed.modules[0u].declaration_counts == (::std::array<::std::uint64_t, 7u>{1u, 0u, 0u, 0u, 0u, 0u, 0u}));
                for(auto const& thread : observed.threads)
                {
                    REQUIRE(thread.frame_count == 1u && thread.typed_slots >= 2u && thread.typed_slots <= 3u &&
                        thread.initialized_slots == thread.typed_slots && thread.unavailable_slots == 0u);
                }
                auto subset{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, {captured.data(), 1u})};
                REQUIRE(subset.status == lib::llvm_jit_checkpoint_query_status::incomplete_cohort &&
                    subset.threads.empty() && subset.modules.empty());
                ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 2u> duplicate{captured[0u], captured[0u]};
                auto duplicated{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, duplicate)};
                REQUIRE(duplicated.status == lib::llvm_jit_checkpoint_query_status::incomplete_cohort &&
                    duplicated.threads.empty() && duplicated.modules.empty());
                ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 2u> reversed{captured[1u], captured[0u]};
                auto ordered{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, reversed)};
                REQUIRE(ordered.status == lib::llvm_jit_checkpoint_query_status::coherent_typed_data && ordered.threads.size() == 2u);
                if(positives == 0u)
                {
                    first = captured; first_ticket = ticket; first_request = request;
                    epoch = observed.observed_runtime_epoch;
                }
                else
                {
                    // New two actual native entries stop at the SAME first
                    // opcode of THIS initialized function/generation. The old
                    // two owners do not gain permission via identical location.
                    REQUIRE(request == first_request && epoch == observed.observed_runtime_epoch);
                    REQUIRE(captured[0u].get() != first[0u].get() && captured[1u].get() != first[1u].get());
                    auto stale{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, first)};
                    REQUIRE(stale.status == lib::llvm_jit_checkpoint_query_status::stale_episode &&
                        stale.threads.empty() && stale.modules.empty());
                    auto old_ticket{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(first_ticket, captured)};
                    REQUIRE(old_ticket.status != lib::llvm_jit_checkpoint_query_status::coherent_typed_data &&
                        old_ticket.threads.empty() && old_ticket.modules.empty());
                }
                ++positives; // ONLY after real two-participant producer + manager success
            }
            REQUIRE(state->control->resume(ticket));
            auto resumed{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, first)};
            REQUIRE(resumed.status != lib::llvm_jit_checkpoint_query_status::coherent_typed_data &&
                resumed.threads.empty() && resumed.modules.empty());
        }
        for(auto& guest : guests) { guest.join(); }
        REQUIRE(outputs[0u] == 42u && outputs[1u] == 42u);
    }
    REQUIRE(positives == 2u);
    lib::reset_runtime_state_host_api(); REQUIRE(state->control->is_closed());
    auto retired{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(first_ticket, first)};
    REQUIRE(retired.status != lib::llvm_jit_checkpoint_query_status::coherent_typed_data &&
        retired.threads.empty() && retired.modules.empty());
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr;
    ::fast_io::print(::fast_io::out(), "CHECKPOINT_TWO_PARTICIPANTS policy=", policy,
        " actual_census=2 positive_episodes=", ::fast_io::mnp::dec(positives), " attempts=", ::fast_io::mnp::dec(attempts),
        " same_pc_new_episode=1 whole_restore=0\n");
}
