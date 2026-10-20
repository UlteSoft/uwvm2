#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
#include <array>
#include <vector>
namespace d = uwvm2::uwvm::debugger::source_dwarf;
namespace e = uwvm2::uwvm::debugger::source_language_expression;
static void require(bool condition, unsigned line=__builtin_LINE())
{ if(!condition) { ::fast_io::io::perrln("TinyGo map layout check failed at ",line);::fast_io::fast_terminate(); } }
static d::type_record type(d::type_kind kind, unsigned width)
{
    d::type_record value{}; value.kind=kind; value.size_known=true;
    value.byte_size=width; value.byte_count=width; value.language=0x0cu;
    value.tinygo_producer=true; return value;
}
static void put(::std::vector<::std::byte>& bytes, unsigned offset, ::std::uint64_t value, unsigned width)
{
    require(offset<=bytes.size() && width<=bytes.size()-offset);
    for(unsigned i{};i!=width;++i) { bytes[offset+i]=static_cast<::std::byte>((value>>(i*8u))&0xffu); }
}
static ::std::vector<d::type_record> layout(unsigned width, unsigned fields)
{
    // Source-owned DATA models of the checked TinyGo runtime layouts. A
    // callback below verifies the only permitted header copy; this is not a
    // VM memory-authority test or a substitute for real producer sessions.
    ::std::vector<d::type_record> types{
        type(d::type_kind::pointer,width), type(d::type_kind::structure,width*(fields==11u?12u:10u)),
        type(d::type_kind::scalar,width), type(d::type_kind::pointer,width),
        type(d::type_kind::scalar,1u), type(d::type_kind::structure,width*2u)};
    types[0u].referenced_type=1u; types[1u].name="runtime.hashmap";
    types[2u].encoding=7u; types[3u].referenced_type=4u; types[4u].encoding=7u;
    auto const member=[&](char const* name,unsigned target,unsigned offset)
    {
        d::member_record value{};value.name=::fast_io::concat_std(::fast_io::mnp::os_c_str(name));
        value.type=target;value.offset_known=true;value.byte_offset=offset;types[1u].members.push_back(value);
    };
    member("buckets",3u,0u);member("seed",2u,width);member("count",2u,width*2u);
    member("keySize",2u,width*3u);member("valueSize",2u,width*4u);
    unsigned trailing{5u};
    if(fields==11u) { member("keySlotSize",2u,width*5u);member("valueSlotSize",2u,width*6u);trailing=7u; }
    member("bucketBits",4u,width*trailing);
    if(fields!=8u) { member("flags",4u,width*trailing+1u); }
    member("keyEqual",5u,width*(trailing+1u));member("keyHash",5u,width*(trailing+3u));
    return types;
}
int main()
{
    d::source_expression length{},capacity{};
    require(d::parse_source_expression("len(value)",length)==d::object_selector_error::none);
    require(d::parse_source_expression("cap(value)",capacity)==d::object_selector_error::none);
    for(unsigned width : {4u,8u}) for(unsigned fields : {8u,9u,11u})
    {
        auto types=layout(width,fields);auto const complete=types;
        ::std::vector<::std::byte> root(width),header(types[1u].byte_size);
        put(root,0u,64u,width);put(header,width*2u,3u,width);
        // Even deliberately hostile buckets/function bytes must never be
        // followed while answering len. Only count in the copied header is used.
        put(header,0u,~::std::uint64_t{},width);put(header,header.size()-width,~::std::uint64_t{},width);
        unsigned reads{};bool refuse{};
        auto read=[&](::std::uint64_t offset,::std::size_t size,::std::vector<::std::byte>& result)
        { ++reads;require(offset==64u && size==header.size());if(refuse) { return false; }result=header;return true; };
        ::std::vector<d::object_node> out{};
        require(e::type(types,0u,length.steps,width,out)==d::inline_query_error::none && reads==0u);
        require(e::value(types,0u,length.steps,root,{},width,read,out)==d::inline_query_error::none &&
            reads==1u && out.size()==1u && out[0u].value_available && out[0u].bits==3u);
        reads=0u;
        require(e::value(types,0u,capacity.steps,root,{},width,read,out)==d::inline_query_error::unavailable && reads==0u && out.empty());
        put(root,0u,0u,width);
        require(e::value(types,0u,length.steps,root,{},width,read,out)==d::inline_query_error::none &&
            reads==0u && out.size()==1u && out[0u].value_available && out[0u].bits==0u);
        put(root,0u,64u,width);refuse=true;
        require(e::value(types,0u,length.steps,root,{},width,read,out)==d::inline_query_error::unavailable && reads==1u && out.empty());
        refuse=false;reads=0u;e::limits cap{};cap.maximum_reads=0u;
        require(e::value(types,0u,length.steps,root,{},width,read,out,cap)==d::inline_query_error::limit_exceeded && reads==0u && out.empty());
        ::std::vector<::std::byte> unknown(width,::std::byte{0xffu});unknown[0u]=::std::byte{};
        require(e::value(types,0u,length.steps,root,unknown,width,read,out)==d::inline_query_error::unavailable && reads==0u && out.empty());
        put(header,width*2u,::std::uint64_t{1u}<<(width*8u-1u),width);
        require(e::value(types,0u,length.steps,root,{},width,read,out)==d::inline_query_error::unavailable && reads==1u && out.empty());
        put(header,width*2u,3u,width);
        for(unsigned mutation{};mutation!=10u;++mutation)
        {
            types=complete;reads=0u;
            switch(mutation)
            {
            case 0u:types[1u].members[2u].byte_offset+=1u;break;
            case 1u:types[2u].encoding=5u;break;
            case 2u:types[1u].members[2u].name="seed";break;
            case 3u:types[1u].byte_size+=width;break;
            case 4u:types[1u].members[2u].offset_known=false;break;
            case 5u:types[1u].tinygo_producer=false;break;
            case 6u:types[0u].address_class_known=true;types[0u].address_class=17u;break;
            case 7u:types[1u].members.back().name="unknown";break;
            case 8u:types[1u].members[2u].bit_field=true;break;
            default:types[1u].members.erase(types[1u].members.begin()+2);break;
            }
            require(e::value(types,0u,length.steps,root,{},width,read,out)!=d::inline_query_error::none && reads==0u && out.empty());
        }
    }
    ::fast_io::io::println("PASS TinyGo map layout DATA: 32/64-bit legacy/flags/slots, bounded header reads and malformed layouts; VM authority qualified separately");
}
