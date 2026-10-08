// Real full-JIT wait bridge, real cooperative participant and linked wait.
#include <uwvm2/uwvm/run/owned_source.h>
#include <uwvm2/runtime/checkpoint/materialization.h>
#include <uwvm2/utils/container/string_concat.h>
#include <uwvm2/uwvm/debugger/checkpoint_state.h>
#include <uwvm2/uwvm/debugger/wasm_state.h>
#include <uwvm2/uwvm/debugger/wasm_mutation.h>
#include <fast_io.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
namespace lib=::uwvm2::runtime::lib;
namespace mode=::uwvm2::uwvm::runtime::runtime_mode;
namespace th=::uwvm2::utils::thread;
namespace ws=::uwvm2::uwvm::debugger::wasm_state;
namespace wm=::uwvm2::uwvm::debugger::wasm_mutation;
using domain=th::cooperative_pause_domain;
static void require(bool value,char const* message)
{ if(!value) { ::fast_io::io::perrln("blocked Wasm wait: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static auto deadline() { return ::std::chrono::steady_clock::now()+::std::chrono::seconds{10}; }
struct observer
{
    ::std::shared_ptr<domain> control{::std::make_shared<domain>(2u)};
    ::std::mutex mutex{}; ::std::condition_variable changed{};
    domain::pause_ticket ticket{};
    lib::llvm_jit_checkpoint_thread_capture_owner capture{};
    th::cooperative_pause_location where{};
    ::std::uint64_t participant{},points{},parks{};
    bool wait_reached{},native_available{};
    static void point(void* opaque,::std::uint_least64_t,th::cooperative_pause_location location) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};
        ::std::lock_guard lock{self.mutex}; ++self.points;
        // Fixture's wait follows three constants (finite timeout has a longer LEB).
        if(location.function==0u && location.offset>=6u)
        { self.wait_reached=true;self.changed.notify_all(); }
    }
    static void before_park(void* opaque,::std::uint_least64_t who,th::cooperative_pause_location location,
        lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};::std::lock_guard lock{self.mutex};
        self.where=location;self.participant=who;
        self.native_available=lib::llvm_jit_capture_debug_native_step_site_host_api().valid;
        auto result{lib::llvm_jit_checkpoint_capture_thread_host_api(self.ticket)};
        require(result.status==lib::llvm_jit_checkpoint_capture_status::captured && result.capture,
            "genuine typed observation of the in-flight wait");
        self.capture=::std::move(result.capture);++self.parks;self.changed.notify_all();
    }
};
int main(int argc,char** argv)
{
    if(argc!=4 && argc!=5 && argc!=6 && argc!=7) { return 64; }
    bool const resumable{argc==5 || argc==7};
    bool const exact_widths{argc>=6};
    unsigned address_bits{},compare_bits{};
    if(exact_widths)
    {
        auto parse_width=[&](unsigned index,unsigned& value)
        {
            auto const text{::fast_io::concat_fast_io(::fast_io::mnp::os_c_str(argv[index]))};
            auto const end{text.data()+text.size()};
            auto const parsed{::fast_io::parse_by_scan(text.data(),end,::fast_io::mnp::dec_get<true,true>(value))};
            require(parsed.code==::fast_io::parse_code::ok && parsed.iter==end && (value==32u || value==64u),
                "explicit fixture address/compare widths");
        };
        parse_width(resumable?5u:4u,address_bits);parse_width(resumable?6u:5u,compare_bits);
    }
    if(resumable) { require(::uwvm2::utils::container::concat_uwvm(::fast_io::mnp::os_c_str(argv[4]))=="resumable","explicit immutable profile purpose"); }
    auto strategy{::uwvm2::utils::container::concat_uwvm(::fast_io::mnp::os_c_str(argv[2]))};
    auto action{::uwvm2::utils::container::concat_uwvm(::fast_io::mnp::os_c_str(argv[3]))};
    require(strategy=="instruction" || strategy=="unwind","explicit stack policy");
    require(action=="notify" || action=="timeout","explicit completion");
    mode::global_runtime_mode=mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler=mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack=strategy=="instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_compile_threads=0u;mode::runtime_compile_threads_existed=true;
    mode::global_runtime_llvm_jit_cache_path_mode=mode::runtime_llvm_jit_cache_path_mode_t::disabled;
    auto& features{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(::uwvm2::uwvm::wasm::storage::wasm_parameter.binfmt1_para)};
    features.disable_threads=false;features.explicit_enable_threads=true;
    features.disable_memory64=false;features.explicit_enable_memory64=true;
    auto const path{::uwvm2::utils::container::u8concat_uwvm(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])))};
    auto& args{::uwvm2::uwvm::cmdline::parsing_result};::uwvm2::uwvm::cmdline::wasm_file_ppos=nullptr;args.clear();
    args.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{u8"blocked-wait",nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::dir});
    args.emplace_back(::uwvm2::utils::cmdline::parameter_parsing_results{
        ::uwvm2::utils::container::u8cstring_view{::fast_io::mnp::os_c_str(path.c_str())},nullptr,::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg});
    // Actual argument vector is finalized before its last element is borrowed.
    ::uwvm2::uwvm::cmdline::wasm_file_ppos=::std::addressof(args.back());
    ::uwvm2::uwvm::wasm::storage::execute_wasm.module_name=u8"blocked-wait";
    require(::uwvm2::uwvm::run::prepare_owned_full_cli_source(true)==static_cast<int>(::uwvm2::uwvm::run::retval::ok),"actual owned validated input");
    auto self{::std::make_shared<observer>()};
    require(lib::llvm_jit_configure_debug_session_host_api(self->control,{self,observer::point,observer::before_park},lib::llvm_jit_debug_safe_point_granularity::instruction)==lib::llvm_jit_debug_configure_result::ok,"actual debugger");
    if(resumable)
    {
        auto profile{::uwvm2::runtime::checkpoint::compilation_profile::create_for_trusted_manager()};
        require(profile && lib::llvm_jit_configure_checkpoint_recording_host_api(profile)==lib::llvm_jit_debug_configure_result::ok,"actual resumable profile");
    }
    else { require(lib::llvm_jit_configure_debug_value_observation_host_api()==lib::llvm_jit_debug_configure_result::ok,"actual -Rdbg observer profile"); }
    require(lib::llvm_jit_prepare_debug_host_api(),"fresh actual full native entries");
    ::std::uint32_t result{99u};::std::atomic_bool completed{};
    ::std::thread guest{[&]
    {
        lib::full_compile_run_config config{};config.entry_function_index=0u;
        // Main-owned exact result storage remains live until guest joins.
        config.entry_abi_buffers.result_buffer=reinterpret_cast<::std::byte*>(::std::addressof(result));
        config.entry_abi_buffers.result_bytes=sizeof(result);
        lib::full_compile_and_run_main_module(u8"blocked-wait",config);completed.store(true);
    }};
    {
        ::std::unique_lock lock{self->mutex};
        require(self->changed.wait_until(lock,deadline(),[&]{return self->wait_reached;}),"real wait opcode reached");
    }
    ::std::this_thread::sleep_for(::std::chrono::milliseconds{50});
    ::std::uint64_t observed_points{};
    lib::llvm_jit_checkpoint_thread_capture_owner old_capture{};
    domain::pause_ticket old_ticket{};
    for(unsigned episode{};episode!=2u;++episode)
    {
        domain::pause_ticket ticket{};
        {
            ::std::lock_guard lock{self->mutex};ticket=self->control->request_pause();self->ticket=ticket;
        }
        require(bool(ticket),"genuine external pause request");
        require(self->control->wait_until_paused(ticket,deadline())==th::cooperative_pause_result::paused,"blocked wait wakes and genuinely parks");
        lib::llvm_jit_checkpoint_thread_capture_owner capture{};::std::uint64_t participant{};
        {
            ::std::lock_guard lock{self->mutex};capture=self->capture;participant=self->participant;
            require(self->parks==episode+1u && !self->native_available,"one actual suspended-wait capture; no VM asm endpoint");
            if(episode==0u) { observed_points=self->points; }
            else { require(observed_points==self->points,"pause does not replay opcode/compare or fabricate trace events"); }
        }
        require(!completed.load(),"guest remains waiting during management");
        ws::request query{};query.selected=ws::selection::operands;query.participant=participant;query.count=8u;
        auto const view{lib::llvm_jit_debug_query_wasm_state_host_api(ticket,{::std::addressof(capture),1u},query)};
        require(view.result==ws::status::available && ws::valid(view) && view.total_values==3u && view.rows.size()==3u,
            "readonly actual wait operands available");
        require((view.rows[0u].data.type.kind==ws::value_kind::i32 || view.rows[0u].data.type.kind==ws::value_kind::i64) &&
            (view.rows[1u].data.type.kind==ws::value_kind::i32 || view.rows[1u].data.type.kind==ws::value_kind::i64) &&
            view.rows[2u].data.type.kind==ws::value_kind::i64,"actual address/expected/timeout Wasm numeric types");
        if(exact_widths)
        {
            require(view.rows[0u].data.type.kind==(address_bits==64u?ws::value_kind::i64:ws::value_kind::i32) &&
                view.rows[1u].data.type.kind==(compare_bits==64u?ws::value_kind::i64:ws::value_kind::i32),
                "memory32/memory64 and wait32/wait64 preserve their exact independent Wasm types");
        }
        for(unsigned row{};row!=3u;++row)
        {
            auto const& value{view.rows[row].data};require(value.available,"in-flight wait retains actual numeric value");
            auto const* first{reinterpret_cast<unsigned char const*>(value.bits.data())};
            // The canonical complete16-byte numeric slot owns these eight bytes.
            // Parse only that bounded copy, never guest/native wait storage.
            ::std::uint64_t bits{};
            auto const parsed{::fast_io::parse_by_scan(first,first+8u,::fast_io::mnp::le_get<64u>(bits))};
            require(parsed.code==::fast_io::parse_code::ok && parsed.iter==first+8u,"complete copied wait numeric bits");
            require(bits==(row==2u ? (action=="notify" ? UINT64_MAX : 200000000u) : 0u),
                "pause and memory edits retain original in-flight wait arguments");
        }
        wm::request mutation{};mutation.participant=participant;
        mutation.target=wm::destination::memory;mutation.source=wm::source_kind::bytes;
        mutation.memory_size=1u;mutation.memory_bytes[0u]=::std::byte{1u};
        require(wm::valid(mutation),"genuine supported memory modification request");
        auto const edited{lib::llvm_jit_debug_mutate_wasm_state_host_api(ticket,{::std::addressof(capture),1u},mutation)};
        require(edited.applied,"actual shared-memory write while wait remains linked");
        ::std::array<::std::byte,16u> label{};label[0u]=::std::byte{1u};
        auto saved{lib::llvm_jit_checkpoint_capture_instance_host_api(ticket,{::std::addressof(capture),1u},label,{})};
        require(!saved.graph,"in-flight wait cannot become a replayable before-opcode instance snapshot");
        require(lib::llvm_jit_checkpoint_retire_saved_execution_host_api(ticket,{::std::addressof(capture),1u})!=
            lib::llvm_jit_checkpoint_execution_retirement_status::execution_retired,"live queue state cannot be retired as a resumable opcode");
        if(episode!=0u)
        {
            require(lib::llvm_jit_debug_query_wasm_state_host_api(old_ticket,{::std::addressof(old_capture),1u},query).result!=ws::status::available,"old episode loses authority");
            if(action=="timeout") { ::std::this_thread::sleep_for(::std::chrono::milliseconds{300}); }
        }
        old_capture=capture;old_ticket=ticket;
        require(self->control->resume(ticket),"resume same linked wait node");
        if(episode==0u) { ::std::this_thread::sleep_for(::std::chrono::milliseconds{10}); }
    }
    if(action=="notify")
    {
        ::std::uint32_t notified{};
        lib::full_compile_run_config config{};config.entry_function_index=1u;
        config.entry_abi_buffers.result_buffer=reinterpret_cast<::std::byte*>(::std::addressof(notified));
        config.entry_abi_buffers.result_bytes=sizeof(notified);
        lib::full_compile_and_run_main_module(u8"blocked-wait",config);
        require(notified==1u,"actual guest notify finds retained wait node exactly once");
    }
    auto const until{deadline()};
    while(!completed.load() && ::std::chrono::steady_clock::now()<until) { ::std::this_thread::yield(); }
    require(completed.load(),"finite actual wait completion");
    guest.join();require(result==(action=="notify"?0u:2u),"original notification/timeout result; original deadline retained");
    lib::reset_runtime_state_host_api();
    ::fast_io::io::println("blocked Wasm wait PASS two real pauses, typed observation, no VM asm, no replay, ",action);
}
