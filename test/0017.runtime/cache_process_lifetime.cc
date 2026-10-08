// The runtime's execution drain must precede the cache service's destruction.
// Interpose the real POSIX destructor to observe that order without accessing an
// already-destroyed C++ object or adding test hooks to production code.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <uwvm2/runtime/wasm_threads/impl.h>
#include <uwvm2/runtime/llvm_jit_cache/impl.h>
#include <atomic>
#include <cstdlib>
#include <dlfcn.h>
#include <latch>
#include <pthread.h>
#include <thread>
#include <unistd.h>
namespace
{
    using destroy_function = int (*)(pthread_cond_t*) noexcept;
    destroy_function real_destroy{};
    std::atomic<pthread_cond_t*> watched{};
    std::atomic<bool> callback_drained{};
    std::latch entered{1};
    uwvm2::object::memory::linear::native_memory_t* memory{};
    namespace strict = uwvm2test::uwvm_int_strict;
    namespace lib = uwvm2::runtime::lib;
    namespace types = uwvm2::uwvm::wasm::type;
    using features = types::feature_list<uwvm2::parser::wasm::standard::wasm1::features::wasm1>;
    using results = types::import_function_result_tuple_t<features>;
    using parameters = types::import_function_parameter_tuple_t<features>;
    using host_call = types::local_imported_function_type_t<results, parameters>;
    struct callback
    {
        inline static constexpr uwvm2::utils::container::u8string_view function_name{u8"shutdown"};
        using result_tuple = results;
        using parameter_tuple = parameters;
        using local_imported_function_type = host_call;
        static void call(host_call&) noexcept
        {
            auto& service=uwvm2::runtime::llvm_jit_cache::details::async_cache_store_worker_instance();
            // Only an address identity is retained. The interposer never accesses
            // the C++ service or its condition_variable after destruction begins.
            watched.store(service.condition.native_handle(),std::memory_order_release);
            entered.count_down();
            auto result=uwvm2::runtime::wasm_threads::memory_wait<4>(*memory,true,0,0,-1);
            if(result.status!=uwvm2::runtime::wasm_threads::wait_status::cancelled){::_Exit(87);}
            callback_drained.store(true,std::memory_order_release);
        }
    };
    struct host_module
    {
        uwvm2::utils::container::u8string_view module_name{u8"lifetime-host"};
        using local_function_tuple=uwvm2::utils::container::tuple<callback>;
    };
}
extern "C" int pthread_cond_destroy(pthread_cond_t* value) noexcept
{
    if(value==watched.load(std::memory_order_acquire))
    {
        if(!callback_drained.load(std::memory_order_acquire))
        {
            constexpr char message[]="FAIL cache service destroyed before VM execution drain\n";
            (void)::write(STDERR_FILENO,message,sizeof(message)-1);
            ::_Exit(86);
        }
        constexpr char message[]="PASS process teardown: VM execution drained before cache service destruction\n";
        (void)::write(STDERR_FILENO,message,sizeof(message)-1);
    }
    if(real_destroy==nullptr){::_Exit(88);}
    return real_destroy(value);
}
int main(int argc,char** argv)
{
    real_destroy=reinterpret_cast<destroy_function>(::dlsym(RTLD_NEXT,"pthread_cond_destroy"));
    UWVM2TEST_REQUIRE(real_destroy!=nullptr);
    namespace mode=uwvm2::uwvm::runtime::runtime_mode;
    mode::global_runtime_llvm_jit_call_stack=argc>1 && std::strcmp(argv[1],"unwind")==0 ?
        mode::runtime_llvm_jit_call_stack_t::unwind : mode::runtime_llvm_jit_call_stack_t::instruction;
    strict::module_builder module{};
    module.types.push_back({{}, {}});
    module.add_import_func("lifetime-host","shutdown",0);
    strict::func_body body{};
    for(auto byte:{0xfe,3,0,0x10,0,0xfe,3,0,0x0b}){strict::append_u8(body.code,byte);}
    module.add_func({{},{}},std::move(body));
    auto wasm=module.build();
    auto selected=strict::make_wasm1p1_feature_parameter();
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(selected).disable_threads=false;
    types::local_imported_t host{host_module{}};
    auto prepared=strict::prepare_runtime_from_wasm(wasm,u8"cache-lifetime",{},selected,{host});
    uwvm2::object::memory::linear::native_memory_t shared{};shared.init_by_page_count(1);memory=&shared;
    std::thread worker{[]
    {
        lib::full_compile_run_config config{};config.entry_function_index=1;
        lib::full_compile_and_run_main_module(u8"cache-lifetime",config);
    }};
    worker.detach();
    entered.wait();
    // exit bypasses automatic-variable destruction: the external module and host
    // memory intentionally stay live until the runtime's process guard drains the
    // detached execution. All namespace/static destructors still run normally.
    std::exit(0);
}
