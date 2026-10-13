// Core 3 explicit table/global declarations, including imports, protected section boundaries and retained policy.
#include <uwvm2/uwvm/wasm/feature/impl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>
namespace f=uwvm2::uwvm::wasm::feature;namespace base=uwvm2::parser::wasm::base;
using bytes=std::vector<std::byte>;using error=base::wasm_parse_error_code;unsigned checks{};
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
bytes raw(std::initializer_list<unsigned> v){bytes b;for(auto x:v)b.push_back(std::byte(x));return b;}
void leb(bytes& b,std::size_t n){do{auto x=n&127;n>>=7;b.push_back(std::byte(x|(n?128:0)));}while(n);}
auto features(){f::wasm_binfmt_ver1_feature_parameter_storage_t p{};auto& w=f::wasm_binfmt_ver1_wasm1p1_parameter(p);w.disable_function_references=false;w.disable_table_initializer=false;return p;}
auto parse(bytes const& b,auto const& p,error expected=error::ok){
 auto page=std::size_t(sysconf(_SC_PAGESIZE));CHECK(b.size()<page);auto memory=static_cast<std::byte*>(mmap(nullptr,2*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(memory!=MAP_FAILED);CHECK(mprotect(memory+page,page,PROT_NONE)==0);auto begin=memory+page-b.size();std::memcpy(begin,b.data(),b.size());
 base::error_impl err{};f::wasm_binfmt_ver1_module_storage_t m{};try{m=f::binfmt_ver1_handler(begin,begin+b.size(),err,p);}catch(fast_io::error const&){}
 if(err.err_code!=expected){std::fprintf(stderr,"parse mismatch: expected %u, actual %u, bytes:",unsigned(expected),unsigned(err.err_code));for(auto byte:b)std::fprintf(stderr," %02x",std::to_integer<unsigned>(byte));std::fputc('\n',stderr);}
 CHECK(err.err_code==expected);CHECK(munmap(memory,2*page)==0);return m;
}

namespace w1=uwvm2::parser::wasm::standard::wasm1::features;namespace w11=uwvm2::parser::wasm::standard::wasm1p1::features;
using tables=w1::table_section_storage_t<w1::wasm1,w11::wasm1p1>;using globals=w1::global_section_storage_t<w1::wasm1,w11::wasm1p1>;
using imports=w1::import_section_storage_t<w1::wasm1,w11::wasm1p1>;
auto const& table_section(auto const& m){return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<tables>(m.sections);}
auto const& global_section(auto const& m){return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<globals>(m.sections);}
auto const& import_section(auto const& m){return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<imports>(m.sections);}
bytes module(bool table,bool imported,bytes type,bool tail=true){
 auto payload=raw({1});if(imported){auto name=raw({1,'m',1,'x',table?1u:3u});payload.insert(payload.end(),name.begin(),name.end());}
 payload.insert(payload.end(),type.begin(),type.end());if(tail){auto suffix=table?raw({0,1}):raw({0});payload.insert(payload.end(),suffix.begin(),suffix.end());if(!table&&!imported){auto init=raw({0xd0,std::to_integer<unsigned>(type.back()),11});payload.insert(payload.end(),init.begin(),init.end());}}
 auto out=raw({0,0x61,0x73,0x6d,1,0,0,0,imported?2u:table?4u:6u});leb(out,payload.size());out.insert(out.end(),payload.begin(),payload.end());return out;
}
bytes with_type0(bytes const& body){auto out=raw({0,0x61,0x73,0x6d,1,0,0,0,1,4,1,0x60,0,0});out.insert(out.end(),body.begin()+8,body.end());return out;}
void section(bytes& out,unsigned id,bytes const& payload){out.push_back(static_cast<std::byte>(id));leb(out,payload.size());out.insert(out.end(),payload.begin(),payload.end());}
bytes typed_initializer_module(bool table,bool explicit_init,bytes declared_type,bytes init,bool distinct_type1=false,bool imported_global=false){
 auto out=raw({0,0x61,0x73,0x6d,1,0,0,0});
 section(out,1,distinct_type1?raw({2,0x60,0,0,0x60,1,0x7f,0}):raw({1,0x60,0,0}));
 if(imported_global){auto imp=raw({1,1,'m',1,'x',3,0x63,0,0});section(out,2,imp);}
 if(!init.empty()&&init[0]==std::byte{0xd2})section(out,3,raw({1,0}));
 if(table){auto payload=raw({1});if(explicit_init){auto prefix=raw({0x40,0});payload.insert(payload.end(),prefix.begin(),prefix.end());}payload.insert(payload.end(),declared_type.begin(),declared_type.end());auto limits=raw({0,1});payload.insert(payload.end(),limits.begin(),limits.end());if(explicit_init)payload.insert(payload.end(),init.begin(),init.end());section(out,4,payload);}
 else{auto payload=raw({1});payload.insert(payload.end(),declared_type.begin(),declared_type.end());payload.push_back(std::byte{0});payload.insert(payload.end(),init.begin(),init.end());section(out,6,payload);}
 if(!init.empty()&&init[0]==std::byte{0xd2})section(out,10,raw({1,2,0,11}));
 return out;
}
int main(){auto p=features();
 for(bool table:{false,true})for(bool imported:{false,true})for(unsigned heap:{0x70,0x6f}){
  auto data=module(table,imported,raw({0x63,heap}));auto m=parse(data,p);
  CHECK(!(table?table_section(m).requires_function_references:global_section(m).requires_function_references));
  if(!imported){if(table){CHECK(table_section(m).tables.size()==1);CHECK(static_cast<unsigned>(table_section(m).tables.index_unchecked(0).reftype)==heap);CHECK(table_section(m).tables.index_unchecked(0).has_core_type);}else{CHECK(global_section(m).local_globals.size()==1);CHECK(static_cast<unsigned>(global_section(m).local_globals.index_unchecked(0).global.type)==heap);CHECK(global_section(m).local_globals.index_unchecked(0).global.has_core_type);}}
  else {auto const* declaration=import_section(m).importdesc.index_unchecked(table?1uz:3uz).index_unchecked(0);CHECK(table?declaration->imports.storage.table.has_core_type:declaration->imports.storage.global.has_core_type);}
  tables table_copy{table_section(m)};globals global_copy{global_section(m)};m={};tables table_moved{std::move(table_copy)};globals global_moved{std::move(global_copy)};CHECK(!(table?table_moved.requires_function_references:global_moved.requires_function_references));
  auto disabled=p;f::wasm_binfmt_ver1_wasm1p1_parameter(disabled).disable_function_references=true;parse(data,disabled);
  disabled=p;f::wasm_binfmt_ver1_wasm1p1_parameter(disabled).disable_reference_types=true;parse(data,disabled,error::wasm1p1_feature_required);
  auto legacy=parse(module(table,imported,raw({heap})),p);CHECK(!(table?table_section(legacy).requires_function_references:global_section(legacy).requires_function_references));
 }
 for(bool table:{false,true})for(bool imported:{false,true})for(auto type:{raw({0x63,0x00}),raw({0x64,0x00})}){
  if(!imported&&type[0]==std::byte{0x64})continue; // The minimal definition fixtures use ref.null/implicit null, so only imports exercise non-null declarations here.
  auto data=with_type0(module(table,imported,type));auto m=parse(data,p);
  auto const& exact=imported?(table?import_section(m).importdesc.index_unchecked(1uz).index_unchecked(0)->imports.storage.table.core_type:import_section(m).importdesc.index_unchecked(3uz).index_unchecked(0)->imports.storage.global.core_type):
      (table?table_section(m).tables.index_unchecked(0).core_type:global_section(m).local_globals.index_unchecked(0).global.core_type);
  CHECK(exact.heap.code==0);CHECK(exact.nullable==(type[0]==std::byte{0x63}));
  auto disabled=p;f::wasm_binfmt_ver1_wasm1p1_parameter(disabled).disable_function_references=true;parse(data,disabled,error::wasm1p1_feature_required);
 }
 for(bool table:{false,true})for(bool imported:{false,true}){
  for(auto t:{raw({0x63}),raw({0x63,0xf0,0x7f}),raw({0x63,0x80,0x80,0x80,0x80,0x10})})parse(module(table,imported,t,false),p,error::illegal_value_type);
  for(auto t:{raw({0x63,0x6e}),raw({0x63,0x69})})parse(module(table,imported,t,false),p,error::wasm1p1_feature_required);
  parse(module(table,imported,raw({0x63,0}),false),p,error::illegal_type_index);
  auto escaped=module(table,imported,raw({0x63}),false);escaped.push_back(std::byte{0x70});parse(escaped,p,error::illegal_value_type);
 }
 parse(module(false,false,raw({0x63,0x70}),false),p,error::global_type_cannot_find_mut);
 // Core 3 validates the exact initializer type, not only the funcref carrier.
 parse(with_type0(module(false,false,raw({0x64,0}))),p,error::init_const_expr_type_mismatch);
 parse(typed_initializer_module(false,false,raw({0x63,0}),raw({0xd0,0,11})),p);
 parse(typed_initializer_module(false,false,raw({0x63,1}),raw({0xd0,0,11}),true),p,error::init_const_expr_type_mismatch);
 parse(typed_initializer_module(false,false,raw({0x64,0}),raw({0xd2,0,11})),p);
 parse(typed_initializer_module(false,false,raw({0x64,1}),raw({0xd2,0,11}),true),p,error::init_const_expr_type_mismatch);
 parse(typed_initializer_module(false,false,raw({0x64,0}),raw({0x23,0,11}),false,true),p,error::init_const_expr_type_mismatch);
 parse(typed_initializer_module(false,false,raw({0x63,0}),raw({0x23,0,11}),false,true),p);
 parse(typed_initializer_module(true,false,raw({0x64,0}),{}),p,error::init_const_expr_type_mismatch);
 parse(typed_initializer_module(true,true,raw({0x64,0}),raw({0xd0,0,11})),p,error::init_const_expr_type_mismatch);
 parse(typed_initializer_module(true,true,raw({0x64,0}),raw({0xd2,0,11})),p);
 {auto rich=p;auto& flags=f::wasm_binfmt_ver1_wasm1p1_parameter(rich);
  flags.disable_gc=false;flags.disable_exceptions=false;
  for(bool table:{false,true})for(auto h:{0x6eu,0x69u}){
   auto m=parse(module(table,false,raw({0x63,h})),rich);
   auto const& core=table?table_section(m).tables.index_unchecked(0).core_type:
       global_section(m).local_globals.index_unchecked(0).global.core_type;
   CHECK(core.heap.code==(h==0x6eu?-18:-23));CHECK(core.nullable);
  }
 }
 std::printf("PASS Core 3 external declaration parser: %u boundary/policy/ownership checks\n",checks);
}
