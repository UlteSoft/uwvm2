// Independent C++ arithmetic oracles over finite copied DATA; this does not
// qualify a source PC, producer DWARF or a product guest transaction.
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
#include <bit>
#include <cstdint>
#include <string_view>
namespace scalar = ::uwvm2::uwvm::debugger::source_scalar_expression;
static void check(bool ok, ::std::string_view label)
{ if(!ok) { ::fast_io::io::perrln("debug_source_scalar_property: ",label); ::fast_io::fast_terminate(); } }
int main()
{
    ::std::uint64_t state{0x2026100412345678ull};
    auto next{[&]() noexcept { state ^= state << 13u; state ^= state >> 7u; state ^= state << 17u; return static_cast<::std::uint32_t>(state); }};
    for(unsigned sample{}; sample != 10000u; ++sample)
    {
        auto const a{next()}, b{next() | 1u}; auto const count{next() & 31u};
        auto resolver{[&](auto const& leaf,bool size,scalar::integer& value)
        {
            if(size) { return false; }
            if(leaf.root_name=="a") { value={a,32u,true}; return true; }
            if(leaf.root_name=="b") { value={b,32u,true}; return true; }
            if(leaf.root_name=="count") { value={count,32u,true}; return true; }
            return false;
        }};
        auto run{[&](::std::string_view text,::std::uint32_t expected,bool boolean=false)
        {
            scalar::program program{};scalar::integer out{};
            check(scalar::parse(text,program)==scalar::error::none,"bounded syntax");
            check(scalar::evaluate(program,resolver,out)==scalar::error::none,"owned evaluation");
            check(!out.floating && out.bits==expected && (boolean || (out.width==32u && out.unsigned_value)),text);
        }};
        run("a + b",a+b);run("a - b",a-b);run("a * b",a*b);run("a / b",a/b);run("a % b",a%b);
        run("a << count",a<<count);run("a >> count",a>>count);run("a < b",a<b,true);run("a >= b",a>=b,true);
        run("(a ^ b) & a",(a^b)&a);run("0 && absent",0u,true);run("1 || absent",1u,true);
        float const first{static_cast<float>(static_cast<::std::int32_t>(next()%2000001u)-1000000)/8.0f};
        float const second{static_cast<float>(1u+next()%1000000u)/16.0f};
        auto real_resolver{[&](auto const& leaf,bool size,scalar::integer& value)
        {
            if(size) { return false; }
            if(leaf.root_name=="first") { value={::std::bit_cast<::std::uint32_t>(first),32u,false,true}; return true; }
            if(leaf.root_name=="second") { value={::std::bit_cast<::std::uint32_t>(second),32u,false,true}; return true; }
            return false;
        }};
        auto real{[&](::std::string_view text,float expected)
        {
            scalar::program program{};scalar::integer out{};
            check(scalar::parse(text,program)==scalar::error::none,"floating syntax");
            check(scalar::evaluate(program,real_resolver,out)==scalar::error::none,"floating owned evaluation");
            check(out.floating && out.width==32u && out.bits==::std::bit_cast<::std::uint32_t>(expected),text);
        }};
        real("first + second",first+second);real("first - second",first-second);real("first * second",first*second);
        real("first / second",first/second);real("-first",-first);
    }
    ::fast_io::io::println("debug_source_scalar_property: PASS 170000 independent integer/IEEE-f32 checks");
}
