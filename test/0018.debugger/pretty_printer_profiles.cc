// Proposed r3 fixture: copied DATA only, run on authorized SSH Linux cgroup.
#include <uwvm2/uwvm/debugger/source_pretty_printer.h>
#include <cstdio>
#include <cstdlib>
namespace dwarf=::uwvm2::uwvm::debugger::source_dwarf;
namespace pretty=::uwvm2::uwvm::debugger::source_pretty_printer;
namespace
{
    void check(bool value,char const* text)
    { if(!value) { ::std::fprintf(stderr,"pretty_printer_profiles FAIL: %s\n",text);::std::exit(1); } }
    dwarf::type_record scalar(char const* name,unsigned width,unsigned encoding=7u)
    { dwarf::type_record out{};out.name=name;out.kind=dwarf::type_kind::scalar;out.language=0x21u;out.byte_size=width;
      out.byte_count=width;out.encoding=encoding;out.size_known=true;return out; }
    dwarf::member_record member(char const* name,unsigned type,unsigned offset,bool inherited=false)
    { dwarf::member_record out{};out.name=name;out.type=type;out.byte_offset=offset;out.offset_known=true;out.inherited=inherited;return out; }
    dwarf::type_record object(char const* name,unsigned size,::std::vector<dwarf::member_record> fields={})
    { dwarf::type_record out{};out.name=name;out.kind=dwarf::type_kind::structure;out.language=0x21u;out.byte_size=size;
      out.size_known=true;out.members=::std::move(fields);return out; }
    void put(::std::vector<::std::byte>& bytes,unsigned offset,unsigned width,::std::uint64_t value)
    { check(offset<=bytes.size() && width<=bytes.size()-offset && width<=8u,"bounded fixture encoding");
      for(unsigned i{};i!=width;++i) { bytes[offset+i]=static_cast<::std::byte>((value>>(i*8u))&255u); } }
    struct fixture
    {
        unsigned width{},root{},calls{};bool selected_offset{};
        ::std::vector<dwarf::type_record> types{};::std::vector<::std::byte> bytes{},known{},eligible{},memory{};
        ::std::vector<dwarf::object_node> tree{};
        explicit fixture(unsigned w):width(w),memory(4096u)
        {
            types={scalar("int",4u,5u),scalar("size_t",w)};
            auto p{object("",w)};p.kind=dwarf::type_kind::pointer;p.byte_count=w;p.referenced_type=0u;types.push_back(::std::move(p));
        }
        void seal()
        { root=types.size()-1u;bytes.resize(types[root].byte_size);known.assign(bytes.size(),::std::byte{255u});eligible=known; }
        pretty::outcome format()
        {
            calls=0u;check(dwarf::query_object_value_with_known_bits(types,root,bytes,known,tree)==dwarf::inline_query_error::none,"owned raw tree");
            if(selected_offset) { for(auto& node:tree) { node.byte_offset+=16u; } }
            auto read{[&](::std::uint64_t at,::std::size_t count,::std::vector<::std::byte>& out)
            { ++calls;if(at>memory.size() || count>memory.size()-at) { return false; }
              out.assign(memory.begin()+static_cast<::std::size_t>(at),memory.begin()+static_cast<::std::size_t>(at)+count);return true; }};
            return pretty::format(types,root,bytes,known,eligible,width,read,tree);
        }
    };
    void unique(unsigned width)
    {
        fixture f{width};
        f.types.push_back(object("std::default_delete<int>",1u));
        f.types.push_back(object("_Head_base<0,int*>",width,{member("_M_head_impl",2u,0u)}));
        f.types.push_back(object("_Head_base<1,deleter>",1u,{member("_M_head_impl",3u,0u)}));
        f.types.push_back(object("std::tuple<int*,deleter>",width,{member("pointer_head",4u,0u,true),member("deleter_head",5u,0u,true)}));
        f.types.push_back(object("__uniq_ptr_impl",width,{member("_M_t",6u,0u)}));
        auto owner{object("std::unique_ptr<int>",width,{member("_M_t",7u,0u)})};owner.template_types={0u,3u};f.types.push_back(::std::move(owner));
        f.seal();put(f.bytes,0u,width,256u);put(f.memory,256u,4u,42u);
        check(f.format()==pretty::outcome::formatted && f.calls==1u && f.tree[0u].pretty_summary.find("= 42")!=::std::string::npos,"unique typed pointer head coexists with deleter head");
        f.types[6u].members.push_back(member("second_pointer_head",4u,0u,true));
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"two valid T pointer heads are ambiguous");f.types[6u].members.pop_back();
        f.types[f.root].template_types[0u]=1u;
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"root template T must equal pointer pointee");f.types[f.root].template_types[0u]=0u;
        f.eligible.assign(f.eligible.size(),::std::byte{});
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"typed pointer still requires eligible known bits");f.eligible=f.known;
        f.types[f.root].template_types.clear();
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"no root template metadata means unavailable ABI");
    }
    void optional_offset(unsigned width)
    {
        fixture f{width};f.types.push_back(scalar("bool",1u,2u));
        f.types.push_back(object("_Optional_payload_base_storage",4u,{member("_M_value",0u,0u)}));
        f.types.push_back(object("_Optional_payload",8u,{member("_M_payload",4u,0u),member("_M_engaged",3u,4u)}));
        f.types.push_back(object("std::optional<int>",8u,{member("_M_payload",5u,0u)}));f.seal();f.selected_offset=true;
        put(f.bytes,0u,4u,42u);put(f.bytes,4u,1u,1u);
        check(f.format()==pretty::outcome::formatted && f.calls==0u && f.tree[0u].pretty_summary=="engaged = 42","optional selected-root offset is relative for payload matching");
    }
    void fixed(unsigned width)
    {
        fixture f{width};f.types.push_back(object("std::pair<int,int>",8u,{member("first",0u,0u),member("second",0u,4u)}));f.seal();
        put(f.bytes,0u,4u,42u);put(f.bytes,4u,4u,77u);f.known.assign(f.known.size(),::std::byte{});
        check(f.format()==pretty::outcome::formatted && f.tree[0u].pretty_summary.find("no scalar preview")!=::std::string::npos && f.tree[0u].pretty_truncated,
              "unknown fixed fields do not look like an empty aggregate");
        f.known.assign(f.known.size(),::std::byte{255u});f.known[4u]=::std::byte{};
        check(f.format()==pretty::outcome::formatted && f.tree[0u].pretty_summary.find("first=42")!=::std::string::npos &&
            f.tree[0u].pretty_summary.find("additional/unavailable raw fields")!=::std::string::npos,"partial fixed preview is explicit");
        auto array{object("int[10]",40u)};array.kind=dwarf::type_kind::array;array.referenced_type=0u;array.dimensions={{0,10u,true,true}};
        f.types.push_back(::std::move(array));f.seal();
        check(f.format()==pretty::outcome::formatted && f.tree[0u].pretty_truncated &&
            f.tree[0u].pretty_summary.find("additional/unavailable raw fields")!=::std::string::npos,"eight-field preview limit is explicit");
    }
    void libcxx_vector(unsigned width,bool compressed)
    {
        fixture f{width};unsigned capacity_type{2u};
        if(compressed) { f.types.push_back(object("__compressed_pair_elem<int*>",width,{member("__value_",2u,0u)}));capacity_type=3u; }
        auto const layout{static_cast<unsigned>(f.types.size())};
        f.types.push_back(object("std::__1::__vector_layout<int>",width*3u,
            {member("__begin_",2u,0u),member("__end_",2u,width),member("__capacity_",capacity_type,width*2u)}));
        auto container{object("std::__1::vector<int>",width*3u,{member("__layout_",layout,0u)})};container.template_types={0u};f.types.push_back(::std::move(container));f.seal();
        put(f.bytes,0u,width,256u);put(f.bytes,width,width,268u);put(f.bytes,width*2u,width,272u);
        for(unsigned i{};i!=3u;++i) { put(f.memory,256u+i*4u,4u,10u+i); }
        check(f.format()==pretty::outcome::formatted && f.calls==1u && f.tree[0u].pretty_summary.find("size=3 capacity=4")!=::std::string::npos,
            "pinned libc++ __layout_ pointer ABI uses __capacity_");
        pretty::details::indexed_sequence plan{};
        check(pretty::details::index_plan(f.types,f.root,f.bytes,f.known,f.eligible,width,plan) && plan.count==3u && plan.element==0u && plan.address==256u,
              "new layout indexing uses identical bounded shape");
        f.known[width*2u]=::std::byte{};
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"new layout capacity endpoint requires known bits");
    }
}
int main()
{
    for(unsigned width:{4u,8u})
    { unique(width);optional_offset(width);fixed(width);libcxx_vector(width,false);libcxx_vector(width,true); }
    ::std::puts("{\"passed\":true,\"scope\":\"r3 copied DATA profiles; no full producer parity\"}");
}
