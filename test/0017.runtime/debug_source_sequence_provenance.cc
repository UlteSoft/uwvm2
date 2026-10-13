#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
using namespace uwvm2::uwvm::debugger;
namespace
{
    ::std::size_t checks{};
    void check(bool value, ::std::string_view reason)
    {
        ++checks;
        if(!value) { ::fast_io::io::perrln("sequence provenance FAIL: ",reason); ::fast_io::fast_terminate(); }
    }
    void encode(::std::vector<::std::byte>& bytes, ::std::size_t offset, ::std::uint64_t value, ::std::uint8_t width)
    {
        check(offset <= bytes.size() && width <= bytes.size()-offset,"fixture bounded carrier");
        // [owned bytes ... offset ... offset+width<=size] end
        // [safe                                          ] checked BEFORE pointer arithmetic.
        auto const first{reinterpret_cast<char*>(bytes.data()+offset)};
        ::fast_io::basic_obuffer_view<char> sink{first,first+width};
        if(width==4u) { ::fast_io::io::print(sink,::fast_io::mnp::le_put<32u>(value)); }
        else { ::fast_io::io::print(sink,::fast_io::mnp::le_put<64u>(value)); }
    }
    source_dwarf::type_record scalar(::std::uint8_t width)
    {
        source_dwarf::type_record t{};t.kind=source_dwarf::type_kind::scalar;
        t.encoding=7u;t.byte_count=width;t.byte_size=width;t.size_known=true;return t;
    }
    source_dwarf::source_expression parse(::std::string_view text)
    {
        source_dwarf::source_expression out{};
        check(source_dwarf::parse_source_expression(text,out)==source_dwarf::object_selector_error::none,"fixture selector");
        return out;
    }
    ::std::vector<source_dwarf::type_record> sequence(::std::uint8_t width, ::std::uint64_t extent, unsigned language)
    {
        ::std::vector<source_dwarf::type_record> t(5u);
        t[0u]=scalar(extent==1u ? 1u : 4u);t[2u]=scalar(width);t[4u]=t[0u];
        if(extent>4u)
        {
            t[4u]={};t[4u].kind=source_dwarf::type_kind::structure;t[4u].size_known=true;t[4u].byte_size=extent;
            source_dwarf::member_record value{};value.name=::fast_io::concat_std("value");value.type=0u;value.offset_known=true;
            t[4u].members={value};
        }
        auto& pointer{t[1u]};pointer.kind=source_dwarf::type_kind::pointer;pointer.size_known=true;
        pointer.byte_size=pointer.byte_count=width;pointer.referenced_type=4u;
        auto& root{t[3u]};root.kind=source_dwarf::type_kind::structure;root.size_known=true;
        root.language=language==2u ? 0x0cu : language==1u ? 0x16u : 0x1cu;root.tinygo_producer=language==2u;
        root.name=::fast_io::concat_std(::std::string_view{language==0u ? "&[T]" : "[]T"});
        root.byte_size=width*(language==0u ? 2u : 3u);
        source_dwarf::member_record pointer_member{},length{};
        pointer_member.name=::fast_io::concat_std(::std::string_view{language==0u ? "data_ptr" : "ptr"});pointer_member.type=1u;pointer_member.offset_known=true;
        length.name=::fast_io::concat_std(::std::string_view{language==0u ? "length" : "len"});length.type=2u;length.offset_known=true;length.byte_offset=width;
        root.members={pointer_member,length};
        if(language!=0u)
        {
            auto capacity{length};capacity.name=::fast_io::concat_std("cap");capacity.byte_offset=width*2u;root.members.push_back(capacity);
        }
        return t;
    }
    // Deliberately permissive owned DATA provider: the evaluator itself must
    // reject a complete declared range crossing the guest address width.
    // No producer, active frame, runtime memory or VM authority is exercised.
    void ranges()
    {
        for(::std::uint8_t width : {4u,8u})
        {
            auto const maximum{width==4u ? 0xffffffffull : ~::std::uint64_t{}};
            for(unsigned language{};language!=3u;++language)
            {
                for(::std::uint64_t extent : {1u,4u,64u,65536u})
                {
                    auto types{sequence(width,extent,language)};
                    auto expr{parse(extent>4u ? "s[0].value" : "s[0]")};
                    ::std::vector<::std::byte> root(static_cast<::std::size_t>(types[3u].byte_size));
                    auto test{[&](::std::uint64_t address,::std::uint64_t length,::std::int64_t index,bool expected,::std::uint64_t expected_offset)
                    {
                        encode(root,0u,address,width);encode(root,width,length,width);expr.steps[0u].index=index;
                        ::std::size_t calls{};::std::uint64_t observed{};
                        auto reader{[&](::std::uint64_t offset,::std::size_t count,::std::vector<::std::byte>& copied)
                        {
                            ++calls;observed=offset;check(count==extent,"only complete selected element");
                            copied.assign(count,::std::byte{});copied[0u]=::std::byte{42u};return true;
                        }};
                        ::std::vector<source_dwarf::object_node> out{};
                        auto status{source_language_expression::value(types,3u,expr.steps,root,{},width,reader,out)};
                        if(expected)
                        { check(status==source_dwarf::inline_query_error::none && calls==1u && observed==expected_offset &&
                            out.size()==1u && out[0u].value_available && out[0u].bits==42u,"valid complete sequence at address boundary"); }
                        else
                        { check(status==source_dwarf::inline_query_error::unavailable && calls==0u && out.empty(),
                            "whole slice crossing address width must fail before callback"); }
                        // The type depends on metadata, not runtime length/address.
                        auto const type_status{source_language_expression::type(types,3u,expr.steps,width,out)};
                        check(index<0 ? type_status==source_dwarf::inline_query_error::unavailable :
                            type_status==source_dwarf::inline_query_error::none && out.size()==1u && out[0u].type==(extent>4u ? 0u : 4u),
                            "type-only selector performs no memory operation");
                    }};
                    test(maximum-extent+1u,1u,0,true,maximum-extent+1u);
                    test(maximum-extent+1u,2u,0,false,0u);
                    test(maximum-extent*2u+1u,2u,1,true,maximum-extent+1u);
                    test(maximum-extent*2u+1u,3u,0,false,0u);
                    if(extent>1u)
                    {
                        test(maximum-extent+2u,1u,0,false,0u);
                        test(1u,maximum/extent+1u,0,false,0u);
                    }
                    test(1u,maximum/extent,0,true,1u);
                    test(32u,3u,2,true,32u+extent*2u);
                    test(32u,3u,3,false,0u);test(32u,3u,-1,false,0u);
                    test(0u,1u,0,false,0u);test(32u,0u,0,false,0u);
                    if(language!=0u)
                    {
                        // A compiler may omit cap's DW_OP_piece. Never promote
                        // an unknown capacity into a prerequisite for indexing.
                        encode(root,0u,32u,width);encode(root,width,3u,width);expr.steps[0u].index=0;
                        ::std::vector<::std::byte> known(root.size(),::std::byte{0xffu});
                        for(::std::size_t i{width*2u};i!=known.size();++i) { known[i]=::std::byte{}; }
                        ::std::size_t calls{};
                        auto reader{[&](::std::uint64_t,::std::size_t count,::std::vector<::std::byte>& copied)
                        { ++calls;copied.assign(count,::std::byte{});copied[0u]=::std::byte{42u};return true; }};
                        ::std::vector<source_dwarf::object_node> out{};
                        check(source_language_expression::value(types,3u,expr.steps,root,known,width,reader,out)==source_dwarf::inline_query_error::none &&
                            calls==1u && out.size()==1u && out[0u].bits==42u,"Go/TinyGo missing cap remains indexable");
                    }
                }
            }
        }
    }
    void gates(bool only_members = false)
    {
        for(::std::uint8_t width : {4u,8u})
        {
            if(!only_members) for(::std::string_view name : {"&[u8]","&mut [u8]","&str","&mut str"})
            {
                for(unsigned language : {0u,0x02u,0x04u,0x0cu,0x16u,0x1cu})
                {
                    auto types{sequence(width,1u,0u)};types[3u].name=::fast_io::concat_std(name);types[3u].language=language;
                    ::std::vector<::std::byte> root(width*2u);encode(root,0u,32u,width);encode(root,width,2u,width);
                    auto expr{parse("s[0]")};::std::size_t calls{};
                    auto reader{[&](::std::uint64_t,::std::size_t count,::std::vector<::std::byte>& copied)
                    { ++calls;copied.assign(count,::std::byte{42u});return true; }};
                    ::std::vector<source_dwarf::object_node> out{};
                    auto status{source_language_expression::value(types,3u,expr.steps,root,{},width,reader,out)};
                    check(language==0x1cu ? status==source_dwarf::inline_query_error::none && calls==1u :
                        status==source_dwarf::inline_query_error::unavailable && calls==0u && out.empty(),
                        "Rust sequence needs original Rust compilation unit");
                    status=source_language_expression::type(types,3u,expr.steps,width,out);
                    check(language==0x1cu ? status==source_dwarf::inline_query_error::none && out.size()==1u && out[0u].type==4u :
                        status==source_dwarf::inline_query_error::unavailable && out.empty(),"type query same Rust language gate");
                    if(name=="&str" || name=="&mut str")
                    {
                        calls=0u;status=source_language_expression::value(types,3u,{},root,{},width,reader,out);
                        check(status==source_dwarf::inline_query_error::none && !out.empty() &&
                            (language==0x1cu ? calls==1u && out[0u].display_text_available :
                                calls==0u && !out[0u].display_text_available),"Rust string display same language gate");
                    }
                }
            }
            for(::std::string_view name : {"&Node","*Node","Node",""})
            {
                for(unsigned producer{};producer!=6u;++producer)
                {
                    ::std::vector<source_dwarf::type_record> types(3u);types[0u]=scalar(4u);
                    auto& pointer{types[1u]};pointer.kind=source_dwarf::type_kind::pointer;pointer.size_known=true;
                    pointer.byte_size=pointer.byte_count=width;pointer.referenced_type=2u;pointer.name=::fast_io::concat_std(name);
                    constexpr unsigned languages[]{0u,0x0cu,0x04u,0x1cu,0x16u,0x0cu};
                    pointer.language=languages[producer];pointer.tinygo_producer=producer==5u;
                    auto& object{types[2u]};object.kind=source_dwarf::type_kind::structure;object.size_known=true;object.byte_size=4u;
                    source_dwarf::member_record member{};member.name=::fast_io::concat_std("value");member.type=0u;member.offset_known=true;object.members={member};
                    ::std::vector<::std::byte> root(width);encode(root,0u,32u,width);::std::size_t calls{};
                    auto reader{[&](::std::uint64_t offset,::std::size_t count,::std::vector<::std::byte>& copied)
                    { ++calls;check(offset==32u && count==4u,"complete pointer object only");copied.assign(count,::std::byte{});copied[0u]=::std::byte{42u};return true; }};
                    auto dot{parse("p.value")};auto arrow{parse("p->value")};::std::vector<source_dwarf::object_node> out{};
                    bool const implicit{producer==4u || producer==5u || (producer==3u && name.starts_with("&"))};
                    auto status{source_language_expression::value(types,1u,dot.steps,root,{},width,reader,out)};
                    check(implicit ? status==source_dwarf::inline_query_error::none && calls==1u && out.size()==1u && out[0u].bits==42u :
                        status==source_dwarf::inline_query_error::unavailable && calls==0u && out.empty(),"implicit dot needs matching language metadata");
                    status=source_language_expression::type(types,1u,dot.steps,width,out);
                    check(implicit ? status==source_dwarf::inline_query_error::none && out.size()==1u && out[0u].type==0u :
                        status==source_dwarf::inline_query_error::unavailable && out.empty(),"type dot same language gate");
                    calls=0u;
                    check(source_language_expression::value(types,1u,arrow.steps,root,{},width,reader,out)==source_dwarf::inline_query_error::none &&
                        calls==1u && out.size()==1u && out[0u].bits==42u,"explicit pointer dereference retained");
                    if(producer==2u)
                    {
                        pointer.reference_type=true;calls=0u;
                        check(source_language_expression::value(types,1u,dot.steps,root,{},width,reader,out)==source_dwarf::inline_query_error::none &&
                            calls==1u && out[0u].bits==42u,"C++ DWARF reference dot retained");
                    }
                }
            }
        }
    }
}
int main(int argc,char** argv)
{
    auto const mode{argc==2 ? ::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[1])) : ::fast_io::concat_std("all")};
    if(mode=="all" || mode=="gates") { gates(); }
    if(mode=="members") { gates(true); }
    if(mode=="all" || mode=="range") { ranges(); }
    check(mode=="all" || mode=="gates" || mode=="range" || mode=="members","fixture mode");
    ::fast_io::io::println("debug_source_sequence_provenance: PASS checks=",checks," owned DATA only");
}
