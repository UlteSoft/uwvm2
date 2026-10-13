#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <atomic>
#include <thread>
namespace
{
    namespace strict = uwvm2test::uwvm_int_strict;
    namespace lib = uwvm2::runtime::lib;
    namespace mode = uwvm2::uwvm::runtime::runtime_mode;
    namespace wasm_type = uwvm2::uwvm::wasm::type;
    using wasm1 = uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using features_t = wasm_type::feature_list<wasm1>;
    using result_t = wasm_type::import_function_result_tuple_t<features_t>;
    using params_t = wasm_type::import_function_parameter_tuple_t<features_t>;
    using host_call_t = wasm_type::local_imported_function_type_t<result_t,params_t>;
    std::atomic<unsigned> probes{};
    struct probe
    {
        inline static constexpr uwvm2::utils::container::u8string_view function_name{u8"probe"};
        using result_tuple=result_t;
        using parameter_tuple=params_t;
        using local_imported_function_type=host_call_t;
        static void call(host_call_t&) noexcept
        {
            std::byte stack_marker{};
            thread_local std::uintptr_t first{};
            auto current=reinterpret_cast<std::uintptr_t>(&stack_marker);
            if(first==0) {first=current;}
            auto distance=current>first ? current-first : first-current;
            // Ordinary host callback depth may differ for the terminal host-tail
            // case. A growing native tail-call stack exceeds this within 100 calls.
            if(distance>65536 || lib::runtime_execution_stop_requested_host_api()) {std::abort();}
            ++probes;
        }
    };
    struct host_module
    {
        uwvm2::utils::container::u8string_view module_name{u8"tail-host"};
        using local_function_tuple=uwvm2::utils::container::tuple<probe>;
    };
    strict::func_body body(std::initializer_list<unsigned> values)
    {
        strict::func_body result{};
        for(auto v:values) {strict::append_u8(result.code,v);}
        return result;
    }
    strict::byte_vec provider()
    {
        strict::module_builder m{};
        m.add_func({{strict::k_val_i64},{strict::k_val_i64}},body({0x20,0,0x42,3,0x7c,0x0b}));
        m.add_export_func(0,"finish");
        return m.build();
    }
    strict::byte_vec consumer(bool indirect)
    {
        strict::module_builder m{};
        m.types.push_back({{}, {}});
        m.types.push_back({{strict::k_val_i64},{strict::k_val_i64}});
        m.add_import_func("tail-host","probe",0);
        m.add_import_func("tail-provider","finish",1);
        auto a=body({0x20,2,0x45,0x04,0x40,0x05,0x00,0x0b, // local starts zero
                     0x41,23,0x21,2,0x10,0, // dirty local, ordinary host callback
                     0x20,0,0x45,0x04,0x40,0x20,1,0x12,1,0x0b, // tail through Wasm import
                     0x42,37, // unrelated old operand gets discarded
                     0x20,0,0x41,1,0x6b,0x20,1,0x42,7,0x7c,
                     0x44,0,0,0,0,0,0,0x22,0x40, // f64.const 9
                     0x41,11,0x12,3,0x0b});
        a.locals.push_back({1,strict::k_val_i32});
        m.add_func({{strict::k_val_i32,strict::k_val_i64},{strict::k_val_i64}},std::move(a));
        strict::func_body b{};
        b.locals.push_back({4096,strict::k_val_i64});
        strict::append_u8(b.code,0x20); strict::append_u32_leb(b.code,4099);
        for(auto v:{0x50,0x04,0x40,0x05,0x00,0x0b,0x42,23,0x21}) {strict::append_u8(b.code,v);}
        strict::append_u32_leb(b.code,4099);
        for(auto v:{0x20,2,0x44,0,0,0,0,0,0,0x22,0x40,0x62,0x04,0x40,0x00,0x0b,
                    0x20,3,0x41,11,0x47,0x04,0x40,0x00,0x0b,
                    0x20,0,0x20,1,0x12,2,0x0b}) {strict::append_u8(b.code,v);}
        m.add_func({{strict::k_val_i32,strict::k_val_i64,strict::k_val_f64,strict::k_val_i32},{strict::k_val_i64}},std::move(b));
        auto entry=body({0x41});strict::append_i32_leb(entry.code,10000);
        for(auto v:{0x42,13,0x12,2,0x0b}) {strict::append_u8(entry.code,v);}
        m.add_func({{},{strict::k_val_i64}},std::move(entry));
        // A tail call directly into a host function has no successor opcode.
        m.add_func({{},{}},body({0x12,0,0x0b}));
        if(indirect)
        {
            m.has_table=true; m.table_min=5;
            strict::element_segment elements{};
            elements.offset_expr=body({0x41,0,0x0b}).code;
            elements.func_indices={0,1,2,3};
            m.elements.push_back(std::move(elements));
            for(auto& function:m.function_bodies)
            {
                // These fixture bodies contain no 0x12 immediate bytes: replace
                // each known direct tail instruction with the corresponding
                // dynamic table selector and type, preserving the argument tuple.
                strict::byte_vec translated{};
                for(std::size_t i{};i!=function.code.size();++i)
                {
                    auto value=std::to_integer<unsigned>(function.code[i]);
                    if(value!=0x12) {translated.push_back(function.code[i]);continue;}
                    auto target=std::to_integer<unsigned>(function.code[++i]);
                    if(target>3) {std::abort();}
                    for(auto byte:{0x41u,target,0x13u,target,0x80u,0u}) {strict::append_u8(translated,byte);}
                }
                function.code=std::move(translated);
            }
        }
        return m.build();
    }
    std::uint64_t enter(unsigned function, bool result)
    {
        std::uint64_t value{};
#if defined(UWVM2TEST_EXECUTION_LAZY)
        lib::lazy_compile_run_config config{};
#else
        lib::full_compile_run_config config{};
#endif
        config.entry_function_index=function;
        config.entry_abi_buffers.result_buffer=result ? reinterpret_cast<std::byte*>(&value) : nullptr;
        config.entry_abi_buffers.result_bytes=result ? 8 : 0;
#if defined(UWVM2TEST_EXECUTION_LAZY)
        lib::lazy_compile_and_run_main_module(u8"tail-chain",config);
#else
        lib::full_compile_and_run_main_module(u8"tail-chain",config);
#endif
        return value;
    }
}
int main(int argc,char** argv)
{
#if defined(UWVM2TEST_TAIL_INDIRECT)
    constexpr bool indirect{true};
#else
    bool const indirect{argc>1 && std::strcmp(argv[1],"indirect")==0};
#endif
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
    mode::global_runtime_llvm_jit_call_stack = ((argc>1 && std::strcmp(argv[1],"unwind")==0) || (argc>2 && std::strcmp(argv[2],"unwind")==0)) ?
        mode::runtime_llvm_jit_call_stack_t::unwind : mode::runtime_llvm_jit_call_stack_t::instruction;
#endif
    auto provider_bytes=provider(), main_bytes=consumer(indirect);
    auto features=strict::make_wasm1p1_feature_parameter();
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_tail_call=false;
    wasm_type::local_imported_t host{host_module{}};
    auto prepared=strict::prepare_runtime_from_wasm(main_bytes,u8"tail-chain",{{&provider_bytes,u8"tail-provider",&features}},features,{host});
#if defined(UWVM2TEST_EXECUTION_LAZY)
    mode::global_runtime_mode=mode::runtime_mode_t::lazy_compile;
#endif
#if defined(UWVM2TEST_EXECUTION_TIERED)
    mode::global_runtime_compiler=mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered;
    for(int i{1};i<argc;++i)
    {
        if(std::strstr(argv[i],"no-t0")) {mode::runtime_tiered_disable_uwvm_int_lazy_interpreter=true;}
        if(std::strstr(argv[i],"no-t2")) {mode::runtime_tiered_disable_llvm_full_jit=true;}
    }
#endif
    mode::global_runtime_compile_threads_resolved=2;
    UWVM2TEST_REQUIRE(enter(4,true)==70016);
    UWVM2TEST_REQUIRE(enter(5,false)==0);
    std::uint64_t values[2]{};
    std::thread workers[2];
    for(unsigned i{};i!=2;++i) {workers[i]=std::thread{[&,i]{values[i]=enter(4,true);}};}
    for(auto& worker:workers) {worker.join();}
    UWVM2TEST_REQUIRE(values[0]==70016 && values[1]==70016);
    UWVM2TEST_REQUIRE(probes==30004);
    lib::reset_runtime_state_host_api();
    std::printf("PASS %s tail transfer: changing signatures/frames, 60006 Wasm transfers, cross-module import, host tail, concurrent bounded host stacks\n",indirect?"indirect":"direct");
}
