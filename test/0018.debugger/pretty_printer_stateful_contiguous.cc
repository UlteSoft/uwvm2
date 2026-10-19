// Owned metadata/bytes only. Execute solely in the admitted Linux cgroup.
#include <uwvm2/uwvm/debugger/source_language_expression.h>
#include <cstdio>
#include <cstdlib>
namespace dwarf=::uwvm2::uwvm::debugger::source_dwarf;
namespace pretty=::uwvm2::uwvm::debugger::source_pretty_printer;
namespace expression=::uwvm2::uwvm::debugger::source_language_expression;
static void check(bool value,char const* text)
{ if(!value) { ::std::fprintf(stderr,"stateful contiguous FAIL: %s\n",text);::std::exit(1); } }
static dwarf::type_record object(char const* name,unsigned bytes)
{ dwarf::type_record t{};t.name=name;t.kind=dwarf::type_kind::structure;t.language=0x21u;t.size_known=true;t.byte_size=bytes;return t; }
static dwarf::member_record member(char const* name,::std::size_t type,unsigned offset,bool inherited=false)
{ dwarf::member_record m{};m.name=name;m.type=type;m.byte_offset=offset;m.offset_known=true;m.inherited=inherited;return m; }
struct fixture
{
    unsigned width;::std::size_t reads{};
    ::std::vector<dwarf::type_record> types{};
    ::std::vector<::std::byte> bytes{},known{},eligible{},memory{};
    ::std::vector<dwarf::object_node> tree{};
    void put(::std::vector<::std::byte>& out,unsigned offset,::std::uint64_t value,unsigned count)
    { check(offset<=out.size() && count<=out.size()-offset,"bounded bytes");for(unsigned i{};i!=count;++i) { out[offset+i]=static_cast<::std::byte>(value>>(i*8u)); } }
    explicit fixture(unsigned w):width(w),memory(1024u)
    {
        auto integer{object("int",4u)};integer.kind=dwarf::type_kind::scalar;integer.encoding=5u;integer.byte_count=4u;
        auto pointer{object("int*",w)};pointer.kind=dwarf::type_kind::pointer;pointer.byte_count=w;pointer.referenced_type=0u;
        types={integer,pointer,integer,object("stateful_allocator<int>",4u)};
        types[3u].members={member("state",2u,0u)};
        auto first{object("__compressed_pair_elem<int*, 0, false>",w)};
        first.members={member("__value_",1u,0u)};first.template_types={1u};first.template_values={{1u,2u,0u,true,true}};
        auto second{object("__compressed_pair_elem<allocator, 1, false>",4u)};
        second.members={member("__value_",3u,0u)};second.template_types={3u};second.template_values={{1u,2u,1u,true,true}};
        types.push_back(::std::move(first));types.push_back(::std::move(second));
        auto pair{object("__compressed_pair<int*, allocator>",w*2u)};pair.template_types={1u,3u};
        pair.members={member("",4u,0u,true),member("",5u,w,true)};types.push_back(::std::move(pair));
        auto vector{object("std::__1::vector<int, stateful_allocator<int> >",w*4u)};vector.template_types={0u,3u};
        vector.members={member("__begin_",1u,0u),member("__end_",1u,w),member("__end_cap_",6u,w*2u)};types.push_back(::std::move(vector));
        bytes.resize(w*4u);known.assign(bytes.size(),::std::byte{255u});eligible=known;
        put(bytes,0u,256u,w);put(bytes,w,268u,w);put(bytes,w*2u,288u,w);put(bytes,w*3u,17u,4u);
        for(unsigned i{};i!=3u;++i) { put(memory,256u+i*4u,(i+1u)*11u,4u); }
    }
    bool read(::std::uint64_t offset,::std::size_t count,::std::vector<::std::byte>& out)
    {
        ++reads;if(offset>memory.size() || count>memory.size()-offset) { return false; }
        out.assign(memory.begin()+static_cast<::std::size_t>(offset),memory.begin()+static_cast<::std::size_t>(offset)+count);return true;
    }
    pretty::outcome format()
    {
        reads=0u;check(dwarf::query_object_value_with_known_bits(types,7u,bytes,known,tree)==dwarf::inline_query_error::none,"raw metadata");
        auto reader{[&](::std::uint64_t offset,::std::size_t count,::std::vector<::std::byte>& out) { return read(offset,count,out); }};
        return pretty::format(types,7u,bytes,known,eligible,width,reader,tree);
    }
};
int main()
{
    for(unsigned width:{4u,8u})
    {
        fixture f{width};pretty::details::field ambiguous{};
        check(!pretty::details::path(f.types,7u,"__end_cap_.__value_",ambiguous),"two __value_ carriers stay generically ambiguous");
        check(f.format()==pretty::outcome::formatted && f.reads==1u,"typed capacity prints actual first carrier");
        ::std::vector<::std::uint64_t> values{};
        for(auto const& node:f.tree) { if(node.detached_copy && node.parent==0u) { values.push_back(node.bits); } }
        check(values==::std::vector<::std::uint64_t>{11u,22u,33u},"ordered copied vector payloads");
        pretty::details::indexed_sequence plan{};
        check(pretty::details::index_plan(f.types,7u,f.bytes,f.known,f.eligible,width,plan) && plan.count==3u && plan.address==256u,"typed capacity index plan");
        dwarf::source_expression selector{};check(dwarf::parse_source_expression("value[1]",selector)==dwarf::object_selector_error::none,"parse selected element");
        auto read{[&](::std::uint64_t offset,::std::size_t count,::std::vector<::std::byte>& out) { return f.read(offset,count,out); }};
        ::std::vector<dwarf::object_node> selected{};f.reads=0u;
        check(expression::value(f.types,7u,selector.steps,f.bytes,f.known,width,read,selected,{},f.eligible)==dwarf::inline_query_error::none &&
            f.reads==1u && selected.size()==1u && selected[0u].value_available && selected[0u].bits==22u,"selected value copies exactly one real element");
        f.eligible[width*2u]=::std::byte{};
        check(f.format()!=pretty::outcome::formatted && f.reads==0u && !pretty::details::index_plan(f.types,7u,f.bytes,f.known,f.eligible,width,plan),"unknown pointer-qualified capacity rejected without reads");
        f.eligible=f.known;f.types[4u].template_values[0u].bits=1u;
        check(f.format()!=pretty::outcome::formatted && f.reads==0u && !pretty::details::index_plan(f.types,7u,f.bytes,f.known,f.eligible,width,plan),"wrong first ordinal rejected without reads");
        f.types[4u].template_values[0u].bits=0u;f.types[6u].members.push_back(f.types[6u].members[0u]);
        check(f.format()!=pretty::outcome::formatted && f.reads==0u,"duplicate typed first carrier rejected without reads");
    }
    ::std::puts("{\"passed\":true,\"scope\":\"32/64-bit stateful contiguous copied DATA; no live ABI qualification\"}");
}
