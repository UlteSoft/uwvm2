#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <array>
#include <uwvm2/uwvm/debugger/source_pretty_printer.h>
namespace d=uwvm2::uwvm::debugger::source_dwarf;
static void require(bool ok){if(!ok)std::abort();}
// Runtime copied locals carry native scalar bytes even when the input Wasm
// and its DWARF use little endian. Exercise the same carrier on BE hosts.
static void copy_i32(std::span<d::copied_numeric_local> values,std::uint32_t bits){
 for(auto&v:values){
  v.wasm_type=0x7f;v.available=true;
  for(std::size_t j{};j+sizeof(bits)<=v.bytes.size();j+=sizeof(bits))
   std::memcpy(v.bytes.data()+j,&bits,sizeof(bits));
 }
}
int main(int argc,char**argv){
 require(argc==1||argc==2);
 auto path=argc==2?argv[1]:std::getenv("UWVM_DEBUG_METADATA_FIXTURE");
 require(path&&*path);std::ifstream f(path,std::ios::binary);require(bool(f));
 std::vector<char> bytes{std::istreambuf_iterator<char>{f},{}};
 require(bytes.size()>=8u&&bytes.size()<=64u*1024u*1024u);
 std::size_t at=8u;
 auto u32=[&](std::size_t end){std::uint32_t out{};for(unsigned n{};n<5u;++n){require(at<end);auto b=static_cast<unsigned char>(bytes[at++]);if(n==4u)require(b<16u);out|=(b&127u)<<(n*7u);if(!(b&128u))return out;}std::abort();};
 std::vector<d::section> sections{};std::uint64_t code_size{};
 while(at<bytes.size()){
  auto id=static_cast<unsigned char>(bytes[at++]);auto n=u32(bytes.size());require(n<=bytes.size()-at);auto end=at+n;
  if(id==0u){auto len=u32(end);require(len<=end-at);std::string_view name{bytes.data()+at,len};at+=len;if(name!="name")sections.push_back({name,{reinterpret_cast<std::byte const*>(bytes.data()+at),end-at}});}
  if(id==10u)code_size=n;at=end;
 }
 require(code_size!=0u);std::unique_ptr<d::index> owner{};auto error=d::index::parse({sections,code_size,4u},owner);
 std::cout<<"index-status="<<static_cast<unsigned>(error)<<'\n';require(error==d::error::none&&owner);
 // Actual parsed producer metadata with fresh DATA scalar copies; no guest/host memory reads.
 auto scopes=owner->scopes();auto variables=owner->variables();auto types=owner->types();
 require(owner->numeric_validation()!=nullptr && owner->numeric_validation()->accepts(scopes,types,variables,{}));
 std::array<d::copied_numeric_local,256> locals{};
 auto fill=[&](unsigned n){copy_i32(locals,n);};
 auto equal=[](auto const&a,auto const&b){
  if(a.size()!=b.size())return false;
  for(std::size_t i{};i!=a.size();++i)if(a[i].identity!=b[i].identity||a[i].name!=b[i].name||a[i].type_name!=b[i].type_name||a[i].parameter!=b[i].parameter||a[i].kind!=b[i].kind||a[i].reason!=b[i].reason||a[i].byte_count!=b[i].byte_count||a[i].bits!=b[i].bits)return false;
  return true;
 };
 std::uint64_t pc{};bool found{};fill(17);
 for(auto const&scope:scopes){
  for(auto const&range:scope.ranges){
   std::vector<d::numeric_variable> out;
   if(range.begin==range.end)continue;
   auto status=d::query_numeric_variables(scopes,types,variables,range.begin,locals,locals.size(),out,{},owner->scope_paths());
   if(status!=d::inline_query_error::none)continue;
   for(auto const&v:out)if(v.kind!=d::numeric_kind::unavailable){pc=range.begin;found=true;break;}
   if(found)break;
  }
  if(found)break;
 }
 require(found);

 // Independent reference to the original physical-subprogram range scan.
 // The new optional output must describe that same successful validated path.
 auto path_check=[&](std::uint64_t point,d::inline_query_limits cap={}){
  std::vector<d::inline_frame> plain,selected;std::size_t physical=d::no_record;
  auto a=d::query_inline_frames(scopes,point,plain,cap,owner->scope_paths());
  auto b=d::query_inline_frames(scopes,point,selected,cap,owner->scope_paths(),&physical);
  require(a==b&&plain.size()==selected.size());
  for(std::size_t i{};i!=plain.size();++i)require(plain[i].name==selected[i].name&&plain[i].call_file==selected[i].call_file&&plain[i].call_line==selected[i].call_line&&plain[i].call_column==selected[i].call_column);
  if(a==d::inline_query_error::none){
   std::size_t reference=d::no_record;
   for(std::size_t i{};i!=scopes.size();++i){
    if(scopes[i].kind!=d::scope_kind::subprogram||!scopes[i].concrete)continue;
    for(auto const&r:scopes[i].ranges)if(r.begin<=point&&point<r.end){reference=i;break;}
    if(reference!=d::no_record)break;
   }
   require(reference!=d::no_record&&reference==physical);
  }else require(physical==d::no_record);
 };
 unsigned positions{};
 for(auto const&scope:scopes){
  for(auto const&r:scope.ranges){
   path_check(r.begin);path_check(r.end);
   if(r.begin<r.end)path_check(r.end-1);
   if(++positions==64)break;
  }
  if(positions==64)break;
 }
 require(positions>0);path_check(~std::uint64_t{});
 // Compare indexed candidates to the original complete-variable scan at both
 // sides of real producer scope boundaries, including unavailable PCs. Keep
 // the physical result empty on every failure and compare its independent
 // original range-scan identity on success.
 auto numeric_at=[&](std::uint64_t point,d::numeric_query_limits cap={}){
  std::vector<d::numeric_variable> plain,indexed;
  std::size_t physical=7;
  auto a=d::query_numeric_variables(scopes,types,variables,point,locals,locals.size(),plain,cap,owner->scope_paths());
  auto b=d::query_numeric_variables(scopes,types,variables,point,locals,locals.size(),indexed,cap,owner->scope_paths(),owner->numeric_validation(),&physical);
  require(a==b&&equal(plain,indexed));
  if(a==d::inline_query_error::none){
   std::size_t reference=d::no_record;
   for(std::size_t i{};i!=scopes.size();++i){
    if(scopes[i].kind!=d::scope_kind::subprogram||!scopes[i].concrete)continue;
    for(auto const&r:scopes[i].ranges)if(r.begin<=point&&point<r.end){reference=i;break;}
    if(reference!=d::no_record)break;
   }
   require(reference!=d::no_record&&physical==reference);
  }else require(physical==d::no_record);
 };
 numeric_at(pc);numeric_at(~std::uint64_t{});
 unsigned numeric_positions{};
 for(auto const&scope:scopes){
  for(auto const&r:scope.ranges){
   numeric_at(r.begin);numeric_at(r.end);
   if(r.begin<r.end)numeric_at(r.end-1);
   if(++numeric_positions==64u)break;
  }
  if(numeric_positions==64u)break;
 }
 require(numeric_positions>0u);
 for(unsigned n{};n!=5u;++n){
  d::numeric_query_limits cap{};
  if(n==0u)cap.inline_limits.max_scopes=0u;
  else if(n==1u)cap.inline_limits.max_ranges=0u;
  else if(n==2u)cap.max_metadata_string_bytes=0u;
  else if(n==3u)cap.max_results=0u;
  else cap.max_variables=0u;
  numeric_at(pc,cap);
 }
 d::inline_query_limits no_scopes{};no_scopes.max_scopes=0;path_check(pc,no_scopes);
 d::inline_query_limits no_strings{};no_strings.max_string_bytes=0;path_check(pc,no_strings);
 auto malformed=std::vector<d::scope_record>(scopes.begin(),scopes.end());require(!malformed.empty());malformed[0].parent=malformed.size();
 std::vector<d::inline_frame> rejected;std::size_t refused=7;
 auto refused_status=d::query_inline_frames(malformed,pc,rejected,{},owner->scope_paths(),&refused);
 require(refused_status==d::inline_query_error::malformed&&refused==d::no_record&&rejected.empty());

 auto check=[&](d::index const& actual,unsigned n){
  std::array<d::copied_numeric_local,256> copied{};
  copy_i32(copied,n);
  auto s=actual.scopes();auto t=actual.types();auto vars=actual.variables();
  std::vector<d::numeric_variable> slow,fast;auto proof=actual.numeric_validation();require(proof);
  auto a=d::query_numeric_variables(s,t,vars,pc,copied,copied.size(),slow,{},actual.scope_paths());
  auto b=d::query_numeric_variables(s,t,vars,pc,copied,copied.size(),fast,{},actual.scope_paths(),proof);
  require(a==d::inline_query_error::none&&a==b&&equal(slow,fast));

  // Named selection uses the same immutable proof, but retains per-query PC,
  // lexical/frame selection, ambiguity and caller-specific limits. No value
  // snapshot or read authority is retained by this optimization.
  unsigned named_checked{};
  for(auto const& candidate:slow){
   if(candidate.name.empty())continue;
   for(auto frame: {std::optional<std::size_t>{},std::optional<std::size_t>{0u}}){
    auto compare=[&](auto ss,auto vv,d::numeric_query_limits cap,auto supplied){
     d::variable_selection x{},y{};
     auto aa=d::query_named_variable(ss,t,vv,pc,candidate.name,x,cap,frame,actual.scope_paths());
     auto bb=d::query_named_variable(ss,t,vv,pc,candidate.name,y,cap,frame,actual.scope_paths(),supplied);
     require(aa==bb);
     require(x.identity==y.identity&&x.type==y.type&&x.scope==y.scope&&x.physical_scope==y.physical_scope&&
       x.location_available==y.location_available&&x.global==y.global&&x.static_storage==y.static_storage&&
       x.location.kind==y.location.kind&&x.location.reason==y.location.reason&&x.location.local_index==y.location.local_index&&
       x.location.constant_bits==y.location.constant_bits&&x.location.displacement==y.location.displacement);
     return aa;
    };
    compare(s,vars,{},proof);
    compare(s,vars,{},owner->numeric_validation()); // another index must fall back.
    d::numeric_query_limits tight{};tight.max_metadata_string_bytes=0u;
    require(compare(s,vars,tight,proof)!=d::inline_query_error::none);
    auto invalid=std::vector<d::variable_record>(vars.begin(),vars.end());require(!invalid.empty());
    invalid[0].scope=s.size();
    require(compare(s,std::span<d::variable_record const>{invalid},{},proof)==d::inline_query_error::malformed);
   }
   if(++named_checked==2u)break;
  }
  require(named_checked>0u);

  bool changed{};for(auto const&v:fast)if(v.kind!=d::numeric_kind::unavailable&&v.bits==n)changed=true;require(changed);
  for(unsigned k{};k!=3;++k){
   d::numeric_query_limits tight{};
   if(k==0)tight.max_metadata_string_bytes=0;else if(k==1)tight.max_locations=0;else tight.max_results=0;
   a=d::query_numeric_variables(s,t,vars,pc,copied,copied.size(),slow,tight,actual.scope_paths());
   b=d::query_numeric_variables(s,t,vars,pc,copied,copied.size(),fast,tight,actual.scope_paths(),proof);
   require(a==b&&equal(slow,fast)&&a!=d::inline_query_error::none);
  }
  auto foreign=std::vector<d::variable_record>(vars.begin(),vars.end());require(!foreign.empty());foreign[0].scope=s.size();
  require(!proof->accepts(s,t,foreign,{}));
  a=d::query_numeric_variables(s,t,foreign,pc,copied,copied.size(),slow,{},actual.scope_paths());
  b=d::query_numeric_variables(s,t,foreign,pc,copied,copied.size(),fast,{},actual.scope_paths(),proof);
  require(a==d::inline_query_error::malformed&&a==b&&slow.empty()&&fast.empty());
  require(!proof->accepts(s,t,vars.subspan(1),{}));
  a=d::query_numeric_variables(s,t,vars,pc,copied,copied.size(),slow,{},actual.scope_paths(),owner->numeric_validation());
  b=d::query_numeric_variables(s,t,vars,pc,copied,copied.size(),fast,{},actual.scope_paths());
  require(a==b&&equal(slow,fast));
 };
 for(unsigned n=1;n!=65;++n)check(*owner,n);
 std::unique_ptr<d::index> second{};require(d::index::parse({sections,code_size,4u},second)==d::error::none&&second);
 require(!owner->numeric_validation()->accepts(second->scopes(),second->types(),second->variables(),{}));
 std::array<std::thread,6> threads;
 for(unsigned i{};i!=threads.size();++i)threads[i]=std::thread([&,i]{for(unsigned n=1;n!=9;++n)check(i%2?*owner:*second,100u+i*16u+n);});
 for(auto&t:threads)t.join();
 std::cout<<"owned numeric metadata: scope-boundary candidate selection, fresh values, strict caps, foreign malformed spans, alternate owners and six concurrent readers passed\n";
}
