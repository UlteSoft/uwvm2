// Finite literal boundary DATA; no live guest/frame or host address authority.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
namespace scalar = uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf = uwvm2::uwvm::debugger::source_dwarf;
static ::std::size_t checks{}, reads{}, queries{};
static void check(bool ok)
{ ++checks; if(!ok) { ::fast_io::io::perrln("Float range FAIL"); ::fast_io::fast_terminate(); } }
static bool values(dwarf::source_expression const&,bool,scalar::integer&)
{ ++reads; return false; }
static bool types(dwarf::source_expression const&,bool,scalar::integer&)
{ ++queries; return false; }
static auto parse_value(::std::string_view text, scalar::program& code, scalar::integer& value, unsigned guest)
{
    check(scalar::parse("1",code)==scalar::error::none);
    auto const status{scalar::parse(text,code)};
    if(status!=scalar::error::none) { check(code.nodes.empty()); return status; }
    check(scalar::evaluate(code,values,value,guest,types)==scalar::error::none);
    check(reads==0u && queries==0u);
    return status;
}
int main(int argc,char const* const* argv)
{
    if(argc>1)
    {
        for(int i{1};i<argc;++i)
        {
            auto const text{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])}};
            check(text.size()<=256u);
            for(unsigned guest:{32u,64u})
            {
                scalar::program code{}; scalar::integer value{};
                auto const status{parse_value({text.data(),text.size()},code,value,guest)};
                ::fast_io::io::println(i-1,"\t",guest,"\t",status==scalar::error::none ? 1u : 0u,
                    "\t",value.bits,"\t",value.width,"\t",value.floating ? 1u : 0u,"\t",reads,"\t",queries);
            }
        }
        return 0;
    }
    struct sample { ::std::string_view text; bool accepted; ::std::uint64_t bits; unsigned width; };
    // The literal compiler independently produces the finite value bits.
    auto const max32{::std::bit_cast<::std::uint32_t>(0x1.fffffep127f)};
    auto const small32{::std::bit_cast<::std::uint32_t>(0x1p-149f)};
    auto const max64{::std::bit_cast<::std::uint64_t>(0x1.fffffffffffffp1023)};
    sample const cases[]{
        {"3.4028235e38f",true,max32,32u},
        {"340282356779733661637539395458142568447.0f",true,max32,32u},
        {"340282356779733661637539395458142568448.0f",false,0u,0u},
        {"340282356779733661637539395458142568449.0F",false,0u,0u},
        {"3.40282356779733661637539395458142568447e38F",true,max32,32u},
        {"3.40282356779733661637539395458142568448e38f",false,0u,0u},
        {"3.40282356779733661637539395458142568449e38f",false,0u,0u},
        {"3.5e38f",false,0u,0u}, {"1e39F",false,0u,0u},
        {"0 && 3.5e38f",false,0u,0u}, {"1 ? 7.0 : 1e39f",false,0u,0u},
        {"3.402_823_5e3_8f",true,max32,32u},
        {"3.402'823'5e3'8F",true,max32,32u},
        {"1.401298464324817e-45f",true,small32,32u},
        {"1e-999f",false,0u,0u}, {"0.0e999f",true,0u,32u},
        {"1.7976931348623157e308",true,max64,64u},
        {"1.7976931348623159e308",false,0u,0u}, {"1e309",false,0u,0u}
    };
    for(unsigned guest:{32u,64u})
    {
        for(auto const& sample:cases)
        {
            scalar::program code{}; scalar::integer value{};
            auto const status{parse_value(sample.text,code,value,guest)};
            check((status==scalar::error::none)==sample.accepted);
            if(sample.accepted) { check(value.bits==sample.bits && value.width==sample.width && value.floating); }
        }
    }
    ::fast_io::io::println("debug_source_float_range: PASS checks=",checks," owned DATA only");
}
