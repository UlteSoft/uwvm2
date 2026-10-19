// Owned DATA only; no live Wasm/GDB/LLDB qualification.
#include <uwvm2/uwvm/debugger/source_pretty_array.h>
#include <cstdio>
#include <cstdlib>
namespace d=uwvm2::uwvm::debugger::source_dwarf;
namespace p=uwvm2::uwvm::debugger::source_pretty_printer;
static void check(bool v,char const* why){if(!v){std::fprintf(stderr,"FAIL std::array DATA: %s\n",why);std::exit(1);}}
int main(){
 for(unsigned width:{4u,8u})for(bool libcxx:{false,true})for(bool pointers:{false,true}){
  std::vector<d::type_record> t(4);auto& element=t[0];element.kind=pointers?d::type_kind::pointer:d::type_kind::scalar;
  element.language=0x21;element.encoding=5;element.byte_count=pointers?width:4;element.byte_size=element.byte_count;element.size_known=true;
  if(pointers){element.referenced_type=3;}
  auto& size=t[3];size.kind=d::type_kind::scalar;size.language=0x21;size.encoding=7;size.size_known=true;size.byte_count=width;size.byte_size=width;
  auto& raw=t[1];raw.kind=d::type_kind::array;raw.language=0x21;raw.referenced_type=0;raw.size_known=true;raw.byte_size=element.byte_size*2;
  raw.dimensions={{0,2,true,true}};
  auto& array=t[2];array.kind=d::type_kind::structure;array.language=0x21;array.name=libcxx?"std::__1::array<int,2>":"std::array<int,2>";
  array.size_known=true;array.byte_size=raw.byte_size;array.template_types={0};array.template_values={{1,3,2,false,true}};
  d::member_record field{};field.name=libcxx?"__elems_":"_M_elems";field.type=1;field.offset_known=true;array.members={field};
  std::vector<std::byte> bytes(array.byte_size),known(bytes.size(),std::byte{255}),eligible=known;bytes[0]=std::byte{7};bytes[element.byte_size]=std::byte{8};
  std::vector<d::object_node> tree;unsigned reads{};
  auto format=[&](p::limits cap={}){check(d::query_object_value_with_known_bits(t,2,bytes,known,tree)==d::inline_query_error::none,"owned raw layout");return p::format(t,2,bytes,known,eligible,width,[&](std::uint64_t,std::size_t,std::vector<std::byte>&){++reads;return false;},tree,cap);};
  check(format()==p::outcome::formatted&&reads==0&&tree.size()==3&&tree[1].name=="[0]"&&tree[2].name=="[1]"&&tree[1].parent==0&&tree[2].parent==0,"direct indexed children use copied storage");
  check(tree[1].bits==7&&tree[2].bits==8,"actual element bytes");
  known[0]=std::byte{};check(format()==p::outcome::formatted&&!tree[1].value_available&&tree[2].value_available&&reads==0,"unknown bits not synthesized");known[0]=std::byte{255};
  p::limits cap{};cap.max_elements=0;check(format(cap)==p::outcome::formatted&&tree.size()==1&&tree[0].pretty_omitted==2,"zero element budget");
  cap.max_elements=1;check(format(cap)==p::outcome::formatted&&tree.size()==2&&tree[0].pretty_omitted==1&&reads==0,"shared finite element budget");
  t[2].template_values[0].bits=3;check(format()!=p::outcome::formatted&&reads==0,"metadata count mismatch rejects");t[2].template_values[0].bits=2;
  t[2].members.push_back(field);check(format()!=p::outcome::formatted&&reads==0,"ambiguous storage rejects");t[2].members.pop_back();
  t[2].template_values[0].bits=0;t[2].byte_size=1;t[2].members.clear();bytes.resize(1);known.resize(1);eligible.resize(1);
  check(format()==p::outcome::formatted&&tree.size()==1&&tree[0].pretty_summary.find("size=0")!=std::string::npos&&reads==0,"empty array needs no fake element storage");
 }
 std::puts("PASS std::array copied-storage DATA");}
