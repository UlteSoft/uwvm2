#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
#include <uwvm2/uwvm/debugger/source_map.h>
using namespace uwvm2::uwvm::debugger;
static void check(bool v,char const* message)
{ if(!v) { ::fast_io::io::perrln("sequences FAIL ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static void encode(::std::vector<::std::byte>& out,::std::size_t offset,::std::uint64_t bits,unsigned width)
{
    auto const first{reinterpret_cast<char*>(out.data()+offset)}; ::fast_io::basic_obuffer_view<char> sink{first,first+width};
    if(width==4u) { ::fast_io::io::print(sink,::fast_io::mnp::le_put<32u>(bits)); }
    else { ::fast_io::io::print(sink,::fast_io::mnp::le_put<64u>(bits)); }
}
int main()
{
    check(source_map_details::normalized_path("/tmp/build/../../../home/user/a/./probe.zig")=="/home/user/a/probe.zig","absolute producer path");
    check(source_map_details::normalized_path("a/../../b")=="../b","relative parent retained");
    check(source_map_details::normalized_path("C:\\work\\out\\..\\probe.cc")=="C:/work/probe.cc","Windows separators");
    {
        // Actual DWARF4 rows: flags belong to one emitted row and must reset.
        constexpr unsigned char payload[]{62,0,0,0,4,0,32,0,0,0,1,1,1,251,14,13,0,1,1,1,1,0,0,0,1,0,0,1,0,112,114,111,98,101,46,99,99,0,0,0,0,0,0,5,2,10,0,0,0,1,10,2,5,1,2,2,1,11,2,3,1,2,5,0,1,1};
        source_map map{}; source_map_sections sections{};
        sections.debug_line = {reinterpret_cast<::std::byte const*>(payload),sizeof(payload)}; sections.code_section_content_size = 30u;
        check(source_map::parse(0u,sections,map)==source_map_error::none,"transient line flags parse");
        auto first{map.lookup(0u,10u)}; auto prologue{map.lookup(0u,15u)}; auto reset{map.lookup(0u,17u)}; auto epilogue{map.lookup(0u,20u)};
        check(first && !first->prologue_end && prologue && prologue->prologue_end && !prologue->epilogue_begin &&
            reset && !reset->prologue_end && !reset->epilogue_begin && epilogue && epilogue->epilogue_begin,"prologue/epilogue row flags reset");
    }
    for(::std::uint8_t width : {4u,8u})
    {
        ::std::vector<source_dwarf::type_record> types(4u);
        auto& scalar{types[0u]};scalar.kind=source_dwarf::type_kind::scalar;scalar.encoding=7u;scalar.byte_count=4u;scalar.byte_size=4u;scalar.size_known=true;
        auto& pointer{types[1u]};pointer.kind=source_dwarf::type_kind::pointer;pointer.byte_count=width;pointer.byte_size=width;pointer.size_known=true;pointer.referenced_type=0u;
        types[2u]=scalar;types[2u].byte_count=width;types[2u].byte_size=width;
        auto& slice{types[3u]};slice.kind=source_dwarf::type_kind::structure;slice.language=0x1cu;slice.name=::fast_io::concat_std("&[u32]");slice.byte_size=width*2u;slice.size_known=true;
        source_dwarf::member_record data{},length{};data.name=::fast_io::concat_std("data_ptr");data.type=1u;data.offset_known=true;
        length.name=::fast_io::concat_std("length");length.type=2u;length.byte_offset=width;length.offset_known=true;slice.members={data,length};
        ::std::vector<::std::byte> root(width*2u),memory(96u);encode(root,0u,32u,width);encode(root,width,3u,width);
        encode(memory,32u,13u,4u);encode(memory,36u,14u,4u);encode(memory,40u,15u,4u);
        ::std::size_t calls{};
        auto const read{[&](::std::uint64_t offset,::std::size_t extent,::std::vector<::std::byte>& out)
        { ++calls;out.clear();if(offset>memory.size() || extent>memory.size()-offset) { return false; }
          out.resize(extent);::fast_io::freestanding::my_memcpy(out.data(),memory.data()+offset,extent);return true; }};
        source_dwarf::source_expression expr{};::std::vector<source_dwarf::object_node> out{};
        check(source_dwarf::parse_source_expression("slice[1]",expr)==source_dwarf::object_selector_error::none,"slice selector");
        check(source_language_expression::value(types,3u,expr.steps,root,{},width,read,out)==source_dwarf::inline_query_error::none && out.size()==1u && out[0u].bits==14u && calls==1u,"bounded guest slice indexing");
        check(source_language_expression::type(types,3u,expr.steps,width,out)==source_dwarf::inline_query_error::none && out.size()==1u && out[0u].type==0u,"slice element type without memory");
        expr.steps[0u].index=3;calls=0u;
        check(source_language_expression::value(types,3u,expr.steps,root,{},width,read,out)==source_dwarf::inline_query_error::unavailable && calls==0u && out.empty(),"index equal to length fails before guest read");
        expr.steps[0u].index=-1;
        check(source_language_expression::value(types,3u,expr.steps,root,{},width,read,out)==source_dwarf::inline_query_error::unavailable && calls==0u,"negative index fails");
        expr.steps[0u].index=1;::std::vector<::std::byte> mask(root.size(),::std::byte{0xffu}),qualified(root.size(),::std::byte{});
        check(source_language_expression::value(types,3u,expr.steps,root,mask,width,read,out,{},qualified)==source_dwarf::inline_query_error::unavailable && calls==0u,"unqualified pointer carrier fails");
        encode(root,0u,width==4u ? 0xfffffffcu : ~::std::uint64_t{3u},width);
        check(source_language_expression::value(types,3u,expr.steps,root,{},width,read,out)==source_dwarf::inline_query_error::unavailable && calls==0u,"address arithmetic overflow fails before callback");
        types[3u].name=::fast_io::concat_std("&str");types[0u].byte_size=types[0u].byte_count=1u;encode(root,0u,32u,width);encode(root,width,4u,width);
        ::fast_io::freestanding::my_memcpy(memory.data()+32u,"uwvm",4u);
        check(source_language_expression::value(types,3u,{},root,{},width,read,out)==source_dwarf::inline_query_error::none && out[0u].display_text_available && out[0u].display_text=="uwvm","Rust string bytes via guest copy");
        encode(root,0u,0u,width);encode(root,width,0u,width);calls=0u;
        check(source_language_expression::value(types,3u,{},root,{},width,read,out)==source_dwarf::inline_query_error::none && out[0u].display_text_available && out[0u].display_text.empty() && calls==0u,"empty Rust string needs no pointer dereference");
        // TinyGo Go layouts: cap may be genuinely omitted by DW_OP_piece.
        auto& go{types[3u]}; go.language=0x16u;go.name=::fast_io::concat_std("[]uint8");go.byte_size=width*3u;
        data.name=::fast_io::concat_std("ptr");length.name=::fast_io::concat_std("len");
        source_dwarf::member_record capacity{length};capacity.name=::fast_io::concat_std("cap");capacity.byte_offset=width*2u;
        go.members={data,length,capacity};root.resize(width*3u);encode(root,0u,32u,width);encode(root,width,4u,width);encode(root,width*2u,0u,width);
        mask.assign(root.size(),::std::byte{0xffu});for(::std::size_t i{width*2u};i!=mask.size();++i) { mask[i]=::std::byte{}; }
        check(source_dwarf::parse_source_expression("s[1]",expr)==source_dwarf::object_selector_error::none,"Go slice expression");calls=0u;
        check(source_language_expression::value(types,3u,expr.steps,root,mask,width,read,out)==source_dwarf::inline_query_error::none &&
            out.size()==1u && out[0u].bits==static_cast<unsigned>('w') && calls==1u,"Go index works with unavailable cap");
        go.language=0x0cu;go.tinygo_producer=true;calls=0u;
        check(source_language_expression::value(types,3u,expr.steps,root,mask,width,read,out)==source_dwarf::inline_query_error::none && out[0u].bits==static_cast<unsigned>('w') && calls==1u,"actual TinyGo C99 producer adapter");
        go.tinygo_producer=false;calls=0u;
        check(source_language_expression::value(types,3u,expr.steps,root,mask,width,read,out)==source_dwarf::inline_query_error::unavailable && calls==0u,"lookalike layout lacks Go CU provenance");
        go.language=0x16u;go.name=::fast_io::concat_std("string");go.byte_size=width*2u;go.members={data,length};root.resize(width*2u);mask.resize(root.size());calls=0u;
        check(source_language_expression::value(types,3u,{},root,mask,width,read,out)==source_dwarf::inline_query_error::none &&
            out[0u].display_text_available && out[0u].display_text=="uwvm" && calls==1u,"Go string bytes");
        for(::std::size_t i{};i!=width;++i) { mask[i]=::std::byte{}; }calls=0u;
        check(source_language_expression::value(types,3u,{},root,mask,width,read,out)==source_dwarf::inline_query_error::none &&
            !out[0u].display_text_available && calls==0u,"missing Go string pointer remains unavailable");
        encode(root,width,0u,width);
        check(source_language_expression::value(types,3u,{},root,mask,width,read,out)==source_dwarf::inline_query_error::none &&
            out[0u].display_text_available && out[0u].display_text.empty() && calls==0u,"empty Go string needs no pointer");
        // Unnamed Go pointer dot; identical C pointer keeps C syntax.
        go.name=::fast_io::concat_std("main.Box");go.byte_size=4u;go.members={};
        source_dwarf::member_record member{};member.name=::fast_io::concat_std("Value");member.type=0u;member.offset_known=true;go.members={member};
        types[1u].referenced_type=3u;types[1u].language=0x0cu;types[1u].tinygo_producer=true;types[0u].byte_size=types[0u].byte_count=4u;
        encode(root,0u,32u,width);encode(memory,32u,29u,4u);check(source_dwarf::parse_source_expression("p.Value",expr)==source_dwarf::object_selector_error::none,"Go pointer dot");calls=0u;
        check(source_language_expression::value(types,1u,expr.steps,root,{},width,read,out)==source_dwarf::inline_query_error::none && out[0u].bits==29u && calls==1u,"unnamed Go pointer dot reads owned pointee");
        types[1u].language=0x0cu;types[1u].tinygo_producer=false;calls=0u;
        check(source_language_expression::value(types,1u,expr.steps,root,{},width,read,out)==source_dwarf::inline_query_error::unavailable && calls==0u,"C pointer dot rejected");
        types[1u].referenced_type=0u;types[0u].byte_size=types[0u].byte_count=1u;mask.assign(root.size(),::std::byte{0xffu});qualified.assign(root.size(),::std::byte{});
        ::fast_io::freestanding::my_memcpy(memory.data()+32u,"uwvm",4u);
        types[0u].name=::fast_io::concat_std("char");types[0u].encoding=6u;
        encode(root,0u,32u,width);memory[36u]=::std::byte{};calls=0u;
        check(source_language_expression::value(types,1u,{},root,{},width,read,out)==source_dwarf::inline_query_error::none &&
            out[0u].display_text_available && out[0u].display_text=="uwvm" && !out[0u].display_text_truncated && calls==5u,"bounded C character-pointer string");
        calls=0u;
        check(source_language_expression::value(types,1u,{},root,mask,width,read,out,{},qualified)==source_dwarf::inline_query_error::none &&
            !out[0u].display_text_available && calls==0u,"unknown character pointer cannot trigger display reads");
    }
    ::fast_io::io::println("debug_source_language_sequences: PASS");
}
