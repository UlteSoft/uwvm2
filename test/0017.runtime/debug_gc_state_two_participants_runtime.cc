// Genuine setup once, then two read-only LLVM-full guest entries -> actual
// before-park captures -> ONE cohort -> hostclose -> N -> publication -> owned
// GC/exn/global/table/local/operand VIEW. No fake capture or restore authority.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/utils/control/owned_file_image.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/runtime/gc/entry_admission.h>
#include <uwvm2/uwvm/debugger/wasm_state.h>
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
// Runtime implementation macros are scoped by its headers and then popped.
// Test the public build flag and compiler exception support here; the actual
// thread/capture API and fresh runtime linkage qualify the platform capability.
#if !defined(UWVM_USE_LLVM_JIT) || (!defined(__cpp_exceptions) && !defined(_CPPUNWIND))
# error Actual checkpoint census fixture requires LLVM full/native threads/C++ EH
#endif
namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
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
    ::fast_io::print(::fast_io::err(), "debug_gc_state_two_participants_runtime FAIL line=", ::fast_io::mnp::dec(line), "\n");
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
            auto candidate{full::full_source_instance::create_unparsed(state.path, u8"gc-state")};
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
    ::std::array<::std::size_t, 2u> points{};
    ::std::size_t minimum_points{20u};
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
        if(self.seen != 2u || self.points[0u] < self.minimum_points || self.points[1u] < self.minimum_points) { return; }
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
    if(argc != 3 && argc != 4) { return 64; }
    auto const option{argc == 4 ? ::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[3])} : ::fast_io::cstring_view{}};
    bool const atomic_workers{option == "observe-atomic" || option == "checkpoint-atomic"};
    bool const observe_only{option == "observe" || option == "observe-atomic"};
    if(argc == 4) { REQUIRE(observe_only || atomic_workers); }
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
    features.disable_exceptions = false; features.explicit_enable_exceptions = true;
    features.disable_simd = false; features.explicit_enable_simd = true;
    features.disable_bulk_memory = false; features.explicit_enable_bulk_memory = true;
    if(atomic_workers) { features.disable_threads = false; features.explicit_enable_threads = true; }
    source_setup setup{};
    setup.path = ::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])));
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        u8"debug-gc-state-two-participants", nullptr, ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(setup.path.c_str())}, nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [actual finalized global CLI argument allocation] end
    // [safe] cursor installed AFTER all vector growth, retained through reset.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = ::std::addressof(arguments.back());
    auto bytes{image::owned_file_image::read(setup.path, 1048576u)}; REQUIRE(bytes);
    auto const original{bytes.image->bytes()}; REQUIRE(original.size() >= 8u && original.size() <= PTRDIFF_MAX);
    setup.immutable = ::std::move(bytes.image);
    REQUIRE(lib::replace_full_source_after_drain_host_api(source_setup::prepare, ::std::addressof(setup)) &&
        setup.ready && setup.source && setup.source->file().has_owned_source_image());
    auto state{::std::make_shared<observer>()};
    // The atomic variant initializes one additional original local before the
    // finite nop window. Both thresholds count genuine callbacks per worker.
    if(atomic_workers) { state->minimum_points = 32u; }
    auto profile{checkpoint::compilation_profile::create_for_trusted_manager()}; REQUIRE(profile);
    REQUIRE(lib::llvm_jit_configure_debug_session_host_api(state->control,
        {state, observer::point, observer::before_park}, lib::llvm_jit_debug_safe_point_granularity::instruction) ==
        lib::llvm_jit_debug_configure_result::ok);
    if(observe_only)
    {
        // The normal -Rdbg typed observer must support this real cohort too;
        // checkpoint recording is a separate optional compilation contract.
        REQUIRE(lib::llvm_jit_configure_debug_value_observation_host_api() == lib::llvm_jit_debug_configure_result::ok);
    }
    else { REQUIRE(lib::llvm_jit_configure_checkpoint_recording_host_api(profile) == lib::llvm_jit_debug_configure_result::ok); }
    REQUIRE(!lib::llvm_jit_checkpoint_capture_thread_host_api({}).capture);
    REQUIRE(lib::llvm_jit_prepare_debug_host_api()); // SAME authoritative fused validation/lowering, once
    // Only this ordinary setup entry mutates globals/table/objects. The two
    // later participants read them, avoiding test-created guest data races.
    lib::full_compile_run_config initialize{}; initialize.entry_function_index = 0u;
    lib::full_compile_and_run_main_module(u8"gc-state", initialize);
    { ::std::lock_guard lock{state->mutex}; state->collecting = true; }
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
                lib::full_compile_run_config config{}; config.entry_function_index = 1u;
                config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(outputs[i]));
                config.entry_abi_buffers.result_bytes = sizeof(outputs[i]);
                lib::full_compile_and_run_main_module(u8"gc-state", config);
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
                ws::request query{ws::selection::globals, state->participants[0u], 0u, 0u, 0u, 0u, 7u};
                auto call = [&](auto const& owners, ws::request const& selected)
                { return lib::llvm_jit_debug_query_wasm_state_host_api(ticket, owners, selected); };
                {
                    auto real_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()}; REQUIRE(real_reader);
                    auto refused{call(captured, query)};
                    REQUIRE(refused.result == ws::status::unavailable_gc_roots && refused.rows.empty() && refused.objects.empty());
                } // Retire actual extra reader BEFORE N can be acquired.
                auto observed{call(captured, query)};
                REQUIRE(observed.result == ws::status::available && ws::valid(observed) && observed.rows.size() == 7u);
                REQUIRE(!observed.snapshot_or_restore_authority());
                REQUIRE(observed.rows[0u].data.ref.kind == ws::reference_kind::structure &&
                    observed.rows[0u].data.ref.object == observed.rows[1u].data.ref.object);
                auto const& node{observed.objects[observed.rows[0u].data.ref.object - 1u]};
                REQUIRE(node.kind == ws::object_kind::structure && node.members.size() == 3u &&
                    node.members[0u].data.ref.object == node.identifier && node.members[1u].data.packed_bits == 8u);
                REQUIRE(observed.rows[2u].data.ref.kind == ws::reference_kind::array &&
                    observed.rows[3u].data.ref.kind == ws::reference_kind::exception &&
                    observed.rows[4u].data.ref.kind == ws::reference_kind::external_wrapper);
                auto const& event{observed.objects[observed.rows[3u].data.ref.object - 1u]};
                REQUIRE(event.kind == ws::object_kind::exception && event.tag_identity_available && event.tag_module == 0u &&
                    event.tag_index == 0u && event.members.size() == 3u && event.members[1u].data.ref.object == node.identifier);
                auto const& wrapper{observed.objects[observed.rows[4u].data.ref.object - 1u]};
                REQUIRE(wrapper.kind == ws::object_kind::external_wrapper && wrapper.members.size() == 1u &&
                    wrapper.members[0u].data.ref.object == node.identifier);
                auto const& array{observed.objects[observed.rows[2u].data.ref.object - 1u]};
                REQUIRE(array.kind == ws::object_kind::array && array.members.size() == 3u);
                auto read32_lane = [](ws::value const& value, ::std::size_t lane)
                {
                    REQUIRE(lane < 4u);
                    auto const* begin{reinterpret_cast<unsigned char const*>(value.bits.data()) + lane * 4u};
                    auto const* end{begin + 4u}; // proven lane<4 in the actual fixed sixteen bytes
                    ::std::uint32_t actual{};
                    auto const parsed{::fast_io::parse_by_scan(begin, end, ::fast_io::mnp::le_get<32u>(actual))};
                    REQUIRE(parsed.code == ::fast_io::parse_code::ok && parsed.iter == end); return actual;
                };
                REQUIRE(read32_lane(node.members[1u].data, 0u) == 127u && read32_lane(event.members[0u].data, 0u) == 77u);
                for(::std::size_t lane{}; lane != 4u; ++lane)
                {
                    REQUIRE(read32_lane(node.members[2u].data, lane) == lane + 1u &&
                        read32_lane(observed.rows[6u].data, lane) == lane + 1u &&
                        read32_lane(event.members[2u].data, lane) == lane + 5u);
                }
                auto read64 = [](ws::value const& value)
                {
                    ::std::uint64_t actual{};
                    auto const* begin{reinterpret_cast<unsigned char const*>(value.bits.data())};
                    auto const* end{begin + 8u}; // actual fixed complete16B view; 8<=16 BEFORE jump
                    auto const parsed{::fast_io::parse_by_scan(begin, end, ::fast_io::mnp::le_get<64u>(actual))};
                    REQUIRE(parsed.code == ::fast_io::parse_code::ok && parsed.iter == end); return actual;
                };
                REQUIRE(read64(array.members[0u].data) == 10u && read64(array.members[1u].data) == 20u &&
                    read64(array.members[2u].data) == 30u && read64(observed.rows[5u].data) == 0xffffffffffffffffull);
                // EVERY member query reborrows an original root and real path
                // under the SAME complete2 cohort + hostclose + actual N proof.
                auto page_request = [&](::std::uint64_t root_index, ::std::uint64_t first_member, ::std::uint64_t count)
                {
                    ws::request page{ws::selection::globals, state->participants[0u], 0u, 0u, 0u, root_index, 1u};
                    page.member_first = first_member; page.member_count = count; return page;
                };
                for(auto const first_member : {128u, 960u, 1024u})
                {
                    auto page{call(captured, page_request(7u, first_member, 64u))};
                    REQUIRE(page.result == ws::status::available && ws::valid(page) && page.rows.size() == 1u &&
                        page.rows[0u].index == 7u && page.selected_object == 1u && page.objects.size() == 1u);
                    auto const& object{page.objects[page.selected_object - 1u]};
                    REQUIRE(object.total_members == 1024u && object.first_member == first_member &&
                        object.next_member == (first_member == 1024u ? 1024u : first_member + 64u) &&
                        object.members.size() == (first_member == 1024u ? 0u : 64u) &&
                        object.has_more_members == (first_member == 128u));
                    for(::std::size_t i{}; i != object.members.size(); ++i)
                    {
                        // [actual bounded selected copied page] end
                        // [safe] i<actual size<=64 BEFORE each view row read.
                        REQUIRE(object.members[i].index == first_member + i && read64(object.members[i].data) == 12345u &&
                            object.members[i].mutability_known && object.members[i].mutable_storage);
                    }
                }
                auto too_far{call(captured, page_request(7u, 1025u, 64u))};
                REQUIRE(too_far.result == ws::status::out_of_range && too_far.rows.empty() && too_far.objects.empty());
                auto distinct{call(captured, page_request(8u, 0u, 64u))};
                REQUIRE(distinct.result == ws::status::available && ws::valid(distinct) && distinct.objects.size() == 65u &&
                    distinct.objects[distinct.selected_object - 1u].members.size() == 64u);
                for(::std::size_t i{1u}; i != distinct.objects.size(); ++i)
                {
                    REQUIRE(distinct.objects[i].kind == ws::object_kind::structure && distinct.objects[i].members.empty() &&
                        distinct.objects[i].total_members == 3u && distinct.objects[i].has_more_members);
                }
                auto aliases{call(captured, page_request(9u, 960u, 64u))};
                REQUIRE(aliases.result == ws::status::available && ws::valid(aliases) && aliases.objects.size() == 2u);
                auto const& alias_members{aliases.objects[aliases.selected_object - 1u].members};
                REQUIRE(alias_members.size() == 64u);
                for(auto const& member : alias_members) { REQUIRE(member.data.ref.object == alias_members.front().data.ref.object); }
                auto nested{page_request(9u, 0u, 3u)}; nested.path_size = 1u; nested.path[0u] = 1000u;
                auto node_page{call(captured, nested)};
                REQUIRE(node_page.result == ws::status::available && ws::valid(node_page) && node_page.selected_object == 2u &&
                    node_page.objects[1u].members.size() == 3u && node_page.objects[0u].members.empty() &&
                    node_page.objects[1u].members[0u].data.ref.object == 2u &&
                    node_page.objects[1u].members[1u].data.packed_bits == 8u &&
                    node_page.objects[1u].members[1u].mutability_known && node_page.objects[1u].members[1u].mutable_storage &&
                    !node_page.objects[1u].members[2u].mutable_storage);
                auto wrapped{page_request(4u, 0u, 3u)}; wrapped.path_size = 1u; wrapped.path[0u] = 0u;
                REQUIRE(call(captured, wrapped).result == ws::status::available);
                auto exn_node{page_request(3u, 0u, 3u)}; exn_node.path_size = 1u; exn_node.path[0u] = 1u;
                REQUIRE(call(captured, exn_node).result == ws::status::available);
                auto scalar_path{page_request(7u, 0u, 3u)}; scalar_path.path_size = 1u; scalar_path.path[0u] = 0u;
                auto scalar_refused{call(captured, scalar_path)};
                REQUIRE(scalar_refused.result == ws::status::invalid_selection && scalar_refused.rows.empty() && scalar_refused.objects.empty());
                auto path_outside{nested}; path_outside.path[0u] = 1024u;
                REQUIRE(call(captured, path_outside).result == ws::status::out_of_range);
                { auto real_extra{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()}; REQUIRE(real_extra);
                  REQUIRE(call(captured, nested).result == ws::status::unavailable_gc_roots); }
                auto one_owner{lib::llvm_jit_debug_query_wasm_state_host_api(ticket, {captured.data(), 1u}, nested)};
                REQUIRE(one_owner.result == ws::status::incomplete_cohort && one_owner.objects.empty());
                query.selected = ws::selection::table; query.count = 2u;
                auto table{call(captured, query)};
                REQUIRE(table.result == ws::status::available && table.rows.size() == 2u &&
                    table.rows[0u].data.ref.object == table.rows[1u].data.ref.object);
                query.selected = ws::selection::locals; query.count = 5u;
                auto locals{call(captured, query)};
                REQUIRE(locals.result == ws::status::available && locals.rows.size() == 5u &&
                    locals.rows[0u].data.ref.kind == ws::reference_kind::structure &&
                    locals.rows[1u].data.ref.kind == ws::reference_kind::array &&
                    locals.rows[2u].data.ref.kind == ws::reference_kind::exception &&
                    locals.rows[3u].data.ref.kind == ws::reference_kind::external_wrapper && read64(locals.rows[4u].data) == 1234u);
                query.selected = ws::selection::operands; query.count = 2u;
                auto operands{call(captured, query)};
                REQUIRE(operands.result == ws::status::available && operands.rows.size() == 2u &&
                    read64(operands.rows[0u].data) == 1234u && operands.rows[1u].data.ref.kind == ws::reference_kind::structure);
                // Independently authenticate and inspect BOTH real workers.
                // A successful query for the first worker cannot prove that the
                // second worker's current locals/operand packet is readable.
                auto const original_participant{query.participant};
                ::std::array<::std::uint32_t, 2u> atomic_values{};
                ::std::size_t atomic_value_count{};
                for(auto const participant : state->participants)
                {
                    query.participant = participant;
                    query.selected = ws::selection::locals; query.count = 5u;
                    auto const local_page{call(captured, query)};
                    REQUIRE(local_page.result == ws::status::available && ws::valid(local_page) &&
                        local_page.rows.size() == 5u && read64(local_page.rows[4u].data) == 1234u);
                    query.selected = ws::selection::operands; query.count = 2u;
                    auto const operand_page{call(captured, query)};
                    REQUIRE(operand_page.result == ws::status::available && ws::valid(operand_page) &&
                        operand_page.rows.size() == 2u && read64(operand_page.rows[0u].data) == 1234u &&
                        operand_page.rows[1u].data.ref.kind == ws::reference_kind::structure);
                    if(atomic_workers)
                    {
                        query.selected = ws::selection::locals; query.first = 5u; query.count = 1u;
                        auto const counter{call(captured, query)};
                        REQUIRE(counter.result == ws::status::available && ws::valid(counter) && counter.rows.size() == 1u &&
                            counter.rows[0u].index == 5u && counter.rows[0u].data.available &&
                            counter.rows[0u].data.type.kind == ws::value_kind::i32 && atomic_value_count < atomic_values.size());
                        atomic_values[atomic_value_count++] = read32_lane(counter.rows[0u].data, 0u);
                        query.first = 0u;
                    }
                }
                if(atomic_workers)
                {
                    // Both real Wasm entries share ONE actual linear memory.
                    // Their atomic RMW returns must be distinct consecutive
                    // values even when the observer sees the workers reversed.
                    auto const base{static_cast<::std::uint32_t>(2u * attempts)};
                    REQUIRE(atomic_value_count == 2u &&
                        ((atomic_values[0u] == base && atomic_values[1u] == base + 1u) ||
                         (atomic_values[1u] == base && atomic_values[0u] == base + 1u)));
                    ::fast_io::print(::fast_io::out(), "ATOMIC_TWO_PARTICIPANTS episode=", ::fast_io::mnp::dec(positives),
                        " old_values=", ::fast_io::mnp::dec(atomic_values[0u]), ",", ::fast_io::mnp::dec(atomic_values[1u]),
                        " actual_cohort=2 typed_local=5\n");
                }
                query.participant = original_participant;
                query.selected = ws::selection::operands; query.count = 2u;
                query.first = 3u;
                auto outside{call(captured, query)};
                REQUIRE(outside.result == ws::status::out_of_range && outside.rows.empty() && outside.objects.empty()); query.first = 0u;
                auto subset{lib::llvm_jit_debug_query_wasm_state_host_api(ticket, {captured.data(), 1u}, query)};
                REQUIRE(subset.result == ws::status::incomplete_cohort && subset.rows.empty() && subset.objects.empty());
                ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 2u> duplicate{captured[0u], captured[0u]};
                auto duplicated{call(duplicate, query)};
                REQUIRE(duplicated.result == ws::status::incomplete_cohort && duplicated.rows.empty() && duplicated.objects.empty());
                ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 2u> reversed{captured[1u], captured[0u]};
                REQUIRE(call(reversed, query).result == ws::status::available);
                lib::llvm_jit_checkpoint_thread_capture_owner forged{captured[0u],
                    reinterpret_cast<lib::llvm_jit_checkpoint_thread_capture const*>(::std::uintptr_t{1u})};
                ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner, 2u> foreign{forged, captured[1u]};
                auto wrong_owner{call(foreign, query)};
                REQUIRE(wrong_owner.result == ws::status::unavailable_activation && wrong_owner.rows.empty() && wrong_owner.objects.empty());
                auto strict{lib::llvm_jit_checkpoint_query_resource_inputs_host_api(ticket, captured)};
                REQUIRE(strict.status != lib::llvm_jit_checkpoint_query_status::coherent_typed_data && strict.threads.empty());
                if(positives == 0u) { first = captured; first_ticket = ticket; first_request = request; epoch = observed.runtime_epoch; }
                else
                {
                    REQUIRE(request == first_request && epoch == observed.runtime_epoch &&
                        captured[0u].get() != first[0u].get() && captured[1u].get() != first[1u].get());
                    auto stale{call(first, query)};
                    REQUIRE(stale.result == ws::status::stale_stop_or_generation && stale.rows.empty() && stale.objects.empty());
                    auto old_ticket{lib::llvm_jit_debug_query_wasm_state_host_api(first_ticket, captured, query)};
                    REQUIRE(old_ticket.result != ws::status::available && old_ticket.rows.empty() && old_ticket.objects.empty());
                }
                ++positives; // ONLY after real two-participant producer + manager success
            }
            REQUIRE(state->control->resume(ticket));
            ws::request after_resume{ws::selection::globals, state->participants[0u], 0u, 0u, 0u, 0u, 1u};
            auto resumed{lib::llvm_jit_debug_query_wasm_state_host_api(ticket, first, after_resume)};
            REQUIRE(resumed.result != ws::status::available && resumed.rows.empty() && resumed.objects.empty());
        }
        for(auto& guest : guests) { guest.join(); }
        REQUIRE(outputs[0u] == 42u && outputs[1u] == 42u);
    }
    REQUIRE(positives == 2u);
    lib::reset_runtime_state_host_api(); REQUIRE(state->control->is_closed());
    ws::request after_reset{ws::selection::globals, 1u, 0u, 0u, 0u, 0u, 1u};
    auto retired{lib::llvm_jit_debug_query_wasm_state_host_api(first_ticket, first, after_reset)};
    REQUIRE(retired.result != ws::status::available && retired.rows.empty() && retired.objects.empty());
    ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr;
    ::fast_io::print(::fast_io::out(), "DEBUG_GC_STATE_TWO_PARTICIPANTS policy=", policy,
        " observe_only=", observe_only, " atomic_workers=", atomic_workers,
        " actual_census=2 positive_episodes=", ::fast_io::mnp::dec(positives), " attempts=", ::fast_io::mnp::dec(attempts),
        " owned_alias_cycle_exn=1 original_member_pages=1 distinct_children=64 old_episode_refused=1 whole_restore=0\n");
}
