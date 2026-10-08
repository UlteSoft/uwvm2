// Core 3 tag declarations: bounded attribute/typeidx parsing, ordering, policy and owned index copies.
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
void section(bytes& b,unsigned id,bytes const& payload){b.push_back(std::byte(id));leb(b,payload.size());b.insert(b.end(),payload.begin(),payload.end());}
auto features(){f::wasm_binfmt_ver1_feature_parameter_storage_t p{};f::wasm_binfmt_ver1_wasm1p1_parameter(p).disable_exceptions=false;return p;}
auto parse(bytes const& b,auto const& p,error expected=error::ok){
 auto page=std::size_t(sysconf(_SC_PAGESIZE));auto pages=((b.size()+page-1)/page)*page;auto memory=static_cast<std::byte*>(mmap(nullptr,pages+page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(memory!=MAP_FAILED);CHECK(mprotect(memory+pages,page,PROT_NONE)==0);auto begin=memory+pages-b.size();std::memcpy(begin,b.data(),b.size());
 base::error_impl err{};f::wasm_binfmt_ver1_module_storage_t m{};try{m=f::binfmt_ver1_handler(begin,begin+b.size(),err,p);}catch(fast_io::error const&){}if(err.err_code!=expected){std::fprintf(stderr,"error=%u expected=%u\n",unsigned(err.err_code),unsigned(expected));}CHECK(err.err_code==expected);CHECK(munmap(memory,pages+page)==0);return m;
}
namespace w1=uwvm2::parser::wasm::standard::wasm1::features;namespace w11=uwvm2::parser::wasm::standard::wasm1p1::features;
using tags=w11::tag_section_storage_t<w1::wasm1,w11::wasm1p1>;
auto const& tag_section(auto const& m){return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<tags>(m.sections);}
bytes module(bytes const& payload,bytes const& types=raw({1,0x60,0,0})){
 auto b=raw({0,0x61,0x73,0x6d,1,0,0,0});section(b,1,types);section(b,13,payload);return b;
}
int main(){auto p=features();
 for(unsigned n:{0,1,2,127,128,1024}){bytes payload;leb(payload,n);for(unsigned i=0;i<n;++i){payload.push_back(std::byte{0});leb(payload,0);}auto data=module(payload);auto m=parse(data,p);CHECK(tag_section(m).present);CHECK(tag_section(m).type_indices.size()==n);tags copy{tag_section(m)};m={};tags moved{std::move(copy)};CHECK(moved.present&&moved.type_indices.size()==n);
 auto disabled=p;f::wasm_binfmt_ver1_wasm1p1_parameter(disabled).disable_exceptions=true;parse(data,disabled,error::wasm1p1_feature_required);
 if(n){auto limited=p;f::wasm_binfmt_ver1_wasm1p1_parameter(limited).parser_limit.max_tag_sec_entries=n-1;parse(data,limited,error::exceed_the_max_parser_limit);}
 auto duplicate=data;section(duplicate,13,raw({0}));parse(duplicate,p,error::duplicate_section);
 }
 // Padded u32 is valid; the zero attribute is a single literal byte.
 for(unsigned width=1;width<=5;++width){auto d=raw({1,0});for(unsigned i=1;i<width;++i)d.push_back(std::byte{0x80});d.push_back(std::byte{0});parse(module(d),p);}
 for(unsigned attr:{1,2,0x7f,0x80,0xff})parse(module(raw({1,attr,0})),p,error::wasm3_invalid_tag_type);
 for(auto payload:{raw({}),raw({0x80}),raw({1}),raw({1,0}),raw({0xff,0xff,0xff,0xff,0x1f})})parse(module(payload),p,error::wasm3_invalid_tag_count);
 for(auto index:{raw({1,0,0x80}),raw({1,0,0x80,0x80,0x80,0x80,0x10}),raw({1,0,0x80,0x80,0x80,0x80,0x80,0})})parse(module(index),p,error::invalid_type_index);
 parse(module(raw({1,0,1})),p,error::illegal_type_index);
 parse(module(raw({1,0,0}),raw({1,0x60,0,1,0x7f})),p,error::wasm3_invalid_tag_type);
 parse(module(raw({1,0,0}),raw({1,0x60,2,0x7f,0x7c,0})),p);
 parse(module(raw({0,0})),p,error::unexpected_section_data);
 auto good=raw({0,0x61,0x73,0x6d,1,0,0,0});section(good,1,raw({1,0x60,0,0}));section(good,5,raw({1,0,0}));section(good,13,raw({1,0,0}));section(good,6,raw({0}));parse(good,p);
 auto bad=raw({0,0x61,0x73,0x6d,1,0,0,0});section(bad,1,raw({1,0x60,0,0}));section(bad,6,raw({0}));section(bad,13,raw({1,0,0}));parse(bad,p,error::invalid_section_canonical_order);
 for(unsigned count:{1,2,128}){
  auto imported=raw({0,0x61,0x73,0x6d,1,0,0,0});section(imported,1,raw({1,0x60,1,0x7f,0}));bytes payload;leb(payload,count);for(unsigned i=0;i<count;++i){auto v=raw({1,'m',1,'t',4,0,0});payload.insert(payload.end(),v.begin(),v.end());}section(imported,2,payload);section(imported,7,raw({1,1,'t',4,0}));parse(imported,p);
  auto disabled=p;f::wasm_binfmt_ver1_wasm1p1_parameter(disabled).disable_exceptions=true;parse(imported,disabled,error::wasm1p1_feature_required);
  uwvm2::parser::wasm::concepts::feature_parameter_t<w1::wasm1> mvp{};base::error_impl e{};try{auto parsed=uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<w1::wasm1>(imported.data(),imported.data()+imported.size(),e,mvp);}catch(fast_io::error const&){}CHECK(e.err_code==error::illegal_importdesc_prefix);
 }
 auto exported=module(raw({2,0,0,0,0}));section(exported,7,raw({2,1,'a',4,0,1,'b',4,1}));parse(exported,p);
 auto invalid=module(raw({1,0,0}));section(invalid,7,raw({1,1,'t',4,1}));parse(invalid,p,error::exported_index_exceeds_maxvul);
 auto missing=raw({0,0x61,0x73,0x6d,1,0,0,0});section(missing,7,raw({1,1,'t',4,0}));parse(missing,p,error::exported_index_exceeds_maxvul);
 for(auto descriptor:{raw({4}),raw({4,0}),raw({4,0,0x80}),raw({4,1,0}),raw({4,0,1})}){
  auto b=raw({0,0x61,0x73,0x6d,1,0,0,0});section(b,1,raw({1,0x60,0,0}));auto imports=raw({1,1,'m',1,'t'});imports.insert(imports.end(),descriptor.begin(),descriptor.end());section(b,2,imports);
  auto expected=descriptor.size()==1||descriptor[1]!=std::byte{0}?error::wasm3_invalid_tag_type:descriptor.size()==2||descriptor.back()==std::byte{0x80}?error::invalid_type_index:error::illegal_type_index;parse(b,p,expected);
 }
 std::printf("PASS Core 3 tag parser: %u boundary/policy/ownership checks\n",checks);
}
