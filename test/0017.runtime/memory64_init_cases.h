#pragma once
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <sys/wait.h>
#include <unistd.h>
#define INIT_CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL init64 %u: %s\n",__LINE__,#x);std::abort();}}while(false)
namespace memory64_init_test
{
namespace storage=uwvm2::uwvm::runtime::storage;
inline unsigned checks{};
inline bool expected_trap{};
inline std::uint64_t expected_address{},expected_bound{};
inline std::uint32_t expected_length{};
inline std::byte const volatile* observed{};
inline void complete_trap()
{
 if(!expected_trap){_Exit(91);}
 for(unsigned i=0;i<65536;++i){if(observed[i]!=std::byte{0xa7}){_Exit(92);}}
 _Exit(93);
}
template<typename Memory>void initialize(Memory& memory,std::size_t pages=1)
{
#if defined(UWVM_SUPPORT_MMAP)
 if constexpr(Memory::can_mmap){memory.status=uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;}
#endif
 memory.init_by_page_count(pages);
}
template<typename Memory,typename Invoke>void cases(Invoke invoke)
{
 Memory memory{};initialize(memory);std::array<std::byte,64> bytes{};
 for(unsigned i=0;i<64;++i){bytes[i]=std::byte(i*13+7);}
 storage::local_defined_data_storage_t data{};data.data.kind=storage::wasm_data_segment_kind::passive;
 data.data.byte_begin=bytes.data();data.data.byte_end=bytes.data()+bytes.size();
 auto success=[&](std::uint64_t dst,std::uint32_t src,std::uint32_t len)
 {
  std::memset(memory.memory_begin,0xa7,65536);invoke(memory,data,dst,src,len);
  for(unsigned i=0;i<65536;++i){INIT_CHECK(memory.memory_begin[i]==(i>=dst&&i-dst<len?bytes[src+i-dst]:std::byte{0xa7}));}++checks;
 };
 success(17,3,39);success(65520,48,16);success(65536,64,0);success(0,64,0);success(0,0,0);
 auto failure=[&](std::uint64_t dst,std::uint32_t src,std::uint32_t len,std::size_t source_bound=64)
 {
  std::memset(memory.memory_begin,0xa7,65536);auto pid=fork();INIT_CHECK(pid>=0);
  if(!pid)
  {
   auto source_failed=src>source_bound||len>source_bound-src;
   expected_trap=true;expected_address=source_failed?src:dst;expected_bound=source_failed?source_bound:65536;expected_length=len;observed=memory.memory_begin;
   invoke(memory,data,dst,src,len);_Exit(94);
  }
  int status{};INIT_CHECK(waitpid(pid,&status,0)==pid);
  if(!WIFEXITED(status)||WEXITSTATUS(status)!=93){std::fprintf(stderr,"init64 dst=%llx src=%x len=%x bound=%zu status=%x\n",(unsigned long long)dst,src,len,source_bound,status);std::abort();}++checks;
 };
 failure(65535,0,2);failure(65537,0,0);failure(0,63,2);failure(0,65,0);
 failure(0x100000011ull,0,1);failure(~0ull,0,0);failure(~0ull,0,2);
 failure(0,0xffffffffu,1);failure(0,1,0xffffffffu);failure(0,0,0xffffffffu);
 storage::drop_wasm_data_segment_payload(data.data);storage::drop_wasm_data_segment_payload(data.data);
 success(65536,0,0);success(0,0,0);failure(0,0,1,0);failure(0,1,0,0);failure(65537,0,0,0);
 // Active data has already been dropped by instantiation and obeys the same
 // empty-instance rule, including a zero-length source/destination check.
 data.data.kind=storage::wasm_data_segment_kind::active;success(65536,0,0);failure(0,0,1,0);
 Memory empty{};initialize(empty,0);storage::local_defined_data_storage_t empty_data{};
 invoke(empty,empty_data,0,0,0);++checks;
#if defined(UWVM_SUPPORT_MMAP)
 if constexpr(Memory::can_mmap&&sizeof(std::size_t)>=8)
 {
  Memory high{};initialize(high,65537);storage::local_defined_data_storage_t live{};
  live.data.byte_begin=bytes.data();live.data.byte_end=bytes.data()+bytes.size();
  std::memset(high.memory_begin+17,0xa7,32);std::memset(high.memory_begin+0x100000011ull,0xa7,32);
  invoke(high,live,0x100000011ull,7,32);
  for(unsigned i=0;i<32;++i){INIT_CHECK(high.memory_begin[17+i]==std::byte{0xa7});INIT_CHECK(high.memory_begin[0x100000011ull+i]==bytes[7+i]);}++checks;
 }
#endif
}
}
