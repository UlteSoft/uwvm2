// Genuine setup once, then two LLVM-full guest entries -> actual
// before-park captures -> ONE cohort -> hostclose -> N -> publication -> owned
// GC/exn/global/table/local/operand source -> one typed global/table write.
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
#include <mutex>
#include <string>
#include <thread>
#include <vector>
// Runtime headers restore their feature macros; derive this fixture's own scope.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
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
    ::fast_io::print(::fast_io::err(), "debug_wasm_mutation_runtime FAIL line=", ::fast_io::mnp::dec(line), "\n");
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
            preload.module_name = u8"gc-state-provider"; preload.parameters = ::uwvm2::uwvm::wasm::storage::wasm_parameter;
            preload.image = ::std::move(state.provider_image); inputs.push_back(::std::move(preload));
            auto candidate{full::full_source_instance::create_unparsed(state.path, u8"gc-state-main", ::std::move(inputs))};
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
    if(argc != 4 && argc != 5) { return 64; }
    bool const resumable{argc == 5};
    if(resumable) { REQUIRE(::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[4])) == "resumable"); }
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
    ::fast_io::io::perrln("wasm mutation phase: prepare real full publication");
    REQUIRE(lib::llvm_jit_prepare_debug_host_api()); // SAME authoritative fused validation/lowering, once
    ::fast_io::io::perrln("wasm mutation phase: initialize provider");
    // Ordinary setup initializes globals/table/objects. The later guest
    // participants read them; management writes happen only while BOTH genuinely
    // parked participants and every tracked host/GC entry are excluded.
    lib::full_compile_run_config initialize{}; initialize.entry_function_index = 0u;
    auto const main_id{setup.source->assigned_main_module_id()};
    auto const provider_member{setup.source->registry().find(u8"gc-state-provider")};
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
    lib::full_compile_and_run_main_module(u8"gc-state-provider", initialize);
    ::fast_io::io::perrln("wasm mutation phase: initialize main");
    lib::full_compile_and_run_main_module(u8"gc-state-main", initialize); // real main-owned editable GC objects
    ::fast_io::io::perrln("wasm mutation phase: bind actual source");
    auto const main_binding{lib::llvm_jit_debug_bind_source_host_api(main_id)};
    auto const provider_binding{lib::llvm_jit_debug_bind_source_host_api(provider_id)};
    REQUIRE(main_binding && provider_binding && main_binding.get() != provider_binding.get());
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
        ::fast_io::io::perrln("wasm mutation phase: actual pause attempt ", ::fast_io::mnp::dec(attempts));
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
                lib::full_compile_and_run_main_module(u8"gc-state-main", config);
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
                ws::request query{ws::selection::globals, state->participants[0u], main_id, 0u, 0u, 0u, 7u};
                auto call = [&](auto const& owners, ws::request const& selected)
                { return lib::llvm_jit_debug_query_wasm_state_host_api(ticket, owners, selected); };
                {
                    auto real_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()}; REQUIRE(real_reader);
                    auto refused{call(captured, query)};
                    REQUIRE(refused.result == ws::status::unavailable_gc_roots && refused.rows.empty() && refused.objects.empty());
                } // Retire actual extra reader BEFORE N can be acquired.
                auto observed{call(captured, query)};
                REQUIRE(observed.result == ws::status::available && ws::valid(observed) && observed.rows.size() == 7u);
                REQUIRE(!observed.snapshot_or_restore_authority() && observed.module == main_id);
                auto provider_request{query}; provider_request.module = provider_id;
                auto provider_view{call(captured, provider_request)};
                REQUIRE(provider_view.result == ws::status::available && ws::valid(provider_view) && provider_view.module == provider_id);
                REQUIRE(observed.objects[observed.rows[0u].data.ref.object - 1u].module == provider_id &&
                    provider_view.objects[provider_view.rows[0u].data.ref.object - 1u].module == provider_id);
                // Each real parser file has a distinct type/context allocation.
                // Equal canonical types do NOT collapse owning declarations.
                REQUIRE(setup.source->file().wasm_module_storage.wasm_binfmt_ver1_storage.module_span.module_begin !=
                    provider_file->wasm_module_storage.wasm_binfmt_ver1_storage.module_span.module_begin);
                REQUIRE(observed.rows[0u].data.ref.kind == ws::reference_kind::structure &&
                    observed.rows[0u].data.ref.object == observed.rows[1u].data.ref.object);
                auto const& node{observed.objects[observed.rows[0u].data.ref.object - 1u]};
                REQUIRE(node.kind == ws::object_kind::structure && node.members.size() == 3u &&
                    node.members[0u].data.ref.object == node.identifier && node.members[1u].data.packed_bits == 8u);
                REQUIRE(observed.rows[2u].data.ref.kind == ws::reference_kind::array &&
                    observed.rows[3u].data.ref.kind == ws::reference_kind::exception &&
                    observed.rows[4u].data.ref.kind == ws::reference_kind::external_wrapper);
                auto const& event{observed.objects[observed.rows[3u].data.ref.object - 1u]};
                REQUIRE(event.kind == ws::object_kind::exception && event.tag_identity_available && event.tag_module == provider_id &&
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
                // Actual mutation under the SAME complete two-participant
                // episode; requests/VIEWs alone never acquire write permission.
                auto mutate=[&](wm::request const& edit)
                { return lib::llvm_jit_debug_mutate_wasm_state_host_api(ticket,captured,edit); };
                auto original=[&](::std::uint64_t destination,ws::selection kind,::std::uint64_t module,
                    ::std::uint64_t first,::std::uint64_t frame=0u,::std::uint64_t table=0u)
                {
                    wm::request edit{};edit.participant=state->participants[0u];edit.module=main_id;edit.index=destination;
                    edit.source=wm::source_kind::original_root;
                    edit.original={kind,edit.participant,module,frame,table,first,1u};return edit;
                };
                auto read_global=[&](::std::uint64_t index)
                {
                    ws::request read{ws::selection::globals,state->participants[0u],main_id,0u,0u,index,1u};
                    auto value{call(captured,read)};REQUIRE(value.result==ws::status::available&&ws::valid(value)&&value.rows.size()==1u);
                    return value;
                };
                // Genuine GC fields/arrays under the SAME complete actual
                // two-participant episode. Every target starts at original
                // typed roots; no observed dense object ID is supplied back.
                auto member=[&](::std::uint64_t root,::std::uint64_t index)
                {
                    wm::request edit{};edit.target=wm::destination::member;edit.participant=state->participants[0u];edit.element=index;
                    edit.target_original={ws::selection::globals,edit.participant,main_id,0u,0u,root,1u};
                    edit.target_original.member_count=1u;return edit;
                };
                auto read_member=[&](::std::uint64_t root,::std::uint64_t index)
                {
                    ws::request read{ws::selection::globals,state->participants[0u],main_id,0u,0u,root,1u};
                    read.member_first=index;read.member_count=1u;
                    auto view{call(captured,read)};REQUIRE(view.result==ws::status::available&&ws::valid(view)&&view.selected_object!=0u);
                    auto found{::std::find_if(view.objects.begin(),view.objects.end(),[&](auto const& object){return object.identifier==view.selected_object;})};
                    REQUIRE(found!=view.objects.end()&&found->members.size()==1u&&found->members.front().index==index);
                    return found->members.front().data;
                };
                auto packed_field{member(0u,1u)};packed_field.source=wm::source_kind::numeric_bits;packed_field.numeric_kind=ws::value_kind::i32;
                packed_field.bits[0u]=::std::byte{0xffu};packed_field.bits[1u]=::std::byte{1u};
                auto field_reply{mutate(packed_field)};
                REQUIRE(field_reply.applied&&field_reply.target==wm::destination::member&&field_reply.module==provider_id&&
                    field_reply.element==1u&&read32_lane(read_member(0u,1u),0u)==255u);
                packed_field.bits[0u]=::std::byte{127u};packed_field.bits[1u]=::std::byte{};
                REQUIRE(mutate(packed_field).applied&&read32_lane(read_member(0u,1u),0u)==127u);
                auto packed_array{member(26u,1u)};packed_array.source=wm::source_kind::numeric_bits;packed_array.numeric_kind=ws::value_kind::i32;
                packed_array.bits[0u]=::std::byte{0x45u};packed_array.bits[1u]=::std::byte{0x23u};packed_array.bits[2u]=::std::byte{1u};
                REQUIRE(mutate(packed_array).applied&&read32_lane(read_member(26u,1u),0u)==0x2345u);
                auto numeric_array{member(2u,1u)};numeric_array.source=wm::source_kind::numeric_bits;numeric_array.numeric_kind=ws::value_kind::i64;
                numeric_array.bits[0u]=::std::byte{99u};REQUIRE(mutate(numeric_array).applied&&read64(read_member(2u,1u))==99u);
                numeric_array.bits[0u]=::std::byte{20u};REQUIRE(mutate(numeric_array).applied&&read64(read_member(2u,1u))==20u);
                for(auto pair : ::std::array<::std::array<::std::uint64_t,2u>,5u>{{{{0u,0u}},{{1u,1u}},{{2u,2u}},{{3u,3u}},{{4u,4u}}}})
                {
                    auto edit{member(24u,pair[0u])};edit.source=wm::source_kind::numeric_bits;
                    edit.numeric_kind=static_cast<ws::value_kind>(pair[1u]);
                    auto const width{pair[1u]==0u || pair[1u]==2u ? 4u : pair[1u]==4u ? 16u : 8u};
                    for(::std::size_t i{};i!=width;++i){edit.bits[i]=::std::byte{0xffu};}
                    REQUIRE(mutate(edit).applied&&read_member(24u,pair[0u]).bits==edit.bits); // raw NaNs/v128 exact, no FP evaluation
                }
                auto field_from=[&](::std::uint64_t target,::std::uint64_t field,::std::uint64_t source)
                {
                    auto edit{member(target,field)};edit.source=wm::source_kind::original_root;
                    edit.original={ws::selection::globals,edit.participant,provider_id,0u,0u,source,1u};return edit;
                };
                auto foreign_compact{field_from(24u,9u,10u)};REQUIRE(mutate(foreign_compact).applied&&
                    read_member(24u,9u).ref.kind==ws::reference_kind::structure);
                auto compact_array{field_from(25u,0u,10u)};REQUIRE(mutate(compact_array).applied&&
                    read_member(25u,0u).ref.kind==ws::reference_kind::structure);
                // Actual source compact belongs to provider, target object to
                // main. This qualifies source/type/lifetime branch execution,
                // not a foreign-only collection/reclaim proof.
                for(auto pair : ::std::array<::std::array<::std::uint64_t,2u>,2u>{{{{5u,3u}},{{6u,4u}}}})
                { auto edit{field_from(24u,pair[0u],pair[1u])};REQUIRE(mutate(edit).applied&&read_member(24u,pair[0u]).ref.kind==provider_view.rows[pair[1u]].data.ref.kind); }
                auto function_field{member(24u,7u)};function_field.source=wm::source_kind::function;
                function_field.function_module=main_id;function_field.function_index=2u;
                REQUIRE(mutate(function_field).applied&&read_member(24u,7u).ref.function_index==2u);
                auto i31_field{member(24u,8u)};i31_field.source=wm::source_kind::i31;i31_field.i31_bits=0x7fffffffu;
                REQUIRE(mutate(i31_field).applied&&read_member(24u,8u).ref.i31_bits==0x7fffffffu);
                auto cycle{member(0u,0u)};REQUIRE(mutate(cycle).applied&&read_member(0u,0u).ref.kind==ws::reference_kind::null);
                cycle.source=wm::source_kind::original_root;cycle.original={ws::selection::globals,cycle.participant,main_id,0u,0u,0u,1u};
                REQUIRE(mutate(cycle).applied&&read_member(0u,0u).ref.kind==ws::reference_kind::structure); // restore actual self-cycle
                auto immutable_field{member(0u,2u)};immutable_field.source=wm::source_kind::numeric_bits;immutable_field.numeric_kind=ws::value_kind::v128;
                auto refusal{mutate(immutable_field)};REQUIRE(!refusal.applied&&refusal.reason==wm::refusal::immutable_member&&
                    read32_lane(read_member(0u,2u),0u)==1u);
                auto immutable_compact{member(10u,0u)};immutable_compact.source=wm::source_kind::numeric_bits;
                refusal=mutate(immutable_compact);REQUIRE(!refusal.applied&&refusal.reason==wm::refusal::immutable_member&&read32_lane(read_member(10u,0u),0u)==37u);
                auto immutable_exn{member(3u,0u)};refusal=mutate(immutable_exn);
                REQUIRE(!refusal.applied&&refusal.reason==wm::refusal::immutable_member&&read32_lane(read_member(3u,0u),0u)==77u);
                auto wrong_field{field_from(24u,9u,2u)};REQUIRE(!mutate(wrong_field).applied&&read_member(24u,9u).ref.kind==ws::reference_kind::structure);
                auto bad_field{numeric_array};bad_field.element=UINT64_MAX;REQUIRE(mutate(bad_field).status==ws::status::out_of_range&&read64(read_member(2u,1u))==20u);
                bad_field=numeric_array;bad_field.target_original.first=UINT64_MAX;REQUIRE(!mutate(bad_field).applied&&read64(read_member(2u,1u))==20u);
                auto labelled{member(0u,1u)};labelled.target=wm::destination::member_path;labelled.target_original={};
                labelled.target_path_session=1u;labelled.target_path_handle=1u;
                REQUIRE(wm::valid(labelled)&&mutate(labelled).status==ws::status::invalid_selection&&read32_lane(read_member(0u,1u),0u)==127u);
                {
                    auto unrelated{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()};REQUIRE(unrelated);
                    REQUIRE(mutate(packed_field).status==ws::status::unavailable_gc_roots);
                }
                REQUIRE(!lib::llvm_jit_debug_mutate_wasm_state_host_api(ticket,{captured.data(),1u},packed_field).applied&&
                    read32_lane(read_member(0u,1u),0u)==127u);
                // Actual foreign compact numeric struct -> main defined root;
                // this must not reenter shared-N while exclusive is owned.
                auto compact{original(11u,ws::selection::globals,provider_id,10u)};
                auto compact_reply{mutate(compact)};REQUIRE(compact_reply.applied&&compact_reply.module==main_id&&compact_reply.index==11u);
                auto compact_value{read_global(11u)};
                REQUIRE(compact_value.rows[0u].data.ref.kind==ws::reference_kind::structure&&
                    read32_lane(compact_value.objects.front().members.front().data,0u)==37u);
                for(auto pair : ::std::array<::std::array<::std::uint64_t,2u>,4u>{{{{12u,0u}},{{13u,2u}},{{14u,3u}},{{15u,4u}}}})
                {
                    auto edit{original(pair[0u],ws::selection::globals,provider_id,pair[1u])};
                    REQUIRE(mutate(edit).applied);auto copy{read_global(pair[0u])};
                    REQUIRE(copy.rows[0u].data.ref.kind==provider_view.rows[pair[1u]].data.ref.kind);
                }
                auto path{original(12u,ws::selection::globals,main_id,0u)};
                path.original.member_count=1u;path.original.path_size=1u;path.original.path[0u]=0u;
                REQUIRE(mutate(path).applied); // authentic GC self-cycle field, no dense ID input.
                auto packed{original(16u,ws::selection::globals,main_id,0u)};
                packed.original.member_count=1u;packed.original.path_size=1u;packed.original.path[0u]=1u;
                REQUIRE(mutate(packed).applied&&read32_lane(read_global(16u).rows.front().data,0u)==127u);
                // Exact4096 original field indices through the genuine GC
                // self-cycle; the actual ticket/capture/manager authenticates
                // every native read/write. The separate ledger is DATA only.
                wp::stop_key path_key{};path_key.stop=attempts+1u;path_key.runtime_epoch=observed.runtime_epoch;
                // Episode number is detached DATA, not a controller stop
                // credential. Actual locations/generations come from genuine
                // parked participants; ticket/captures remain native authority.
                for(auto const& actual:census.participants)
                {
                    lib::llvm_jit_debug_source_position position{};
                    REQUIRE(lib::llvm_jit_debug_source_position_host_api(main_binding,state->control,ticket,actual.id,position)&&
                        position.module==actual.location.code_unit&&position.function==actual.location.function&&
                        position.runtime_epoch==actual.location.code_generation&&position.function_generation!=0u);
                    path_key.cohort.push_back({actual.id,actual.location.code_unit,actual.location.function,
                        actual.location.offset,actual.location.code_generation,position.function_generation});
                }
                ::std::sort(path_key.cohort.begin(),path_key.cohort.end(),
                    [](auto const& a,auto const& b){return a.participant<b.participant;});
                REQUIRE(wp::valid(path_key));wp::ledger paths{};
                wp::request create{};create.action=wp::operation::create;
                create.root={ws::selection::globals,state->participants[0u],main_id,0u,0u,0u,1u};create.root.member_count=1u;
                auto candidate{paths.prepare(create,path_key)};REQUIRE(candidate.data_prepared);
                REQUIRE(call(captured,candidate.query).result==ws::status::available);
                auto path_label{paths.commit(candidate,path_key)};REQUIRE(path_label.result==ws::status::available);
                wp::request extend{};extend.action=wp::operation::extend;extend.suffix_size=16u;
                for(::std::size_t depth{};depth!=4080u;depth+=16u)
                {
                    extend.session=path_label.session;extend.handle=path_label.handle;
                    auto next{paths.prepare(extend,path_key)};REQUIRE(next.data_prepared);
                    auto current{call(captured,next.query)};
                    REQUIRE(current.result==ws::status::available&&ws::valid(current)&&current.objects.size()<=2u);
                    auto previous{path_label};path_label=paths.commit(next,path_key);
                    REQUIRE(path_label.result==ws::status::available&&path_label.depth==depth+16u&&
                        !paths.prepare_value(previous.session,previous.handle,{},path_key).data_prepared);
                }
                ::std::array<::std::uint64_t,16u> packed_suffix{};packed_suffix.back()=1u;
                auto packed_source{paths.prepare_value(path_label.session,path_label.handle,packed_suffix,path_key)};
                REQUIRE(packed_source.data_prepared&&packed_source.query.long_path.size()==4096u);
                auto deep{original(16u,ws::selection::globals,main_id,0u)};deep.original=::std::move(packed_source.query);
                REQUIRE(mutate(deep).applied&&read32_lane(read_global(16u).rows.front().data,0u)==127u);
                extend.session=path_label.session;extend.handle=path_label.handle;
                candidate=paths.prepare(extend,path_key);REQUIRE(candidate.data_prepared);
                auto cycle_page{call(captured,candidate.query)};
                REQUIRE(cycle_page.result==ws::status::available&&ws::valid(cycle_page)&&
                    cycle_page.selected_object!=0u&&cycle_page.objects.size()<=2u);
                path_label=paths.commit(candidate,path_key);REQUIRE(path_label.result==ws::status::available&&path_label.depth==4096u);
                auto full_source{paths.prepare_value(path_label.session,path_label.handle,{},path_key)};
                REQUIRE(full_source.data_prepared);
                auto deep_target{member(0u,1u)};deep_target.target_original=full_source.query;
                deep_target.source=wm::source_kind::numeric_bits;deep_target.numeric_kind=ws::value_kind::i32;deep_target.bits[0u]=::std::byte{0x66u};
                REQUIRE(mutate(deep_target).applied&&read32_lane(read_member(0u,1u),0u)==0x66u);
                deep_target.bits[0u]=::std::byte{127u};REQUIRE(mutate(deep_target).applied&&read32_lane(read_member(0u,1u),0u)==127u);
                deep_target.target_original.long_path.back()=UINT64_MAX;
                REQUIRE(!mutate(deep_target).applied&&read32_lane(read_member(0u,1u),0u)==127u);deep.index=12u;deep.original=::std::move(full_source.query);
                REQUIRE(mutate(deep).applied&&read_global(12u).rows.front().data.ref.kind==ws::reference_kind::structure);
                ::std::array<::std::uint64_t,1u> extra_edge{};
                REQUIRE(!paths.prepare_value(path_label.session,path_label.handle,extra_edge,path_key).data_prepared&&
                    paths.prepare_value(path_label.session,path_label.handle,{},path_key).data_prepared);
                auto changed_key{path_key};++changed_key.stop;
                REQUIRE(!paths.prepare_value(path_label.session,path_label.handle,{},changed_key).data_prepared);
                paths.clear();REQUIRE(!paths.prepare_value(path_label.session,path_label.handle,{},path_key).data_prepared);
                deep.index=16u;deep.original.long_path.back()=1u;
                // Labels alone reach neither original-root resolution nor a
                // naked function-literal fallback in the native API.
                wm::request unresolved{};unresolved.participant=state->participants[0u];unresolved.module=main_id;unresolved.index=12u;
                unresolved.source=wm::source_kind::original_path;unresolved.path_session=path_label.session;unresolved.path_handle=path_label.handle;
                REQUIRE(wm::valid(unresolved)&&mutate(unresolved).status==ws::status::invalid_selection&&
                    read_global(12u).rows.front().data.ref.kind==ws::reference_kind::structure);
                // Out-of-range field/overlong path leaves the target unchanged.
                deep.original.long_path.back()=UINT64_MAX;REQUIRE(!mutate(deep).applied&&
                    read32_lane(read_global(16u).rows.front().data,0u)==127u);
                deep.original.long_path.back()=1u;deep.original.long_path.push_back(0u);
                REQUIRE(mutate(deep).status==ws::status::invalid_selection&&
                    read32_lane(read_global(16u).rows.front().data,0u)==127u);
                auto local{original(12u,ws::selection::locals,0u,0u)};
                REQUIRE(mutate(local).applied); // captured nondefaultable actual node local.
                auto operand{original(12u,ws::selection::operands,0u,1u)};
                REQUIRE(mutate(operand).applied);
                // Real hidden if-entry parameter from the SAME fused producer;
                // lexical metadata or a copied operand never substitutes it.
                ws::request saved_request{ws::selection::saved_parameters,state->participants[0u],0u,0u,0u,0u,1u};
                auto saved_source{call(captured,saved_request)};
                REQUIRE(saved_source.result==ws::status::available&&ws::valid(saved_source)&&saved_source.rows.size()==1u&&
                    saved_source.rows.front().data.ref.kind==ws::reference_kind::structure);
                auto saved{original(12u,ws::selection::saved_parameters,0u,0u)};
                REQUIRE(mutate(saved).applied);
                for(auto pair : ::std::array<::std::array<::std::uint64_t,2u>,4u>{{{{16u,0u}},{{17u,1u}},{{18u,2u}},{{19u,3u}}}})
                {
                    wm::request edit{};edit.participant=state->participants[0u];edit.module=main_id;edit.index=pair[0u];
                    edit.source=wm::source_kind::numeric_bits;edit.numeric_kind=static_cast<ws::value_kind>(pair[1u]);
                    // Numeric bits are wire LE and may contain a signalling NaN;
                    // no float evaluation/canonicalization is permitted.
                    auto const width{pair[1u]==0u || pair[1u]==2u ? 4u:8u};
                    for(::std::size_t byte{};byte!=width;++byte) { edit.bits[byte]=::std::byte{0xffu}; }
                    REQUIRE(mutate(edit).applied&&read_global(pair[0u]).rows.front().data.bits==edit.bits);
                }
                wm::request vector{};vector.participant=state->participants[0u];vector.module=main_id;vector.index=20u;
                vector.source=wm::source_kind::numeric_bits;vector.numeric_kind=ws::value_kind::v128;
                for(::std::size_t i{};i!=16u;++i){vector.bits[i]=static_cast<::std::byte>(i);}
                REQUIRE(mutate(vector).applied&&read_global(20u).rows.front().data.bits==vector.bits);
                wm::request small{};small.participant=state->participants[0u];small.module=main_id;small.index=21u;
                small.source=wm::source_kind::i31;small.i31_bits=0x7fffffffu;
                REQUIRE(mutate(small).applied&&read_global(21u).rows.front().data.ref.i31_bits==0x7fffffffu);
                wm::request function{};function.participant=state->participants[0u];function.module=main_id;function.index=22u;
                function.source=wm::source_kind::function;function.function_module=main_id;function.function_index=2u;
                REQUIRE(mutate(function).applied&&read_global(22u).rows.front().data.ref.function_index==2u);
                function.target=wm::destination::table;function.index=1u;function.element=1u;
                auto ftable_reply{mutate(function)};REQUIRE(ftable_reply.applied&&ftable_reply.module==main_id&&ftable_reply.index==1u);
                ws::request ftable{ws::selection::table,state->participants[0u],main_id,0u,1u,1u,1u};
                auto function_table{call(captured,ftable)};REQUIRE(function_table.result==ws::status::available&&function_table.rows.size()==1u&&function_table.rows.front().data.ref.function_index==2u);
                auto shared_table{original(0u,ws::selection::globals,provider_id,0u)};
                shared_table.target=wm::destination::table;shared_table.index=0u;shared_table.element=1u;
                auto alias_reply{mutate(shared_table)};REQUIRE(alias_reply.applied&&alias_reply.module==provider_id&&alias_reply.index==0u);
                ws::request table_alias{ws::selection::table,state->participants[0u],provider_id,0u,0u,1u,1u};
                auto shared_alias{call(captured,table_alias)};REQUIRE(shared_alias.result==ws::status::available&&shared_alias.rows.size()==1u&&shared_alias.rows.front().data.ref.kind==ws::reference_kind::structure);
                // Failed mutations must leave the actual destination unchanged.
                auto wrong{original(12u,ws::selection::globals,provider_id,2u)};
                REQUIRE(!mutate(wrong).applied&&read_global(12u).rows.front().data.ref.kind==ws::reference_kind::structure);
                wm::request immutable{};immutable.participant=state->participants[0u];immutable.module=main_id;immutable.index=5u;
                immutable.source=wm::source_kind::numeric_bits;immutable.numeric_kind=ws::value_kind::i64;
                auto refused_const{mutate(immutable)};REQUIRE(!refused_const.applied&&refused_const.reason==wm::refusal::immutable_global);
                REQUIRE(read64(read_global(5u).rows.front().data)==0xffffffffffffffffull);
                auto missing{compact};missing.original.first=UINT64_MAX;
                REQUIRE(!mutate(missing).applied);
                shared_table.element=UINT64_C(4294967296);
                REQUIRE(mutate(shared_table).status==ws::status::out_of_range);
                REQUIRE(!lib::llvm_jit_debug_mutate_wasm_state_host_api(ticket,{captured.data(),1u},compact).applied);
                {
                    auto unrelated{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()};REQUIRE(unrelated);
                    REQUIRE(mutate(compact).status==ws::status::unavailable_gc_roots);
                }
                ::std::shared_ptr<void const> fake_owner{::std::make_shared<int const>(1)};
                lib::llvm_jit_checkpoint_thread_capture_owner fake{fake_owner,captured.front().get()};
                auto fake_captures{captured};fake_captures.front()=fake;
                REQUIRE(!lib::llvm_jit_debug_mutate_wasm_state_host_api(ticket,fake_captures,compact).applied);
                wm::request armed{};armed.participant=state->participants[0u];armed.module=main_id;armed.index=23u;
                armed.source=wm::source_kind::numeric_bits;armed.numeric_kind=ws::value_kind::i32;armed.bits[0u]=::std::byte{1u};
                REQUIRE(mutate(armed).applied); // guest consumes changed refs only after real complete batch.
                // EVERY member query reborrows an original root and real path
                // under the SAME complete2 cohort + hostclose + actual N proof.
                auto page_request = [&](::std::uint64_t root_index, ::std::uint64_t first_member, ::std::uint64_t count)
                {
                    ws::request page{ws::selection::globals, state->participants[0u], main_id, 0u, 0u, root_index, 1u};
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
            ws::request after_resume{ws::selection::globals, state->participants[0u], main_id, 0u, 0u, 0u, 1u};
            auto resumed{lib::llvm_jit_debug_query_wasm_state_host_api(ticket, first, after_resume)};
            REQUIRE(resumed.result != ws::status::available && resumed.rows.empty() && resumed.objects.empty());
            wm::request stale_edit{};stale_edit.participant=state->participants[0u];stale_edit.module=main_id;stale_edit.index=16u;
            stale_edit.source=wm::source_kind::numeric_bits;stale_edit.numeric_kind=ws::value_kind::i32;
            REQUIRE(!lib::llvm_jit_debug_mutate_wasm_state_host_api(ticket,first,stale_edit).applied);
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
    ::fast_io::print(::fast_io::out(), "DEBUG_WASM_MUTATION policy=", policy,
        " actual_census=2 owned_modules=2 imported_alias_tag_table=1 positive_episodes=", ::fast_io::mnp::dec(positives), " attempts=", ::fast_io::mnp::dec(attempts),
        " actual_typed_mutations=1 foreign_compact_root=1 resumed_guest_ref_use=1 original_member_pages=1 deep_cycle4096=1 native_unresolved_handle_refused=1 old_episode_refused=1 whole_restore=0\n");
}

#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <uwvm2/utils/macro/pop_macros.h>
