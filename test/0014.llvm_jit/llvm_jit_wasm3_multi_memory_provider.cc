#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <array>
#include <vector>

namespace
{
    namespace strict = uwvm2test::uwvm_int_strict;
    namespace jit = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    struct memory
    {
        inline static constexpr uwvm2::utils::container::u8string_view memory_name{u8"mem"};
        inline static constexpr std::uint_least64_t page_size{65536};
        std::array<std::byte,65536> bytes{};
        friend bool memory_grow(memory&,std::uint_least64_t n) noexcept { return n==0; }
        friend std::byte* memory_begin(memory& m) noexcept { return m.bytes.data(); }
        friend std::uint_least64_t memory_size(memory&) noexcept { return 1; }
    };
    struct host
    {
        uwvm2::utils::container::u8string_view module_name{};
        using local_memory_tuple=uwvm2::utils::container::tuple<memory>;
        local_memory_tuple local_memory{};
    };
    constexpr std::array<std::array<unsigned,2>,6> indices{{{0,1},{1,0},{0,2},{2,0},{3,0},{0,3}}};
}
int main()
{
    strict::module_builder builder{};
    builder.add_import_memory("a","mem",1,1,true);
    builder.add_import_memory("a","mem",1,1,true);
    builder.add_import_memory("b","mem",1,1,true);
    builder.has_memory=true;builder.memory_min=1;builder.memory_max=1;builder.memory_has_max=true;
    for(auto pair:indices)
    {
        strict::func_type type{{0x7f,0x7f,0x7f},{}};
        strict::func_body body{};
        for(unsigned i{};i!=3;++i) { strict::append_u8(body.code,0x20);strict::append_u32_leb(body.code,i); }
        strict::append_u8(body.code,0xfc);strict::append_u8(body.code,10);
        strict::append_u32_leb(body.code,pair[0]);strict::append_u32_leb(body.code,pair[1]);strict::append_u8(body.code,0x0b);
        builder.add_func(std::move(type),std::move(body));
    }
    auto wasm{builder.build()};auto features{strict::make_wasm1p1_feature_parameter()};
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_multi_memory=false;
    using host_t=uwvm2::uwvm::wasm::type::local_imported_t;
    host_t a{host{.module_name=u8"a"}},b{host{.module_name=u8"b"}};
    auto prepared{strict::prepare_runtime_from_wasm(wasm,u8"wasm3-multi-provider",{},features,{a,b})};
    UWVM2TEST_REQUIRE(prepared.mod!=nullptr);
    jit::compile_option options{};options.validator_feature_parameter=&features;options.verify_llvm_jit_ir=true;
    uwvm2::validation::error::code_validation_error_impl error{};
    auto compiled{jit::compile_all_from_uwvm(*prepared.mod,options,error,0)};
    UWVM2TEST_REQUIRE(error.err_code==uwvm2::validation::error::code_validation_error_code::ok && compiled.llvm_jit_module.emitted);
    auto const info_a{jit::details::resolve_runtime_memory_access_info(*prepared.mod,0)};
    auto const info_alias{jit::details::resolve_runtime_memory_access_info(*prepared.mod,1)};
    auto const info_b{jit::details::resolve_runtime_memory_access_info(*prepared.mod,2)};
    auto const info_native{jit::details::resolve_runtime_memory_access_info(*prepared.mod,3)};
    UWVM2TEST_REQUIRE(info_a.local_imported_module_ptr!=nullptr && info_b.local_imported_module_ptr!=nullptr && info_native.memory_p!=nullptr);
    UWVM2TEST_REQUIRE(info_a.local_imported_module_ptr==info_alias.local_imported_module_ptr);
    unsigned transfers{};
    for(unsigned function{};function!=indices.size();++function)
    {
        for(auto offsets: {std::array<std::uint32_t,3>{1001,1000,5000},{15000,15001,5000},{65536,65536,0}})
        {
            std::array<std::vector<std::byte>,3> expected{};
            for(unsigned j{};j!=3;++j)
            {
                expected[j].resize(65536);
                for(unsigned i{};i!=65536;++i) { expected[j][i]=std::byte((i*37+j*19+11)&255); }
            }
            UWVM2TEST_REQUIRE(info_a.local_imported_module_ptr->memory_write_to_index(0,0,expected[0].data(),65536));
            UWVM2TEST_REQUIRE(info_b.local_imported_module_ptr->memory_write_to_index(0,0,expected[1].data(),65536));
            std::memcpy(info_native.memory_p->memory_begin,expected[2].data(),65536);
            auto map=[](unsigned index){return index<2 ? 0 : index-1;};
            auto dst=map(indices[function][0]),src=map(indices[function][1]);
            if(offsets[2]) { std::memmove(expected[dst].data()+offsets[0],expected[src].data()+offsets[1],offsets[2]); }
            uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,function,nullptr,0,reinterpret_cast<std::byte const*>(offsets.data()),sizeof(offsets));
            std::array<std::vector<std::byte>,3> actual{};
            for(auto& v:actual) { v.resize(65536); }
            UWVM2TEST_REQUIRE(info_a.local_imported_module_ptr->memory_read_from_index(0,0,actual[0].data(),65536));
            UWVM2TEST_REQUIRE(info_b.local_imported_module_ptr->memory_read_from_index(0,0,actual[1].data(),65536));
            std::memcpy(actual[2].data(),info_native.memory_p->memory_begin,65536);
            if(actual!=expected) { std::fprintf(stderr,"function=%u dst=%u src=%u len=%u\n",function,offsets[0],offsets[1],offsets[2]);return 1; }
            ++transfers;
        }
    }
    // The actual generated new memory.copy instructions must reject either invalid range, including len=0 endpoints.
    for(auto offsets: {std::array<std::uint32_t,3>{65535,0,2},{0,65535,2},{65537,0,0},{0,65537,0}})
    {
        for(unsigned function{};function!=indices.size();++function)
        {
            UWVM2TEST_REQUIRE(strict::run_in_child_expect_trap_message("memory access out of bounds",[&]
            { uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod,function,nullptr,0,reinterpret_cast<std::byte const*>(offsets.data()),sizeof(offsets)); })==0);
        }
    }
    std::printf("PASS Core 3 imported multi-memory: %u alias/provider/native transfers and 24 generated-code bounds traps\n",transfers);
}
