// Bounded Rust numeric token/type DATA; all IO through fast_io.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
namespace d=uwvm2::uwvm::debugger::source_dwarf;
using L=s::language_semantics;
static unsigned checks{};
static void check(bool ok,::std::string_view why)
{++checks;if(!ok){::fast_io::io::perrln("Rust numeric FAIL: ",why);::fast_io::fast_terminate();}}
static s::error probe(::std::string_view text,s::integer& out,unsigned guest=32u,L language=L::rust)
{
 s::program p{};auto status=s::parse_admitted(text,p);if(status!=s::error::none)return status;
 auto absent=[](d::source_expression const&,bool,s::integer&){return false;};
 return s::evaluate(p,absent,out,guest,absent,language);
}
int main(int argc,char** argv)
{
 if(argc>1)
 {
  for(int i=1;i<argc;++i)
  {auto text=::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])};s::integer v{};
   auto status=probe({text.data(),text.size()},v);
   ::fast_io::io::println(i-1,"\t",static_cast<unsigned>(status),"\t",v.bits,"\t",v.width,"\t",v.unsigned_value,"\t",s::copied_type_name(v));}
  return 0;
 }
 struct W{::std::string_view text;::std::uint64_t bits;unsigned width;bool uns;};
 W const witnesses[]{
 {"255u8 - 1",254ull,8u,true}, {"1 + 1u8",2ull,8u,true}, {"-128 + 0i8",128ull,8u,false}, {"0xff + 0u8",255ull,8u,true}, {"1u8 == 1",1ull,8u,true}, {"-1i8 == -1",1ull,8u,true}, {"1.5f32 + 2.0",1080033280ull,32u,false}, {"1.5 + 2f32",1080033280ull,32u,false}, {"true && (255u8 == 255)",1ull,8u,true},
 {"255u8",255ull,8u,true},
 {"127i8",127ull,8u,false},
 {"-128i8",128ull,8u,false},
 {"-(128_i8)",128ull,8u,false},
 {"65535u16",65535ull,16u,true},
 {"-32768i16",32768ull,16u,false},
 {"4294967295u32",4294967295ull,32u,true},
 {"-2147483648i32",2147483648ull,32u,false},
 {"18446744073709551615u64",18446744073709551615ull,64u,true},
 {"-9223372036854775808i64",9223372036854775808ull,64u,false},
 {"0xff_u8",255ull,8u,true},
 {"0b___1111__u8",15ull,8u,true},
 {"0o__77___u16",63ull,16u,true},
 {"009u16",9ull,16u,true},
 {"1__2___u16",12ull,16u,true},
 {"1_u8",1ull,8u,true},
 {"255u8 - 1u8",254ull,8u,true},
 {"!0u8",255ull,8u,true},
 {"!(-128i8)",127ull,8u,false},
 {"!(1u8 + 1u8)",253ull,8u,true},
 {"1u8 << 7u32",128ull,8u,true},
 {"128u8 >> 7i16",1ull,8u,true},
 {"1i16 + 2i16",3ull,16u,false},
 {"1u8 == 1u8",1ull,8u,true},
 {"true && (255u8 > 1u8)",1ull,8u,true},
 {"1usize",1ull,0u,true},
 {"-1isize",~::std::uint64_t{},0u,false},
 {"1f32",1065353216ull,32u,false},
 {"1.25f32",1067450368ull,32u,false},
 {"2e3_f64",4656510908468559872ull,64u,false},
 {"1__2.5__f32",1095237632ull,32u,false},
 {"1.5f32 + 2.0f32",1080033280ull,32u,false}
 };
 for(auto guest:{32u,64u})for(auto const& w:witnesses)
 {
  s::integer v{};check(probe(w.text,v,guest)==s::error::none,w.text);
  auto const width{w.width?w.width:guest};
  check(v.width==width && v.bits==(w.bits&s::details::mask(width)) && v.unsigned_value==w.uns,w.text);
 }

 // R66 compound constraints, cast boundaries and pointer-sized nominal types.
 W const compound[]{
 {"(1 + 2) + 3u8",6ull,8u,true},
 {"(255 - 1) - 1u8",253ull,8u,true},
 {"(1 << 7) + 0i8",128ull,8u,false},
 {"!(1 + 1) + 0u8",253ull,8u,true},
 {"-(1 + 1) + 0i8",254ull,8u,false},
 {"(1.0 + 2.0) + 0f32",1077936128ull,32u,false},
 {"(1 + 2) + (3 + 0u8)",6ull,8u,true},
 {"(1 + 2) == 3u8",1ull,8u,true},
 {"false && ((1 + 2) == 0u8)",0ull,8u,true},
 {"(1 + 2) * (3 + 0i16)",9ull,16u,false},
 {"((1 + 2) << (1u8 + 1)) + 0i16",12ull,16u,false},
 {"(256 + 1) as u8",1ull,8u,true},
 {"((1 + 2) as u8) + 1u8",4ull,8u,true},
 {"((1.0 + 2.0) as f32) + 0f32",1077936128ull,32u,false},
 {"(1 + 2) + 0usize",3ull,0u,true},
 {"-(1 + 2) + 0isize",18446744073709551613ull,0u,false},
 {"(1usize << 1u8) == 2usize",1ull,8u,true},
 {"true || (((1 << 8) + 0u8) == 0u8)",1ull,8u,true},
 {"false && (((1 / 0) + 0u8) == 0u8)",0ull,8u,true},
 {"(1 + 2) + (3 + 4)",10ull,32u,false},
 {"(1.0 + 2.0) + (3.0 + 4.0)",4621819117588971520ull,64u,false},
 {"((1 + 2) | (4 + 0u8)) & 7u8",7ull,8u,true},
 {"(1 + 2) + (3 + (4 + 0i64))",10ull,64u,false},
 {"((1 + 2) as usize) + 0usize",3ull,0u,true},
 };
 for(auto guest:{32u,64u})for(auto const& w:compound)
 {s::integer v{};check(probe(w.text,v,guest)==s::error::none,w.text);auto width=w.width?w.width:guest;
  check(v.width==width && v.bits==(w.bits&s::details::mask(width)) && v.unsigned_value==w.uns,w.text);}
 for(auto text:{"(256 - 1) + 0u8","(128 - 1) + 0i8","-(129 - 1) + 0i8","(255 + 1) + 0u8","(1u8 + 2) + (3 + 0u16)","(1.0 + 2.0) + (3.0 + 0f32) + 0f64","(1 + 2) + 0f32","(1.0 + 2.0) + 0i32","false && ((256 - 1) == 0u8)","true || ((1u8 + 2) == 0u16)","-(1 + 2) + 0u8","(1 + 2) == true","(1usize + 2) + 0u32","(1usize + 2) + 0u64","(1isize + 2) + 0i32","(1isize + 2) + 0i64","((1 + 2) as usize) + 0u32","false && ((1usize + 2) == 0u32)","((2147483648 - 1) as u32) + 0u32","((1 + 2) << (1.0 + 0f32)) + 0i8"})
 {s::integer v{};check(probe(text,v)!=s::error::none && v.bits==0u,text);}

 // A declared type can constrain a complete nested tree. Type traversal has
 // no value access; dead arithmetic stays unread but dead type errors fail.
 for(auto guest:{32u,64u})
 {
  unsigned reads{},queries{};
  auto type=[&](d::source_expression const& leaf,bool size,s::integer& v)
  {++queries;if(size)return false;
   if(leaf.root_name=="word"){v={0u,guest,true,false,s::value_category::integer};v.rust_pointer_sized=true;return true;}
   if(leaf.root_name=="small"){v={0u,8u,true,false,s::value_category::integer};return true;}return false;};
  auto value=[&](d::source_expression const& leaf,bool size,s::integer& v)
  {++reads;if(!type(leaf,size,v))return false;v.bits=5u;return true;};
  auto evaluate=[&](::std::string_view text,s::integer& v)
  {s::program p{};auto e=s::parse_admitted(text,p);return e==s::error::none?s::evaluate(p,value,v,guest,type,L::rust):e;};
  s::integer v{};
  check(evaluate("(1 + 2) + word",v)==s::error::none && v.bits==8u && v.rust_pointer_sized && reads==1u,"declared usize constrains compound");
  reads=0u;
  check(evaluate("false && (((1 / 0) + small) == 0u8)",v)==s::error::none && v.bits==0u && reads==0u && queries<40u,"typed dead arithmetic never reads values");
  check(evaluate("false && (((1 + 2) + small) == 0u16)",v)!=s::error::none && reads==0u,"dead type conflict fails before value access");
  auto wrong=[&](d::source_expression const&,bool,s::integer& out){++reads;out={5u,guest,true,false,s::value_category::integer};return true;};
  s::program p{};check(s::parse_admitted("(1 + 2) + word",p)==s::error::none,"metadata mismatch syntax");
  check(s::evaluate(p,wrong,v,guest,type,L::rust)!=s::error::none,"fixed-width value cannot impersonate declared usize");
 }
 // The unsuffixed decimal is parsed at the inferred f32 precision.
 {s::integer v{};check(probe("(1.0000000596046448 + 0.0) + 0f32",v)==s::error::none &&
   v.width==32u && v.bits==1065353217u,"context f32 single rounding");}

 // Shift boundaries use all primitive widths and both authenticated guest ABIs.
 // The right-shift witness is floor division of the signed magnitude, independently
 // of the implementation's sign-fill path. rustc const/type checks supply a second oracle.
 for(auto guest:{32u,64u})for(auto width:{8u,16u,32u,64u})for(bool uns:{false,true})
 {
  auto const top{::std::uint64_t{1u}<<(width-1u)},full{s::details::mask(width)};
  auto const suffix{::fast_io::concat_fast_io(::fast_io::mnp::os_c_str(uns?"u":"i"),width)};
  ::std::uint64_t const edges[]{0u,1u,2u,top-1u,top,top+1u,full-1u,full};
  for(auto bits:edges)for(auto count:{0u,1u,2u,width/2u,width-2u,width-1u})
  for(bool left:{false,true})
  {
   bool const neg{!uns && (bits&top)!=0u};
   auto const magnitude{neg?(0u-bits)&full:bits};
   auto const literal{::fast_io::concat_fast_io(::fast_io::mnp::os_c_str(neg?"-":""),magnitude,suffix)};
   auto const text{::fast_io::concat_fast_io(literal,::fast_io::mnp::os_c_str(left?" << ":" >> "),count,"u64")};
   s::integer v{};check(probe({text.data(),text.size()},v,guest)==s::error::none,"Rust shift edge status");
   auto const divisor{::std::uint64_t{1u}<<count};
   auto const quotient{magnitude/divisor+(neg && magnitude%divisor!=0u ? 1u:0u)};
   auto const expected{left?(bits<<count)&full:neg?(0u-quotient)&full:quotient};
   check(v.width==width && v.unsigned_value==uns && v.bits==expected,"Rust shift edge value/type");
  }
 }
 for(auto text:{"1i8 << 8u8","-1i8 >> 8i16","1u8 << -1i16","1i64 >> 64u8",
  "1i64 << 18446744073709551615u64","1i8 << 1f32","1f32 << 1u8","1u8 << true",
  "false >> 1u8","'a' << 1u8","1u8 >> 'a'","false && ((1u8 << 1f32) == 0u8)",
  "true || ((1u8 << true) == 0u8)","-(1i8 << 7u8)","(1i8 << 7u8) / -1i8","(1i8 << 7u8) % -1i8"})
 {s::integer v{};check(probe(text,v)!=s::error::none && v.bits==0u,text);}
 for(auto text:{"false && ((1i8 << 8u8) == 0i8)","true || ((-1i8 << -1i16) == 0i8)"})
 {s::integer v{};check(probe(text,v)==s::error::none && v.category==s::value_category::boolean,text);}
 for(auto lang:{L::c,L::cpp,L::c23})
 for(auto text:{"-1 << 0","-1 << 1","1073741824 << 1"})
 {s::integer v{};check(probe(text,v,32u,lang)==s::error::arithmetic,"C profile shift restrictions preserved");}
 for(auto text:{"false && (255u8 == 256)","1u8 + 1.0","1f32 + 1","256 + 0u8","256u8","128i8","-129i8","0xffi8","65536u16","32768i16","4294967296u32","2147483648i32","9223372036854775808i64","18446744073709551616u64","1u128","1i128","1u9","1U8","1u8junk","1u8_u16","0Xu8","0Xffu8","0b2u8","0o8u8","1'u8",".5f32","1.f32","1e_f32","0x1p2f32","1.0u8","-1u8","+1i8","~1u8","1u8 + 1u16","1u8 == 1i8","1f32 + 1f64","1u8 + 1f32","255u8 + 1u8","0u8 - 1u8","128u8 * 2u8","-(-128i8)","false && (256u8 == 0u8)","true || (1u8 == 1u16)"})
 {s::integer v{};check(probe(text,v)!=s::error::none && v.bits==0u,text);}
 for(auto text:{"255u8","1f32","0b___11_u8"})
 for(auto lang:{L::c,L::cpp,L::c23,L::go,L::zig,L::shared_numeric})
 {s::integer v{};check(probe(text,v,32u,lang)!=s::error::none,text);}
 for(auto guest:{32u,64u})
 {
  s::integer v{};
  check(probe("4294967296usize",v,guest)==(guest==32u?s::error::arithmetic:s::error::none),"usize follows guest ABI");
  check(probe("-2147483649isize",v,guest)==(guest==32u?s::error::arithmetic:s::error::none),"isize follows guest ABI");
 }
 ::fast_io::io::println("PASS Rust numeric DATA ",checks," checks");
}
