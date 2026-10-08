// Rust character expression DATA and original CU UTF identity. fast_io IO only.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
namespace d=uwvm2::uwvm::debugger::source_dwarf;
using L=s::language_semantics;
static unsigned checks{};
static void check(bool v,::std::string_view why)
{ ++checks;if(!v){::fast_io::io::perrln("Rust char FAIL: ",why);::fast_io::fast_terminate();} }
static s::error probe(::std::string_view text,s::integer& out,unsigned guest=32u)
{
 s::program p{};auto e=s::parse_admitted(text,p);if(e!=s::error::none)return e;
 auto absent=[](d::source_expression const&,bool,s::integer&){return false;};
 return s::evaluate(p,absent,out,guest,absent,L::rust);
}
int main(int argc,char** argv)
{
 if(argc>1)
 {
  for(int i=1;i<argc;++i)
  { auto text=::fast_io::string_view{::fast_io::mnp::os_c_str(argv[i])};s::integer v{};
    auto status=probe({text.data(),text.size()},v);
    ::fast_io::io::println(i-1,"\t",static_cast<unsigned>(status),"\t",v.bits,"\t",v.width,"\t",v.unsigned_value,"\t",s::copied_type_name(v)); }
  return 0;
 }
 struct W {::std::string_view text;::std::uint64_t bits;unsigned width;bool uns;::std::string_view kind;};
 W const cases[]{
 {"'a'",97,32,true,"Rust char"},{"'\\n'",10,32,true,"Rust char"},{"'\\0'",0,32,true,"Rust char"},
 {"'\\''",39,32,true,"Rust char"},{"'\\\\'",92,32,true,"Rust char"},{"'\\\"'",34,32,true,"Rust char"},
 {"'\\x7f'",127,32,true,"Rust char"},{"'\\u{3bb}'",955,32,true,"Rust char"},
 {"'\\u{1f642}'",128578,32,true,"Rust char"},{"'\\u{10_ffff__}'",0x10ffff,32,true,"Rust char"},
 {"'\\u{1f642}' as u8",66,8,true,"unsigned integer"},{"'\\u{1f642}' as i8",66,8,false,"signed integer"},
 {"'\\u{3bb}' as u32",955,32,true,"unsigned integer"},{"'\\u{3bb}' as i32",955,32,false,"signed integer"},
 {"(255 as u8) as char",255,32,true,"Rust char"},{"(65 as u8) as char == 'A'",1,8,true,"bool"},
 {"'a' as char",97,32,true,"Rust char"},{"'\\u{3bb}' < '\\u{1f642}'",1,8,true,"bool"},
 {"'a' == 'a'",1,8,true,"bool"},{"'a' != 'b'",1,8,true,"bool"},{"'a' >= 'b'",0,8,true,"bool"},
 {"('a' as u32) + (1 as u32)",98,32,true,"unsigned integer"}
 };
 for(auto guest:{32u,64u})for(auto const& c:cases)
 {s::integer v{};check(probe(c.text,v,guest)==s::error::none,c.text);
  check(v.bits==c.bits && v.width==c.width && v.unsigned_value==c.uns && s::copied_type_name(v)==c.kind,c.text);}
 for(auto text:{"'\\x1'","'\\x80'","'\\x001'","'\\123'","'\\a'","'\\?'","'\\u{}'","'\\u{_41}'","'\\u{0000000}'","'\\u{d800}'","'\\u{110000}'",
   "'a'+1","+'a'","-'a'","!'a'","'a' & 'b'","'a' << 1","'a' == (97 as u32)","'a' as f32","'a' as bool",
   "(65 as u32) as char","(65 as i8) as char","true as char","1.0 as char","char(65 as u8)","false && ('a'+1 == 0)","true || ('a' as f32 == 0.0)"})
 {s::integer v{};check(probe(text,v)!=s::error::none,text);}
 d::type_record type{};type.kind=d::type_kind::scalar;type.language=0x1c;type.encoding=0x10;type.byte_count=4u;
 type.identity={0u,1u};type.declaration_identity={0u,1u};
 s::integer v{128578,32,true,false,s::value_category::integer};
 check(s::attach_standard_integer_type(type,v,32u)&&v.rust_character,"genuine Rust CU/UTF metadata");
 for(auto language:{L::c,L::cpp,L::go,L::zig,L::shared_numeric})
 {s::program p{};check(s::parse("'\\u{3bb}'",p,L::rust)==s::error::none,"Rust syntax DATA");
  auto no=[](d::source_expression const&,bool,s::integer&){return false;};s::integer x{};
  check(s::evaluate(p,no,x,32u,no,language)!=s::error::none,"syntax DATA cannot select non-Rust CU");}
 for(auto bad:{0xd800u,0x110000u})
 {s::integer x{bad,32,true,false,s::value_category::integer};
  check(!s::attach_standard_integer_type(type,x,32u)&&x.width==0,"invalid copied Rust char remains unavailable");}
 ::fast_io::io::println("debug_source_rust_character_literal: PASS checks=",checks);
}
