// Real runtime lifecycle tests, separate from the main safe-point fixture.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/utils/thread/cooperative_pause_domain.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iterator>
#include <latch>
#include <semaphore>
#include <string>
#include <thread>
#include <sys/syscall.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::abort(); } } while(false)
namespace strict=uwvm2test::uwvm_int_strict;
namespace lib=uwvm2::runtime::lib;
namespace mode=uwvm2::uwvm::runtime::runtime_mode;
namespace types=uwvm2::uwvm::wasm::type;
namespace container=uwvm2::utils::container;
namespace threads=uwvm2::utils::thread;
using value=uwvm2::parser::wasm::standard::wasm1::type::value_type;
using features=types::feature_list<uwvm2::parser::wasm::standard::wasm1::features::wasm1>;
using result_tuple=types::import_function_result_tuple_t<features,value::i32>;
using parameter_tuple=types::import_function_parameter_tuple_t<features>;
using host_call=types::local_imported_function_type_t<result_tuple,parameter_tuple>;
using domain=threads::cooperative_pause_domain;
struct state
{
    std::atomic_bool old_entered{},new_executed{},new_cancelled{},reset_done{},nested_entered{},nested_continue{};
    std::atomic_uint nested_returns{};
    std::latch new_arrived{1},release_new{1};
    void const* module{};
    std::vector<unsigned char> nested_code;
};
static state* active{};
struct old_tick
{
    inline static constexpr container::u8string_view function_name{u8"old_tick"};
    using result_tuple=::result_tuple;using parameter_tuple=::parameter_tuple;using local_imported_function_type=host_call;
    static void call(host_call& call) noexcept
    {active->old_entered.store(true,std::memory_order_release);container::get<0>(call.res)=lib::runtime_execution_stop_requested_host_api();}
};
struct new_entry
{
    inline static constexpr container::u8string_view function_name{u8"new_entry"};
    using result_tuple=::result_tuple;using parameter_tuple=::parameter_tuple;using local_imported_function_type=host_call;
    static void call(host_call& call) noexcept
    {
        active->new_cancelled.store(lib::runtime_execution_stop_requested_host_api(),std::memory_order_release);
        active->new_executed.store(true,std::memory_order_release);active->new_arrived.count_down();
        active->release_new.wait();CHECK(!active->reset_done.load(std::memory_order_acquire));container::get<0>(call.res)=43;
    }
};
struct reenter
{
    inline static constexpr container::u8string_view function_name{u8"reenter"};
    using result_tuple=::result_tuple;using parameter_tuple=::parameter_tuple;using local_imported_function_type=host_call;
    static void call(host_call& call) noexcept
    {
        std::uint32_t result{};
        // The same native thread already has an outer participation lease. A
        // capacity-one domain makes accidental second admission fail visibly.
        lib::llvm_jit_call_raw_host_api(active->module,8,&result,sizeof(result),nullptr,0);
        CHECK(result==73);active->nested_returns.fetch_add(1,std::memory_order_release);container::get<0>(call.res)=result;
    }
};
struct nested_tick
{
    inline static constexpr container::u8string_view function_name{u8"nested_tick"};
    using result_tuple=::result_tuple;using parameter_tuple=::parameter_tuple;using local_imported_function_type=host_call;
    static void call(host_call& call) noexcept
    {active->nested_entered.store(true,std::memory_order_release);container::get<0>(call.res)=active->nested_continue.load(std::memory_order_acquire);}
};
struct host
{
    container::u8string_view module_name{u8"lifecycle-host"};
    using local_function_tuple=container::tuple<old_tick,new_entry,reenter,nested_tick>;
};
auto deadline(){return std::chrono::steady_clock::now()+std::chrono::seconds{10};}
template<class Predicate>void await(Predicate predicate)
{auto until=deadline();while(!predicate()){CHECK(std::chrono::steady_clock::now()<until);std::this_thread::yield();}}
static void run_entry(unsigned function,std::uint32_t& output)
{
    lib::full_compile_run_config config{};config.entry_function_index=function;
    config.entry_abi_buffers.result_buffer=reinterpret_cast<std::byte*>(&output);
    config.entry_abi_buffers.result_bytes=sizeof(output);
    lib::full_compile_and_run_main_module(u8"debug-lifecycle",config);
}
static void reset_waiter(std::shared_ptr<domain> const& control)
{
    std::uint32_t old_result{},new_result{};
    std::thread old{[&]() -> void {run_entry(5,old_result);}};
    await([]{return active->old_entered.load(std::memory_order_acquire);});
    auto ticket=control->request_pause();CHECK(ticket);
    CHECK(control->wait_until_paused(ticket,deadline())==threads::cooperative_pause_result::paused);
    auto snapshot=control->capture(ticket);CHECK(snapshot.participants.size()==1&&snapshot.participants[0].location.function==5);
    std::atomic_long new_tid{};std::latch attempted{1};
    std::thread newcomer{[&]() -> void
    {new_tid.store(syscall(SYS_gettid),std::memory_order_release);attempted.count_down();run_entry(6,new_result);}};
    attempted.wait();
    // No runtime test hook: /proc confirms the attempted entry is blocked in a
    // futex before reset. The later stop_requested assertion additionally proves
    // it held the OLD execution lease, not a freshly admitted post-reset one.
    await([&]
    {
        std::ifstream input("/proc/self/task/"+std::to_string(new_tid.load(std::memory_order_acquire))+"/wchan");
        std::string wait;input>>wait;return wait.find("futex")!=std::string::npos;
    });
    CHECK(control->while_stopped(ticket,[&]{CHECK(!active->new_executed.load(std::memory_order_acquire));}));
    std::thread resetter{[]() -> void {lib::reset_runtime_state_host_api();active->reset_done.store(true,std::memory_order_release);}};
    active->new_arrived.wait();
    CHECK(control->is_closed());CHECK(active->new_cancelled.load(std::memory_order_acquire));
    CHECK(!active->reset_done.load(std::memory_order_acquire));
    CHECK(control->capture(ticket).result==threads::cooperative_pause_result::closed);
    active->release_new.count_down();old.join();newcomer.join();resetter.join();
    CHECK(old_result==41&&new_result==43&&active->reset_done.load(std::memory_order_acquire));
    std::puts("PASS paused new entry: blocked before guest marker; reset woke old-generation admission; live callback held drain; results 41/43");
}
static void nested_reentry(std::shared_ptr<domain> const& control)
{
    std::binary_semaphore completed{0},next{0};std::uint32_t results[2]{};
    std::thread worker{[&]() -> void
    {
        for(unsigned round{};round!=2;++round)
        {run_entry(9,results[round]);completed.release();next.acquire();}
    }};
    for(unsigned round{};round!=2;++round)
    {
        await([]{return active->nested_entered.load(std::memory_order_acquire);});
        auto ticket=control->request_pause();CHECK(ticket);
        CHECK(control->wait_until_paused(ticket,deadline())==threads::cooperative_pause_result::paused);
        auto snapshot=control->capture(ticket);CHECK(snapshot.participants.size()==1);
        auto const location=snapshot.participants[0].location;CHECK(location.function==8&&location.offset>0);
        CHECK(location.offset<active->nested_code.size()&&active->nested_code[location.offset]==0x03u);
        std::printf("PROBE nested pause round=%u module=%llu function=%llu offset=%llu opcode=0x03 participants=1\n",round,
                    static_cast<unsigned long long>(location.code_unit),static_cast<unsigned long long>(location.function),
                    static_cast<unsigned long long>(location.offset));
        active->nested_continue.store(true,std::memory_order_release);CHECK(control->resume(ticket));
        completed.acquire();CHECK(results[round]==146&&active->nested_returns.load(std::memory_order_acquire)==round+1);
        auto empty=control->request_pause();CHECK(empty);
        CHECK(control->wait_until_paused(empty,deadline())==threads::cooperative_pause_result::paused);
        CHECK(control->capture(empty).participants.empty());CHECK(control->resume(empty));
        active->nested_entered.store(false,std::memory_order_release);active->nested_continue.store(false,std::memory_order_release);
        next.release();
    }
    worker.join();lib::reset_runtime_state_host_api();CHECK(control->is_closed());
    std::puts("PASS callback raw reentry: capacity-one domain, exact nested function 8, two same-thread rounds, nested and outer throw/try_table results 146, empty after each host return");
}
int main(int argc,char** argv)
{
    if(argc!=4)return 2;alarm(40);
    std::ifstream file(argv[1],std::ios::binary);std::vector<char> input{std::istreambuf_iterator<char>{file},{}};CHECK(file&&!input.empty());
    strict::byte_vec bytes(input.size());std::memcpy(bytes.data(),input.data(),input.size());
    auto features=strict::make_wasm1p1_feature_parameter();
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_exceptions=false;
    types::local_imported_t provider{host{}};
    auto prepared=strict::prepare_runtime_from_wasm(bytes,u8"debug-lifecycle",{},features,{provider});
    mode::global_runtime_mode=mode::runtime_mode_t::full_compile;mode::global_runtime_compiler=mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack=std::string_view{argv[2]}=="instruction"?mode::runtime_llvm_jit_call_stack_t::instruction:mode::runtime_llvm_jit_call_stack_t::unwind;
    state shared;shared.module=prepared.mod;active=&shared;
    auto const code=prepared.mod->local_defined_function_vec_storage.index_unchecked(4).wasm_code_ptr;CHECK(code!=nullptr);
    for(auto const byte:std::span{code->body.expr_begin,code->body.code_end})shared.nested_code.push_back(static_cast<unsigned char>(byte));
    auto control=std::make_shared<domain>(std::string_view{argv[3]}=="reset"?2:1);
    CHECK(lib::llvm_jit_configure_debug_safe_points_host_api(control)==lib::llvm_jit_debug_configure_result::ok);
    lib::full_compile_run_config warm{};warm.entry_function_index=4;lib::full_compile_and_run_main_module(u8"debug-lifecycle",warm);
    if(std::string_view{argv[3]}=="reset")reset_waiter(control);else if(std::string_view{argv[3]}=="reentry")nested_reentry(control);else return 2;
    alarm(0);
}
