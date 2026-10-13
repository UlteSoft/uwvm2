#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <uwvm2/uwvm/debugger/command.h>
using namespace uwvm2::uwvm::debugger;
namespace scalar = source_scalar_expression;
static void check(bool ok, char const* reason)
{ if(!ok) { ::fast_io::io::perrln("source scalar FAIL: ",::fast_io::mnp::os_c_str(reason)); ::fast_io::fast_terminate(); } }
int main()
{
    ::std::size_t reads{};
    auto const resolve{[&](source_dwarf::source_expression const& leaf,bool size,scalar::integer& value)
    { ++reads; if(leaf.root_name == "shadow") { value = {size ? 4u : 7u,size ? 64u : 32u,size}; return true; }
      if(leaf.root_name == "packet" && size) { value = {28u,64u,true}; return true; } return false; }};
    auto const verify{[&](::std::string_view expression,::std::uint64_t expected)
    { scalar::program code{}; scalar::integer value{};
      check(scalar::parse(expression,code)==scalar::error::none,"syntax");
      check(scalar::evaluate(code,resolve,value)==scalar::error::none && value.bits==expected,"result"); }};
    verify("0xff & 15",15u); verify("0xffffffff + 1",0u); verify("true && !false",1u);
    verify("0b101010 + 0o12 + 010",60u); verify("0B1111 & 0XFF",15u);
    verify("1'000 + 1_000",2000u); verify("0xffff'ffff + 1",0u); verify("0x_2a + 0b_10 + 0o_10",52u);
    verify("0b1111_0000 &^ 0b0011_0000",192u);
    verify("255 &^ 3 * 4",1008u); // Go &^ shares the multiplicative precedence group.
    for(auto expression : {"0b", "0o", "0b102", "09", "0o8", "0x__1", "1__0", "1''0", "123_", "0b1'", "0x10000000000000000"})
    { scalar::program code{}; check(scalar::parse(expression,code)!=scalar::error::none,"malformed base/separator/overflow rejected"); }
    verify("shadow + 2 * 3",13u); verify("(shadow + 2) * 3",27u);
    verify("sizeof(packet) + sizeof(shadow)",32u);
    verify("shadow == 7 && (shadow << 1) == 14",1u);
    verify("-7 / 2",0xfffffffdu); verify("-7 % 2",0xffffffffu);
    verify("~0 & 255",255u); verify("1 << 4 + 1",32u);
    auto before{reads}; verify("0 && missing",0u); verify("1 || missing",1u); check(reads==before,"short circuit invokes no resolver");
    for(auto expression : {"2147483647 + 1","9223372036854775807 + 1","-2147483647 - 2","1 / 0","1 << 32","2 * 2147483647","1 << -1"})
    { scalar::program code{}; scalar::integer value{}; check(scalar::parse(expression,code)==scalar::error::none,"bad arithmetic syntax");
      check(scalar::evaluate(code,resolve,value)==scalar::error::arithmetic,"invalid arithmetic fails without UB"); }
    for(auto expression : {"shadow = 9","run()","&shadow","(int*)shadow","shadow++"})
    { scalar::program code{}; check(scalar::parse(expression,code)!=scalar::error::none,"side effect/call/pointer cast rejected"); }
    auto const command{parse_console_command(::fast_io::string_view{"print 1 2 shadow + 2"})};
    check(command.kind==console_command_kind::source_value && command.requested_step_thread==1u && command.disassembly_stop_identifier==2u &&
        ::std::string_view{command.source_variable_name.data(),command.source_variable_name_size}=="shadow + 2","complete spaced expression with authenticated selectors");
    auto const plain{parse_console_command(::fast_io::string_view{"print shadow + 2"})};
    check(plain.kind==console_command_kind::source_value && ::std::string_view{plain.source_variable_name.data(),plain.source_variable_name_size}=="shadow + 2","plain spaced expression");
    for(auto text : {"print 1 2 1 || missing", "print 1 2 0 && missing", "print 1 2 1 + 2", "print 1 2 1 / 0"})
    {
        auto const parsed{parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)})};
        check(parsed.kind==console_command_kind::source_value && !parsed.source_frame_explicit &&
              parsed.requested_step_thread==1u && parsed.disassembly_stop_identifier==2u,"numeric expression prefix is not a frame");
        scalar::program code{};
        check(scalar::parse({parsed.source_variable_name.data(),parsed.source_variable_name_size},code)==scalar::error::none,"numeric expression retained completely");
    }
    auto const framed{parse_console_command(::fast_io::string_view{"print 1 2 3 shadow + 2"})};
    check(framed.source_frame_explicit && framed.source_frame_ordinal==3u &&
          ::std::string_view{framed.source_variable_name.data(),framed.source_variable_name_size}=="shadow + 2","legacy frame prefix retained");
    // Actual f32 rounding, mixed arithmetic and signed zero truth values.
    auto const numeric_resolve{[](source_dwarf::source_expression const& leaf,bool size,scalar::integer& value)
    {
        if(size) { return false; }
        if(leaf.root_name == "real32") { value = {::std::bit_cast<::std::uint32_t>(3.5f),32u,false,true}; return true; }
        if(leaf.root_name == "real64") { value = {::std::bit_cast<::std::uint64_t>(-2.5),64u,false,true}; return true; }
        if(leaf.root_name == "nan") { value = {::std::bit_cast<::std::uint64_t>(::std::numeric_limits<double>::quiet_NaN()),64u,false,true}; return true; }
        return false;
    }};
    auto const numeric{[&](::std::string_view text,unsigned guest_bits = 32u)
    {
        scalar::program code{}; scalar::integer value{};
        check(scalar::parse(text,code)==scalar::error::none,"numeric syntax");
        check(scalar::evaluate(code,numeric_resolve,value,guest_bits)==scalar::error::none,"numeric evaluation"); return value;
    }};
    auto const real{[&](::std::string_view text,double expected,unsigned width)
    { auto value{numeric(text)}; check(value.floating && value.width==width && scalar::details::floating_number(value)==expected,"floating precision/value"); }};
    real("real32 + 2",5.5,32u); real("real32 + real64",1.0,64u);
    real("1.25e2 / .5",250.0,64u); real("16777216.0f + 1",16777216.0,32u);
    real("static_cast<double>(real32) + 0.25",3.75,64u);
    real("real32 as f64",3.5,64u); real("float64(real32)",3.5,64u);
    check(numeric("!-0.0").bits==1u && numeric("!nan").bits==0u,"IEEE truth including signed zero/NaN");
    check(numeric("nan != nan").bits==1u && numeric("nan == nan").bits==0u,"IEEE NaN comparisons");
    check(numeric("(int)real32").bits==3u && numeric("int32(real64)").bits==0xfffffffeu,"float truncation toward zero");
    check(numeric("(unsigned int)-0.5").bits==0u,"truncate before unsigned conversion bounds");
    check(numeric("-1 as u8").bits==255u && numeric("(unsigned char)257").bits==1u,"owned integer narrowing");
    check(numeric("(bool)real64").bits==1u,"numeric boolean conversion");
    check(numeric("sizeof(long)",32u).bits==4u && numeric("sizeof(long)",64u).bits==8u,"guest ABI type size");
    check(numeric("sizeof(double)").bits==8u && numeric("sizeof(short)").bits==2u,"builtin type size");
    check(numeric("'A' + '\\n'").bits==75u,"ASCII character escape");
    check(numeric("4294967295u + 1u").bits==0u,"unsigned suffix wrapping");
    check(numeric("1L",32u).width==32u && numeric("1L",64u).width==64u,"long suffix uses actual guest ABI");
    check(numeric("1UL",64u).width==64u && numeric("1UL",64u).unsigned_value,"unsigned long guest width");
    check(numeric("2147483648L",32u).width==64u && !numeric("2147483648L",32u).unsigned_value,"decimal long promotes without negative reinterpretation");
    { scalar::program code{};scalar::integer value{};
      check(scalar::parse("9223372036854775808LL",code)==scalar::error::none,"bounded decimal long long syntax");
      check(scalar::evaluate(code,numeric_resolve,value)==scalar::error::arithmetic,"unrepresentable signed suffix never becomes negative"); }

    for(auto text : {"(int)2147483648.0","(unsigned int)-1.0","(unsigned long long)18446744073709551616.0","(long long)9223372036854775808.0","(int)nan","(int)(1.0 / 0.0)"})
    {
        scalar::program code{};scalar::integer value{};check(scalar::parse(text,code)==scalar::error::none,"out-of-range float cast syntax");
        check(scalar::evaluate(code,numeric_resolve,value)==scalar::error::arithmetic,"float casts never overflow host integers");
    }
    for(auto text : {"real32 & 1","~real64","real32 << 1","real64 % 2"})
    { scalar::program code{};scalar::integer value{};check(scalar::parse(text,code)==scalar::error::none,"invalid floating operator syntax");
      check(scalar::evaluate(code,numeric_resolve,value)==scalar::error::unsupported,"floating bit operations rejected"); }
    for(auto text : {"1e", "1.0ff", "1e999", "(int*)real32", "static_cast<void*>(real32)", "'ab'", "1uu", "1lL"})
    { scalar::program code{};check(scalar::parse(text,code)!=scalar::error::none,"malformed numeric/pointer syntax rejected"); }
    for(auto text : {"enable", "disable", "enable 1", "disable 2", "ignore 3 0", "ignore 3 18446744073709551615"})
    { auto const command{parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)})};
      check(command.kind==console_command_kind::breakpoint_control && command.operation==::uwvm2::utils::control::operation::status && command.payload_size==0u,
            "breakpoint policy uses bounded authenticated host status admission"); }
    for(auto text : {"enable 0", "disable 0", "ignore", "ignore 0 1", "ignore 1 -1", "ignore 1 18446744073709551616", "enable 1 2"})
    { auto const command{parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)})};
      check(command.kind==console_command_kind::invalid,"malformed breakpoint policy rejected before mutation"); }
    ::fast_io::io::println("debug_source_scalar_expression: PASS");
}
