#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <array>
#include <atomic>
#include <thread>
namespace
{
    namespace strict=uwvm2test::uwvm_int_strict;
    namespace lib=uwvm2::runtime::lib;
    namespace mode=uwvm2::uwvm::runtime::runtime_mode;
    namespace types=uwvm2::uwvm::wasm::type;
    namespace container=uwvm2::utils::container;
    using wasm1=uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using value=uwvm2::parser::wasm::standard::wasm1::type::value_type;
    using features_t=types::feature_list<wasm1>;
    std::atomic<unsigned> host_calls{};
    struct finish
    {
        inline static constexpr container::u8string_view function_name{u8"finish"};
        using result_tuple=types::import_function_result_tuple_t<features_t,value::i32,value::i64,value::f32,value::f64,value::i64>;
        using parameter_tuple=types::import_function_parameter_tuple_t<features_t>;
        using local_imported_function_type=types::local_imported_function_type_t<result_tuple,parameter_tuple>;
        static void call(local_imported_function_type& call) noexcept
        {
            container::get<0>(call.res)=31;container::get<1>(call.res)=37;
            container::get<2>(call.res)=2.5f;container::get<3>(call.res)=9.25;
            container::get<4>(call.res)=123;++host_calls;
        }
    };
    struct host
    {
        container::u8string_view module_name{u8"tail-host"};
        using local_function_tuple=container::tuple<finish>;
    };
    void bytes(strict::byte_vec& out,std::initializer_list<unsigned> values)
    {for(auto v:values){strict::append_u8(out,v);}}
    strict::byte_vec module(bool provider,bool indirect)
    {
        strict::module_builder m{};
        strict::func_type result{{},{strict::k_val_i32,strict::k_val_i64,strict::k_val_f32,strict::k_val_f64,strict::k_val_i64}};
        if(provider)
        {
            strict::func_body body{};
            bytes(body.code,{0x41,31,0x42,37,0x43,0,0,0x20,0x40,0x44,0,0,0,0,0,0x80,0x22,0x40,0x42});
            strict::append_i64_leb(body.code,123);bytes(body.code,{0x0b});
            m.add_func(result,std::move(body));m.add_export_func(0,"finish");return m.build();
        }
        m.types.push_back(result);m.add_import_func("tail-host","finish",0);m.add_import_func("tail-provider","finish",0);
        auto a_type=result;a_type.params={strict::k_val_i32,strict::k_val_i32};
        auto b_type=result;b_type.params={strict::k_val_i32,strict::k_val_i32,strict::k_val_f64};
        auto tail=[&](strict::byte_vec& out,unsigned function,unsigned type)
        {
            if(indirect){bytes(out,{0x41,function,0x13,type,0});}
            else{bytes(out,{0x12,function});}
        };
        strict::func_body a{};
        bytes(a.code,{0x20,0,0x45,0x04,0x40,0x20,1,0x04,0x40});tail(a.code,0,0);
        bytes(a.code,{0x05});tail(a.code,1,0);bytes(a.code,{0x0b,0x0b});
        bytes(a.code,{0x20,0,0x20,1,0x44,0,0,0,0,0,0,0x22,0x40});tail(a.code,3,2);bytes(a.code,{0x0b});
        m.add_func(a_type,std::move(a));
        strict::func_body b{};
        bytes(b.code,{0x20,2,0x44,0,0,0,0,0,0,0x22,0x40,0x62,0x04,0x40,0x00,0x0b,
                      0x20,0,0x41,1,0x6b,0x20,1});tail(b.code,2,1);bytes(b.code,{0x0b});
        m.add_func(b_type,std::move(b));
        if(indirect)
        {
            m.has_table=true;m.table_min=4;strict::element_segment e{};bytes(e.offset_expr,{0x41,0,0x0b});e.func_indices={0,1,2,3};m.elements.push_back(std::move(e));
        }
        return m.build();
    }
    int enter(bool host)
    {
        std::array<std::byte,64> guarded;guarded.fill(std::byte{0x5a});
        std::uint32_t args[]{30000,host?1u:0u};
#if defined(UWVM2TEST_EXECUTION_LAZY)
        lib::lazy_compile_run_config config{};
#else
        lib::full_compile_run_config config{};
#endif
        config.entry_function_index=2;
        config.entry_abi_buffers.result_buffer=guarded.data()+7;config.entry_abi_buffers.result_bytes=32;
        config.entry_abi_buffers.param_buffer=reinterpret_cast<std::byte*>(args);config.entry_abi_buffers.param_bytes=sizeof(args);
#if defined(UWVM2TEST_EXECUTION_LAZY)
        lib::lazy_compile_and_run_main_module(u8"tail-tuple",config);
#else
        lib::full_compile_and_run_main_module(u8"tail-tuple",config);
#endif
        for(unsigned i{};i!=guarded.size();++i) {if(i<7 || i>=39) {UWVM2TEST_REQUIRE(guarded[i]==std::byte{0x5a});}}
        std::int32_t x;std::int64_t y,z;float f;double d;
        std::memcpy(&x,guarded.data()+7,4);std::memcpy(&y,guarded.data()+11,8);std::memcpy(&f,guarded.data()+19,4);
        std::memcpy(&d,guarded.data()+23,8);std::memcpy(&z,guarded.data()+31,8);
        UWVM2TEST_REQUIRE(x==31 && y==37 && f==2.5f && d==9.25 && z==123);return 0;
    }
}
int main(int argc,char** argv)
{
    bool indirect{};bool unwind{};
    for(int i{1};i<argc;++i){indirect|=std::strcmp(argv[i],"indirect")==0;unwind|=std::strcmp(argv[i],"unwind")==0;}
    mode::global_runtime_llvm_jit_call_stack=unwind?mode::runtime_llvm_jit_call_stack_t::unwind:mode::runtime_llvm_jit_call_stack_t::instruction;
    auto features=strict::make_wasm1p1_feature_parameter();
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_tail_call=false;
    auto provider=module(true,false),consumer=module(false,indirect);
    types::local_imported_t imported{host{}};
    auto prepared=strict::prepare_runtime_from_wasm(consumer,u8"tail-tuple",{{&provider,u8"tail-provider",&features}},features,{imported});
#if defined(UWVM2TEST_EXECUTION_LAZY)
    mode::global_runtime_mode=mode::runtime_mode_t::lazy_compile;
#endif
#if defined(UWVM2TEST_EXECUTION_TIERED)
    mode::global_runtime_compiler=mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered;
    for(int i{1};i<argc;++i)
    {
        if(std::strstr(argv[i],"no-t0")){mode::runtime_tiered_disable_uwvm_int_lazy_interpreter=true;}
        if(std::strstr(argv[i],"no-t2")){mode::runtime_tiered_disable_llvm_full_jit=true;}
    }
#endif
    mode::global_runtime_compile_threads_resolved=2;
    UWVM2TEST_REQUIRE(enter(false)==0);UWVM2TEST_REQUIRE(enter(true)==0);
    int errors[2]{};
    std::thread workers[2];for(unsigned i{};i!=2;++i){workers[i]=std::thread{[&,i]{errors[i]=enter(i!=0);}};}
    for(auto& worker:workers){worker.join();}
    UWVM2TEST_REQUIRE(errors[0]==0 && errors[1]==0 && host_calls==2);
    lib::reset_runtime_state_host_api();
    std::printf("PASS %s tuple tails: cross-module, host adapter, unaligned guarded result buffers, concurrent entries\n",indirect?"indirect":"direct");
}
