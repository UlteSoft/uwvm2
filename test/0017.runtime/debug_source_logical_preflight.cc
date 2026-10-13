// Native compiler truth/type witnesses and copied declaration DATA. No stop lease.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <type_traits>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
namespace d=s::dwarf;
using L=s::language_semantics;
static ::std::size_t checks{},reads{},queries{},rhs_reads{};
static void check(bool v,::std::string_view why)
{ ++checks;if(!v) { ::fast_io::io::perrln("logical preflight FAIL: ",why);::fast_io::fast_terminate(); } }
template<typename T> static s::integer copied(T v)
{
 if constexpr(::std::is_same_v<T,bool>)return s::from_dwarf_numeric(d::numeric_kind::boolean,v,8u);
 else if constexpr(::std::is_same_v<T,float>)return s::from_dwarf_numeric(d::numeric_kind::f32_bits,::std::bit_cast<::std::uint32_t>(v),32u);
 else if constexpr(::std::is_same_v<T,double>)return s::from_dwarf_numeric(d::numeric_kind::f64_bits,::std::bit_cast<::std::uint64_t>(v),64u);
 else
 {
  auto out{s::from_dwarf_numeric(::std::is_unsigned_v<T>?d::numeric_kind::unsigned_integer:d::numeric_kind::signed_integer,
      static_cast<::std::uint64_t>(v)&s::details::mask(sizeof(T)*8u),sizeof(T)*8u)};
  if constexpr(::std::is_same_v<T,char>)
  {
   // The finite plain-char builtin identity proves a signed Wasm base.
   // On an unsigned-char host retain representation + canonical DATA only;
   // native host signedness cannot manufacture that guest builtin identity.
   if constexpr(!::std::is_unsigned_v<T>)out.builtin_identity=s::narrow_builtin::plain_char;
  }
  else if constexpr(::std::is_same_v<T,signed char>)out.builtin_identity=s::narrow_builtin::signed_char;
  else if constexpr(::std::is_same_v<T,unsigned char>)out.builtin_identity=s::narrow_builtin::unsigned_char;
  else if constexpr(::std::is_same_v<T,short>)out.builtin_identity=s::narrow_builtin::signed_short;
  else if constexpr(::std::is_same_v<T,unsigned short>)out.builtin_identity=s::narrow_builtin::unsigned_short;
  else if constexpr(::std::is_same_v<T,int>)out.wide_identity=s::wide_builtin::signed_int;
  else if constexpr(::std::is_same_v<T,unsigned int>)out.wide_identity=s::wide_builtin::unsigned_int;
  else if constexpr(::std::is_same_v<T,long>)out.wide_identity=s::wide_builtin::signed_long;
  else if constexpr(::std::is_same_v<T,unsigned long>)out.wide_identity=s::wide_builtin::unsigned_long;
  else if constexpr(::std::is_same_v<T,long long>)out.wide_identity=s::wide_builtin::signed_long_long;
  else if constexpr(::std::is_same_v<T,unsigned long long>)out.wide_identity=s::wide_builtin::unsigned_long_long;
  out.declaration_identity={9u,sizeof(T)*32u+static_cast<unsigned>(out.wide_identity)*2u+static_cast<unsigned>(out.builtin_identity)};
  out.declaration_identity_known=true;return out;
 }
}
__attribute__((noinline)) static void native_probe(s::integer left,s::integer right,bool a,bool expected_and,bool expected_or)
{
 for(L language:{L::c,L::cpp,L::c23})
 {
  auto types{[&](d::source_expression const& e,s::integer& out)
  { ++queries;if(!e.steps.empty() || (e.root_name!="left" && e.root_name!="right"))return false;out=e.root_name=="left"?left:right;out.bits=0u;return true; }};
  for(bool conjunction:{false,true})
  {
   reads=queries=rhs_reads=0u;s::program code{};s::integer out{};
   bool const skip{conjunction?!static_cast<bool>(a):static_cast<bool>(a)};
   auto values{[&](d::source_expression const& e,bool size,s::integer& value)
   { ++reads;if(size)return false;if(e.root_name=="left") { value=left;return true; }
     ++rhs_reads;if(skip)return false;value=right;return true; }};
   auto text{conjunction?::std::string_view{"left && right"} : ::std::string_view{"left || right"}};
   check(s::parse(text,code)==s::error::none && s::evaluate(code,values,out,sizeof(void*)*8u,types,language)==s::error::none,text);
   bool const expected{conjunction?expected_and:expected_or};
   check(out.bits==expected && out.width==(language==L::cpp?8u:32u) && out.unsigned_value==(language==L::cpp) &&
      out.category==(language==L::cpp?s::value_category::boolean:s::value_category::unspecified) &&
      (language==L::cpp || (out.wide_identity==s::wide_builtin::signed_int && s::copied_type_name(out)=="int")),"native compiler truth/result type");
   check(reads==(skip?1u:2u) && rhs_reads==(skip?0u:1u) && queries==2u,"both declarations, selected values in order");
  }
 }
}
template<typename T,typename U> static void native_pair()
{
 static_assert(::std::is_same_v<decltype(T{} && U{}),bool> && ::std::is_same_v<decltype(T{} || U{}),bool>);
 for(T a:{T(0),T(1),T(-1)})for(U b:{U(0),U(2),U(-1)})native_probe(copied(a),copied(b),static_cast<bool>(a),static_cast<bool>(a && b),static_cast<bool>(a || b));
}
template<typename T> static void native_row()
{
 native_pair<T,bool>();native_pair<T,char>();native_pair<T,signed char>();native_pair<T,unsigned char>();
 native_pair<T,short>();native_pair<T,unsigned short>();native_pair<T,int>();native_pair<T,unsigned int>();
 native_pair<T,long>();native_pair<T,unsigned long>();native_pair<T,long long>();native_pair<T,unsigned long long>();
 native_pair<T,float>();native_pair<T,double>();
}
static s::error constants(::std::string_view text,unsigned guest,L language,s::integer& out)
{
 reads=0u;s::program code{};auto values{[](d::source_expression const&,bool,s::integer&) { ++reads;return false; }};
 auto status{s::parse(text,code)};return status==s::error::none?s::evaluate(code,values,out,guest,s::details::no_type_resolver{},language):status;
}
int main(int argc,char const* const* argv)
{
 if(argc>1)
 {
  for(int i{1};i<argc;++i)
  {
   auto const input{::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])}};s::integer out{};
   auto status{constants({input.data(),input.size()},32u,L::cpp,out)};
   ::fast_io::io::println(i-1,"\t",static_cast<unsigned>(status),"\t",out.bits,"\t",out.width,"\t",reads);
  }
  return 0;
 }
 for(unsigned repeat{};repeat!=16u;++repeat)
 {
  native_row<bool>();native_row<char>();native_row<signed char>();native_row<unsigned char>();
  native_row<short>();native_row<unsigned short>();native_row<int>();native_row<unsigned int>();
  native_row<long>();native_row<unsigned long>();native_row<long long>();native_row<unsigned long long>();
  native_row<float>();native_row<double>();
 }
 for(unsigned guest:{32u,64u})for(L language:{L::c,L::cpp,L::c23})
 {
  for(auto text:{"0 && (1/0)","1 || (1/0)","0 && (1<<-1)","1 || (2147483647+1)",
                "0 && (0 ? 1/0 : 2)","1 || sizeof(1/0)","0 && (1 || (1/0))"})
  { s::integer out{};check(constants(text,guest,language,out)==s::error::none && reads==0u,"valid dead arithmetic is never executed"); }
  for(auto text:{"0 && (1.0%1)","1 || (1.0<<1)","0 && ~1.0","1 || (1.0|1)",
                "0 && (0 && (1.0%1))","1 || (0 ? 1 : (1.0%1))","0 && sizeof(1.0%1)"})
  { s::integer out{99u};check(constants(text,guest,language,out)==s::error::unsupported && out.bits==0u && reads==0u,"invalid dead type is rejected before values"); }
  for(auto text:{"0 && missing","1 || missing"})
  { s::integer out{99u};check(constants(text,guest,language,out)==s::error::unavailable && out.bits==0u && reads==0u,"unknown declaration has no value fallback"); }
  auto original{copied(1)};auto type{[&](d::source_expression const&,s::integer& out) { ++queries;out=original;out.bits=0u;return true; }};
  for(unsigned change{};change!=7u;++change)for(bool right:{false,true})
  {
   auto mismatched{original};
   switch(change)
   { case 0u:mismatched.width=64u;mismatched.wide_identity=s::wide_builtin::unknown;break;
     case 1u:mismatched.unsigned_value=true;mismatched.wide_identity=s::wide_builtin::unsigned_int;break;
     case 2u:mismatched.floating=true;mismatched.wide_identity=s::wide_builtin::unknown;break;
     case 3u:mismatched.category=s::value_category::unspecified;break;
     case 4u:mismatched.wide_identity=s::wide_builtin::signed_long;break;
     case 5u:++mismatched.declaration_identity.offset;break;
     case 6u:mismatched.declaration_identity_known=false;break; }
   s::program code{};s::integer out{99u};reads=queries=rhs_reads=0u;
   auto values{[&](d::source_expression const& e,bool,s::integer& v)
   { ++reads;if(e.root_name=="right")++rhs_reads;v=(right==(e.root_name=="right"))?mismatched:original;return true; }};
   check(s::parse("left && right",code)==s::error::none && s::evaluate(code,values,out,guest,type,language)==s::error::unavailable &&
      out.bits==0u && reads==(right?2u:1u) && queries==2u,"selected value agrees with all authenticated type fields");
  }
  for(bool conjunction:{false,true})
  {
   auto left{copied(conjunction?0:1)};auto no_values{[&](d::source_expression const& e,bool,s::integer& out)
   { ++reads;if(e.root_name=="left") { out=left;return true; }++rhs_reads;return false; }};
   auto metadata{[&](d::source_expression const& e,s::integer& out)
   { ++queries;out=e.root_name=="left"?left:original;out.bits=0u;return true; }};
   s::program code{};s::integer out{};reads=rhs_reads=queries=0u;
   check(s::parse(conjunction?"left && right":"left || right",code)==s::error::none &&
      s::evaluate(code,no_values,out,guest,metadata,language)==s::error::none && rhs_reads==0u && reads==1u && queries==2u,"dead declared value may be unavailable");
  }
 }
 for(double a:{-0.0,::std::numeric_limits<double>::quiet_NaN(),::std::numeric_limits<double>::infinity()})
 for(double b:{-0.0,::std::numeric_limits<double>::quiet_NaN(),::std::numeric_limits<double>::infinity()})
 native_probe(copied(a),copied(b),static_cast<bool>(a),static_cast<bool>(a && b),static_cast<bool>(a || b));
 // The default finite shared grammar deliberately retains its existing lazy contract.
 for(auto text:{"0 && missing","1 || missing","0 && (1.0%1)"})
 { s::integer out{};check(constants(text,32u,L::shared_numeric,out)==s::error::none && reads==0u,"shared profile compatibility"); }
 // A malformed/DAG program must fail within existing depth/type budgets before any read.
 s::program dag{};s::node leaf{};leaf.op=s::operation::reference;leaf.reference.root_name=::fast_io::concat_std("left");dag.nodes.push_back(leaf);
 for(unsigned i{1u};i!=16u;++i) { s::node n{};n.op=s::operation::logical_and;n.lhs=n.rhs=i-1u;dag.nodes.push_back(n); }dag.root=15u;
 s::integer out{99u};reads=queries=0u;
 auto values{[](d::source_expression const&,bool,s::integer&) { ++reads;return false; }};
 auto types{[](d::source_expression const&,s::integer& v) { ++queries;v=copied(1);v.bits=0u;return true; }};
 check(s::evaluate(dag,values,out,32u,types,L::cpp)==s::error::limit_exceeded && out.bits==0u && reads==0u && queries<=4096u,"shared 4096 type visits bounds repeated edges");
 ::fast_io::io::println("debug_source_logical_preflight: PASS checks=",checks," native_pairs=196 repeats=16 no guest execution");
}
