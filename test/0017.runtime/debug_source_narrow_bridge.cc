// Primitive result typing with actual production metadata helper; no VM read authority.
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
#include <type_traits>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
namespace d=uwvm2::uwvm::debugger::source_dwarf;
using language=s::language_semantics;
static ::std::size_t checks{};
static void check(bool ok,char const* why)
{ ++checks;if(!ok) { ::fast_io::io::perrln("narrow bridge FAIL: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate(); } }
template<typename T> static constexpr ::std::string_view name()
{ if constexpr(::std::is_same_v<T,signed char>) { return "signed char"; }
 else if constexpr(::std::is_same_v<T,unsigned char>) { return "unsigned char"; }
 else if constexpr(::std::is_same_v<T,short>) { return "short"; }
 else { return "unsigned short"; } }
static d::type_record record(::std::string_view base_name,unsigned bytes,bool uns,::std::uint64_t unit=4u,::std::uint64_t offset=90u)
{
 d::type_record t{};t.identity={unit,50u};t.declaration_identity={unit,offset};t.kind=d::type_kind::scalar;t.byte_count=bytes;t.byte_size=bytes;t.size_known=true;
 t.language=0x1au;t.encoding=bytes==1u ? (uns ? 8u : 6u) : (uns ? 7u : 5u);
 t.narrow_builtin=d::cxx_narrow_from_base(t.language,false,base_name,t.encoding,bytes);t.name=::fast_io::concat_std("display-alias");t.named_type_alias=true;return t;
}
static s::integer numeric(d::type_record const& t,::std::uint64_t bits)
{ auto out{s::from_dwarf_numeric(t.encoding==7u || t.encoding==8u ? d::numeric_kind::unsigned_integer : d::numeric_kind::signed_integer,bits,t.byte_count*8u)};
 check(s::attach_narrow_type(t,out),"actual helper attaches immutable declaration DATA");return out; }
template<typename T> static void matrix()
{
 static_assert(::std::is_same_v<decltype(true ? T{} : T{}),T>);
 auto left{record(name<T>(),sizeof(T),::std::is_unsigned_v<T>)};auto right{record(name<T>(),sizeof(T),::std::is_unsigned_v<T>,8u,140u)};
 for(int x=-5;x<=5;++x) { for(int y=-5;y<=5;++y) { for(bool yes:{false,true}) { for(unsigned style{};style<3u;++style) { for(unsigned guest:{32u,64u})
 {
  T const a{static_cast<T>(x)},b{static_cast<T>(y)},expected{yes ? a : b};
  auto const abits{static_cast<::std::uint64_t>(a)&s::details::mask(sizeof(T)*8u)},bbits{static_cast<::std::uint64_t>(b)&s::details::mask(sizeof(T)*8u)};
  auto const expected_bits{static_cast<::std::uint64_t>(expected)&s::details::mask(sizeof(T)*8u)};
  auto text{style==0u ? ::fast_io::concat_std(yes ? "1 ? left : (" : "0 ? left : (",name<T>(),")",y) :
   style==1u ? ::fast_io::concat_std(yes ? "1 ? (" : "0 ? (",name<T>(),")",x," : right") : ::fast_io::concat_std(yes ? "1 ? left : right" : "0 ? left : right")};
  s::program code{};check(s::parse(text,code)==s::error::none,"matrix syntax");::std::size_t reads{},queries{};
  auto types{[&](d::source_expression const& e,s::integer& out)
  { ++queries;if(e.root_name=="left") { out=numeric(left,0u);return true; }if(e.root_name=="right") { out=numeric(right,0u);return true; }return false; }};
  auto values{[&](d::source_expression const& e,bool size,s::integer& out)
  { ++reads;if(size) { return false; }if(yes && e.root_name=="left") { out=numeric(left,abits);return true; }
    if(!yes && e.root_name=="right") { out=numeric(right,bbits);return true; }return false; }};
  auto const wanted_reads{style==0u ? static_cast<::std::size_t>(yes) : style==1u ? static_cast<::std::size_t>(!yes) : 1u};
  auto const wanted_queries{style==2u ? 2u : 1u};s::integer out{};
  check(s::evaluate(code,values,out,guest,types,language::cpp)==s::error::none,"native same standard primitive result");
  check(out.bits==expected_bits && out.width==sizeof(T)*8u && out.unsigned_value==::std::is_unsigned_v<T> && out.category==s::value_category::integer &&
   s::copied_type_name(out)==name<T>() && !out.declaration_identity_known && reads==wanted_reads && queries==wanted_queries,"primitive name and copied value; no invented common DIE");
  reads=0u;queries=0u;
  check(s::evaluate(code,values,out,guest,types,language::c)==s::error::none && out.width==32u && !out.unsigned_value &&
   out.bits==(static_cast<::std::uint64_t>(static_cast<int>(expected))&0xffffffffu) && s::copied_type_name(out)=="int" && reads==wanted_reads && queries==wanted_queries,"C integer promotion regression");
  reads=0u;queries=0u;
  check(s::evaluate(code,values,out,guest,types)==s::error::unsupported && reads==0u && out.bits==0u,"shared syntax does not acquire a C++ profile");
 } } } } }
}
static void metadata_guards()
{
 for(auto lang:{0x04u,0x11u,0x19u,0x1au,0x21u,0x2au,0x2bu,0x3au})
 {
  for(auto text:{::std::string_view{"short"},::std::string_view{"short int"},::std::string_view{"signed short"},::std::string_view{"signed short int"}})
   check(d::cxx_narrow_from_base(lang,false,text,5u,2u)==s::narrow_builtin::signed_short,"C++ standard signed short producer names");
  for(auto text:{::std::string_view{"unsigned short"},::std::string_view{"unsigned short int"},::std::string_view{"short unsigned int"}})
   check(d::cxx_narrow_from_base(lang,false,text,7u,2u)==s::narrow_builtin::unsigned_short,"standard unsigned short producer names");
  check(d::cxx_narrow_from_base(lang,false,"char",6u,1u)==s::narrow_builtin::plain_char &&
   d::cxx_narrow_from_base(lang,false,"signed char",6u,1u)==s::narrow_builtin::signed_char &&
   d::cxx_narrow_from_base(lang,false,"unsigned char",8u,1u)==s::narrow_builtin::unsigned_char,"separate character rank identities");
 }
 for(auto lang:{0u,0x01u,0x0cu,0x10u,0x1du,0x3eu,0x16u,0x1cu,0x27u})
  check(d::cxx_narrow_from_base(lang,false,"short",5u,2u)==s::narrow_builtin::unknown,"other language cannot prove C++ primitive");
 for(auto text:{::std::string_view{"i16"},::std::string_view{"uint16_t"},::std::string_view{"my_short"},::std::string_view{"unsigned"},::std::string_view{"signed  short"}})
  check(d::cxx_narrow_from_base(0x1au,false,text,5u,2u)==s::narrow_builtin::unknown,"unknown base names do not establish native rank");
 check(d::cxx_narrow_from_base(0x1au,true,"short",5u,2u)==s::narrow_builtin::unknown,"TinyGo cannot acquire C++ type evidence");
 for(unsigned encoding{};encoding<16u;++encoding) { for(unsigned bytes{};bytes<5u;++bytes)
 { check((d::cxx_narrow_from_base(0x1au,false,"char",encoding,bytes)==s::narrow_builtin::plain_char)==(encoding==6u && bytes==1u),"char name encoding and ABI all agree");
   check((d::cxx_narrow_from_base(0x1au,false,"short",encoding,bytes)==s::narrow_builtin::signed_short)==(encoding==5u && bytes==2u),"short name encoding and ABI all agree"); } }
 auto base{record("short",2u,false)};
 for(unsigned fault{};fault<7u;++fault)
 { auto t{base};if(fault==0u) { t.kind=d::type_kind::enumeration; }if(fault==1u) { t.kind=d::type_kind::pointer; }
   if(fault==2u) { t.atomic_scalar=true; }if(fault==3u) { t.display_qualifiers=8u; }if(fault==4u) { t.declaration_identity.offset=0u; }
   if(fault==5u) { t.byte_count=1u; }if(fault==6u) { t.encoding=4u; }
   auto out{s::from_dwarf_numeric(d::numeric_kind::signed_integer,1u,16u)};check(!s::attach_narrow_type(t,out) && !out.declaration_identity_known && out.builtin_identity==s::narrow_builtin::unknown,"unsafe scalar layouts cannot attach type proof"); }
 for(unsigned fault{};fault<4u;++fault)
 { auto t{base};if(fault==0u) { t.language=0x1du; }if(fault==1u) { t.tinygo_producer=true; }if(fault==2u) { t.identity.unit=8u; }
   if(fault==3u) { t.narrow_builtin=static_cast<s::narrow_builtin>(255u); }
   auto out{numeric(t,1u)};check(out.declaration_identity_known && out.builtin_identity==s::narrow_builtin::unknown && s::copied_type_name(out)=="signed integer","canonical DATA preserved without unsupported standard identity"); }
 auto out{s::from_dwarf_numeric(d::numeric_kind::boolean,1u,8u)};check(s::copied_type_name(out)=="bool" && !s::attach_narrow_type(base,out),"Boolean retains existing category");
 out=s::from_dwarf_numeric(d::numeric_kind::f32_bits,0u,32u);check(s::copied_type_name(out)=="float" && !s::attach_narrow_type(base,out),"float retains existing category");
 out=s::from_dwarf_numeric(d::numeric_kind::f64_bits,0u,64u);check(s::copied_type_name(out)=="double","double display unchanged");
 out=s::from_dwarf_numeric(d::numeric_kind::signed_integer,1u,16u);out.builtin_identity=static_cast<s::narrow_builtin>(255u);check(s::copied_type_name(out).empty(),"invalid primitive marker has no type name");
}
static void copy_guards()
{
 auto a{record("short",2u,false)},b{record("short",2u,false,8u,140u)};
 ::std::size_t reads{};auto types{[&](d::source_expression const& e,s::integer& out) { if(e.root_name!="left" && e.root_name!="right") { return false; }out=numeric(e.root_name=="left" ? a : b,0u);return true; }};
 auto values{[&](d::source_expression const& e,bool size,s::integer& out) { ++reads;if(size || e.root_name!="right") { return false; }out=numeric(b,2u);return true; }};
 s::program code{};s::integer out{};
 check(s::parse("1 ? (0 ? left : right) : (short)3",code)==s::error::none && s::evaluate(code,values,out,32u,types,language::cpp)==s::error::none && out.bits==2u && out.width==16u && reads==1u && !out.declaration_identity_known && s::copied_type_name(out)=="short","nested standard results across distinct base DIEs");
 reads=0u;auto mismatch{[&](d::source_expression const& e,bool z,s::integer& v) { auto ok{values(e,z,v)};v.declaration_identity.offset=141u;return ok; }};
 check(s::evaluate(code,mismatch,out,32u,types,language::cpp)==s::error::unavailable && out.bits==0u && reads==1u,"standard match cannot bypass selected declaration identity");
 a=record("char",1u,false);b=record("signed char",1u,false);reads=0u;
 check(s::parse("1 ? left : right",code)==s::error::none && s::evaluate(code,values,out,32u,types,language::cpp)==s::error::unavailable && reads==0u && out.bits==0u,"same DIE cannot assert char and signed char");
 a=record("short",2u,false);b=a;reads=0u;
 check(s::parse("1 ? left : (i16)2",code)==s::error::none && s::evaluate(code,values,out,32u,types,language::cpp)==s::error::unsupported && reads==0u,"shared alias cast cannot match a native variable");
 check(s::parse("1 ? (short)1 : (short)(1/0)",code)==s::error::none && s::evaluate(code,values,out,32u,types,language::cpp)==s::error::none && out.bits==1u && s::copied_type_name(out)=="short" && reads==0u,"dead-arm type does not run arithmetic");
 check(s::parse("1 ? +(short)1 : +(short)2",code)==s::error::none && s::evaluate(code,values,out,32u,types,language::cpp)==s::error::none && out.width==32u && s::copied_type_name(out)=="int","promotion removes narrow spelling");
}
int main(int argc,char** argv)
{
 if(argc>=2)
 { auto t{record("short",2u,false)};auto v{numeric(t,65535u)};auto types{[&](d::source_expression const& e,s::integer& out) { if(e.root_name!="left" && e.root_name!="right") { return false; }out=numeric(t,0u);return true; }};
   auto values{[&](d::source_expression const& e,bool z,s::integer& out) { if(z || (e.root_name!="left" && e.root_name!="right")) { return false; }out=v;return true; }};
   for(int i=1;i<argc;++i) { s::program code{};s::integer out{};auto p{s::parse(::std::string_view{argv[i]},code)};
    auto e{p==s::error::none ? s::evaluate(code,values,out,32u,types,language::cpp) : p};
    ::fast_io::io::println(static_cast<unsigned>(p),"\t",static_cast<unsigned>(e),"\t",out.bits,"\t",out.width,"\t",out.unsigned_value,"\t",static_cast<unsigned>(out.category),"\t",s::copied_type_name(out)); }
   return 0; }
 matrix<signed char>();matrix<unsigned char>();matrix<short>();matrix<unsigned short>();metadata_guards();copy_guards();
 ::fast_io::io::println("debug_source_narrow_bridge: PASS checks=",checks," native-type-and-name metadata-DATA only");
}
