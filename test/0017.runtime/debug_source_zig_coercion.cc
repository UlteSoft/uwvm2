#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <uwvm2/uwvm/debugger/command.h>
namespace scalar = uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf = uwvm2::uwvm::debugger::source_dwarf;
static void check(bool ok)
{ if(!ok) { ::fast_io::io::perrln("FAIL finite Zig source coercion"); ::fast_io::fast_terminate(); } }
int main()
{
    ::std::size_t reads{};
    auto const resolver{[&](dwarf::source_expression const& leaf, bool size, scalar::integer& out)
    {
        ++reads; if(size) { return false; }
        if(leaf.root_name == "value") { out = {7u,32u,false}; return true; }
        if(leaf.root_name == "negative") { out = {0xfffffff9u,32u,false}; return true; }
        if(leaf.root_name == "unsigned32") { out = {0xffffffffu,32u,true}; return true; }
        if(leaf.root_name == "wide") { out = {7u,64u,false}; return true; }
        if(leaf.root_name == "bool_carrier") { out = {1u,8u,true}; return true; }
        if(leaf.root_name == "decimal") { out = {::std::bit_cast<::std::uint32_t>(1.25f),32u,false,true}; return true; }
        if(leaf.root_name == "decimal64") { out = {::std::bit_cast<::std::uint64_t>(1.25),64u,false,true}; return true; }
        return false;
    }};
    auto const evaluate{[&](::std::string_view text, scalar::integer& out, unsigned bits=32u)
    { scalar::program program{}; auto parsed{scalar::parse(text,program)}; return parsed == scalar::error::none ? scalar::evaluate(program,resolver,out,bits) : parsed; }};
    auto const integer{[&](::std::string_view text, ::std::uint64_t bits, unsigned width, bool uns=false, unsigned guest=32u)
    { scalar::integer out{};check(evaluate(text,out,guest)==scalar::error::none && !out.floating && out.bits==bits && out.width==width && out.unsigned_value==uns); }};
    integer("@as(i64, value)",7u,64u);
    integer("@as(i64, negative)",0xfffffffffffffff9ull,64u);
    integer("@as(i64, unsigned32)",0xffffffffu,64u);
    integer("@as(u64, unsigned32)",0xffffffffu,64u,true);
    integer("@as(i32, value)",7u,32u);
    integer("@as(isize, value)",7u,32u);
    integer("@as(isize, value)",7u,64u,false,64u);
    integer("@as(usize, unsigned32)",0xffffffffu,64u,true,64u);
    integer("@as(u8, 255)",255u,8u,true);
    integer("@as(i8, 127)",127u,8u);
    integer("@as(i16, 32767)",32767u,16u);
    integer("@as(i64, value) + 2",9u,64u);
    integer("@as( i64 , @as(i32, value))",7u,64u);
    for(auto text : {"@as(f64, decimal)","@as(f32, 1.25)","@as(f64, decimal64)"})
    {
        scalar::integer out{};check(evaluate(text,out)==scalar::error::none && out.floating && scalar::details::floating_number(out)==1.25);
    }
    for(auto text : {"@as(i16, value)","@as(u32, value)","@as(i32, unsigned32)","@as(i32, wide)",
        "@as(f32, decimal64)","@as(f64, value)","@as(i32, decimal)","@as(i32, true)","@as(i32, bool_carrier)","@as(i32, !true)","@as(i32, value > 1)","@as(i64, value + 2)","@as(u64, unsigned32 + 1)","@as(i64, (int)value)"})
    { scalar::integer out{};check(evaluate(text,out)==scalar::error::unsupported); }
    for(auto text : {"@as(u8, 256)","@as(i8, 128)","@as(i16, 32768)","@as(f32, 1e100)"})
    { scalar::integer out{};check(evaluate(text,out)==scalar::error::arithmetic); }
    for(auto text : {"@as(bool, value)","@as(*i32, value)","@as(double, decimal)","@as(i32,)","@as(i32, value, 2)","@intCast(value)","@as(i64, run())","@as(i64, value = 9)"})
    { scalar::program out{};check(scalar::parse(text,out)!=scalar::error::none); }
    // These exercise the float helper changed for bounded Zig constants,
    // including shared IEEE overflow and values that must retain their bits.
    for(double value : {1e100,-1e100})
    { auto const out{scalar::details::real_value(value,32u)};
      check(out.floating && out.width==32u && ::std::isinf(scalar::details::floating_number(out)) &&
            ::std::signbit(scalar::details::floating_number(out))==::std::signbit(value)); }
    check(scalar::details::real_value(-0.0,32u).bits==0x80000000u);
    check(::std::isnan(scalar::details::floating_number(scalar::details::real_value(::std::numeric_limits<double>::quiet_NaN(),32u))));
    auto const command{uwvm2::uwvm::debugger::parse_console_command(::fast_io::string_view{"print 1 2 @as(i64, value)"})};
    check(command.kind==uwvm2::uwvm::debugger::console_command_kind::source_value &&
          command.requested_step_thread==1u && command.disassembly_stop_identifier==2u &&
          ::std::string_view{command.source_variable_name.data(),command.source_variable_name_size}=="@as(i64, value)");
    auto const before{reads};scalar::integer out{};
    check(evaluate("1 || @as(i64, missing)",out)==scalar::error::none && out.bits==1u && reads==before);
    ::fast_io::io::println("PASS finite Zig @as coercion, checked rejection and short circuit");
}
