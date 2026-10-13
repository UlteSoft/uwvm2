// Core 3 explicit nullable heap encodings through the actual module parser and owned type storage.
#include <uwvm2/uwvm/wasm/feature/impl.h>
#include <uwvm2/validation/standard/wasm3/function_signature.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>
namespace f=uwvm2::uwvm::wasm::feature;
namespace v3=uwvm2::validation::standard::wasm3;
namespace t3=uwvm2::parser::wasm::standard::wasm3::type;
namespace base=uwvm2::parser::wasm::base;
using carrier=uwvm2::parser::wasm::standard::wasm1p1::type::value_type;
using bytes=std::vector<std::byte>;
unsigned checks{};
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
bytes raw(std::initializer_list<unsigned> v){bytes r;for(auto x:v)r.push_back(std::byte(x));return r;}
void leb(bytes& b,std::size_t n){do{auto x=n&127;n>>=7;b.push_back(std::byte(x|(n?128:0)));}while(n);}
bytes module(bytes const& signature){auto b=raw({0,0x61,0x73,0x6d,1,0,0,0,1});leb(b,signature.size()+2);b.push_back(std::byte{1});b.push_back(std::byte{0x60});b.insert(b.end(),signature.begin(),signature.end());return b;}
f::wasm_binfmt_ver1_feature_parameter_storage_t features(){f::wasm_binfmt_ver1_feature_parameter_storage_t p{};f::wasm_binfmt_ver1_wasm1p1_parameter(p).disable_function_references=false;return p;}
using section=uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<uwvm2::parser::wasm::standard::wasm1::features::wasm1,uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1>;
auto const& types(auto const& m){return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<section>(m.sections);}
auto parse(bytes const& b,auto const& p,base::wasm_parse_error_code expected=base::wasm_parse_error_code::ok){base::error_impl err{};f::wasm_binfmt_ver1_module_storage_t m{};try{m=f::binfmt_ver1_handler(b.data(),b.data()+b.size(),err,p);}catch(fast_io::error const&){}CHECK(err.err_code==expected);return m;}
void guarded(bytes const& b,bool good){
 auto page=std::size_t(sysconf(_SC_PAGESIZE));CHECK(b.size()<page);
 auto p=static_cast<std::byte*>(mmap(nullptr,2*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(p!=MAP_FAILED);CHECK(mprotect(p+page,page,PROT_NONE)==0);
 auto begin=p+page-b.size();if(!b.empty())std::memcpy(begin,b.data(),b.size());std::byte const* cursor=begin;t3::owned_function_signature<carrier> out;out.type_index=123;
 auto r=v3::scan_core3_function_signature(cursor,p+page,{},out);CHECK((r.error==v3::function_signature_error::ok)==good);
 if(good){CHECK(cursor==p+page);CHECK(out.carriers.size()==out.parameters.size()+out.results.size()+1);}else{CHECK(cursor==begin&&out.type_index==123&&out.carriers.empty());CHECK(r.error_offset<=b.size());}
 CHECK(munmap(p,2*page)==0);
}
int main(){
 // Including the Core 3-enabled parser must not make its extension participate in a wasm1-only feature pack.
 {using w1=uwvm2::parser::wasm::standard::wasm1::features::wasm1;
  uwvm2::parser::wasm::concepts::feature_parameter_t<w1> options{};base::error_impl error{};auto data=module(raw({1,0x7f,0}));
  auto legacy=uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<w1>(data.data(),data.data()+data.size(),error,options);
  CHECK(error.err_code==base::wasm_parse_error_code::ok);
  auto const& section=uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<w1>>(legacy.sections);
  CHECK(section.owned_signatures.empty());CHECK(section.types.size()==1);}
 auto p=features();
 auto feature_scan=[&](unsigned prefix,unsigned heap,bool function_refs,bool gc,v3::function_signature_error expected){
  auto sig=raw({1,prefix,heap,0});std::byte const* cursor=sig.data();
  t3::owned_function_signature<carrier> out{};
  v3::function_signature_policy policy{};policy.function_references=function_refs;policy.gc=gc;
  auto result=v3::scan_core3_function_signature(cursor,sig.data()+sig.size(),policy,out);
  CHECK(result.error==expected);CHECK(cursor==(expected==v3::function_signature_error::ok?sig.data()+sig.size():sig.data()));
 };
 for(auto heap:{0x72u,0x73u})for(auto prefix:{0x63u,0x64u}){
  feature_scan(prefix,heap,false,true,v3::function_signature_error::ok);
  feature_scan(prefix,heap,true,false,v3::function_signature_error::gc_disabled);
 }
 for(auto heap:{0x6fu,0x70u}){
  feature_scan(0x63u,heap,false,false,v3::function_signature_error::ok);
  feature_scan(0x64u,heap,false,false,v3::function_signature_error::function_references_disabled);
 }
 for(auto h:{0x70u,0x6fu}){
  auto sig=raw({1,0x63,h,1,0x63,h});guarded(sig,true);
  for(std::size_t n=0;n<sig.size();++n)guarded(bytes(sig.begin(),sig.begin()+n),false);
  auto data=module(sig);auto m=parse(data,p);auto const& s=types(m);CHECK(s.owned_signatures.size()==1);CHECK(!s.requires_function_references);CHECK(!s.owned_signatures.index_unchecked(0).requires_function_references);CHECK(s.types.size()==1);
  auto const& ft=s.types.index_unchecked(0);CHECK(static_cast<unsigned>(*ft.parameter.begin)==h);CHECK(ft.parameter.end-ft.parameter.begin==1);CHECK(static_cast<unsigned>(*ft.result.begin)==h);
  CHECK(s.owned_signatures.index_unchecked(0).parameters.index_unchecked(0).nullable);
  // Destroy the original owner and source bytes. Copies must own disjoint carrier buffers, not source views.
  section copy{s};CHECK(copy.types.index_unchecked(0).parameter.begin!=ft.parameter.begin);
  section assigned;assigned=s;section moved{std::move(copy)};section move_assigned;move_assigned=std::move(assigned);
  m={};data.clear();data.shrink_to_fit();CHECK(!moved.requires_function_references&&!move_assigned.requires_function_references);CHECK(static_cast<unsigned>(*moved.types.index_unchecked(0).parameter.begin)==h);CHECK(static_cast<unsigned>(*move_assigned.types.index_unchecked(0).result.begin)==h);
  auto disabled=p;f::wasm_binfmt_ver1_wasm1p1_parameter(disabled).disable_function_references=true;parse(module(sig),disabled,base::wasm_parse_error_code::illegal_value_type);
  disabled=p;f::wasm_binfmt_ver1_wasm1p1_parameter(disabled).disable_reference_types=true;parse(module(sig),disabled,base::wasm_parse_error_code::wasm1p1_feature_required);
  for(auto bad: {raw({1,0x63,h|128u,0x7f,0}),raw({1,0x63,0,0}),raw({1,0x63,0xff,0xff,0xff,0xff,0x0f,0})})guarded(bad,false);
 }
 for(auto sig:{raw({0,0}),raw({3,0x7f,0x63,0x70,0x7e,2,0x63,0x6f,0x7b})}){guarded(sig,true);auto m=parse(module(sig),p);CHECK(types(m).owned_signatures.size()==1);}
 auto disabled=p;f::wasm_binfmt_ver1_wasm1p1_parameter(disabled).disable_simd=true;parse(module(raw({0,1,0x7b})),disabled,base::wasm_parse_error_code::wasm1p1_feature_required);
 disabled=p;f::wasm_binfmt_ver1_wasm1p1_parameter(disabled).disable_multi_value=true;parse(module(raw({0,2,0x63,0x70,0x63,0x6f})),disabled,base::wasm_parse_error_code::wasm1p1_feature_required);
 disabled=p;f::wasm_binfmt_ver1_wasm1p1_parameter(disabled).controllable_allow_multi_result_vector=true;parse(module(raw({0,2,0x63,0x70,0x63,0x6f})),disabled,base::wasm_parse_error_code::wasm1p1_feature_required);
 {auto m=parse(module(raw({1,0x64,0x70,0})),p);auto const& s=types(m);
  CHECK(s.owned_signatures.size()==1);CHECK(!s.owned_signatures.index_unchecked(0).parameters.index_unchecked(0).nullable);
  CHECK(static_cast<unsigned>(*s.types.index_unchecked(0).parameter.begin)==0x70);}
 {auto m=parse(module(raw({1,0x63,0x00,1,0x64,0x00})),p);auto const& s=types(m);
  CHECK(s.owned_signatures.size()==1);CHECK(s.owned_signatures.index_unchecked(0).parameters.index_unchecked(0).heap.code==0);
  CHECK(s.owned_signatures.index_unchecked(0).parameters.index_unchecked(0).nullable);
  CHECK(s.owned_signatures.index_unchecked(0).results.index_unchecked(0).heap.code==0);
  CHECK(!s.owned_signatures.index_unchecked(0).results.index_unchecked(0).nullable);
  CHECK(static_cast<unsigned>(*s.types.index_unchecked(0).parameter.begin)==0x70);}
 parse(module(raw({1,0x64,0x01,0})),p,base::wasm_parse_error_code::illegal_type_index);
 // Exception references are independent of function references. Keep their
 // precise nullable heap in owned metadata and project only the 0x69 ABI byte.
 {auto exn_features=p;auto& flags=f::wasm_binfmt_ver1_wasm1p1_parameter(exn_features);
  flags.disable_exceptions=false;flags.disable_function_references=true;
  for(auto sig:{raw({1,0x63,0x69,1,0x64,0x69}),raw({1,0x64,0x74,1,0x69})}){
   guarded(sig,true);auto m=parse(module(sig),exn_features);auto const& s=types(m);
   CHECK(s.owned_signatures.size()==1);CHECK(!s.requires_function_references);
   CHECK(static_cast<unsigned>(*s.types.index_unchecked(0).parameter.begin)==0x69);
   CHECK(static_cast<unsigned>(*s.types.index_unchecked(0).result.begin)==0x69);}
  flags.disable_exceptions=true;
  parse(module(raw({1,0x63,0x69,0})),exn_features,base::wasm_parse_error_code::illegal_value_type);
  flags.disable_exceptions=false;flags.disable_reference_types=true;
  parse(module(raw({1,0x63,0x69,0})),exn_features,base::wasm_parse_error_code::wasm1p1_feature_required);
 }
 // A large type vector repeatedly relocates owners. Every signature view must keep its own allocation.
 auto b=raw({0,0x61,0x73,0x6d,1,0,0,0,1});bytes payload;leb(payload,1024);for(unsigned i=0;i<1024;++i){auto v=raw({0x60,1,0x63,(i&1)?0x6fu:0x70u,0});payload.insert(payload.end(),v.begin(),v.end());}leb(b,payload.size());b.insert(b.end(),payload.begin(),payload.end());auto m=parse(b,p);section copy{types(m)};m={};
 for(unsigned i=0;i<1024;++i)CHECK(static_cast<unsigned>(*copy.types.index_unchecked(i).parameter.begin)==((i&1)?0x6fu:0x70u));
 std::printf("PASS Core 3 owned function signatures: %u syntax/boundary/policy/ownership checks\n",checks);
}
