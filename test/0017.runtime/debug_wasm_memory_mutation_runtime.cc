// Genuine setup once, then two LLVM-full guest entries -> actual
// before-park captures -> ONE cohort -> hostclose -> N -> publication -> owned
// real modern GC root state -> one bounded selected linear-memory write.
// No fake capture, copied VIEW write permission or restore authority.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/utils/control/owned_file_image.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/runtime/gc/entry_admission.h>
#include <uwvm2/uwvm/debugger/wasm_state.h>
#include <uwvm2/uwvm/debugger/wasm_mutation.h>
#include <uwvm2/uwvm/debugger/wasm_path.h>
#include <fast_io.h>
#include <fast_io_unit/string.h>
#include <atomic>
#include <algorithm>
#include <array>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <limits>
#include <span>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#if !defined(UWVM_RUNTIME_LLVM_JIT) || !defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) || !defined(UWVM_CPP_EXCEPTIONS)
# error Actual checkpoint census fixture requires LLVM full/native threads/C++ EH
#endif
namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
namespace wm = ::uwvm2::uwvm::debugger::wasm_mutation;
namespace wp = ::uwvm2::uwvm::debugger::wasm_path;
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
    ::fast_io::print(::fast_io::err(), "debug_wasm_memory_mutation_runtime FAIL line=", ::fast_io::mnp::dec(line), "\n");
    ::fast_io::fast_terminate();
}
#define REQUIRE(x) require(bool(x), __LINE__)
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{20}; }
struct source_setup
{
    ::std::u8string path{}, provider_path{};
    image::owned_file_image::owner immutable{}, provider_image{};
    full::full_source_instance::mutable_owner source{};
    bool ready{};
    static bool prepare(void* pointer) noexcept
    {
        // [actual main-owned synchronous drained setup context] owner_end
        // [safe] retained until replace_full_source_after_drain callback returns.
        auto& state{*static_cast<source_setup*>(pointer)};
        try
        {
            ::std::vector<full::full_preload_input> inputs{};
            full::full_preload_input preload{}; preload.file_name = state.provider_path;
            preload.module_name = u8"memory-state-provider"; preload.parameters = ::uwvm2::uwvm::wasm::storage::wasm_parameter;
            preload.image = ::std::move(state.provider_image); inputs.push_back(::std::move(preload));
            auto candidate{full::full_source_instance::create_unparsed(state.path, u8"memory-state-main", ::std::move(inputs))};
            if(!full::select_unparsed_full_source_after_drain(candidate)) { return false; }
            // SAME exclusive source image is adopted BEFORE magic/section parse,
            // not copied from a mutable file mapping after compiler validation.
            auto const loaded{::uwvm2::uwvm::wasm::loader::load_wasm_file(candidate->file_for_native_initialization(),
                candidate->owned_file_name(), candidate->owned_rename(), ::uwvm2::uwvm::wasm::storage::wasm_parameter,
                ::std::move(state.immutable))};
            if(loaded != ::uwvm2::uwvm::wasm::loader::load_wasm_file_rtl::ok) { return false; }
            auto& preload_file{candidate->preloaded_files_for_native_initialization().index_unchecked(0u)};
            auto const provider_loaded{::uwvm2::uwvm::wasm::loader::load_wasm_file(preload_file,
                preload_file.file_name, preload_file.module_name, preload_file.wasm_parameter,
                candidate->take_preloaded_image_for_native_initialization(0u))};
            if(provider_loaded != ::uwvm2::uwvm::wasm::loader::load_wasm_file_rtl::ok ||
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
int main(int argc, char** argv)
{
    if(argc != 5 && argc != 6) { return 64; }
    bool const resumable{argc == 6};
    if(resumable) { REQUIRE(::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[5])) == "resumable"); }
    auto const variant{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[4]))};
    REQUIRE(variant == "small" || variant == "large");
    bool const large{variant == "large"};
    // Narrow target-resource skip BEFORE opening or initializing large Wasm.
    // The small fixture remains mandatory on every supported memory backend.
    if(large && (sizeof(::std::size_t) < 8u || !::uwvm2::object::memory::linear::native_memory_t::can_mmap))
    {
        ::fast_io::print(::fast_io::out(), "SKIP ONLY_MEMORY64_GT4G requires64bit sparse-mmap backend; small fixture remains required\n");
        return 77;
    }
    ::std::uint64_t const wide_offset{large ? 4294967304ull : 72ull};
    ::std::uint64_t const wide_end{large ? 4295032832ull : 131072ull};
    auto const policy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[3]))};
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
    features.disable_multi_memory = false; features.explicit_enable_multi_memory = true;
    features.disable_threads = false; features.explicit_enable_threads = true;
    features.disable_table64 = false; features.explicit_enable_table64 = true;
    features.disable_exceptions = false; features.explicit_enable_exceptions = true;
    features.disable_simd = false; features.explicit_enable_simd = true;
    features.disable_bulk_memory = false; features.explicit_enable_bulk_memory = true;
    source_setup setup{};
    setup.path = ::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])));
    setup.provider_path = ::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[2])));
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"debug-wasm-memory-mutation", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(setup.path.c_str())}, nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [actual finalized global CLI argument allocation] end
    // [safe] cursor installed AFTER all vector growth, retained through reset.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    auto bytes{image::owned_file_image::read(setup.path, 1048576u)}; REQUIRE(bytes);
    auto const original{bytes.image->bytes()}; REQUIRE(original.size() >= 8u && original.size() <= PTRDIFF_MAX);
    setup.immutable = ::std::move(bytes.image);
    auto provider_bytes{image::owned_file_image::read(setup.provider_path, 1048576u)}; REQUIRE(provider_bytes);
    setup.provider_image = ::std::move(provider_bytes.image);
    REQUIRE(lib::replace_full_source_after_drain_host_api(source_setup::prepare, ::std::addressof(setup)) &&
        setup.ready && setup.source && setup.source->file().has_owned_source_image());
    auto state{::std::make_shared<observer>()};
    REQUIRE(lib::llvm_jit_configure_debug_session_host_api(state->control,
        {state, observer::point, observer::before_park}, lib::llvm_jit_debug_safe_point_granularity::instruction) ==
        lib::llvm_jit_debug_configure_result::ok);
    // Match -Rdbg: typed observation/mutation, without compiling restore entries.
    if(resumable)
    {
        auto const profile{checkpoint::compilation_profile::create_for_trusted_manager()}; REQUIRE(profile);
        REQUIRE(lib::llvm_jit_configure_checkpoint_recording_host_api(profile) == lib::llvm_jit_debug_configure_result::ok);
    }
    else { REQUIRE(lib::llvm_jit_configure_debug_value_observation_host_api() == lib::llvm_jit_debug_configure_result::ok); }
    REQUIRE(!lib::llvm_jit_checkpoint_capture_thread_host_api({}).capture);
    REQUIRE(lib::llvm_jit_prepare_debug_host_api()); // SAME authoritative fused validation/lowering, once
    // Ordinary setup initializes globals/table/objects. The later guest
    // participants read them; management writes happen only while BOTH genuinely
    // parked participants and every tracked host/GC entry are excluded.
    lib::full_compile_run_config initialize{}; initialize.entry_function_index = 0u;
    auto const main_id{setup.source->assigned_main_module_id()};
    auto const provider_member{setup.source->registry().find(u8"memory-state-provider")};
    REQUIRE(provider_member != setup.source->registry().end());
    auto const provider_id{setup.source->bound_initialized_module_id(::std::addressof(provider_member->second))};
    REQUIRE(main_id < 2u && provider_id < 2u && main_id != provider_id && setup.source->registry().size() == 2u);
    auto const provider_file{setup.source->actual_validated_file(provider_id,
        setup.source->actual_full_validation_epoch(), ::std::addressof(provider_member->second))};
    REQUIRE(provider_file && provider_file != ::std::addressof(setup.source->file()) && provider_file->has_owned_source_image());
    REQUIRE(!setup.source->actual_validated_file(main_id, setup.source->actual_full_validation_epoch(), ::std::addressof(provider_member->second)));
    REQUIRE(!setup.source->actual_validated_file(provider_id, setup.source->actual_full_validation_epoch() + 1u, ::std::addressof(provider_member->second)));
    // Actual provider entry initializes all imported globals/tables/GC/tag state;
    // two later main guest entries READ it without a test-created data race.
    lib::full_compile_and_run_main_module(u8"memory-state-provider", initialize);
    lib::full_compile_and_run_main_module(u8"memory-state-main", initialize); // real main-owned editable GC objects
    auto const main_binding{lib::llvm_jit_debug_bind_source_host_api(main_id)};
    auto const provider_binding{lib::llvm_jit_debug_bind_source_host_api(provider_id)};
    REQUIRE(main_binding && provider_binding && main_binding.get() != provider_binding.get());
    { ::std::lock_guard lock{state->mutex}; state->collecting = true; }
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 2u> first{};
    domain::pause_ticket first_ticket{};
    ::std::uint_least64_t epoch{};
    ::std::size_t positives{}, attempts{};
    // Scheduler misses are bounded: no selected owner is fabricated and a
    // one-participant actual census is never counted as a positive.
    for(; attempts != 8u && positives != 2u; ++attempts)
    {
        state->prepare_attempt();
        // Prior attempt's actual guests and verification entry have returned.
        // Reset with NORMAL real Wasm setup entries, never direct host memory
        // stores or manufactured guards. Every episode must commit anew.
        lib::full_compile_and_run_main_module(u8"memory-state-provider", initialize);
        lib::full_compile_and_run_main_module(u8"memory-state-main", initialize);
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
                lib::full_compile_run_config config{}; config.entry_function_index = 1u;
                config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(outputs[i]));
                config.entry_abi_buffers.result_bytes = sizeof(outputs[i]);
                lib::full_compile_and_run_main_module(u8"memory-state-main", config);
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
                {
                    ::std::lock_guard lock{state->mutex};
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
                ws::request query{ws::selection::globals, state->participants[0u], main_id, 0u, 0u, 0u, 1u};
                auto observed{lib::llvm_jit_debug_query_wasm_state_host_api(ticket, captured, query)};
                REQUIRE(observed.result == ws::status::available && ws::valid(observed) && observed.rows.size() == 1u &&
                    observed.rows.front().data.ref.kind == ws::reference_kind::structure);
                REQUIRE(!observed.snapshot_or_restore_authority());
                auto bytes_edit = [&](::std::uint64_t module, ::std::uint64_t memory, ::std::uint64_t offset)
                {
                    wm::request edit{}; edit.target = wm::destination::memory; edit.source = wm::source_kind::bytes;
                    edit.participant = state->participants[0u]; edit.module = module; edit.index = memory; edit.element = offset;
                    edit.memory_size = 5u;
                    edit.memory_bytes[0u] = ::std::byte{0x01u}; edit.memory_bytes[1u] = ::std::byte{0x23u};
                    edit.memory_bytes[2u] = ::std::byte{0x45u}; edit.memory_bytes[3u] = ::std::byte{0x67u};
                    edit.memory_bytes[4u] = ::std::byte{0x89u}; return edit;
                };
                auto mutate = [&](wm::request const& edit)
                { return lib::llvm_jit_debug_mutate_wasm_state_host_api(ticket, captured, edit); };
                auto negative = [&](wm::request bad)
                {
                    // If a refusal accidentally performs a partial write, the
                    // real resumed guests check all selected bytes and sentinel
                    // edges, including the last in-bounds byte at each end.
                    bad.memory_bytes.fill(::std::byte{});
                    bad.memory_bytes[0u] = ::std::byte{0xf0u};
                    REQUIRE(!mutate(bad).applied);
                };
                // Both module-local aliases resolve to the same actual provider
                // memory. The committed result labels the actual owner index.
                auto alias{bytes_edit(main_id, 0u, 64u)};
                auto alias_reply{mutate(alias)};
                REQUIRE(alias_reply.applied && alias_reply.target == wm::destination::memory &&
                    alias_reply.module == provider_id && alias_reply.index == 0u && alias_reply.element == 64u &&
                    alias_reply.memory_size == 5u && alias_reply.address_bytes == 4u);
                auto direct{bytes_edit(provider_id, 0u, 80u)};
                direct.memory_bytes[0u] = ::std::byte{0xdeu}; direct.memory_bytes[1u] = ::std::byte{0xadu};
                direct.memory_bytes[2u] = ::std::byte{0xbeu}; direct.memory_bytes[3u] = ::std::byte{0xefu};
                direct.memory_bytes[4u] = ::std::byte{0xccu};
                auto direct_reply{mutate(direct)};
                REQUIRE(direct_reply.applied && direct_reply.module == provider_id && direct_reply.index == 0u &&
                    direct_reply.element == 80u && direct_reply.memory_size == 5u && direct_reply.address_bytes == 4u);
                auto shared{bytes_edit(main_id, 1u, 64u)};
                auto shared_reply{mutate(shared)};
                REQUIRE(shared_reply.applied && shared_reply.module == provider_id && shared_reply.index == 1u &&
                    shared_reply.memory_size == 5u && shared_reply.address_bytes == 4u);
                auto local{bytes_edit(main_id, 2u, 64u)};
                auto local_reply{mutate(local)};
                REQUIRE(local_reply.applied && local_reply.module == main_id && local_reply.index == 2u &&
                    local_reply.memory_size == 5u && local_reply.address_bytes == 4u);
                auto wide{bytes_edit(main_id, 3u, wide_offset)};
                auto wide_reply{mutate(wide)};
                REQUIRE(wide_reply.applied && wide_reply.module == main_id && wide_reply.index == 3u &&
                    wide_reply.element == wide_offset && wide_reply.memory_size == 5u && wide_reply.address_bytes == 8u);
                // Canonical guest memory index includes the owner's import prefix2.
                auto full{bytes_edit(main_id, 0u, 256u)}; full.memory_size = 256u;
                for(::std::size_t i{}; i != full.memory_bytes.size(); ++i)
                { full.memory_bytes[i] = static_cast<::std::byte>(i); }
                auto full_reply{mutate(full)};
                REQUIRE(full_reply.applied && full_reply.memory_size == 256u && full_reply.address_bytes == 4u);
                // complete maximal byte transaction, no scalar interpretation
                for(auto const memory : {0u, 1u, 2u, 3u})
                {
                    auto bad{bytes_edit(main_id, memory, memory == 3u ? wide_end : 65536u)};
                    negative(bad); // offset exactly at end, nonzero payload
                    bad.element -= 1u; bad.memory_size = 2u; negative(bad); // first byte in bounds, second outside
                    bad.element = UINT64_MAX; bad.memory_size = 2u; negative(bad); // no offset+size wrap
                }
                auto bad_direct{direct}; bad_direct.element = 65536u; negative(bad_direct);
                bad_direct.element = 65535u; bad_direct.memory_size = 2u; negative(bad_direct);
                auto bad{local}; bad.module = UINT64_MAX; negative(bad);
                bad = local; bad.index = UINT64_MAX; negative(bad);
                bad = local; bad.participant = UINT64_MAX; negative(bad);
                bad = local; bad.participant = 0u; negative(bad);
                bad = local; bad.memory_size = 0u; negative(bad);
                bad = local; bad.memory_size = 257u; negative(bad); // request fixed buffer256 is never overread
                bad = local; bad.memory_bytes.fill(::std::byte{}); bad.memory_bytes[0u] = ::std::byte{0xf0u};
                REQUIRE(!lib::llvm_jit_debug_mutate_wasm_state_host_api({}, captured, bad).applied);
                REQUIRE(!lib::llvm_jit_debug_mutate_wasm_state_host_api(ticket, {captured.data(), 1u}, bad).applied);
                if(positives == 0u)
                {
                    first = captured; first_ticket = ticket; epoch = observed.runtime_epoch;
                }
                else
                {
                    REQUIRE(epoch == observed.runtime_epoch && captured[0u].get() != first[0u].get() &&
                        captured[1u].get() != first[1u].get());
                    REQUIRE(!lib::llvm_jit_debug_mutate_wasm_state_host_api(ticket, first, bad).applied);
                    REQUIRE(!lib::llvm_jit_debug_mutate_wasm_state_host_api(first_ticket, captured, bad).applied);
                }
                // Existing typed-global mutation only arms final guest checks;
                // it does not create a memory capability or make captures true.
                wm::request armed{}; armed.participant = state->participants[0u]; armed.module = main_id; armed.index = 1u;
                armed.source = wm::source_kind::numeric_bits; armed.numeric_kind = ws::value_kind::i32;
                armed.bits[0u] = ::std::byte{1u}; REQUIRE(mutate(armed).applied);
                ++positives; // actual two-participant captures AND all byte transactions succeeded
            }
            REQUIRE(state->control->resume(ticket));
            wm::request retired{}; retired.target = wm::destination::memory; retired.source = wm::source_kind::bytes;
            retired.participant = state->participants[0u]; retired.module = main_id; retired.index = 2u;
            retired.element = 64u; retired.memory_size = 1u; retired.memory_bytes[0u] = ::std::byte{0xf0u};
            REQUIRE(!lib::llvm_jit_debug_mutate_wasm_state_host_api(ticket, first, retired).applied);
        }
        for(auto& guest : guests) { guest.join(); }
        // These values come from actual machine execution AFTER the edit:
        // imported alias, plain/local memory32, shared atomic reads, memory64,
        // maximal256-byte payload and out-of-range sentinels were all consumed.
        REQUIRE(outputs[0u] == 42u && outputs[1u] == 42u);
        // A separate normal real guest read AFTER the late stale request and
        // joins makes sentinel checking independent of the resume/read race.
        { ::std::lock_guard lock{state->mutex}; state->collecting = false; }
        ::std::uint32_t verified{}; lib::full_compile_run_config verify{}; verify.entry_function_index = 1u;
        verify.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(verified));
        verify.entry_abi_buffers.result_bytes = sizeof(verified);
        lib::full_compile_and_run_main_module(u8"memory-state-main", verify); REQUIRE(verified == 42u);
        { ::std::lock_guard lock{state->mutex}; state->collecting = true; }
    }
    REQUIRE(positives == 2u);
    lib::reset_runtime_state_host_api(); REQUIRE(state->control->is_closed());
    wm::request retired{}; retired.target = wm::destination::memory; retired.source = wm::source_kind::bytes;
    retired.participant = 1u; retired.module = main_id; retired.index = 2u; retired.element = 64u;
    retired.memory_size = 1u; retired.memory_bytes[0u] = ::std::byte{0xf0u};
    REQUIRE(!lib::llvm_jit_debug_mutate_wasm_state_host_api(first_ticket, first, retired).applied);
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr;
    ::fast_io::print(::fast_io::out(), "DEBUG_WASM_MEMORY_MUTATION policy=", policy, " variant=", variant,
        " actual_census=2 owned_modules=2 pure_wasm_imported_memory_alias=1 shared_atomic_reads=1 multi_memory=1 memory64=1",
        " gt4g=", large ? "1" : "0", " positive_episodes=", ::fast_io::mnp::dec(positives),
        " attempts=", ::fast_io::mnp::dec(attempts), " max_byte_transaction=256 refusal_sentinels_guest_verified=1",
        " modern_gc_before_park=1 native_memory_pointer_api=0 fake_capture=0 collection_claim=0 whole_restore=0\n");
}
