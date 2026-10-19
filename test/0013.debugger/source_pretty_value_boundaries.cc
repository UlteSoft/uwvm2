// Owned metadata/copied-DATA boundary checks; this is not a VM stop or product benchmark.
#include <uwvm2/uwvm/debugger/source_pretty_printer.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
namespace pretty = ::uwvm2::uwvm::debugger::source_pretty_printer;
namespace registry = ::uwvm2::uwvm::debugger::source_pretty_registry;
namespace
{
    void check(bool good,char const* reason)
    { if(!good) { ::std::fprintf(stderr,"source pretty value boundaries: %s\n",reason); ::std::exit(1); } }
    dwarf::type_record scalar(::std::string name,unsigned width,unsigned encoding=7u)
    { dwarf::type_record t{}; t.name=::std::move(name);t.language=0x21u;t.kind=dwarf::type_kind::scalar;
      t.size_known=true;t.byte_size=width;t.byte_count=width;t.encoding=encoding;return t; }
    dwarf::member_record member(char const* name,::std::size_t type,unsigned offset)
    { dwarf::member_record m{};m.name=name;m.type=type;m.offset_known=true;m.byte_offset=offset;return m; }
    dwarf::type_record object(char const* name,unsigned bytes,::std::vector<dwarf::member_record> members={})
    { dwarf::type_record t{};t.name=name;t.language=0x21u;t.kind=dwarf::type_kind::structure;
      t.size_known=true;t.byte_size=bytes;t.members=::std::move(members);return t; }
    dwarf::type_record pointer(::std::size_t target,unsigned width)
    { auto t{object("pointer",width)};t.kind=dwarf::type_kind::pointer;t.byte_count=width;t.referenced_type=target;return t; }
    void put(::std::vector<::std::byte>& bytes,unsigned at,unsigned count,::std::uint64_t bits)
    { check(at<=bytes.size() && count<=bytes.size()-at,"fixture storage extent");
      for(unsigned i{};i!=count;++i) {bytes[at+i]=static_cast<::std::byte>((bits>>(i*8u))&255u);} }
    struct fixture
    {
        unsigned width{},root{},reads{};bool fail_read{};
        ::std::vector<dwarf::type_record> types{};
        ::std::vector<::std::byte> bytes{},known{},eligible{},memory{1024u};
        ::std::vector<dwarf::object_node> tree{};
        explicit fixture(unsigned w):width(w)
        {types={scalar("int",4u,5u),scalar("bool",1u,2u),scalar("size_t",w)};}
        void seal()
        {root=static_cast<unsigned>(types.size()-1u);bytes.assign(types[root].byte_size,::std::byte{});
         known.assign(bytes.size(),::std::byte{255u});eligible=known;}
        pretty::outcome run(pretty::limits cap={})
        {
            reads=0u;check(dwarf::query_object_value_with_known_bits(types,root,bytes,known,tree)==dwarf::inline_query_error::none,"owned raw object is valid");
            auto read{[&](::std::uint64_t address,::std::size_t count,::std::vector<::std::byte>& output)
            {++reads;if(fail_read || address>memory.size() || count>memory.size()-address) {return false;}
             output.assign(memory.begin()+static_cast<::std::size_t>(address),memory.begin()+static_cast<::std::size_t>(address)+count);return true;}};
            return pretty::format(types,root,bytes,known,eligible,static_cast<::std::uint8_t>(width),read,tree,cap);
        }
        bool has_copied_int(::std::uint64_t value) const
        {for(auto const& n:tree) {if(n.detached_copy && n.type==0u && n.value_available && n.bits==value) {return true;}}return false;}
    };
    void weak_counts(unsigned width,bool libcxx)
    {
        fixture f{width};f.types.push_back(pointer(0u,width));
        auto const count_width{libcxx?width:4u};f.types.push_back(scalar("signed ownership count",count_width,5u));
        f.types.push_back(object("control block",count_width*2u,{member(libcxx?"__shared_owners_":"_M_use_count",4u,0u),member(libcxx?"__shared_weak_owners_":"_M_weak_count",4u,count_width)}));
        f.types.push_back(pointer(5u,width));
        if(libcxx) {f.types.push_back(object("std::__1::shared_ptr<int>",width*2u,{member("__ptr_",3u,0u),member("__cntrl_",6u,width)}));}
        else {f.types.push_back(object("count handle",width,{member("_M_pi",6u,0u)}));
              f.types.push_back(object("std::shared_ptr<int>",width*2u,{member("_M_ptr",3u,0u),member("_M_refcount",7u,width)}));}
        f.types.back().template_types={0u};f.seal();put(f.bytes,0u,width,256u);put(f.bytes,width,width,512u);put(f.memory,256u,4u,23u);
        put(f.memory,512u,count_width,libcxx?0u:1u);put(f.memory,512u+count_width,count_width,libcxx?0u:1u);
        check(f.run()==pretty::outcome::formatted && f.reads==2u && f.has_copied_int(23u) &&
              f.tree[0u].pretty_summary.find("use_count=1 weak_count=0")!=::std::string::npos,"one strong owner has zero explicit weak references");
        put(f.memory,512u+count_width,count_width,libcxx?1u:2u);
        check(f.run()==pretty::outcome::formatted && f.tree[0u].pretty_summary.find("use_count=1 weak_count=1")!=::std::string::npos,"one explicit weak reference excludes the strong-owner anchor");
        f.types[f.root].name=libcxx?"std::__1::weak_ptr<int>":"std::weak_ptr<int>";
        check(f.run()==pretty::outcome::formatted && f.reads==1u && !f.has_copied_int(23u),"live weak pointer never reads a pointee");
        // libc++ drops the strong anchor as the last shared owner releases:
        // one surviving weak_ptr then leaves __shared_weak_owners_ == 0.
        put(f.memory,512u,count_width,libcxx?dwarf::numeric_mask(count_width):0u);
        put(f.memory,512u+count_width,count_width,libcxx?0u:1u);
        check(f.run()==pretty::outcome::formatted && f.reads==1u && !f.has_copied_int(23u) &&
              f.tree[0u].pretty_summary.find("use_count=0 weak_count=1 expired")!=::std::string::npos,"expired weak reference counts survive anchor retirement");
        pretty::limits cap{};cap.maximum_reads=0u;
        check(f.run(cap)!=pretty::outcome::formatted && f.reads==0u && !f.tree[0u].pretty_available,"zero read budget never borrows a control block");
    }
    void nested_eligibility(unsigned width)
    {
        fixture f{width};f.types.push_back(pointer(0u,width));
        f.types.push_back(object("application::Owner",width,{member("ptr",3u,0u)}));
        f.types.push_back(object("application::Maybe",width*2u,{member("ready",1u,0u),member("payload",4u,width)}));f.seal();
        put(f.bytes,0u,1u,1u);put(f.bytes,width,width,256u);put(f.memory,256u,4u,23u);
        registry::registry profiles{};
        auto owner{registry::parse_host_command("pretty-printer pointer owner application::Owner ptr")};
        auto maybe{registry::parse_host_command("pretty-printer optional maybe application::Maybe ready payload")};
        check(owner.operation==registry::action::install && maybe.operation==registry::action::install,"fixture registry requests");
        profiles.profiles={owner.value,maybe.value};pretty::limits cap{};cap.registry=&profiles;
        f.eligible[width]=::std::byte{};
        check(f.run(cap)==pretty::outcome::formatted && f.reads==0u && !f.has_copied_int(23u),"locally copied active payload preserves denied pointer eligibility");
        f.eligible=f.known;
        check(f.run(cap)==pretty::outcome::formatted && f.reads==1u && f.has_copied_int(23u),"fully qualified nested pointer uses the same bounded callback");
        cap.max_elements=1u;
        check(f.run(cap)==pretty::outcome::formatted && f.reads==0u && !f.has_copied_int(23u),"outer active value consumes the shared synthetic element budget");
        cap.max_elements=200u;cap.maximum_reads=0u;
        check(f.run(cap)==pretty::outcome::formatted && f.reads==0u && !f.has_copied_int(23u),"nested printer cannot renew an exhausted read budget");
        cap.maximum_reads=32u;put(f.bytes,0u,1u,0u);
        check(f.run(cap)==pretty::outcome::formatted && f.reads==0u && f.tree[0u].pretty_summary=="none","inactive payload never initiates its nested pointer printer");
    }
    void bounded_active_summary(unsigned width)
    {
        fixture f{width};f.types[0u].name.assign(4090u,'T');
        auto head{object("active payload",4u,{member("_M_storage",0u,0u)})};head.template_types={0u};f.types.push_back(head);
        auto storage{object("variant union",4u,{member("_M_first",3u,0u)})};storage.kind=dwarf::type_kind::union_type;f.types.push_back(storage);
        f.types.push_back(scalar("variant index",1u,8u));
        auto root{object("std::variant<Payload>",8u,{member("_M_u",4u,0u),member("_M_index",5u,4u)})};root.template_types={0u};f.types.push_back(root);f.seal();
        put(f.bytes,0u,4u,42u);
        check(f.run()==pretty::outcome::formatted && f.has_copied_int(42u) && f.tree[0u].pretty_summary.size()<=4096u,"active-value metadata and scalar preview obey the complete summary byte limit");
    }
}
int main()
{
    for(unsigned width:{4u,8u}) {weak_counts(width,false);weak_counts(width,true);nested_eligibility(width);bounded_active_summary(width);}
    ::std::puts("source pretty value boundary checks passed (copied DATA only)");
}
