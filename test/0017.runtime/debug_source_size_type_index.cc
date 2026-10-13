// Fresh compiler Wasm32/64 DWARF; selected declaration DATA, no live guest values.
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <uwvm2/uwvm/debugger/source_frames.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <uwvm2/uwvm/debugger/source_dwarf_query.h>
namespace d=uwvm2::uwvm::debugger::source_dwarf;
namespace s=uwvm2::uwvm::debugger::source_scalar_expression;
namespace f=uwvm2::uwvm::debugger::source_frames;
using K=s::wide_builtin;
static ::std::size_t checks{},proofs{};
static void check(bool ok,::std::string_view why)
{ ++checks;if(!ok) { ::fast_io::io::perrln("size type index FAIL: ",why);::fast_io::fast_terminate(); } }
int main(int argc,char** argv)
{
 check(argc>=4 && argc%3==1,"path, guest width, language triples");
 for(int i{1};i<argc;i+=3)
 {
  ::std::string_view width_text{argv[i+1]};check(width_text=="32" || width_text=="64","explicit target width");
  unsigned guest{width_text=="32"?32u:64u};bool cpp{::std::string_view{argv[i+2]}=="cpp"};
  ::fast_io::native_file_loader file{::fast_io::mnp::os_c_str(argv[i]),::fast_io::open_mode::in};
  auto bytes{::std::span<::std::byte const>{reinterpret_cast<::std::byte const*>(file.data()),file.size()}};
  check(bytes.size()>=8u,"Wasm header size");constexpr unsigned char magic[]{0u,0x61u,0x73u,0x6du,1u,0u,0u,0u};
  for(unsigned j{};j<8u;++j)check(::std::to_integer<unsigned char>(bytes[j])==magic[j],"Wasm magic");
  d::details::reader module{bytes,8u};::std::vector<d::section> sections{};::std::uint64_t code_size{};
  while(module.cursor!=bytes.size())
  {
   ::std::uint8_t id{};::std::uint32_t length{};
   check(module.byte(id) && module.leb(length) && length<=bytes.size()-module.cursor,"bounded section");
   auto payload{bytes.subspan(module.cursor,length)};module.cursor+=length;
   if(id==10u) { check(code_size==0u,"single Code");code_size=length; }if(id!=0u)continue;
   d::details::reader custom{payload};::std::uint32_t n{};
   check(custom.leb(n) && n<=payload.size()-custom.cursor,"bounded custom name");
   ::std::string_view name{reinterpret_cast<char const*>(payload.data()+custom.cursor),n};custom.cursor+=n;
   if(name.starts_with(".debug_"))sections.push_back({name,payload.subspan(custom.cursor)});
  }
  ::std::unique_ptr<d::index> index{};check(code_size!=0u && d::index::parse({sections,code_size,static_cast<::std::uint8_t>(guest/8u)},index)==d::error::none && index,"genuine Wasm compiler index");
  unsigned active{};
  for(auto const& scope:index->scopes())
  {
   if(scope.kind!=d::scope_kind::subprogram || scope.name!="size_rank" || !scope.concrete || scope.ranges.empty())continue;
   f::language_context context{};check(f::current_language(index->scopes(),scope.ranges[0].begin,0u,context)==f::error::none,"actual selected CU");
   auto lang{s::language_from_dwarf(context.language,context.tinygo_producer)};
   check(cpp?lang==s::language_semantics::cpp:lang==s::language_semantics::c || lang==s::language_semantics::c23,"actual producer language profile");
   d::variable_selection selected{};check(d::query_named_variable(index->scopes(),index->types(),index->variables(),scope.ranges[0].begin,"a",selected)==d::inline_query_error::none && selected.type<index->types().size(),"actual compiler size_t declaration");
   auto const& observed{index->types()[selected.type]};
   check(observed.wide_builtin==K::unsigned_long && observed.byte_count*8u==guest && observed.encoding==7u,"actual size_t underlying type independently matches target witness");
   auto fetch{[&](d::source_expression const& e,s::integer& out,bool extent)
   {
    if(!e.steps.empty())return false;d::variable_selection v{};
    if(d::query_named_variable(index->scopes(),index->types(),index->variables(),scope.ranges[0].begin,e.root_name,v)!=d::inline_query_error::none || v.type>=index->types().size())return false;
    auto const& t{index->types()[v.type]};
    if(extent) { out={t.byte_count,guest,true};return true; }
    out=s::from_dwarf_numeric(t.encoding==7u?d::numeric_kind::unsigned_integer:d::numeric_kind::signed_integer,0u,static_cast<unsigned>(t.byte_count)*8u);
    return s::attach_standard_integer_type(t,out,guest);
   }};
   struct witness { ::std::string_view text;K small,large;::std::uint64_t bits;unsigned reads{},extents{},queries{}; };
   constexpr witness rows[]{
    {"sizeof(int)",K::unsigned_long,K::unsigned_long,4u},
    {"sizeof(a)",K::unsigned_long,K::unsigned_long,0u,0u,1u},
    {"sizeof(a+1)",K::unsigned_long,K::unsigned_long,0u,0u,0u,1u},
    {"sizeof(sizeof(a))",K::unsigned_long,K::unsigned_long,0u,0u,0u,1u},
    {"(size_t)1",K::unsigned_long,K::unsigned_long,1u},
    {"sizeof(int)+b",K::unsigned_long,K::unsigned_long,5u,1u},
    {"sizeof(int)+c",K::signed_long_long,K::unsigned_long_long,5u,1u},
    {"1?sizeof(int):b",K::unsigned_long,K::unsigned_long,4u,0u,0u,1u},
    {"0?sizeof(int):b",K::unsigned_long,K::unsigned_long,1u,1u,0u,1u},
    {"1?sizeof(int):c",K::signed_long_long,K::unsigned_long_long,4u,0u,0u,1u},
    {"0?sizeof(int):c",K::signed_long_long,K::unsigned_long_long,1u,1u,0u,1u},
    {"1?sizeof(int):sizeof(a)",K::unsigned_long,K::unsigned_long,4u,0u,0u,1u}
   };
   for(auto row:rows)
   {
    ::std::size_t reads{},extents{},queries{};
    auto types{[&](d::source_expression const& e,bool extent,s::integer& out) { ++queries;return fetch(e,out,extent); }};
    auto values{[&](d::source_expression const& e,bool extent,s::integer& out)
    { if(extent)++extents;else ++reads;if(!fetch(e,out,extent))return false;
      if(!extent)out.bits=e.root_name=="a"?4u:1u;return true; }};
    s::program p{};s::integer out{};check(s::parse(row.text,p)==s::error::none && s::evaluate(p,values,out,guest,types,lang)==s::error::none,row.text);
    K k{guest==32u?row.small:row.large};unsigned width{k==K::signed_long_long || k==K::unsigned_long_long?64u:guest};
    check(out.wide_identity==k && out.width==width && out.unsigned_value==d::c_wide_unsigned(k) && !out.floating && out.category!=s::value_category::boolean,"actual compiler target rank/conversions");
    check(out.bits==(row.bits?row.bits:guest/8u) && !out.declaration_identity_known,"value and computed type without invented DIE");
    check(reads==row.reads && extents==row.extents && queries==row.queries,"exact selected scalar DATA and declaration-only reads");++proofs;
   }
   ++active;
  }
  check(active==1u,"one concrete size_rank producer");
 }
 ::fast_io::io::println("debug_source_size_type_index: PASS checks=",checks," producer_modules=",(argc-1)/3," size_type_proofs=",proofs," no live stop qualification");
}
