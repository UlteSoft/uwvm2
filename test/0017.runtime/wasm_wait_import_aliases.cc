// Two Wasm modules share one native memory owner; another memory must stay isolated.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <atomic>
#include <thread>
namespace
{
    namespace strict=uwvm2test::uwvm_int_strict;
    namespace lib=uwvm2::runtime::lib;
    namespace mode=uwvm2::uwvm::runtime::runtime_mode;
    strict::func_body body(std::initializer_list<unsigned> bytes)
    {
        strict::func_body result{};
        for(auto byte:bytes){strict::append_u8(result.code,byte);}
        return result;
    }
    strict::byte_vec provider()
    {
        strict::module_builder m{};
        m.has_memory=m.memory_has_max=m.memory_shared=true;
        m.memory_min=1;m.memory_max=4;m.add_export_memory("shared");
        // i32 wait, timeout -1, on memory 0; the consumer also waits with i64.
        m.add_func({{},{strict::k_val_i32}},body({0x41,0,0x41,0,0x42,0x7f,0xfe,1,2,0,0x0b}));
        m.add_export_func(0,"wait");
        m.add_func({{},{strict::k_val_i32}},body({0x41,1,0x40,0,0x0b}));
        m.add_export_func(1,"grow");
        return m.build();
    }
    strict::byte_vec consumer()
    {
        strict::module_builder m{};
        m.types.push_back({{},{strict::k_val_i32}});
        m.add_import_func("owner","wait",0);m.add_import_func("owner","grow",0);
        m.add_import_memory("owner","shared",1,4,true,true);
        m.add_import_memory("owner","shared",1,4,true,true);
        m.has_memory=m.memory_has_max=m.memory_shared=true;m.memory_min=1;m.memory_max=4;
        // Function 2 calls into the provider; function 3 waits directly via alias 1.
        m.add_func({{},{strict::k_val_i32}},body({0x10,0,0x0b}));
        m.add_func({{},{strict::k_val_i32}},body({0x41,0,0x42,0,0x42,0x7f,0xfe,2,0x43,1,0,0x0b}));
        // Functions 4/5/6 notify one on alias 0 / private memory 2 / offset 8.
        for(auto [memory,offset]:{std::pair{0u,0u},{2u,0u},{0u,8u}})
        {m.add_func({{},{strict::k_val_i32}},body({0x41,0,0x41,1,0xfe,0,0x42,memory,offset,0x0b}));}
        m.add_func({{},{strict::k_val_i32}},body({0x10,1,0x0b})); // 7: grow through provider
        m.add_func({{},{strict::k_val_i32}},body({0x3f,1,0x0b})); // 8: size through alias 1
        return m.build();
    }
    std::uint32_t enter(void const* module,unsigned function,bool raw)
    {
        std::uint32_t result{};
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
        if(raw){lib::llvm_jit_call_raw_host_api(module,function,&result,4,nullptr,0);return result;}
#else
        (void)module;(void)raw;
#endif
#if defined(UWVM2TEST_EXECUTION_LAZY)
        lib::lazy_compile_run_config config{};
#else
        lib::full_compile_run_config config{};
#endif
        config.entry_function_index=function;
        config.entry_abi_buffers.result_buffer=reinterpret_cast<std::byte*>(&result);
        config.entry_abi_buffers.result_bytes=4;
#if defined(UWVM2TEST_EXECUTION_LAZY)
        lib::lazy_compile_and_run_main_module(u8"aliases",config);
#else
        lib::full_compile_and_run_main_module(u8"aliases",config);
#endif
        return result;
    }
}
int main(int argc,char** argv)
{
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
    mode::global_runtime_llvm_jit_call_stack=argc>1 && std::strcmp(argv[1],"unwind")==0?
        mode::runtime_llvm_jit_call_stack_t::unwind:mode::runtime_llvm_jit_call_stack_t::instruction;
#else
    (void)argc;(void)argv;
#endif
    auto owner=provider(),aliases=consumer();
    auto features=strict::make_wasm1p1_feature_parameter();
    auto& policy=uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features);
    policy.disable_threads=policy.disable_multi_memory=false;
    auto prepared=strict::prepare_runtime_from_wasm(aliases,u8"aliases",{{&owner,u8"owner",&features}},features);
#if defined(UWVM2TEST_EXECUTION_LAZY)
    mode::global_runtime_mode=mode::runtime_mode_t::lazy_compile;
#endif
#if defined(UWVM2TEST_EXECUTION_TIERED)
    mode::global_runtime_compiler=mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered;
#endif
    mode::global_runtime_compile_threads_resolved=2;
    auto const first=prepared.mod->imported_memory_vec_storage.index_unchecked(0).target.defined_ptr;
    auto const second=prepared.mod->imported_memory_vec_storage.index_unchecked(1).target.defined_ptr;
    UWVM2TEST_REQUIRE(first!=nullptr && first==second);
    UWVM2TEST_REQUIRE(first!=std::addressof(prepared.mod->local_defined_memory_vec_storage.index_unchecked(0)));
    UWVM2TEST_REQUIRE(enter(prepared.mod,8,false)==1);
    for(bool raw:{false,true})
    {
        std::uint32_t results[2]{UINT32_MAX,UINT32_MAX};
        std::thread waiters[2];
        for(unsigned i{};i!=2;++i)
        {waiters[i]=std::thread{[&,i]{results[i]=enter(prepared.mod,2+i,raw);}};}
        unsigned notified{};
        while(notified!=2)
        {
            UWVM2TEST_REQUIRE(enter(prepared.mod,5,raw)==0); // Same address, different owner.
            UWVM2TEST_REQUIRE(enter(prepared.mod,6,raw)==0); // Same owner, different offset.
            auto count=enter(prepared.mod,4,raw);
            UWVM2TEST_REQUIRE(count<=1);
            notified+=count;
            if(count==1 && notified==1)
            {
                UWVM2TEST_REQUIRE(enter(prepared.mod,7,raw)==(raw?2u:1u));
                UWVM2TEST_REQUIRE(enter(prepared.mod,8,raw)==(raw?3u:2u));
            }
            std::this_thread::yield();
        }
        for(auto& waiter:waiters){waiter.join();}
        UWVM2TEST_REQUIRE(results[0]==0 && results[1]==0);
    }
    lib::reset_runtime_state_host_api();
    std::puts("PASS guest wait32/wait64 across provider calls and duplicate imports, owner/offset isolation, exact notification and shared growth");
}
