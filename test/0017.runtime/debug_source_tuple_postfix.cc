// Production expression/selector regression. Owned DATA only, no stop authority.
#include <uwvm2/uwvm/debugger/source_language_expression.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#endif
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger;
namespace dwarf = source_dwarf;
static void check(bool value, char const* text)
{ if(!value) { ::fast_io::io::perrln("tuple/postfix FAIL: ",::fast_io::mnp::os_c_str(text)); ::fast_io::fast_terminate(); } }
static dwarf::type_record integer(bool uns = false)
{ dwarf::type_record t{}; t.kind=dwarf::type_kind::scalar; t.size_known=true; t.byte_size=4u; t.byte_count=4u; t.encoding=uns ? 7u : 5u; return t; }
static dwarf::member_record member(char const* name, ::std::size_t type, ::std::uint64_t offset)
{ dwarf::member_record m{}; m.name=::fast_io::concat_std(::fast_io::mnp::os_c_str(name)); m.type=type; m.byte_offset=offset; m.offset_known=true; return m; }
static dwarf::source_expression parse(::std::string_view text)
{ dwarf::source_expression p{}; check(dwarf::parse_source_expression(text,p)==dwarf::object_selector_error::none,"parse"); return p; }
static void copied_tuple(::std::span<dwarf::type_record const> types, ::std::size_t root)
{
    auto const& t=types[root]; check(t.size_known && t.byte_size<=65536u,"bounded tuple layout");
    ::std::vector<::std::byte> bytes(static_cast<::std::size_t>(t.byte_size));
    for(auto const& m:t.members)
    {
        if(m.name!="__0" && m.name!="__1") { continue; }
        check(m.offset_known && m.type<types.size() && types[m.type].byte_size==4u && m.byte_offset<=bytes.size() &&
            bytes.size()-m.byte_offset>=4u,"actual producer tuple member extent");
        auto const bits{m.name=="__0" ? 0xfffffff7u : 42u};
        for(unsigned i{};i!=4u;++i) { bytes[static_cast<::std::size_t>(m.byte_offset)+i]=static_cast<::std::byte>(bits>>(8u*i)); }
    }
    ::std::vector<dwarf::object_node> out{};
    auto const no_read=[](auto,auto,auto&)->bool { check(false,"tuple requires no memory callback");return false; };
    for(auto expression:{"pair.0","pair.1"})
    {
        auto const p=parse(expression);
        check(source_language_expression::value(types,root,p.steps,bytes,{},4u,no_read,out)==dwarf::inline_query_error::none &&
            out.size()==1u && out[0u].value_available && out[0u].bits==(p.steps[0u].member=="0" ? 0xfffffff7u : 42u),"numeric Rust selector follows producer offset");
        check(source_language_expression::type(types,root,p.steps,4u,out)==dwarf::inline_query_error::none && out.size()==1u &&
            out[0u].byte_size==4u,"numeric selector type");
    }
    source_scalar_expression::program program{}; source_scalar_expression::integer result{};
    check(source_scalar_expression::parse("pair.0 + pair.1",program)==source_scalar_expression::error::none,"tuple arithmetic parse");
    auto const resolve=[&](dwarf::source_expression const& p,bool size,source_scalar_expression::integer& value)
    {
        if(size || source_language_expression::value(types,root,p.steps,bytes,{},4u,no_read,out)!=dwarf::inline_query_error::none ||
           out.size()!=1u || !out[0u].value_available) { return false; }
        value={out[0u].bits,32u,out[0u].scalar_kind==dwarf::numeric_kind::unsigned_integer};return true;
    };
    check(source_scalar_expression::evaluate(program,resolve,result)==source_scalar_expression::error::none && result.bits==33u,"tuple arithmetic values");
}
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
static void actual_metadata(char const* path, bool zig = false)
{
    ::fast_io::native_file_loader file{::fast_io::mnp::os_c_str(path),::fast_io::open_mode::in};
    auto const bytes=::std::span<::std::byte const>{reinterpret_cast<::std::byte const*>(file.data()),file.size()};
    check(bytes.size()>=8u,"Wasm header");
    ::std::array<unsigned char,8u> const magic{0u,0x61u,0x73u,0x6du,1u,0u,0u,0u};
    for(unsigned i{};i!=8u;++i) { check(::std::to_integer<unsigned char>(bytes[i])==magic[i],"Wasm version1"); }
    dwarf::details::reader input{bytes,8u}; ::std::vector<dwarf::section> sections{}; ::std::uint64_t code_size{};
    while(input.cursor!=bytes.size())
    {
        ::std::uint8_t id{};::std::uint32_t size{};
        check(input.byte(id) && input.leb(size) && size<=bytes.size()-input.cursor,"section bounds");
        auto const payload=bytes.subspan(input.cursor,size);input.cursor+=size;
        if(id==10u) { check(code_size==0u,"one code section");code_size=size; }
        if(id!=0u) { continue; }
        dwarf::details::reader custom{payload};::std::uint32_t length{};
        check(custom.leb(length) && length<=payload.size()-custom.cursor,"custom name bounds");
        ::std::string_view const name{reinterpret_cast<char const*>(payload.data()+custom.cursor),length};custom.cursor+=length;
        if(name.starts_with(".debug_") || name=="external_debug_info") { sections.push_back({name,payload.subspan(custom.cursor)}); }
    }
    ::std::unique_ptr<dwarf::index> parsed{};
    check(code_size && dwarf::index::parse({sections,code_size,4u},parsed)==dwarf::error::none && parsed,"actual Rust DWARF index");
    if(zig)
    {
        unsigned tested{};
        for(auto const& v:parsed->variables())
        {
            if(v.name!="p" || v.type>=parsed->types().size()) { continue; }
            auto const& pointer=parsed->types()[v.type];
            check(pointer.kind==dwarf::type_kind::pointer && pointer.byte_size==4u && !v.locations.empty(),"actual Zig pointer parameter metadata");
            ::std::vector<::std::byte> carrier(4u);carrier[0u]=::std::byte{16u};::std::vector<dwarf::object_node> out{};unsigned reads{};
            auto const read=[&](auto offset,auto count,auto& bytes)
            { ++reads;check(offset==16u && count==4u,"actual Zig pointee layout bounds");bytes={::std::byte{42u},::std::byte{},::std::byte{},::std::byte{}};return true; };
            auto const expression=parse("p.*");
            check(source_language_expression::value(parsed->types(),v.type,expression.steps,carrier,{},4u,read,out)==dwarf::inline_query_error::none &&
                out.size()==1u && out[0u].value_available && out[0u].bits==42u && reads==1u,"actual Zig metadata postfix guest read plan");
            check(source_language_expression::type(parsed->types(),v.type,expression.steps,4u,out)==dwarf::inline_query_error::none &&
                out.size()==1u && out[0u].byte_size==4u,"actual Zig postfix type");++tested;
        }
        check(tested!=0u,"actual Zig parameter retained");
        ::fast_io::io::println("PASS actual Zig Wasm pointer DWARF and postfix owned value; no runtime stop qualification");return;
    }
    bool tuple_seen{},tuple_struct_seen{};
    for(::std::size_t i{};i!=parsed->types().size();++i)
    {
        auto const& t=parsed->types()[i];
        if(t.language!=0x1cu || (t.name!="(i32, u32)" && t.name!="TupleProbe")) { continue; }
        check(t.members.size()==2u && t.members[0u].name=="__0" && t.members[1u].name=="__1","rustc tuple field spelling");
        copied_tuple(parsed->types(),i);
        if(t.name=="TupleProbe") { tuple_struct_seen=true; } else { tuple_seen=true; }
    }
    check(tuple_seen && tuple_struct_seen,"actual tuple and tuple-struct metadata both present");
    ::fast_io::io::println("PASS actual rustc Wasm tuple/tuple-struct DWARF and owned values; no runtime stop qualification");
}

