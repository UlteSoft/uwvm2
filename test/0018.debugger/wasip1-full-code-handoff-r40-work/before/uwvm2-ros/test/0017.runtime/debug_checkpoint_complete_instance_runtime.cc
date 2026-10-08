// Actual setup then two read-only LLVM-full guests. New instance census under
// one real pause/cohort/hostclose/N/publication, not a copied debugger VIEW.
// The const graph is logical DATA; whole-instance restoration is not claimed.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/utils/control/owned_file_image.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/runtime/gc/entry_admission.h>
#include <uwvm2/runtime/gc/frame_roots.h>
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
    ::fast_io::print(::fast_io::err(), "debug_checkpoint_complete_instance_runtime FAIL line=", ::fast_io::mnp::dec(line), "\n");
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

static cp::object const& object(cp::state const& graph, cp::object_id id, cp::object_kind kind)
{
    REQUIRE(id != 0u && id <= graph.objects.size());
    // [owned graph dense IDs1..N] end; bound checked BEFORE id-1/index.
    auto const& item{graph.objects[static_cast<::std::size_t>(id-1u)]};
    REQUIRE(item.kind == kind); return item;
}
static cp::object_id resource(cp::object const& instance, unsigned space, ::std::size_t index)
{
    REQUIRE(instance.kind == cp::object_kind::instance && space < 7u && !instance.links.empty());
    ::std::size_t offset{1u};
    for(unsigned prior{}; prior != space; ++prior)
    {
        REQUIRE(offset <= instance.links.size() && instance.words[prior] <= instance.links.size()-offset);
        offset += static_cast<::std::size_t>(instance.words[prior]); // remaining proof BEFORE addition
    }
    REQUIRE(offset <= instance.links.size() && index < instance.words[space] && index < instance.links.size()-offset);
    return instance.links[offset+index]; // both actual extents checked BEFORE +index
}
// A pause can legitimately land near the two final drop instructions.
// Count the actual long-nop-window snapshots only; do not invent a stop site or
// treat a scheduling miss as a resource-census failure. All returned graphs are
// independently validated before this layout-only selection.
static bool stable_operand_window(cp::state const& graph)
{
    ::std::size_t threads{}, frames{};
    for(auto const& item : graph.objects)
    {
        if(item.kind == cp::object_kind::thread) { ++threads; }
        else if(item.kind == cp::object_kind::frame)
        {
            ++frames;
            if(item.words[1u] != 6u || item.words[2u] != 2u || item.values.size() != 8u) { return false; }
        }
    }
    return threads == 2u && frames == 2u;
}
static void check_graph(cp::state const& graph, cp::limits const& cap, ::std::array<::std::byte,16u> const& label)
{
    REQUIRE(cp::validate_graph(graph,cap) == cp::error::none && graph.recording_id == label);
    REQUIRE(graph.root_instances.size() == 1u && graph.next_logical_thread == 3u);
    auto const& instance{object(graph,graph.root_instances[0u],cp::object_kind::instance)};
    constexpr ::std::array<::std::uint64_t,8u> counts{3u,2u,1u,11u,1u,2u,2u,0u};
    REQUIRE(instance.words == counts);
    auto const module{instance.links[0u]}; REQUIRE(object(graph,module,cp::object_kind::module).bytes.size() >= 8u);
    auto const& module_state{object(graph,module,cp::object_kind::module)};
    REQUIRE(module_state.flags == 1u && module_state.words[0u] == 1u);
    // This real fixture enables modern GC/memory64/EH, but leaves threads and
    // relaxed SIMD disabled. Product capabilities cannot become requirements.
    REQUIRE((module_state.words[2u] & (std::uint64_t{1u} << 18u)) != 0u);
    REQUIRE((graph.required_features & (std::uint64_t{1u} << 16u)) == 0u);
    REQUIRE((graph.required_features & (std::uint64_t{1u} << 3u)) == 0u);
    auto const callback{resource(instance,0u,2u)}; auto const& function{object(graph,callback,cp::object_kind::function)};
    // Generation 1 borrows its body from the captured module; only replacements carry body bytes.
    REQUIRE(function.words[0u] == 2u && function.words[1u] == 1u && function.bytes.empty());
    auto global = [&](::std::size_t index) -> cp::value const&
    { auto const& item{object(graph,resource(instance,3u,index),cp::object_kind::global)}; REQUIRE(item.values.size() == 1u); return item.values[0u]; };
    auto const id{global(0u).target}; REQUIRE(global(0u).reference == cp::reference_kind::structure && global(1u).target == id);
    auto const& node{object(graph,id,cp::object_kind::structure)};
    REQUIRE(node.values.size() == 3u && node.values[0u].target == id && node.values[1u].type.kind == cp::value_kind::i8 && node.values[1u].low_bits == 127u);
    REQUIRE(node.values[2u].type.kind == cp::value_kind::v128 && node.values[2u].low_bits == 0x0000000200000001ull && node.values[2u].high_bits == 0x0000000400000003ull);
    auto const& small{object(graph,global(2u).target,cp::object_kind::array)};
    REQUIRE(small.values.size() == 3u && small.values[0u].low_bits == 10u && small.values[1u].low_bits == 20u && small.values[2u].low_bits == 30u);
    // Actual first catch_ref and throw_ref re-catch roots preserve ONE Core
    // exception record even if the runtime issued different native tokens.
    REQUIRE(global(3u).reference == cp::reference_kind::exception && global(10u).reference == cp::reference_kind::exception && global(10u).target == global(3u).target);
    auto const& exception{object(graph,global(3u).target,cp::object_kind::exception)};
    REQUIRE(exception.links.size() == 2u && exception.links[0u] == resource(instance,4u,0u) && exception.values.size() == 3u);
    REQUIRE(exception.values[0u].low_bits == 77u && exception.values[1u].target == id && exception.values[2u].low_bits == 0x0000000600000005ull && exception.values[2u].high_bits == 0x0000000800000007ull);
    auto const& trace{object(graph,exception.links[1u],cp::object_kind::exception_trace)};
    REQUIRE(trace.words[0u] == 1u && trace.links.size() == 1u && trace.values.size() == 1u && trace.links[0u] == resource(instance,0u,0u) && trace.values[0u].low_bits == cp::unknown_diagnostic_instruction && trace.bytes.size() >= 40u);
    auto const& wrapper{object(graph,global(4u).target,cp::object_kind::external)}; REQUIRE(wrapper.values.size() == 1u && wrapper.values[0u].target == id);
    REQUIRE(global(5u).type.kind == cp::value_kind::i64 && global(5u).low_bits == UINT64_MAX && global(6u).low_bits == node.values[2u].low_bits && global(6u).high_bits == node.values[2u].high_bits);
    auto const& large{object(graph,global(7u).target,cp::object_kind::array)}; REQUIRE(large.values.size() == 1024u);
    for(auto const& value : large.values) { REQUIRE(value.type.kind == cp::value_kind::i64 && value.low_bits == 12345u); }
    auto const& distinct{object(graph,global(8u).target,cp::object_kind::array)}; REQUIRE(distinct.values.size() == 64u);
    ::std::array<cp::object_id,64u> seen{};
    for(::std::size_t index{}; index != seen.size(); ++index)
    {
        REQUIRE(index < distinct.values.size()); auto const child{distinct.values[index].target}; REQUIRE(child != id);
        for(::std::size_t prior{}; prior != index; ++prior) { REQUIRE(seen[prior] != child); } seen[index] = child;
        auto const& fields{object(graph,child,cp::object_kind::structure).values};
        REQUIRE(fields.size() == 3u && fields[0u].reference == cp::reference_kind::null && fields[1u].type.kind == cp::value_kind::i8 && fields[1u].low_bits == index && fields[2u].low_bits == 0u && fields[2u].high_bits == 0u);
    }
    auto const& aliases{object(graph,global(9u).target,cp::object_kind::array)}; REQUIRE(aliases.values.size() == 1024u);
    for(auto const& value : aliases.values) { REQUIRE(value.target == id); }
    auto const table_id{resource(instance,1u,0u)}; auto const& table{object(graph,table_id,cp::object_kind::table)};
    REQUIRE(table.words[0u] == 64u && table.words[1u] == 2u && table.words[2u] == 2u && table.words[3u] == 4u && table.values.size() == 1u && table.values[0u].initialized && table.values[0u].target == id);
    auto const& empty{object(graph,resource(instance,1u,1u),cp::object_kind::table)};
    REQUIRE(empty.words[0u] == 64u && empty.words[1u] == 0u && empty.words[2u] == 0u && empty.words[3u] == 0u && empty.values.size() == 1u);
    auto const& type{empty.values[0u]}; REQUIRE(!type.initialized && !type.type.nullable && type.type.heap == cp::heap_kind::defined && type.type.type_module == module && type.type.type_index == function.words[2u] && type.target == 0u && type.reference == cp::reference_kind::null && type.low_bits == 0u && type.high_bits == 0u);
    auto const memory_id{resource(instance,2u,0u)}; auto const& memory{object(graph,memory_id,cp::object_kind::memory)};
    REQUIRE(memory.words[0u] == 64u && memory.words[1u] == 3u && memory.words[2u] == 3u && memory.words[3u] == 5u);
    ::std::size_t memory_chunks{},table_chunks{},structures{},arrays{},exceptions{},traces{},native_threads{},frames{};
    for(auto const& item : graph.objects)
    {
        if(item.kind == cp::object_kind::memory_chunk)
        {
            REQUIRE(item.links.size() == 1u && item.links[0u] == memory_id && item.bytes.size() == 65536u && (item.words[0u] == 0u || item.words[0u] == 131072u));
            auto const marked{item.words[0u] == 0u ? 9u : 17u}; auto const bits{item.words[0u] == 0u ? ::std::byte{0xabu} : ::std::byte{0xcdu}};
            for(::std::size_t byte{}; byte != item.bytes.size(); ++byte) { REQUIRE(item.bytes[byte] == (byte == marked ? bits : ::std::byte{})); } ++memory_chunks;
        }
        else if(item.kind == cp::object_kind::table_chunk) { REQUIRE(item.links.size() == 1u && item.links[0u] == table_id && item.words[0u] == 1u && item.values.size() == 1u && item.values[0u].target == seen[0u]); ++table_chunks; }
        else if(item.kind == cp::object_kind::structure) { ++structures; }
        else if(item.kind == cp::object_kind::array) { ++arrays; }
        else if(item.kind == cp::object_kind::exception) { ++exceptions; }
        else if(item.kind == cp::object_kind::exception_trace) { ++traces; }
        else if(item.kind == cp::object_kind::frame) { ++frames; }
        else if(item.kind == cp::object_kind::thread)
        {
            REQUIRE(item.links.size() == 1u); auto const& frame{object(graph,item.links[0u],cp::object_kind::frame)};
            REQUIRE(!frame.links.empty() && frame.links[0u] == resource(instance,0u,1u) && frame.words[1u] == 6u && frame.words[2u] == 2u && frame.values.size() == 8u);
            REQUIRE(frame.values[0u].target == id && frame.values[1u].target == global(2u).target && frame.values[2u].target == global(3u).target && frame.values[3u].target == global(4u).target && frame.values[4u].low_bits == 1234u && frame.values[5u].target == global(10u).target && frame.values[6u].low_bits == 1234u && frame.values[7u].target == id); ++native_threads;
        }
    }
    REQUIRE(memory_chunks == 2u && table_chunks == 1u && structures == 65u && arrays == 4u && exceptions == 1u && traces == 1u && native_threads == 2u && frames == 2u);
    auto const& data{object(graph,resource(instance,5u,0u),cp::object_kind::data)};
    constexpr ::std::array<::std::byte,6u> alive{::std::byte{'a'},::std::byte{'l'},::std::byte{'i'},::std::byte{'v'},::std::byte{'e'},::std::byte{}};
    REQUIRE(data.flags == 0u && data.bytes.size() == alive.size()); for(::std::size_t index{}; index != alive.size(); ++index) { REQUIRE(data.bytes[index] == alive[index]); }
    auto const& dropped{object(graph,resource(instance,5u,1u),cp::object_kind::data)}; REQUIRE(dropped.flags == 1u && dropped.bytes.empty());
    auto const& element{object(graph,resource(instance,6u,0u),cp::object_kind::element)}; REQUIRE(element.flags == 0u && element.values.size() == 1u && element.values[0u].target == callback);
    auto const& gone{object(graph,resource(instance,6u,1u),cp::object_kind::element)}; REQUIRE(gone.flags == 1u && gone.values.empty());
}
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
    auto bytes{image::owned_file_image::read(setup.path, 1048576u)}; REQUIRE(bytes);
    auto const original{bytes.image->bytes()}; REQUIRE(original.size() >= 8u && original.size() <= PTRDIFF_MAX);
    setup.immutable = ::std::move(bytes.image);
    REQUIRE(lib::replace_full_source_after_drain_host_api(source_setup::prepare, ::std::addressof(setup)) &&
        setup.ready && setup.source && setup.source->file().has_owned_source_image());
    auto state{::std::make_shared<observer>()};
    auto profile{checkpoint::compilation_profile::create_for_trusted_manager({}, checkpoint::compilation_purpose::observe_values)};
    REQUIRE(profile && profile->purpose() == checkpoint::compilation_purpose::observe_values);
    REQUIRE(lib::llvm_jit_configure_debug_session_host_api(state->control,
        {state, observer::point, observer::before_park}, lib::llvm_jit_debug_safe_point_granularity::instruction) ==
        lib::llvm_jit_debug_configure_result::ok);
    // The resumable setter must reject an observation-only profile. Select the
    // genuine value producer through its own quiescent observation interface.
    REQUIRE(lib::llvm_jit_configure_checkpoint_recording_host_api(profile) == lib::llvm_jit_debug_configure_result::invalid_context);
    REQUIRE(lib::llvm_jit_configure_debug_value_observation_host_api(profile->limits()) == lib::llvm_jit_debug_configure_result::ok);
    REQUIRE(!lib::llvm_jit_checkpoint_capture_thread_host_api({}).capture);
    REQUIRE(lib::llvm_jit_prepare_debug_host_api()); // SAME authoritative fused validation/lowering, once
    // Only this ordinary setup entry mutates globals/table/objects. The two
    // later participants read them, avoiding test-created guest data races.
    lib::full_compile_run_config initialize{}; initialize.entry_function_index = 0u;
    lib::full_compile_and_run_main_module(u8"checkpoint-instance", initialize);
    { ::std::lock_guard lock{state->mutex}; state->collecting = true; }

    ::std::array<::std::byte,16u> label{}; label[0u] = ::std::byte{0x43u}; label[15u] = ::std::byte{0x5au};
    cp::limits cap{}; cap.max_file_bytes = 2u*1024u*1024u; cap.max_payload_bytes = 1024u*1024u;
    cap.max_objects = 1024u; cap.max_links = 8192u; cap.max_values = 16384u; cap.max_threads = 2u; cap.max_frames = 2u;
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner,2u> first{};
    domain::pause_ticket first_ticket{}; ::std::shared_ptr<cp::state const> detached{};
    ::std::size_t positives{},attempts{};
    auto refused = [](lib::llvm_jit_checkpoint_instance_capture_result const& result)
    { REQUIRE(result.status != lib::llvm_jit_checkpoint_instance_capture_status::captured && !result.graph); };
    for(; attempts != 8u && positives != 2u; ++attempts)
    {
        state->prepare_attempt(); ::std::barrier start{3};
        ::std::array<::std::uint32_t,2u> outputs{}; ::std::array<::std::thread,2u> guests{};
        for(::std::size_t index{}; index != guests.size(); ++index)
        {
            guests[index] = ::std::thread{[&,index]
            {
                start.arrive_and_wait(); lib::full_compile_run_config config{}; config.entry_function_index = 1u;
                // [two independent actual host output cells0..2] end
                // [safe] index<2 BEFORE buffer selection; retained until guest.join.
                config.entry_abi_buffers.result_buffer = reinterpret_cast<::std::byte*>(::std::addressof(outputs[index]));
                config.entry_abi_buffers.result_bytes = sizeof(outputs[index]);
                lib::full_compile_and_run_main_module(u8"checkpoint-instance",config);
                ::std::lock_guard lock{state->mutex}; ++state->done; state->changed.notify_all();
            }};
        }
        start.arrive_and_wait(); domain::pause_ticket ticket{};
        { ::std::unique_lock lock{state->mutex}; REQUIRE(state->changed.wait_until(lock,deadline(),[&] { return bool(state->ticket) || state->done == 2u; })); ticket = state->ticket; }
        if(ticket)
        {
            REQUIRE(state->control->wait_until_paused(ticket,deadline()) == threads::cooperative_pause_result::paused);
            auto const roster{state->control->capture(ticket)};
            REQUIRE(roster.result == threads::cooperative_pause_result::paused && roster.participants.size() <= 2u);
            if(roster.participants.size() == 2u)
            {
                ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner,2u> captured{};
                {
                    ::std::lock_guard lock{state->mutex}; REQUIRE(state->seen == 2u);
                    for(::std::size_t index{}; index != captured.size(); ++index)
                    {
                        REQUIRE(state->captures[index].status == lib::llvm_jit_checkpoint_capture_status::captured);
                        captured[index] = state->captures[index].capture; REQUIRE(captured[index]);
                        bool matched{}; for(auto const& actual : roster.participants)
                        { matched |= actual.id == state->participants[index] && actual.location == state->locations[index]; }
                        REQUIRE(matched); // genuine before-park capture matched to actual retained cohort
                    }
                }
                auto call = [&](auto const& owners) { return lib::llvm_jit_checkpoint_capture_instance_host_api(ticket,owners,label,cap); };
                { auto reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()}; REQUIRE(reader); refused(call(captured)); }
                // The actual additional reader above left before exclusive N can admit this copy.
                ::std::array<::std::byte,16u> empty_label{};
                auto no_label{lib::llvm_jit_checkpoint_capture_instance_host_api(ticket,captured,empty_label,cap)};
                refused(no_label); REQUIRE(no_label.status == lib::llvm_jit_checkpoint_instance_capture_status::invalid_recording_label);
                refused(lib::llvm_jit_checkpoint_capture_instance_host_api(ticket,{captured.data(),1u},label,cap));
                ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner,2u> duplicate{captured[0u],captured[0u]}; refused(call(duplicate));
                lib::llvm_jit_checkpoint_thread_capture_owner bad_address{captured[0u],reinterpret_cast<lib::llvm_jit_checkpoint_thread_capture const*>(::std::uintptr_t{1u})};
                ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner,2u> foreign{bad_address,captured[1u]}; refused(call(foreign));
                lib::llvm_jit_checkpoint_thread_capture_owner bad_control{captured[0u].get(),[](auto*) noexcept {}};
                foreign = {bad_control,captured[1u]}; refused(call(foreign)); // same pointer, different actual shared control block
                auto tiny{cap}; tiny.max_objects = 1u;
                auto limited{lib::llvm_jit_checkpoint_capture_instance_host_api(ticket,captured,label,tiny)};
                refused(limited); REQUIRE(limited.data_error == cp::error::limit_exceeded);
                auto actual{call(captured)};
                if(actual.status != lib::llvm_jit_checkpoint_instance_capture_status::captured || actual.data_error != cp::error::none || !actual.graph)
                { ::fast_io::print(::fast_io::err(), "complete census refused status=", ::fast_io::mnp::dec(static_cast<unsigned>(actual.status)),
                    " data_error=", ::fast_io::mnp::dec(static_cast<unsigned>(actual.data_error)), "\n"); }
                REQUIRE(actual.status == lib::llvm_jit_checkpoint_instance_capture_status::captured && actual.data_error == cp::error::none && actual.graph);
                REQUIRE(cp::validate_graph(*actual.graph,cap) == cp::error::none);
                if(stable_operand_window(*actual.graph))
                {
                    check_graph(*actual.graph,cap,label);
                    namespace gc=::uwvm2::runtime::gc;
                    gc::root_reference inherited_values[2u]{};
                    gc::scoped_root_frame outer_roots{{inherited_values,2u}};REQUIRE(outer_roots && outer_roots.publish(2u));
                    gc::scoped_root_frame inner_roots{{inherited_values,1u}};REQUIRE(inner_roots && inner_roots.publish(1u));
                    auto const* inherited{gc::current_root_frames()};
                    lib::llvm_jit_checkpoint_prepare_request prepare{};
                    prepare.recording_label=label;prepare.graph_budget=cap;
                    // Thousands of native resume landings also reserve the
                    // complete object/symbol workspace. This explicit test
                    // budget preserves the public 256 MiB default and all
                    // real process/cgroup limits.
                    prepare.maximum_native_payload_bytes=512u*1024u*1024u;
                    auto expect_refused=[](lib::llvm_jit_checkpoint_prepare_result const& data)
                    { REQUIRE(data.status!=lib::llvm_jit_checkpoint_prepare_status::prepared_and_discarded && data.engines==0u); };
                    auto invalid=prepare;invalid.recording_label={};
                    REQUIRE(lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,captured,invalid).status==
                        lib::llvm_jit_checkpoint_prepare_status::invalid_request);
                    auto small=prepare;small.maximum_native_payload_bytes=1u;
                    REQUIRE(lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,captured,small).status==
                        lib::llvm_jit_checkpoint_prepare_status::resource_preparation_declined);
                    auto source_small=prepare;source_small.maximum_original_source_bytes=1u;
                    REQUIRE(lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,captured,source_small).status==
                        lib::llvm_jit_checkpoint_prepare_status::resource_preparation_declined);
                    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner,2u> duplicate{captured[0u],captured[0u]};
                    expect_refused(lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,duplicate,prepare));
                    expect_refused(lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,{captured.data(),1u},prepare));
                    auto source_limited=prepare;source_limited.maximum_private_source_initializer_modules=0u;
                    auto const source_epoch=lib::observe_compiler_runtime_generation_host_api();
                    auto const source_serial=::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire);
                    auto source_refused=lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,captured,source_limited);
                    REQUIRE(source_refused.status==lib::llvm_jit_checkpoint_prepare_status::source_initializer_binding_preparation_declined &&
                        !source_refused.runtime_source_initializer_bindings_prepared && source_refused.engines==0u);
                    REQUIRE(lib::observe_compiler_runtime_generation_host_api()==source_epoch &&
                        ::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire)==source_serial);
                    auto publication_limited=prepare;publication_limited.maximum_private_full_publication_functions=0u;
                    auto publication_refused=lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,captured,publication_limited);
                    REQUIRE(publication_refused.status==lib::llvm_jit_checkpoint_prepare_status::full_publication_record_preparation_declined &&
                        !publication_refused.runtime_full_publication_records_prepared && publication_refused.resource_diagnostic==9u &&
                        lib::observe_compiler_runtime_generation_host_api()==source_epoch &&
                        ::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire)==source_serial);
                    auto prepared=lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,captured,prepare);
                    ::fast_io::print(::fast_io::out(),"WORLD_PREPARATION status=",::fast_io::mnp::dec(static_cast<unsigned>(prepared.status)),
                        " resource=",::fast_io::mnp::dec(prepared.resource_diagnostic)," engine=",::fast_io::mnp::dec(prepared.engine_diagnostic),
                        " modules=",::fast_io::mnp::dec(prepared.modules)," engines=",::fast_io::mnp::dec(prepared.engines),
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
                    REQUIRE(prepared.status==lib::llvm_jit_checkpoint_prepare_status::prepared_and_discarded &&
                        prepared.data_error==cp::error::none && prepared.modules==1u &&
                        prepared.runtime_full_publication_records_prepared && prepared.prepared_full_publication_modules==1u &&
                        prepared.prepared_full_publication_functions==prepared.functions &&
                        prepared.runtime_source_initializer_bindings_prepared && prepared.prepared_source_initializer_modules==1u &&
                        prepared.prepared_source_initializer_preloads==0u &&
                        prepared.engines==1u && prepared.functions!=0u && prepared.prepared_threads==2u && prepared.prepared_frames==2u &&
                        prepared.prepared_root_carriers!=0u && prepared.native_payload_bytes<=prepare.maximum_native_payload_bytes);
                    REQUIRE(prepared.validated_private_root_scopes==2u && prepared.installed_private_root_frames==6u &&
                        prepared.installed_private_root_carriers>=prepared.prepared_root_carriers && prepared.private_root_chain_restored &&
                        gc::current_root_frames()==inherited);
                    REQUIRE(prepared.started_private_root_workers==2u && prepared.enrolled_private_root_workers==2u &&
                        prepared.joined_private_root_workers==2u && prepared.private_worker_cohort_held &&
                        prepared.private_worker_roots_restored && gc::current_root_frames()==inherited);
                    REQUIRE(prepared.prepared_private_debug_workers==2u && prepared.enrolled_private_debug_workers==2u &&
                        prepared.private_debug_cohort_held && prepared.private_debug_tls_restored);
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
                    if(positives==0u)
                    {
                        auto object_small=prepare;object_small.maximum_native_payload_bytes=1024u*1024u;
                        auto object_refused=lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,captured,object_small);
                        REQUIRE(object_refused.status==lib::llvm_jit_checkpoint_prepare_status::engine_preparation_declined &&
                            object_refused.engine_diagnostic==5u && object_refused.resource_diagnostic==9u &&
                            object_refused.engines==0u && !object_refused.runtime_native_endpoint_capture_prepared &&
                            object_refused.native_payload_bytes<=object_small.maximum_native_payload_bytes);
                        REQUIRE(gc::current_root_frames()==inherited &&
                            lib::observe_compiler_runtime_generation_host_api()==source_epoch &&
                            gc::published_initializer_serial.load(::std::memory_order_acquire)==source_serial);
                    }
