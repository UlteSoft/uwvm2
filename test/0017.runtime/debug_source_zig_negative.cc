// Finite negative-literal syntax/type/value DATA, without VM or guest authority.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <uwvm2/uwvm/debugger/command.h>
namespace scalar = uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf = uwvm2::uwvm::debugger::source_dwarf;
static ::std::size_t checks{},reads{},queries{};
static void check(bool ok,::std::string_view reason)
{
    ++checks;
    if(!ok) { ::fast_io::io::perrln("Zig negative FAIL: ",reason);::fast_io::fast_terminate(); }
}
static ::std::string_view view(::fast_io::string const& text) { return {text.data(),text.size()}; }
static ::std::uint64_t mask(unsigned width) { return width == 64u ? ~::std::uint64_t{} : (::std::uint64_t{1u}<<width)-1u; }
static bool values(dwarf::source_expression const&,bool,scalar::integer&) { ++reads;return false; }
static bool types(dwarf::source_expression const&,bool,scalar::integer&) { ++queries;return false; }
static scalar::integer run(::std::string_view text,scalar::error expected,unsigned guest,::std::string_view reason)
{
    scalar::program code{};check(scalar::parse(text,code)==scalar::error::none,"bounded literal grammar");
    auto const r{reads},q{queries};scalar::integer out{123u,64u,true};
    auto const actual{scalar::evaluate(code,values,out,guest,types)};
    if(actual!=expected) { ::fast_io::io::perrln("expression: ",text,", actual status: ",static_cast<unsigned>(actual)); }
    check(actual==expected,reason);
    check(reads==r && queries==q,"owned literal must not query values or metadata");
    if(expected!=scalar::error::none)
    { check(out.bits==0u && out.width==32u && !out.unsigned_value && !out.floating,"error publishes no value"); }
    return out;
}
static void same(scalar::integer got,scalar::integer expected)
{
    check(got.bits==expected.bits && got.width==expected.width && got.unsigned_value==expected.unsigned_value &&
        got.floating==expected.floating,"result bits and category");
}
static ::fast_io::string magnitude(::std::uint64_t number,unsigned style)
{
    if(style==1u) { return ::fast_io::concat_fast_io("0x",::fast_io::mnp::hex(number)); }
    if(style==2u) { return ::fast_io::concat_fast_io("0b_",::fast_io::mnp::bin(number)); }
    if(style==3u) { return ::fast_io::concat_fast_io("0o",::fast_io::mnp::oct(number)); }
    return ::fast_io::concat_fast_io(number);
}
int main(int argc,char const* const* argv)
{
    auto const mode{argc>1 ? ::fast_io::string_view{::fast_io::mnp::os_c_str(argv[1])} : ::fast_io::string_view{}};
    bool const all{mode.empty()};
    check(all || mode=="min" || mode=="wide" || mode=="float" || mode=="dead","known baseline selector");
    if(all || mode=="min")
    { same(run("@as(i8, -128)",scalar::error::none,32u,"negative signed minimum is representable"),{128u,8u,false}); }
    if(all || mode=="wide")
    { same(run("@as(i64, -0xffffffff)",scalar::error::none,32u,"negative hex-width magnitude must not wrap at 32 bits"),{0xffffffff00000001ull,64u,false}); }
    if(all || mode=="float")
    { same(run("@as(f32, -0.0)",scalar::error::none,32u,"floating negative zero is preserved"),{0x80000000u,32u,false,true}); }
    if(all || mode=="dead")
    { run("1 ? 7 : @as(i8, -129)",scalar::error::arithmetic,32u,"dead negative literal still requires destination bounds"); }
    if(!all) { return 0; }
    struct destination { ::std::string_view name;unsigned width;bool uns; };
    destination const destinations[]{
        {"i8",8u,false},{"u8",8u,true},{"i16",16u,false},{"u16",16u,true},
        {"i32",32u,false},{"u32",32u,true},{"i64",64u,false},{"u64",64u,true},
        {"isize",0u,false},{"usize",0u,true}
    };
    ::std::vector<::std::uint64_t> samples{
        0u,1u,2u,127u,128u,129u,255u,256u,257u,32767u,32768u,32769u,
        65535u,65536u,65537u,2147483647u,2147483648u,2147483649u,4294967295u,4294967296ull,4294967297ull,
        0x7fffffffffffffffull,0x8000000000000000ull,0x8000000000000001ull,0xfffffffffffffffeull,0xffffffffffffffffull
    };
    ::std::uint64_t state{0xd51a32f14796acb5ull};
    for(unsigned i{};i!=32u;++i)
    { state=state*6364136223846793005ull+1442695040888963407ull;samples.push_back(i%2u ? state : state%65538u); }
    for(unsigned guest : {32u,64u})
    {
        for(auto const& to : destinations)
        {
            auto const width{to.width ? to.width : guest};
            for(auto number : samples)
            {
                // Independent representability oracle: a nonzero negative
                // number cannot fit an unsigned destination. Signed endpoints
                // use its mathematical magnitude, not C literal candidates.
                bool const safe{to.uns ? number==0u : number <= (::std::uint64_t{1u}<<(width-1u))};
                auto const status{safe ? scalar::error::none : scalar::error::arithmetic};
                for(unsigned style{};style!=4u;++style)
                {
                    auto const digits{magnitude(number,style)};
                    auto const expr{::fast_io::concat_fast_io("@as(",to.name,", - (",digits,"))")};
                    auto got{run(view(expr),status,guest,"negative representability boundary")};
                    if(safe) { same(got,{(0u-number)&mask(width),width,to.uns}); }
                    auto const dead{::fast_io::concat_fast_io("1 ? 7 : ",expr)};
                    got=run(view(dead),status,guest,"unselected negative literal has identical bounds");
                    auto const common{width<32u ? 32u : width};bool const common_unsigned{to.uns && width>=32u};
                    if(safe) { same(got,{7u,common,common_unsigned}); }
                    auto const selected{::fast_io::concat_fast_io("0 ? 7 : ",expr)};
                    got=run(view(selected),status,guest,"selected negative literal has identical bounds");
                    if(safe) { same(got,{(0u-number)&mask(common),common,common_unsigned}); }
                }
            }
        }
        for(unsigned number{};number!=257u;++number)
        {
            auto const expr{::fast_io::concat_fast_io("@as(i8, -",number,")")};
            auto got{run(view(expr),number<=128u ? scalar::error::none : scalar::error::arithmetic,guest,"all narrow negative magnitudes")};
            if(number<=128u) { same(got,{(0u-static_cast<::std::uint64_t>(number))&255u,8u,false}); }
        }
        same(run("@as(i64, -9223372036854775808)",scalar::error::none,guest,"signed 64-bit minimum"),{0x8000000000000000ull,64u,false});
        same(run("@as(i64, -0x8000_0000_0000_0000)",scalar::error::none,guest,"hex signed 64-bit minimum"),{0x8000000000000000ull,64u,false});
        same(run("@as(i64, @as(i16, -32768))",scalar::error::none,guest,"nested negative literal widening"),{0xffffffffffff8000ull,64u,false});
        for(auto text : {"@as(u64, -1)","@as(i64, -9223372036854775809)","@as(i64, -18446744073709551615)",
                         "1 ? 7 : @as(i64, -18446744073709551615)","@as(f32, -1e100)","1 ? 7 : @as(f32, -1e100)"})
        { run(text,scalar::error::arithmetic,guest,"integer and floating endpoints are checked"); }
        for(auto text : {"@as(i64, -value)","@as(i64, -(value + 1))","@as(i64, -(1+2))","@as(i64, - ( -1))",
                         "@as(i64, +1)","@as(i64, -1u)","@as(i64, -1LL)","@as(i64, -(int)1)",
                         "@as(i32, -true)","@as(f64, -1.0f)","@as(i64, -@as(i32, 1))",
                         "1 ? 7 : @as(i64, -(1+2))"})
        { run(text,scalar::error::unsupported,guest,"no compound CTFE, unary plus or typed-literal evidence"); }
        same(run("@as(i8, -'A')",scalar::error::none,guest,"owned ASCII code point is a constant"),{191u,8u,false});
        same(run("@as(i8, -' ')",scalar::error::none,guest,"owned ASCII whitespace"),{224u,8u,false});
        same(run("@as(i8, -'\\n')",scalar::error::none,guest,"owned escaped code point"),{246u,8u,false});
        same(run("@as(u8, -'\\0')",scalar::error::none,guest,"owned zero code point"),{0u,8u,true});
        struct real { ::std::string_view text;float f32;double f64; };
        real const reals[]{
            {"0.0",-0.0f,-0.0},{"1.25",-1.25f,-1.25},{"1e-40",-1e-40f,-1e-40},
            {"3.4028234663852886e38",-3.4028234663852886e38f,-3.4028234663852886e38}
        };
        for(auto const& real : reals)
        {
            auto const a{::fast_io::concat_fast_io("@as(f32, -",real.text,")")};
            auto const b{::fast_io::concat_fast_io("@as(f64, -",real.text,")")};
            same(run(view(a),scalar::error::none,guest,"negative float coercion"),{::std::bit_cast<::std::uint32_t>(real.f32),32u,false,true});
            same(run(view(b),scalar::error::none,guest,"negative double coercion"),{::std::bit_cast<::std::uint64_t>(real.f64),64u,false,true});
            auto const selected{::fast_io::concat_fast_io("0 ? 7 : ",a)};
            same(run(view(selected),scalar::error::none,guest,"selected negative float coercion"),{::std::bit_cast<::std::uint32_t>(real.f32),32u,false,true});
        }
        same(run("1 || @as(i8, -129)",scalar::error::none,guest,"logical short circuit keeps its existing laziness"),{1u,32u,false});
        same(run("0 && @as(i64, -missing)",scalar::error::none,guest,"logical short circuit reads no metadata"),{0u,32u,false});
        scalar::program code{};check(scalar::parse("@as(i64, -9223372036854775808)",code)==scalar::error::none,"legacy negative grammar");
        scalar::integer out{};check(scalar::evaluate(code,values,out,guest)==scalar::error::none,"value-only API gets owned constant support");
        same(out,{0x8000000000000000ull,64u,false});
        scalar::details::no_type_resolver const absent{};
        check(scalar::evaluate(code,values,out,guest,absent)==scalar::error::none,"const absent type resolver");
        same(out,{0x8000000000000000ull,64u,false});
        // Outside @as the shared C-style unary/literal rules remain intact.
        same(run("-0xffffffff",scalar::error::none,guest,"ordinary C unsigned negation"),{1u,32u,true});
        same(run("-9223372036854775808",scalar::error::none,guest,"ordinary C literal candidates"),{0x8000000000000000ull,64u,true});
    }
    for(auto expression : {"@as(i8, -128)","@as(i64, -9223372036854775808)","@as(f32, -0.0)"})
    {
        auto const line{::fast_io::concat_fast_io("print-frame 1 41 2 ",::fast_io::mnp::os_c_str(expression))};
        auto const cmd{uwvm2::uwvm::debugger::parse_console_command(::fast_io::string_view{line.data(),line.size()})};
        auto const input{::fast_io::string_view{::fast_io::mnp::os_c_str(expression)}};
        check(cmd.kind==uwvm2::uwvm::debugger::console_command_kind::source_value && cmd.source_frame_explicit &&
              cmd.requested_step_thread==1u && cmd.disassembly_stop_identifier==41u && cmd.source_frame_ordinal==2u &&
              ::std::string_view{cmd.source_variable_name.data(),cmd.source_variable_name_size}==::std::string_view{input.data(),input.size()},
              "console retains authenticated selectors and full negative expression");
    }
    check(reads==0u && queries==0u,"literal qualification has no frame, stop or memory authority");
    ::fast_io::io::println("debug_source_zig_negative: PASS checks=",checks," owned DATA only");
}
