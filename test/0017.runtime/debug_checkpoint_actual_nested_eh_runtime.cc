// Actual original parse/initializer/single fused compiler/native pause ->
// retire+drain -> restored native parent/child -> guest catch_all_ref/catch_ref ->
// new current stops -> throw_ref with GC payload. No file/new-world restore claim.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/uwvm/debugger/wasm_state.h>
#include <uwvm2/utils/container/string_concat.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
namespace lib = ::uwvm2::runtime::lib;
namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
namespace threads = ::uwvm2::utils::thread;
namespace cp = ::uwvm2::runtime::checkpoint;
namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
using domain = threads::cooperative_pause_domain;
static void require(bool valid, char const* message)
{
    if(!valid)
    { ::fast_io::io::perrln("checkpoint actual nested EH: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static auto deadline() { return ::std::chrono::steady_clock::now()+::std::chrono::seconds{20}; }
// Select only a PC for this exact official-assembler fixture. These copied
// diagnostic bytes never authenticate a frame, source, reference or restore.
// Actual point + current-ticket before-park capture are still the sole issuer.
static ::std::uint64_t unique_nop_run(lib::llvm_jit_debug_source_image const& image, ::std::size_t wanted, ::std::uint64_t selected_function=0u)
{
    bool found_function{}, found{}; ::std::uint64_t result{};
    for(auto const& function : image.functions)
    {
        if(function.function != selected_function) { continue; }
        require(!found_function,"one exact root source function"); found_function=true;
        auto const& bytes{function.expression_bytes}; require(!bytes.empty(),"actual owned diagnostic expression bytes");
        for(::std::size_t index{}; index != bytes.size();)
        {
            // [complete owned copied expression0..N] end
            // [safe] index<N before byte selection; no raw bytecode pointer walk.
            if(bytes[index] != ::std::byte{0x01u}) { ++index; continue; }
            auto const first{index};
            do { ++index; } while(index != bytes.size() && bytes[index] == ::std::byte{0x01u});
            if(index-first == wanted)
            { require(!found,"one unique exact fixture nop marker"); result=first; found=true; }
        }
    }
    require(found_function && found,"exact root fixture nop marker present"); return result;
}
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::atomic_uint phase{};
    ::std::atomic_size_t child_count{};
    ::std::uint64_t first_catch_offset{}, second_catch_offset{}, child_stop_offset{};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    domain::pause_ticket ticket{};
    lib::llvm_jit_checkpoint_thread_capture_owner capture{};
    threads::cooperative_pause_location location{};
    ::std::uint_least64_t participant{};
    bool requested{}, copied{}, finished{};
    static void point(void* opaque,::std::uint_least64_t participant,threads::cooperative_pause_location where) noexcept
    {
        // [actual runtime-retained observer owner] both native runs are live.
        // [safe] this true native context was installed by the trusted manager.
        auto& self{*static_cast<observer*>(opaque)}; auto const phase{self.phase.load(::std::memory_order_acquire)};
        if(phase == 0u)
        { if(where.function != 1u || where.offset != self.child_stop_offset) { return; } }
        else
        { if(where.function != 0u || where.offset != (phase == 1u ? self.first_catch_offset : self.second_catch_offset)) { return; } }
        auto ticket{self.control->request_pause()}; require(bool(ticket),"real current pause request");
        ::std::lock_guard lock{self.mutex}; require(!self.requested,"one actual episode");
        self.ticket=::std::move(ticket);self.location=where;self.participant=participant;self.requested=true;self.changed.notify_all();
    }
    static void before_park(void* opaque,::std::uint_least64_t participant,threads::cooperative_pause_location where,
        lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)}; ::std::lock_guard lock{self.mutex};
        require(self.requested && !self.copied && self.location==where && self.participant==participant,"same actual before-park episode");
        auto saved{lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket)};
        require(saved.status==lib::llvm_jit_checkpoint_capture_status::captured && saved.capture,
            "actual canonical source/engine/plan/current native typed capture");
        auto const actual{lib::llvm_jit_observe_checkpoint_recording_host_api()};
        require(actual.instrumented && actual.at_current_opcode && actual.status==cp::status::ok,
            "genuine fully materialized current opcode packet");
        if(self.phase.load(::std::memory_order_relaxed)==0u)
        { require(actual.native_frames==2u && actual.typed_slots==3u && actual.site==4u,"real uninitialized local, operand and saved if parameter with protected awaiting parent"); }
        else
        { require(actual.native_frames==1u && actual.typed_slots==8u,"real parent locals and live exception operand after guest unwind"); }
        self.capture=::std::move(saved.capture);self.copied=true;self.changed.notify_all();
    }
};
static domain::pause_ticket wait_for_capture(observer& state)
{
    domain::pause_ticket ticket{};
    {
        ::std::unique_lock lock{state.mutex};
        require(state.changed.wait_until(lock,deadline(),[&]{return state.copied || state.finished;}),"finite actual stop event");
        require(state.copied && !state.finished && state.capture,"execution actually parked with current capture");ticket=state.ticket;
    }
    require(state.control->wait_until_paused(ticket,deadline())==threads::cooperative_pause_result::paused,"complete actual cohort parked");return ticket;
}
static ::std::uint32_t u32(ws::value const& value)
{
    require(value.available && value.type.kind==ws::value_kind::i32,"actual canonical i32 value");
    ::std::uint32_t result{};
    // [owned canonical LE numeric bytes16] end
    // [safe] fixed four-byte parse range lies inside complete array BEFORE advance.
    auto const first{reinterpret_cast<unsigned char const*>(value.bits.data())};
    auto const parsed{::fast_io::parse_by_scan(first,first+4u,::fast_io::mnp::le_get<32>(result))};
    require(parsed.code==::fast_io::parse_code::ok && parsed.iter==first+4u,"canonical LE i32 independent of host byte order");return result;
}
static ws::object const& object(ws::view const& view,::std::uint64_t id)
{
    require(id!=0u && id<=view.objects.size(),"bounded copied query-local object ID");
    // [owned query-local objects1..N] end
    // [safe] nonzero complete bound BEFORE subtraction/indexing. No native token.
    return view.objects[static_cast<::std::size_t>(id-1u)];
}
static void prepare_world_frames(domain::pause_ticket const& ticket,
    ::std::span<lib::llvm_jit_checkpoint_thread_capture_owner const> captures, ::std::size_t frames, bool references)
{
    lib::llvm_jit_checkpoint_prepare_request request{};request.recording_label[0u]=::std::byte{0x73u};
    auto const prepared{lib::llvm_jit_checkpoint_prepare_instance_host_api(ticket,captures,request)};
    ::fast_io::print(::fast_io::out(),"WORLD_EH_FRAME_PREPARATION status=",::fast_io::mnp::dec(static_cast<unsigned>(prepared.status)),
        " frame=",::fast_io::mnp::dec(prepared.frame_diagnostic),
        " threads=",::fast_io::mnp::dec(prepared.prepared_threads)," frames=",::fast_io::mnp::dec(prepared.prepared_frames),
        " roots=",::fast_io::mnp::dec(prepared.prepared_root_carriers),"\n");
    require(prepared.status==lib::llvm_jit_checkpoint_prepare_status::prepared_and_discarded &&
        prepared.prepared_threads==1u && prepared.prepared_frames==frames &&
        (!references || prepared.prepared_root_carriers!=0u),
        "genuine lexical handlers, nondefaultable locals and caught reference packets prepare against fresh plans");
}
static void check_caught_state(observer& state,domain::pause_ticket const& ticket, bool rethrown)
{
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner,1u> owners{state.capture};
    prepare_world_frames(ticket,owners,1u,true);
    ws::request request{};request.participant=state.participant;request.frame=0u;request.selected=ws::selection::locals;request.count=7u;
    auto const view{lib::llvm_jit_debug_query_wasm_state_host_api(ticket,owners,request)};
    require(view.result==ws::status::available && view.rows.size()==7u,"ONE current/N-owned real complete root local query");
    require(u32(view.rows[0u].data)==37u,"guest tag scalar payload37 retained");
    auto const& first{view.rows[1u].data};auto const& alias{view.rows[2u].data};
    require(first.available && alias.available && first.ref.kind==ws::reference_kind::structure && alias.ref.kind==ws::reference_kind::structure &&
        first.ref.object!=0u && first.ref.object==alias.ref.object,"genuine GC payload aliases keep ONE actual object");
    auto const& record{object(view,first.ref.object)};
    require(record.kind==ws::object_kind::structure && record.members.size()==1u && u32(record.members[0u].data)==(rethrown?84u:42u),
        "actual rooted GC payload field changes across throw_ref");
    auto const& held{view.rows[3u].data};auto const& held_alias{view.rows[4u].data};auto const& named{view.rows[6u].data};
    require(held.available && held_alias.available && named.available && held.ref.kind==ws::reference_kind::exception &&
        held_alias.ref.kind==ws::reference_kind::exception && named.ref.kind==ws::reference_kind::exception &&
        held.ref.object!=0u && held.ref.object==held_alias.ref.object,"one retained catch_all_ref token has real local aliases");
    for(auto const id : ::std::array<::std::uint64_t,2u>{held.ref.object,named.ref.object})
    {
        auto const& exception{object(view,id)};
        require(exception.kind==ws::object_kind::exception && exception.tag_identity_available && exception.tag_module==0u && exception.tag_index==0u &&
            exception.members.size()==2u && u32(exception.members[0u].data)==37u && exception.members[1u].data.available &&
            exception.members[1u].data.ref.kind==ws::reference_kind::structure && exception.members[1u].data.ref.object==first.ref.object,
            "catch_ref and throw_ref retain canonical tag and exact immutable payload's GC alias");
    }
    if(rethrown)
    { require(view.rows[5u].data.available && view.rows[5u].data.ref.kind==ws::reference_kind::structure &&
        view.rows[5u].data.ref.object==first.ref.object,"second throw_ref catch returns the same mutated aggregate"); }
    request.selected=ws::selection::globals;request.frame=0u;request.first=0u;request.count=1u;
    auto const globals{lib::llvm_jit_debug_query_wasm_state_host_api(ticket,owners,request)};
    require(globals.result==ws::status::available && globals.rows.size()==1u && u32(globals.rows[0u].data)==(rethrown?3u:2u),
        "only guest catches execute; private retirement is never consumed by catch_all_ref");
    request.selected=ws::selection::operands;
    auto const operands{lib::llvm_jit_debug_query_wasm_state_host_api(ticket,owners,request)};
    require(operands.result==ws::status::available && operands.rows.size()==1u && operands.rows[0u].data.available &&
        operands.rows[0u].data.ref.kind==ws::reference_kind::exception,"real current caught exception operand is rooted/materialized");
}
int main(int argc,char** argv)
{
    if(argc!=3) { return 2; }
    auto const policy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]))};require(policy=="instruction" || policy=="unwind","explicit stack strategy");
    mode::global_runtime_mode=mode::runtime_mode_t::full_compile;mode::global_runtime_compiler=mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack=policy=="instruction"?mode::runtime_llvm_jit_call_stack_t::instruction:mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads=0u;mode::runtime_compile_threads_existed=true;
    mode::global_runtime_llvm_jit_cache_path_mode=mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto const path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    // [old argument owner] no source borrow yet; clear its pointer before release.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=nullptr;arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{u8"checkpoint-actual-eh",nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())},nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [complete owning arguments] no further growth. Borrow only after emplacement.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=::std::addressof(arguments.back());
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc=false;features.explicit_enable_gc=true;features.disable_function_references=false;features.explicit_enable_function_references=true;
    features.disable_reference_types=false;features.explicit_enable_reference_types=true;features.disable_exceptions=false;features.explicit_enable_exceptions=true;
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name=u8"checkpoint-actual-eh";
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true)==static_cast<int>(::uwvm2::uwvm::run::retval::ok),"actual immutable source parse and initializer");
    auto state{::std::make_shared<observer>()};auto profile{cp::compilation_profile::create_for_trusted_manager()};require(bool(profile),"actual immutable resumable profile");
    require(lib::llvm_jit_configure_debug_session_host_api(state->control,{state,observer::point,observer::before_park},lib::llvm_jit_debug_safe_point_granularity::instruction)==lib::llvm_jit_debug_configure_result::ok,"actual reserved observer");
    require(lib::llvm_jit_configure_checkpoint_recording_host_api(profile)==lib::llvm_jit_debug_configure_result::ok,"actual precompile checkpoint selection");
    ::uwvm2::runtime::gc::scoped_cli_gc_execution compilation_owner{};
    require(lib::runtime_gc_prepare_cli_collection_host_api()==::uwvm2::runtime::gc::managed_gc_configure_result::requested,"real managed roots reserved before compile");
    require(lib::llvm_jit_prepare_debug_host_api(),"actual single fused validation/translation and ALL native entries resolved");
    auto source_binding{lib::llvm_jit_debug_bind_source_host_api(0u)};require(bool(source_binding),"actual current owned source binding");
    lib::llvm_jit_debug_source_image source{};require(lib::llvm_jit_debug_copy_source_image_host_api(source_binding,source),"actual bounded diagnostic expression copy");
    state->first_catch_offset=unique_nop_run(source,4u);state->second_catch_offset=unique_nop_run(source,5u);
    state->child_stop_offset=unique_nop_run(source,3u,1u);
    ::std::array<::std::byte,4u> original{};original.fill(::std::byte{0xa5u});
    ::std::thread guest{[&]
    {
        ::uwvm2::runtime::gc::scoped_cli_gc_execution actual_original_vm{};
        lib::full_compile_run_config run{};run.entry_function_index=0u;
        // [main-owned exact declared result4] end; remains live through join.
        run.entry_abi_buffers.result_buffer=original.data();run.entry_abi_buffers.result_bytes=original.size();
        lib::full_compile_and_run_main_module(u8"checkpoint-actual-eh",run);
        ::std::lock_guard lock{state->mutex};state->finished=true;state->changed.notify_all();
    }};
    auto const first_ticket{wait_for_capture(*state)};auto const initial{state->capture};
    ::std::array<lib::llvm_jit_checkpoint_thread_capture_owner,1u> originals{initial};
    {
        ws::request request{};request.participant=state->participant;request.frame=0u;
        request.selected=ws::selection::locals;request.count=1u;
        auto const local{lib::llvm_jit_debug_query_wasm_state_host_api(first_ticket,originals,request)};
        require(local.result==ws::status::available && local.rows.size()==1u && !local.rows[0u].data.available &&
            local.rows[0u].data.unavailable==ws::unavailable_reason::not_initialized &&
            local.rows[0u].data.type.kind==ws::value_kind::reference && !local.rows[0u].data.type.nullable,
            "actual nondefaultable child local is uninitialized, not a null reference");
        request.selected=ws::selection::saved_parameters;
        auto const saved{lib::llvm_jit_debug_query_wasm_state_host_api(first_ticket,originals,request)};
        require(saved.result==ws::status::available && saved.rows.size()==1u && saved.rows[0u].data.available &&
            saved.rows[0u].data.type.kind==ws::value_kind::i64,"actual distinct saved if-entry i64 parameter");
        auto const* first{reinterpret_cast<unsigned char const*>(saved.rows[0u].data.bits.data())};::std::uint64_t bits{};
        // [owned fixed16 LE data] complete8-byte range BEFORE end advance.
        auto const parsed{::fast_io::parse_by_scan(first,first+8u,::fast_io::mnp::le_get<64u>(bits))};
        require(parsed.code==::fast_io::parse_code::ok && parsed.iter==first+8u && bits==99u,
            "real saved if-entry parameter is99 at this exact stop");
        ::fast_io::io::println("WORLD_STRUCTURED_INPUT uninitialized_nondefaultable=1 saved_if_parameters=1 bits=99");
    }
    prepare_world_frames(first_ticket,originals,2u,false);
    ::std::mutex watchdog_mutex{};::std::condition_variable watchdog_changed{};bool drained{};
    ::std::thread watchdog{[&]
    { ::std::unique_lock lock{watchdog_mutex};require(watchdog_changed.wait_until(lock,deadline(),[&]{return drained;}),"finite true execution-domain cleanup/drain"); }};
    require(lib::llvm_jit_checkpoint_retire_saved_execution_host_api(first_ticket,originals)==lib::llvm_jit_checkpoint_execution_retirement_status::execution_retired,
        "private control signal passes both real catch_all_ref and named catch without becoming guest EH; actual domain drains");
    { ::std::lock_guard lock{watchdog_mutex};drained=true;watchdog_changed.notify_all(); }watchdog.join();guest.join();
    for(auto const byte:original) { require(byte==::std::byte{0xa5u},"retired execution publishes no host result through guest catches"); }
    {
        ::std::lock_guard lock{state->mutex};state->phase.store(1u,::std::memory_order_release);
        state->requested=state->copied=state->finished=false;state->ticket={};state->capture.reset();
    }
    ::std::array<::std::byte,4u> output{};output.fill(::std::byte{0x5au});
    auto status{lib::llvm_jit_checkpoint_continuation_status::allocation_failed};
    ::std::thread resumed{[&]
    {
        // Deliberately no external CLI TLS classification here. The private
        // actual canonical dispatcher supplies VM managed execution after proof.
        status=lib::llvm_jit_checkpoint_continue_saved_thread_host_api(initial,output.data(),output.size());
        ::std::lock_guard lock{state->mutex};state->finished=true;state->changed.notify_all();
    }};
    auto const caught_ticket{wait_for_capture(*state)};check_caught_state(*state,caught_ticket,false);
    {
        ::std::lock_guard lock{state->mutex};state->phase.store(2u,::std::memory_order_release);
        state->requested=state->copied=false;state->ticket={};state->capture.reset();
    }
    require(state->control->resume(caught_ticket),"resume genuine postcatch episode");
    auto const rethrown_ticket{wait_for_capture(*state)};check_caught_state(*state,rethrown_ticket,true);
    require(state->control->resume(rethrown_ticket),"resume genuine post-throw_ref episode");resumed.join();
    require(status==lib::llvm_jit_checkpoint_continuation_status::continued,"actual parent survives guest catch/ref/rethrow and returns normally");
    ::std::uint32_t result{};
    // [real owning exact native scalar result4] end; fixed width before copy.
    ::fast_io::freestanding::my_memcpy(::std::addressof(result),output.data(),sizeof(result));
    require(result==205u,"payload37 + original mutated object84 + throw_ref payload alias84");
    lib::reset_runtime_state_host_api();
    ::fast_io::io::println("checkpoint_actual_nested_eh: PASS real two-frame retirement/drain/resume, foreign private signal, catch_all_ref/catch_ref/throw_ref, typed exn and GC payload aliases; new_world_restore=false");
}
