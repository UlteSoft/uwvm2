// Finite copied scalar types; this fixture has no VM pause/read authority.
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
#include <type_traits>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
namespace d=uwvm2::uwvm::debugger::source_dwarf;
using language=s::language_semantics;
static ::std::size_t checks{};
static void check(bool ok,char const* why)
{ ++checks;if(!ok) { ::fast_io::io::perrln("exact narrow FAIL: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate(); } }
static s::error compute(::std::string_view text,language mode,s::integer& out,unsigned guest=32u)
{ s::program code{};auto value{[](d::source_expression const&,bool,s::integer&) { return false; }};
  check(s::parse(text,code)==s::error::none,"syntax");return s::evaluate(code,value,out,guest,s::details::no_type_resolver{},mode); }
static void verify(::std::string_view text,language mode,::std::uint64_t bits,unsigned width,bool uns,
 s::value_category category=s::value_category::unspecified,s::narrow_builtin type=s::narrow_builtin::unknown)
{ for(unsigned guest:{32u,64u}) { s::integer out{};check(compute(text,mode,out,guest)==s::error::none,"evaluation");
  check(out.bits==bits && out.width==width && out.unsigned_value==uns && !out.floating && out.category==category && out.builtin_identity==type,"type and native value"); } }
template<typename T> static constexpr char const* name()
{ if constexpr(::std::is_same_v<T,signed char>) { return "signed char"; }
  else if constexpr(::std::is_same_v<T,unsigned char>) { return "unsigned char"; }
  else if constexpr(::std::is_same_v<T,short>) { return "short"; }
  else { return "unsigned short"; } }
template<typename T> static constexpr auto identity()
{ if constexpr(::std::is_same_v<T,signed char>) { return s::narrow_builtin::signed_char; }
  else if constexpr(::std::is_same_v<T,unsigned char>) { return s::narrow_builtin::unsigned_char; }
  else if constexpr(::std::is_same_v<T,short>) { return s::narrow_builtin::signed_short; }
  else { return s::narrow_builtin::unsigned_short; } }
template<typename T> static void matrix()
{
 static_assert(::std::is_same_v<decltype(true ? T{} : T{}),T>);
 for(int x=-5;x<=5;++x) { for(int y=-5;y<=5;++y) { for(bool yes:{false,true})
 {
  T const a{static_cast<T>(x)},b{static_cast<T>(y)};T const expected{yes ? a : b};
  auto text{::fast_io::concat_std(yes ? "1 ? (" : "0 ? (",::fast_io::mnp::os_c_str(name<T>()),")",x," : (",::fast_io::mnp::os_c_str(name<T>()),")",y)};
  auto const bits{static_cast<::std::uint64_t>(expected)&s::details::mask(sizeof(T)*8u)};
  verify(text,language::cpp,bits,sizeof(T)*8u,::std::is_unsigned_v<T>,s::value_category::integer,identity<T>());
  auto const wide{static_cast<::std::uint64_t>(static_cast<int>(expected))&0xffffffffu};
  verify(text,language::c,wide,32u,false);verify(text,language::c23,wide,32u,false);
 } } }
}
static void declared_identity()
{
 for(unsigned guest:{32u,64u}) { for(bool yes:{false,true}) { for(unsigned mode{};mode<5u;++mode)
 {
  ::std::size_t reads{},queries{};
  auto types{[&](d::source_expression const& expression,s::integer& out)
  { ++queries;if(expression.root_name!="left" && expression.root_name!="right") { return false; }
    out=s::from_dwarf_numeric(d::numeric_kind::signed_integer,0u,16u);
    out.declaration_identity={4u,90u};out.declaration_identity_known=mode!=1u;
    if(expression.root_name=="right" && mode==2u) { out.declaration_identity.unit=8u; }
    if(expression.root_name=="right" && mode==3u) { out.declaration_identity.offset=100u; }
    if(mode==4u) { out.declaration_identity.offset=0u; }return true; }};
  auto values{[&](d::source_expression const& expression,bool size,s::integer& out)
  { ++reads;if(size || expression.root_name!=(yes ? "left" : "right")) { return false; }
    out=s::from_dwarf_numeric(d::numeric_kind::signed_integer,yes ? 65535u : 2u,16u);
    out.declaration_identity={4u,90u};out.declaration_identity_known=true;return true; }};
  s::program code{};s::integer out{};check(s::parse(yes ? "1 ? left : right" : "0 ? left : right",code)==s::error::none,"declared syntax");
  auto const status{s::evaluate(code,values,out,guest,types,language::cpp)};
  check(mode==0u ? status==s::error::none && out.width==16u && out.bits==(yes ? 65535u : 2u) && out.declaration_identity_known && reads==1u && queries==2u :
    status!=s::error::none && out.bits==0u && reads==0u,"canonical same DIE only; no caller/CU/layout substitution before values");
  if(mode==0u)
  {
   auto mismatched{[&](d::source_expression const& e,bool z,s::integer& value) { auto ok{values(e,z,value)};value.declaration_identity.offset=91u;return ok; }};
   reads=0u;queries=0u;
   check(s::evaluate(code,mismatched,out,guest,types,language::cpp)==s::error::unavailable && reads==1u && out.bits==0u,"copied value identity must match selected declaration");
  }
 } } }
}
int main(int argc,char** argv)
{
 if(argc>=3)
 { auto mode{::std::string_view{argv[1]}=="cpp" ? language::cpp : ::std::string_view{argv[1]}=="c23" ? language::c23 : language::c};
   for(int i=2;i<argc;++i) { s::program code{};s::integer out{};auto resolve{[](d::source_expression const&,bool,s::integer&) { return false; }};
    auto parse{s::parse(::std::string_view{argv[i]},code)};auto status{parse==s::error::none ? s::evaluate(code,resolve,out,32u,s::details::no_type_resolver{},mode) : parse};
    ::fast_io::io::println(static_cast<unsigned>(parse),"\t",static_cast<unsigned>(status),"\t",out.bits,"\t",out.width,"\t",out.unsigned_value,"\t",static_cast<unsigned>(out.category)); }
   return 0; }
 matrix<signed char>();matrix<unsigned char>();matrix<short>();matrix<unsigned short>();declared_identity();
 check(s::language_from_dwarf(0x3eu,false)==language::c23 && s::language_from_dwarf(0x3eu,true)==language::shared_numeric,"explicit C23 metadata and TinyGo exclusion");
 for(auto const* text:{"true","(true)","false"}) { verify(text,language::c23,::std::string_view{text}=="false" ? 0u : 1u,8u,true,s::value_category::boolean); }
 verify("1 ? true : false",language::c23,1u,32u,false);verify("1 ? (0 ? true : false) : true",language::c23,0u,32u,false);
 verify("!false",language::c23,1u,32u,false);verify("true < false",language::c23,0u,32u,false);verify("true + true",language::c23,2u,32u,false);
 verify("true || (1/0)",language::c23,1u,32u,false);verify("false && (1/0)",language::c23,0u,32u,false);
 verify("1 ? (signed short int)-1 : (int short signed)2",language::cpp,65535u,16u,false,s::value_category::integer,s::narrow_builtin::signed_short);
 verify("0 ? (short unsigned)1 : (unsigned int short)65535",language::cpp,65535u,16u,true,s::value_category::integer,s::narrow_builtin::unsigned_short);
 verify("1 ? (char)1 : (signed char)2",language::cpp,1u,32u,false);verify("1 ? 'a' : (signed char)2",language::cpp,97u,32u,false);
 verify("1 ? (1 ? (short)-1 : (short)2) : (short)3",language::cpp,65535u,16u,false,s::value_category::integer,s::narrow_builtin::signed_short);
 verify("1 ? (short)1 : (short)(1/0)",language::cpp,1u,16u,false,s::value_category::integer,s::narrow_builtin::signed_short);
 for(auto const* text:{"'a'","' '","'\\n'","'\\0'"})
 { auto bits{::std::string_view{text}=="'a'" ? 97u : ::std::string_view{text}=="' '" ? 32u : ::std::string_view{text}=="'\\n'" ? 10u : 0u};
   verify(text,language::cpp,bits,8u,false,s::value_category::integer,s::narrow_builtin::plain_char);verify(text,language::c,bits,32u,false); }
 verify("1 ? 'a' : 'b'",language::cpp,97u,8u,false,s::value_category::integer,s::narrow_builtin::plain_char);
 for(auto const* text:{"1 ? (i8)1 : (i8)2","1 ? (short)1 : (i16)2"})
 { s::integer out{};check(compute(text,language::cpp,out)==s::error::unsupported && out.bits==0u,"shared alias spelling is not exact C++ identity"); }
 ::fast_io::io::println("debug_source_exact_narrow: PASS checks=",checks," profiles=C,CPP,C23 widths=2 copied-type-DATA only");
}
