// Core 3 explicit local valtypes through the actual parser, including body boundaries and retained feature policy.
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
bytes module(bytes const& declarations){auto b=raw({0,0x61,0x73,0x6d,1,0,0,0,1,4,1,0x60,0,0,3,2,1,0,10});bytes code=raw({1});leb(code,declarations.size());code.insert(code.end(),declarations.begin(),declarations.end());leb(b,code.size());b.insert(b.end(),code.begin(),code.end());return b;}
auto features(){f::wasm_binfmt_ver1_feature_parameter_storage_t p{};f::wasm_binfmt_ver1_wasm1p1_parameter(p).disable_function_references=false;return p;}
using section=uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<uwvm2::parser::wasm::standard::wasm1::features::wasm1,uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1>;
auto const& codes(auto const& m){return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<section>(m.sections);}
auto parse(bytes const& b,auto const& p,error expected=error::ok){
 auto page=std::size_t(sysconf(_SC_PAGESIZE));CHECK(b.size()<page);auto memory=static_cast<std::byte*>(mmap(nullptr,2*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(memory!=MAP_FAILED);CHECK(mprotect(memory+page,page,PROT_NONE)==0);auto begin=memory+page-b.size();std::memcpy(begin,b.data(),b.size());
 base::error_impl err{};f::wasm_binfmt_ver1_module_storage_t m{};try{m=f::binfmt_ver1_handler(begin,begin+b.size(),err,p);}catch(fast_io::error const&){}CHECK(err.err_code==expected);CHECK(munmap(memory,2*page)==0);return m;
}
int main(){auto p=features();
 for(unsigned h:{0x70,0x6f})for(unsigned count:{0,1,127,128}){
  bytes decl=raw({1});leb(decl,count);auto tail=raw({0x63,h,11});decl.insert(decl.end(),tail.begin(),tail.end());auto m=parse(module(decl),p);auto const& s=codes(m);CHECK(!s.locals_require_function_references);CHECK(s.codes.size()==1);auto const& c=s.codes.index_unchecked(0);CHECK(c.locals.size()==1);CHECK(c.locals.index_unchecked(0).count==count);CHECK(static_cast<unsigned>(c.locals.index_unchecked(0).type)==h);CHECK(c.locals.index_unchecked(0).has_core_type);CHECK(c.locals.index_unchecked(0).core_type.nullable);CHECK(c.all_local_count==count);
  section copy{s};m={};section moved{std::move(copy)};CHECK(!moved.locals_require_function_references);CHECK(static_cast<unsigned>(moved.codes.index_unchecked(0).locals.index_unchecked(0).type)==h);
  auto disabled=p;f::wasm_binfmt_ver1_wasm1p1_parameter(disabled).disable_function_references=true;parse(module(decl),disabled);
  disabled=p;f::wasm_binfmt_ver1_wasm1p1_parameter(disabled).disable_reference_types=true;parse(module(decl),disabled,error::wasm1p1_feature_required);
 }
 auto m=parse(module(raw({3,1,0x63,0x70,2,0x63,0x6f,3,0x7f,11})),p);CHECK(codes(m).codes.index_unchecked(0).locals.size()==3);CHECK(codes(m).codes.index_unchecked(0).all_local_count==6);
 auto short_form=parse(module(raw({1,1,0x70,11})),p);CHECK(!codes(short_form).locals_require_function_references);CHECK(!codes(short_form).codes.index_unchecked(0).locals.index_unchecked(0).has_core_type);
 for(auto declaration:{raw({1,0,0x64,0x70,11}),raw({1,1,0x63,0x00,11}),raw({1,1,0x64,0x00,11})}){
  auto typed=parse(module(declaration),p);auto const& run=codes(typed).codes.index_unchecked(0).locals.index_unchecked(0);
  CHECK(run.has_core_type);CHECK(static_cast<unsigned>(run.type)==0x70);
  CHECK(run.core_type.nullable==(declaration[2]==std::byte{0x63}||declaration[2]==std::byte{0x73}));
  CHECK(run.core_type.heap.code==(declaration[2]==std::byte{0x73}?-13:(declaration[3]==std::byte{0x70}?-16:0)));
 }
 {auto gc_features=p;auto& flags=f::wasm_binfmt_ver1_wasm1p1_parameter(gc_features);
  flags.disable_gc=false;flags.disable_function_references=true;
  for(auto h:{0x72u,0x73u}){
   auto declaration=raw({1,1,h,11});auto m=parse(module(declaration),gc_features);
   auto const& s=codes(m);CHECK(!s.locals_require_function_references);
   auto const& local=s.codes.index_unchecked(0).locals.index_unchecked(0);
   CHECK(local.has_core_type&&local.core_type.nullable);
   CHECK(static_cast<unsigned>(local.type)==(h==0x72u?0x6fu:0x70u));
   parse(module(declaration),p,error::wasm1p1_feature_required);
  }
 }
 for(auto t:{raw({1,0,0x63}),raw({1,1,0x63,0xf0,0x7f,11}),raw({1,1,0x63,0x80,0x80,0x80,0x80,0x10,11})})parse(module(t),p,error::illegal_value_type);
 parse(module(raw({1,1,0x63,0x01,11})),p,error::illegal_type_index);
 parse(module(raw({1,0,0x64,0x6f,11})),p);
 for(auto t:{raw({1,0,0x63,0x6e,11}),raw({1,0,0x63,0x69,11})})parse(module(t),p,error::wasm1p1_feature_required);
 {auto rich=p;auto& flags=f::wasm_binfmt_ver1_wasm1p1_parameter(rich);
  flags.disable_gc=false;flags.disable_exceptions=false;
  for(auto h:{0x6eu,0x69u}){
   auto m=parse(module(raw({1,1,0x63,h,11})),rich);
   auto const& local=codes(m).codes.index_unchecked(0).locals.index_unchecked(0);
   CHECK(local.has_core_type);CHECK(local.core_type.nullable);
   CHECK(local.core_type.heap.code==(h==0x6eu?-18:-23));
  }
 }
 // The next byte in the code section is deliberately 0x70. It is outside this function body and cannot supply its heap.
 auto escaped=module(raw({1,1,0x63}));escaped.back()=std::byte{0x63};escaped.push_back(std::byte{0x70});escaped[19]=std::byte(std::to_integer<unsigned>(escaped[19])+1u);parse(escaped,p,error::illegal_value_type);
 {using w1=uwvm2::parser::wasm::standard::wasm1::features::wasm1;uwvm2::parser::wasm::concepts::feature_parameter_t<w1> options{};base::error_impl err{};auto data=module(raw({1,2,0x7f,11}));auto legacy=uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<w1>(data.data(),data.data()+data.size(),err,options);CHECK(err.err_code==error::ok);auto const& section=uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<w1>>(legacy.sections);CHECK(!section.locals_require_function_references);CHECK(section.codes.index_unchecked(0).all_local_count==2);}
 std::printf("PASS Core 3 local-value parser: %u boundary/policy/ownership checks\n",checks);
}
