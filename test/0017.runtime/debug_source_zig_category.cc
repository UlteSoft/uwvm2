// Copied scalar/type DATA only; no producer, real VM stop or memory authority.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
namespace scalar = uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf = uwvm2::uwvm::debugger::source_dwarf;
static ::std::size_t checks{},reads{},queries{};
static void check(bool ok,::std::string_view why)
{
    ++checks;if(!ok) { ::fast_io::io::perrln("Zig category FAIL: ",why);::fast_io::fast_terminate(); }
}
#ifdef UWVM_ZIG_CATEGORY_BASELINE
int main(int argc,char const* const* argv)
{
    auto const mode{argc>1 ? ::fast_io::string_view{::fast_io::mnp::os_c_str(argv[1])} : ::fast_io::string_view{}};
    check(mode=="widen" || mode=="nested" || mode=="bool","baseline selector");
    auto const value{[&](dwarf::source_expression const&,bool,scalar::integer& out){++reads;out={1u,mode=="bool" ? 32u : 8u,true};return true;}};
    auto const type{[&](dwarf::source_expression const&,bool,scalar::integer& out){++queries;out={0u,mode=="bool" ? 32u : 8u,true};return true;}};
    auto const text{mode=="widen" ? "@as(i16, cell)" : mode=="nested" ? "@as(i16, @as(u8, 255))" : "@as(u64, cell)"};
    scalar::program code{};scalar::integer out{};check(scalar::parse(text,code)==scalar::error::none,"baseline syntax");
    auto const status{scalar::evaluate(code,value,out,32u,type)};
    if(mode=="bool") { check(status==scalar::error::unsupported && reads==0u,"DWARF boolean must not become an integer"); }
    else { check(status==scalar::error::none,mode=="widen" ? "typed u8 widening is representable" : "nested eight-bit literal widening is representable"); }
}
#else
using category = scalar::value_category;
static dwarf::type_record declaration{};
static ::std::uint64_t copied_bits{};
enum class fault { none, boolean, unknown, opposite_integer, missing, invalid_category, floating_category };
static fault value_fault{},type_fault{};
static scalar::integer change(scalar::integer out,fault which)
{
    switch(which)
    {
        case fault::boolean:out.category=category::boolean;break;
        case fault::unknown:out.category=category::unspecified;break;
        case fault::opposite_integer:out.unsigned_value=!out.unsigned_value;break;
        case fault::invalid_category:out.category=static_cast<category>(255u);break;
        case fault::floating_category:out.floating=true;out.category=category::integer;break;
        default:break;
    }
    return out;
}
static bool values(dwarf::source_expression const& leaf,bool size,scalar::integer& out)
{
    ++reads;
    if(value_fault==fault::missing || size || leaf.root_name!="cell" || !leaf.steps.empty()) { return false; }
    out=scalar::from_dwarf_numeric(dwarf::value_details::classify(declaration),copied_bits,
        static_cast<unsigned>(declaration.byte_count)*8u);
    out=change(out,value_fault);return true;
}
static bool types(dwarf::source_expression const& leaf,bool size,scalar::integer& out)
{
    ++queries;
    if(type_fault==fault::missing || size || leaf.root_name!="cell" || !leaf.steps.empty()) { return false; }
    ::std::vector<dwarf::object_node> layout{};
    if(uwvm2::uwvm::debugger::source_language_expression::type({::std::addressof(declaration),1u},0u,leaf.steps,4u,layout)!=
        dwarf::inline_query_error::none || layout.size()!=1u ||
        layout[0u].reason!=dwarf::object_unavailable_reason::none) { return false; }
    out=scalar::from_dwarf_numeric(layout[0u].scalar_kind,0x12345678u,static_cast<unsigned>(layout[0u].scalar_bytes)*8u);
    out=change(out,type_fault);return true;
}
static void declare(unsigned encoding,unsigned bytes,::std::uint64_t bits)
{
    declaration={};declaration.kind=dwarf::type_kind::scalar;declaration.encoding=encoding;
    declaration.byte_count=static_cast<::std::uint8_t>(bytes);declaration.byte_size=bytes;declaration.size_known=true;
    copied_bits=bits;type_fault=value_fault=fault::none;
}
static ::std::string_view view(::fast_io::string const& text) { return {text.data(),text.size()}; }
static scalar::integer run(::std::string_view text,scalar::error expected,::std::size_t count,unsigned guest)
{
    scalar::program code{};check(scalar::parse(text,code)==scalar::error::none,"finite syntax");
    auto const before{reads};scalar::integer out{123u,64u,true};
    auto const status{scalar::evaluate(code,values,out,guest,types)};
    if(status!=expected) { ::fast_io::io::perrln(text," status=",static_cast<unsigned>(status)," expected=",static_cast<unsigned>(expected)); }
    check(status==expected,"classification and inclusion result");
    check(reads-before==count,"type proof precedes reads and dead values stay unused");
    if(expected!=scalar::error::none)
    { check(out.bits==0u && out.width==32u && !out.unsigned_value && !out.floating && out.category==category::unspecified,"refusal publishes no value"); }
    return out;
}
static void same(scalar::integer got,::std::uint64_t bits,unsigned width,bool uns,category kind)
{
    check(got.bits==bits && got.width==width && got.unsigned_value==uns && !got.floating && got.category==kind,"value and exact copied result category");
}
static ::std::uint64_t expected_bits(unsigned value,bool uns,unsigned width)
{
    // Independent signed byte interpretation and destination byte reduction.
    ::std::int64_t const number{uns || value<128u ? static_cast<::std::int64_t>(value) : static_cast<::std::int64_t>(value)-256};
    auto const bits{static_cast<::std::uint64_t>(number)};
    return width==64u ? bits : bits % (::std::uint64_t{1u}<<width);
}
int main()
{
    struct destination { ::std::string_view name;unsigned width;bool uns; };
    destination const destinations[]{{"i8",8u,false},{"u8",8u,true},{"i16",16u,false},{"u16",16u,true},
        {"i32",32u,false},{"u32",32u,true},{"i64",64u,false},{"u64",64u,true},{"isize",0u,false},{"usize",0u,true}};
    for(unsigned guest:{32u,64u})
    {
        for(unsigned encoding:{5u,6u,7u,8u})
        {
            bool const uns{encoding==7u || encoding==8u};
            for(unsigned value{};value<256u;++value)
            {
                declare(encoding,1u,value);
                for(auto const& to:destinations)
                {
                    unsigned const width{to.width ? to.width : guest};
                    // Widening of the whole declared type, never value-based narrowing.
                    bool const safe{to.uns ? uns : !uns || width>8u};
                    auto const text{::fast_io::concat_fast_io("@as(",to.name,", cell)")};
                    auto const status{safe ? scalar::error::none : scalar::error::unsupported};
                    auto out{run(view(text),status,safe ? 1u : 0u,guest)};
                    if(safe) { same(out,expected_bits(value,uns,width),width,to.uns,category::integer); }
                    auto const dead{::fast_io::concat_fast_io("1 ? 7 : ",text)};
                    out=run(view(dead),status,0u,guest);
                    if(safe) { same(out,7u,width<32u ? 32u : width,to.uns && width>=32u,category::unspecified); }
                    auto const selected{::fast_io::concat_fast_io("0 ? 9 : ",text)};
                    out=run(view(selected),status,safe ? 1u : 0u,guest);
                    if(safe) { same(out,expected_bits(value,uns,width<32u ? 32u : width),width<32u ? 32u : width,to.uns && width>=32u,category::unspecified); }
                }
            }
        }
        for(unsigned bytes:{1u,2u,4u,8u})
        {
            declare(2u,bytes,1u);
            for(auto const& to:destinations)
            {
                auto const text{::fast_io::concat_fast_io("@as(",to.name,", cell)")};
                run(view(text),scalar::error::unsupported,0u,guest);
                auto const dead{::fast_io::concat_fast_io("1 ? 7 : ",text)};
                run(view(dead),scalar::error::unsupported,0u,guest);
            }
        }
        for(unsigned bytes:{1u,2u,4u,8u})
        {
            for(unsigned bits:{0u,1u,2u,255u})
            {
                declare(2u,bytes,bits);
                same(run("+cell",scalar::error::none,1u,guest),bits!=0u,32u,false,category::integer);
                same(run("cell + 1",scalar::error::none,1u,guest),(bits!=0u)+1u,32u,false,category::unspecified);
                same(run("1 ? +cell : 7",scalar::error::none,1u,guest),bits!=0u,32u,false,category::unspecified);
            }
        }
        declare(7u,1u,255u);
        for(auto drift:{fault::boolean,fault::unknown,fault::opposite_integer,fault::missing,fault::invalid_category,fault::floating_category})
        {
            value_fault=drift;
            for(auto text:{"@as(i16, cell)","@as(i64, @as(u8, cell))","1 ? @as(u64, cell) : 7"})
            { run(text,scalar::error::unavailable,1u,guest); }
        }
        value_fault=fault::none;
        type_fault=fault::unknown;run("@as(i16, cell)",scalar::error::unsupported,0u,guest);
        type_fault=fault::missing;run("@as(i16, cell)",scalar::error::unavailable,0u,guest);
        type_fault=fault::invalid_category;run("@as(i16, cell)",scalar::error::unavailable,0u,guest);
        type_fault=fault::floating_category;run("@as(i16, cell)",scalar::error::unavailable,0u,guest);
        type_fault=fault::none;
        for(auto text:{"@as(i16, cell + 1)","@as(i16, (u8)cell)","@as(i16, -cell)","@as(i32, true)"})
        { run(text,scalar::error::unsupported,0u,guest); }
        same(run("@as(i16, @as(i8, -128))",scalar::error::none,0u,guest),65408u,16u,false,category::integer);
        same(run("@as(i64, @as(u8, 255))",scalar::error::none,0u,guest),255u,64u,false,category::integer);
        same(run("1 ? @as(i16, @as(i8, -128)) : 0",scalar::error::none,0u,guest),4294967168u,32u,false,category::unspecified);
        same(run("1 ? 7 : @as(i16, @as(u8, 255))",scalar::error::none,0u,guest),7u,32u,false,category::unspecified);
        run("@as(u64, @as(i8, -1))",scalar::error::unsupported,0u,guest);
        run("1 ? 7 : @as(u64, @as(i8, -1))",scalar::error::unsupported,0u,guest);
        auto const before_queries{queries};
        same(run("1 || @as(i16, missing)",scalar::error::none,0u,guest),1u,32u,false,category::unspecified);
        same(run("0 && @as(i16, missing)",scalar::error::none,0u,guest),0u,32u,false,category::unspecified);
        check(queries==before_queries,"short circuit obtains no declaration");
        same(run("(int)cell + 1",scalar::error::none,1u,guest),256u,32u,false,category::unspecified);
        same(run("1 ? +cell : 7",scalar::error::none,1u,guest),255u,32u,false,category::unspecified);
        same(run("1 ? (u16)cell : 7",scalar::error::none,1u,guest),255u,32u,false,category::unspecified);
        scalar::program code{};check(scalar::parse("@as(i16, cell)",code)==scalar::error::none,"legacy typed callback syntax");
        scalar::integer out{};
        check(scalar::evaluate(code,values,out,guest)==scalar::error::none,"classified value-only callback remains finite DATA");
        same(out,255u,16u,false,category::integer);
        auto const two{[](dwarf::source_expression const& leaf,scalar::integer& v){return types(leaf,false,v);}};
        check(scalar::evaluate(code,values,out,guest,two)==scalar::error::none,"two-argument classification metadata");
        same(out,255u,16u,false,category::integer);
    }
    for(auto kind:{dwarf::numeric_kind::signed_integer,dwarf::numeric_kind::unsigned_integer,dwarf::numeric_kind::boolean,
        dwarf::numeric_kind::f32_bits,dwarf::numeric_kind::f64_bits,dwarf::numeric_kind::unavailable})
    {
        for(unsigned width:{0u,1u,2u,3u,4u,8u,16u,24u,32u,64u,128u})
        {
            bool const integer_width{width==8u || width==16u || width==32u || width==64u};
            bool const valid{kind==dwarf::numeric_kind::f32_bits ? width==32u :
                kind==dwarf::numeric_kind::f64_bits ? width==64u : kind!=dwarf::numeric_kind::unavailable && integer_width};
            auto const packed{scalar::from_dwarf_numeric(kind,123u,width)};
            check(valid ? packed.width==width && packed.bits==123u : packed.width==0u && packed.bits==0u,
                "DWARF category/storage pairing checked before numeric publication");
        }
    }
    check(queries>0u,"actual metadata type traversal exercised");
    ::fast_io::io::println("debug_source_zig_category: PASS checks=",checks," owned DATA only");
}
#endif
