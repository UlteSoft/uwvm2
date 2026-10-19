#include <fast_io.h>
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <cstdlib>
#include <thread>
#include <array>
#include <uwvm2/uwvm/debugger/source_pretty_printer.h>
namespace d=uwvm2::uwvm::debugger::source_dwarf;
static void require(bool ok){if(!ok)std::abort();}
int main(int argc,char**argv){
 require(argc==2);std::ifstream f(argv[1],std::ios::binary);require(bool(f));
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
 auto types=owner->types();

#if defined(UWVM_USE_THREAD_LOCAL)
 require(owner->object_validation()!=nullptr);
#else
 require(owner->object_validation()==nullptr);
#endif
 require(d::object_details::validate_uncached(types,{})==d::inline_query_error::none);
 std::size_t scalar=d::no_record,vector=d::no_record;
 for(std::size_t i{};i<types.size();++i){
  auto const&t=types[i];
  if(t.kind==d::type_kind::scalar&&t.byte_count==4&&t.encoding==5&&!t.atomic_scalar)scalar=i;
  if(t.name.starts_with("vector<int,")&&t.size_known&&t.byte_size==12)vector=i;
 }
 require(scalar!=d::no_record&&vector!=d::no_record);
 auto bad=std::vector<d::type_record>(types.begin(),types.end());
 bad[scalar].referenced_type=bad.size();
 auto check=[&](d::index const& actual){
  auto own=actual.types();d::object_validation_scope scope{actual.object_validation()};
  std::vector<d::object_node> tree;std::array<std::byte,4> value{};
  for(unsigned n=1;n!=65;++n){
   value[0]=static_cast<std::byte>(n);
   require(d::query_object_value(own,scalar,value,tree)==d::inline_query_error::none);
   require(tree.size()==1&&tree[0].value_available&&tree[0].bits==n);
  }
  require(d::query_object_value(bad,scalar,value,tree)==d::inline_query_error::malformed&&tree.empty());
  d::object_query_limits tight{};tight.max_types=own.size()-1;
  require(d::query_object_value(own,scalar,value,tree,tight)==d::inline_query_error::limit_exceeded);
  tight={};tight.max_metadata_string_bytes=0;
  require(d::query_object_value(own,scalar,value,tree,tight)==d::inline_query_error::limit_exceeded);
  tight={};tight.max_object_bytes=1;
  require(d::query_object_value(own,scalar,value,tree,tight)==d::inline_query_error::limit_exceeded);
  std::array<std::byte,12> object{};
  auto put=[&](unsigned at,unsigned v){for(unsigned i=0;i!=4;++i)object[at+i]=static_cast<std::byte>((v>>(8*i))&255);};
  put(0,256);put(4,260);put(8,260);
  require(d::query_object_value(own,vector,object,tree)==d::inline_query_error::none);
  unsigned reads{};auto denied=[&](std::uint64_t,std::size_t,std::vector<std::byte>&){++reads;return false;};
  auto before=tree.size();
  auto result=uwvm2::uwvm::debugger::source_pretty_printer::format(own,vector,object,{},{},4,denied,tree);
  require(reads>0&&result!=uwvm2::uwvm::debugger::source_pretty_printer::outcome::formatted);
  require(tree.size()==before&&!tree[0].pretty_available);
 };
 std::unique_ptr<d::index> second{};
 require(d::index::parse({sections,code_size,4u},second)==d::error::none&&second);
 {
  d::object_validation_scope outer{owner->object_validation()};
  check(*second);check(*owner);
  {d::object_validation_scope disabled{nullptr};check(*second);}
  check(*owner);
 }
 std::array<std::thread,6> threads;
 for(unsigned i{};i!=threads.size();++i)threads[i]=std::thread([&,i]{for(unsigned n{};n!=8;++n)check(i%2?*owner:*second);});
 for(auto&t:threads)t.join();
 check(*owner);
 std::cout<<"owned metadata validation: fresh values, foreign malformed span, stricter schema/object limits, denied reads, reentrancy and six threads passed\n";
}
