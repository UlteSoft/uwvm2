// Standard integer specifier combinations; copied DATA only, no live guest authority.
#include <fast_io.h>
#include <type_traits>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
namespace scalar=uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf=uwvm2::uwvm::debugger::source_dwarf;
static ::std::size_t checks{};
static void check(bool ok,::std::string_view why)
{++checks;if(!ok){::fast_io::io::perrln("Builtin types FAIL: ",why);::fast_io::fast_terminate();}}
#ifdef UWVM_BUILTIN_TYPES_BASELINE
int main(int argc,char const* const* argv)
{
 check(argc==2,"baseline input");auto text=::fast_io::string_view{::fast_io::mnp::os_c_str(argv[1])};scalar::program code{};
 auto const parsed=scalar::parse({text.data(),text.size()},code);scalar::integer value{};
 auto const values=[](dwarf::source_expression const& leaf,bool size,scalar::integer& out)
 {if(size || leaf.root_name!="value" || !leaf.steps.empty()){return false;}out={42u,32u,false};return true;};
 check(parsed==scalar::error::none && scalar::evaluate(code,values,value)==scalar::error::none,"new integer specifier unavailable");
}
#else
static ::std::size_t reads{},queries{};static unsigned root_code{},guest_bits{};
static unsigned identify(dwarf::source_expression const& ref)
{
 if(!ref.steps.empty()){return 0u;}
 if(ref.root_name=="signed"){return 8u;}if(ref.root_name=="signed_value"){return 9u;}
 if(ref.root_name=="value"){return 1u;}if(ref.root_name=="flag"){return 2u;}
 if(ref.root_name=="static_cast_value"){return 3u;}if(ref.root_name=="static_cast"){return 4u;}
 if(ref.root_name=="aspect"){return 5u;}if(ref.root_name=="as_value"){return 6u;}if(ref.root_name=="as"){return 7u;}
 return 0u;
}
static bool resolve(dwarf::source_expression const& ref,bool size,scalar::integer& out)
{
 ++reads;root_code=identify(ref);if(root_code==0u){return false;}
 if(size){out={root_code==2u ? 1u : 4u,guest_bits,true};return true;}
 if(root_code==2u){out=scalar::from_dwarf_numeric(dwarf::numeric_kind::boolean,2u,8u);return true;}
 out={root_code==1u ? 42u : root_code+5u,32u,false,false,scalar::value_category::integer};return true;
}
static bool type(dwarf::source_expression const& ref,bool size,scalar::integer& out)
{
 ++queries;auto const code=identify(ref);if(code==0u){return false;}
 if(size){out={code==2u ? 1u : 4u,guest_bits,true};return true;}
 if(code==2u){out=scalar::from_dwarf_numeric(dwarf::numeric_kind::boolean,0u,8u);return true;}
 out={0u,32u,false,false,scalar::value_category::integer};return true;
}
struct result{scalar::error parsed{},evaluated{};scalar::integer value{};};
static result run(::std::string_view text,unsigned guest)
{
 reads=queries=root_code=0u;guest_bits=guest;scalar::program code{};result out{};
 check(scalar::parse("1",code)==scalar::error::none,"seed output");
 out.parsed=scalar::parse(text,code);
 if(out.parsed==scalar::error::none){out.evaluated=scalar::evaluate(code,resolve,out.value,guest,type);}
 else{check(code.nodes.empty(),"rejected parse clears output");out.evaluated=scalar::error::unsupported;}
 return out;
}
template<typename Text> static void verify(Text const& text,unsigned guest,::std::uint64_t bits,unsigned width,
 bool uns=false,bool floating=false,::std::size_t count=0u)
{
 auto const out=run({text.data(),text.size()},guest);
 check(out.parsed==scalar::error::none && out.evaluated==scalar::error::none,"valid spaced conversion");
 check(out.value.bits==bits && out.value.width==width && out.value.unsigned_value==uns && out.value.floating==floating,"native oracle result");
 check(reads==count && queries==0u,"value and type callbacks");
}
template<typename Spelling,typename Canonical,unsigned Width,bool Uns>
static void native_type(::fast_io::string_view spelling)
{
 static_assert(::std::is_same_v<Spelling,Canonical>,"compiler type-specifier equivalence");
 static_assert(Width==0u || sizeof(Spelling)*8u==Width,"compiler fixed-width type");
 static_assert(::std::is_same_v<Spelling,char> || ::std::numeric_limits<Spelling>::is_signed!=Uns,"compiler signedness");
 for(unsigned guest:{32u,64u})
 {
  auto test=[&]<typename Target>()
  {
   auto const width=static_cast<unsigned>(sizeof(Target)*8u);
   auto const literal=static_cast<::std::uint64_t>(static_cast<Target>(-73)) & (width==64u ? ~::std::uint64_t{} : (::std::uint64_t{1u}<<width)-1u);
   verify(::fast_io::concat_fast_io("static_cast < ",spelling," > (-73)"),guest,literal,width,Uns);
   verify(::fast_io::concat_fast_io("(",spelling,")(value)"),guest,42u,width,Uns,false,1u);
   verify(::fast_io::concat_fast_io("sizeof(",spelling,")"),guest,sizeof(Target),guest,true);
   verify(::fast_io::concat_fast_io("static_cast<",spelling,">(flag)"),guest,1u,width,Uns,false,1u);
  };
  if constexpr(Width==0u)
  {
   if(guest==32u){test.template operator()<::std::conditional_t<Uns,::std::uint32_t,::std::int32_t>>();}
   else{test.template operator()<::std::conditional_t<Uns,::std::uint64_t,::std::int64_t>>();}
  }
  else{test.template operator()<::std::conditional_t<::std::is_same_v<Spelling,char>,signed char,Spelling>>();}
 }
}
static ::fast_io::string_view gap(unsigned n){static constexpr char text[]{"    "};return ::fast_io::string_view{text,n};}
int main(int argc,char const* const* argv)
{
 if(argc>1)
 {
  for(int i{1};i<argc;++i)
  {
   auto const text=::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])};check(text.size()<=256u,"bounded CLI bridge");
   for(unsigned guest:{32u,64u})
   {
    auto const r=run({text.data(),text.size()},guest);
    ::fast_io::io::println(i-1,"\t",guest,"\t",r.parsed==scalar::error::none ? 1u : 0u,"\t",static_cast<unsigned>(r.evaluated),
      "\t",r.value.bits,"\t",r.value.width,"\t",r.value.unsigned_value ? 1u : 0u,"\t",r.value.floating ? 1u : 0u,
      "\t",reads,"\t",queries,"\t",root_code);
   }
  }
  return 0;
 }
 native_type<char,char,8u,false>(::fast_io::string_view{"char"});
 native_type<char signed,signed char,8u,false>(::fast_io::string_view{"char signed"});
 native_type<char unsigned,unsigned char,8u,true>(::fast_io::string_view{"char unsigned"});
 native_type<int,int,32u,false>(::fast_io::string_view{"int"});
 native_type<int long,long int,0u,false>(::fast_io::string_view{"int long"});
 native_type<int long long,long long int,64u,false>(::fast_io::string_view{"int long long"});
 native_type<int long long signed,signed long long int,64u,false>(::fast_io::string_view{"int long long signed"});
 native_type<int long long unsigned,unsigned long long int,64u,true>(::fast_io::string_view{"int long long unsigned"});
 native_type<int long signed,signed long int,0u,false>(::fast_io::string_view{"int long signed"});
 native_type<int long signed long,signed long long int,64u,false>(::fast_io::string_view{"int long signed long"});
 native_type<int long unsigned,unsigned long int,0u,true>(::fast_io::string_view{"int long unsigned"});
 native_type<int long unsigned long,unsigned long long int,64u,true>(::fast_io::string_view{"int long unsigned long"});
 native_type<int short,short int,16u,false>(::fast_io::string_view{"int short"});
 native_type<int short signed,signed short int,16u,false>(::fast_io::string_view{"int short signed"});
 native_type<int short unsigned,unsigned short int,16u,true>(::fast_io::string_view{"int short unsigned"});
 native_type<int signed,signed int,32u,false>(::fast_io::string_view{"int signed"});
 native_type<int signed long,signed long int,0u,false>(::fast_io::string_view{"int signed long"});
 native_type<int signed long long,signed long long int,64u,false>(::fast_io::string_view{"int signed long long"});
 native_type<int signed short,signed short int,16u,false>(::fast_io::string_view{"int signed short"});
 native_type<int unsigned,unsigned int,32u,true>(::fast_io::string_view{"int unsigned"});
 native_type<int unsigned long,unsigned long int,0u,true>(::fast_io::string_view{"int unsigned long"});
 native_type<int unsigned long long,unsigned long long int,64u,true>(::fast_io::string_view{"int unsigned long long"});
 native_type<int unsigned short,unsigned short int,16u,true>(::fast_io::string_view{"int unsigned short"});
 native_type<long,long,0u,false>(::fast_io::string_view{"long"});
 native_type<long int,long int,0u,false>(::fast_io::string_view{"long int"});
 native_type<long int long,long long int,64u,false>(::fast_io::string_view{"long int long"});
 native_type<long int long signed,signed long long int,64u,false>(::fast_io::string_view{"long int long signed"});
 native_type<long int long unsigned,unsigned long long int,64u,true>(::fast_io::string_view{"long int long unsigned"});
 native_type<long int signed,signed long int,0u,false>(::fast_io::string_view{"long int signed"});
 native_type<long int signed long,signed long long int,64u,false>(::fast_io::string_view{"long int signed long"});
 native_type<long int unsigned,unsigned long int,0u,true>(::fast_io::string_view{"long int unsigned"});
 native_type<long int unsigned long,unsigned long long int,64u,true>(::fast_io::string_view{"long int unsigned long"});
 native_type<long long,long long,64u,false>(::fast_io::string_view{"long long"});
 native_type<long long int,long long int,64u,false>(::fast_io::string_view{"long long int"});
 native_type<long long int signed,signed long long int,64u,false>(::fast_io::string_view{"long long int signed"});
 native_type<long long int unsigned,unsigned long long int,64u,true>(::fast_io::string_view{"long long int unsigned"});
 native_type<long long signed,signed long long,64u,false>(::fast_io::string_view{"long long signed"});
 native_type<long long signed int,signed long long int,64u,false>(::fast_io::string_view{"long long signed int"});
 native_type<long long unsigned,unsigned long long,64u,true>(::fast_io::string_view{"long long unsigned"});
 native_type<long long unsigned int,unsigned long long int,64u,true>(::fast_io::string_view{"long long unsigned int"});
 native_type<long signed,signed long,0u,false>(::fast_io::string_view{"long signed"});
 native_type<long signed int,signed long int,0u,false>(::fast_io::string_view{"long signed int"});
 native_type<long signed int long,signed long long int,64u,false>(::fast_io::string_view{"long signed int long"});
 native_type<long signed long,signed long long,64u,false>(::fast_io::string_view{"long signed long"});
 native_type<long signed long int,signed long long int,64u,false>(::fast_io::string_view{"long signed long int"});
 native_type<long unsigned,unsigned long,0u,true>(::fast_io::string_view{"long unsigned"});
 native_type<long unsigned int,unsigned long int,0u,true>(::fast_io::string_view{"long unsigned int"});
 native_type<long unsigned int long,unsigned long long int,64u,true>(::fast_io::string_view{"long unsigned int long"});
 native_type<long unsigned long,unsigned long long,64u,true>(::fast_io::string_view{"long unsigned long"});
 native_type<long unsigned long int,unsigned long long int,64u,true>(::fast_io::string_view{"long unsigned long int"});
 native_type<short,short,16u,false>(::fast_io::string_view{"short"});
 native_type<short int,short int,16u,false>(::fast_io::string_view{"short int"});
 native_type<short int signed,signed short int,16u,false>(::fast_io::string_view{"short int signed"});
 native_type<short int unsigned,unsigned short int,16u,true>(::fast_io::string_view{"short int unsigned"});
 native_type<short signed,signed short,16u,false>(::fast_io::string_view{"short signed"});
 native_type<short signed int,signed short int,16u,false>(::fast_io::string_view{"short signed int"});
 native_type<short unsigned,unsigned short,16u,true>(::fast_io::string_view{"short unsigned"});
 native_type<short unsigned int,unsigned short int,16u,true>(::fast_io::string_view{"short unsigned int"});
 native_type<signed,signed,32u,false>(::fast_io::string_view{"signed"});
 native_type<signed char,signed char,8u,false>(::fast_io::string_view{"signed char"});
 native_type<signed int,signed int,32u,false>(::fast_io::string_view{"signed int"});
 native_type<signed int long,signed long int,0u,false>(::fast_io::string_view{"signed int long"});
 native_type<signed int long long,signed long long int,64u,false>(::fast_io::string_view{"signed int long long"});
 native_type<signed int short,signed short int,16u,false>(::fast_io::string_view{"signed int short"});
 native_type<signed long,signed long,0u,false>(::fast_io::string_view{"signed long"});
 native_type<signed long int,signed long int,0u,false>(::fast_io::string_view{"signed long int"});
 native_type<signed long int long,signed long long int,64u,false>(::fast_io::string_view{"signed long int long"});
 native_type<signed long long,signed long long,64u,false>(::fast_io::string_view{"signed long long"});
 native_type<signed long long int,signed long long int,64u,false>(::fast_io::string_view{"signed long long int"});
 native_type<signed short,signed short,16u,false>(::fast_io::string_view{"signed short"});
 native_type<signed short int,signed short int,16u,false>(::fast_io::string_view{"signed short int"});
 native_type<unsigned,unsigned,32u,true>(::fast_io::string_view{"unsigned"});
 native_type<unsigned char,unsigned char,8u,true>(::fast_io::string_view{"unsigned char"});
 native_type<unsigned int,unsigned int,32u,true>(::fast_io::string_view{"unsigned int"});
 native_type<unsigned int long,unsigned long int,0u,true>(::fast_io::string_view{"unsigned int long"});
 native_type<unsigned int long long,unsigned long long int,64u,true>(::fast_io::string_view{"unsigned int long long"});
 native_type<unsigned int short,unsigned short int,16u,true>(::fast_io::string_view{"unsigned int short"});
 native_type<unsigned long,unsigned long,0u,true>(::fast_io::string_view{"unsigned long"});
 native_type<unsigned long int,unsigned long int,0u,true>(::fast_io::string_view{"unsigned long int"});
 native_type<unsigned long int long,unsigned long long int,64u,true>(::fast_io::string_view{"unsigned long int long"});
 native_type<unsigned long long,unsigned long long,64u,true>(::fast_io::string_view{"unsigned long long"});
 native_type<unsigned long long int,unsigned long long int,64u,true>(::fast_io::string_view{"unsigned long long int"});
 native_type<unsigned short,unsigned short,16u,true>(::fast_io::string_view{"unsigned short"});
 native_type<unsigned short int,unsigned short int,16u,true>(::fast_io::string_view{"unsigned short int"});
 for(unsigned mask{};mask!=1024u;++mask)
 {
  auto a=gap(mask&3u),b=gap((mask>>2u)&3u),c=gap((mask>>4u)&3u),d=gap((mask>>6u)&3u),e=gap((mask>>8u)&3u);
  for(unsigned guest:{32u,64u})
  {
   verify(::fast_io::concat_fast_io(a,"static_cast",b,"<",c,"int",d,">",e,"(",a,"42",b,")"),guest,static_cast<int>(42),32u);
   verify(::fast_io::concat_fast_io("static_cast",b,"<",c,"unsigned ",d,"char",a,">",e,"(",a,"flag",b,")"),guest,static_cast<unsigned char>(true),8u,true,false,1u);
   verify(::fast_io::concat_fast_io("(",a,"unsigned ",d,"char",b,")",c,"(",e,"flag",a,")"),guest,static_cast<unsigned char>(true),8u,true,false,1u);
   verify(::fast_io::concat_fast_io("sizeof",b,"(",c,"unsigned ",d,"char",a,")"),guest,sizeof(unsigned char),guest,true);
   verify(::fast_io::concat_fast_io("@as",b,"(",c,"i32",d,",",e,"255",a,")"),guest,255u,32u);
   verify(::fast_io::concat_fast_io("static_cast",b,"<",c,"float",d,">",e,"(",a,"0.15",b,")"),guest,::std::bit_cast<::std::uint32_t>(static_cast<float>(0.15)),32u,false,true);
  }
 }
 for(unsigned guest:{32u,64u})
 {
  verify(::fast_io::string_view{"static_cast_value < value"},guest,1u,32u,false,false,2u);
  verify(::fast_io::string_view{"static_cast + 1"},guest,10u,32u,false,false,1u);
  verify(::fast_io::string_view{"aspect + 1"},guest,11u,32u,false,false,1u);
  verify(::fast_io::string_view{"as_value + 1"},guest,12u,32u,false,false,1u);
  for(auto text:{"static_castx<int>(value, 1)","sta tic_cast<int>(value)","@asx (i32, value)",
    "static_cast < int* > (value)","@as ( *i32, value)","static_cast < unsignedchar > (value)",
    "(unsignedchar)(value)","@as (u 8, 1)","static_cast < int > (value = 8)","@as (u8, value++)"})
  {
   auto view=::fast_io::string_view{::fast_io::mnp::os_c_str(text)};auto r=run({view.data(),view.size()},guest);
   check(r.parsed!=scalar::error::none && reads==0u && queries==0u,"unsupported or injected conversion rejected before callbacks");
  }
 }
 ::fast_io::io::println("debug_source_builtin_type_specifiers: PASS checks=",checks," compiler type equivalence and numeric/sizeof oracle; owned DATA");
}
#endif
