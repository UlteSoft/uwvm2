// ASCII escapes and language types: compiler witnesses and bounded owned DATA only.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
namespace d=uwvm2::uwvm::debugger::source_dwarf;
using L=s::language_semantics;
static ::std::size_t checks{};
static void check(bool ok,::std::string_view why)
{ ++checks;if(!ok) { ::fast_io::io::perrln("character escape FAIL: ",why);::fast_io::fast_terminate(); } }
#define UWVM_ESCAPE_CASE(expr,value) static_assert((expr)==(value)); static_assert(sizeof(expr)==sizeof(char));
#include "fixtures/debug_source_character_escape_cases.h"
#undef UWVM_ESCAPE_CASE
struct witness { ::std::string_view text;::std::uint64_t value; };
static constexpr witness witnesses[]{
#define UWVM_ESCAPE_CASE(expr,value) {#expr,static_cast<::std::uint64_t>(expr)},
#include "fixtures/debug_source_character_escape_cases.h"
#undef UWVM_ESCAPE_CASE
};
static void probe(::std::string_view text,unsigned ordinal)
{
 s::program p{};s::integer v{};::std::size_t reads{};
 auto never{[&](d::source_expression const&,bool,s::integer&) { ++reads;return false; }};
 auto parsed{s::parse(text,p)};auto status{parsed==s::error::none?s::evaluate(p,never,v,32u,s::details::no_type_resolver{},L::cpp):parsed};
 ::fast_io::io::println(ordinal,"\t",static_cast<unsigned>(status),"\t",v.bits,"\t",v.width,"\t",v.unsigned_value,"\t",v.floating,"\t",reads);
}
int main(int argc,char** argv)
{
 if(argc>1) { for(int i=1;i<argc;++i) { auto a{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])}};probe({a.data(),a.size()},i-1); }return 0; }
 for(unsigned repeat{};repeat<16u;++repeat) { for(unsigned guest:{32u,64u}) { for(auto language:{L::shared_numeric,L::c,L::cpp,L::c23}) { for(auto w:witnesses)
 {
  ::std::size_t reads{},types{};auto never{[&](d::source_expression const&,bool,s::integer&) { ++reads;return false; }};
  auto absent{[&](d::source_expression const&,bool,s::integer&) { ++types;return false; }};
  auto eval{[&](::std::string_view text,::std::uint64_t bits,unsigned width,bool uns,::std::string_view name)
  { s::program p{};s::integer value{};check(s::parse(text,p)==s::error::none,"complete character grammar");
    check(s::evaluate(p,never,value,guest,absent,language)==s::error::none && value.bits==bits && value.width==width && value.unsigned_value==uns && !value.floating,"independent character value and width");
    check(s::copied_type_name(value)==name && reads==0u && types==0u,"copied language type without any resolver calls"); }};
  auto const narrow{language==L::cpp};auto const native{language!=L::shared_numeric};
  eval(w.text,w.value,narrow?8u:32u,false,narrow?"char":native?"int":"signed integer");
  eval(::fast_io::concat_std("+",w.text),w.value,32u,false,native?"int":"signed integer");
  eval(::fast_io::concat_std("sizeof ",w.text),narrow?1u:4u,guest,true,native?"unsigned long":"unsigned integer");
  eval(::fast_io::concat_std("sizeof(",w.text," + 1/0)"),4u,guest,true,native?"unsigned long":"unsigned integer");
  eval(::fast_io::concat_std("1 ? ",w.text," : ",w.text),w.value,narrow?8u:32u,false,narrow?"char":native?"int":"signed integer");
  eval(::fast_io::concat_std("1 ? ",w.text," : (1/0)"),w.value,32u,false,native?"int":"signed integer");
 } } } }
 auto never{[](d::source_expression const&,bool,s::integer&) { return false; }};
 for(auto token:{::std::string_view{"'\\x0000000041'"},::std::string_view{"'\\x7F'"},::std::string_view{"'\\000'"}})
 { s::program p{};s::integer value{};check(s::parse(token,p)==s::error::none && s::evaluate(p,never,value)==s::error::none,"hex case/leading zeros and octal NUL"); }
 for(auto text:{::std::string_view{"'\\x'"},::std::string_view{"'\\X41'"},::std::string_view{"'\\x80'"},::std::string_view{"'\\200'"},::std::string_view{"'\\400'"},::std::string_view{"'\\x100'"},::std::string_view{"'\\x41g'"},::std::string_view{"'\\08'"},::std::string_view{"'\\0000'"},::std::string_view{"'\\1234'"},::std::string_view{"'\\q'"},::std::string_view{"'\\u0041'"},::std::string_view{"'\\x41 '"},::std::string_view{"'\\1 '"},::std::string_view{"'\\x41"},::std::string_view{"'\\x41''a'"},::std::string_view{"L'\\x41'"},::std::string_view{"u8'\\x41'"},::std::string_view{"'\\x41';continue"},::std::string_view{"'\\x41' + call()"}})
 { for(auto prefix:{::std::string_view{},::std::string_view{"0 && "},::std::string_view{"1 ? 7 : "}})
  { s::program p{};check(s::parse(::fast_io::concat_std(prefix,text),p)!=s::error::none && p.nodes.empty(),"malformed and unsupported complete syntax cannot hide in a dead arm"); } }
 for(auto text:{::std::string_view{"sizeof('\\x28' + '\\x29')"},::std::string_view{"sizeof('\\'' + '\\x29')"},::std::string_view{"sizeof('\\x5c' + '\\047')"}})
 { s::program p{};s::integer value{};check(s::parse(text,p)==s::error::none && s::evaluate(p,never,value,32u,s::details::no_type_resolver{},L::cpp)==s::error::none && value.bits==4u,"sizeof delimiters follow complete character tokens"); }
 char digits[240u]{};for(auto& c:digits) { c='0'; }
 auto long_zero{::fast_io::concat_std("'\\x",::std::string_view{digits,240u},"41'")};s::program p{};s::integer value{};
 check(s::parse(long_zero,p)==s::error::none && s::evaluate(p,never,value)==s::error::none && value.bits==65u,"bounded leading-zero escape");
 for(auto& c:digits) { c='f'; }
 auto overflow{::fast_io::concat_std("'\\x",::std::string_view{digits,240u},"'")};check(s::parse(overflow,p)!=s::error::none && p.nodes.empty(),"large magnitude cannot overflow into ASCII");
 ::fast_io::io::println("debug_source_character_escape: PASS checks=",checks," compiler_witnesses=283 ASCII finite DATA only");
}
