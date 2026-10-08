// C++ UTF character syntax and native compiler witnesses. No guest read tokens.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <type_traits>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
namespace d=uwvm2::uwvm::debugger::source_dwarf;
using L=s::language_semantics;
static ::std::size_t checks{};
static void check(bool v,::std::string_view why)
{ ++checks;if(!v) { ::fast_io::io::perrln("UTF character FAIL: ",why);::fast_io::fast_terminate(); } }
template<typename T> constexpr ::std::string_view native_name()
{
 if constexpr(::std::is_same_v<T,char8_t>) return "char8_t";
 else if constexpr(::std::is_same_v<T,char16_t>) return "char16_t";
 else if constexpr(::std::is_same_v<T,char32_t>) return "char32_t";
 else if constexpr(::std::is_same_v<T,int>) return "int";
 else if constexpr(::std::is_same_v<T,unsigned>) return "unsigned int";
 else if constexpr(::std::is_same_v<T,bool>) return "bool";
 else return "unsupported";
}
struct witness { ::std::string_view text;::std::uint64_t bits;unsigned width;bool uns;::std::string_view type; };
#define W(expr) {#expr,static_cast<::std::uint64_t>(expr),sizeof(decltype(expr))*8u,::std::is_unsigned_v<decltype(expr)>,native_name<decltype(expr)>()}
static constexpr witness corpus[]{
 W(u8'a'),W(u8'\n'),W(u8'\0'),W(u8'\''),W(u8'\\'),W(u8'\?'),W(u8'\u0041'),
 W(u8'\x80'),W(u8'\xff'),W(u8'\377'),W(u8'\x0000000041'),
 W(u'\u03bb'),W(u'\u0000'),W(u'\uffff'),W(u'\xD800'),W(u'\xffff'),
 W(U'\U0001F642'),W(U'\U0010FFFF'),W(U'\u03bb'),W(U'\xD800'),W(U'\xffffffff'),
 W(+u8'\xff'),W(+u'\uffff'),W(+U'\U0001F642'),W(-U'\U0001F642'),
 W(u8'\x80'+1),W(u'\u03bb'+1),W(U'\U0001F642'+1),W(u'\uffff'>>4),W(U'\xffffffff'>>4),
 W(u'\u03bb'==u'\u03bb'),W(U'\U0001F642'>U'\u03bb'),
 W(1?u8'a':u8'b'),W(1?u'\u03bb':u'\u0041'),W(0?U'a':U'\U0001F642'),
 W(1?u8'a':(unsigned char)65),W(1?u'\u03bb':(unsigned short)955),
 W(1?u'\u03bb':U'\U0001F642'),W(1?U'\U0001F642':42u),
 W(static_cast<char8_t>(128)),W(static_cast<char16_t>(955)),W(static_cast<char32_t>(128578)),
 W(char8_t(255)),W(char16_t(955)),W(char32_t(128578)),
 W((char8_t)128),W((char16_t)955),W((char32_t)128578),
 W(1?char8_t(128):u8'\xff'),W(1?char16_t(955):u'\u0041'),
 W(1?char32_t(128578):U'\u0041')
};
#undef W
static void probe(::std::string_view text,unsigned ordinal)
{
 ::std::size_t reads{},types{};s::program p{};s::integer v{};
 auto never{[&](d::source_expression const&,bool,s::integer&) { ++reads;return false; }};
 auto absent{[&](d::source_expression const&,bool,s::integer&) { ++types;return false; }};
 auto parsed=s::parse(text,p,L::cpp);
 auto status=parsed==s::error::none?s::evaluate(p,never,v,32u,absent,L::cpp):parsed;
 ::fast_io::io::println(ordinal,"\t",static_cast<unsigned>(parsed),"\t",static_cast<unsigned>(status),"\t",v.bits,"\t",v.width,"\t",v.unsigned_value,"\t",s::copied_type_name(v),"\t",reads,"\t",types);
}
int main(int argc,char** argv)
{
 if(argc>1)
 {
  for(int i=1;i<argc;++i)
  { auto text=::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])};probe({text.data(),text.size()},i-1); }
  return 0;
 }
 for(auto guest:{32u,64u}) for(auto const& w:corpus)
 {
  ::std::size_t reads{},types{};s::program p{};s::integer v{};
  auto never{[&](d::source_expression const&,bool,s::integer&) { ++reads;return false; }};
  auto absent{[&](d::source_expression const&,bool,s::integer&) { ++types;return false; }};
  check(s::parse(w.text,p,L::cpp)==s::error::none,"complete UTF literal/cast grammar");
  check(s::evaluate(p,never,v,guest,absent,L::cpp)==s::error::none,"C++ result");
  check(v.bits==w.bits && v.width==w.width && v.unsigned_value==w.uns && s::copied_type_name(v)==w.type,"compiler value/width/signedness/type witness");
  auto query=::fast_io::concat_std("sizeof(",w.text,")");
  check(s::parse(query,p,L::cpp)==s::error::none && s::evaluate(p,never,v,guest,absent,L::cpp)==s::error::none && v.bits==w.width/8u,"unevaluated sizeof witness");
  check(reads==0u && types==0u,"owned literal results cannot call either resolver");
 }
 auto never=[](d::source_expression const&,bool,s::integer&) { return false; };
 for(auto text:{"u8'\\u0080'","u'\\U0001F642'","U'\\U00110000'","u'\\uD800'","U'\\uDFFF'","u8'\\x100'","u'\\x10000'","U'\\x100000000'","u'\\u03b'","u'\\u03bb0'","U'\\U0001F64Z'","u'ab'","u8''","L'a'","u'\\q'","u8'\\08'","u'\\0000'","U'\\xFFFFFFFFFFFFFFFFFFFF'","u'a';continue"})
 {
  for(auto prefix:{"","0 && ","1 ? 7 : "})
  { s::program p{};check(s::parse(::fast_io::concat_std(::std::string_view{prefix},::std::string_view{text}),p,L::cpp)!=s::error::none && p.nodes.empty(),"malformed UTF grammar cannot hide in a dead arm"); }
 }
 for(auto text:{"u8'a'","u'\\u03bb'","U'\\U0001F642'","sizeof(char16_t)","char32_t(128578)","0 && u8'a'","1 || U'a'","1 ? 7 : u'a'"})
 {
  s::program p{};check(s::parse(text,p,L::cpp)==s::error::none,"syntax admission");
  for(auto language:{L::shared_numeric,L::c,L::c23,L::rust,L::go,L::zig})
  { s::integer v{};check(s::evaluate(p,never,v,32u,s::details::no_type_resolver{},language)!=s::error::none && v.bits==0u,"C++ syntax cannot select another CU's language"); }
 }
 for(auto text:{"sizeof(char8_t)","sizeof(char16_t)","sizeof(char32_t)","sizeof(u')' + u'(')","sizeof(U'\\U0001F642' + 1/0)"})
 { s::program p{};s::integer v{};check(s::parse(text,p,L::cpp)==s::error::none && s::evaluate(p,never,v,32u,s::details::no_type_resolver{},L::cpp)==s::error::none,"complete UTF sizeof delimiters"); }
 ::fast_io::io::println("debug_source_utf_character_literal: PASS checks=",checks," compiler_witnesses=",sizeof(corpus)/sizeof(*corpus)," C++ finite UTF syntax DATA only");
}
