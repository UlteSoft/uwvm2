// Core 3 explicit element declarations, including empty vectors and declarative segments.
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
auto features(){f::wasm_binfmt_ver1_feature_parameter_storage_t p{};f::wasm_binfmt_ver1_wasm1p1_parameter(p).disable_function_references=false;return p;}
auto parse(bytes const& b,auto const& p,error expected=error::ok){
 auto page=std::size_t(sysconf(_SC_PAGESIZE));CHECK(b.size()<page);auto memory=static_cast<std::byte*>(mmap(nullptr,2*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(memory!=MAP_FAILED);CHECK(mprotect(memory+page,page,PROT_NONE)==0);auto begin=memory+page-b.size();std::memcpy(begin,b.data(),b.size());
 base::error_impl err{};f::wasm_binfmt_ver1_module_storage_t m{};try{m=f::binfmt_ver1_handler(begin,begin+b.size(),err,p);}catch(fast_io::error const&){}CHECK(err.err_code==expected);CHECK(munmap(memory,2*page)==0);return m;
}

namespace w1=uwvm2::parser::wasm::standard::wasm1::features;namespace w11=uwvm2::parser::wasm::standard::wasm1p1::features;
using elements=w1::element_section_storage_t<w1::wasm1,w11::wasm1p1>;
auto const& element_section(auto const& m){return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<elements>(m.sections);}
bytes module(unsigned flags,bytes type,unsigned count,bool tail=true){
 auto out=raw({0,0x61,0x73,0x6d,1,0,0,0});auto section=[&](unsigned id,bytes const& p){out.push_back(std::byte(id));leb(out,p.size());out.insert(out.end(),p.begin(),p.end());};
 unsigned heap=type.size()>1?std::to_integer<unsigned>(type[1]):std::to_integer<unsigned>(type[0]);
 if(flags==6){auto table=raw({1,heap==0x6f?0x6fu:0x70u,0});leb(table,count);section(4,table);}
 auto payload=raw({1,flags});if(flags==6){auto offset=raw({0,0x41,0,11});payload.insert(payload.end(),offset.begin(),offset.end());}payload.insert(payload.end(),type.begin(),type.end());
 if(tail){leb(payload,count);for(unsigned i=0;i<count;++i){auto expr=raw({0xd0,heap,11});payload.insert(payload.end(),expr.begin(),expr.end());}}
 section(9,payload);return out;
}
int main(){auto p=features();
 for(unsigned flags:{5,6,7})for(unsigned heap:{0x70,0x6f})for(unsigned count:{0,1,127,128}){
  auto data=module(flags,raw({0x63,heap}),count);auto m=parse(data,p);auto const& sec=element_section(m);CHECK(sec.requires_function_references);CHECK(sec.elems.size()==1);
  elements copy{sec};m={};elements moved{std::move(copy)};CHECK(moved.requires_function_references);
  auto disabled=p;f::wasm_binfmt_ver1_wasm1p1_parameter(disabled).disable_function_references=true;parse(data,disabled,error::wasm1p1_feature_required);
  disabled=p;f::wasm_binfmt_ver1_wasm1p1_parameter(disabled).disable_reference_types=true;parse(data,disabled,error::wasm1p1_feature_required);
  auto legacy=parse(module(flags,raw({heap}),count),p);CHECK(!element_section(legacy).requires_function_references);
 }
 for(unsigned flags:{5,6,7}){
  for(auto t:{raw({0x63}),raw({0x63,0xf0,0x7f}),raw({0x63,0x80,0x80,0x80,0x80,0x10})})parse(module(flags,t,0,false),p,error::illegal_value_type);
  for(auto t:{raw({0x64,0x70}),raw({0x63,0}),raw({0x63,0x6e})})parse(module(flags,t,0,false),p,error::wasm3_rich_value_not_integrated);
  auto escaped=module(flags,raw({0x63}),0,false);escaped.push_back(std::byte{0x70});parse(escaped,p,error::illegal_value_type);
 }
 std::printf("PASS Core 3 element declaration parser: %u boundary/policy/ownership checks\n",checks);
}
