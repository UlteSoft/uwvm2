#include <uwvm2/validation/standard/wasm3/heap_immediate.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>
namespace v3=uwvm2::validation::standard::wasm3;
using e=v3::function_heap_immediate_error;using bytes=std::vector<std::byte>;unsigned checks{};
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x);std::abort();}}while(false)
bytes raw(std::initializer_list<unsigned> v){bytes b;for(auto x:v)b.push_back(std::byte(x));return b;}
bytes encode(std::uint64_t v){bytes b;do{auto x=v&127;v>>=7;if(v||(x&64)){b.push_back(std::byte(x|128));}else{b.push_back(std::byte(x));break;}}while(true);return b;}
auto check(bytes const& data,std::size_t count,bool enabled,e error,bool exception_refs=false,bool gc_enabled=false){
 auto page=std::size_t(sysconf(_SC_PAGESIZE));CHECK(data.size()<page);auto p=static_cast<std::byte*>(mmap(nullptr,2*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(p!=MAP_FAILED);CHECK(mprotect(p+page,page,PROT_NONE)==0);auto end=p+page;auto begin=end-data.size();if(!data.empty())std::memcpy(begin,data.data(),data.size());std::byte const* cursor=begin;
 auto r=v3::scan_function_ref_null_heap(cursor,end,enabled,count,exception_refs,gc_enabled);if(r.error!=error){std::fprintf(stderr,"heap mismatch: size=%zu first=%u enabled=%u exn=%u expected=%u actual=%u\n",data.size(),data.empty()?0u:std::to_integer<unsigned>(data[0]),unsigned(enabled),unsigned(exception_refs),unsigned(error),unsigned(r.error));}CHECK(r.error==error);CHECK(r.error_offset<=data.size());CHECK(cursor==(error==e::ok?end:begin));CHECK(munmap(p,2*page)==0);return r;
}
int main(){
 for(auto index:{0ull,1ull,11ull,63ull,64ull,127ull,128ull,16383ull,16384ull,0xfffffffeull,0xffffffffull}){
  auto b=encode(index);auto count=index+1;auto r=check(b,count,true,e::ok);CHECK(r.heap.code==static_cast<std::int64_t>(index)&&r.carrier==0x70);check(b,index,true,e::unknown_type);check(b,count,false,e::function_references_disabled);
  for(std::size_t n=0;n<b.size();++n)check(bytes(b.begin(),b.begin()+n),count,true,e::binary);
 }
 for(unsigned last=0;last<256;++last)check(raw({0xff,0xff,0xff,0xff,last}),0x100000000ull,true,last<=0x0f?e::ok:e::binary);
 for(auto b:{raw({0}),raw({0x80,0}),raw({0x80,0x80,0}),raw({0x80,0x80,0x80,0}),raw({0x80,0x80,0x80,0x80,0})})check(b,1,true,e::ok);
 for(auto h:{0x70u,0x6fu})for(bool enabled:{false,true}){auto r=check(raw({h}),0,enabled,e::ok);CHECK(r.carrier==h);check(raw({h|128,0x7f}),0,enabled,e::binary);}
 for(unsigned h=0x69;h<=0x74;++h)if(h!=0x70&&h!=0x6f)
  check(raw({h}),0,true,(h==0x72||h==0x73)?e::gc_disabled:e::unsupported_heap);
 for(unsigned h:{0x72u,0x73u}){
  check(raw({h}),0,false,e::gc_disabled);
  auto r=check(raw({h}),0,false,e::ok,false,true);
  CHECK(r.carrier==(h==0x73u?0x70u:0x6fu));
 }
 for(unsigned h:{0x69u,0x74u})for(bool function_refs:{false,true}){
  auto r=check(raw({h}),0,function_refs,e::ok,true);CHECK(r.carrier==0x69u);
  check(raw({h|128u,0x7fu}),0,function_refs,e::binary,true);
 }
 // The opt-in never turns unrelated bottom/GC heaps into exnref.
 for(unsigned h:{0x6au,0x6bu})check(raw({h}),0,true,e::unsupported_heap,true);
 check(raw({0x73u}),0,true,e::gc_disabled,true);
 std::byte const* empty{};auto r=v3::scan_function_ref_null_heap(empty,empty,true,0);CHECK(r.error==e::binary&&empty==nullptr);
 std::printf("PASS ref.null signed-33 heap decoder: %u boundary/policy/index checks\n",checks);
}
