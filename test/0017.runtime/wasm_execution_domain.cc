#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <uwvm2/runtime/wasm_threads/impl.h>
#include <atomic>
#include <latch>
#include <thread>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
namespace
{
    namespace strict = uwvm2test::uwvm_int_strict;
    namespace lib = uwvm2::runtime::lib;
    namespace wasm_type = uwvm2::uwvm::wasm::type;
    using wasm1 = uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using features_t = wasm_type::feature_list<wasm1>;
    using result_t = wasm_type::import_function_result_tuple_t<features_t>;
    using params_t = wasm_type::import_function_parameter_tuple_t<features_t>;
    using host_call_t = wasm_type::local_imported_function_type_t<result_t,params_t>;
    struct scenario
    {
        std::latch entered{1}, stopped{1}, may_return{1};
        std::atomic<bool> reset_done{};
        bool expect_stopped{};
        std::atomic<unsigned> nested_count{};
        std::latch* rendezvous{};
        void const* module{};
        uwvm2::object::memory::linear::native_memory_t memory{};
    };
    scenario* active{};
    struct probe
    {
        inline static constexpr uwvm2::utils::container::u8string_view function_name{u8"probe"};
        using result_tuple = result_t;
        using parameter_tuple = params_t;
        using local_imported_function_type = host_call_t;
        static void call(host_call_t&) noexcept
        {
            if(lib::runtime_execution_stop_requested_host_api()!=active->expect_stopped) {std::abort();}
            ++active->nested_count;
            if(active->rendezvous != nullptr)
            {
                active->rendezvous->count_down();
                active->rendezvous->wait();
            }
        }
    };
    struct block
    {
        inline static constexpr uwvm2::utils::container::u8string_view function_name{u8"block"};
        using result_tuple = result_t;
        using parameter_tuple = params_t;
        using local_imported_function_type = host_call_t;
        static void call(host_call_t&) noexcept
        {
            if(lib::runtime_execution_stop_requested_host_api()) {std::abort();}
            active->entered.count_down();
            // The real VM host entry installs the wait domain and its generation
            // cancellation token. Reset must wake this infinite wait before drain.
            auto const waited=uwvm2::runtime::wasm_threads::memory_wait<4>(active->memory,true,0,0,-1);
            if(waited.status!=uwvm2::runtime::wasm_threads::wait_status::cancelled) {std::abort();}
            active->stopped.count_down();
            active->may_return.wait();
            if(active->reset_done.load(std::memory_order_acquire)) {std::abort();}
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
            // Reset has closed admission; a nested callback still owns its outer
            // execution and must be able to return through live generated code.
            active->expect_stopped=true;
            std::uint32_t result{};
            lib::llvm_jit_call_raw_host_api(active->module,3,&result,4,nullptr,0);
            if(result!=17 || active->nested_count!=1) {std::abort();}
#endif
        }
    };
    struct host_module
    {
        uwvm2::utils::container::u8string_view module_name{u8"lifetime-host"};
        using local_function_tuple = uwvm2::utils::container::tuple<block,probe>;
    };
    strict::byte_vec make_module()
    {
        strict::module_builder module{};
        module.has_memory=module.memory_has_max=module.memory_shared=true;
        module.memory_min=1;module.memory_max=7;
        module.types.push_back({{}, {}});
        module.add_import_func("lifetime-host","block",0);
        module.add_import_func("lifetime-host","probe",0);
        strict::func_body main{}, nested{};
        // Keep a value across a new threads instruction and an imported callback.
        for(auto byte:{0x41,37,0xfe,3,0,0x10,0,0x41,6,0x6a,0x0b}) {strict::append_u8(main.code,byte);}
        for(auto byte:{0x10,1,0xfe,3,0,0x41,17,0x0b}) {strict::append_u8(nested.code,byte);}
        module.add_func({{}, {strict::k_val_i32}},std::move(main));
        module.add_func({{}, {strict::k_val_i32}},std::move(nested));
        for(unsigned i{};i!=32;++i)
        {
            strict::func_body cold{};
            for(auto byte:{0xfe,3,0,0x41,int(i),0x0b}) {strict::append_u8(cold.code,byte);}
            module.add_func({{}, {strict::k_val_i32}},std::move(cold));
        }
        strict::func_body trap{},caller{};
        for(auto byte:{0xfe,3,0,0,0x0b}) {strict::append_u8(trap.code,byte);}
        for(auto byte:{0x10,36,0x0b}) {strict::append_u8(caller.code,byte);}
        module.add_func({{}, {strict::k_val_i32}},std::move(trap));
        module.add_func({{}, {strict::k_val_i32}},std::move(caller));
        strict::func_body size{},grow{};
        for(auto byte:{0x3f,0,0x0b}) {strict::append_u8(size.code,byte);}
        for(auto byte:{0x41,1,0x40,0,0x0b}) {strict::append_u8(grow.code,byte);}
        module.add_func({{}, {strict::k_val_i32}},std::move(size));
        module.add_func({{}, {strict::k_val_i32}},std::move(grow));
        // Actual new FE 00/01/02 guest instructions, with older values kept live
        // across suspension to exercise spilling/refilling mixed register rings.
        auto add_wait{[&](unsigned operation, unsigned expected, bool immediate_timeout)
        {
            strict::func_body body{};
            for(auto b:{0x41,37,0x42,5,0x41,0}) {strict::append_u8(body.code,b);}
            strict::append_u8(body.code,operation==2u?0x42:0x41);
            strict::append_u8(body.code,expected);
            if(operation!=0u)
            {strict::append_u8(body.code,0x42);strict::append_u8(body.code,immediate_timeout?0:0x7f);}
            for(auto b:{0xfe,int(operation),operation==2u?3:2,0,0xac,0x7c,0xa7,0x6a,0x0b})
            {strict::append_u8(body.code,b);}
            module.add_func({{}, {strict::k_val_i32}},std::move(body));
        }};
        add_wait(0,1,false); // 40 notify(1), result + 42
        add_wait(1,1,false); // 41 mismatch => 43
        add_wait(1,0,true);  // 42 immediate timeout => 44
        add_wait(2,0,false); // 43 wait64 => 42 after notification
        add_wait(1,0,false); // 44 wait32 => 42 after notification
        return module.build();
    }
    void enter(void const* module,std::uint32_t index,std::uint32_t& result,bool raw)
    {
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
        if(raw) {lib::llvm_jit_call_raw_host_api(module,index,&result,4,nullptr,0);return;}
#else
        (void)module; (void)raw;
#endif
#if defined(UWVM2TEST_EXECUTION_LAZY)
        lib::lazy_compile_run_config config{};
#else
        lib::full_compile_run_config config{};
#endif
        config.entry_function_index=index;
        config.entry_abi_buffers.result_buffer=reinterpret_cast<std::byte*>(&result);
        config.entry_abi_buffers.result_bytes=4;
#if defined(UWVM2TEST_EXECUTION_LAZY)
        lib::lazy_compile_and_run_main_module(u8"execution-domain",config);
#else
        lib::full_compile_and_run_main_module(u8"execution-domain",config);
#endif
    }
}
int main(int argc,char** argv)
{
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
    namespace mode=uwvm2::uwvm::runtime::runtime_mode;
    mode::global_runtime_llvm_jit_call_stack = argc>1 && std::strcmp(argv[1],"unwind")==0 ?
        mode::runtime_llvm_jit_call_stack_t::unwind : mode::runtime_llvm_jit_call_stack_t::instruction;
#else
    (void)argc; (void)argv;
#endif
    auto wasm{make_module()};
    auto features{strict::make_wasm1p1_feature_parameter()};
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_threads=false;
    wasm_type::local_imported_t host{host_module{}};
    auto prepared{strict::prepare_runtime_from_wasm(wasm,u8"execution-domain",{},features,{host})};
#if defined(UWVM2TEST_EXECUTION_LAZY)
    uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode = uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::lazy_compile;
#endif
#if defined(UWVM2TEST_EXECUTION_TIERED)
    namespace tiered=uwvm2::uwvm::runtime::runtime_mode;
    tiered::global_runtime_compiler=tiered::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered;
    tiered::runtime_tiered_disable_uwvm_int_lazy_interpreter=argc>2 && std::strstr(argv[2],"no-t0")!=nullptr;
    tiered::runtime_tiered_disable_llvm_full_jit=argc>2 && std::strstr(argv[2],"no-t2")!=nullptr;
#endif
    uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_resolved=2;
    // Actual shared-memory syntax reaches both execution backends. Concurrent
    // grow calls must return distinct old sizes, and size must observe publication.
    std::uint32_t initial_size{};enter(prepared.mod,38,initial_size,false);UWVM2TEST_REQUIRE(initial_size==1);
    std::uint32_t grown[4]{};std::thread growers[4];
    for(unsigned i{};i!=4;++i){growers[i]=std::thread{[&,i]{enter(prepared.mod,39,grown[i],false);}};}
    for(auto& thread:growers){thread.join();}
    std::sort(std::begin(grown),std::end(grown));
    for(unsigned i{};i!=4;++i){UWVM2TEST_REQUIRE(grown[i]==i+1);}
    enter(prepared.mod,38,initial_size,false);UWVM2TEST_REQUIRE(initial_size==5);
    std::fputs("shared size/grow PASS; entering guest wait mismatch and timeout\n",stderr);
    std::uint32_t waited{};
    enter(prepared.mod,41,waited,false);UWVM2TEST_REQUIRE(waited==43);
    enter(prepared.mod,42,waited,false);UWVM2TEST_REQUIRE(waited==44);
    std::fputs("guest wait mismatch and timeout PASS\n",stderr);
    for(bool raw:{false,true})
    {
        std::fprintf(stderr,"guest wait/notify concurrency raw=%d\n",raw);
        std::uint32_t answers[2]{};
        std::thread waiters[2];
        for(unsigned i{};i!=2;++i){waiters[i]=std::thread{[&,i]{enter(prepared.mod,43+i,answers[i],raw);}};}
        unsigned notified{};
        while(notified!=2)
        {
            enter(prepared.mod,40,waited,raw);
            UWVM2TEST_REQUIRE(waited==42 || waited==43);
            notified+=waited-42;
            if(waited!=42 && notified==1)
            {
                // One notification has completed. Grow while the other guest
                // can still be suspended on the same native memory owner.
                std::uint32_t old_size{};enter(prepared.mod,39,old_size,raw);
                UWVM2TEST_REQUIRE(old_size==(raw?6u:5u));
            }
            std::this_thread::yield();
        }
        for(auto& waiter:waiters){waiter.join();}
        UWVM2TEST_REQUIRE(answers[0]==42 && answers[1]==42);
    }
    enter(prepared.mod,39,initial_size,false);UWVM2TEST_REQUIRE(initial_size==UINT32_MAX);
    std::puts("guest wait32/wait64/notify mixed-ring concurrent execution and grow PASS");
    for(bool stop_only:{false,true}) for(bool raw:{false,true})
    {
        scenario state{}; state.module=prepared.mod; active=&state; state.memory.init_by_page_count(1);
        UWVM2TEST_REQUIRE(!lib::runtime_execution_stop_requested_host_api());
        std::uint32_t answer{};
        std::thread worker{[&]
        {
            enter(prepared.mod,2,answer,raw);
            if(lib::runtime_execution_stop_requested_host_api()) {std::abort();}
        }};
        state.entered.wait();
        std::thread resetter{[&]
        {
            if(stop_only) {lib::runtime_stop_and_drain_host_api();}
            else {lib::reset_runtime_state_host_api();}
            state.reset_done.store(true,std::memory_order_release);
        }};
        state.stopped.wait();
        UWVM2TEST_REQUIRE(!state.reset_done.load(std::memory_order_acquire));
        state.may_return.count_down();
        worker.join(); resetter.join();
        UWVM2TEST_REQUIRE(answer==43 && state.reset_done.load(std::memory_order_acquire));
        if(stop_only)
        {
            // Draining is idempotent and leaves admission closed. An explicit
            // reset retires the retained registries and publishes a new generation.
            lib::runtime_stop_and_drain_host_api();
            // Rejection is tested in a child because the public runtime's failure
            // policy is fatal. No compiler or guest thread remains live here.
            int output[2]{};UWVM2TEST_REQUIRE(::pipe(output)==0);
            auto child=::fork();UWVM2TEST_REQUIRE(child>=0);
            if(child==0)
            {
                ::close(output[0]);(void)::dup2(output[1],STDERR_FILENO);::close(output[1]);::alarm(10);
                uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());
                std::uint32_t rejected{};enter(prepared.mod,3,rejected,raw);::_exit(99);
            }
            ::close(output[1]);std::string diagnostic;char buffer[512];
            for(;;)
            {
                auto n=::read(output[0],buffer,sizeof(buffer));if(n<=0){break;}
                diagnostic.append(buffer,static_cast<std::size_t>(n));
            }
            ::close(output[0]);int status{};UWVM2TEST_REQUIRE(::waitpid(child,&status,0)==child);
            UWVM2TEST_REQUIRE(WIFSIGNALED(status) && diagnostic.find("Runtime execution admission is closed")!=std::string::npos);
            lib::reset_runtime_state_host_api();
        }
        // Reuse the retained external module after reset; no cancelled token or
        // stale compiled function address may survive into the new generation.
        state.expect_stopped=false;
        answer=0; enter(prepared.mod,3,answer,raw);
        UWVM2TEST_REQUIRE(answer==17 && !lib::runtime_execution_stop_requested_host_api());
        // Both executions must reach the host barrier simultaneously. With a
        // whole-execution unwind mutex this deadlocks; full-mode immutable maps
        // permit the two generated stacks to remain live at the same time.
        std::latch both{2}; state.rendezvous=&both;
        std::uint32_t a{},b{};
        std::thread first{[&]{enter(prepared.mod,3,a,raw);}};
        std::thread second{[&]{enter(prepared.mod,3,b,raw);}};
        first.join(); second.join();
        state.rendezvous=nullptr;
        UWVM2TEST_REQUIRE(a==17 && b==17);
        // Independent cold units race publication; returning one invocation must
        // leave schedulers and metadata usable by the other admitted executions.
        std::thread publishers[4];
        for(unsigned t{};t!=4;++t) {publishers[t]=std::thread{[&,t]
        {
            for(unsigned i=t;i<32;i+=4)
            {std::uint32_t value{};enter(prepared.mod,4+i,value,raw);if(value!=i) {std::abort();}}
        }};}
        for(auto& publisher:publishers) {publisher.join();}
        lib::runtime_request_execution_stop_host_api();
        lib::reset_runtime_state_host_api();
    }
    if(argc>2 && std::strcmp(argv[2],"trap")==0)
    {
        std::uint32_t result{};enter(prepared.mod,37,result,false);std::abort();
    }
    std::puts("PASS VM execution admission: concurrent reset/stop-and-drain, live callback/code lifetime, atomic.fence, shared size/grow, managed wait cancellation, fresh generation");
}
