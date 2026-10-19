// Finite standard-library DWARF-shaped DATA. Reads copy owned guest bytes;
// this component fixture never issues a frame, VM or native-address lease.
#include <uwvm2/uwvm/debugger/source_pretty_printer.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <string_view>

namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
namespace pretty = ::uwvm2::uwvm::debugger::source_pretty_printer;
namespace
{
    ::std::size_t checks{};
    void check(bool condition, char const* reason)
    {
        ++checks;
        if(!condition) { ::std::fprintf(stderr,"pretty printer FAIL: %s\n",reason); ::std::exit(1); }
    }
    void put(::std::vector<::std::byte>& bytes, ::std::size_t offset, ::std::uint64_t value, unsigned width)
    {
        check(offset<=bytes.size() && width<=bytes.size()-offset,"bounded fixture encoding");
        for(unsigned i{};i!=width;++i) { bytes[offset+i]=static_cast<::std::byte>((value>>(i*8u))&255u); }
    }
    dwarf::type_record scalar(char const* name, unsigned width, bool sign=false)
    {
        dwarf::type_record out{};out.name=name;out.kind=dwarf::type_kind::scalar;
        out.language=0x21u;out.encoding=sign ? 5u : 7u;out.byte_count=width;
        out.byte_size=width;out.size_known=true;return out;
    }
    dwarf::type_record pointer(::std::size_t target, unsigned width)
    {
        dwarf::type_record out{};out.kind=dwarf::type_kind::pointer;out.language=0x21u;
        out.referenced_type=target;out.byte_count=width;out.byte_size=width;out.size_known=true;return out;
    }
    dwarf::member_record member(char const* name, ::std::size_t type, ::std::uint64_t offset)
    {
        dwarf::member_record out{};out.name=name;out.type=type;out.byte_offset=offset;out.offset_known=true;return out;
    }
    dwarf::type_record object(char const* name, ::std::uint64_t size, ::std::vector<dwarf::member_record> members)
    {
        dwarf::type_record out{};out.name=name;out.kind=dwarf::type_kind::structure;out.language=0x21u;
        out.byte_size=size;out.size_known=true;out.members=::std::move(members);return out;
    }
    struct fixture
    {
        unsigned width{};::std::size_t root{};
        ::std::vector<dwarf::type_record> types{};
        ::std::vector<::std::byte> bytes{},known{},qualified{},memory{};
        ::std::size_t calls{},copied{};bool fail{},short_read{},long_read{};
        ::std::vector<dwarf::object_node> tree{};
        explicit fixture(unsigned address_width):width(address_width),memory(4096u) {}
        void seal()
        { bytes.resize(static_cast<::std::size_t>(types[root].byte_size));known.assign(bytes.size(),::std::byte{255u});qualified=known; }
        bool read(::std::uint64_t offset,::std::size_t size,::std::vector<::std::byte>& out)
        {
            ++calls;copied+=size;
            if(fail || offset>memory.size() || size>memory.size()-static_cast<::std::size_t>(offset)) { return false; }
            out.assign(memory.begin()+static_cast<::std::size_t>(offset),memory.begin()+static_cast<::std::size_t>(offset)+size);
            if(short_read && !out.empty()) { out.pop_back(); }
            if(long_read) { out.push_back(::std::byte{}); }
            return true;
        }
        pretty::outcome format(pretty::limits const& cap={})
        {
            calls=copied=0u;tree.clear();
            check(dwarf::query_object_value_with_known_bits(types,root,bytes,known,tree)==dwarf::inline_query_error::none,
                  "production copied object tree");
            auto reader{[&](::std::uint64_t offset,::std::size_t size,::std::vector<::std::byte>& out)
            { return read(offset,size,out); }};
            return pretty::format(types,root,bytes,known,qualified,static_cast<::std::uint8_t>(width),reader,tree,cap);
        }
        void vector()
        {
            types={scalar("int",4u,true),pointer(0u,width)};
            types.push_back(object("_Vector_impl_data",width*3u,{member("_M_start",1u,0u),member("_M_finish",1u,width),member("_M_end_of_storage",1u,width*2u)}));
            types.push_back(object("std::vector<int, std::allocator<int> >",width*3u,{member("_M_impl",2u,0u)}));
            root=3u;seal();put(bytes,0u,256u,width);put(bytes,width,320u,width);put(bytes,width*2u,384u,width);
            for(unsigned i{};i!=16u;++i) { put(memory,256u+i*4u,i*3u+1u,4u); }
        }
        void string(bool view=false)
        {
            types={scalar("char",1u),pointer(0u,width),scalar("size_t",width)};
            if(view) { types.push_back(object("std::basic_string_view<char, std::char_traits<char> >",width*2u,{member("_M_len",2u,0u),member("_M_str",1u,width)})); }
            else
            {
                types.push_back(object("_Alloc_hider",width,{member("_M_p",1u,0u)}));
                types.push_back(object("std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >",width*2u+16u,
                                       {member("_M_dataplus",3u,0u),member("_M_string_length",2u,width)}));
            }
            root=types.size()-1u;seal();put(bytes,view ? width : 0u,512u,width);put(bytes,view ? 0u : width,8u,width);
            char const text[8]{'a','b','\0','c','\n','\x1b','"','z'};
            for(unsigned i{};i!=8u;++i) { memory[512u+i]=static_cast<::std::byte>(text[i]); }
        }
    };
    ::std::uint64_t functional()
    {
        ::std::uint64_t checksum{};
        for(unsigned width : {4u,8u})
        {
            fixture v{width};v.vector();pretty::limits cap{};cap.max_elements=8u;
            pretty::details::indexed_sequence plan{};
            check(pretty::details::index_plan(v.types,v.root,v.bytes,v.known,v.qualified,width,plan) &&
                  plan.element==0u && plan.address==256u && plan.count==16u && v.calls==0u,"vector indexing plan reads no memory");
            v.qualified[0u]=::std::byte{};
            check(!pretty::details::index_plan(v.types,v.root,v.bytes,v.known,v.qualified,width,plan) && v.calls==0u,"index plan rejects unqualified pointer");
            v.qualified=v.known;v.known[0u]=::std::byte{};
            check(!pretty::details::index_plan(v.types,v.root,v.bytes,v.known,v.qualified,width,plan) && v.calls==0u,"index plan intersects availability with pointer qualification");
            v.known=v.qualified;
            dwarf::source_expression expression{};
            check(dwarf::parse_source_expression("v[1]",expression)==dwarf::object_selector_error::none,"container index expression parses");
            auto reader{[&](::std::uint64_t offset,::std::size_t size,::std::vector<::std::byte>& out) { return v.read(offset,size,out); }};
            ::std::vector<dwarf::object_node> indexed{};
            check(::uwvm2::uwvm::debugger::source_language_expression::type(v.types,v.root,expression.steps,
                  static_cast<::std::uint8_t>(width),indexed)==dwarf::inline_query_error::none && indexed.size()==1u &&
                  indexed[0u].type==0u && indexed[0u].byte_size==4u && v.calls==0u,
                  "container ptype and sizeof planning use metadata without a value read");
            check(::uwvm2::uwvm::debugger::source_language_expression::value(v.types,v.root,expression.steps,v.bytes,v.known,
                  static_cast<::std::uint8_t>(width),reader,indexed,{},v.qualified)==dwarf::inline_query_error::none &&
                  v.calls==1u && v.copied==4u && indexed.size()==1u && indexed[0u].value_available && indexed[0u].bits==4u,
                  "production container indexing copies only selected element");
            for(::std::int64_t index : {-1,16})
            {
                v.calls=v.copied=0u;expression.steps[0u].index=index;
                check(::uwvm2::uwvm::debugger::source_language_expression::value(v.types,v.root,expression.steps,v.bytes,v.known,
                      static_cast<::std::uint8_t>(width),reader,indexed,{},v.qualified)==dwarf::inline_query_error::unavailable && v.calls==0u,
                      "negative and out-of-range container index never read memory");
            }
            cap.max_elements=0u;
            check(v.format(cap)==pretty::outcome::formatted && v.calls==0u && v.tree[0u].pretty_available &&
                  v.tree[0u].pretty_truncated && v.tree[0u].pretty_omitted==16u,"summary-only policy has no heap reads or synthetic values");
            cap.max_elements=8u;
            check(v.format(cap)==pretty::outcome::formatted,"libstdc++ vector recognized");
            check(v.tree[0u].pretty_available && !v.tree[0u].pretty_summary.empty() && !v.tree[0u].pretty_printer.empty(),"vector summary available");
            ::std::size_t elements{};
            for(auto const& node:v.tree) if(node.detached_copy && node.parent==0u && node.depth==1u)
            { check(node.value_available && node.bits==elements*3u+1u,"copied vector element value");++elements; }
            check(elements==8u && v.tree[0u].pretty_truncated && v.tree[0u].pretty_omitted==8u,"explicit vector child limit");checksum+=v.tree[0u].pretty_summary.size()+elements;
            v.qualified.assign(v.qualified.size(),::std::byte{});
            check(v.format(cap)!=pretty::outcome::formatted && v.calls==0u && !v.tree[0u].pretty_available,"unqualified pointer never read");
            v.qualified=v.known;v.fail=true;
            check(v.format(cap)!=pretty::outcome::formatted && !v.tree[0u].pretty_available,"failed copy unavailable");v.fail=false;
            v.short_read=true;
            check(v.format(cap)!=pretty::outcome::formatted && !v.tree[0u].pretty_available,"short copy unavailable");v.short_read=false;
            v.long_read=true;
            check(v.format(cap)!=pretty::outcome::formatted && !v.tree[0u].pretty_available,"oversized copy unavailable");v.long_read=false;
            cap.maximum_reads=0u;
            check(v.format(cap)!=pretty::outcome::formatted && v.calls==0u,"read budget enforced");cap.maximum_reads=32u;
            v.known[width]=::std::byte{};v.qualified=v.known;
            check(v.format(cap)!=pretty::outcome::formatted && v.calls==0u,"unknown endpoint remains unavailable");
            v.known.assign(v.known.size(),::std::byte{255u});v.qualified=v.known;
            put(v.bytes,width*2u,316u,width);
            check(v.format(cap)!=pretty::outcome::formatted && v.calls==0u,"capacity before finish rejected before read");
            put(v.bytes,width*2u,384u,width);
            put(v.bytes,width,252u,width);
            check(v.format(cap)!=pretty::outcome::formatted && v.calls==0u,"reversed vector pointers rejected before read");
            put(v.bytes,width,256u,width);put(v.bytes,width*2u,256u,width);
            check(v.format(cap)==pretty::outcome::formatted && v.calls==0u,"empty vector needs no copy");
            v.types[v.root].language=0x0cu;
            check(v.format(cap)==pretty::outcome::not_matched && v.calls==0u,"C lookalike has no standard C++ profile");
            for(bool view : {false,true})
            {
                fixture s{width};s.string(view);
                check(s.format(cap)==pretty::outcome::formatted && s.tree[0u].pretty_available,"libstdc++ string or view recognized");
                check(s.tree[0u].display_text.size()==8u && s.tree[0u].display_text[2u]=='\0',"owned string preserves embedded NUL");
                auto const rendered{::fast_io::concat_std(dwarf::escaped_metadata_text{s.tree[0u].pretty_summary})};
                check(rendered.find('\x1b')==::std::string::npos && rendered.find('\n')==::std::string::npos &&
                      rendered.find('\0')==::std::string::npos,"final summary renderer escapes terminal data");
                checksum+=s.tree[0u].pretty_summary.size();
                put(s.bytes,view ? width : 0u,width==4u ? 0xfffffffeull : 0xfffffffffffffffeull,width);
                check(s.format(cap)!=pretty::outcome::formatted && s.calls==0u,"declared string crosses address limit");
                check(!pretty::details::index_plan(s.types,s.root,s.bytes,s.known,s.qualified,width,plan) && s.calls==0u,
                      "container index plan validates complete declared range before selection");
            }
        }
        return checksum;
    }
}
int main(int argc,char** argv)
{
    ::std::size_t iterations{1u};bool gated{};
    if((argc==3 || argc==4) && ::std::string_view{argv[1]}=="--iterations")
    {
        char* end{};unsigned long long const parsed_iterations{::std::strtoull(argv[2],&end,10)};
        check(end && end!=argv[2] && *end=='\0' && parsed_iterations>=1u && parsed_iterations<=100000000u,"bounded iterations");
        iterations=static_cast<::std::size_t>(parsed_iterations);
        if(argc==4) { check(::std::string_view{argv[3]}=="--gate","exact profiling gate option");gated=true; }
    }
    else { check(argc==1,"usage: pretty_printer_bench [--iterations N [--gate]]"); }
    auto const proof{functional()};fixture v{4u};v.vector();pretty::limits cap{};cap.max_elements=8u;
    if(gated) { ::std::fputs("UWVM_PRETTY_READY\n",stderr);::std::fflush(stderr);check(::std::getchar()=='G',"profiling start gate"); }
    ::std::uint64_t checksum{proof};auto const begin{::std::chrono::steady_clock::now()};
    for(::std::size_t i{};i!=iterations;++i)
    {
        check(v.format(cap)==pretty::outcome::formatted && v.tree[0u].pretty_available,"steady formatter correctness");
        checksum+=v.tree[0u].pretty_summary.size();for(auto const& node:v.tree) { if(node.detached_copy) { checksum+=node.bits; } }
    }
    auto const nanoseconds{::std::chrono::duration_cast<::std::chrono::nanoseconds>(::std::chrono::steady_clock::now()-begin).count()};
    if(gated) { ::std::fputs("UWVM_PRETTY_DONE\n",stderr);::std::fflush(stderr);check(::std::getchar()=='D',"profiling completion gate"); }
    ::std::printf("{\"passed\":true,\"scope\":\"owned formatter DATA; no full debugger parity\",\"iterations\":%zu,\"checks\":%zu,\"nanoseconds\":%lld,\"checksum\":%llu}\n",
                  iterations,checks,static_cast<long long>(nanoseconds),static_cast<unsigned long long>(checksum));
}
