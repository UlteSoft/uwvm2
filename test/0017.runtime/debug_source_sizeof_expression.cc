// Finite copied DATA and native sizeof witnesses; no runtime pause/read grant.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <type_traits>
namespace scalar=uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf=uwvm2::uwvm::debugger::source_dwarf;
static ::std::size_t checks{},reads{},queries{};
static void check(bool value,::std::string_view reason)
{ ++checks;if(!value) { ::fast_io::io::perrln("sizeof expression FAIL: ",reason);::fast_io::fast_terminate(); } }
static bool values(dwarf::source_expression const&,bool,scalar::integer&)
{ ++reads;return false; }
struct types
{
 unsigned guest{32u};
 bool operator()(dwarf::source_expression const& e,bool size,scalar::integer& out) const
 {
  ++queries;
  if(size)
  {
   if(e.root_name=="packet" && e.steps.empty()) { out={28u,guest,true};return true; }
   return false;
  }
  if(!e.steps.empty())
  {
   if(e.root_name=="p" && e.steps.size()==1u && e.steps[0u].kind==dwarf::source_expression_step_kind::dereference)
   { out=scalar::from_dwarf_numeric(dwarf::numeric_kind::signed_integer,0u,32u);return true; }
   if(e.root_name=="record" && e.steps.size()==1u && e.steps[0u].kind==dwarf::source_expression_step_kind::member && e.steps[0u].member=="field")
   { out=scalar::from_dwarf_numeric(dwarf::numeric_kind::signed_integer,0u,16u);out.builtin_identity=scalar::narrow_builtin::signed_short;return true; }
   return false;
  }
  if(e.root_name=="left" || e.root_name=="right")
  { out=scalar::from_dwarf_numeric(dwarf::numeric_kind::signed_integer,0u,32u);out.wide_identity=scalar::wide_builtin::signed_int;return true; }
  if(e.root_name=="short_value")
  { out=scalar::from_dwarf_numeric(dwarf::numeric_kind::signed_integer,0u,16u);out.builtin_identity=scalar::narrow_builtin::signed_short;out.declaration_identity={7u,99u};out.declaration_identity_known=true;return true; }
  if(e.root_name=="flag" || e.root_name=="off")
  { out=scalar::from_dwarf_numeric(dwarf::numeric_kind::boolean,0u,8u);return true; }
  if(e.root_name=="real32" || e.root_name=="real64")
  { out={0u,e.root_name=="real32"?32u:64u,false,true};return true; }
  if(e.root_name=="bad") { out={0u,3u,false};return true; }
  if(e.root_name=="wronglong")
  { out={0u,64u,false};out.wide_identity=scalar::wide_builtin::signed_long;return true; }
  return false;
 }
};
static scalar::error probe(::std::string_view text,scalar::integer& out,unsigned guest,scalar::language_semantics language)
{
 reads=queries=0u;scalar::program code{};
 auto const parsed{scalar::parse(text,code)};
 return parsed==scalar::error::none?scalar::evaluate(code,values,out,guest,types{guest},language):parsed;
}
static void size(::std::string_view text,::std::uint64_t expected,unsigned guest=32u,scalar::language_semantics language=scalar::language_semantics::cpp)
{
 scalar::integer out{};auto const status{probe(text,out,guest,language)};
 if(status!=scalar::error::none) { ::fast_io::io::perrln("operand=",text," status=",static_cast<unsigned>(status)); }
 check(status==scalar::error::none,text);check(out.bits==expected && out.width==guest && out.unsigned_value && !out.floating,"guest-sized unsigned sizeof value");
 check(reads==0u,"sizeof expression must not call any value/sizeof resolver");
 check(out.wide_identity==(language==scalar::language_semantics::shared_numeric?scalar::wide_builtin::unknown:scalar::wide_builtin::unsigned_long) && out.builtin_identity==scalar::narrow_builtin::unknown && !out.declaration_identity_known,"Wasm size_t rank without an operand declaration key");
}
template<typename T> constexpr ::std::string_view name()
{
 if constexpr(::std::is_same_v<T,int>)return "int";
 else if constexpr(::std::is_same_v<T,unsigned int>)return "unsigned int";
 else if constexpr(::std::is_same_v<T,long>)return "long";
 else if constexpr(::std::is_same_v<T,unsigned long>)return "unsigned long";
 else if constexpr(::std::is_same_v<T,long long>)return "long long";
 else return "unsigned long long";
}
template<typename T,typename U> static void native_pair()
{
 constexpr unsigned guest{sizeof(void*)*8u};
 for(auto language:{scalar::language_semantics::c,scalar::language_semantics::cpp,scalar::language_semantics::c23})
 {
  for(::std::string_view op:{"+","-","*","/","%","|","&","^"})
  { size(::fast_io::concat_std("sizeof((",name<T>(),")1",op,"(",name<U>(),")2)"),sizeof(decltype(T{}+U{})),guest,language);check(queries==0u,"native literal witness needs no guest metadata"); }
  size(::fast_io::concat_std("sizeof(1 ? (",name<T>(),")1 : (",name<U>(),")2)"),sizeof(decltype(true?T{}:U{})),guest,language);
  size(::fast_io::concat_std("sizeof((",name<T>(),")1 << (",name<U>(),")2)"),sizeof(T),guest,language);
 }
}
template<typename T> static void native_row()
{ native_pair<T,int>();native_pair<T,unsigned int>();native_pair<T,long>();native_pair<T,unsigned long>();native_pair<T,long long>();native_pair<T,unsigned long long>(); }
static ::std::string balanced(unsigned n)
{ return n==1u ? ::fast_io::concat_std("1") : ::fast_io::concat_std("(",balanced(n/2u),"+",balanced(n-n/2u),")"); }
int main(int argc,char const* const* argv)
{
 if(argc>1)
 {
  for(int i{1};i<argc;++i)
  {
   auto const input{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])}};scalar::integer out{};
   auto const status{probe({input.data(),input.size()},out,32u,scalar::language_semantics::cpp)};
   ::fast_io::io::println(i-1,"\t",static_cast<unsigned>(status),"\t",out.bits,"\t",out.width,"\t",out.unsigned_value?1u:0u,"\t",reads);
  }
  return 0;
 }
 static_assert(sizeof(int)==4u && sizeof(long long)==8u && sizeof(long)==sizeof(void*));
 native_row<int>();native_row<unsigned int>();native_row<long>();native_row<unsigned long>();native_row<long long>();native_row<unsigned long long>();
 for(unsigned guest:{32u,64u})for(auto language:{scalar::language_semantics::shared_numeric,scalar::language_semantics::c,scalar::language_semantics::cpp,scalar::language_semantics::c23})
 {
  for(auto text:{"sizeof(1+2)","sizeof(1/0)","sizeof(1%0)","sizeof(1<<-1)","sizeof(1<<99)","sizeof(2147483647+1)",
                 "sizeof(left+right)","sizeof(left/0)","sizeof(left<<-1)","sizeof((int)1e300)","sizeof('('+')')", "sizeof('\\''+1)","sizeof('\\\\'+1)",
                 "sizeof(1'0+(short)left)","sizeof(record.field+1)","sizeof(*p+1)"})
  { size(text,4u,guest,language); }
  size("sizeof((short)left)",2u,guest,language);
  size("sizeof((unsigned char)left)",1u,guest,language);
  size("sizeof((bool)left)",1u,guest,language);
  size("sizeof((long)left)",guest/8u,guest,language);
  size("sizeof((long long)left)",8u,guest,language);
  size("sizeof((float)left)",4u,guest,language);
  size("sizeof(real32+real64)",8u,guest,language);
  size("sizeof(-(-9223372036854775807-1))",8u,guest,language);
  size("sizeof(sizeof(packet)+1)",guest/8u,guest,language);check(queries==1u,"nested aggregate sizeof asks only declared extent");
  size("sizeof(sizeof(1/0))",guest/8u,guest,language);
  size("sizeof(true)",language==scalar::language_semantics::cpp || language==scalar::language_semantics::c23?1u:4u,guest,language);
  size("sizeof((false))",language==scalar::language_semantics::cpp || language==scalar::language_semantics::c23?1u:4u,guest,language);
  size("sizeof('a')",language==scalar::language_semantics::cpp?1u:4u,guest,language);
  size("sizeof(!left)",language==scalar::language_semantics::cpp?1u:4u,guest,language);
  size("sizeof(left<right)",language==scalar::language_semantics::cpp?1u:4u,guest,language);
  if(language!=scalar::language_semantics::shared_numeric)
  {
   size("sizeof(1 ? short_value : short_value)",language==scalar::language_semantics::cpp?2u:4u,guest,language);
   size("sizeof(1 ? flag : off)",language==scalar::language_semantics::cpp?1u:4u,guest,language);
  }
  else
  {
   scalar::integer out{};check(probe("sizeof(1 ? short_value : short_value)",out,guest,language)==scalar::error::unsupported && reads==0u,"shared API retains ambiguous narrow conditional refusal");
   check(probe("sizeof(1 ? flag : off)",out,guest,language)==scalar::error::unsupported && reads==0u,"shared API retains Boolean conditional refusal");
  }
  for(auto text:{"sizeof(1||missing)","sizeof((int)missing)","sizeof(left+missing)","sizeof(+bad)"})
  { scalar::integer out{};check(probe(text,out,guest,language)==scalar::error::unavailable,"missing or invalid declaration fails before value reads");check(reads==0u,"unavailable sizeof makes no value calls"); }
 }
 { scalar::integer out{};check(probe("sizeof(wronglong+1)",out,32u,scalar::language_semantics::cpp)==scalar::error::unavailable && reads==0u,"guest ABI checked during type inference"); }
 size("sizeof(wronglong+1)",8u,64u);
 for(auto text:{"sizeof(call())","sizeof(left+call())","sizeof(left=2)","sizeof(++left)","sizeof(left++)","sizeof(*(left+1))","sizeof(1,2)","sizeof((left+1).field)","sizeof(left+2) trailing","sizeof()"})
 { scalar::integer out{};check(probe(text,out,32u,scalar::language_semantics::cpp)!=scalar::error::none && reads==0u && queries==0u,"unsupported syntax never invokes either callback"); }
 {
  scalar::program code{};scalar::integer out{};reads=queries=0u;
  check(scalar::parse("sizeof(left+1)",code)==scalar::error::none,"missing type callback probe syntax");
  check(scalar::evaluate(code,values,out)==scalar::error::unavailable && reads==0u,"no implicit fallback from type to value callback");
 }
 {
  auto text{::fast_io::concat_std("sizeof(",balanced(64u),")")};size(text,4u);scalar::program code{};
  check(scalar::parse(text,code)==scalar::error::none && code.nodes.size()==128u,"sizeof AST shares original exact node cap");
  text=::fast_io::concat_std("sizeof(",balanced(65u),")");check(scalar::parse(text,code)==scalar::error::limit_exceeded && code.nodes.empty(),"oversized sizeof AST refused atomically");
 }
 {
  scalar::program code{};scalar::integer out{};check(scalar::parse("sizeof(1+2)",code)==scalar::error::none,"cycle probe syntax");
  code.nodes[code.root].lhs=code.root;reads=queries=0u;
  check(scalar::evaluate(code,values,out,32u,types{})==scalar::error::limit_exceeded && reads==0u && queries==0u,"hostile DATA cycle uses bounded type recursion");
 }
 ::fast_io::io::println("debug_source_sizeof_expression: PASS checks=",checks);
}
