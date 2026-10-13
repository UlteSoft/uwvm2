// Standard Wasm C ABI size_t rank; copied DATA, no guest read authority.
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
#include <type_traits>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
namespace d=uwvm2::uwvm::debugger::source_dwarf;
using K=s::wide_builtin;using L=s::language_semantics;
static ::std::size_t checks{},reads{},extents{},queries{};
static void check(bool ok,::std::string_view why)
{ ++checks;if(!ok) { ::fast_io::io::perrln("size type FAIL: ",why);::fast_io::fast_terminate(); } }
struct query
{
 unsigned guest;
 bool operator()(d::source_expression const& e,bool size,s::integer& out) const
 {
  ++queries;if(!e.steps.empty() || e.root_name!="value")return false;
  out=size?s::integer{28u,guest,true}:s::from_dwarf_numeric(d::numeric_kind::signed_integer,0u,32u);
  if(!size)out.wide_identity=K::signed_int;
  return true;
 }
};
struct copy
{
 unsigned guest;
 bool operator()(d::source_expression const& e,bool size,s::integer& out) const
 {
  if(!size) { ++reads;return false; }
  ++extents;if(!e.steps.empty() || e.root_name!="value")return false;
  // An extent has no operand-value/DIE capability. Do not propagate this key.
  out={28u,guest,true,false,s::value_category::integer};out.wide_identity=K::unsigned_long;out.declaration_identity={7u,99u};out.declaration_identity_known=true;return true;
 }
};
static s::error probe(::std::string_view text,s::integer& out,unsigned guest,L language)
{
 reads=extents=queries=0u;s::program p{};auto e=s::parse(text,p);
 return e==s::error::none?s::evaluate(p,copy{guest},out,guest,query{guest},language):e;
}
struct witness { ::std::string_view text;K type32,type64;::std::uint64_t bits32,bits64;unsigned extent_calls{},type_calls{}; };
static constexpr witness cases[]{
 {"sizeof(int)",K::unsigned_long,K::unsigned_long,4u,4u},
 {"sizeof(1 / 0)",K::unsigned_long,K::unsigned_long,4u,4u},
 {"sizeof(value)",K::unsigned_long,K::unsigned_long,28u,28u,1u},
 {"sizeof(value + 1)",K::unsigned_long,K::unsigned_long,4u,4u,0u,1u},
 {"sizeof(sizeof(value))",K::unsigned_long,K::unsigned_long,4u,8u,0u,1u},
 {"sizeof(sizeof(value) + (long)1)",K::unsigned_long,K::unsigned_long,4u,8u,0u,1u},
 {"(size_t)1",K::unsigned_long,K::unsigned_long,1u,1u},
 {"sizeof(int) + (long)1",K::unsigned_long,K::unsigned_long,5u,5u},
 {"sizeof(int) + (unsigned int)1",K::unsigned_long,K::unsigned_long,5u,5u},
 {"sizeof(int) + (long long)1",K::signed_long_long,K::unsigned_long_long,5u,5u},
 {"sizeof(int) + (unsigned long long)1",K::unsigned_long_long,K::unsigned_long_long,5u,5u},
 {"1 ? sizeof(int) : (long)1",K::unsigned_long,K::unsigned_long,4u,4u},
 {"0 ? sizeof(int) : (long)1",K::unsigned_long,K::unsigned_long,1u,1u},
 {"1 ? sizeof(int) : (long long)1",K::signed_long_long,K::unsigned_long_long,4u,4u},
 {"0 ? sizeof(int) : (long long)1",K::signed_long_long,K::unsigned_long_long,1u,1u},
 {"sizeof(int) << 1",K::unsigned_long,K::unsigned_long,8u,8u},
 {"~(size_t)0",K::unsigned_long,K::unsigned_long,0xffffffffu,0xffffffffffffffffu},
 {"-sizeof(int)",K::unsigned_long,K::unsigned_long,0xfffffffcu,0xfffffffffffffffcu},
 {"sizeof(int) % 3",K::unsigned_long,K::unsigned_long,1u,1u},
 {"sizeof(int) | 1",K::unsigned_long,K::unsigned_long,5u,5u},
 {"1 ? sizeof(int) : sizeof(1 / 0)",K::unsigned_long,K::unsigned_long,4u,4u},
 {"1 ? sizeof(int) : sizeof(value)",K::unsigned_long,K::unsigned_long,4u,4u,0u,1u}
};
static unsigned width(K k,unsigned guest)
{ return k==K::signed_long_long || k==K::unsigned_long_long?64u:guest; }
static void compiler_witnesses()
{
 // Native LP64 compiler witnesses supplement the independently pinned Wasm
 // ILP32/LP64 producer witnesses. Host sizeof is not a guest ABI selector.
 static_assert(::std::is_same_v<decltype(sizeof(int)),decltype(sizeof(void*))>);
 using T=unsigned long;
 static_assert(::std::is_same_v<decltype(T{}+long{}),T>);
 static_assert(::std::is_same_v<decltype(true?T{}:long{}),T>);
 static_assert(::std::is_same_v<decltype(T{}+static_cast<unsigned int>(0)),T>);
 if constexpr(sizeof(long)==8u)
 {
  static_assert(sizeof(long)!=8u || ::std::is_same_v<decltype(T{}+static_cast<long long>(0)),unsigned long long>);
  static_assert(sizeof(long)!=8u || ::std::is_same_v<decltype(true?T{}:static_cast<long long>(0)),unsigned long long>);
 }
}
int main()
{
 compiler_witnesses();
 for(unsigned repeat{};repeat<512u;++repeat)for(unsigned guest:{32u,64u})for(auto lang:{L::c,L::cpp,L::c23})for(auto c:cases)
 {
  s::integer out{};check(probe(c.text,out,guest,lang)==s::error::none,c.text);
  K const k{guest==32u?c.type32:c.type64};
  check(out.bits==(guest==32u?c.bits32:c.bits64) && out.width==width(k,guest) && out.unsigned_value==d::c_wide_unsigned(k) && !out.floating,"independent Wasm extent/value/conversion table");
  check(out.wide_identity==k && out.category!=s::value_category::boolean && out.builtin_identity==s::narrow_builtin::unknown && !out.declaration_identity_known,"canonical result rank without invented operand DIE");
  check(reads==0u && extents==c.extent_calls && queries==c.type_calls,"only required metadata extent/type callback; no operand value access");
 }
 for(unsigned guest:{32u,64u})
 {
  for(auto text:{"sizeof(int)","sizeof(1+2)","sizeof(value)","(size_t)1"})
  { s::integer out{};check(probe(text,out,guest,L::shared_numeric)==s::error::none && out.wide_identity==K::unknown,"shared/TinyGo/unknown ABI does not acquire a native rank"); }
  for(auto lang:{L::c,L::cpp,L::c23})
  {
   for(auto text:{"0 && sizeof(value)","1 || sizeof(value)"})
   { s::integer out{};check(probe(text,out,guest,lang)==s::error::none && reads==0u && extents==0u && queries==1u,"dead sizeof type queried; extent resolver not invoked"); }
   for(unsigned fault{};fault<4u;++fault)
   {
    s::program p{};s::integer out{};check(s::parse("sizeof(value)",p)==s::error::none,"invalid extent syntax");
    auto invalid{[&](d::source_expression const&,bool,s::integer& v)
    { v={28u,guest,true};if(fault==0u)v.width=guest==32u?64u:32u;if(fault==1u)v.unsigned_value=false;
      if(fault==2u)v.floating=true;if(fault==3u)v.category=s::value_category::boolean;return true; }};
    check(s::evaluate(p,invalid,out,guest,query{guest},lang)==s::error::unavailable && out.bits==0u,"wrong width/sign/floating/Boolean extent refused");
   }
   for(auto text:{"sizeof(missing)","sizeof(value+missing)","1 ? sizeof(int) : sizeof(missing)","0 && sizeof(missing)"})
   { s::integer out{};check(probe(text,out,guest,lang)==s::error::unavailable && reads==0u,"missing declaration cannot mint size_t"); }
  }
 }
 check(s::language_from_dwarf(0x0cu,true)==L::shared_numeric && s::language_from_dwarf(0x16u,false)==L::shared_numeric,"TinyGo and Go remain excluded");
 ::fast_io::io::println("debug_source_size_type: PASS checks=",checks," finite standard Wasm C ABI only");
}
