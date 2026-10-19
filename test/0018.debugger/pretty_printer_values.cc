// Active alternative and ownership checks use copied DATA, not a fake VM stop.
#include <uwvm2/uwvm/debugger/source_pretty_printer.h>
#include <cstdio>
#include <cstdlib>
namespace dwarf=::uwvm2::uwvm::debugger::source_dwarf;
namespace pretty=::uwvm2::uwvm::debugger::source_pretty_printer;
namespace registry=::uwvm2::uwvm::debugger::source_pretty_registry;
namespace
{
    void check(bool ok,char const* reason)
    { if(!ok) { ::std::fprintf(stderr,"pretty_printer_values FAIL: %s\n",reason);::std::exit(1); } }
    dwarf::type_record integer(char const* name,unsigned width,unsigned encoding=7u)
    { dwarf::type_record t{};t.name=name;t.language=0x21u;t.kind=dwarf::type_kind::scalar;
      t.size_known=true;t.byte_count=width;t.byte_size=width;t.encoding=encoding;return t; }
    dwarf::type_record object(char const* name,unsigned size,::std::vector<dwarf::member_record> fields={})
    { dwarf::type_record t{};t.name=name;t.language=0x21u;t.kind=dwarf::type_kind::structure;
      t.size_known=true;t.byte_size=size;t.members=::std::move(fields);return t; }
    dwarf::member_record field(char const* name,::std::size_t type,unsigned offset)
    { dwarf::member_record f{};f.name=name;f.type=type;f.byte_offset=offset;f.offset_known=true;return f; }
    dwarf::type_record pointer(::std::size_t target,unsigned width)
    { auto t=object("pointer",width);t.kind=dwarf::type_kind::pointer;t.byte_count=width;t.referenced_type=target;return t; }
    void put(::std::vector<::std::byte>& bytes,unsigned at,unsigned width,::std::uint64_t value)
    { check(at<=bytes.size() && width<=bytes.size()-at,"fixture range");for(unsigned i{};i!=width;++i) {bytes[at+i]=static_cast<::std::byte>((value>>(i*8u))&255u);} }
    struct fixture
    {
        unsigned width{},root{},reads{};bool fail{};
        ::std::vector<dwarf::type_record> types{};
        ::std::vector<::std::byte> bytes{},known{},eligible{},memory{};
        ::std::vector<dwarf::object_node> tree{};
        explicit fixture(unsigned w):width(w),memory(4096u)
        { types={integer("int",4u,5u),integer("bool",1u,2u),integer("size_t",w)}; }
        void seal() {root=types.size()-1u;bytes.assign(types[root].byte_size,::std::byte{});known.assign(bytes.size(),::std::byte{255u});eligible=known;}
        pretty::outcome run(pretty::limits const& cap={})
        {
            reads=0u;check(dwarf::query_object_value_with_known_bits(types,root,bytes,known,tree)==dwarf::inline_query_error::none,"owned raw value");
            auto read{[&](::std::uint64_t at,::std::size_t count,::std::vector<::std::byte>& out)
            { ++reads;if(fail || at>memory.size() || count>memory.size()-at) {return false;}
              out.assign(memory.begin()+static_cast<::std::size_t>(at),memory.begin()+static_cast<::std::size_t>(at)+count);return true; }};
            return pretty::format(types,root,bytes,known,eligible,width,read,tree,cap);
        }
        dwarf::object_node const* copied() const
        {for(auto const& n:tree) {if(n.detached_copy && n.parent==0u) {return &n;}}return nullptr;}
    };
    void variant(unsigned width,bool libcxx)
    {
        fixture f{width};
        auto integer_head=object("alt<int>",4u,{field(libcxx ? "__value" : "_M_storage",0u,0u)});integer_head.template_types={0u};f.types.push_back(integer_head);
        auto bool_head=object("alt<bool>",1u,{field(libcxx ? "__value" : "_M_storage",1u,0u)});bool_head.template_types={1u};f.types.push_back(bool_head);
        auto tail=object("union<bool>",1u,{field(libcxx ? "__head" : "_M_first",4u,0u)});tail.kind=dwarf::type_kind::union_type;f.types.push_back(tail);
        auto head=object("union<int,bool>",4u,{field(libcxx ? "__head" : "_M_first",3u,0u),field(libcxx ? "__tail" : "_M_rest",5u,0u)});head.kind=dwarf::type_kind::union_type;f.types.push_back(head);
        f.types.push_back(integer("variant index",1u,8u));
        if(libcxx)
        {f.types.push_back(object("variant_impl",8u,{field("__data",6u,0u),field("__index",7u,4u)}));f.types.push_back(object("std::__1::variant<int,bool>",8u,{field("__impl_",8u,0u)}));}
        else {f.types.push_back(object("std::variant<int,bool>",8u,{field("_M_u",6u,0u),field("_M_index",7u,4u)}));}
        // The index is unsigned metadata, independent from bool's semantics.
        f.types.back().template_types={0u,1u};f.seal();
        put(f.bytes,0u,4u,42u);
        check(f.run()==pretty::outcome::formatted && f.copied() && f.copied()->bits==42u && f.reads==0u,"variant active first payload copied locally");
        put(f.bytes,0u,4u,1u);put(f.bytes,4u,1u,1u);
        check(f.run()==pretty::outcome::formatted && f.copied() && f.copied()->type==1u && f.copied()->bits==1u,"variant active second payload type follows index");
        f.known[0u]=::std::byte{};
        check(f.run()==pretty::outcome::formatted && f.copied() && !f.copied()->value_available,"unknown active payload stays unavailable");f.known=f.eligible;
        put(f.bytes,4u,1u,255u);
        check(f.run()==pretty::outcome::formatted && !f.copied() && f.tree[0u].pretty_summary=="valueless_by_exception","variant valueless does not format stale payload");
        put(f.bytes,4u,1u,2u);
        check(f.run()!=pretty::outcome::formatted && !f.tree[0u].pretty_available && f.reads==0u,"invalid variant index atomically keeps raw tree");
    }
    void expected(unsigned width)
    {
        fixture f{width};f.types.push_back(integer("error",2u));
        auto t=object("std::expected<int,error>",8u,{field("_M_val",0u,0u),field("_M_unex",3u,0u),field("_M_has_value",1u,4u)});t.template_types={0u,3u};f.types.push_back(t);f.seal();
        put(f.bytes,0u,4u,17u);put(f.bytes,4u,1u,1u);
        check(f.run()==pretty::outcome::formatted && f.copied() && f.copied()->bits==17u,"expected value selects T");
        put(f.bytes,0u,4u,9u);put(f.bytes,4u,1u,0u);
        check(f.run()==pretty::outcome::formatted && f.copied() && f.copied()->type==3u && f.copied()->bits==9u,"expected error selects E");
        f.known[4u]=::std::byte{};
        check(f.run()!=pretty::outcome::formatted && !f.copied(),"expected unavailable discriminant cannot select union");
    }
    void expected_void(unsigned width,bool libcxx)
    {
        fixture f{width};
        if(libcxx)
        {
            auto storage=object("expected void storage",4u,{field("__unex_",0u,0u)});
            storage.kind=dwarf::type_kind::union_type;f.types.push_back(storage);
            f.types.push_back(object("std::__1::expected<void,int>",8u,{field("__union_",3u,0u),field("__has_val_",1u,4u)}));
        }
        else {f.types.push_back(object("std::expected<void,int>",8u,{field("_M_unex",0u,0u),field("_M_has_value",1u,4u)}));}
        // This is the index's actual absent-DW_AT_type convention, without
        // manufacturing an explicit type_record named void for the template T.
        f.types.back().template_types={dwarf::no_record,0u};f.seal();
        put(f.bytes,0u,4u,29u);put(f.bytes,4u,1u,1u);
        check(f.run()==pretty::outcome::formatted && !f.copied() && f.reads==0u &&
              f.tree[0u].pretty_summary=="value (void)","no_record void value has no copied payload or guest read");
        pretty::limits cap{};cap.maximum_reads=0u;
        check(f.run(cap)==pretty::outcome::formatted && !f.copied() && f.reads==0u &&
              f.tree[0u].pretty_summary=="value (void)","active void remains available with zero guest-read allowance");
        put(f.bytes,4u,1u,0u);
        check(f.run(cap)==pretty::outcome::formatted && f.copied() && f.copied()->type==0u &&
              f.copied()->bits==29u && f.reads==0u && f.tree[0u].pretty_summary=="error = 29",
              "no_record void success type does not suppress the actual typed error payload");
        f.known[4u]=::std::byte{};
        check(f.run()!=pretty::outcome::formatted && !f.copied() && !f.tree[0u].pretty_available && f.reads==0u,
              "unknown expected void discriminant retains only raw data");
        f.known=f.eligible;put(f.bytes,4u,1u,2u);
        check(f.run()!=pretty::outcome::formatted && !f.copied() && !f.tree[0u].pretty_available && f.reads==0u,
              "invalid expected void discriminant cannot create an active void value");
        auto const void_type=f.types.size();dwarf::type_record explicit_void{};explicit_void.name="void";
        f.types.push_back(explicit_void);f.types[f.root].template_types[0u]=void_type;put(f.bytes,4u,1u,1u);
        check(f.run()==pretty::outcome::formatted && !f.copied() && f.reads==0u && f.tree[0u].pretty_summary=="value (void)",
              "explicit unavailable void type metadata remains compatible");
        for(bool has_value : {false,true})
        {
            f.types[f.root].template_types={dwarf::no_record,0u};put(f.bytes,4u,1u,has_value ? 1u : 0u);
            check(dwarf::query_object_value_with_known_bits(f.types,f.root,f.bytes,f.known,f.tree)==dwarf::inline_query_error::none,
                  "capture original raw tree before rejecting malformed template metadata");
            auto const raw_nodes=f.tree.size();auto const raw_storage=f.tree.data();
            f.types[f.root].template_types[has_value ? 0u : 1u]=f.types.size();f.reads=0u;
            auto reject_read{[&](::std::uint64_t,::std::size_t,::std::vector<::std::byte>&) {++f.reads;return false;}};
            check(pretty::format(f.types,f.root,f.bytes,f.known,f.eligible,width,reject_read,f.tree)!=pretty::outcome::formatted &&
                  f.reads==0u && !f.copied() && !f.tree[0u].pretty_available && f.tree.size()==raw_nodes && f.tree.data()==raw_storage,
                  "ordinary out-of-range selected template index preserves the original raw tree without reads");
        }
    }
    void shared(unsigned width,bool libcxx)
    {
        fixture f{width};f.types.push_back(pointer(0u,width));
        auto count=integer("signed count",libcxx ? width : 4u,5u);f.types.push_back(count);
        f.types.push_back(object("control block",count.byte_count*2u,{field(libcxx ? "__shared_owners_" : "_M_use_count",4u,0u),field(libcxx ? "__shared_weak_owners_" : "_M_weak_count",4u,count.byte_count)}));
        f.types.push_back(pointer(5u,width));
        if(libcxx) {f.types.push_back(object("std::__1::shared_ptr<int>",width*2u,{field("__ptr_",3u,0u),field("__cntrl_",6u,width)}));}
        else {f.types.push_back(object("refcount",width,{field("_M_pi",6u,0u)}));f.types.push_back(object("std::shared_ptr<int>",width*2u,{field("_M_ptr",3u,0u),field("_M_refcount",7u,width)}));}
        f.types.back().template_types={0u};f.seal();put(f.bytes,0u,width,256u);put(f.bytes,width,width,512u);put(f.memory,256u,4u,23u);
        put(f.memory,512u,count.byte_count,libcxx ? 0u : 1u);put(f.memory,512u+count.byte_count,count.byte_count,libcxx ? 0u : 1u);
        check(f.run()==pretty::outcome::formatted && f.reads==2u && f.copied() && f.copied()->bits==23u && f.tree[0u].pretty_summary.find("use_count=1")!=::std::string::npos,"shared counts and live pointee independently copied");
        f.types[f.root].name=libcxx ? "std::__1::weak_ptr<int>" : "std::weak_ptr<int>";
        check(f.run()==pretty::outcome::formatted && f.reads==1u && !f.copied(),"weak printer never copies pointee");
        put(f.memory,512u,count.byte_count,libcxx ? dwarf::numeric_mask(count.byte_count) : 0u);
        check(f.run()==pretty::outcome::formatted && f.reads==1u && f.tree[0u].pretty_summary.find("expired")!=::std::string::npos,"weak expired encoding is ABI-specific");
        f.eligible[width]=::std::byte{};
        check(f.run()!=pretty::outcome::formatted && f.reads==0u,"control pointer requires eligible carrier");
        f.eligible=f.known;
        auto const alias=f.types.size();dwarf::type_record v{};v.name="element_type";v.language=0x21u;v.named_type_alias=true;v.void_type=true;
        f.types.push_back(v);f.types[3u].referenced_type=alias;
        f.types[f.root].name=libcxx ? "std::__1::shared_ptr<void>" : "std::shared_ptr<void>";
        f.types[f.root].template_types={dwarf::no_record};
        put(f.memory,512u,count.byte_count,libcxx ? 1u : 2u);
        put(f.memory,512u+count.byte_count,count.byte_count,libcxx ? 1u : 2u);
        check(f.run()==pretty::outcome::formatted && f.reads==1u && !f.copied() &&
              f.tree[0u].pretty_summary.find("stored pointer to void")!=::std::string::npos,
              "owned void alias permits only control counts, never pointee reads");
        f.types[alias].void_type=false;
        check(f.run()!=pretty::outcome::formatted && f.reads==0u && !f.copied(),
              "arbitrary unavailable alias is never inferred as void");
        f.types[alias].void_type=true;f.types[alias].size_known=true;
        check(f.run()!=pretty::outcome::formatted && f.reads==0u,"inconsistent void metadata is rejected before reads");
    }
    void family_names()
    {
        for(auto stem:{"vector","map","deque","duration","span","bitset","shared_ptr"})
        {
            auto outer=object(("std::wrapper<std::__1::"+::std::string{stem}+"<int>>").c_str(),8u);
            check(!pretty::details::family(outer,stem),"nested template type cannot select root ABI");
            check(pretty::details::family(outer,"wrapper"),"outer unqualified identifier remains selectable");
            outer.aggregate_name="std::__1::"+::std::string{stem}+"<std::vector<int>>";outer.name="UserAlias";
            check(pretty::details::family(outer,stem),"owned aggregate identity survives display alias");
        }
        auto prefix=object("std::vectorish<std::vector<int>>",8u);
        check(!pretty::details::family(prefix,"vector"),"root prefix and nested name never select vector");
        prefix.name="std::vector<std::deque<int>>::iterator";
        check(!pretty::details::family(prefix,"vector") && !pretty::details::family(prefix,"deque") &&
              pretty::details::family(prefix,"iterator"),"qualified nested type is distinct from its template owner");
        for(auto bad:{"std::vector<int","std::vector<int>>","wrapper<std::vector<int>"})
        { prefix.name=bad;check(!pretty::details::family(prefix,"vector"),"unbalanced type metadata selects no library family"); }
    }
    void adapters(unsigned width)
    {
        for(bool libcxx : {false,true})
        {
            fixture f{width};auto t=object(libcxx ? "std::__1::chrono::duration<int>" : "std::chrono::duration<int>",4u,
                {field(libcxx ? "__rep_" : "__r",0u,0u)});t.template_types={0u};f.types.push_back(t);f.seal();put(f.bytes,0u,4u,23u);
            check(f.run()==pretty::outcome::formatted && f.copied() && f.copied()->bits==23u && f.reads==0u &&
                  f.tree[0u].pretty_summary.find("duration ticks = 23")!=::std::string::npos,"duration keeps real copied rep value");
            f.types[f.root].aggregate_name=f.types[f.root].name;f.types[f.root].name="application::Milliseconds";f.types[f.root].named_type_alias=true;
            check(f.run()==pretty::outcome::formatted && f.tree[0u].type_name=="application::Milliseconds" &&
                  f.tree[0u].pretty_summary.find("duration ticks = 23")!=::std::string::npos,"exact underlying DIE preserves both alias display and built-in profile");
            auto const duration{f.root};auto point=object(libcxx ? "std::__1::chrono::time_point<std::__1::chrono::steady_clock,std::__1::chrono::duration<int>>" : "std::chrono::time_point<std::chrono::steady_clock,std::chrono::duration<int>>",4u,
                {field(libcxx ? "__d_" : "__d",duration,0u)});point.template_types={0u,duration};f.types.push_back(point);f.seal();put(f.bytes,0u,4u,31u);
            check(f.run()==pretty::outcome::formatted && f.reads==0u && f.tree[0u].pretty_summary.find("duration ticks = 31")!=::std::string::npos,
                  "time point recursively selects its exact declared duration");
        }
        for(auto name : {"std::stack<int,std::deque<int>>","std::queue<int,std::deque<int>>","std::priority_queue<int,std::deque<int>>"})
        {
            fixture f{width};f.types.push_back(object("container",4u,{field("first",0u,0u)}));
            auto t=object(name,4u,{field("c",3u,0u)});t.template_types={0u,3u};f.types.push_back(t);f.seal();put(f.bytes,0u,4u,17u);
            check(f.run()==pretty::outcome::formatted && f.copied() && f.copied()->type==3u && f.reads==0u,"container adaptor selects proven backing type");
            f.types[f.root].template_types[1u]=0u;
            check(f.run()!=pretty::outcome::formatted && !f.copied() && f.reads==0u,"adaptor name cannot replace exact template proof");
        }
    }
    void custom(unsigned width)
    {
        fixture f{width};f.types.push_back(pointer(0u,width));f.types.push_back(object("application::Sequence<int>",width*3u,{field("data",3u,0u),field("count",2u,width),field("capacity",2u,width*2u)}));f.seal();
        put(f.bytes,0u,width,256u);put(f.bytes,width,width,2u);put(f.bytes,width*2u,width,2u);put(f.memory,256u,4u,6u);put(f.memory,260u,4u,7u);
        registry::registry registered{};registry::profile p{};p.name="application";p.type_pattern="application::Sequence<*>";p.data_path="data";p.size_path="count";p.capacity_path="capacity";registered.profiles.push_back(p);
        pretty::limits cap{};cap.registry=&registered;
        check(f.run(cap)==pretty::outcome::formatted && f.reads==1u && f.tree[0u].pretty_printer=="application","registered typed sequence uses canonical callback");
        registered.profiles[0u].enabled=false;
        check(f.run(cap)==pretty::outcome::not_matched && f.reads==0u,"disabled profile does not initiate a copy");
        registered.profiles[0u].enabled=true;f.known[0u]=::std::byte{};
        check(f.run(cap)!=pretty::outcome::formatted && f.reads==0u,"custom profile name cannot authorize unknown pointer bits");
    }
}
int main()
{
    family_names();
    for(unsigned width:{4u,8u}) {variant(width,false);variant(width,true);expected(width);expected_void(width,false);expected_void(width,true);shared(width,false);shared(width,true);adapters(width);custom(width);}
    ::std::puts("{\"passed\":true,\"scope\":\"active variant/expected, ownership and custom profile copied DATA\"}");
}
