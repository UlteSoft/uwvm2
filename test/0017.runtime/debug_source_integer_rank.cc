// Standard rank and canonical copied names; finite DATA, no guest read capability.
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
#include <type_traits>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
namespace d=uwvm2::uwvm::debugger::source_dwarf;
using K=s::wide_builtin;using L=s::language_semantics;
static ::std::size_t checks{};
static void check(bool ok,char const* why)
{ ++checks;if(!ok) { ::fast_io::io::perrln("integer rank FAIL: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate(); } }
static ::std::string_view name(K k)
{ switch(k) { case K::signed_int:return "int";case K::unsigned_int:return "unsigned int";
 case K::signed_long:return "long";case K::unsigned_long:return "unsigned long";
 case K::signed_long_long:return "long long";case K::unsigned_long_long:return "unsigned long long";default:return {}; } }
template<typename T> static constexpr K kind()
{ if constexpr(::std::is_same_v<T,int>) { return K::signed_int; }else if constexpr(::std::is_same_v<T,unsigned>) { return K::unsigned_int; }
 else if constexpr(::std::is_same_v<T,long>) { return K::signed_long; }else if constexpr(::std::is_same_v<T,unsigned long>) { return K::unsigned_long; }
 else if constexpr(::std::is_same_v<T,long long>) { return K::signed_long_long; }else { static_assert(::std::is_same_v<T,unsigned long long>);return K::unsigned_long_long; } }
static unsigned width(K k,unsigned guest) { return d::c_wide_rank(k)==3u ? 32u : d::c_wide_rank(k)==5u ? 64u : guest; }
static d::type_record record(K k,unsigned guest,::std::uint64_t unit,::std::uint64_t offset)
{ d::type_record t{};t.identity={unit,offset-1u};t.declaration_identity={unit,offset};t.language=0x1au;t.kind=d::type_kind::scalar;
 t.byte_count=width(k,guest)/8u;t.byte_size=t.byte_count;t.size_known=true;t.encoding=d::c_wide_unsigned(k)?7u:5u;
 t.name=::fast_io::concat_std("display-only-alias");t.named_type_alias=true;
 t.wide_builtin=d::c_wide_from_base(t.language,false,name(k),t.encoding,t.byte_count,guest/8u);return t; }
static s::integer numeric(d::type_record const& t,::std::uint64_t bits,unsigned guest)
{ auto out{s::from_dwarf_numeric(t.encoding==7u ? d::numeric_kind::unsigned_integer : d::numeric_kind::signed_integer,bits,t.byte_count*8u)};
 check(s::attach_standard_integer_type(t,out,guest),"production helper attaches standard metadata");return out; }
template<typename T,typename U> static void native_pair()
{
 using A=decltype(T{}+U{});using C=decltype(true?T{}:U{});using B=decltype(T{}|U{});
 static_assert(::std::is_same_v<A,C> && ::std::is_same_v<A,B>);
 static_assert((sizeof(void*)==4u || sizeof(void*)==8u) && sizeof(long)==sizeof(void*) && sizeof(int)==4u && sizeof(long long)==8u);
 constexpr unsigned guest{sizeof(void*)*8u};
 auto a{record(kind<T>(),guest,4u,80u)},b{record(kind<U>(),guest,8u,160u)};
 for(int x=-3;x<=3;++x) { for(int y=-3;y<=3;++y) { for(unsigned op{};op<4u;++op) { for(auto language:{L::c,L::cpp})
 {
  T const left{static_cast<T>(x)};U const right{static_cast<U>(y)};
  A const expected{op==0u ? static_cast<A>(left+right) : op==1u ? static_cast<A>(left|right) : op==2u ? static_cast<A>(true?left:right) : static_cast<A>(false?left:right)};
  ::std::string_view text{op==0u ? "left + right" : op==1u ? "left | right" : op==2u ? "1 ? left : right" : "0 ? left : right"};
  s::program p{};s::integer out{};::std::size_t reads{},queries{};
  auto types{[&](d::source_expression const& e,s::integer& v) { ++queries;if(e.root_name!="left" && e.root_name!="right") { return false; }v=numeric(e.root_name=="left"?a:b,0u,guest);return true; }};
  auto values{[&](d::source_expression const& e,bool size,s::integer& v)
  { ++reads;if(size || (e.root_name!="left" && e.root_name!="right") || (op==2u && e.root_name!="left") || (op==3u && e.root_name!="right")) { return false; }
    v=e.root_name=="left" ? numeric(a,static_cast<::std::uint64_t>(left)&s::details::mask(sizeof(T)*8u),guest) :
       numeric(b,static_cast<::std::uint64_t>(right)&s::details::mask(sizeof(U)*8u),guest);return true; }};
  check(s::parse(text,p)==s::error::none,"native expression parsed");
  check(s::evaluate(p,values,out,guest,types,language)==s::error::none,"native integer rank result");
  check(out.bits==(static_cast<::std::uint64_t>(expected)&s::details::mask(sizeof(A)*8u)) && out.width==sizeof(A)*8u && out.unsigned_value==::std::is_unsigned_v<A>,"native compiler value and extent witness");
  check(out.wide_identity==kind<A>() && s::copied_type_name(out)==name(kind<A>()) && !out.declaration_identity_known,"native decltype rank and copied primitive name; no invented common DIE");
  check(reads==(op<2u?2u:1u) && queries==(op<2u?0u:2u),"only selected conditional value copied");
 } } } }
}
template<typename T> static void native_row()
{ native_pair<T,int>();native_pair<T,unsigned>();native_pair<T,long>();native_pair<T,unsigned long>();native_pair<T,long long>();native_pair<T,unsigned long long>(); }
static void model32()
{
 // Independent ILP32 standard conversion table. Index order: i,u,l,ul,ll,ull.
 constexpr K ks[]{K::signed_int,K::unsigned_int,K::signed_long,K::unsigned_long,K::signed_long_long,K::unsigned_long_long};
 constexpr unsigned table[6u][6u]{{0,1,2,3,4,5},{1,1,3,3,4,5},{2,3,2,3,4,5},{3,3,3,3,4,5},{4,4,4,4,4,5},{5,5,5,5,5,5}};
 for(unsigned i{};i<6u;++i) { for(unsigned j{};j<6u;++j) { for(auto language:{L::c,L::cpp,L::c23})
 {
  auto a{record(ks[i],32u,4u,80u)},b{record(ks[j],32u,8u,160u)};
  for(auto op:{::std::string_view{" + "},::std::string_view{" | "},::std::string_view{" ? "}})
  {
   auto text{op==" ? " ? ::fast_io::concat_std("1 ? (",name(ks[i]),")1 : (",name(ks[j]),")2") : ::fast_io::concat_std("(",name(ks[i]),")1",op,"(",name(ks[j]),")2")};
   s::program p{};s::integer out{};auto never{[](d::source_expression const&,bool,s::integer&) { return false; }};
   check(s::parse(text,p)==s::error::none && s::evaluate(p,never,out,32u,s::details::no_type_resolver{},language)==s::error::none,"ILP32 syntax and result");
   auto k{ks[table[i][j]]};check(out.wide_identity==k && out.width==width(k,32u) && out.unsigned_value==d::c_wide_unsigned(k) && s::copied_type_name(out)==name(k),"independent same-width ILP32 rank table");
   check(out.bits==(op==" ? "?1u:3u),"ILP32 finite value");
  }
 } } }
}
static void literals_and_guards()
{
 struct item { ::std::string_view text{},name32{},name64{}; };
 constexpr item cases[]{
  {"1","int","int"},{"1U","unsigned int","unsigned int"},{"1L","long","long"},{"1UL","unsigned long","unsigned long"},
  {"1LL","long long","long long"},{"1ULL","unsigned long long","unsigned long long"},
  {"2147483648","long long","long"},{"4294967296U","unsigned long long","unsigned long"},
  {"0xffffffffL","unsigned long","long"},{"0xffffffffffffffff","unsigned long long","unsigned long"},
  {"1 ? (long)1 : (unsigned int)2","unsigned long","long"},
  {"(long long)1 + (unsigned long)2","long long","unsigned long long"},
  {"(long)1 << (int)2","long","long"},{"~(unsigned long)1","unsigned long","unsigned long"},
  {"(short)1 + (long)2","long","long"},{"+true","int","int"},
  {"sizeof(int)","unsigned long","unsigned long"},{"(i32)1 + (int)2","signed integer","signed integer"}};
 auto never{[](d::source_expression const&,bool,s::integer&) { return false; }};
 for(auto c:cases) { for(unsigned guest:{32u,64u}) { for(auto language:{L::c,L::cpp,L::c23})
 { s::program p{};s::integer out{};check(s::parse(c.text,p)==s::error::none && s::evaluate(p,never,out,guest,s::details::no_type_resolver{},language)==s::error::none && s::copied_type_name(out)==(guest==32u?c.name32:c.name64),"finite standard literal/cast/shift/promotion names"); } } }
 for(auto text:{::std::string_view{"(int)1"},::std::string_view{"(long)1 + 2"},::std::string_view{"1L"}})
 { s::program p{};s::integer out{};check(s::parse(text,p)==s::error::none && s::evaluate(p,never,out)==s::error::none && out.wide_identity==K::unknown && s::copied_type_name(out)=="signed integer","shared dialect cannot acquire native wide rank"); }
 for(auto text:{::std::string_view{"(long signed int)1"},::std::string_view{"(int unsigned long long)1"},::std::string_view{"(long int long unsigned)1"}})
 { s::program p{};s::integer out{};check(s::parse(text,p)==s::error::none && s::evaluate(p,never,out,32u,s::details::no_type_resolver{},L::cpp)==s::error::none && out.wide_identity!=K::unknown,"standard specifier permutations"); }
 auto a{record(K::signed_long,32u,4u,80u)},b{record(K::signed_int,32u,4u,80u)};::std::size_t reads{};
 auto types{[&](d::source_expression const& e,s::integer& v) { if(e.root_name!="left" && e.root_name!="right") { return false; }v=numeric(e.root_name=="left"?a:b,0u,32u);return true; }};
 auto values{[&](d::source_expression const& e,bool z,s::integer& v) { ++reads;if(z || e.root_name!="left") { return false; }v=numeric(a,1u,32u);return true; }};
 s::program p{};s::integer out{};check(s::parse("1 ? left : right",p)==s::error::none && s::evaluate(p,values,out,32u,types,L::cpp)==s::error::unavailable && reads==0u && out.bits==0u,"same DIE cannot prove conflicting int/long rank");
 b=a;auto mismatch{[&](d::source_expression const& e,bool z,s::integer& v) { bool ok{values(e,z,v)};v.declaration_identity.offset=81u;return ok; }};reads=0u;
 check(s::evaluate(p,mismatch,out,32u,types,L::cpp)==s::error::unavailable && reads==1u && out.bits==0u,"selected wide copied declaration identity mismatch");
 reads=0u;auto missing{[&](d::source_expression const& e,bool z,s::integer& v) { bool ok{values(e,z,v)};v.wide_identity=K::unknown;v.declaration_identity_known=false;return ok; }};
 check(s::evaluate(p,missing,out,32u,types,L::cpp)==s::error::unavailable && reads==1u,"selected copy cannot drop a proven standard rank");
 for(unsigned fault{};fault<9u;++fault)
 { auto t{a};if(fault==0u) { t.language=0x16u; }if(fault==1u) { t.tinygo_producer=true; }if(fault==2u) { t.atomic_scalar=true; }if(fault==3u) { t.kind=d::type_kind::pointer; }
   if(fault==4u) { t.encoding=6u; }if(fault==5u) { t.identity.unit=8u; }if(fault==6u) { t.declaration_identity.offset=0u; }if(fault==7u) { t.wide_builtin=K::unknown; }if(fault==8u) { t.wide_builtin=K::unsigned_long; }
   auto v{s::from_dwarf_numeric(d::numeric_kind::signed_integer,1u,32u)};
   check(!s::attach_standard_integer_type(t,v,32u) && v.wide_identity==K::unknown && !v.declaration_identity_known,"unsupported native metadata cannot invent rank"); }
 auto v{s::from_dwarf_numeric(d::numeric_kind::signed_integer,1u,32u)};
 check(!s::attach_standard_integer_type(a,v,64u) && !v.declaration_identity_known,"Wasm32 long cannot attach on guest64 ABI");
 for(auto k:{K::signed_int,K::signed_long,K::signed_long_long,K::unsigned_int,K::unsigned_long,K::unsigned_long_long})
 { for(unsigned guest:{32u,64u})
  { auto t{record(k,guest,4u,80u)};check(d::c_wide_from_base(t.language,false,name(k),t.encoding,t.byte_count,guest/8u)==k,"bounded actual base name encoding ABI proof");
    check(d::c_wide_from_base(0x0cu,true,name(k),t.encoding,t.byte_count,guest/8u)==K::unknown && d::c_wide_from_base(0x16u,false,name(k),t.encoding,t.byte_count,guest/8u)==K::unknown,"TinyGo and Go exclusion");
    t.name=::fast_io::concat_std("long");t.wide_builtin=K::unknown;auto value{s::from_dwarf_numeric(t.encoding==7u?d::numeric_kind::unsigned_integer:d::numeric_kind::signed_integer,1u,t.byte_count*8u)};
    check(!s::attach_standard_integer_type(t,value,guest),"display name and equal extent alone are not rank evidence");
  } }
 v={1u,32u,false};v.wide_identity=static_cast<K>(255u);check(s::copied_type_name(v).empty(),"invalid wide marker has no name");
 check(s::parse("1 ? (long)1 : ((long)1/0)",p)==s::error::none && s::evaluate(p,never,out,32u,s::details::no_type_resolver{},L::cpp)==s::error::none && out.bits==1u && s::copied_type_name(out)=="long","dead-arm arithmetic is not evaluated for rank");
}
int main(int argc,char** argv)
{
 if(argc>1)
 { auto never{[](d::source_expression const&,bool,s::integer&) { return false; }};
   for(int i=1;i<argc;++i) { s::program p{};s::integer out{};auto parse{s::parse(::std::string_view{argv[i]},p)};
    auto e{parse==s::error::none ? s::evaluate(p,never,out,32u,s::details::no_type_resolver{},L::cpp) : parse};
    ::fast_io::io::println(static_cast<unsigned>(parse),"\t",static_cast<unsigned>(e),"\t",out.bits,"\t",out.width,"\t",out.unsigned_value,"\t",s::copied_type_name(out)); }
   return 0; }
 native_row<int>();native_row<unsigned>();native_row<long>();native_row<unsigned long>();native_row<long long>();native_row<unsigned long long>();model32();literals_and_guards();
 ::fast_io::io::println("debug_source_integer_rank: PASS checks=",checks," native standard and independent ILP32 rank DATA only");
}
