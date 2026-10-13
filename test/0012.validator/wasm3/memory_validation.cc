#include <uwvm2/validation/standard/wasm3/memory_validation.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>
namespace w3=uwvm2::validation::standard::wasm3;
using error=uwvm2::validation::error::code_validation_error_code;
using bytes=std::vector<std::byte>;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
unsigned checks{};
void leb(bytes& b,std::uint64_t v)
{do{auto x=static_cast<unsigned>(v&127);v>>=7;b.push_back(std::byte(x|(v?128:0)));}while(v);}
void check(bytes const& input,bool valid,error expected={},unsigned expected_index=0,std::uint64_t expected_offset=0,
 bool multi=true,bool wide=true,std::size_t count=2,bool expect_lookup=true)
{
 auto page=static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
 auto mapping=static_cast<std::byte*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
 CHECK(mapping!=MAP_FAILED);CHECK(mprotect(mapping+page,page,PROT_NONE)==0);
 auto begin=mapping+page-input.size()-1;*begin=std::byte{0x28};if(!input.empty()){std::memcpy(begin+1,input.data(),input.size());}
 auto cursor=static_cast<std::byte const*>(begin+1);auto end=mapping+page;
 uwvm2::validation::error::code_validation_error_impl err{};bool seen=false,threw=false;
 try
 {
  auto result=w3::read_memory_argument64(cursor,end,begin,multi,wide,count,[&](std::uint32_t index)
  {CHECK(index<count);seen=true;return index==0?w3::storage_address_type::i32:w3::storage_address_type::i64;},2,u8"i32.load",err);
  CHECK(valid);CHECK(cursor==end);CHECK(result.immediate.memory_index==expected_index);
  CHECK(result.immediate.offset==expected_offset);CHECK(result.address_type==(expected_index==0?w3::storage_address_type::i32:w3::storage_address_type::i64));
 }
 catch(fast_io::error const&){threw=true;CHECK(!valid);CHECK(err.err_code==expected);CHECK(err.err_curr==begin);CHECK(cursor==begin+1);}
 CHECK(threw!=valid);CHECK(seen==expect_lookup);CHECK(munmap(mapping,page*2)==0);++checks;
}
int main()
{
 for(auto offset:{0ull,1ull,0xffffffffull,0x100000000ull,0xffffffffffffffffull})for(unsigned index:{0u,1u})
 {
  bytes b{};leb(b,66);leb(b,index);leb(b,offset);
  bool valid=index==1||offset<=0xffffffffull;
  check(b,valid,error::invalid_memarg_offset,index,offset);
  for(std::size_t length=0;length<b.size();++length)
  {
   bytes prefix(b.begin(),b.begin()+length);
   check(prefix,false,length==0?error::invalid_memarg_align:length==1?error::invalid_memory_index:error::invalid_memarg_offset,0,0,true,true,2,false);
  }
 }
 for(unsigned index:{2u,0xffffffffu})
 {bytes b{};leb(b,66);leb(b,index);leb(b,0);check(b,false,error::illegal_memory_index,0,0,true,true,2,false);}
 check({std::byte{2},std::byte{0}},false,error::illegal_memory_index,0,0,true,true,0,false);
 check({std::byte{66},std::byte{1},std::byte{0}},false,error::wasm1p1_feature_required,0,0,false,true,2,false);
 check({std::byte{3},std::byte{0}},false,error::illegal_memarg_alignment);
 check({std::byte{0x80},std::byte{1},std::byte{0}},false,error::invalid_memarg_align,0,0,true,true,2,false);
 bytes padded{std::byte{2}};for(unsigned i=0;i<9;++i){padded.push_back(std::byte{0x80});}padded.push_back(std::byte{});
 check(padded,true);check(padded,false,error::invalid_memarg_offset,0,0,true,false,2,false);
 bytes overflow{std::byte{66},std::byte{1}};for(unsigned i=0;i<9;++i){overflow.push_back(std::byte{0xff});}overflow.push_back(std::byte{2});
 check(overflow,false,error::invalid_memarg_offset,0,0,true,true,2,false);
 std::printf("PASS address-aware memarg validation: %u checks, mixed memory32/64 offsets, guarded input boundaries, transactional errors, index-before-lookup\n",checks);
}
