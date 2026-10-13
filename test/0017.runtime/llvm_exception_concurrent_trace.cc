// Actual runtime trace integration: eight admitted hosts execute distinct native
// call chains concurrently; one elected chain throws after the guest rendezvous.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <atomic>
#include <array>
#include <latch>
#include <thread>
#include <fstream>
#include <iterator>
namespace strict = uwvm2test::uwvm_int_strict;
namespace lib = uwvm2::runtime::lib;
namespace wasm_type = uwvm2::uwvm::wasm::type;
namespace mode = uwvm2::uwvm::runtime::runtime_mode;
using host_features = wasm_type::feature_list<uwvm2::parser::wasm::standard::wasm1::features::wasm1>;
struct rendezvous
{
    inline static constexpr uwvm2::utils::container::u8string_view function_name{u8"rendezvous"};
    using result_tuple = wasm_type::import_function_result_tuple_t<host_features>;
    using parameter_tuple = wasm_type::import_function_parameter_tuple_t<host_features>;
    using local_imported_function_type = wasm_type::local_imported_function_type_t<result_tuple,parameter_tuple>;
    inline static std::latch barrier{8};
    inline static std::atomic_uint entered{};
    static void call(local_imported_function_type&) noexcept
    {
        if(entered.fetch_add(1,std::memory_order_acq_rel)==7)
        {
            std::fputs("PROBE concurrent_native_entries=8\n",stdout);
            std::fflush(stdout);
        }
        barrier.count_down();
        barrier.wait();
    }
};
struct host_module
{
    uwvm2::utils::container::u8string_view module_name{u8"trace-host"};
    using local_function_tuple = uwvm2::utils::container::tuple<rendezvous>;
};
int main(int argc,char** argv)
{
    if(argc!=3) { std::fputs("usage: probe module.wasm elected-group-0-to-7-or-8-control\n",stderr);return 2; }
    auto const elected{unsigned(std::strtoul(argv[2],nullptr,10))};
    if(elected>8) { return 2; }
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<char> input{std::istreambuf_iterator<char>{file},{}};
    UWVM2TEST_REQUIRE(file&&!input.empty());
    strict::byte_vec bytes(input.size());
    std::memcpy(bytes.data(),input.data(),input.size());
    auto features{strict::make_wasm1p1_feature_parameter()};
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_exceptions=false;
    wasm_type::local_imported_t host{host_module{}};
    auto prepared{strict::prepare_runtime_from_wasm(bytes,u8"concurrent-trace",{},features,{host})};
    mode::global_runtime_llvm_jit_call_stack=mode::runtime_llvm_jit_call_stack_t::unwind;
    mode::global_runtime_mode=mode::runtime_mode_t::lazy_compile;
    mode::global_runtime_compile_threads_resolved=4;
    lib::lazy_compile_run_config warm{};
    warm.entry_function_index=1; // Import0, independent warm function1.
    lib::lazy_compile_and_run_main_module(u8"concurrent-trace",warm);
    std::latch start{8};
    std::array<std::thread,8> workers;
    for(unsigned group{};group!=8;++group)
    {
        workers[group]=std::thread([&,group]() -> void
        {
            std::uint32_t param{group==elected},result{};
            lib::lazy_compile_run_config config{};
            config.entry_function_index=4+3*group;
            // Both byte spans cover live, disjoint thread-local i32 carriers;
            // the host entry validates their exact widths before native entry.
            config.entry_abi_buffers.param_buffer=reinterpret_cast<std::byte const*>(std::addressof(param));
            config.entry_abi_buffers.param_bytes=sizeof(param);
            config.entry_abi_buffers.result_buffer=reinterpret_cast<std::byte*>(std::addressof(result));
            config.entry_abi_buffers.result_bytes=sizeof(result);
            start.count_down();start.wait();
            lib::lazy_compile_and_run_main_module(u8"concurrent-trace",config);
            // The shared strict REQUIRE macro returns an int. It cannot be used
            // in this void thread callable: deducing int and falling off the
            // successful path would be undefined behavior in the test itself.
            if(group==elected||result!=3)
            {
                std::fprintf(stderr,"FAIL normal result group=%u elected=%u actual=%u expected=3\n",group,elected,result);
                std::fflush(stderr);
                std::abort();
            }
        });
    }
    for(auto& worker:workers) { worker.join(); }
    if(elected==8) { std::puts("PASS nonthrowing concurrent outputs=3");return 0; }
    std::fputs("FAIL elected guest exception returned normally\n",stderr);
    return 1;
}
