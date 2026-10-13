#pragma once
#include <array>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <thread>
#include <sys/wait.h>
#include <unistd.h>
#define BULK_CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL bulk %u: %s\n",__LINE__,#x);std::abort();}}while(false)
namespace memory64_bulk_test
{
inline unsigned checks{};
inline std::byte const volatile* observed{};
inline std::byte const* expected{};
inline std::uint64_t expected_offset{},expected_length{};
inline bool trap_expected{};
inline void complete_trap(int=0)
{
 if(!trap_expected){_Exit(90);}
 for(unsigned i=0;i<65536;++i){if(observed[i]!=expected[i]){_Exit(91);}}
 _Exit(92);
}
template<typename Memory>void initialize(Memory& memory,std::size_t pages=1)
{
#if defined(UWVM_SUPPORT_MMAP)
 if constexpr(Memory::can_mmap){memory.status=uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;}
#endif
 memory.init_by_page_count(pages);
}
template<bool Copy,bool Destination64,bool Source64,typename Memory,typename Invoke>
void cases(Memory& destination,Memory& source,Invoke invoke)
{
 constexpr bool Length64=Copy?(Destination64&&Source64):Destination64;
 std::array<std::byte,65536> before{},source_before{},want{};
 for(unsigned i=0;i<65536;++i){before[i]=std::byte(i*17+3);source_before[i]=std::byte(i*29+0x53);}
 auto reset=[&]{std::memcpy(destination.memory_begin,before.data(),65536);std::memcpy(source.memory_begin,source_before.data(),65536);};
 auto success=[&](std::uint64_t dst,std::uint64_t src,std::uint64_t n,bool alias)
 {
  reset();want=before;
  if constexpr(Copy){std::memmove(want.data()+dst,(alias?before:source_before).data()+src,n);}
  else {std::memset(want.data()+dst,0xef,n);}
  invoke(destination,alias?destination:source,dst,Copy?src:0xdeadbeef,n);
  BULK_CHECK(!std::memcmp(destination.memory_begin,want.data(),65536));
  BULK_CHECK(!std::memcmp(source.memory_begin,source_before.data(),65536));++checks;
 };
 success(17,31,255,false);success(65520,65520,16,false);success(65536,65536,0,false);
 success(0,0,0,false);success(65536,0,0,false);success(0,65536,0,false);
 if constexpr(Copy){success(18,17,255,true);success(17,18,255,true);success(17,17,255,true);}
 auto failure=[&](std::uint64_t dst,std::uint64_t src,std::uint64_t n,bool alias=false)
 {
  reset();auto pid=fork();BULK_CHECK(pid>=0);
  if(!pid)
  {
   observed=destination.memory_begin;expected=before.data();trap_expected=true;expected_length=n;
   bool source_failed=Copy&&(src>65536||n>65536-src);expected_offset=source_failed?src:dst;
   std::signal(SIGSEGV,complete_trap);std::signal(SIGBUS,complete_trap);std::signal(SIGABRT,complete_trap);
   std::signal(SIGILL,complete_trap);std::signal(SIGTRAP,complete_trap);
   invoke(destination,alias?destination:source,dst,Copy?src:0xdeadbeef,n);_Exit(93);
  }
  int status{};BULK_CHECK(waitpid(pid,&status,0)==pid);
  if(!WIFEXITED(status)||WEXITSTATUS(status)!=92)
  {std::fprintf(stderr,"bulk trap failed: copy=%u d64=%u s64=%u dst=%llx src=%llx n=%llx status=%x\n",
    Copy,Destination64,Source64,(unsigned long long)dst,(unsigned long long)src,(unsigned long long)n,status);std::abort();}
  ++checks;
 };
 failure(65535,0,2);failure(65537,0,0);failure(0,0,65537);
 failure(0,0,Length64?~0ull:0xffffffffull);
 if constexpr(Copy){failure(0,65535,2);failure(0,65537,0);failure(65535,65535,2,true);}
 if constexpr(Destination64){failure(0x100000011ull,0,1);failure(~0ull,0,0);failure(~0ull,0,2);}
 else {failure(0xffffffffull,0,2);}
 if constexpr(Copy&&Source64){failure(0,0x100000011ull,1);failure(0,~0ull,0);failure(0,~0ull,2);}
 if constexpr(Length64){failure(0,0,0x100000001ull);failure(17,17,~0ull);}
 Memory empty{};initialize(empty,0);
 invoke(empty,empty,0,Copy?0:0xdeadbeef,0);++checks;
}
// Touch just two pages of sparse >4 GiB reservations. This detects accidental
// u32 truncation with a successful real transfer, without allocating 4 GiB RAM.
template<bool Copy,bool Destination64,bool Source64,typename Memory,typename Invoke>
void high_addresses(Invoke invoke)
{
#if defined(UWVM_SUPPORT_MMAP)
 if constexpr(sizeof(std::size_t)>=8&&Memory::can_mmap&&(Destination64||(Copy&&Source64)))
 {
  Memory destination{},source{};initialize(destination,Destination64?65537:1);initialize(source,Copy&&Source64?65537:1);
  std::uint64_t dst=Destination64?0x100000011ull:17,src=Copy&&Source64?0x10000001full:31;
  std::array<std::byte,32> payload{};for(unsigned i=0;i<32;++i){payload[i]=std::byte(i*7+5);}
  std::memset(destination.memory_begin+17,0xa7,32);std::memset(destination.memory_begin+dst,0xa7,32);
  std::memcpy(source.memory_begin+src,payload.data(),32);
  invoke(destination,source,dst,Copy?src:0xdeadbeef,32);
  if constexpr(Copy){BULK_CHECK(!std::memcmp(destination.memory_begin+dst,payload.data(),32));}
  else {for(unsigned i=0;i<32;++i){BULK_CHECK(destination.memory_begin[dst+i]==std::byte{0xef});}}
  if constexpr(Destination64){for(unsigned i=0;i<32;++i){BULK_CHECK(destination.memory_begin[17+i]==std::byte{0xa7});}}
  ++checks;
 }
#endif
}

template<typename Memory,typename Invoke>void concurrent_grow(Invoke invoke)
{
 if constexpr(Memory::support_multi_thread)
 {
  Memory a{},b{};initialize(a);initialize(b);
  std::atomic<bool> start{};std::array<std::thread,4> workers{};
  for(unsigned id=0;id<4;++id)
  {
   auto offset=64+id*512;std::memset(a.memory_begin+offset,id+7,64);std::memset(b.memory_begin+offset,id+7,64);
   workers[id]=std::thread([&,id,offset]
   {
    while(!start.load(std::memory_order_acquire)){}
    for(unsigned i=0;i<1000;++i)
    {
     // Each worker owns disjoint byte ranges; only relocation/growth metadata
     // is shared. Opposite directions exercise the native pin ordering.
     invoke(a,b,offset,offset,64);invoke(b,a,offset,offset,64);
    }
   });
  }
  start.store(true,std::memory_order_release);
  for(unsigned i=1;i<16;++i){BULK_CHECK(a.grow_strictly(1,16*65536));BULK_CHECK(b.grow_strictly(1,16*65536));}
  for(auto& worker:workers){worker.join();}
  for(unsigned id=0;id<4;++id)for(unsigned i=0;i<64;++i)
  {BULK_CHECK(a.memory_begin[64+id*512+i]==std::byte(id+7));BULK_CHECK(b.memory_begin[64+id*512+i]==std::byte(id+7));}
  ++checks;
 }
}
}
