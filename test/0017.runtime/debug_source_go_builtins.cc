#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
using namespace uwvm2::uwvm::debugger;
static ::std::size_t checks{};
static void require(bool ok, char const* message)
{
    ++checks;
    if(!ok) { ::fast_io::io::perrln("Go builtin failure: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static void put(::std::vector<::std::byte>& bytes, ::std::size_t at, ::std::uint64_t value, unsigned width)
{
    ::fast_io::basic_obuffer_view<char> sink{reinterpret_cast<char*>(bytes.data()+at),reinterpret_cast<char*>(bytes.data()+at+width)};
    if(width == 4u) { ::fast_io::io::print(sink,::fast_io::mnp::le_put<32u>(value)); }
    else { ::fast_io::io::print(sink,::fast_io::mnp::le_put<64u>(value)); }
}
int main()
{
    for(::std::uint8_t width : {4u,8u})
    {
        ::std::vector<source_dwarf::type_record> types(4u);
        auto& byte{types[0u]}; byte.kind=source_dwarf::type_kind::scalar;byte.encoding=7u;byte.size_known=true;byte.byte_size=byte.byte_count=1u;
        auto& pointer{types[1u]};pointer.kind=source_dwarf::type_kind::pointer;pointer.size_known=true;pointer.byte_size=pointer.byte_count=width;pointer.referenced_type=0u;
        auto& field{types[2u]};field.kind=source_dwarf::type_kind::scalar;field.encoding=7u;field.size_known=true;field.byte_size=field.byte_count=width;
        auto& slice{types[3u]};slice.kind=source_dwarf::type_kind::structure;slice.language=0x0cu;slice.tinygo_producer=true;
        slice.name=::fast_io::concat_std("[]uint8");slice.size_known=true;slice.byte_size=3u*width;
        source_dwarf::member_record p{},l{},c{};p.name=::fast_io::concat_std("ptr");p.type=1u;p.offset_known=true;
        l.name=::fast_io::concat_std("len");l.type=2u;l.offset_known=true;l.byte_offset=width;
        c=l;c.name=::fast_io::concat_std("cap");c.byte_offset=2u*width;slice.members={p,l,c};
        ::std::vector<::std::byte> bytes(3u*width),known(bytes.size(),::std::byte{0xffu});
        put(bytes,width,3u,width);put(bytes,2u*width,5u,width);
        ::std::size_t reads{};
        auto read=[&](auto,auto,auto&){++reads;return false;};
        auto query=[&](char const* text, bool type=false)
        {
            source_dwarf::source_expression parsed{};require(source_dwarf::parse_source_expression(::std::string_view{text},parsed)==source_dwarf::object_selector_error::none,"builtin syntax");
            ::std::vector<source_dwarf::object_node> out{};
            auto status=type ? source_language_expression::type(types,3u,parsed.steps,width,out) :
                source_language_expression::value(types,3u,parsed.steps,bytes,known,width,read,out);
            require(reads==0u,"length/capacity followed data pointer");return ::std::pair{status,out};
        };
        auto expect=[&](char const* text,::std::uint64_t value)
        {
            auto [status,out]=query(text);require(status==source_dwarf::inline_query_error::none && out.size()==1u &&
                out[0u].value_available && out[0u].bits==value && out[0u].scalar_kind==source_dwarf::numeric_kind::signed_integer &&
                out[0u].scalar_bytes==width,"Go int result");
        };
        expect("len(s)",3u);expect("cap(s)",5u);
        // Both the pointer bits and an independent descriptor field may be
        // genuinely absent. Only the requested known scalar supplies a value.
        for(unsigned i{};i!=width;++i) { known[i]=::std::byte{};known[2u*width+i]=::std::byte{}; }
        expect("len(s)",3u);require(query("cap(s)").first==source_dwarf::inline_query_error::unavailable,"missing cap fabricated");
        for(unsigned i{};i!=width;++i) { known[width+i]=::std::byte{};known[2u*width+i]=::std::byte{0xffu}; }
        expect("cap(s)",5u);require(query("len(s)").first==source_dwarf::inline_query_error::unavailable,"missing len fabricated");
        auto [status,layout]=query("len(s)",true);require(status==source_dwarf::inline_query_error::none && layout.size()==1u &&
            !layout[0u].value_available && layout[0u].scalar_bytes==width,"type query copied missing descriptor value");
        known.assign(bytes.size(),::std::byte{0xffu});put(bytes,2u*width,2u,width);
        require(query("len(s)").first==source_dwarf::inline_query_error::unavailable,"len greater than cap");
        require(query("cap(s)").first==source_dwarf::inline_query_error::unavailable,"cap less than len");
        put(bytes,2u*width,5u,width);
        auto maximum=width==4u ? 0x7fffffffull : 0x7fffffffffffffffull;
        put(bytes,width,maximum+1u,width);require(query("len(s)").first==source_dwarf::inline_query_error::unavailable,"Go int overflow");
        put(bytes,width,0u,width);put(bytes,2u*width,0u,width);expect("len(s)",0u);expect("cap(s)",0u);
        // Scalar arithmetic and unevaluated size use the same owned reference
        // plans. The model resolver below has no runtime/source authority.
        put(bytes,width,3u,width);put(bytes,2u*width,5u,width);
        auto resolve=[&](source_dwarf::source_expression const& expr,bool size,source_scalar_expression::integer& out)
        {
            ::std::vector<source_dwarf::object_node> nodes{};
            auto status=size ? source_language_expression::type(types,3u,expr.steps,width,nodes) :
                source_language_expression::value(types,3u,expr.steps,bytes,known,width,read,nodes);
            if(status!=source_dwarf::inline_query_error::none || nodes.size()!=1u) { return false; }
            if(size) { out={nodes[0u].byte_size,unsigned(width)*8u,true};return true; }
            out=source_scalar_expression::from_dwarf_numeric(nodes[0u].scalar_kind,nodes[0u].bits,unsigned(width)*8u);return nodes[0u].value_available;
        };
        for(auto text : {::std::string_view{"len(s) + cap(s)"},::std::string_view{"sizeof(len(s))"},::std::string_view{"len(s)-1"},::std::string_view{"(cap(s))-1"}})
        {
            source_scalar_expression::program code{};source_scalar_expression::integer value{};
            require(source_scalar_expression::parse(text,code)==source_scalar_expression::error::none,"scalar builtin parse");
            require(source_scalar_expression::evaluate(code,resolve,value,unsigned(width)*8u)==source_scalar_expression::error::none &&
                value.bits==(text.starts_with("sizeof") ? width : text=="len(s)-1" ? 2u : text=="(cap(s))-1" ? 4u : 8u),"scalar builtin evaluation");
        }
        slice.language=0x1cu;slice.tinygo_producer=false;require(query("len(s)").first==source_dwarf::inline_query_error::unavailable,"Rust lookalike acquired Go builtin");
        slice.language=0x0cu;require(query("len(s)").first==source_dwarf::inline_query_error::unavailable,"C lookalike acquired Go builtin");
        slice.language=0x16u;expect("len(s)",3u);
        slice.name=::fast_io::concat_std("string");slice.members={p,l};slice.byte_size=2u*width;bytes.resize(2u*width);known.resize(bytes.size());
        put(bytes,width,7u,width);expect("len(s)",7u);require(query("cap(s)").first==source_dwarf::inline_query_error::unavailable,"cap(string) accepted");
        require(query("cap(s)",true).first==source_dwarf::inline_query_error::unavailable,"cap(string) type accepted");
        l.byte_offset=0u;slice.members={p,l};require(query("len(s)").first==source_dwarf::inline_query_error::unavailable,"malformed descriptor offset accepted");
    }
    for(auto text : {"len()","len(s,t)","len(s+1)","cap(len(s))","len(s)[0]","len(s).x","len(call())","len(s=1)","*len(s)"})
    {
        source_dwarf::source_expression parsed{};require(source_dwarf::parse_source_expression(text,parsed)!=source_dwarf::object_selector_error::none,"unsupported builtin argument accepted");
    }
    source_dwarf::source_expression parsed{};
    require(source_dwarf::parse_source_expression("len(((box.Numbers)))",parsed)==source_dwarf::object_selector_error::none &&
        parsed.root_name=="box" && parsed.steps.size()==2u,"member path builtin");
    require(source_dwarf::parse_source_expression("length",parsed)==source_dwarf::object_selector_error::none && parsed.root_name=="length","keyword prefix changed root");
    for(auto text : {"len ","cap ","len .x","cap .x"})
    {
        require(source_dwarf::parse_source_expression(text,parsed)==source_dwarf::object_selector_error::none &&
            parsed.root_name==(text[0]=='l' ? "len" : "cap"),"builtin lookahead swallowed root whitespace");
    }
    ::fast_io::io::println("debug_source_go_builtins: PASS ",checks," assertions");
}
