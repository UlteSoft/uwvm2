// Native compiler literal oracle and owned scalar syntax DATA; no VM read authority.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
namespace scalar = uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf = uwvm2::uwvm::debugger::source_dwarf;
static ::std::size_t checks{};
static void check(bool ok,::std::string_view why)
{ ++checks;if(!ok) { ::fast_io::io::perrln("Float separator FAIL: ",why);::fast_io::fast_terminate(); } }
#ifdef UWVM_FLOAT_SEPARATOR_BASELINE
int main(int argc,char const* const* argv)
{
    check(argc==2,"baseline selector");
    auto const text{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[1])}};
    scalar::program code{};
    check(scalar::parse({text.data(),text.size()},code)==scalar::error::none,"decimal separators accepted");
}
#else
static ::std::size_t reads{},queries{};
static ::std::string_view view(::fast_io::string const& s) { return {s.data(),s.size()}; }
static bool values(dwarf::source_expression const&,bool,scalar::integer&)
{ ++reads;return false; }
static bool types(dwarf::source_expression const&,bool,scalar::integer&)
{ ++queries;return false; }
static void verify(::std::string_view text,::std::uint64_t bits,unsigned width,bool floating,unsigned guest)
{
    auto const r{reads},q{queries};scalar::program code{};scalar::integer out{};
    auto const parsed{scalar::parse(text,code)};
    if(parsed!=scalar::error::none) { ::fast_io::io::perrln(text," parse=",static_cast<unsigned>(parsed)); }
    check(parsed==scalar::error::none,"valid literal/expression syntax");
    check(scalar::evaluate(code,values,out,guest,types)==scalar::error::none,"literal/expression result");
    if(out.bits!=bits || out.width!=width) { ::fast_io::io::perrln(text," bits=",out.bits," expected=",bits); }
    check(out.bits==bits && out.width==width && out.floating==floating && !out.unsigned_value,
          "actual compiler IEEE bits and numeric type");
    check(reads==r && queries==q,"literal/dead arithmetic grants no resolver reads");
}
static ::fast_io::string separated(::std::string_view original,::std::uint64_t& state)
{
    char data[256u]{};::std::size_t size{};
    for(::std::size_t i{};i!=original.size();++i)
    {
        if(i && original[i]>='0' && original[i]<='9' && original[i-1u]>='0' && original[i-1u]<='9')
        {
            state^=state<<13u;state^=state>>7u;state^=state<<17u;
            auto const kind{state%3u};if(kind) { data[size++]=kind==1u ? '\'' : '_'; }
        }
        check(size<sizeof(data),"bounded normalized input carrier");data[size++]=original[i];
    }
    return ::fast_io::concat_fast_io(::std::string_view{data,size});
}
static ::std::uint64_t bits32(float value) { return ::std::bit_cast<::std::uint32_t>(value); }
static ::std::uint64_t bits64(double value) { return ::std::bit_cast<::std::uint64_t>(value); }
int main()
{
    struct sample { ::std::string_view text;::std::uint64_t bits;unsigned width; };
    // Apostrophes in these native C++ constants are handled by the compiler,
    // independently of both the production parser and the fast_io converter.
    sample const samples[]{
        {"1234.5678",bits64(1'234.5'678),64u},{"1234.5678e10",bits64(1'234.5'678e1'0),64u},
        {".123456789e+02",bits64(.1'234'567'89e+0'2),64u},{"12345.e-02",bits64(12'345.e-0'2),64u},
        {"9007199254740991.0",bits64(9'007'199'254'740'991.0),64u},
        {"1.7976931348623157e308",bits64(1.797'693'134'862'315'7e3'08),64u},
        {"2.2250738585072014e-308",bits64(2.225'073'858'507'201'4e-3'08),64u},
        {"4.9406564584124654e-324",bits64(4.940'656'458'412'465'4e-3'24),64u},
        {"1234.5678f",bits32(1'234.5'678f),32u},{"1234.5678e10F",bits32(1'234.5'678e1'0F),32u},
        {".123456789e+02f",bits32(.1'234'567'89e+0'2f),32u},{"12345.e-02F",bits32(12'345.e-0'2F),32u},
        {"16777217.0f",bits32(16'777'217.0f),32u},
        {"3.4028234663852886e38f",bits32(3.402'823'466'385'288'6e3'8f),32u},
        {"1.1754943508222875e-38f",bits32(1.175'494'350'822'287'5e-3'8f),32u},
        {"1.401298464324817e-45f",bits32(1.401'298'464'324'817e-4'5f),32u}
    };
    ::std::uint64_t state{0x3141592653589793ull};
    for(unsigned guest:{32u,64u})
    {
        for(auto const& sample:samples)
        {
            auto const native32{sample.width==32u ? ::std::bit_cast<float>(static_cast<::std::uint32_t>(sample.bits)) : 0.0f};
            auto const native64{sample.width==64u ? ::std::bit_cast<double>(sample.bits) : static_cast<double>(native32)};
            for(unsigned variant{};variant!=512u;++variant)
            {
                auto const text{separated(sample.text,state)};
                verify(view(text),sample.bits,sample.width,true,guest);
                auto const negative{::fast_io::concat_fast_io("-(",text,")")};
                verify(view(negative),sample.width==32u ? bits32(-native32) : bits64(-native64),sample.width,true,guest);
                auto const sum{::fast_io::concat_fast_io("(",text,") + ",sample.width==32u ? ::fast_io::string_view{"0.5f"} : ::fast_io::string_view{"0.5"})};
                verify(view(sum),sample.width==32u ? bits32(native32+0.5f) : bits64(native64+0.5),sample.width,true,guest);
                auto const selected{::fast_io::concat_fast_io("1 ? ",text," : 7.0")};
                verify(view(selected),bits64(native64),64u,true,guest);
                auto const dead{::fast_io::concat_fast_io("1 ? 7.0 : ",text)};
                verify(view(dead),bits64(7.0),64u,true,guest);
                auto const shorted{::fast_io::concat_fast_io("0 && (",text," + missing)")};
                verify(view(shorted),0u,32u,false,guest);
            }
        }
        for(auto text:{"1_.5","1._5","1.5_e1","1.5e_1","1.5e1_","1.5e+_1","1.5e-_1",
                      "1__2.5","1''2.5","1'_2.5","1_'2.5","1'.5","1.'5","1.5'e1",
                      "1.5e'1","1.5e1'","1.5e+'1",".1__2",".1''2","1.2f_","1.2f'",
                      "1_2e","1_2e+","1_2e-","1_2.3_4e9_9_9","1.2ff","0x1.2","1.2L"})
        {
            auto const literal{::fast_io::string_view{::fast_io::mnp::os_c_str(text)}};
            for(auto const& wrapped:{::fast_io::concat_fast_io(literal),::fast_io::concat_fast_io("0 && (",literal,")"),
                                    ::fast_io::concat_fast_io("1 ? 7.0 : ",literal)})
            {
                scalar::program code{};check(scalar::parse("1",code)==scalar::error::none,"seed old output");
                auto const r{reads},q{queries};
                check(scalar::parse(view(wrapped),code)!=scalar::error::none && code.nodes.empty(),"bad separator/token rejected even in dead syntax");
                check(reads==r && queries==q,"rejected syntax has no resolver authority");
            }
        }
        verify("1'000.5",bits64(1'000.5),64u,true,guest);
        verify("1_000.5",bits64(1'000.5),64u,true,guest);
        verify(".1_25f",bits32(.1'25f),32u,true,guest);
        verify("0.15e+0_2",bits64(0.15e+0'2),64u,true,guest);
        // Existing integer bases/prefix separators and character literals survive.
        for(auto const& item: {::std::pair<::std::string_view,::std::uint64_t>{"0b_10 + 0o_10 + 0x_2a",52u},
                             {"0_77",63u},{"1'000 + 1_000",2000u},{"'_'",95u},{"'e'",101u}})
        { verify(item.first,item.second,32u,false,guest); }
    }
    ::fast_io::io::println("debug_source_float_separator: PASS checks=",checks," owned DATA only");
}
#endif
