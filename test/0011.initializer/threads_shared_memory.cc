// Shared-memory syntax and initializer metadata; all executions run in the Linux test cgroup.
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/uwvm/cmdline/callback/wasm_feature.h>
#include <cstdio>
#include <thread>
#include <latch>
namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    using bytes = byte_vec;
    using parse_code = uwvm2::parser::wasm::base::wasm_parse_error_code;
    bytes module(bytes const& type, bool imported)
    {
        bytes out{};
        for(unsigned b:{0,97,115,109,1,0,0,0}) { append_u8(out,b); }
        bytes section{}; append_u8(section,1);
        if(imported) { for(unsigned b:{1,109,1,120,2}) { append_u8(section,b); } }
        section.insert(section.end(),type.begin(),type.end());
        append_u8(out,imported ? 2 : 5); append_u32_leb(out,section.size());
        out.insert(out.end(),section.begin(),section.end()); return out;
    }
    bytes type(unsigned flags,unsigned min,unsigned max)
    {
        bytes result{}; append_u8(result,flags); append_u32_leb(result,min);
        if(flags & 1u) { append_u32_leb(result,max); } return result;
    }
    bool parse(bytes const& wasm,bool threads,parse_code* code=nullptr)
    {
        auto features{make_wasm1p1_feature_parameter()};
        uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_threads=!threads;
        uwvm2::parser::wasm::base::error_impl error{};
        try
        {
            auto parsed=uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(wasm.data(),wasm.data()+wasm.size(),error,features);
            // Instantiate every details formatter too: memory metadata changed its type.
            auto details=fast_io::u8concat_fast_io(uwvm2::parser::wasm::binfmt::ver1::section_details(parsed));
            if(details.empty()) { std::abort(); }
            return true;
        }
        catch(fast_io::error const&) { if(code!=nullptr) {*code=error.err_code;} return false; }
    }
}
int main()
{
    unsigned checked{};
    for(bool imported:{false,true}) for(unsigned flags{};flags<256;++flags)
    {
        auto wasm=module(type(flags,0,1),imported);
        UWVM2TEST_REQUIRE(parse(wasm,true)==(flags==0 || flags==1 || flags==3)); ++checked;
        parse_code code{}; bool accepted=parse(wasm,false,&code);
        UWVM2TEST_REQUIRE(accepted==(flags==0 || flags==1)); ++checked;
        if(flags==3) {UWVM2TEST_REQUIRE(code==parse_code::wasm1p1_feature_required);}
    }
    for(bool imported:{false,true})
    {
        for(auto [min,max]:{std::pair{0u,0u}, {1u,1u}, {65536u,65536u}})
        { UWVM2TEST_REQUIRE(parse(module(type(3,min,max),imported),true)); ++checked; }
        for(auto [min,max]:{std::pair{2u,1u}, {0u,65537u}, {65537u,65537u}})
        { UWVM2TEST_REQUIRE(!parse(module(type(3,min,max),imported),true)); ++checked; }
        auto complete=type(3,65536,65536);
        for(std::size_t n{};n<complete.size();++n)
        { auto truncated=complete;truncated.resize(n);UWVM2TEST_REQUIRE(!parse(module(truncated,imported),true));++checked; }
        for(auto encoded: {std::initializer_list<unsigned>{3,0x80,0x80,0x80,0x80,0x10,0},
                          {3,0,0x80,0x80,0x80,0x80,0x10}, {3,0x80,0x80,0x80,0x80,0x80,0,0},
                          {3,0,0x80,0x80,0x80,0x80,0x80,0}})
        {bytes malformed{};for(auto b:encoded){append_u8(malformed,b);}UWVM2TEST_REQUIRE(!parse(module(malformed,imported),true));++checked;}
        bytes padded{};for(auto b:{3,0x80,0x80,0x80,0x80,0,0x81,0x80,0x80,0x80,0}){append_u8(padded,b);}
        UWVM2TEST_REQUIRE(parse(module(padded,imported),true));++checked;
    }
    auto features{make_wasm1p1_feature_parameter()};
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_threads=false;
    for(bool shared:{false,true})
    {
        auto wasm=module(type(shared ? 3 : 1,1,2),false);
        auto prepared=prepare_runtime_from_wasm(wasm,u8"shared-memory",{},features);
        auto const& memory=prepared.mod->local_defined_memory_vec_storage.index_unchecked(0);
        UWVM2TEST_REQUIRE(memory.memory_type_ptr->shared==shared);
        UWVM2TEST_REQUIRE(memory.memory.sequentially_consistent_size==shared);
        UWVM2TEST_REQUIRE(memory.effective_limits.min==1 && memory.effective_limits.max==2);
    }
    for(bool imported:{false,true})
    {
        auto wasm=module(type(3,1,2),imported);
        UWVM2TEST_REQUIRE(run_in_child_expect_trap_message("shared memory", [&]
        {
            uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());
            uwvm2::parser::wasm::base::error_impl error{};
            auto parsed=uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(wasm.data(),wasm.data()+wasm.size(),error,features);
            auto disabled=features;
            uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(disabled).disable_threads=true;
            uwvm2::uwvm::runtime::initializer::details::enforce_wasm1p1_initializer_feature_parameters(parsed,disabled);
        })==0);
    }
    // Sharedness is invariant in import matching; limits retain normal subtyping.
    for(bool provided_shared:{false,true}) for(bool expected_shared:{false,true})
    {
        auto provider=module(type(provided_shared ? 3 : 1,1,2),false);
        for(unsigned b:{7,5,1,1,120,2,0}) { append_u8(provider,b); } // export memory "x"
        auto consumer=module(type(expected_shared ? 3 : 1,1,3),true); // import "m"."x"
        if(provided_shared==expected_shared)
        {
            auto linked=prepare_runtime_from_wasm(consumer,u8"consumer",{{&provider,u8"m",&features}},features);
            auto const& imported=linked.mod->imported_memory_vec_storage.index_unchecked(0);
            UWVM2TEST_REQUIRE(imported.target.defined_ptr!=nullptr);
            UWVM2TEST_REQUIRE(imported.target.defined_ptr->memory_type_ptr->shared==provided_shared);
        }
        else
        {
            UWVM2TEST_REQUIRE(run_in_child_expect_trap_message("type mismatch",[&]
            {
                uwvm2::uwvm::io::u8log_output.reopen(fast_io::io_dup,fast_io::u8err());
                (void)prepare_runtime_from_wasm(consumer,u8"consumer",{{&provider,u8"m",&features}},features);
            })==0);
        }
    }
    for(bool shared:{false,true}) for(bool imported:{false,true})
    {
        auto target=module(type(shared?3:1,1,2),imported);
        bytes segments{};append_u8(segments,2);
        for(unsigned segment{};segment!=2;++segment)
        {
            for(unsigned b:{0u,0x41u,16u+2u*segment,0x0bu,4u}){append_u8(segments,b);}
            for(unsigned b:segment==0?std::initializer_list<unsigned>{0xaa,0xbb,0xcc,0xdd}:std::initializer_list<unsigned>{0x11,0x22,0x33,0x44})
            {append_u8(segments,b);}
        }
        append_u8(target,11);append_u32_leb(target,segments.size());target.insert(target.end(),segments.begin(),segments.end());
        auto provider=module(type(shared?3:1,1,2),false);
        for(unsigned b:{7,5,1,1,120,2,0}){append_u8(provider,b);}
        auto prepared=imported?prepare_runtime_from_wasm(target,u8"data-consumer",{{&provider,u8"m",&features}},features):
            prepare_runtime_from_wasm(target,u8"data-local",{},features);
        auto const& owner=imported?*prepared.mod->imported_memory_vec_storage.index_unchecked(0).target.defined_ptr:
            prepared.mod->local_defined_memory_vec_storage.index_unchecked(0);
        unsigned at=16;
        for(unsigned b:{0xaa,0xbb,0x11,0x22,0x33,0x44}){UWVM2TEST_REQUIRE(std::to_integer<unsigned>(owner.memory.memory_begin[at++])==b);}
        for(auto const& segment:prepared.mod->local_defined_data_vec_storage){UWVM2TEST_REQUIRE(segment.data.dropped);}
    }
    // Force growth to contend with the initializer's real snapshot helper.
    // The borrowed allocation must survive the copy and be preserved by grow.
    {
        uwvm2::object::memory::linear::allocator_memory_t memory{};
        memory.sequentially_consistent_size=true;memory.init_by_page_count(1);
        std::latch pinned{1},may_copy{1};
        std::thread writer{[&]
        {
            uwvm2::uwvm::runtime::initializer::details::with_native_initialization_memory(memory,[&](std::byte* base,std::size_t length)
            {
                if(length!=65536){std::abort();}
                pinned.count_down();may_copy.wait();std::memset(base,0x5a,length);
            });
        }};
        pinned.wait();
        std::thread grower{[&]{if(!memory.try_grow_silently(1,2*65536)){std::abort();}}};
        while(!memory.growing_flag_p->test(std::memory_order_acquire)){std::this_thread::yield();}
        may_copy.count_down();writer.join();grower.join();
        UWVM2TEST_REQUIRE(memory.get_page_size()==2);
        for(std::size_t i{};i!=65536;++i){UWVM2TEST_REQUIRE(memory.memory_begin[i]==std::byte{0x5a});}
        for(std::size_t i=65536;i!=131072;++i){UWVM2TEST_REQUIRE(memory.memory_begin[i]==std::byte{});}
    }
    // MVP-only feature packs continue to use their original limits-only type.
    using old_type=uwvm2::parser::wasm::standard::wasm1::type::memory_type;
    static_assert(!uwvm2::uwvm::runtime::initializer::details::wasm_memory_is_shared(old_type{}));
    std::printf("shared-memory parser checks=%u initializer metadata=2 gates=2 import-match=4 data-segments=4 pinned-grow=1 PASS\n",checked);
}
