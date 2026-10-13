// Finite source expression/type/value DATA; no producer or VM stop authority.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>

namespace scalar = uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf = uwvm2::uwvm::debugger::source_dwarf;
static ::std::size_t checks{},reads{},type_queries{};
static void check(bool ok, ::std::string_view reason)
{
    ++checks;
    if(!ok) { ::fast_io::io::perrln("Zig type proof FAIL: ",reason); ::fast_io::fast_terminate(); }
}
struct source { ::std::string_view name; scalar::integer value; };
static source const sources[]{
    {"value",{7u,32u,false}}, {"negative",{0xfffffff9u,32u,false}},
    {"signed16",{0xfff9u,16u,false}}, {"unsigned16",{65535u,16u,true}},
    {"unsigned32",{0xffffffffu,32u,true}}, {"wide",{7u,64u,false}},
    {"unsigned64",{0xfffffffffffffff9ull,64u,true}},
    {"decimal",{::std::bit_cast<::std::uint32_t>(1.25f),32u,false,true}},
    {"decimal64",{::std::bit_cast<::std::uint64_t>(1.25),64u,false,true}},
    {"bool_carrier",{1u,8u,true}}, {"byte_carrier",{7u,8u,false}},
    {"dead32",{0u,32u,false}}
};
static bool values(dwarf::source_expression const& leaf,bool size,scalar::integer& out)
{
    ++reads;
    if(size || !leaf.steps.empty() || leaf.root_name == "dead32") { return false; }
    for(auto const& item : sources)
    { if(item.name == leaf.root_name) { out = item.value; return true; } }
    return false;
}
static bool types(dwarf::source_expression const& leaf,bool size,scalar::integer& out)
{
    ++type_queries;
    if(size || !leaf.steps.empty()) { return false; }
    for(auto const& item : sources)
    {
        if(item.name == leaf.root_name)
        { out = item.value; out.bits = 0x12345678u; return true; } // deliberately not a value carrier
    }
    return false;
}
static scalar::error evaluate(::std::string_view text,scalar::integer& out,unsigned guest)
{
    scalar::program code{};
    check(scalar::parse(text,code) == scalar::error::none,"finite syntax");
    return scalar::evaluate(code,values,out,guest,types);
}
static void refuse(::std::string_view text,scalar::error expected,unsigned guest,::std::string_view reason)
{
    scalar::integer out{123u,64u,true};auto const before{reads};
    check(evaluate(text,out,guest) == expected,reason);
    check(out.bits == 0u && reads == before,"type/shape refusal occurs without value access");
}
static void accept(::std::string_view text,scalar::integer expected,unsigned guest,::std::size_t count)
{
    scalar::integer out{};auto const before{reads};
    check(evaluate(text,out,guest) == scalar::error::none,"supported coercion/branch evaluation");
    check(out.bits == expected.bits && out.width == expected.width && out.unsigned_value == expected.unsigned_value &&
          out.floating == expected.floating && reads-before == count,"expected value/type and reachable reads");
}
int main(int argc,char const* const* argv)
{
    auto const mode{argc > 1 ? ::fast_io::string_view{::fast_io::mnp::os_c_str(argv[1])} : ::fast_io::string_view{}};
    bool const all{mode.empty()};
    check(all || mode == "type" || mode == "constant" || mode == "read","known old-source selector");
    if(all || mode == "type") { refuse("1 ? 7 : @as(i16, value)",scalar::error::unsupported,32u,"dead arm requires a safe source type"); }
    if(all || mode == "constant") { refuse("1 ? 7 : @as(u8, 256)",scalar::error::arithmetic,32u,"dead arm requires a representable literal"); }
    if(all || mode == "read") { refuse("@as(i64, value + 2)",scalar::error::unsupported,32u,"compound source must not read values"); }
    if(!all) { return 0; }
    struct destination { ::std::string_view name; unsigned width; bool uns{},floating{}; };
    destination const destinations[]{
        {"i8",8u},{"u8",8u,true},{"i16",16u},{"u16",16u,true},
        {"i32",32u},{"u32",32u,true},{"i64",64u},{"u64",64u,true},
        {"isize",0u},{"usize",0u,true},{"f32",32u,false,true},{"f64",64u,false,true}
    };
    for(unsigned guest : {32u,64u})
    {
        for(auto const& from : sources)
        {
            for(auto const& to : destinations)
            {
                auto const width{to.width ? to.width : guest};
                // Independent inclusion relation for the documented finite
                // runtime coercion subset. No value-dependent narrowing.
                bool const supported{from.value.width != 8u &&
                    (to.floating ? from.value.floating && width >= from.value.width :
                     !from.value.floating && (to.uns ? from.value.unsigned_value && width >= from.value.width :
                      from.value.unsigned_value ? width > from.value.width : width >= from.value.width))};
                auto const expression{::fast_io::concat_std("@as(",to.name,", ",from.name,")")};
                for(auto const& text : {::fast_io::concat_std("1 ? 7 : ",expression),
                                        ::fast_io::concat_std("0 ? ",expression," : 7"),
                                        ::fast_io::concat_std("1 ? 7 : (0 ? ",expression," : 9)")})
                {
                    if(!supported) { refuse(text,scalar::error::unsupported,guest,"unsupported dead-arm conversion is checked"); continue; }
                    scalar::integer result{7u,width < 32u ? 32u : width,to.uns && width >= 32u,to.floating};
                    if(to.floating) { result.bits = width == 32u ? ::std::bit_cast<::std::uint32_t>(7.0f) : ::std::bit_cast<::std::uint64_t>(7.0); }
                    accept(text,result,guest,0u);
                }
                if(supported && from.name != "dead32")
                {
                    auto result{from.value};result.width = width;result.unsigned_value = to.uns;
                    if(to.floating)
                    { result.bits = width == 32u ? ::std::bit_cast<::std::uint32_t>(1.25f) : ::std::bit_cast<::std::uint64_t>(1.25); }
                    else if(!from.value.unsigned_value && from.name == "negative")
                    { result.bits = width == 64u ? 0xfffffffffffffff9ull : 0xfffffff9u; }
                    else if(!from.value.unsigned_value && from.name == "signed16")
                    { result.bits = width == 64u ? 0xfffffffffffffff9ull : width == 32u ? 0xfffffff9u : 0xfff9u; }
                    accept(expression,result,guest,1u);
                }
            }
        }
        for(auto text : {"@as(i64, value + 2)","@as(i32, !value)","@as(i32, value > 1)",
                        "@as(i64, (int)value)","@as(f64, (double)decimal)","@as(i64, -value)",
                        "@as(i32, true)","@as(i64, @as(i32, value + 2))"})
        { refuse(text,scalar::error::unsupported,guest,"unsupported source shape checked before its value"); }
        for(auto text : {"1 ? 7 : @as(u8, 256)","1 ? 7 : @as(i8, 128)","0 ? @as(i16, 32768) : 7",
                        "1 ? 7 : @as(u16, 65536)","1 ? 7 : @as(i32, 2147483648)",
                        "1 ? 7 : @as(u32, 4294967296)","1 ? 7 : @as(i64, 9223372036854775808)",
                        "1 ? 7 : @as(f32, 1e100)","1 ? 7 : (int)@as(u8, 256)"})
        { refuse(text,scalar::error::arithmetic,guest,"owned constant bounds checked in the dead arm"); }
        for(auto text : {"1 ? 7 : @as(i64, @as(i16, value))","1 ? 7 : @as(f64, @as(f32, decimal64))",
                        "1 ? 7 : (int)@as(u32, value)","1 ? 7 : @as(i64, @as(i32, bool_carrier))"})
        { refuse(text,scalar::error::unsupported,guest,"nested unsupported coercion cannot hide behind a cast"); }
        accept("1 ? 7 : @as(u8, 255)",{7u,32u,false},guest,0u);
        accept("1 ? 7 : @as(i8, 127)",{7u,32u,false},guest,0u);
        accept("1 ? 7 : @as(i16, 32767)",{7u,32u,false},guest,0u);
        accept("1 ? 7 : @as(u16, 65535)",{7u,32u,false},guest,0u);
        accept("1 ? 7 : @as(u32, 4294967295)",{7u,32u,true},guest,0u);
        accept("1 ? 7 : @as(u64, 18446744073709551615)",{7u,64u,true},guest,0u);
        accept("0 ? 9 : @as(i64, value)",{7u,64u,false},guest,1u);
        accept("1 ? @as(i64, value) : 9",{7u,64u,false},guest,1u);
        accept("1 ? 7 : @as(i64, @as(i32, dead32))",{7u,64u,false},guest,0u);
        accept("1 || @as(i64, missing)",{1u,32u,false},guest,0u);
        accept("0 && @as(i64, missing)",{0u,32u,false},guest,0u);
    }
    check(type_queries > 0u,"type DATA callbacks exercised");
    ::fast_io::io::println("debug_source_zig_typeproof: PASS checks=",checks," owned DATA only");
}
