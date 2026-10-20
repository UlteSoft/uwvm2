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
  std::size_t found=d::no_record;
  for(std::size_t i{};i!=types.size();++i)if(types[i].language==0x1c && types[i].name.starts_with(name) &&
      (unit==~std::uint64_t{} || types[i].identity.unit==unit)) {require(found==d::no_record);found=i;}
  require(found!=d::no_record);return found;
 };
 auto vi=pick("VecDeque<i32,");auto unit=types[vi].identity.unit;
 auto vs=pick("VecDeque<alloc::string::String,",unit),vu=pick("VecDeque<(),",unit);
 auto schema=[&](std::size_t root){pp::details::rust_deque_layout out;require(pp::details::rust_deque_schema(types,root,4,out));return out;};
 auto ints=schema(vi),words=schema(vs),units=schema(vu);auto st=words.buffer.element;
 require(types[ints.buffer.element].name=="i32" && types[ints.buffer.element].byte_size==4);
 require(types[types[ints.buffer.data.type].referenced_type].name=="u8");
 require(d::rust_unit(types[units.buffer.element]) && units.buffer.zero_sized);
 auto write=[&](std::vector<std::byte>& bytes,pp::details::field field,std::uint64_t value){
  require(field.type<types.size() && types[field.type].byte_size==4 && field.offset+4<=bytes.size());
  for(unsigned i{};i!=4;++i)bytes[field.offset+i]=std::byte((value>>(8*i))&255);
 };
 auto descriptor=[&](std::size_t root,std::uint64_t address,std::uint64_t length,std::uint64_t capacity,std::uint64_t head){
  auto meta=schema(root);std::vector<std::byte> bytes(types[root].byte_size);
  write(bytes,meta.buffer.data,address);write(bytes,meta.buffer.length,length);write(bytes,meta.buffer.capacity,capacity);write(bytes,meta.head,head);return bytes;
 };
 std::map<std::uint64_t,std::vector<std::byte>> memory;std::vector<std::pair<std::uint64_t,std::size_t>> reads;
 auto reader=[&](std::uint64_t address,std::size_t count,std::vector<std::byte>& out){
  reads.emplace_back(address,count);require(count>0);out.clear();
  for(auto const& [base,bytes]:memory)if(address>=base && address-base<=bytes.size() && count<=bytes.size()-(address-base)){
   out.assign(bytes.begin()+(address-base),bytes.begin()+(address-base)+count);return true;
  }return false;
 };
 std::vector<std::byte> values(32);
 for(unsigned i{};i!=8;++i){auto value=i<6?i+8:i;for(unsigned j{};j!=4;++j)values[i*4+j]=std::byte((value>>(8*j))&255);}
 memory[0x1000]=values;
 auto raw=[&](std::span<d::type_record const> actual,std::size_t root,std::vector<std::byte> const& bytes){
  std::vector<d::object_node> tree;require(d::query_object_value(actual,root,bytes,tree)==d::inline_query_error::none && !tree.empty());return tree;
 };
 auto indexed=[](std::vector<d::object_node> const& tree){
  std::vector<d::object_node> out;for(auto const& n:tree)if(n.parent==0 && n.detached_copy && n.name.starts_with("["))out.push_back(n);return out;
 };
 auto show=[&](std::size_t root,std::vector<std::byte> const& bytes,pp::limits cap=pp::limits{}){
  auto tree=raw(types,root,bytes);require(pp::format(types,root,bytes,{}, {},4,reader,tree,cap)==pp::outcome::formatted && tree[0].pretty_available);return tree;
 };
 auto ring=descriptor(vi,0x1000,8,8,6);reads.clear();auto tree=show(vi,ring);auto children=indexed(tree);
 require(reads==decltype(reads){{0x1018,8},{0x1000,24}} && children.size()==8);
 require(tree[0].pretty_printer=="Rust VecDeque" && tree[0].pretty_summary.starts_with("size=8 capacity=8"));
 for(unsigned i{};i!=8;++i)require(children[i].type==ints.buffer.element && children[i].bits==i+6);
 pp::details::indexed_sequence plan;require(pp::details::index_plan(types,vi,ring,{}, {},4,plan));
 std::size_t element{};std::uint64_t bound{};bool bounded{};
 require(pp::details::index_element_type(types,vi,4,element,bound,bounded) && element==ints.buffer.element && !bounded);
 for(unsigned i{};i!=8;++i){std::uint64_t address{};require(pp::details::indexed_address(plan,i,4,4,address) && address==0x1000+((i+6)%8)*4);}
 namespace le=uwvm2::uwvm::debugger::source_language_expression;
 for(unsigned i{};i!=8;++i){
  d::source_expression_step step{};step.kind=d::source_expression_step_kind::index;step.index=i;
  std::vector<d::object_node> selected;reads.clear();
  require(le::value(types,vi,{&step,1},ring,{},4,reader,selected)==d::inline_query_error::none);
  require(reads.size()==1 && selected.size()==1 && selected[0].bits==i+6);
  std::vector<d::object_node> selected_type;
  require(le::type(types,vi,{&step,1},4,selected_type)==d::inline_query_error::none && selected_type[0].type==ints.buffer.element);
 }
 pp::limits prefix{};prefix.max_elements=3;reads.clear();tree=show(vi,ring,prefix);
 require(reads==decltype(reads){{0x1018,8},{0x1000,4}} && indexed(tree).size()==3 && tree[0].pretty_omitted==5);
 prefix.max_elements=2;reads.clear();tree=show(vi,ring,prefix);require(reads==decltype(reads){{0x1018,8}} && indexed(tree).size()==2);
 prefix.max_elements=0;reads.clear();tree=show(vi,ring,prefix);require(reads.empty() && indexed(tree).empty() && tree[0].pretty_omitted==8);
 reads.clear();tree=show(vi,descriptor(vi,0,0,0,0));require(reads.empty() && tree[0].pretty_summary=="size=0 capacity=0 []");
 reads.clear();tree=show(vi,descriptor(vi,0x1000,2,8,2));children=indexed(tree);
 require(reads==decltype(reads){{0x1008,8}} && children.size()==2 && children[0].bits==10 && children[1].bits==11);
 pp::details::rust_sequence_layout strings;require(pp::details::rust_sequence_schema(types,st,4,strings));
 std::string const expected[]{"first","second","\xce\xbb","last"};std::vector<std::byte> wordbytes(types[st].byte_size*4);
 for(unsigned i{};i!=4;++i){
  auto address=0x4000+i*0x100;auto const& value=expected[i];memory[address]={reinterpret_cast<std::byte const*>(value.data()),reinterpret_cast<std::byte const*>(value.data()+value.size())};
  std::vector<std::byte> desc(types[st].byte_size);write(desc,strings.data,address);write(desc,strings.length,value.size());write(desc,strings.capacity,value.size());
  std::copy(desc.begin(),desc.end(),wordbytes.begin()+((i+3)%4)*types[st].byte_size);
 }
 memory[0x3000]=wordbytes;reads.clear();tree=show(vs,descriptor(vs,0x3000,4,4,3));children=indexed(tree);
 require(reads.size()==6 && children.size()==4);
 for(unsigned i{};i!=4;++i)require(children[i].display_text_available && children[i].display_text==expected[i]);
 pp::limits no_reads{};no_reads.maximum_reads=0;reads.clear();auto zst=descriptor(vu,1,7,0,0xfffffff8u);tree=show(vu,zst,no_reads);children=indexed(tree);
 require(reads.empty() && children.size()==7 && tree[0].pretty_summary.starts_with("size=7 capacity=4294967295"));
 for(auto const& child:children)require(child.pretty_summary=="()" && child.byte_size==0 && !child.value_available);
 require(pp::details::index_plan(types,vu,zst,{}, {},4,plan) && plan.copied_storage && plan.count==7);
 auto deny=[&](std::span<d::type_record const> actual,std::vector<std::byte> const& bytes,
  std::span<std::byte const> known=std::span<std::byte const>{},std::span<std::byte const> eligible=std::span<std::byte const>{}){
  auto original=raw(actual,vi,bytes);auto nodes=original.size();reads.clear();
  require(pp::format(actual,vi,bytes,known,eligible,4,reader,original)!=pp::outcome::formatted && reads.empty() && original.size()==nodes && !original[0].pretty_available);
  pp::details::indexed_sequence denied;require(!pp::details::index_plan(actual,vi,bytes,known,eligible,4,denied));
 };
 deny(types,descriptor(vi,0x1000,9,8,6));deny(types,descriptor(vi,0x1000,1,0,0));deny(types,descriptor(vi,0x1000,1,8,8));
 deny(types,descriptor(vi,0,1,8,0));deny(types,descriptor(vi,0xffffffe0u,1,9,0)); // full allocation overflows even though selected prefix fits
 for(auto field:{ints.buffer.data,ints.buffer.length,ints.buffer.capacity,ints.head}){
  std::vector<std::byte> known(ring.size(),std::byte{255});known[field.offset]=std::byte{};deny(types,ring,known);
 }
 std::vector<std::byte> eligible(ring.size(),std::byte{255});eligible[ints.buffer.data.offset]=std::byte{};deny(types,ring,{},eligible);
 auto foreign=std::vector<d::type_record>(types.begin(),types.end());pp::details::field buf{};require(pp::details::path(types,vi,"buf",buf));
 require(!foreign[buf.type].template_types.empty());foreign[buf.type].template_types[0]=types[ints.buffer.data.type].referenced_type;deny(foreign,ring);
 foreign.assign(types.begin(),types.end());foreign[ints.head.type].encoding=5u;deny(foreign,ring);
 reads.clear();auto original=raw(types,vi,ring);pp::limits one_read{};one_read.maximum_reads=1;
 require(pp::format(types,vi,ring,{}, {},4,reader,original,one_read)==pp::outcome::limit_exceeded && reads.empty() && !original[0].pretty_available);
 // A failed second segment must not commit the successful first segment.
 memory.erase(0x1000);memory[0x1018]={values.begin()+24,values.end()};reads.clear();original=raw(types,vi,ring);
 require(pp::format(types,vi,ring,{}, {},4,reader,original)==pp::outcome::unavailable && reads.size()==2 && !original[0].pretty_available && indexed(original).empty());
 std::cout<<"PASS genuine Rust DWARF VecDeque ring/index/nested/ZST/schema/mask/allocation/caps/atomic fallback; copied DATA, no live VM claim\n";
}
