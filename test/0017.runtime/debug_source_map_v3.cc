#include <fast_io.h>
#include <source_location>
#include <uwvm2/uwvm/debugger/source_map.h>
namespace dbg=uwvm2::uwvm::debugger;
namespace v3=dbg::source_map_v3;
static void check(bool value,::std::source_location where=::std::source_location::current())
{ if(!value) { ::fast_io::io::perrln("FAIL bounded source-map v3 line ",where.line());::fast_io::fast_terminate(); } }
int main(int argc,char** argv)
{
    if(argc==6)
    {
        ::std::uint64_t begin{},size{},file{};
        for(auto pair:{::std::pair{argv[2],&begin},::std::pair{argv[3],&size},::std::pair{argv[4],&file}})
        {
            ::std::string_view text{pair.first};auto const parsed{::fast_io::parse_by_scan(text.data(),text.data()+text.size(),*pair.second)};
            check(parsed.code==::fast_io::parse_code::ok && parsed.iter==text.data()+text.size());
        }
        ::std::string_view name{argv[5]};::std::vector<::std::byte> payload{};auto length{name.size()};
        do { auto c{static_cast<unsigned char>(length&127u)};length>>=7u;if(length) { c|=128u; }payload.push_back(static_cast<::std::byte>(c)); }while(length);
        for(unsigned char c:name) { payload.push_back(static_cast<::std::byte>(c)); }
        ::fast_io::string bytes{},directory{};check(dbg::read_source_map_sidecar(payload,argv[1],bytes,directory));
        v3::image image{};check(v3::parse(v3::view(bytes),begin,size,file,image)==v3::error::none);
        ::fast_io::io::println("sources ",image.sources.size()," rows ",image.rows.size());
        for(auto const& row:image.rows) { ::fast_io::io::println(row.begin," ",row.end," ",row.file," ",row.line," ",row.column); }
        return 0;
    }
    check(argc==1);
    dbg::source_map_sections sections{};sections.source_map_declared=true;sections.code_section_file_offset=10u;
    sections.code_section_content_size=40u;sections.module_file_size=100u;sections.source_map_directory="/tmp/maps";
    sections.source_map_json=R"({"version":3,"sources":["probe.ts"],"names":[],"sourceRoot":"./src","mappings":"oBAAA,KACC,E,IACD","sourcesContent":["ignored source text"],"unknown":{"a":[true,false,null,-1.2e+3]}})";
    dbg::source_map map{};check(dbg::source_map::parse(4u,sections,map)==dbg::source_map_error::none && map.range_count()==3u);
    check(!map.lookup(4u,9u) && !map.lookup(3u,10u) && !map.lookup(4u,40u));
    auto first{map.lookup(4u,10u)};check(first && first->file=="/tmp/maps/src/probe.ts" && first->line==1u && first->column==1u && first->is_statement);
    auto second{map.lookup(4u,16u)};check(second && second->line==2u && second->column==2u);
    check(!map.lookup(4u,17u) && !map.lookup(4u,20u));
    auto third{map.lookup(4u,21u)};check(third && third->line==3u && third->column==1u);
    for(auto text:{
        R"({"version":3,"sources":["p.ts"],"mappings":"DAAA"})",
        R"({"version":3,"sources":["p.ts"],"mappings":"oBCAAA"})",
        R"({"version":3,"sources":["p.ts"],"mappings":"oBAAA,"})",
        R"({"version":3,"sources":["p.ts"],"mappings":"oBAAA;AAAA"})",
        R"({"version":3,"sources":["p.ts"],"mappings":"oBAAAA"})",
        R"({"version":3,"sources":["p.ts"],"mappings":"ggggggggggggQAAA"})",
        R"({"version":3,"sources":["p.ts"],"mappings":"oBAAA","version":3})",
        R"({"version":3,"sources":["\ud800"],"mappings":"oBAAA"})",
        R"({"version":3,"sources":["\u0000"],"mappings":"oBAAA"})",
        R"({"version":3,"sources":["p.ts"],"mappings":"oBAAA","sections":[]})",
        R"({"version":3,"sources":["p.ts"],"mappings":"oBAAA"}garbage)",
        R"({"version":3,"sources":["p.ts"],"mappings":"oBAAA","x":[1,]})"})
    {
        sections.source_map_json=text;check(dbg::source_map::parse(4u,sections,map)!=dbg::source_map_error::none);
        check(map.range_count()==3u && map.lookup(4u,10u)->file=="/tmp/maps/src/probe.ts");
    }
    v3::image unicode{};
    check(v3::parse(R"({"version":3,"sources":["caf\u00e9\ud83d\ude00.ts"],"mappings":"oBAAA,AAEC"})",10u,40u,100u,unicode)==v3::error::none);
    check(unicode.rows.size()==1u && unicode.rows[0].line==3u && unicode.rows[0].column==2u && v3::view(unicode.sources[0])=="caf\xc3\xa9\xf0\x9f\x98\x80.ts");
    check(v3::parse("{}",101u,0u,100u,unicode)==v3::error::malformed && unicode.rows.size()==1u);
    ::fast_io::string bytes{},directory{};
    for(auto name:{"../p.map","https://p.map","p%2emap","p.map?x",".",".."})
    {
        ::std::string_view value{name};::std::vector<::std::byte> payload{static_cast<::std::byte>(value.size())};
        for(unsigned char c:value) { payload.push_back(static_cast<::std::byte>(c)); }
        check(!dbg::read_source_map_sidecar(payload,"/tmp/unused.wasm",bytes,directory) && bytes.empty());
    }
    ::fast_io::io::println("PASS bounded JSON/UTF-8/VLQ coordinates, gaps, duplicate points, atomic failure and sidecar URL rejection");
}
