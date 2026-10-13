// Actual LLVM-full instruction stops: preparation has no guest side effects,
// observer stacks belong to the stopped guest, and EH continues after stepping.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iterator>
#include <mutex>
#include <thread>
#include <vector>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::abort(); } } while(false)
namespace strict=uwvm2test::uwvm_int_strict;
namespace lib=uwvm2::runtime::lib;
namespace mode=uwvm2::uwvm::runtime::runtime_mode;
namespace types=uwvm2::uwvm::wasm::type;
namespace container=uwvm2::utils::container;
namespace threads=uwvm2::utils::thread;
using domain=threads::cooperative_pause_domain;
using value=uwvm2::parser::wasm::standard::wasm1::type::value_type;
using features=types::feature_list<uwvm2::parser::wasm::standard::wasm1::features::wasm1>;
struct marker
{
    inline static constexpr container::u8string_view function_name{u8"marker"};
    using result_tuple=types::import_function_result_tuple_t<features,value::i32>;
    using parameter_tuple=types::import_function_parameter_tuple_t<features>;
    using local_imported_function_type=types::local_imported_function_type_t<result_tuple,parameter_tuple>;
    inline static std::atomic_uint calls{};
    static void call(local_imported_function_type& call) noexcept
    {calls.fetch_add(1,std::memory_order_relaxed);container::get<0>(call.res)=13;}
};
struct host
{
    container::u8string_view module_name{u8"observer-host"};
    using local_function_tuple=container::tuple<marker>;
};
auto deadline(){return std::chrono::steady_clock::now()+std::chrono::seconds{20};}
struct event
{
    domain::pause_ticket ticket{};
    threads::cooperative_pause_location location{};
    std::uint_least64_t participant{};
    uwvm2::runtime::exception::diagnostic_trace_ref stack{};
};
struct observer
{
    std::shared_ptr<domain> control{};
    std::mutex mutex{};
    std::condition_variable changed{};
    std::vector<event> events{};
    bool done{};
    static void observe(void* opaque,std::uint_least64_t participant,threads::cooperative_pause_location location) noexcept
    {
        // opaque is an owning, host-constructed observer context retained by the
        // runtime until all real guest entries have drained. No guest supplies it.
        auto& self{*static_cast<observer*>(opaque)};
        CHECK(participant!=0);
        CHECK(!lib::llvm_jit_prepare_debug_host_api());
        CHECK(lib::llvm_jit_configure_debug_safe_points_host_api(self.control)==lib::llvm_jit_debug_configure_result::invalid_context);
        auto ticket=self.control->request_pause();CHECK(ticket);
        std::lock_guard lock{self.mutex};
        self.events.push_back({std::move(ticket),location,participant,{}});
        self.changed.notify_all();
    }
    static void before_park(void* opaque,std::uint_least64_t participant,threads::cooperative_pause_location location,
                            lib::llvm_jit_debug_local_view) noexcept
    {
        auto& self{*static_cast<observer*>(opaque)};
        auto stack=lib::llvm_jit_capture_debug_stack_host_api();
        std::lock_guard lock{self.mutex};
        CHECK(!self.events.empty() && self.events.back().participant==participant && self.events.back().location==location);
        self.events.back().stack=std::move(stack);
    }
};
int main(int argc,char** argv)
{
    if(argc!=3){return 2;}
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<char> input{std::istreambuf_iterator<char>{file},{}};CHECK(file&&!input.empty());
    strict::byte_vec bytes(input.size());std::memcpy(bytes.data(),input.data(),input.size());
    auto features=strict::make_wasm1p1_feature_parameter();
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_exceptions=false;
    types::local_imported_t provider{host{}};
    auto prepared=strict::prepare_runtime_from_wasm(bytes,u8"observer",{},features,{provider});
    mode::global_runtime_mode=mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler=mode::runtime_compiler_t::llvm_jit_only;
    std::string_view policy{argv[2]};
    mode::global_runtime_llvm_jit_call_stack=policy=="instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction :
        policy=="unwind" ? mode::runtime_llvm_jit_call_stack_t::unwind : mode::runtime_llvm_jit_call_stack_t::none;
    CHECK(!lib::llvm_jit_prepare_debug_host_api());
    CHECK(!lib::llvm_jit_capture_debug_stack_host_api());
    auto state=std::make_shared<observer>();state->control=std::make_shared<domain>(1);
    std::weak_ptr<observer> weak=state;
    CHECK(lib::llvm_jit_configure_debug_session_host_api(state->control,{{},observer::observe})==lib::llvm_jit_debug_configure_result::invalid_context);
    CHECK(lib::llvm_jit_configure_debug_session_host_api(state->control,{},static_cast<lib::llvm_jit_debug_safe_point_granularity>(999))==lib::llvm_jit_debug_configure_result::invalid_context);
    CHECK(lib::llvm_jit_configure_debug_session_host_api(state->control,{state,observer::observe,observer::before_park},lib::llvm_jit_debug_safe_point_granularity::instruction)==lib::llvm_jit_debug_configure_result::ok);
    CHECK(lib::llvm_jit_prepare_debug_host_api());
    CHECK(marker::calls.load()==0 && state->events.empty());
    // Compilation publishes code without inventing a guest thread/PC or calling
    // the warm function/import. An empty stopped snapshot is explicitly empty.
    auto empty=state->control->request_pause();CHECK(empty);
    CHECK(state->control->wait_until_paused(empty,deadline())==threads::cooperative_pause_result::paused);
    CHECK(state->control->capture(empty).participants.empty());CHECK(state->control->resume(empty));
    CHECK(lib::llvm_jit_configure_debug_session_host_api(state->control,{state,observer::observe})==lib::llvm_jit_debug_configure_result::already_published);
    std::uint32_t output{};
    std::thread worker{[&]() -> void
    {
        lib::full_compile_run_config config{};config.entry_function_index=3;
        config.entry_abi_buffers.result_buffer=reinterpret_cast<std::byte*>(&output);
        config.entry_abi_buffers.result_bytes=sizeof(output);
        lib::full_compile_and_run_main_module(u8"observer",config);
        std::lock_guard lock{state->mutex};state->done=true;state->changed.notify_all();
    }};
    std::vector<event> saved{};
    bool seen_try{},seen_throw{},seen_add{},seen_nested{};
    for(std::size_t next{};;++next)
    {
        event current{};
        {
            std::unique_lock lock{state->mutex};
            CHECK(state->changed.wait_until(lock,deadline(),[&]{return state->done||state->events.size()>next;}));
            if(state->done && state->events.size()==next){break;}
            CHECK(next<100);current=state->events[next];
        }
        CHECK(state->control->wait_until_paused(current.ticket,deadline())==threads::cooperative_pause_result::paused);
        // The cold hook has completed before the domain can acknowledge parked.
        {std::lock_guard lock{state->mutex};current=state->events[next];}
        auto stopped=state->control->capture(current.ticket);
        CHECK(stopped.participants.size()==1 && stopped.participants[0].id==current.participant);
        CHECK(stopped.participants[0].location==current.location);
        CHECK(current.location.code_unit==0 && (current.location.function==2 || current.location.function==3));
        auto const code=prepared.mod->local_defined_function_vec_storage.index_unchecked(current.location.function-1).wasm_code_ptr;
        CHECK(code!=nullptr && current.location.offset<static_cast<std::size_t>(code->body.code_end-code->body.expr_begin));
        unsigned const opcode=static_cast<unsigned>(code->body.expr_begin[current.location.offset]);
        CHECK(opcode!=0x00u); // unreachable after the try_table is never executed.
        seen_try|=opcode==0x1fu;seen_throw|=opcode==0x08u;seen_add|=opcode==0x6au;
        if(policy=="none"){CHECK(!current.stack);}
        else
        {
            CHECK(current.stack && !current.stack->truncated());
            auto frames=current.stack->frames();CHECK(!frames.empty());
            CHECK(frames[0].module_id==0 && frames[0].function_index==current.location.function);
            CHECK(frames[0].module_name==u8"observer");
            if(current.location.function==2)
            {CHECK(frames.size()==2 && frames[1].function_index==3);seen_nested=true;}
            else{CHECK(frames.size()==1);}
        }
        CHECK(!lib::llvm_jit_capture_debug_stack_host_api()); // manager has no guest stack.
        saved.push_back(current);CHECK(state->control->resume(current.ticket));
    }
    worker.join();CHECK(output==74 && marker::calls.load()==0);
    CHECK(seen_try && seen_throw && seen_add && (policy=="none"||seen_nested));
    auto control=state->control;state.reset();CHECK(!weak.expired());
    lib::reset_runtime_state_host_api();CHECK(weak.expired() && control->is_closed());
    // Owning stack/name snapshots survive code registry retirement.
    for(auto const& old:saved){if(old.stack){CHECK(old.stack->frames()[0].module_name==u8"observer");}}
    std::printf("PASS LLVM full observer: %zu actual instruction stops, side-effect-free prepare, nested guest stacks, throw/catch step, owner/reset lifetime (%s)\n",saved.size(),argv[2]);
}
