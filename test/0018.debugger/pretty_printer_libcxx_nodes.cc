// libc++ list/forward/tree/hash schemas copied from the bundled upstream
// headers. This is owned DATA qualification, separate from compiler fixtures.
#include <uwvm2/uwvm/debugger/source_pretty_libcxx_nodes.h>
#include <cstdio>
#include <cstdlib>
namespace dwarf=::uwvm2::uwvm::debugger::source_dwarf;
namespace pretty=::uwvm2::uwvm::debugger::source_pretty_printer;
namespace
{
    void check(bool value,char const* why)
    { if(!value) { ::std::fprintf(stderr,"pretty_printer_libcxx_nodes FAIL: %s\n",why);::std::exit(1); } }
    dwarf::type_record object(char const* name,unsigned size,::std::vector<dwarf::member_record> members={})
    { dwarf::type_record t{};t.name=name;t.kind=dwarf::type_kind::structure;t.language=0x21u;t.byte_size=size;t.size_known=true;t.members=::std::move(members);return t; }
    dwarf::type_record scalar(char const* name,unsigned width)
    { auto t{object(name,width)};t.kind=dwarf::type_kind::scalar;t.byte_count=width;t.encoding=7u;return t; }
    dwarf::member_record field(char const* name,unsigned type,unsigned offset,bool inherited=false)
    { dwarf::member_record f{};f.name=name;f.type=type;f.byte_offset=offset;f.offset_known=true;f.inherited=inherited;return f; }
    void put(::std::vector<::std::byte>& bytes,unsigned offset,unsigned width,::std::uint64_t value)
    { check(offset<=bytes.size() && width<=bytes.size()-offset && width<=8u,"bounded fixture write");
      for(unsigned i{};i!=width;++i) { bytes[offset+i]=static_cast<::std::byte>((value>>(i*8u))&255u); } }
    enum class kind { list,forward,tree,hash };
    struct fixture
    {
        unsigned width{},root{},payload{},calls{},count_offset{},head_offset{};kind family{};
        ::std::vector<dwarf::type_record> types{};::std::vector<::std::byte> bytes{},known{},eligible{},memory{};
        ::std::vector<dwarf::object_node> tree{};
        fixture(unsigned w,kind k):width(w),family(k),memory(4096u)
        {
            types={scalar("int",4u),scalar("size_t",w)};auto p{object("",w)};p.kind=dwarf::type_kind::pointer;p.byte_count=w;p.referenced_type=k==kind::forward ? 5u : 3u;types.push_back(::std::move(p));
            auto const base_size{k==kind::list ? w*2u : k==kind::tree ? w*4u : w};
            types.push_back(object("__node_base",base_size,k==kind::list ?
                ::std::vector<dwarf::member_record>{field("__prev_",2u,0u),field("__next_",2u,w)} : k==kind::tree ?
                ::std::vector<dwarf::member_record>{field("__left_",2u,0u),field("__right_",2u,w),field("__parent_",2u,w*2u)} :
                ::std::vector<dwarf::member_record>{field("__next_",2u,0u)}));
            auto storage{object("",4u,{field("__value_",0u,0u)})};storage.kind=dwarf::type_kind::union_type;types.push_back(::std::move(storage));
            payload=k==kind::hash ? base_size+w : base_size;
            auto node{object(k==kind::list ? "std::__1::__list_node<int,void*>" : k==kind::forward ? "std::__1::__forward_list_node<int,void*>" :
                k==kind::tree ? "std::__1::__tree_node<int,void*>" : "std::__1::__hash_node<int,void*>",(payload+4u+w-1u)/w*w,
                {field("base",3u,0u,true),field("",4u,payload)})};node.template_types={0u};types.push_back(::std::move(node));
            if(k==kind::list)
            {
                types.push_back(object("__list_imp",w*3u,{field("__end_",3u,0u),field("__size_",1u,w*2u)}));
                auto t{object("std::__1::list<int>",w*3u,{field("base",6u,0u,true)})};t.template_types={0u};types.push_back(::std::move(t));head_offset=w;count_offset=w*2u;
            }
            else if(k==kind::forward)
            {
                types.push_back(object("__forward_list_base",w,{field("__before_begin_",3u,0u)}));
                auto t{object("std::__1::forward_list<int>",w,{field("base",6u,0u,true)})};t.template_types={0u};types.push_back(::std::move(t));
            }
            else if(k==kind::tree)
            {
                types.push_back(object("__tree_end_node",w,{field("__left_",2u,0u)}));
                auto impl{object("__tree<int>",w*3u,{field("__begin_node_",2u,0u),field("__end_node_",6u,w),field("__size_",1u,w*2u)})};impl.template_types={0u};types.push_back(::std::move(impl));
                types.push_back(object("std::__1::set<int>",w*3u,{field("__tree_",7u,0u)}));head_offset=w;count_offset=w*2u;
            }
            else
            {
                auto impl{object("__hash_table<int>",w*2u,{field("__first_node_",3u,0u),field("__size_",1u,w)})};impl.template_types={0u};types.push_back(::std::move(impl));
                types.push_back(object("std::__1::unordered_set<int>",w*2u,{field("__table_",6u,0u)}));count_offset=w;
            }
            root=types.size()-1u;for(::std::size_t i{};i!=types.size();++i) { types[i].identity={1u,i+1u};types[i].declaration_identity=types[i].identity; }
            bytes.resize(types[root].byte_size);known.assign(bytes.size(),::std::byte{255u});eligible=known;
            put(bytes,head_offset,w,k==kind::tree ? 512u : 256u);if(k!=kind::forward) { put(bytes,count_offset,w,3u); }
            if(k==kind::list) { put(bytes,0u,w,768u); }
            for(unsigned i{};i!=3u;++i)
            {
                auto const address{256u+i*256u};put(memory,address+payload,4u,(i+1u)*10u);
                if(k==kind::tree)
                { put(memory,address,w,i==1u ? 256u : 0u);put(memory,address+w,w,i==1u ? 768u : 0u); }
                else if(k==kind::list)
                { put(memory,address,w,i==0u ? 128u : address-256u);put(memory,address+w,w,i==2u ? 128u : address+256u); }
                else { put(memory,address,w,i==2u ? 0u : address+256u); }
            }
        }
        pretty::outcome format(pretty::limits const& cap={})
        {
            calls=0u;check(dwarf::query_object_value_with_known_bits(types,root,bytes,known,tree)==dwarf::inline_query_error::none,"raw metadata query");
            auto read{[&](::std::uint64_t at,::std::size_t size,::std::vector<::std::byte>& out)
            { ++calls;if(at>memory.size() || size>memory.size()-at) { return false; }out.assign(memory.begin()+at,memory.begin()+at+size);return true; }};
            return pretty::format(types,root,bytes,known,eligible,width,read,tree,cap);
        }
        unsigned values() const
        { unsigned n{};for(auto const& value:tree) if(value.detached_copy && value.parent==0u)
          { check(value.value_available && value.bits==(n+1u)*10u,"copied libc++ value order");++n; }return n; }
        unsigned add(dwarf::type_record type) { types.push_back(::std::move(type));return types.size()-1u; }
        unsigned compressed(unsigned first)
        {
            auto const extent{static_cast<unsigned>(types[first].byte_size)};
            auto index_type{scalar("int index",4u)};index_type.encoding=5u;auto const index{add(::std::move(index_type))};
            auto const state{add(object("StatefulComparatorOrAllocator",4u,{field("state",0u,0u)}))};
            auto carrier{object("std::__1::__compressed_pair_elem<T,0,false>",extent,{field("__value_",first,0u)})};
            carrier.template_types={first};carrier.template_values={{1u,index,0u,true,true}};auto const head{add(::std::move(carrier))};
            carrier=object("std::__1::__compressed_pair_elem<A,1,false>",4u,{field("__value_",state,0u)});
            carrier.template_types={state};carrier.template_values={{1u,index,1u,true,true}};auto const tail{add(::std::move(carrier))};
            auto pair{object("std::__1::__compressed_pair<T,A>",(extent+4u+width-1u)/width*width,
                {field("",head,0u,true),field("",tail,extent,true)})};pair.template_types={first,state};return add(::std::move(pair));
        }
        void old_pairs()
        {
            unsigned extent{};
            if(family==kind::list)
            {
                auto const size{compressed(1u)};extent=width*2u+types[size].byte_size;
                types[6u].members[1u]=field("__size_alloc_",size,width*2u);types[6u].byte_size=extent;
            }
            else if(family==kind::forward)
            {
                auto const before{compressed(3u)};extent=types[before].byte_size;
                types[6u].members[0u].type=before;types[6u].byte_size=extent;
            }
            else if(family==kind::tree)
            {
                auto const end{compressed(6u)};auto const size{compressed(1u)};count_offset=width+types[end].byte_size;
                extent=count_offset+types[size].byte_size;types[7u].members[1u]=field("__pair1_",end,width);
                types[7u].members[2u]=field("__pair3_",size,count_offset);types[7u].byte_size=extent;
            }
            else
            {
                auto const first{compressed(3u)};auto const size{compressed(1u)};count_offset=types[first].byte_size;
                extent=count_offset+types[size].byte_size;types[6u].members[0u]=field("__p1_",first,0u);
                types[6u].members[1u]=field("__p2_",size,count_offset);types[6u].byte_size=extent;
            }
            types[root].byte_size=extent;bytes.assign(extent,::std::byte{});known.assign(extent,::std::byte{255u});eligible=known;
            put(bytes,head_offset,width,family==kind::tree ? 512u : 256u);if(family!=kind::forward) { put(bytes,count_offset,width,3u); }
            if(family==kind::list) { put(bytes,0u,width,768u); }
            for(::std::size_t i{};i!=types.size();++i) { types[i].identity={1u,i+1u};types[i].declaration_identity=types[i].identity; }
        }
        void map_payload(bool old=false)
        {
            check(family==kind::tree || family==kind::hash,"map-shaped fixture family");
            auto pair{object("std::__1::pair<const int,int>",8u,{field("first",0u,0u),field("second",0u,4u)})};pair.template_types={0u,0u};
            auto const pair_index{static_cast<unsigned>(types.size())};types.push_back(::std::move(pair));
            auto nominal{object(family==kind::tree ? "std::__1::__value_type<int,int>" : "std::__1::__hash_value_type<int,int>",old ? width+8u : 0u)};
            nominal.size_known=old;nominal.template_types={0u,0u};if(old) { nominal.members={field("__cc_",pair_index,width)}; }
            auto const nominal_index{static_cast<unsigned>(types.size())};types.push_back(::std::move(nominal));
            types[4u].byte_size=old ? width+8u : 8u;types[4u].members[0u].type=old ? nominal_index : pair_index;
            types[5u].byte_size=(payload+types[4u].byte_size+width-1u)/width*width;types[5u].template_types={nominal_index};
            types[family==kind::tree ? 7u : 6u].template_types={nominal_index};
            types[root].name=family==kind::tree ? "std::__1::map<int,int>" : "std::__1::unordered_map<int,int>";types[root].template_types={0u,0u};
            for(::std::size_t i{};i!=types.size();++i) { types[i].identity={1u,i+1u};types[i].declaration_identity=types[i].identity; }
            for(unsigned i{};i!=3u;++i)
            { put(memory,256u+i*256u+payload+(old ? width : 0u),4u,(i+1u)*10u);
              put(memory,256u+i*256u+payload+(old ? width : 0u)+4u,4u,(i+1u)*100u); }
        }
    };
}
int main()
{
    for(unsigned width:{4u,8u}) for(auto k:{kind::list,kind::forward,kind::tree,kind::hash})
    {
        fixture f{width,k};check(f.format()==pretty::outcome::formatted && f.values()==3u,"complete libc++ container");
        pretty::limits cap{};cap.max_elements=1u;check(f.format(cap)==pretty::outcome::formatted && f.values()==1u && f.tree[0u].pretty_truncated,"partial libc++ container");
        f.eligible.assign(f.eligible.size(),::std::byte{});check(f.format()!=pretty::outcome::formatted && f.calls==0u,"libc++ pointer carrier mask");f.eligible=f.known;
        if(k==kind::tree) { put(f.memory,256u,width,512u); }
        else { put(f.memory,768u+(k==kind::list ? width : 0u),width,256u); }
        check(f.format()!=pretty::outcome::formatted && !f.tree[0u].pretty_available,"libc++ cycle is rejected atomically");
        fixture stateful{width,k};stateful.old_pairs();
        check(stateful.format()==pretty::outcome::formatted && stateful.values()==3u,"LLVM18 typed compressed first supports stateful allocator/comparator");
        if(k==kind::tree || k==kind::hash)
        {
            for(bool old:{false,true})
            {
                fixture map{width,k};map.map_payload(old);if(old) { map.old_pairs(); }
                check(map.format()==pretty::outcome::formatted,"libc++ old/modern nominal wrapper binds real pair K/V");
                unsigned values{};for(auto const& node:map.tree) if(node.detached_copy && node.parent==0u)
                { check(node.kind==dwarf::type_kind::structure && map.types[node.type].name=="std::__1::pair<const int,int>","map synthetic value is the actual pair");++values; }
                check(values==3u && map.tree[0u].pretty_summary.find("100")!=::std::string::npos &&
                    map.tree[0u].pretty_summary.find("__value_type")==::std::string::npos,"pair payload values survive nonzero wrapper offset");
                // Real LLVM18 nodes store __node_value_type as a distinct
                // typedef record of the owning tree's __value_type. Newer
                // nodes can similarly alias the stored pair itself.
                auto const storage=map.types[4u].members[0u].type;
                auto alias=map.types[storage];alias.aggregate_name=alias.name;alias.name="NodePayloadAlias";alias.named_type_alias=true;
                alias.identity.offset+=10000u;auto const alias_index=map.add(alias);
                map.types[4u].members[0u].type=alias_index;
                auto const nominal=map.types[5u].template_types[0u];
                auto node_element=map.types[nominal];node_element.aggregate_name=node_element.name;
                node_element.name="NodeElementAlias";node_element.named_type_alias=true;node_element.identity.offset+=20000u;
                auto const node_alias=map.add(node_element);map.types[5u].template_types[0u]=node_alias;
                check(map.format()==pretty::outcome::formatted && map.tree[0u].pretty_summary.find("100")!=::std::string::npos,
                      "node and payload aliases bind only their exact owned final declarations");
                if(old)
                {
                    map.types[alias_index].declaration_identity.unit=2u;
                    check(map.format()!=pretty::outcome::formatted && map.calls==0u,
                          "a complete nominal wrapper requires its own payload declaration before reads");
                }
                else
                {
                    // An incomplete modern nominal wrapper does not specify a
                    // pair DIE: its real node field supplies the concrete pair.
                    // Check that field's actual extent rather than demanding an
                    // unrelated nominal-to-pair declaration identity.
                    map.types[alias_index].byte_size+=width;
                    check(map.format()!=pretty::outcome::formatted && map.calls==0u,
                          "modern concrete pair cannot exceed the owned node storage");
                }
                map.types[alias_index]=alias;map.types[alias_index].atomic_scalar=true;
                check(map.format()!=pretty::outcome::formatted && map.calls==0u,
                      "an atomic alias cannot borrow an ordinary payload layout");
                map.types[alias_index]=alias;map.types[node_alias].declaration_identity.offset+=1000u;
                check(map.format()!=pretty::outcome::formatted && map.calls==0u,
                      "node template aliases require the same final declaration");
                map.types[node_alias]=node_element;
                map.types[map.root].template_types[0u]=1u;
                check(map.format()!=pretty::outcome::formatted && map.calls==0u,"map K mismatch rejects nominal-to-pair binding before reads");
            }
        }
    }
    ::std::puts("{\"passed\":true,\"scope\":\"owned libc++ node schemas, separate from producer qualification\"}");
}
