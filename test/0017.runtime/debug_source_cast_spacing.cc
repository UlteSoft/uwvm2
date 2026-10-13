// Named numeric conversion spacing; copied DATA only, no live guest authority.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
namespace scalar=uwvm2::uwvm::debugger::source_scalar_expression;
namespace dwarf=uwvm2::uwvm::debugger::source_dwarf;
static ::std::size_t checks{};
static void check(bool ok,::std::string_view why)
{++checks;if(!ok){::fast_io::io::perrln("Cast spacing FAIL: ",why);::fast_io::fast_terminate();}}
#ifdef UWVM_CAST_SPACING_BASELINE
int main(int argc,char const* const* argv)
{
 check(argc==2,"baseline input");auto text=::fast_io::string_view{::fast_io::mnp::os_c_str(argv[1])};scalar::program code{};
 auto const parsed=scalar::parse({text.data(),text.size()},code);scalar::integer value{};
 auto const values=[](dwarf::source_expression const& leaf,bool size,scalar::integer& out)
 {if(size || leaf.root_name!="value" || !leaf.steps.empty()){return false;}out={42u,32u,false};return true;};
 check(parsed==scalar::error::none && scalar::evaluate(code,values,value)==scalar::error::none,"new token spacing unavailable");
}
#else
static ::std::size_t reads{},queries{};static unsigned root_code{},guest_bits{};
static unsigned identify(dwarf::source_expression const& ref)
{
 if(!ref.steps.empty()){return 0u;}
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
 ::fast_io::io::println("debug_source_cast_spacing: PASS checks=",checks," native numeric/sizeof spacing oracle; owned DATA");
}
#endif
