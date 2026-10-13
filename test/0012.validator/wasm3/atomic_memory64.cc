#include <uwvm2/validation/standard/wasm3/threads.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>
namespace w3=uwvm2::validation::standard::wasm3;
using error=uwvm2::validation::error::code_validation_error_code;
using bytes=std::vector<std::byte>;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
unsigned checks{};
void leb(bytes& b,std::uint64_t v){do{auto x=unsigned(v&127);v>>=7;b.push_back(std::byte(x|(v?128:0)));}while(v);}
bytes instruction(unsigned opcode,unsigned alignment,unsigned index,std::uint64_t offset)
{bytes b{};leb(b,opcode);leb(b,64+alignment);leb(b,index);leb(b,offset);return b;}
void check(bytes const& input,bool valid,error wanted={},bool lookup=true,bool enabled=true,bool multi=true,
 std::size_t count=2,std::uint64_t offset=0)
{
 auto page=std::size_t(sysconf(_SC_PAGESIZE));
 auto mapping=static_cast<std::byte*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
 CHECK(mapping!=MAP_FAILED);CHECK(mprotect(mapping+page,page,PROT_NONE)==0);
 auto begin=mapping+page-input.size()-1;*begin=std::byte{0xfe};if(!input.empty()){std::memcpy(begin+1,input.data(),input.size());}
 auto cursor=static_cast<std::byte const*>(begin+1);auto end=mapping+page;
 bool seen=false,threw=false;uwvm2::validation::error::code_validation_error_impl err{};
 try
 {
  auto decoded=w3::read_atomic_instruction64(cursor,end,begin,enabled,multi,true,count,[&](std::uint32_t index)
  {CHECK(index<count);seen=true;return index==0?w3::storage_address_type::i32:w3::storage_address_type::i64;},err);
  CHECK(valid);CHECK(cursor==end);CHECK(decoded.immediate.memory.offset==offset);
  if(decoded.immediate.descriptor.kind!=w3::atomic_instruction_kind::fence)
  {CHECK(decoded.address_type==(decoded.immediate.memory.memory_index?w3::storage_address_type::i64:w3::storage_address_type::i32));}
 }
 catch(fast_io::error const&)
 {threw=true;CHECK(!valid);CHECK(err.err_code==wanted);CHECK(err.err_curr==begin);CHECK(cursor==begin+1);}
 CHECK(threw!=valid);CHECK(seen==lookup);CHECK(munmap(mapping,page*2)==0);++checks;
}
int main(int argc,char** argv)
{
 if(argc==2)
 {
  bytes input{};auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};
  auto n=std::strlen(argv[1]);if(n%2){return 2;}
  for(std::size_t i=0;i<n;i+=2){input.push_back(std::byte((digit(argv[1][i])<<4)|digit(argv[1][i+1])));}
  std::byte const* cursor=input.data();uwvm2::validation::error::code_validation_error_impl err{};
  try
  {
   static_cast<void>(w3::read_atomic_instruction64(cursor,input.data()+input.size(),input.data(),true,true,true,2,
    [](std::uint32_t index){return index?w3::storage_address_type::i64:w3::storage_address_type::i32;},err));
   return cursor==input.data()+input.size()?0:1;
  }
  catch(fast_io::error const&){return 1;}
 }
 CHECK(argc==1);
 // Literal widths from the threads binary grammar, including wait/notify and
 // all nine seven-opcode load/store/RMW families.
 constexpr unsigned widths[]{2,3,0,1,0,1,2};
 std::vector<unsigned> opcodes{0,1,2};for(unsigned op=0x10;op<=0x4e;++op){opcodes.push_back(op);}
 for(auto op:opcodes)
 {
  auto alignment=op<3?(op==2?3u:2u):widths[(op-0x10)%7];
  for(unsigned index:{0u,1u})for(auto offset:{0ull,0xffffffffull,0x100000000ull,~0ull})
  {
   auto b=instruction(op,alignment,index,offset);bool valid=index||offset<=0xffffffffull;
   check(b,valid,error::invalid_memarg_offset,true,true,true,2,offset);
   // Every truncation ends immediately against an inaccessible page. Decode
   // errors occur before selected-memory lookup and leave the cursor untouched.
   for(std::size_t n=0;n<b.size();++n)
   {
    bytes prefix(b.begin(),b.begin()+n);
    auto wanted=n==0?error::invalid_const_immediate:n==1?error::invalid_memarg_align:n==2?error::invalid_memory_index:error::invalid_memarg_offset;
    check(prefix,false,wanted,false);
   }
  }
  check(instruction(op,alignment,2,0),false,error::illegal_memory_index,false);
  check(instruction(op,alignment+1,1,0),false,error::invalid_memarg_align,false);
  if(alignment){check(instruction(op,alignment-1,1,0),false,error::invalid_memarg_align,false);}
  check(instruction(op,alignment,1,0),false,error::wasm1p1_feature_required,false,false);
  check(instruction(op,alignment,1,0),false,error::wasm1p1_feature_required,false,true,false);
  // Existing u32 scanner keeps its bytecode and accepted offset range.
  auto b=instruction(op,alignment,1,0xffffffffull);auto c=b.data();std::byte const* p=c;
  auto old=w3::scan_atomic_instruction(p,c+b.size(),true);CHECK(old.error==w3::atomic_immediate_error::ok&&old.memory.offset==0xffffffffu);++checks;
  b=instruction(op,alignment,1,0x100000000ull);p=b.data();old=w3::scan_atomic_instruction(p,b.data()+b.size(),true);
  CHECK(old.error==w3::atomic_immediate_error::memory_argument&&old.memory.error==w3::memory_immediate_error::offset&&p==b.data());++checks;
 }
 check({std::byte{3},std::byte{}},true,{},false,true,true,0);
 check({std::byte{0x83},std::byte{},std::byte{}},true,{},false,true,true,0);
 check({std::byte{3},std::byte{1}},false,error::invalid_const_immediate,false);
 check({std::byte{3},std::byte{0x80},std::byte{}},false,error::invalid_const_immediate,false);
 for(unsigned opcode:{4u,15u,79u,0xffffffffu}){bytes b{};leb(b,opcode);check(b,false,error::invalid_const_immediate,false);}
 auto overflow=instruction(0x10,2,1,~0ull);overflow.back()=std::byte{2};check(overflow,false,error::invalid_memarg_offset,false);
 std::printf("PASS memory64 atomic decode: %u checks, all 67 thread opcodes, exact alignment, u64 offsets, guarded truncations, selected address type, memory32 compatibility\n",checks);
}
