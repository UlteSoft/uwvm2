// Linked-container DWARF schema + copied-byte DATA only. No runtime read
// capability, host pointer or native container ABI is used by this fixture.
// Compile/run only in the authorized SSH Linux 64 GiB cgroup.
#include <uwvm2/uwvm/debugger/source_pretty_printer.h>
#include <cstdio>
#include <cstdlib>

namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
namespace pretty = ::uwvm2::uwvm::debugger::source_pretty_printer;
namespace
{
    void check(bool value,char const* reason)
    { if(!value) { ::std::fprintf(stderr,"pretty_printer_linked FAIL: %s\n",reason);::std::exit(1); } }
    dwarf::type_record scalar(char const* name,unsigned width,bool sign=false)
    {
        dwarf::type_record out{};out.name=name;out.kind=dwarf::type_kind::scalar;out.language=0x21u;
        out.byte_size=width;out.byte_count=static_cast<::std::uint8_t>(width);out.encoding=sign ? 5u : 7u;out.size_known=true;return out;
    }
    dwarf::member_record member(char const* name,::std::size_t type,unsigned offset,bool inherited=false)
    { dwarf::member_record out{};out.name=name;out.type=type;out.byte_offset=offset;out.offset_known=true;out.inherited=inherited;return out; }
    dwarf::type_record object(char const* name,unsigned width,::std::vector<dwarf::member_record> members={})
    {
        dwarf::type_record out{};out.name=name;out.kind=dwarf::type_kind::structure;out.language=0x21u;
        out.byte_size=width;out.size_known=true;out.members=::std::move(members);return out;
    }
    void put(::std::vector<::std::byte>& bytes,unsigned offset,unsigned width,::std::uint64_t value)
    {
        check(offset<=bytes.size() && width<=bytes.size()-offset && width<=8u,"bounded fixture encoding");
        for(unsigned i{};i!=width;++i) { bytes[offset+i]=static_cast<::std::byte>((value>>(i*8u))&255u); }
    }
    enum class family { list,tree,hash };
    struct fixture
    {
        unsigned width{},base_size{},node_size{},root{},count_offset{},head_offset{};family kind{};
        ::std::vector<dwarf::type_record> types{};::std::vector<::std::byte> bytes{},known{},qualified{},memory{};
        ::std::vector<dwarf::object_node> tree{};unsigned calls{};::std::size_t copied_bytes{};bool fail{},short_read{};
        fixture(unsigned w,family f):width(w),kind(f),memory(4096u)
        {
            types={scalar("int",4u,true),scalar("size_t",w)};
            auto ptr{object("",w)};ptr.kind=dwarf::type_kind::pointer;ptr.byte_count=w;ptr.referenced_type=3u;types.push_back(::std::move(ptr));
            base_size=f==family::tree ? 4u*w : f==family::list ? 2u*w : w;
            types.push_back(object(f==family::tree ? "_Rb_tree_node_base" : f==family::list ? "_List_node_base" : "_Hash_node_base",base_size,
                f==family::tree ? ::std::vector<dwarf::member_record>{member("_M_color",0u,0u),member("_M_parent",2u,w),member("_M_left",2u,w*2u),member("_M_right",2u,w*3u)} :
                f==family::list ? ::std::vector<dwarf::member_record>{member("_M_next",2u,0u),member("_M_prev",2u,w)} :
                ::std::vector<dwarf::member_record>{member("_M_nxt",2u,0u)}));
            types.push_back(scalar("unsigned char",1u));
            auto array{object("unsigned char[4]",4u)};array.kind=dwarf::type_kind::array;array.referenced_type=4u;array.dimensions={{0,4u,true,true}};types.push_back(::std::move(array));
            auto storage{object("__aligned_membuf<int>",4u,{member("_M_storage",5u,0u)})};storage.template_types={0u};types.push_back(::std::move(storage));
            node_size=(base_size+4u+w-1u)/w*w;
            auto node{object(f==family::tree ? "std::_Rb_tree_node<int>" : f==family::list ? "std::_List_node<int>" : "std::__detail::_Hash_node<int, false>",node_size,
                {member("base",3u,0u,true),member("_M_storage",6u,base_size)})};node.template_types={0u};types.push_back(::std::move(node));
            if(f==family::list)
            {
                types.push_back(object("_List_node_header",w*3u,{member("_M_next",2u,0u),member("_M_prev",2u,w),member("_M_size",1u,w*2u)}));
                types.push_back(object("_List_impl",w*3u,{member("_M_node",8u,0u)}));
                auto container{object("std::list<int>",w*3u,{member("_M_impl",9u,0u)})};container.template_types={0u};types.push_back(::std::move(container));
                count_offset=w*2u;head_offset=0u;
            }
            else if(f==family::tree)
            {
                types.push_back(object("_Rb_tree_impl",w*5u,{member("_M_header",3u,0u),member("_M_node_count",1u,w*4u)}));
                auto impl{object("std::_Rb_tree<int, int>",w*5u,{member("_M_impl",8u,0u)})};impl.template_types={0u,0u};types.push_back(::std::move(impl));
                types.push_back(object("std::set<int>",w*5u,{member("_M_t",9u,0u)}));count_offset=w*4u;head_offset=w;
            }
            else
            {
                auto impl{object("std::_Hashtable<int, int>",w*2u,{member("_M_before_begin",3u,0u),member("_M_element_count",1u,w)})};impl.template_types={0u,0u};types.push_back(::std::move(impl));
                types.push_back(object("std::unordered_set<int>",w*2u,{member("_M_h",8u,0u)}));count_offset=w;head_offset=0u;
            }
            root=types.size()-1u;
            for(::std::size_t i{};i!=types.size();++i) { types[i].identity={1u,i+1u};types[i].declaration_identity=types[i].identity; }
            bytes.resize(types[root].byte_size);known.assign(bytes.size(),::std::byte{255u});qualified=known;populate();
        }
        void populate()
        {
            put(bytes,count_offset,width,3u);put(bytes,head_offset,width,kind==family::tree ? 512u : 256u);
            if(kind==family::list) { put(bytes,width,width,768u); }
            for(unsigned i{};i!=3u;++i)
            {
                unsigned const at{256u+i*256u};put(memory,at+base_size,4u,(i+1u)*10u);
                if(kind==family::tree)
                { put(memory,at+width,width,i==1u ? 128u : 512u);put(memory,at+width*2u,width,i==1u ? 256u : 0u);put(memory,at+width*3u,width,i==1u ? 768u : 0u); }
                else
                { put(memory,at,width,i==2u ? (kind==family::list ? 128u : 0u) : at+256u);
                  if(kind==family::list) { put(memory,at+width,width,i==0u ? 128u : at-256u); } }
            }
        }
        pretty::outcome format(pretty::limits const& limits={})
        {
            calls=0u;copied_bytes=0u;check(dwarf::query_object_value_with_known_bits(types,root,bytes,known,tree)==dwarf::inline_query_error::none,"owned root tree");
            auto reader{[&](::std::uint64_t at,::std::size_t count,::std::vector<::std::byte>& out)
            { ++calls;copied_bytes+=count;if(fail || at>memory.size() || count>memory.size()-at) { return false; }
              out.assign(memory.begin()+static_cast<::std::size_t>(at),memory.begin()+static_cast<::std::size_t>(at)+count);
              if(short_read && !out.empty()) { out.pop_back(); }return true; }};
            return pretty::format(types,root,bytes,known,qualified,width,reader,tree,limits);
        }
        unsigned values() const
        {
            unsigned result{};for(auto const& n:tree) if(n.detached_copy && n.parent==0u)
            { check(n.value_available && n.bits==(result+1u)*10u,"linked payload order and copied scalar");++result; }return result;
        }
    };
    void hash_cache_variants(unsigned width)
    {
        fixture f{width,family::hash};
        auto boolean{scalar("bool",1u)};boolean.encoding=2u;
        auto const boolean_type{f.types.size()};f.types.push_back(boolean);
        auto traits{object("std::__detail::_Hashtable_traits<false,false,true>",1u)};
        traits.template_values={{0u,boolean_type,0u,false,true},{1u,boolean_type,0u,false,true},{2u,boolean_type,1u,false,true}};
        auto const traits_type{f.types.size()};f.types.push_back(traits);
        f.types[8u].template_types.assign(10u,0u);f.types[8u].template_types[9u]=traits_type;
        f.types[7u].template_values={{1u,boolean_type,0u,false,true}};
        auto other{f.types[7u]};other.name="std::__detail::_Hash_node<int,true>";
        other.byte_size+=width;other.template_values[0u].bits=1u;
        other.identity={1u,f.types.size()+1u};other.declaration_identity=other.identity;
        auto const cached_type{f.types.size()};f.types.push_back(other);
        check(f.format()==pretty::outcome::formatted && f.values()==3u && f.calls==3u && f.copied_bytes==3u*f.node_size,
              "table traits choose uncached nodes in same-CU dual-cache metadata");
        f.types[traits_type].template_values[0u].bits=1u;
        check(f.format()==pretty::outcome::formatted && f.values()==3u && f.calls==3u && f.copied_bytes==3u*(f.node_size+width),
              "table traits choose cached nodes in same-CU dual-cache metadata");
        f.types[cached_type].template_values[0u].available=false;
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,
              "missing actual selected cache bit cannot fall through to other node ABI");
        f.types[cached_type].template_values[0u].available=true;
        f.types[traits_type].template_values[0u].bits=2u;
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"invalid table boolean does not read nodes");
        f.types[traits_type].template_values[0u].bits=0u;
        f.types[traits_type].template_values[0u].available=false;
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"unavailable table trait does not infer cache from spelling");
        put(f.bytes,f.count_offset,width,0u);put(f.bytes,f.head_offset,width,0u);
        check(f.format()==pretty::outcome::formatted && f.calls==0u,
              "empty table needs no cache trait or node ABI");f.populate();
        f.types[traits_type].template_values[0u].available=true;
        f.types[7u].template_values.push_back(f.types[7u].template_values[0u]);
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"duplicate node cache parameter cannot read nodes");
        f.types[7u].template_values.resize(1u);
        f.types[8u].template_types.resize(2u);
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,
              "unproven legacy table still rejects distinct same-value node identities");
    }
    void lookup_budgets()
    {
        ::std::vector<dwarf::type_record> types{scalar("int",4u)};
        types.push_back(object("Ambiguous",4u,{member("_M_next",0u,0u),member("_M_next",0u,0u)}));
        types.push_back(object("Single",4u,{member("_M_next",0u,0u)}));
        types.push_back(object("Root",8u,{member("A",1u,0u,true),member("B",2u,4u,true)}));
        pretty::details::field selected{123u,456u};
        check(!pretty::details::member(types,{3u,0u},"_M_next",selected) && selected.type==123u,
              "ambiguous nested base cannot disappear behind another match");
        types[1u].members.resize(1u);types[1u].members[0u].offset_known=false;
        check(!pretty::details::path(types,3u,"_M_next",selected),"unsupported inherited candidate cannot be ignored");
        types.push_back(object("Cycle",4u,{member("self",4u,0u,true)}));
        check(!pretty::details::member(types,{4u,0u},"absent",selected),"by-value inheritance cycle rejects immediately");
        // A compact shared DAG used to multiply recursive work exponentially.
        // Its two branches contain no desired field, so the edge budget must
        // fail before trying the root's final otherwise-attractive field.
        types.push_back(object("Leaf",4u));unsigned prior{5u};
        for(unsigned level{};level!=12u;++level)
        {
            types.push_back(object("Diamond",4u,{member("left",prior,0u,true),member("right",prior,0u,true)}));prior=types.size()-1u;
        }
        types.push_back(object("BudgetRoot",8u,{member("wide",prior,0u,true),member("needle",0u,4u)}));
        check(!pretty::details::path(types,types.size()-1u,"needle",selected),"4096-edge lookup budget is shared across branches");
        auto reader{[](::std::uint64_t,::std::size_t,::std::vector<::std::byte>&) { return false; }};
        pretty::details::session<decltype(reader)> session{types,4u,reader,{}};
        check(!session.base_zero(4u,0u),"base-zero cycle is bounded");
        check(!session.base_zero(types.size()-1u,0u),"base-zero shared DAG has an edge budget");
    }
}
int main()
{
    lookup_budgets();
    for(unsigned width:{4u,8u}) { hash_cache_variants(width); }
    unsigned cases{};
    for(unsigned width:{4u,8u}) for(auto kind:{family::list,family::tree,family::hash})
    {
        fixture f{width,kind};check(f.format()==pretty::outcome::formatted && f.values()==3u && f.calls==3u && !f.tree[0u].pretty_truncated,"complete list/tree/hash copy");
        pretty::limits cap{};cap.max_elements=1u;
        check(f.format(cap)==pretty::outcome::formatted && f.values()==1u && f.tree[0u].pretty_omitted==2u && f.tree[0u].pretty_truncated,"bounded partial element expansion");
        cap={};cap.maximum_reads=1u;
        check(f.format(cap)==pretty::outcome::formatted && f.calls==1u && f.tree[0u].pretty_truncated,"read budget publishes explicitly partial copied result");
        cap.maximum_reads=0u;
        check(f.format(cap)==pretty::outcome::formatted && f.calls==0u && f.tree[0u].pretty_omitted==3u,"zero read budget issues no reads");
        f.qualified.assign(f.qualified.size(),::std::byte{});
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"unqualified linked head rejected");f.qualified=f.known;
        f.known[f.head_offset]=::std::byte{};
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"known mask intersects pointer eligibility");f.known.assign(f.known.size(),::std::byte{255u});
        f.fail=true;check(f.format()!=pretty::outcome::formatted && !f.tree[0u].pretty_available,"failed node read does not publish partial mutations");f.fail=false;
        f.short_read=true;check(f.format()!=pretty::outcome::formatted && !f.tree[0u].pretty_available,"short node copy rejected");f.short_read=false;
        f.types[7u].template_types[0u]=1u;
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"node template argument must match container value type");f.types[7u].template_types[0u]=0u;
        f.types[6u].template_types[0u]=1u;
        check(f.format()!=pretty::outcome::formatted && f.calls==0u,"aligned storage template must prove exact payload type");f.types[6u].template_types[0u]=0u;
        if(kind==family::tree) { put(f.memory,256u+width*2u,width,512u); }
        else { put(f.memory,768u,width,256u); }
        check(f.format()!=pretty::outcome::formatted && !f.tree[0u].pretty_available,"cycle at full-count boundary never claims complete container");f.populate();
        if(kind==family::list)
        {
            put(f.memory,512u+width,width,768u);
            check(f.format()!=pretty::outcome::formatted,"broken list back-link rejected");f.populate();
        }
        put(f.bytes,f.count_offset,width,0u);put(f.bytes,f.head_offset,width,kind==family::list ? 128u : 0u);
        if(kind==family::list) { put(f.bytes,width,width,128u); }
        check(f.format()==pretty::outcome::formatted && f.calls==0u,"empty container requires no node reads");
        ++cases;
    }
    ::std::printf("{\"passed\":true,\"linked_schema_cases\":%u,\"scope\":\"owned copied DATA, not full debugger parity\"}\n",cases);
}
