// Owned DWARF-shaped DATA and cold reader callbacks only; no VM/frame lease.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
namespace dwarf = ::uwvm2::uwvm::debugger::source_dwarf;
namespace language = ::uwvm2::uwvm::debugger::source_language_expression;
static void check(bool value, char const* message)
{
    if(!value) { ::fast_io::io::perrln("string preview FAIL: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static void encode(::std::vector<::std::byte>& bytes, ::std::size_t offset, ::std::uint64_t value, unsigned width)
{
    auto const first{reinterpret_cast<char*>(bytes.data()+offset)};
    ::fast_io::basic_obuffer_view<char> sink{first,first+width};
    if(width==4u) { ::fast_io::io::print(sink,::fast_io::mnp::le_put<32u>(value)); }
    else { ::fast_io::io::print(sink,::fast_io::mnp::le_put<64u>(value)); }
}
int main(int argc, char** argv)
{
    check(argc==1 || argc==2,"optional range/preview mode");
    auto const mode{argc==2 ? ::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[1])) : ::fast_io::concat_std("all")};
    check(mode=="all" || mode=="range" || mode=="preview","exact mode");
    ::std::size_t cases{};
    for(::std::uint8_t width : {4u,8u})
    for(unsigned family{}; family!=3u; ++family)
    {
        ::std::vector<dwarf::type_record> types(4u);
        types[0u].kind=dwarf::type_kind::scalar;types[0u].encoding=7u;
        types[0u].byte_count=1u;types[0u].byte_size=1u;types[0u].size_known=true;
        types[1u].kind=dwarf::type_kind::pointer;types[1u].referenced_type=0u;
        types[1u].byte_count=width;types[1u].byte_size=width;types[1u].size_known=true;
        types[2u].kind=dwarf::type_kind::scalar;types[2u].encoding=family==0u ? 7u : 5u;
        types[2u].byte_count=width;types[2u].byte_size=width;types[2u].size_known=true;
        auto& string{types[3u]};string.kind=dwarf::type_kind::structure;string.size_known=true;string.byte_size=width*2u;
        string.name=::fast_io::concat_std(::std::string_view{family==0u ? "&str" : "string"});
        string.language=family==0u ? 0x1cu : family==1u ? 0x16u : 0x0cu;
        string.tinygo_producer=family==2u;
        dwarf::member_record pointer{},length{};
        pointer.name=::fast_io::concat_std(::std::string_view{family==0u ? "data_ptr" : "ptr"});pointer.type=1u;pointer.offset_known=true;
        length.name=::fast_io::concat_std(::std::string_view{family==0u ? "length" : "len"});length.type=2u;length.byte_offset=width;length.offset_known=true;
        string.members={pointer,length};
        ::std::vector<::std::byte> root(width*2u),known(root.size(),::std::byte{0xffu}),qualified(root.size(),::std::byte{0xffu});
        ::std::vector<dwarf::object_node> out{};
        ::std::size_t calls{},requested{};::std::uint64_t requested_address{};
        bool short_copy{},fail_copy{},oversized_copy{};
        auto const read{[&](::std::uint64_t address,::std::size_t extent,::std::vector<::std::byte>& bytes)
        {
            ++calls;requested=extent;requested_address=address;
            // Deliberately permissive. The production language layer itself
            // must reject guest-address overflow BEFORE invoking a callback.
            bytes.assign(extent+(oversized_copy ? 1u : 0u),::std::byte{'A'});
            if(extent>=3u) { bytes[0u]=::std::byte{0x1bu};bytes[1u]=::std::byte{'\n'};bytes[2u]=::std::byte{}; }
            if(short_copy && !bytes.empty()) { bytes.pop_back(); }
            return !fail_copy;
        }};
        auto const query{[&](::std::uint64_t address,::std::uint64_t size,language::limits cap=language::limits{})
        {
            encode(root,0u,address,width);encode(root,width,size,width);calls=requested=0u;
            ++cases;return language::value(types,3u,{},root,known,width,read,out,cap,qualified);
        }};
        auto const unavailable{[&]
        { return !out.empty() && !out[0u].display_text_available && !out[0u].display_text_truncated && out[0u].display_text.empty(); }};
        auto const maximum{width==4u ? 0xffffffffull : ~::std::uint64_t{}};
        if(mode!="preview")
        {
            check(query(maximum-3u,8u)==dwarf::inline_query_error::none && calls==0u && unavailable(),"whole string crossing guest address limit must not reach reader");
            check(query(maximum-3u,8192u)==dwarf::inline_query_error::none && calls==0u && unavailable(),"truncated preview still validates whole declared range");
            check(query(maximum-3u,4u)==dwarf::inline_query_error::none && calls==1u && requested==4u && requested_address==maximum-3u &&
                out[0u].display_text_available && out[0u].display_text.size()==4u && !out[0u].display_text_truncated,"exact last guest byte remains valid");
            check(query(maximum,1u)==dwarf::inline_query_error::none && calls==1u && requested==1u && out[0u].display_text=="A","single final byte");
            check(query(0u,3u)==dwarf::inline_query_error::none && calls==0u && unavailable(),"nonempty null pointer");
            check(query(2u,maximum)==dwarf::inline_query_error::none && calls==0u && unavailable(),"signed length or full declared range overflow");
        }
        if(mode!="range")
        {
            check(query(32u,8192u)==dwarf::inline_query_error::none && calls==1u && requested==4096u && requested_address==32u &&
                out[0u].display_text_available && out[0u].display_text.size()==4096u && out[0u].display_text_truncated,"long Rust/Go/TinyGo string has one bounded explicit preview");
            auto const formatted{::fast_io::concat_std(dwarf::object_details_of(out[0u]))};
            check(formatted.find("(truncated)")!=formatted.npos && formatted.find('\n')==formatted.npos &&
                formatted.find('\x1b')==formatted.npos && formatted.find('\0')==formatted.npos,"formatter keeps truncation marker and escapes terminal bytes");
            check(query(32u,4096u)==dwarf::inline_query_error::none && calls==1u && requested==4096u &&
                out[0u].display_text_available && !out[0u].display_text_truncated,"exact display cap is complete");
            check(query(32u,4097u)==dwarf::inline_query_error::none && calls==1u && requested==4096u && out[0u].display_text_truncated,"one byte beyond cap");
            check(query(32u,4u)==dwarf::inline_query_error::none && calls==1u && out[0u].display_text.size()==4u &&
                out[0u].display_text[2u]=='\0' && !out[0u].display_text_truncated,"embedded NUL belongs to length-delimited string");
            short_copy=true;
            check(query(32u,8192u)==dwarf::inline_query_error::none && calls==1u && unavailable(),"incomplete callback does not publish partial text");short_copy=false;
            oversized_copy=true;
            check(query(32u,8192u)==dwarf::inline_query_error::none && calls==1u && unavailable(),"oversized callback does not publish text");oversized_copy=false;
            fail_copy=true;
            check(query(32u,8192u)==dwarf::inline_query_error::none && calls==1u && unavailable(),"failed callback does not publish text");fail_copy=false;
            check(query(32u,8192u,{0u})==dwarf::inline_query_error::none && calls==0u && unavailable(),"no read budget");
            for(::std::size_t i{};i!=width;++i) { qualified[i]=::std::byte{}; }
            check(query(32u,8192u)==dwarf::inline_query_error::none && calls==0u && unavailable(),"unqualified pointer cannot display preview");
            check(query(0u,0u,{0u})==dwarf::inline_query_error::none && calls==0u && out[0u].display_text_available &&
                out[0u].display_text.empty() && !out[0u].display_text_truncated,"empty string needs no pointer or read budget");
            qualified.assign(root.size(),::std::byte{0xffu});
            for(::std::size_t i{width};i!=known.size();++i) { known[i]=::std::byte{}; }
            check(query(32u,8192u)==dwarf::inline_query_error::none && calls==0u && unavailable(),"unknown length");
            known.assign(root.size(),::std::byte{0xffu});
            if(family!=0u)
            {
                string.language=0x0cu;string.tinygo_producer=false;
                check(query(32u,8192u)==dwarf::inline_query_error::none && calls==0u && unavailable(),"C lookalike has no Go producer route");
            }
        }
    }
    ::fast_io::io::println("debug_source_string_preview: PASS cases=",cases," owned DATA only");
}
