// Real LLVM full runtime, host controller and eight guest executions. The host
// management domain is never imported; the guest can only call the tick fixture.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/utils/thread/cooperative_pause_domain.h>
#include <atomic>
#include <array>
#include <chrono>
#include <fstream>
#include <iterator>
#include <thread>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::abort(); } } while(false)
namespace strict=uwvm2test::uwvm_int_strict;
namespace lib=uwvm2::runtime::lib;
namespace mode=uwvm2::uwvm::runtime::runtime_mode;
namespace types=uwvm2::uwvm::wasm::type;
namespace container=uwvm2::utils::container;
namespace threads=uwvm2::utils::thread;
using value=uwvm2::parser::wasm::standard::wasm1::type::value_type;
using features=types::feature_list<uwvm2::parser::wasm::standard::wasm1::features::wasm1>;
using domain=threads::cooperative_pause_domain;
inline thread_local unsigned worker_index{};
struct tick
{
    inline static constexpr container::u8string_view function_name{u8"tick"};
    using result_tuple=types::import_function_result_tuple_t<features,value::i32>;
    using parameter_tuple=types::import_function_parameter_tuple_t<features>;
    using local_imported_function_type=types::local_imported_function_type_t<result_tuple,parameter_tuple>;
    inline static std::atomic_uint entered{};
    inline static std::atomic_ullong progress{};
    inline static std::atomic_bool finish{};
    static void call(local_imported_function_type& call) noexcept
    {
        entered.fetch_or(1u<<worker_index,std::memory_order_release);
        progress.fetch_add(1,std::memory_order_relaxed);
        container::get<0>(call.res)=finish.load(std::memory_order_acquire);
    }
};
struct host
{
    container::u8string_view module_name{u8"debug-host"};
    using local_function_tuple=container::tuple<tick>;
};
auto deadline() {return std::chrono::steady_clock::now()+std::chrono::seconds{15};}
int main(int argc,char** argv)
{
    if(argc!=3) {return 2;}
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<char> input{std::istreambuf_iterator<char>{file},{}};CHECK(file&&!input.empty());
    strict::byte_vec bytes(input.size());std::memcpy(bytes.data(),input.data(),input.size());
    auto features= strict::make_wasm1p1_feature_parameter();
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_exceptions=false;
    types::local_imported_t provider{host{}};
    auto prepared=strict::prepare_runtime_from_wasm(bytes,u8"debug-pause",{},features,{provider});
    mode::global_runtime_mode=mode::runtime_mode_t::full_compile;
    mode::global_runtime_compiler=mode::runtime_compiler_t::llvm_jit_only;
    mode::global_runtime_llvm_jit_call_stack=std::string_view{argv[2]}=="instruction" ? mode::runtime_llvm_jit_call_stack_t::instruction : mode::runtime_llvm_jit_call_stack_t::unwind;
    auto control=std::make_shared<domain>(8);
    CHECK(lib::llvm_jit_configure_debug_safe_points_host_api(control)==lib::llvm_jit_debug_configure_result::ok);
    lib::full_compile_run_config warm{};warm.entry_function_index=1;
    lib::full_compile_and_run_main_module(u8"debug-pause",warm);
    CHECK(lib::llvm_jit_configure_debug_safe_points_host_api({})==lib::llvm_jit_debug_configure_result::already_published);
    std::array<std::thread,8> workers;
    std::array<std::uint32_t,8> outputs{};
    for(unsigned i{};i!=8;++i)
    {
        workers[i]=std::thread{[&,i]() -> void
        {
            worker_index=i;
            lib::full_compile_run_config config{};config.entry_function_index=3;
            config.entry_abi_buffers.result_buffer=reinterpret_cast<std::byte*>(&outputs[i]);
            config.entry_abi_buffers.result_bytes=sizeof(outputs[i]);
            lib::full_compile_and_run_main_module(u8"debug-pause",config);
        }};
    }
    auto until=deadline();
    while(tick::entered.load(std::memory_order_acquire)!=255) {CHECK(std::chrono::steady_clock::now()<until);std::this_thread::yield();}
    auto const code=prepared.mod->local_defined_function_vec_storage.index_unchecked(2).wasm_code_ptr;
    CHECK(code!=nullptr);
    for(unsigned round{};round!=32;++round)
    {
        auto ticket=control->request_pause();CHECK(ticket);
        CHECK(control->wait_until_paused(ticket,deadline())==threads::cooperative_pause_result::paused);
        auto snapshot=control->capture(ticket);CHECK(snapshot.result==threads::cooperative_pause_result::paused && snapshot.participants.size()==8);
        for(auto const& thread:snapshot.participants)
        {
            auto const& where=thread.location;CHECK(where.function==3 && where.offset>0);
            CHECK(where.offset<static_cast<std::size_t>(code->body.code_end-code->body.expr_begin));
            CHECK(static_cast<unsigned>(code->body.expr_begin[where.offset])==0x03u);
        }
        auto const progress=tick::progress.load();
        CHECK(control->while_stopped(ticket,[&]{for(unsigned i{};i!=1000;++i){CHECK(tick::progress.load()==progress);}}));
        CHECK(control->resume(ticket));CHECK(!control->resume(ticket));
    }
    tick::finish.store(true,std::memory_order_release);
    for(auto& worker:workers){worker.join();}
    for(auto output:outputs){CHECK(output==73);}
    // Quiescent reset closes/removes this opt-in. Surviving management handles
    // cannot pause a later runtime generation and configure-after-reset is valid.
    lib::reset_runtime_state_host_api();CHECK(control->is_closed());
    CHECK(!control->request_pause());
    CHECK(lib::llvm_jit_configure_debug_safe_points_host_api({})==lib::llvm_jit_debug_configure_result::ok);
    lib::full_compile_and_run_main_module(u8"debug-pause",warm);
    lib::reset_runtime_state_host_api();
    std::puts("PASS actual LLVM full safe points: 8 concurrent guests x 32 pauses, exact loop offsets, stable progress, native EH resumes, reset invalidates controller");
}
