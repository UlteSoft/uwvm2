#include <fast_io.h>
#include <map>
#include <source_location>
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <cstdlib>
#include <thread>
#include <array>
#include <uwvm2/uwvm/debugger/source_pretty_printer.h>
#include <uwvm2/uwvm/debugger/source_language_expression.h>
namespace d=uwvm2::uwvm::debugger::source_dwarf;
static void require(bool ok,std::source_location where=std::source_location::current()){if(!ok){std::cerr<<"Rust production-metadata DATA assertion line "<<where.line()<<std::endl;std::abort();}}
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

 require(code_size!=0u);std::unique_ptr<d::index> owner{};
 require(d::index::parse({sections,code_size,4u},owner)==d::error::none && owner);
 auto types=owner->types();
 namespace pp=uwvm2::uwvm::debugger::source_pretty_printer;
 auto pick=[&](std::string_view name,std::uint64_t unit=~std::uint64_t{}){
  for(std::size_t i{};i!=types.size();++i)if(types[i].language==0x1c && types[i].name==name && (unit==~std::uint64_t{}||types[i].identity.unit==unit))return i;
  require(false);return d::no_record;
 };
 auto ri=pick("Rc<i32, alloc::alloc::Global>");auto cu=types[ri].identity.unit;
 auto ai=pick("Arc<i32, alloc::alloc::Global>",cu),rs=pick("Rc<alloc::string::String, alloc::alloc::Global>",cu);
 auto av=pick("Arc<alloc::vec::Vec<i32, alloc::alloc::Global>, alloc::alloc::Global>",cu);
 auto ru=pick("Rc<(), alloc::alloc::Global>",cu),au=pick("Arc<(), alloc::alloc::Global>",cu);
 auto schema=[&](std::size_t root){pp::details::rust_shared_layout v;require(pp::details::rust_shared_schema(types,root,4,v));return v;};
 auto rc=schema(ri),arc=schema(ai);std::size_t rw=d::no_record,aw=d::no_record;
 for(std::size_t i{};i!=types.size();++i)if(types[i].identity.unit==cu && types[i].name=="Weak<i32, alloc::alloc::Global>"){
  auto v=schema(i);require(v.weak_owner);(v.atomic?aw:rw)=i;
 }
 require(rw!=d::no_record && aw!=d::no_record && !rc.atomic && arc.atomic && rc.header_bytes==8 && arc.header_bytes==8);
 std::map<std::uint64_t,std::vector<std::byte>> memory;std::vector<std::pair<std::uint64_t,std::size_t>> reads;
 auto reader=[&](std::uint64_t address,std::size_t size,std::vector<std::byte>& out){
  reads.emplace_back(address,size);out.clear();require(size>0);
  for(auto const&[base,b]:memory)if(address>=base && address-base<=b.size() && size<=b.size()-(address-base)){
   out.assign(b.begin()+(address-base),b.begin()+(address-base)+size);return true;
  }return false;
 };
 auto write=[&](std::vector<std::byte>& b,pp::details::field field,std::uint64_t value){
  require(field.type<types.size() && types[field.type].byte_size==4 && field.offset+4<=b.size());
  for(unsigned i{};i!=4;++i)b[field.offset+i]=std::byte((value>>(8*i))&255);
 };
 auto descriptor=[&](std::size_t root,std::uint64_t address){std::vector<std::byte>b(types[root].byte_size);write(b,schema(root).data,address);return b;};
 auto allocate=[&](std::size_t root,std::uint64_t address,std::uint64_t strong,std::uint64_t weak,std::vector<std::byte> const& value){
  auto v=schema(root);std::vector<std::byte>b(types[v.inner].byte_size);write(b,v.strong,strong);write(b,v.weak,weak);
  require(value.size()==types[v.value.type].byte_size);std::copy(value.begin(),value.end(),b.begin()+v.value.offset);memory[address]=b;
 };
 auto integer=[](std::uint32_t n){std::vector<std::byte>b(4);for(unsigned i{};i!=4;++i)b[i]=std::byte((n>>(8*i))&255);return b;};
 auto raw=[&](std::span<d::type_record const> actual,std::size_t root,std::vector<std::byte>const& b){std::vector<d::object_node>t;require(d::query_object_value(actual,root,b,t)==d::inline_query_error::none && !t.empty());return t;};
 auto child=[&](std::vector<d::object_node>const&t,std::string_view name)->d::object_node const&{for(auto const&n:t)if(n.parent==0 && n.detached_copy && n.name==name)return n;require(false);return t[0];};
 auto show=[&](std::size_t root,std::vector<std::byte>const&b,pp::limits cap=pp::limits{}){auto t=raw(types,root,b);require(pp::format(types,root,b,{}, {},4,reader,t,cap)==pp::outcome::formatted && t[0].pretty_available);return t;};
 allocate(ri,0x1000,3,3,integer(-17));auto desc=descriptor(ri,0x1000);reads.clear();auto tree=show(ri,desc);
 require(reads==decltype(reads){{0x1000,8},{0x1008,4}} && tree[0].pretty_printer=="Rust Rc" && tree[0].pretty_summary=="strong=3 weak=2 = -17");
 require(child(tree,"value").bits==0xffffffefu && child(tree,"strong").bits==3 && child(tree,"weak").bits==2);
 allocate(ai,0x2000,2,2,integer(42));reads.clear();tree=show(ai,descriptor(ai,0x2000));require(tree[0].pretty_printer=="Rust Arc" && tree[0].pretty_summary=="strong=2 weak=1 = 42");
 for(auto root:{rw,aw}){
  auto live=root==rw?0x1000:0x2000;reads.clear();tree=show(root,descriptor(root,live));require(reads.size()==2 && child(tree,"value").bits==(root==rw?0xffffffefu:42));
  auto v=schema(root);std::vector<std::byte>header(v.header_bytes);write(header,v.strong,0);write(header,v.weak,2);memory[0x3000]=header;
  reads.clear();tree=show(root,descriptor(root,0x3000));require(reads==decltype(reads){{0x3000,8}} && tree[0].pretty_summary=="expired strong=0 weak=0" && child(tree,"strong").bits==0 && child(tree,"weak").bits==0);
  for(auto const&n:tree)require(!n.detached_copy||n.name!="value");
  pp::limits cap{};cap.maximum_reads=0;reads.clear();tree=show(root,descriptor(root,0xffffffffu),cap);require(reads.empty() && tree[0].pretty_summary=="empty strong=0 weak=0");
 }
 for(auto root:{ru,au}){allocate(root,0x4000,1,1,{});reads.clear();tree=show(root,descriptor(root,0x4000));require(reads==decltype(reads){{0x4000,8}} && child(tree,"value").pretty_summary=="()" && tree[0].pretty_summary=="strong=1 weak=0 = ()");}
 auto string=schema(rs).value.type;pp::details::rust_sequence_layout layout;require(pp::details::rust_sequence_schema(types,string,4,layout));
 std::string text="rc-\xce\xbb";memory[0x6000]={reinterpret_cast<std::byte const*>(text.data()),reinterpret_cast<std::byte const*>(text.data()+text.size())};
 std::vector<std::byte>sv(types[string].byte_size);write(sv,layout.data,0x6000);write(sv,layout.length,text.size());write(sv,layout.capacity,text.size());allocate(rs,0x5000,1,1,sv);
 reads.clear();tree=show(rs,descriptor(rs,0x5000));require(reads.size()==3 && child(tree,"value").display_text==text);
 auto vec=schema(av).value.type;require(pp::details::rust_sequence_schema(types,vec,4,layout));std::vector<std::byte>vv(types[vec].byte_size);
 write(vv,layout.data,0x8000);write(vv,layout.length,2);write(vv,layout.capacity,2);auto values=integer(4),second=integer(9);values.insert(values.end(),second.begin(),second.end());memory[0x8000]=values;allocate(av,0x7000,2,1,vv);
 reads.clear();tree=show(av,descriptor(av,0x7000));require(reads.size()==3 && child(tree,"value").pretty_summary=="size=2 capacity=2 [4, 9]");
 namespace le=uwvm2::uwvm::debugger::source_language_expression;d::source_expression_step step{};step.kind=d::source_expression_step_kind::dereference;
 for(auto root:{ri,ai,rs,av,ru,au}){
  auto address=root==ri?0x1000:root==ai?0x2000:root==rs?0x5000:root==av?0x7000:0x4000;auto b=descriptor(root,address);std::vector<d::object_node> selected;
  reads.clear();require(le::value(types,root,{&step,1},b,{},4,reader,selected)==d::inline_query_error::none && !selected.empty() && selected[0].type==schema(root).value.type);
  require(reads.size()==(root==ru||root==au?1u:root==rs||root==av?3u:2u)); // String's final display may read the text.
  require(le::type(types,root,{&step,1},4,selected)==d::inline_query_error::none && selected[0].type==schema(root).value.type);
 }
 for(auto root:{rw,aw}){std::vector<d::object_node>out;reads.clear();require(le::value(types,root,{&step,1},descriptor(root,0x3000),{},4,reader,out)!=d::inline_query_error::none && reads.empty());require(le::type(types,root,{&step,1},4,out)!=d::inline_query_error::none);}
 auto reject=[&](std::span<d::type_record const> actual,std::size_t root,std::vector<std::byte>const& b,std::span<std::byte const> known={},std::span<std::byte const> eligible={}){
  auto t=raw(actual,root,b);auto n=t.size();reads.clear();require(pp::format(actual,root,b,known,eligible,4,reader,t)!=pp::outcome::formatted && reads.empty() && !t[0].pretty_available && t.size()==n);
 };
 reject(types,ri,descriptor(ri,0));reject(types,ri,descriptor(ri,0xfffffff8u));
 std::vector<std::byte>mask(desc.size(),std::byte{255});mask[rc.data.offset]=std::byte{};reject(types,ri,desc,mask);reject(types,ri,desc,{},mask);
 auto bad=std::vector<d::type_record>(types.begin(),types.end());bad[ri].template_types[0]=schema(rs).value.type;reject(bad,ri,desc);
 bad.assign(types.begin(),types.end());bad[rc.strong.type].encoding=5;reject(bad,ri,desc);
 bad.assign(types.begin(),types.end());bad[rc.inner].members[1].byte_offset=0;reject(bad,ri,desc);
 bad.assign(types.begin(),types.end());bad[rc.value.type].atomic_scalar=true;reject(bad,ri,desc);
 for(auto counts:std::array<std::pair<unsigned,unsigned>,3>{{{0,1},{1,0},{0,0}}}){allocate(ri,0x9000,counts.first,counts.second,integer(7));reads.clear();auto t=raw(types,ri,descriptor(ri,0x9000));require(pp::format(types,ri,descriptor(ri,0x9000),{}, {},4,reader,t)==pp::outcome::unavailable && reads.size()==1 && !t[0].pretty_available);}
 allocate(ai,0x9000,1,0xffffffffu,integer(7));reads.clear();auto original=raw(types,ai,descriptor(ai,0x9000));require(pp::format(types,ai,descriptor(ai,0x9000),{}, {},4,reader,original)==pp::outcome::unavailable && reads.size()==1 && !original[0].pretty_available);
 for(unsigned budget=0;budget!=2;++budget){pp::limits cap{};cap.maximum_reads=budget;reads.clear();original=raw(types,ri,desc);auto result=pp::format(types,ri,desc,{}, {},4,reader,original,cap);require(result==pp::outcome::limit_exceeded && reads.size()==budget && !original[0].pretty_available);}
 pp::limits cap{};cap.max_elements=0;reads.clear();tree=show(ri,desc,cap);require(reads.size()==1 && tree[0].pretty_omitted==3);
 cap.max_elements=1;reads.clear();tree=show(ri,desc,cap);require(reads.size()==2 && tree[0].pretty_omitted==2 && child(tree,"value").bits==0xffffffefu);
 cap=pp::limits{};cap.max_bytes=7;reads.clear();original=raw(types,ri,desc);require(pp::format(types,ri,desc,{}, {},4,reader,original,cap)==pp::outcome::limit_exceeded && reads.empty() && !original[0].pretty_available);
 memory[0x1000].resize(8);reads.clear();original=raw(types,ri,desc);require(pp::format(types,ri,desc,{}, {},4,reader,original)==pp::outcome::unavailable && reads.size()==2 && !original[0].pretty_available);
 std::vector<d::object_node>out;le::limits lcap{};lcap.maximum_reads=1;reads.clear();require(le::value(types,ri,{&step,1},desc,{},4,reader,out,lcap)==d::inline_query_error::limit_exceeded && reads.empty());
 std::cout<<"PASS genuine Rust Rc/Arc/Weak ownership, expired/empty, nested/ZST, explicit Deref, schema/masks/budgets/atomic fallback; copied DATA\n";
}