#endif
int main(int argc,char const* const* argv)
{
#if defined(UWVM_USE_LLVM_JIT) || defined(UWVM_USE_DEFAULT_JIT)
    if(argc==2) { actual_metadata(argv[1]);return 0; }
    if(argc==3) { check(::std::string_view{argv[2]}=="zig","metadata producer label");actual_metadata(argv[1],true);return 0; }
#else
    (void)argv;
#endif
    check(argc==1,"arguments");
    ::std::vector<dwarf::type_record> types{integer(),integer(true)};
    dwarf::type_record tuple{};tuple.kind=dwarf::type_kind::structure;tuple.language=0x1cu;tuple.size_known=true;tuple.byte_size=8u;
    tuple.members={member("__0",0u,4u),member("__1",1u,0u)};types.push_back(tuple);copied_tuple(types,2u);
    ::std::vector<::std::byte> bytes(8u);::std::vector<dwarf::object_node> out{};
    auto const query=[&](::std::string_view s) { auto p=parse(s);return dwarf::query_selected_object_type(types,2u,
        ::std::vector<dwarf::object_selector_step>{{dwarf::object_selector_kind::member,p.steps[0u].member,{}}},out); };
    check(query("pair.01")==dwarf::inline_query_error::unavailable && out.empty(),"noncanonical numeric alias refused");
    check(query("pair.2")==dwarf::inline_query_error::unavailable && out.empty(),"missing tuple field refused");
    types[2u].language=0x4u;
    check(query("pair.0")==dwarf::inline_query_error::unavailable && out.empty(),"C++ never aliases Rust field names");
    check(query("pair.__0")==dwarf::inline_query_error::none,"exact producer field still available");
    types[2u].language=0x1cu;types[2u].members.push_back(member("0",0u,4u));
    check(query("pair.0")==dwarf::inline_query_error::ambiguous && out.empty(),"exact/alias collision stays ambiguous");
    types[2u].members.pop_back();types[2u].members.push_back(types[2u].members.front());
    check(query("pair.0")==dwarf::inline_query_error::ambiguous && out.empty(),"duplicate producer field stays ambiguous");types[2u].members.pop_back();
    auto const p=parse("pair.0");::std::vector<::std::byte> known(8u);
    check(source_language_expression::value(types,2u,p.steps,bytes,known,4u,[](auto,auto,auto&){return false;},out)==dwarf::inline_query_error::none &&
        out.size()==1u && !out[0u].value_available,"unknown tuple bits are never invented");
    for(auto width:{4u,8u})
    {
        dwarf::type_record pointer{};pointer.kind=dwarf::type_kind::pointer;pointer.size_known=true;pointer.byte_count=width;pointer.byte_size=width;pointer.referenced_type=0u;
        types.push_back(pointer);auto const root=types.size()-1u;::std::vector<::std::byte> carrier(width);carrier[0u]=::std::byte{16u};unsigned reads{};
        auto const read=[&](auto offset,auto size,auto& copied)
        { ++reads;check(offset==16u && size==4u,"bounded pointer read");copied={::std::byte{42u},::std::byte{},::std::byte{},::std::byte{}};return true; };
        auto const zig=parse("pointer.*");auto const c=parse("*pointer");
        check(zig.steps.size()==1u && zig.steps[0u].kind==c.steps[0u].kind,"postfix/unary same bounded plan");
        check(source_language_expression::value(types,root,zig.steps,carrier,{},width,read,out)==dwarf::inline_query_error::none &&
            out.size()==1u && out[0u].bits==42u && reads==1u,"postfix copied guest value");
        source_scalar_expression::program arithmetic{}; source_scalar_expression::integer answer{};
        check(source_scalar_expression::parse("pointer.* + 1",arithmetic)==source_scalar_expression::error::none,"postfix scalar leaf");
        auto const resolve=[&](dwarf::source_expression const& leaf,bool size,source_scalar_expression::integer& value)
        {
            if(size || source_language_expression::value(types,root,leaf.steps,carrier,{},width,read,out)!=dwarf::inline_query_error::none ||
               out.size()!=1u || !out[0u].value_available) { return false; }
            value={out[0u].bits,32u,false};return true;
        };
        check(source_scalar_expression::evaluate(arithmetic,resolve,answer)==source_scalar_expression::error::none && answer.bits==43u && reads==2u,
            "postfix dereference participates in scalar expression");
        carrier[0u]=::std::byte{};reads=0u;
        check(source_language_expression::value(types,root,zig.steps,carrier,{},width,read,out)==dwarf::inline_query_error::unavailable &&
            out.empty() && reads==0u,"null postfix refuses before callback");
        carrier[0u]=::std::byte{16u};::std::vector<::std::byte> missing(width);
        check(source_language_expression::value(types,root,zig.steps,carrier,missing,width,read,out)==dwarf::inline_query_error::unavailable &&
            out.empty() && reads==0u,"unknown pointer refuses before callback");
    }
    auto const nested=parse("p.*.next.*.value");check(nested.steps.size()==4u,"postfix/member chaining");
    dwarf::source_expression bad{};
    for(auto text:{"p.?","p.*()","p.* = 1"}) { check(dwarf::parse_source_expression(text,bad)!=dwarf::object_selector_error::none && bad.steps.empty(),"unsupported optional/side effects refused"); }
    ::fast_io::io::println("PASS Rust numeric selectors and postfix guest dereference; finite DATA only");
}
