// Primitive copied DATA semantics. Live producer/frame qualification is separate.
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
#include <bit>
#include <limits>
namespace s = ::uwvm2::uwvm::debugger::source_scalar_expression;
using kind = s::value_category;
static ::std::uint64_t checks{};
static void check(bool ok, ::std::string_view reason)
{ ++checks; if(!ok) { ::fast_io::io::perrln("predicate FAIL: ",reason); ::fast_io::fast_terminate(); } }
int main()
{
    // The same compiled test can demonstrate the old-header result failure.
    s::program probe{};s::integer answer{};
    auto absent{[](auto const&,bool,s::integer&) { return false; }};
    check(s::parse("2 < 3",probe)==s::error::none,"comparison parse");
    auto const rust{s::language_from_dwarf(0x1cu,false)};
    check(s::evaluate(probe,absent,answer,32u,s::details::no_type_resolver{},rust)==s::error::none &&
          answer.category==kind::boolean && answer.width==8u && answer.bits==1u,"Rust predicate must be Boolean DATA");
#ifndef R48_BASELINE_PROBE
    using lang = s::language_semantics;
    { char text[]{"false & true == false"};s::program owned{};check(s::parse(text,owned)==s::error::none,"owned precedence syntax");text[0]='x';
      check(s::evaluate(owned,absent,answer,32u,s::details::no_type_resolver{},lang::rust)==s::error::none && answer.bits==1u && answer.category==kind::boolean,"native precedence uses owned syntax bytes"); }
    check(rust==lang::rust && s::language_from_dwarf(0x16u,false)==lang::go && s::language_from_dwarf(0x27u,false)==lang::zig,"known CU classification");
    check(s::language_from_dwarf(0x0cu,true)==lang::go && s::language_from_dwarf(0x04u,true)==lang::go,"authenticated TinyGo producer overrides CU language");
    check(s::language_from_dwarf(0u,false)==lang::shared_numeric && s::language_from_dwarf(0xffffu,false)==lang::shared_numeric,"unknown language remains legacy");
    for(auto l:{lang::c,lang::c23,lang::cpp,lang::shared_numeric})
    { check(s::evaluate(probe,absent,answer,32u,s::details::no_type_resolver{},l)==s::error::none && answer.bits==1u &&
            answer.width==(l==lang::cpp ? 8u : 32u),"legacy C/C++/unknown comparison preserved"); }
    for(unsigned guest:{32u,64u}) for(auto l:{lang::rust,lang::go,lang::zig})
    {
        ::std::size_t reads{},types{}; bool drift{};
        s::integer a{7u,32u,false,false,kind::integer},b{3u,32u,false,false,kind::integer};
        auto value{[&](auto const& leaf,bool size,s::integer& out)
        {
            ++reads;if(size) { return false; }
            if(leaf.root_name=="a") { out=a;return true; }
            if(leaf.root_name=="b") { out=b;return true; }
            if(leaf.root_name=="flag" || leaf.root_name=="android" || leaf.root_name=="ordinary") { out={1u,drift ? 32u : 8u,true,false,kind::boolean};return true; }
            return false;
        }};
        auto type{[&](auto const& leaf,bool size,s::integer& out)
        {
            ++types;if(size) { return false; }
            if(leaf.root_name=="a") { out=a;out.bits=0u;return true; }
            if(leaf.root_name=="b") { out=b;out.bits=0u;return true; }
            if(leaf.root_name=="flag" || leaf.root_name=="android" || leaf.root_name=="ordinary") { out={0u,8u,true,false,kind::boolean};return true; }
            return false;
        }};
        auto run{[&](::std::string_view text,s::error expected=s::error::none)
        { s::program p{};s::integer out{};check(s::parse(text,p)==s::error::none,"owned parse");
          auto const status{s::evaluate(p,value,out,guest,type,l)};check(status==expected,text);return out; }};
        auto boolean{[&](::std::string_view text,bool expected)
        { auto out{run(text)};check(out.bits==expected && out.width==8u && out.unsigned_value && !out.floating && out.category==kind::boolean &&
                                  s::copied_type_name(out)=="bool",text); }};
        boolean("true",true);boolean("false",false);boolean("!true",false);boolean("!flag",false);
        boolean("a == b",false);boolean("a != b",true);boolean("a > b",true);boolean("a >= b",true);boolean("a < b",false);boolean("a <= b",false);
        boolean("true == false",false);boolean("flag != false",true);boolean("android == ordinary",true);
        run("bool(1)",s::error::unsupported);run("bool(1.0)",s::error::unsupported);
        if(l==lang::zig) { run("bool(true)",s::error::unsupported);run("i32(true)",s::error::unsupported); }
        else { boolean("bool(true)",true); }
        run("f64(true)",s::error::unsupported);
        if(l==lang::rust) { auto v{run("true as i32")};check(v.bits==1u && v.width==32u && v.category==kind::integer,"Rust bool as integer"); }
        else { run("i32(true)",s::error::unsupported); }
        if(l==lang::zig)
        { boolean("(a > b) and flag",true);boolean("(a < b) or flag",true);boolean("flag and !false or false",true);
          run("true && false",s::error::unsupported);run("false || true",s::error::unsupported); }
        else
        { boolean("(a > b) && flag",true);boolean("(a < b) || flag",true);run("true and false",s::error::unsupported); }
        auto const lazy_and{l==lang::zig ? "false and (1 / 0 > 0)" : "false && (1 / 0 > 0)"};
        auto const lazy_or{l==lang::zig ? "true or (1 / 0 > 0)" : "true || (1 / 0 > 0)"};
        auto before{reads};boolean(lazy_and,false);boolean(lazy_or,true);check(reads==before,"dead arithmetic has no value read/evaluation");
        before=reads;run(l==lang::zig ? "false and missing" : "false && missing",s::error::unavailable);check(reads==before,"unknown dead type refused before values");
        before=reads;run(l==lang::zig ? "false and bool(1)" : "false && bool(1)",s::error::unsupported);check(reads==before,"invalid dead Boolean conversion refused before values");
        before=reads;run(l==lang::zig ? "false and 1" : "false && 1",s::error::unsupported);check(reads==before,"nonBoolean dead operand refused before values");
        before=reads;run(l==lang::zig ? "1 or true" : "1 || true",s::error::unsupported);check(reads==before,"numeric truth conversion refused before values");
        drift=true;run(l==lang::zig ? "flag and true" : "flag && true",s::error::unavailable);drift=false;
        for(auto text:{"true + 1","true == 1","1 == false","-true","+flag","~true","true ? 1 : 0"}) { run(text,s::error::unsupported); }
        if(l==lang::rust)
        {
            boolean("false & true == false",true);boolean("false == true | true",false);boolean("true | false == false",false);
            boolean("true & false",false);boolean("true | false",true);boolean("flag ^ true",false);
            boolean("false < true",true);boolean("true >= false",true);
            for(unsigned width:{8u,16u,32u,64u}) for(bool uns:{false,true})
            { a={5u,width,uns,false,kind::integer};auto out{run("!a")};auto const mask{width==64u ? ~::std::uint64_t{} : (::std::uint64_t{1u}<<width)-1u};
              check(out.bits==((~5ull)&mask) && out.width==width && out.unsigned_value==uns && out.category==kind::integer,"Rust integer ! preserves primitive width"); }
        }
        else
        { if(l==lang::go) { boolean("1 + 2 << 3 == 17",true);boolean("1 | 2 + 3 == 6",true); }
          run("!a",s::error::unsupported);for(auto text:{"true & false","true | false","true ^ false","false < true"}) { run(text,s::error::unsupported); } }
        // Independent compiler Boolean comparison oracle, including IEEE NaN.
        ::std::uint64_t state{0x20261008};
        auto next{[&] { state ^= state<<13u;state ^= state>>7u;state ^= state<<17u;return static_cast<::std::uint32_t>(state); }};
        for(unsigned i{};i!=10000u;++i)
        {
            auto const x{::std::bit_cast<::std::int32_t>(next())},y{::std::bit_cast<::std::int32_t>(next())};
            a=s::from_dwarf_numeric(s::dwarf::numeric_kind::signed_integer,static_cast<::std::uint32_t>(x),32u);
            b=s::from_dwarf_numeric(s::dwarf::numeric_kind::signed_integer,static_cast<::std::uint32_t>(y),32u);
            boolean("a < b",x<y);boolean("a >= b",x>=y);boolean("a == b",x==y);boolean("a != b",x!=y);
        }
        for(double x:{-0.0,0.0,-1.25,1.25,(::std::numeric_limits<double>::max)(),(::std::numeric_limits<double>::denorm_min)(),
                       ::std::numeric_limits<double>::infinity(),-::std::numeric_limits<double>::infinity(),::std::numeric_limits<double>::quiet_NaN()})
        {
            a=s::from_dwarf_numeric(s::dwarf::numeric_kind::f64_bits,::std::bit_cast<::std::uint64_t>(x),64u);
            b=s::from_dwarf_numeric(s::dwarf::numeric_kind::f64_bits,::std::bit_cast<::std::uint64_t>(0.0),64u);
            boolean("a < b",x<0.0);boolean("a >= b",x>=0.0);boolean("a == b",x==0.0);boolean("a != b",x!=0.0);
        }
        check(types!=0u,"logical declaration-only preflight exercised");
    }
#endif
    ::fast_io::io::println("PASS primitive language predicates checks=",checks);
}
