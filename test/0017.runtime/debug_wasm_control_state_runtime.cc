// Actual current full LLVM JIT: every real instruction stop is captured,
// the manager then performs a coherent rooted typed query while fully parked.
// Use fresh runtime/main/host objects with this exact source and explicit
// instruction/unwind strategy. No previous runtime object can qualify it.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/utils/container/string_concat.h>
#include <uwvm2/uwvm/debugger/wasm_state.h>
#include <fast_io.h>
#include <algorithm>
#include <atomic>
#include <vector>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
namespace lib=::uwvm2::runtime::lib;
namespace mode=::uwvm2::uwvm::runtime::runtime_mode;
namespace threads=::uwvm2::utils::thread;
namespace cp=::uwvm2::runtime::checkpoint;
namespace ws=::uwvm2::uwvm::debugger::wasm_state;
using domain=threads::cooperative_pause_domain;
static void require(bool valid,char const* message)
{ if(!valid) { ::fast_io::io::perrln("control-state observation: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static auto deadline() { return ::std::chrono::steady_clock::now()+::std::chrono::seconds{20}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(1u)};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    domain::pause_ticket ticket{}; lib::llvm_jit_checkpoint_thread_capture_owner capture{};
    lib::llvm_jit_checkpoint_recording_observation recording{};
    threads::cooperative_pause_location location{};
    ::std::uint_least64_t participant{}, sequence{};
    bool finished{};
    static void point(void* opaque,::std::uint_least64_t who,threads::cooperative_pause_location where) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};
        auto ticket{self.control->request_pause()}; require(bool(ticket),"actual per-instruction pause request");
        ::std::lock_guard lock{self.mutex}; self.participant=who; self.location=where; self.ticket=::std::move(ticket);
    }
    static void before_park(void* opaque,::std::uint_least64_t who,threads::cooperative_pause_location where,
        lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)}; ::std::lock_guard lock{self.mutex};
        require(who==self.participant && where==self.location && self.ticket,"same actual callback participant/ticket/source location");
        auto saved{lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket)};
        require(saved.status==lib::llvm_jit_checkpoint_capture_status::captured && saved.capture,"complete real nested-control/EH/parent typed materialization");
        self.recording=lib::llvm_jit_observe_checkpoint_recording_host_api();
        require(self.recording.instrumented && self.recording.at_current_opcode && self.recording.status==cp::status::ok,
            "current actual ordinal and typed packet, not a previous source point");
        self.capture=::std::move(saved.capture); ++self.sequence; self.changed.notify_all();
    }
};
int main(int argc,char** argv)
{
    if(argc!=4) { return 2; }
    auto const strategy{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[2]))};
    auto const fixture{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[3]))};
    require(strategy=="instruction" || strategy=="unwind","explicit actual stack strategy");
    require(fixture=="nested" || fixture=="saved-gc" || fixture=="loop-first" || fixture=="identity-if","explicit fixture");
    mode::global_runtime_mode=mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler=mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack=strategy=="instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads=0u; mode::runtime_compile_threads_existed=true;
    mode::global_runtime_llvm_jit_cache_path_mode=mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto const path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& arguments{::uwvm2::uwvm::cmdline::parsing_result};
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=nullptr; arguments.clear();
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{u8"nested-observation",nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    arguments.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())},nullptr,
        ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // [complete stable argument vector] end
    // [safe] no later growth; borrow original last file argument only now.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=::std::addressof(arguments.back());
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_gc=false; features.explicit_enable_gc=true;
    features.disable_function_references=false; features.explicit_enable_function_references=true;
    features.disable_reference_types=false; features.explicit_enable_reference_types=true;
    features.disable_exceptions=false; features.explicit_enable_exceptions=true;
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name=u8"nested-observation";
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true)==static_cast<int>(::uwvm2::uwvm::run::retval::ok),"actual official-validated modern input parsed/initialized once");
    auto state{::std::make_shared<observer>()};
    require(lib::llvm_jit_configure_debug_session_host_api(state->control,{state,observer::point,observer::before_park},
        lib::llvm_jit_debug_safe_point_granularity::instruction)==lib::llvm_jit_debug_configure_result::ok,"real reserved debugger");
    require(lib::llvm_jit_configure_debug_value_observation_host_api()==lib::llvm_jit_debug_configure_result::ok,"observer-only selection before any publication");
    require(lib::llvm_jit_prepare_debug_host_api(),"same fused full validate+translate, complete actual native entries");
    ::std::uint32_t original{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config run{}; run.entry_function_index=0u;
        // [main-owned exact four-byte native result storage] end
        // [safe] actual owner lives until guest joins; no serialized pointer.
        run.entry_abi_buffers.result_buffer=reinterpret_cast<::std::byte*>(::std::addressof(original));
        run.entry_abi_buffers.result_bytes=sizeof(original);
        lib::full_compile_and_run_main_module(u8"nested-observation",run);
        ::std::lock_guard lock{state->mutex}; state->finished=true; state->changed.notify_all();
    }};
    ::std::uint_least64_t consumed{}; ::std::size_t queried{}, nested_calls{}, exceptions{}, controls{}, handlers{}, saved_values{}, rich_types{};
    ::std::vector<::std::size_t> nested_offsets{}; bool repeated_nested_point{};
    for(;;)
    {
        domain::pause_ticket ticket{}; lib::llvm_jit_checkpoint_thread_capture_owner capture{};
        ::std::uint_least64_t participant{}; lib::llvm_jit_checkpoint_recording_observation observed{};
        threads::cooperative_pause_location location{};
        {
            ::std::unique_lock lock{state->mutex};
            require(state->changed.wait_until(lock,deadline(),[&]{return state->sequence>consumed || state->finished;}),"finite real pause/completion");
            if(state->finished && state->sequence==consumed) { break; }
            consumed=state->sequence; ticket=state->ticket; capture=state->capture; participant=state->participant; observed=state->recording; location=state->location;
        }
        require(state->control->wait_until_paused(ticket,deadline())==threads::cooperative_pause_result::paused,"actual cohort fully parked before management query");
        if(observed.native_frames>1u) { ++nested_calls; }
        if(fixture=="nested" && location.function==2u)
        {
            auto const offset{static_cast<::std::size_t>(location.offset)};
            if(::std::find(nested_offsets.begin(),nested_offsets.end(),offset)!=nested_offsets.end()) { repeated_nested_point=true; }
            else { nested_offsets.push_back(offset); }
        }
        for(auto selected : {ws::selection::locals,ws::selection::operands})
        {
            ws::request request{}; request.selected=selected; request.participant=participant; request.count=ws::maximum_rows;
            auto const view{lib::llvm_jit_debug_query_wasm_state_host_api(ticket,{::std::addressof(capture),1u},request)};
            require(view.result==ws::status::available && ws::valid(view),"actual stopped full typed local/operand query including nested/EH sites");
            for(auto const& object : view.objects) { if(object.kind==ws::object_kind::exception) { ++exceptions; } }
            ++queried;
        }
        for(auto selected : {ws::selection::controls,ws::selection::handlers,ws::selection::saved_parameters})
        {
            ws::request request{};request.selected=selected;request.participant=participant;request.count=64u;
            auto const data{lib::llvm_jit_debug_query_wasm_state_host_api(ticket,{::std::addressof(capture),1u},request)};
            require(data.result==ws::status::available&&ws::valid(data),"real authenticated control/handler/saved parameter page");
            controls+=data.controls.size();handlers+=data.handlers.size();saved_values+=data.rows.size();
            for(auto const& control : data.controls)
            {
                for(auto types : {ws::selection::control_parameters,ws::selection::control_results})
                {
                    request.selected=types;request.index=control.index;
                    auto const tuple{lib::llvm_jit_debug_query_wasm_state_host_api(ticket,{::std::addressof(capture),1u},request)};
                    require(tuple.result==ws::status::available&&ws::valid(tuple)&&tuple.total_values==
                        (types==ws::selection::control_parameters?control.parameter_count:control.result_count),"same actual control tuple types and declared count");
                    for(auto const& type : tuple.declarations){if(type.type.kind==ws::value_kind::reference){++rich_types;}}
                    request.first=tuple.total_values;
                    auto const end{lib::llvm_jit_debug_query_wasm_state_host_api(ticket,{::std::addressof(capture),1u},request)};
                    require(end.result==ws::status::available&&end.declarations.empty(),"real exact empty final declaration page");
                    request.first=tuple.total_values+1u;
                    require(lib::llvm_jit_debug_query_wasm_state_host_api(ticket,{::std::addressof(capture),1u},request).result==ws::status::out_of_range,
                        "actual declaration first checked before pointer/index use");request.first=0u;
                }
            }
            for(auto const& handler : data.handlers)
            {
                request.selected=ws::selection::handler_parameters;request.index=handler.index;
                auto const tuple{lib::llvm_jit_debug_query_wasm_state_host_api(ticket,{::std::addressof(capture),1u},request)};
                require(tuple.result==ws::status::available&&ws::valid(tuple)&&tuple.total_values==handler.parameter_count,
                    "real handler payload declaration without fabricated native caught state");
            }
            request.selected=selected;request.index=0u;
        }
        require(state->control->resume(ticket),"resume same actual pause episode");
    }
    guest.join(); require(original==(fixture=="nested" ? 316u : fixture=="saved-gc" ? 7u : 42u),"actual normal guest result independent of debugger");
    require(queried>=6u && (fixture!="nested" || (nested_calls>=8u && exceptions!=0u && repeated_nested_point)),"real modern nested calls/catch_ref/throw_ref state reached");
    require(controls!=0u && (fixture!="nested" || handlers!=0u) && (fixture!="saved-gc" || (saved_values!=0u&&rich_types!=0u)),
        "real producer generated modern nested/EH/saved reference metadata and owning values");
    lib::reset_runtime_state_host_api();
    ::fast_io::io::println("control-state actual full JIT PASS queries=",::fast_io::mnp::dec(queried),
        " nested-call-stops=",::fast_io::mnp::dec(nested_calls)," exn-objects=",::fast_io::mnp::dec(exceptions)," restore=false");
}
