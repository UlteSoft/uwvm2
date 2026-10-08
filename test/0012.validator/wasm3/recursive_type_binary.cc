// Core 3 type grammar; every payload ends immediately before an inaccessible page.
#include <uwvm2/validation/standard/wasm3/recursive_type_binary.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>
namespace v3=uwvm2::validation::standard::wasm3;
namespace t3=uwvm2::parser::wasm::standard::wasm3::type;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x); std::abort(); } } while(false)
using bytes=std::vector<std::byte>;
bytes raw(std::initializer_list<unsigned> data){bytes out;for(auto x:data)out.push_back(std::byte(x));return out;}
unsigned checks{};
t3::recursive_type_section decode(bytes const& data,bool valid)
{
 auto page=std::size_t(sysconf(_SC_PAGESIZE));CHECK(data.size()<page);
 auto base=static_cast<std::byte*>(mmap(nullptr,2*page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(base!=MAP_FAILED);
 CHECK(mprotect(base+page,page,PROT_NONE)==0);auto end=base+page;auto begin=end-data.size();
 if(!data.empty())std::memcpy(begin,data.data(),data.size());
 std::byte const* cursor=begin;t3::recursive_type_section output;output.type_count=12345;
 auto result=v3::scan_core3_type_section(cursor,end,output);
 CHECK((result.error==v3::recursive_type_binary_error::ok)==valid);
 if(valid)CHECK(cursor==end);else{CHECK(cursor==begin&&output.type_count==12345&&output.groups.empty());CHECK(result.error_offset<=data.size());}
 CHECK(munmap(base,2*page)==0);++checks;return output;
}
void positive(bytes const& data)
{
 decode(data,true);
 for(std::size_t n=0;n<data.size();++n)decode(bytes(data.begin(),data.begin()+n),false);
 auto trailing=data;trailing.push_back(std::byte{});decode(trailing,false);
}
int main()
{
 // Exercise the signed-33 carrier's full final-byte range independently of heap-type policy.
 for(unsigned last=0;last!=256;++last)
 {
  auto data=raw({0x80,0x80,0x80,0x80,last,0});
  v3::recursive_binary_details::reader input{data};std::int_least64_t value=42;
  bool valid=last<=15||(last>=112&&last<=127);
  CHECK(input.s33(value)==valid);CHECK(input.position==5);
  if(valid)
  {
   auto expected=(last<=15?std::int_least64_t(last):std::int_least64_t(last)-128)*0x10000000ll;
   CHECK(value==expected);
  }
  else CHECK(value==42&&input.status.error==v3::recursive_type_binary_error::invalid_integer);
  ++checks;
 }
 std::byte const* empty{};t3::recursive_type_section unchanged;unchanged.type_count=12345;
 CHECK(v3::scan_core3_type_section(empty,empty,unchanged).error==v3::recursive_type_binary_error::truncated);
 CHECK(empty==nullptr&&unchanged.type_count==12345);
 positive(raw({0}));positive(raw({1,0x4e,0}));positive(raw({1,0x60,0,0}));positive(raw({1,0x5f,0}));
 positive(raw({1,0x4f,0,0x60,0,0}));positive(raw({1,0x50,0,0x60,0,0}));
 positive(raw({1,0x4e,2,0x5f,1,0x63,1,0,0x5e,0x64,0,1}));
 // Empty groups do not occupy the module type index space.
 auto groups=decode(raw({3,0x4e,0,0x4e,2,0x5f,0,0x5e,0x78,0,0x60,0,0}),true);
 CHECK(groups.type_count==3&&groups.groups.size()==3);
 CHECK(groups.groups.index_unchecked(1).first_type_index==0&&groups.groups.index_unchecked(2).first_type_index==2);
 for(unsigned prefix=0;prefix<256;++prefix)
 {
  bool val=(prefix>=0x69&&prefix<=0x74)||(prefix>=0x7b&&prefix<=0x7f);
  if(prefix==0x63||prefix==0x64)continue;
  decode(raw({1,0x60,1,prefix,0}),val);
  decode(raw({1,0x5e,prefix,0}),val||prefix==0x77||prefix==0x78);
 }
 for(unsigned heap=0x69;heap<=0x74;++heap)
 {
  for(auto ref:{0x63u,0x64u})
  {
   positive(raw({1,0x60,1,ref,heap,0}));
   // Single-byte abstract heaps cannot be encoded as padded negative s33 values.
   decode(raw({1,0x60,1,ref,heap|0x80,0x7f,0}),false);
  }
 }
 for(auto field: {0x78u,0x77u,0x7fu,0x70u})for(unsigned mut=0;mut<256;++mut)
  decode(raw({1,0x5e,field,mut}),mut<2);
 for(auto ref:{0x63u,0x64u})
 {
  positive(raw({1,0x60,1,ref,0,0}));positive(raw({1,0x60,1,ref,0x80,0x80,0x80,0x80,0,0}));
  // All 32 unsigned index bits survive signed-33 decoding, even on a 32-bit host.
  auto max=decode(raw({1,0x60,1,ref,0xff,0xff,0xff,0xff,0x0f,0}),true);
  CHECK(max.groups.index_unchecked(0).types.index_unchecked(0).parameters.index_unchecked(0).heap.code==0xffffffffll);
  for(unsigned final=0;final<256;++final)
   decode(raw({1,0x60,1,ref,0xff,0xff,0xff,0xff,final,0}),final<=0x0f);
 }
 // Syntax accepts lists of supertypes; the semantic validator rejects multiple parents and invalid indices.
 positive(raw({1,0x50,2,0,1,0x60,0,0}));
 for(auto data:{raw({0xff,0xff,0xff,0xff,0x10}),raw({1,0x4e,0xff,0xff,0xff,0xff,0x0f}),
                raw({1,0x5f,0xff,0xff,0xff,0xff,0x0f}),raw({1,0x60,0xff,0xff,0xff,0xff,0x0f,0}),
                raw({1,0x4e,1,0x4e,0}),raw({1,0x5f,1,0x78}),raw({1,0x5e,0x78}),raw({1,0x50,0})})decode(data,false);
 std::printf("PASS recursive type binary: %u guarded boundary/grammar checks\n",checks);
}
