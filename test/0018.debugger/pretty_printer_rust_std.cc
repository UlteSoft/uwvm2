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
 d::limits small{};small.max_total_string_bytes=1024u*1024u;std::unique_ptr<d::index> denied_owner;
 require(d::index::parse({sections,code_size,4u},denied_owner,small)==d::error::limit_exceeded && !denied_owner);

 // Production LLVM plain-local memory recipes remain distinct from copied
 // values. Exact carrier width, captured-prefix bounds and availability are
 // checked before the existing authenticated reader can receive an offset.
 for(unsigned width:{4u,8u}){
  std::array<std::byte,3> expression{std::byte{0xed},std::byte{0},std::byte{1}};
  auto plan=d::decode_single_location_plan(expression,width);
  require(plan.kind==d::plan_kind::wasm_local_guest_offset && plan.reason==d::unavailable_reason::none);
  std::array<d::copied_numeric_local,2> locals{};auto& local=locals[1];
  local.available=true;local.wasm_type=width==4u?0x7f:0x7e;
  std::uint64_t expected=width==4u?0xfedcba98u:0xfedcba9876543210ull;
  if(width==4u){auto x=static_cast<std::uint32_t>(expected);std::memcpy(local.bytes.data(),&x,4);}
  else std::memcpy(local.bytes.data(),&expected,8);
  std::uint64_t offset=42;
  require(d::resolve_local_guest_offset(plan,locals,2,offset)==d::object_location_error::none && offset==expected);
  auto check=[&](d::object_location_error expected_error){
   offset=42;require(d::resolve_local_guest_offset(plan,locals,2,offset)==expected_error && offset==0);
  };
  local.available=false;check(d::object_location_error::local_unavailable);local.available=true;
  local.wasm_type=0x7d;check(d::object_location_error::carrier_mismatch);local.wasm_type=width==4u?0x7f:0x7e;
  plan.local_index=2;check(d::object_location_error::local_not_captured);plan.local_index=1;
  plan.storage=d::wasm_location_space::global;check(d::object_location_error::malformed_metadata);plan.storage=d::wasm_location_space::local;
  plan.integer_transform_count=1;check(d::object_location_error::malformed_metadata);plan.integer_transform_count=0;
  plan.reason=d::unavailable_reason::no_location;check(d::object_location_error::malformed_metadata);plan.reason=d::unavailable_reason::none;
  offset=42;require(d::resolve_local_guest_offset(plan,locals,1,offset)==d::object_location_error::malformed_metadata && offset==0);
  for(auto space:{1u,2u}){
   expression[1]=std::byte(space);auto rejected=d::decode_single_location_plan(expression,width);
   require(rejected.kind==d::plan_kind::unavailable);
  }
  std::array<std::byte,4> direct{std::byte{0xed},std::byte{0},std::byte{1},std::byte{0x9f}};
  require(d::decode_single_location_plan(direct,width).kind==d::plan_kind::wasm_local_value);
 }

 auto scopes=owner->scopes();auto vars=owner->variables();
 require(owner->numeric_validation() && owner->numeric_validation()->accepts(scopes,owner->types(),vars,{}));
 std::size_t metadata_strings{},metadata_locations{};
 require(d::value_details::validate_numeric_metadata(scopes,owner->types(),vars,{},metadata_strings,metadata_locations)==d::inline_query_error::none);
 require(metadata_strings>1024u*1024u && metadata_strings<=8u*1024u*1024u);
 bool selected_marker{};
 for(auto const& scope:scopes)if(scope.name=="pretty_rust_ready"){
  require(scope.ranges.size()==1 && scope.ranges[0].begin<scope.ranges[0].end);
  auto pc=scope.ranges[0].begin;
  for(auto name:{"values","title","slice","text","words","units"}){
   d::variable_selection selected;
   require(d::query_named_variable(scopes,owner->types(),vars,pc,name,selected,{},0u,owner->scope_paths(),owner->numeric_validation())==d::inline_query_error::none);
   require(selected.location_available && selected.type<owner->types().size());
   if(std::string_view{name}=="slice" || std::string_view{name}=="text")
    require(selected.location.kind==d::plan_kind::wasm_local_guest_offset);
   else require(selected.location.kind==d::plan_kind::frame_relative_offset);
   d::numeric_query_limits tight{};tight.max_metadata_string_bytes=1024u*1024u;
   require(d::query_named_variable(scopes,owner->types(),vars,pc,name,selected,tight,0u,owner->scope_paths(),owner->numeric_validation())==d::inline_query_error::limit_exceeded);
  }
  selected_marker=true;
 }
 require(selected_marker);
 auto types=owner->types();
 namespace pp=uwvm2::uwvm::debugger::source_pretty_printer;
 auto pick=[&](std::string_view name,std::uint64_t unit=~std::uint64_t{}){
  std::size_t found=d::no_record;
  for(std::size_t i{};i!=types.size();++i)if(types[i].language==0x1c && types[i].name.starts_with(name) &&
      (unit==~std::uint64_t{} || types[i].identity.unit==unit)) {require(found==d::no_record);found=i;}
  require(found!=d::no_record);return found;
 };
 auto vi=pick("Vec<i32,");auto unit=types[vi].identity.unit;
 auto vs=pick("Vec<alloc::string::String,",unit), vu=pick("Vec<(),",unit);
 auto st=pick("String",unit), sl=pick("&[i32]",unit), sr=pick("&str",unit);
 auto schema=[&](std::size_t root){
  pp::details::rust_sequence_layout out;require(pp::details::rust_sequence_schema(types,root,4,out));return out;
 };
 auto ints=schema(vi), words=schema(vs), units=schema(vu), string=schema(st), slice=schema(sl), str=schema(sr);
 require(types[ints.element].name=="i32" && types[ints.element].byte_size==4 && !ints.zero_sized);
 require(words.element==st && d::rust_unit(types[units.element]) && units.zero_sized);
 require(string.string && str.string && slice.element==ints.element && slice.capacity.type==d::no_record);
 require(types[types[ints.data.type].referenced_type].byte_size==1); // real erased u8* storage
 auto write=[&](std::vector<std::byte>& b,pp::details::field field,std::uint64_t value){
  require(field.type<types.size());auto n=types[field.type].byte_size;
  require(n==4 && field.offset<=b.size() && n<=b.size()-field.offset);
  for(std::uint64_t i{};i!=n;++i)b[field.offset+i]=std::byte((value>>(8*i))&255);
 };
 auto descriptor=[&](std::size_t root,std::uint64_t address,std::uint64_t length,std::uint64_t capacity){
  auto meta=schema(root);std::vector<std::byte> b(types[root].byte_size);
  write(b,meta.data,address);write(b,meta.length,length);
  if(meta.capacity.type!=d::no_record)write(b,meta.capacity,capacity);return b;
 };
 std::map<std::uint64_t,std::vector<std::byte>> memory;std::size_t reads{};
 auto reader=[&](std::uint64_t address,std::size_t count,std::vector<std::byte>& out){
  ++reads;out.clear();require(count>0);
  for(auto const& [base,bytes]:memory)if(address>=base && address-base<=bytes.size() && count<=bytes.size()-(address-base)){
   out.assign(bytes.begin()+(address-base),bytes.begin()+(address-base)+count);return true;
  }return false;
 };
 std::vector<std::byte> values(64);
 for(unsigned i{};i!=16;++i){auto v=i*3+1;for(unsigned j{};j!=4;++j)values[i*4+j]=std::byte((v>>(8*j))&255);}
 memory[0x1000]=values;
 std::string text="native Rust \xce\xbb printer";require(text.size()==22);
 memory[0x2000]=std::vector<std::byte>(reinterpret_cast<std::byte const*>(text.data()),reinterpret_cast<std::byte const*>(text.data()+text.size()));
 auto raw=[&](std::span<d::type_record const> actual,std::size_t root,std::vector<std::byte> const& bytes){
  std::vector<d::object_node> tree;require(d::query_object_value(actual,root,bytes,tree)==d::inline_query_error::none);require(!tree.empty());return tree;
 };
 auto indexed=[](std::vector<d::object_node> const& tree){
  std::vector<d::object_node> out;
  for(auto const& n:tree)if(n.parent==0 && n.detached_copy && n.name.starts_with("["))out.push_back(n);
  return out;
 };
 auto show=[&](std::size_t root,std::vector<std::byte> const& bytes,pp::limits cap=pp::limits{}){
  auto tree=raw(types,root,bytes);require(pp::format(types,root,bytes,{}, {},4,reader,tree,cap)==pp::outcome::formatted);require(tree[0].pretty_available);return tree;
 };
 auto descriptor_ints=descriptor(vi,0x1000,16,32);reads=0;
 auto tree=show(vi,descriptor_ints);auto children=indexed(tree);
 require(reads==1 && children.size()==16 && tree[0].pretty_summary.starts_with("size=16 capacity=32"));
 for(unsigned i{};i!=16;++i)require(children[i].type==ints.element && children[i].bits==i*3+1 && children[i].byte_size==4);
 std::size_t element{};std::uint64_t count{};bool bounded{};
 require(pp::details::index_element_type(types,vi,4,element,count,bounded) && element==ints.element && !bounded);
 pp::details::indexed_sequence plan;
 require(pp::details::index_plan(types,vi,descriptor_ints,{}, {},4,plan) && plan.element==ints.element && plan.address==0x1000 && plan.count==16);
 reads=0;auto borrowed=show(sl,descriptor(sl,0x1008,4,0));children=indexed(borrowed);
 require(reads==1 && children.size()==4);
 for(unsigned i{};i!=4;++i)require(children[i].bits==(i+2)*3+1 && children[i].type==ints.element);
 for(auto root:{st,sr}){
  reads=0;auto rendered=show(root,descriptor(root,0x2000,text.size(),text.size()));
  require(reads==1 && rendered[0].display_text_available && rendered[0].display_text==text);
 }
 std::vector<std::byte> wordbytes(3*types[st].byte_size);std::string const expected[]{"first","second","\xce\xbb"};
 for(unsigned i{};i!=3;++i){
  auto address=0x4000+i*0x100;auto const& value=expected[i];
  memory[address]=std::vector<std::byte>(reinterpret_cast<std::byte const*>(value.data()),reinterpret_cast<std::byte const*>(value.data()+value.size()));
  auto desc=descriptor(st,address,value.size(),value.size());
  std::copy(desc.begin(),desc.end(),wordbytes.begin()+i*types[st].byte_size);
 }
 memory[0x3000]=wordbytes;reads=0;
 auto nested=show(vs,descriptor(vs,0x3000,3,3));children=indexed(nested);
 require(reads==4 && children.size()==3);
 for(unsigned i{};i!=3;++i)require(children[i].type==st && children[i].display_text_available && children[i].display_text==expected[i]);
 auto zero=descriptor(vu,1,7,0);pp::limits no_reads{};no_reads.maximum_reads=0;reads=0;
 tree=show(vu,zero,no_reads);children=indexed(tree);
 require(reads==0 && children.size()==7 && tree[0].pretty_summary.starts_with("size=7 capacity=4294967295"));
 for(auto const& child:children)
 {
  require(child.type==units.element && child.byte_size==0 && child.pretty_summary=="()" && !child.value_available);
  auto wire=::fast_io::concat_std(d::object_node_details{&child});
  require(wire.find(", value=")==std::string::npos && wire.find(", pretty=\"()\"")!=std::string::npos);
 }
 require(pp::details::index_plan(types,vu,zero,{}, {},4,plan) && plan.element==units.element && plan.count==7 && plan.copied_storage);
 auto deny=[&](std::span<d::type_record const> actual,std::vector<std::byte> const& bytes,
               std::span<std::byte const> known=std::span<std::byte const>{},std::span<std::byte const> eligible=std::span<std::byte const>{}){
  auto original=raw(actual,vi,bytes);reads=0;
  require(pp::format(actual,vi,bytes,known,eligible,4,reader,original)!=pp::outcome::formatted && reads==0 && !original[0].pretty_available);
 };
 auto bad_capacity=descriptor(vi,0x1000,16,8);deny(types,bad_capacity);
 auto overflow=descriptor(vi,0xfffffff0,16,32);deny(types,overflow);
 std::vector<std::byte> known(descriptor_ints.size(),std::byte{255});
 known[ints.length.offset]=std::byte{};deny(types,descriptor_ints,known);
 known.assign(known.size(),std::byte{255});known[ints.data.offset]=std::byte{};deny(types,descriptor_ints,{},known);
 auto foreign=std::vector<d::type_record>(types.begin(),types.end());pp::details::field buf{};
 require(pp::details::path(types,vi,"buf",buf));require(!foreign[buf.type].template_types.empty());
 foreign[buf.type].template_types[0]=types[ints.data.type].referenced_type;deny(foreign,descriptor_ints);
 pp::limits prefix{};prefix.max_elements=3;reads=0;tree=show(vi,descriptor_ints,prefix);
 require(reads==1 && indexed(tree).size()==3 && tree[0].pretty_truncated && tree[0].pretty_omitted==13);
 std::cout<<"PASS genuine Rust DWARF production-default index and copied-DATA Vec/String/slice/nested/ZST/schema/mask/bounds/caps; no native VM qualification\n";
}
