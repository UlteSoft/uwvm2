// Core 3 binary GC instructions: every buffer ends at an inaccessible page.
#include <uwvm2/validation/standard/wasm3/gc_immediate.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>
namespace v3=uwvm2::validation::standard::wasm3;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x);std::abort();}}while(false)
using bytes=std::vector<std::byte>;
bytes raw(std::initializer_list<unsigned> data){bytes out;for(auto x:data)out.push_back(std::byte(x));return out;}
unsigned checks{};
v3::gc_instruction_immediate scan(bytes const& data,std::size_t consumed)
{
 auto page=std::size_t(sysconf(_SC_PAGESIZE));CHECK(data.size()<page);
 auto base=static_cast<std::byte*>(mmap(nullptr,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));CHECK(base!=MAP_FAILED);
 CHECK(mprotect(base+page,page,PROT_NONE)==0);auto end=base+page;auto begin=end-data.size();if(!data.empty())std::memcpy(begin,data.data(),data.size());
 std::byte const* cursor=begin;auto result=v3::scan_gc_instruction(cursor,end);
 CHECK((result.error==v3::gc_immediate_error::ok)==(consumed!=0));
 CHECK(cursor==begin+consumed);CHECK(result.error_offset<=data.size());
 CHECK(munmap(base,page*2)==0);++checks;return result;
}
v3::gc_instruction_immediate positive(bytes const& data)
{
 auto result=scan(data,data.size());
 for(std::size_t n=0;n<data.size();++n)scan(bytes(data.begin(),data.begin()+n),0);
 auto followed=data;followed.push_back(std::byte{0xde});followed.push_back(std::byte{0xad});scan(followed,data.size());
 return result;
}
int main()
{
 std::byte const* empty{};CHECK(v3::scan_gc_instruction(empty,empty).error==v3::gc_immediate_error::malformed&&empty==nullptr);
 std::vector<bytes> cases={raw({0,0}),raw({1,0}),raw({2,0,0}),raw({3,0,0}),raw({4,0,0}),raw({5,0,0}),
 raw({6,0}),raw({7,0}),raw({8,0,0}),raw({9,0,0}),raw({10,0,0}),raw({11,0}),raw({12,0}),raw({13,0}),raw({14,0}),raw({15}),
 raw({16,0}),raw({17,0,0}),raw({18,0,0}),raw({19,0,0}),raw({20,0x6e}),raw({21,0x6c}),raw({22,0x6f}),raw({23,0x70}),
 raw({24,0,0,0x6e,0x6c}),raw({25,0,0,0x6e,0x6c}),raw({26}),raw({27}),raw({28}),raw({29}),raw({30})};
 CHECK(cases.size()==31);
 for(unsigned i=0;i<cases.size();++i)
 {
  CHECK(positive(cases[i]).opcode==i);
  auto padded=raw({i|0x80u,0x80,0x80,0x80,0});padded.insert(padded.end(),cases[i].begin()+1,cases[i].end());
  CHECK(positive(padded).opcode==i);
 }
 for(unsigned flags=0;flags<256;++flags)for(unsigned opcode:{24u,25u})
 {
  auto b=raw({opcode,flags,37,0x6e,0x6c});auto r=scan(b,flags<4?b.size():0);
  if(flags<4)CHECK(r.first==37&&r.from.nullable==bool(flags&1)&&r.to.nullable==bool(flags&2)&&r.from.heap.code==-18&&r.to.heap.code==-20);
  else CHECK(r.error==v3::gc_immediate_error::cast_flags);
 }
 CHECK(positive(raw({0,0xff,0xff,0xff,0xff,0x0f})).first==0xffffffffu);
 CHECK(positive(raw({8,1,0xff,0xff,0xff,0xff,0x0f})).second==0xffffffffu);
 CHECK(positive(raw({22,0xff,0xff,0xff,0xff,0x0f})).to.heap.code==0xffffffffll);
 for(unsigned final=0;final<256;++final)
 {
  auto b=raw({0,0xff,0xff,0xff,0xff,final});scan(b,final<=15?b.size():0);
 }
 for(unsigned opcode=31;opcode<128;++opcode)CHECK(scan(raw({opcode}),0).error==v3::gc_immediate_error::unknown_opcode);
 CHECK(scan(raw({0xff,0xff,0xff,0xff,0x0f}),0).error==v3::gc_immediate_error::unknown_opcode);
 for(unsigned opcode:{20u,21u,22u,23u})
 {
  for(unsigned heap=0x69;heap<=0x74;++heap)
  {
   auto r=positive(raw({opcode,heap}));CHECK(r.to.heap.code==std::int64_t(heap)-128&&r.to.nullable==bool(opcode&1));
   CHECK(scan(raw({opcode,heap|0x80,0x7f}),0).error==v3::gc_immediate_error::malformed);
  }
 }
 CHECK(scan(raw({24,0x80,0,0,0x6e,0x6c}),0).error==v3::gc_immediate_error::cast_flags);
 std::printf("PASS Core 3 GC immediate: %u guarded cases, all 31 opcodes, transactional cursor, cast flags, full u32/s33 indices\n",checks);
}