#endif
                    auto workers_small=prepare;workers_small.maximum_private_root_workers=1u;
                    auto workers_refused=lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,captured,workers_small);
                    REQUIRE(workers_refused.status==lib::llvm_jit_checkpoint_prepare_status::worker_root_preparation_declined &&
                        workers_refused.prepared_frames==2u && workers_refused.private_root_chain_restored &&
                        workers_refused.started_private_root_workers==0u && workers_refused.enrolled_private_root_workers==0u &&
                        workers_refused.joined_private_root_workers==0u && !workers_refused.private_worker_cohort_held &&
                        gc::current_root_frames()==inherited);
                    REQUIRE(workers_refused.prepared_private_debug_workers==0u && workers_refused.enrolled_private_debug_workers==0u &&
                        !workers_refused.private_debug_cohort_held);
                    auto root_small=prepare;root_small.maximum_private_root_frames=3u; // Fits one packet, refuses ALL before installation.
                    auto root_refused=lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,captured,root_small);
                    REQUIRE(root_refused.status==lib::llvm_jit_checkpoint_prepare_status::root_preparation_declined &&
                        root_refused.engines==prepared.engines && root_refused.prepared_frames==2u &&
                        root_refused.validated_private_root_scopes==0u && root_refused.installed_private_root_frames==0u &&
                        !root_refused.private_root_chain_restored && gc::current_root_frames()==inherited);
                    auto late=prepare;REQUIRE(prepared.native_payload_bytes>1u);
                    late.maximum_native_payload_bytes=prepared.native_payload_bytes-1u;
                    auto partial=lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,captured,late);
                    REQUIRE(partial.prepared_private_debug_workers==0u && partial.enrolled_private_debug_workers==0u &&
                        !partial.private_debug_cohort_held);
                    REQUIRE(partial.status==lib::llvm_jit_checkpoint_prepare_status::worker_root_preparation_declined &&
                        partial.modules==prepared.modules && partial.engines==prepared.engines && partial.prepared_frames==2u &&
                        partial.native_payload_bytes<=late.maximum_native_payload_bytes);
                    REQUIRE(gc::current_root_frames()==inherited && partial.private_root_chain_restored &&
                        partial.started_private_root_workers==0u && partial.joined_private_root_workers==0u);
                    // The final byte of actual private worker storage cannot fit:
                    // refuse before ANY OS launch, after genuine cold TLS rollback.
                    auto unchanged=call(captured);
                    REQUIRE(unchanged.status==lib::llvm_jit_checkpoint_instance_capture_status::captured && unchanged.graph);
                    check_graph(*unchanged.graph,cap,label);
                    if(positives!=0u)
                    {
                        expect_refused(lib::llvm_jit_checkpoint_prepare_instance_host_api(first_ticket,captured,prepare));
                        expect_refused(lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,first,prepare));
                    }

                    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner,2u> reversed{captured[1u],captured[0u]}; auto reordered{call(reversed)};
                    REQUIRE(reordered.status == lib::llvm_jit_checkpoint_instance_capture_status::captured && reordered.data_error == cp::error::none && reordered.graph);
                    check_graph(*reordered.graph,cap,label);
                    if(positives == 0u) { first = captured; first_ticket = ticket; detached = actual.graph; }
                    else { refused(call(first)); refused(lib::llvm_jit_checkpoint_capture_instance_host_api(first_ticket,captured,label,cap)); }
                    ++positives; // only actual two-thread, complete-graph positives count
                }
            }
            REQUIRE(state->control->resume(ticket));
            refused(lib::llvm_jit_checkpoint_capture_instance_host_api(ticket,first,label,cap));
        }
        for(auto& guest : guests) { guest.join(); }
        REQUIRE(outputs[0u] == 42u && outputs[1u] == 42u);
    }
    REQUIRE(positives == 2u && detached);
    lib::reset_runtime_state_host_api(); REQUIRE(state->control->is_closed());
    refused(lib::llvm_jit_checkpoint_capture_instance_host_api(first_ticket,first,label,cap));
    // Copied const logical DATA survives reset; no runtime lease/native entry/ticket is reissued.
    check_graph(*detached,cap,label); ::uwvm2::uwvm::cmdline::wasm_file_ppos = nullptr;
    ::fast_io::print(::fast_io::out(),"CHECKPOINT_COMPLETE_INSTANCE policy=",policy,
        " actual_census=2 positive_episodes=",::fast_io::mnp::dec(positives)," attempts=",::fast_io::mnp::dec(attempts),
        " structures=65 arrays=4 sparse_mem64_pages=2 empty_nonnullable_table64=1",
        " passive_drop_data_elem=1 alias_cycle_exn_trace=1 throw_ref_same_exn_object=1 old_episode_refused=1 whole_restore=0\n");
}

#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <uwvm2/utils/macro/pop_macros.h>
