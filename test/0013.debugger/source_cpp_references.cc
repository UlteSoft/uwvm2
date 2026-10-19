// Pure copied-DATA reference semantics; real compiler producer acceptance is separate.
#include <uwvm2/uwvm/debugger/source_language_expression.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>
namespace dwarf=::uwvm2::uwvm::debugger::source_dwarf;
namespace language=::uwvm2::uwvm::debugger::source_language_expression;
namespace
{
    void check(bool good,char const* why)
    {if(!good) {::std::fprintf(stderr,"C++ references: %s\n",why);::std::exit(1);}}
    dwarf::type_record object(char const* name,unsigned bytes,::std::vector<dwarf::member_record> fields={})
    {dwarf::type_record t{};t.language=0x21u;t.name=name;t.kind=dwarf::type_kind::structure;t.size_known=true;t.byte_size=bytes;t.members=::std::move(fields);return t;}
    dwarf::member_record member(char const* name,::std::size_t type,unsigned offset)
    {dwarf::member_record m{};m.name=name;m.type=type;m.byte_offset=offset;m.offset_known=true;return m;}
    dwarf::type_record pointer(::std::size_t target,unsigned width,bool reference=false)
    {auto t{object("pointer",width)};t.kind=dwarf::type_kind::pointer;t.byte_count=width;t.referenced_type=target;t.reference_type=reference;return t;}
    void put(::std::vector<::std::byte>& bytes,unsigned at,unsigned width,::std::uint64_t value)
    {check(at<=bytes.size() && width<=bytes.size()-at,"fixture extent");for(unsigned i{};i!=width;++i) {bytes[at+i]=static_cast<::std::byte>((value>>(8u*i))&255u);}}
    dwarf::source_expression_step index(::std::int64_t value)
    {dwarf::source_expression_step s{};s.kind=dwarf::source_expression_step_kind::index;s.index=value;return s;}
    dwarf::source_expression_step field(char const* name)
    {dwarf::source_expression_step s{};s.kind=dwarf::source_expression_step_kind::member;s.member=name;return s;}
    struct fixture
    {
        unsigned width{},calls{};bool fail_read{};
        ::std::vector<dwarf::type_record> types{};
        ::std::vector<::std::byte> memory=::std::vector<::std::byte>(2048u),carrier{},known{},eligible{};
        ::std::vector<dwarf::object_node> out{};
        explicit fixture(unsigned w):width(w),carrier(w),known(w,::std::byte{255u}),eligible(known)
        {
            auto integer{object("int",4u)};integer.kind=dwarf::type_kind::scalar;integer.byte_count=4u;integer.encoding=5u;types.push_back(integer);
            types.push_back(pointer(0u,w));
            types.push_back(object("vector impl",w*3u,{member("_M_start",1u,0u),member("_M_finish",1u,w),member("_M_end_of_storage",1u,w*2u)}));
            types.push_back(object("std::vector<int>",w*3u,{member("_M_impl",2u,0u)}));types.back().template_types={0u};
            types.push_back(pointer(3u,w,true)); // 4 vector reference
            types.push_back(object("Holder",w,{member("values",4u,0u)}));
            types.push_back(pointer(5u,w,true)); // 6 holder reference
            auto array{object("int[3]",12u)};array.kind=dwarf::type_kind::array;array.referenced_type=0u;
            dwarf::dimension_record bound{};bound.lower_bound_known=true;bound.count_known=true;bound.count=3u;array.dimensions.push_back(bound);types.push_back(array);
            types.push_back(object("std::array<int,3>",12u,{member("_M_elems",7u,0u)}));types.back().template_types={0u};types.back().template_values.push_back({1u,0u,3u,false,true});
            types.push_back(pointer(8u,w,true)); // 9 array reference
            types.push_back(pointer(0u,w,true)); // 10 scalar reference
            types.push_back(pointer(3u,w)); // 11 ordinary pointer
            put(carrier,0u,w,256u);put(memory,256u,w,512u);put(memory,256u+w,w,524u);put(memory,256u+w*2u,w,524u);
            for(unsigned i{};i!=3u;++i) {put(memory,512u+i*4u,4u,7u+i);put(memory,1536u+i*4u,4u,16u+i);}
            put(memory,1024u,w,256u);put(memory,1792u,4u,19u);
        }
        dwarf::inline_query_error run(unsigned root,::std::vector<dwarf::source_expression_step> const& steps={},language::limits cap={})
        {
            calls=0u;auto read{[&](::std::uint64_t address,::std::size_t count,::std::vector<::std::byte>& bytes)
            {++calls;if(fail_read || address>memory.size() || count>memory.size()-address) {return false;}
             bytes.assign(memory.begin()+static_cast<::std::size_t>(address),memory.begin()+static_cast<::std::size_t>(address)+count);return true;}};
            return language::value(types,root,steps,carrier,known,static_cast<::std::uint8_t>(width),read,out,cap,eligible);
        }
    };
    void run(unsigned width)
    {
        fixture f{width};
        check(f.run(4u)==dwarf::inline_query_error::none && f.calls==2u && !f.out.empty() && f.out[0u].type==3u && f.out[0u].pretty_available,
              "bare vector reference reads descriptor then bounded elements and prints the referent");
        check(f.run(4u,{index(1)})==dwarf::inline_query_error::none && f.calls==2u && f.out.size()==1u && f.out[0u].type==0u && f.out[0u].bits==8u,
              "vector reference index unwraps before container planning");
        f.types[4u].rvalue_reference_type=true;
        check(f.run(4u,{index(1)})==dwarf::inline_query_error::none && f.calls==2u && f.out[0u].bits==8u,"rvalue references have the same transparent value semantics");
        f.types[4u].rvalue_reference_type=false;
        check(f.run(5u,{field("values")})==dwarf::inline_query_error::none && f.calls==2u && f.out[0u].type==3u,"final selected reference field unwraps once");
        check(f.run(5u,{field("values"),index(1)})==dwarf::inline_query_error::none && f.calls==2u && f.out[0u].bits==8u,"member plus index shares one reference read");
        put(f.carrier,0u,width,1024u);
        check(f.run(6u,{field("values"),index(1)})==dwarf::inline_query_error::none && f.calls==3u && f.out[0u].bits==8u,"nested reference/member/index reads do not reset the allowance");
        put(f.carrier,0u,width,1536u);
        check(f.run(9u,{index(1)})==dwarf::inline_query_error::none && f.calls==1u && f.out[0u].bits==17u,"array reference indexing selects already copied storage");
        put(f.carrier,0u,width,1792u);
        check(f.run(10u)==dwarf::inline_query_error::none && f.calls==1u && f.out[0u].bits==19u,"scalar reference displays its value");
        put(f.carrier,0u,width,256u);
        language::limits cap{};cap.raw=true;
        check(f.run(4u,{},cap)==dwarf::inline_query_error::none && f.calls==1u && f.out[0u].type==3u && !f.out[0u].pretty_available,"raw formatting retains transparent reference semantics");
        cap.raw=false;cap.maximum_reads=0u;
        check(f.run(4u,{},cap)==dwarf::inline_query_error::limit_exceeded && f.calls==0u && f.out.empty(),"zero allowance performs zero callbacks");
        cap.maximum_reads=1u;
        check(f.run(4u,{index(1)},cap)==dwarf::inline_query_error::limit_exceeded && f.calls==1u && f.out.empty(),"descriptor and index reads share the same allowance");
        cap.maximum_reads=32u;f.fail_read=true;
        check(f.run(4u,{},cap)==dwarf::inline_query_error::unavailable && f.calls==1u && f.out.empty(),"failed referent copy publishes no partial result");
        f.fail_read=false;f.known[0u]=::std::byte{};
        check(f.run(4u,{},cap)!=dwarf::inline_query_error::none && f.calls==0u,"eligible bits cannot replace unknown value bits");
        f.known=f.eligible;f.eligible[0u]=::std::byte{};
        check(f.run(4u,{},cap)!=dwarf::inline_query_error::none && f.calls==0u,"known bytes cannot replace denied carrier eligibility");
        f.eligible=f.known;put(f.carrier,0u,width,0u);
        check(f.run(4u,{},cap)!=dwarf::inline_query_error::none && f.calls==0u,"null carrier never reads a referent");
        put(f.carrier,0u,width,width==4u?0xfffffff8ull:0xfffffffffffffff8ull);
        check(f.run(4u,{},cap)!=dwarf::inline_query_error::none && f.calls==0u,"entire referent must fit the declared guest address width");
        put(f.carrier,0u,width,256u);f.types[4u].address_class_known=true;f.types[4u].address_class=1u;
        check(f.run(4u,{},cap)!=dwarf::inline_query_error::none && f.calls==0u,"nonconventional reference address class cannot read");
        f.types[4u].address_class=0u;
        check(f.run(11u)==dwarf::inline_query_error::none && f.calls==0u && f.out.size()==1u && f.out[0u].kind==dwarf::type_kind::pointer && f.out[0u].bits==256u,
              "ordinary pointers remain address displays without implicit reads");
        ::std::vector<dwarf::object_node> layout{};f.calls=0u;
        check(language::type(f.types,4u,{},static_cast<::std::uint8_t>(width),layout)==dwarf::inline_query_error::none && layout[0u].type==3u && layout[0u].byte_size==width*3u && f.calls==0u,
              "sizeof/type obtains referent extent from metadata alone");
        check(language::type(f.types,4u,::std::vector<dwarf::source_expression_step>{index(1)},static_cast<::std::uint8_t>(width),layout)==dwarf::inline_query_error::none && layout[0u].type==0u && f.calls==0u,
              "index type query never reads the descriptor or an element");
        f.types.push_back(pointer(12u,width,true));put(f.carrier,0u,width,1024u);put(f.memory,1024u,width,1024u);
        check(f.run(12u)==dwarf::inline_query_error::limit_exceeded && f.calls==32u && f.out.empty(),"cyclic reference metadata/data cannot exceed the shared 32-read bound");
        check(language::type(f.types,12u,{},static_cast<::std::uint8_t>(width),layout)==dwarf::inline_query_error::limit_exceeded,"cyclic reference type walk has an independent finite hop bound");
    }
}
int main() {for(unsigned width:{4u,8u}) {run(width);}::std::puts("C++ reference copied-DATA checks passed");}
