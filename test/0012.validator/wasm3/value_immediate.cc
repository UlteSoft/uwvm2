#include <uwvm2/validation/standard/wasm3/value_immediate.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>
namespace v3=uwvm2::validation::standard::wasm3;
using e=v3::value_carrier_error;using bytes=std::vector<std::byte>;unsigned checks{};
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x);std::abort();}}while(false)
bytes raw(std::initializer_list<unsigned> v){bytes b;for(auto x:v)b.push_back(std::byte(x));return b;}
auto check(bytes const& data,bool enabled,e error,std::size_t known_function_types=0,bool gc_enabled=false){
 auto page=std::size_t(sysconf(_SC_PAGESIZE));CHECK(data.size()<page);auto p=static_cast<std::byte*>(mmap(nullptr,2*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(p!=MAP_FAILED);CHECK(mprotect(p+page,page,PROT_NONE)==0);auto end=p+page;auto begin=end-data.size();if(!data.empty())std::memcpy(begin,data.data(),data.size());std::byte const* cursor=begin;
 auto r=v3::scan_core3_value_carrier(cursor,end,enabled,known_function_types,gc_enabled);CHECK(r.error==error);CHECK(r.error_offset<=data.size());CHECK(cursor==(error==e::ok?end:begin));CHECK(munmap(p,2*page)==0);return r;
}
int main(){
 for(unsigned b=0;b<256;++b)for(bool enabled:{false,true}){
  auto expected=(b>=0x7b&&b<=0x7f)||b==0x70||b==0x6f?e::ok:
    (b==0x72||b==0x73?e::gc_disabled:(b>=0x69&&b<=0x74?e::rich_type_not_integrated:e::binary));
  auto r=check(raw({b}),enabled,expected);if(expected==e::ok)CHECK(r.carrier==(b==0x73?0x70:b));
 }
 for(unsigned h=0;h<256;++h)for(unsigned prefix:{0x63,0x64})for(bool enabled:{false,true}){
  auto expected=h<0x40?(enabled?e::unknown_function_type:e::function_references_disabled):
    (h==0x70||h==0x6f?(prefix==0x64&&!enabled?e::function_references_disabled:e::ok):
     (h==0x72||h==0x73?e::gc_disabled:(h>=0x69&&h<=0x74?e::rich_type_not_integrated:e::binary)));
  auto r=check(raw({prefix,h}),enabled,expected);if(expected==e::ok)CHECK(r.carrier==h&&r.type.nullable==(prefix==0x63));
 }
 for(auto b:{raw({0x63,0xf0,0x7f}),raw({0x63,0xef,0x7f}),raw({0x63,0x80}),raw({0x63,0x80,0x80,0x80,0x80,0x10})})check(b,true,e::binary);
 for(auto b:{raw({0x63,0x80,1}),raw({0x63,0x80,0x80,0x80,0x80,0}),raw({0x64,0xff,0xff,0xff,0xff,0x0f})})check(b,true,e::unknown_function_type);
 {auto r=check(raw({0x63,0x00}),true,e::ok,1);CHECK(r.carrier==0x70&&r.type.heap.code==0&&r.type.nullable);}
 {auto r=check(raw({0x64,0x00}),true,e::ok,1);CHECK(r.carrier==0x70&&r.type.heap.code==0&&!r.type.nullable);}
 check(raw({0x63,0x00}),false,e::function_references_disabled,1);
 for(auto h:{0x72u,0x73u})for(auto prefix:{0x63u,0x64u}){
  check(raw({prefix,h}),false,e::gc_disabled);
  auto r=check(raw({prefix,h}),false,e::ok,0,true);
  CHECK(r.carrier==(h==0x73u?0x70u:0x6fu));
 }
 check(raw({0x63,0x70}),false,e::ok);
 check(raw({0x63,0x6f}),false,e::ok);
 check(raw({0x64,0x70}),false,e::function_references_disabled);
 check(raw({0x64,0x6f}),false,e::function_references_disabled);
 check(raw({0x63,0x01}),true,e::unknown_function_type,1);
 if constexpr(sizeof(std::size_t)>4){auto r=check(raw({0x63,0xff,0xff,0xff,0xff,0x0f}),true,e::ok,0x1'0000'0000ull);CHECK(r.type.heap.code==0xffff'ffffll);}
 std::byte const* empty{};auto r=v3::scan_core3_value_carrier(empty,empty,true);CHECK(r.error==e::binary&&empty==nullptr);
 std::printf("PASS Core 3 value-immediate decoder: %u boundary/policy/projection checks\n",checks);
}
