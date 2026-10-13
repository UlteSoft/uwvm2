// Native witnesses and finite declaration DATA; no runtime pause/read grant.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <type_traits>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
namespace d=uwvm2::uwvm::debugger::source_dwarf;
static ::std::size_t checks{},value_reads{},extent_queries{},type_queries{};
static void check(bool ok,::std::string_view why)
{ ++checks;if(!ok) { ::fast_io::io::perrln("sizeof unary FAIL: ",why);::fast_io::fast_terminate(); } }
static bool declaration(d::source_expression const& e,bool size,s::integer& out,unsigned guest)
{
 unsigned bytes{};s::integer type{};
 if(e.steps.empty())
 {
  if(e.root_name=="packet")bytes=28u;
  else if(e.root_name=="array")bytes=10u;
  else if(e.root_name=="p")bytes=guest/8u;
  else if(e.root_name=="value" || e.root_name=="outside")
  { bytes=4u;type=s::from_dwarf_numeric(d::numeric_kind::signed_integer,0u,32u);type.wide_identity=s::wide_builtin::signed_int; }
  else if(e.root_name=="short_value")
  { bytes=2u;type=s::from_dwarf_numeric(d::numeric_kind::signed_integer,0u,16u);type.builtin_identity=s::narrow_builtin::signed_short; }
  else if(e.root_name=="flag")
  { bytes=1u;type=s::from_dwarf_numeric(d::numeric_kind::boolean,0u,8u); }
  else if(e.root_name=="real32" || e.root_name=="real64")
  { bytes=e.root_name=="real32"?4u:8u;type={0u,bytes*8u,false,true}; }
  else if(e.root_name=="bad" && !size) { out={0u,3u,false};return true; }
  else return false; // absent/function/incomplete/bit-field DATA stays unavailable.
 }
 else if(e.steps.size()==1u)
 {
  auto const& step{e.steps[0u]};
  if(e.root_name=="p" && step.kind==d::source_expression_step_kind::dereference)
  { bytes=4u;type=s::from_dwarf_numeric(d::numeric_kind::signed_integer,0u,32u);type.wide_identity=s::wide_builtin::signed_int; }
  else if((e.root_name=="array" && step.kind==d::source_expression_step_kind::index && step.index>=0 && step.index<5) ||
          (e.root_name=="record" && step.kind==d::source_expression_step_kind::member && step.member=="field"))
  { bytes=2u;type=s::from_dwarf_numeric(d::numeric_kind::signed_integer,0u,16u);type.builtin_identity=s::narrow_builtin::signed_short; }
  else return false;
 }
 else return false;
 if(size) { out={bytes,guest,true};return true; }
 if(type.width==0u || (e.steps.empty() && (e.root_name=="packet" || e.root_name=="array" || e.root_name=="p")))return false;
 out=type;return true;
}
struct type_query
{
 unsigned guest;
 bool operator()(d::source_expression const& e,bool size,s::integer& out) const
 { ++type_queries;return declaration(e,size,out,guest); }
};
struct value_copy
{
 unsigned guest;
 bool operator()(d::source_expression const& e,bool size,s::integer& out) const
 {
  if(size) { ++extent_queries;return declaration(e,true,out,guest); }
  ++value_reads;
  if(e.root_name=="outside" && e.steps.empty()) { out={42u,32u,false};out.wide_identity=s::wide_builtin::signed_int;return true; }
  return false; // every actual operand value is unavailable.
 }
};
static s::error probe(::std::string_view text,s::integer& out,unsigned guest,s::language_semantics language)
{
 value_reads=extent_queries=type_queries=0u;s::program code{};auto const parsed{s::parse(text,code)};
 return parsed==s::error::none?s::evaluate(code,value_copy{guest},out,guest,type_query{guest},language):parsed;
}
static void result(::std::string_view text,::std::uint64_t bits,unsigned guest,s::language_semantics language,unsigned extents=0u)
{
 s::integer out{};auto const status{probe(text,out,guest,language)};
 if(status!=s::error::none) { ::fast_io::io::perrln("operand=",text," status=",static_cast<unsigned>(status)); }
 check(status==s::error::none,text);check(out.bits==bits && out.width==guest && out.unsigned_value && !out.floating,"guest-sized sizeof result");
 check(value_reads==0u && extent_queries==extents,"only existing declaration extent path; never operand value reads");
 check(out.wide_identity==(language==s::language_semantics::shared_numeric?s::wide_builtin::unknown:s::wide_builtin::unsigned_long) && out.builtin_identity==s::narrow_builtin::unknown && !out.declaration_identity_known,"Wasm size_t rank without an operand declaration key");
}
template<typename T> constexpr ::std::string_view name()
{
 if constexpr(::std::is_same_v<T,int>)return "int";
 else if constexpr(::std::is_same_v<T,unsigned int>)return "unsigned int";
 else if constexpr(::std::is_same_v<T,long>)return "long";
 else if constexpr(::std::is_same_v<T,unsigned long>)return "unsigned long";
 else if constexpr(::std::is_same_v<T,long long>)return "long long";
 else if constexpr(::std::is_same_v<T,unsigned long long>)return "unsigned long long";
 else if constexpr(::std::is_same_v<T,char>)return "char";
 else if constexpr(::std::is_same_v<T,signed char>)return "signed char";
 else if constexpr(::std::is_same_v<T,unsigned char>)return "unsigned char";
 else if constexpr(::std::is_same_v<T,short>)return "short";
 else if constexpr(::std::is_same_v<T,unsigned short>)return "unsigned short";
 else if constexpr(::std::is_same_v<T,bool>)return "bool";
 else if constexpr(::std::is_same_v<T,float>)return "float";
 else return "double";
}
template<typename T> static void native()
{
 constexpr unsigned guest{sizeof(void*)*8u};
 for(auto language:{s::language_semantics::c,s::language_semantics::cpp,s::language_semantics::c23})
 {
  result(::fast_io::concat_std("sizeof + (",name<T>(),")1"),sizeof(+T{}),guest,language);
  result(::fast_io::concat_std("sizeof - (",name<T>(),")1"),sizeof(-T{}),guest,language);
  result(::fast_io::concat_std("sizeof ! (",name<T>(),")1"),language==s::language_semantics::cpp?sizeof(!T{}):sizeof(int),guest,language);
  if constexpr(::std::is_integral_v<T> && !::std::is_same_v<T,bool>)result(::fast_io::concat_std("sizeof ~ (",name<T>(),")1"),sizeof(~T{}),guest,language);
  result(::fast_io::concat_std("sizeof sizeof + (",name<T>(),")1"),sizeof(sizeof +T{}),guest,language);
 }
}
int main(int argc,char const* const* argv)
{
 if(argc>1)
 {
  for(int i{1};i<argc;++i)
  {
   auto const input{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])}};s::integer out{};
   auto const status{probe({input.data(),input.size()},out,32u,s::language_semantics::cpp)};
   ::fast_io::io::println(i-1,"\t",static_cast<unsigned>(status),"\t",out.bits,"\t",out.width,"\t",out.unsigned_value?1u:0u,"\t",value_reads,"\t",extent_queries);
  }
  return 0;
 }
 static_assert(sizeof(int)==4u && sizeof(long)==sizeof(void*) && sizeof(long long)==8u && sizeof(bool)==1u);
 native<int>();native<unsigned>();native<long>();native<unsigned long>();native<long long>();native<unsigned long long>();
 native<char>();native<signed char>();native<unsigned char>();native<short>();native<unsigned short>();native<bool>();native<float>();native<double>();
 for(unsigned repeat{};repeat!=200u;++repeat)
 for(unsigned guest:{32u,64u})
 for(auto language:{s::language_semantics::shared_numeric,s::language_semantics::c,s::language_semantics::cpp,s::language_semantics::c23})
 {
  result("sizeof 1",4u,guest,language);result("sizeof -1",4u,guest,language);result("sizeof ~value",4u,guest,language);
  result("sizeof +short_value",4u,guest,language);result("sizeof -real64",8u,guest,language);
  result("sizeof !flag",language==s::language_semantics::cpp?1u:4u,guest,language);
  result("sizeof true",language==s::language_semantics::cpp || language==s::language_semantics::c23?1u:4u,guest,language);
  result("sizeof '('",language==s::language_semantics::cpp?1u:4u,guest,language);
  result("sizeof -1 + 2",6u,guest,language);result("sizeof -1 * 2",8u,guest,language);
  result("sizeof sizeof +value",guest/8u,guest,language);result("sizeof sizeof packet",guest/8u,guest,language);
  result("sizeof +*p",4u,guest,language);result("sizeof +array[1]",4u,guest,language);
  result("sizeof value",4u,guest,language,1u);result("sizeof p",guest/8u,guest,language,1u);
  result("sizeof *p",4u,guest,language,1u);result("sizeof array",10u,guest,language,1u);
  result("sizeof array[1]",2u,guest,language,1u);result("sizeof record.field",2u,guest,language,1u);
  result("sizeof packet + 1",29u,guest,language,1u);
  s::integer out{};
  check(probe("sizeof -value + outside",out,guest,language)==s::error::none && out.bits==46u && value_reads==1u && extent_queries==0u,"outside binary operand remains evaluated exactly once");
  check(probe("sizeof -value / 0",out,guest,language)==s::error::arithmetic && value_reads==0u,"outside division is evaluated and refused");
  check(probe("sizeof +value << -1",out,guest,language)==s::error::arithmetic && value_reads==0u,"outside shift remains evaluated");
  for(::std::string_view text:{"sizeof +missing","sizeof +bad","sizeof bitfield","sizeof function","sizeof incomplete","sizeof +packet"})
  { check(probe(text,out,guest,language)!=s::error::none && value_reads==0u && out.bits==0u,"absent/invalid/aggregate scalar type does not gain a size or value grant"); }
 }
 for(::std::string_view text:{"sizeof","sizeof +","sizeof ++value","sizeof value++","sizeof --value","sizeof value--","sizeof call()","sizeof *(value+1)","sizeof *0x1000","sizeof value;continue","sizeof...value","sizeof 1 ? 2 : call()","sizeof.field","*sizeof","(sizeof)","sizeof::field"})
 { s::program p{};check(s::parse(text,p)!=s::error::none,"unsafe complete unary grammar refusal"); }
 s::program code{};s::integer out{};
 check(s::parse("sizeof -value",code)==s::error::none,"unary size without a type callback syntax");value_reads=extent_queries=0u;
 check(s::evaluate(code,value_copy{32u},out,32u,s::details::no_type_resolver{},s::language_semantics::cpp)==s::error::unavailable && value_reads==0u && extent_queries==0u,"no type callback never falls back to reading operand values");
 ::std::string deep{};for(unsigned i{};i!=40u;++i)deep=::fast_io::concat_std(deep,"sizeof ");deep=::fast_io::concat_std(deep,"1");
 check(s::parse(deep,code)==s::error::limit_exceeded,"unary sizeof recursion stays bounded");
 check(s::parse("sizeof -1 ? 2 : 3",code)==s::error::none && s::evaluate(code,value_copy{32u},out,32u,type_query{32u},s::language_semantics::cpp)==s::error::none && out.bits==2u,"conditional remains outside the unary sizeof operand");
 ::fast_io::io::println("debug_source_sizeof_unary: PASS checks=",checks," native sizeof/unary precedence and finite declaration DATA only");
}
