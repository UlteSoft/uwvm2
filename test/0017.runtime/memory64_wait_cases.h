#pragma once
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <thread>
#include <latch>
#include <sys/wait.h>
#include <unistd.h>
#define WAIT_CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL wait64 %u: %s\n",__LINE__,#x);std::abort();}}while(false)
namespace memory64_wait_test
{
namespace waiting=uwvm2::runtime::wasm_threads;
namespace util=uwvm2::utils::thread;
inline unsigned checks{},expected_trap{};
inline std::uint64_t expected_offset{},expected_address{},expected_length{};
inline unsigned expected_bytes{};
inline void complete_trap(unsigned kind){_Exit(expected_trap==kind?92:93);}
template<typename Memory>void initialize(Memory& memory,std::size_t pages=1)
{
#if defined(UWVM_SUPPORT_MMAP)
 if constexpr(Memory::can_mmap){memory.status=uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;}
#endif
 if constexpr(requires{memory.sequentially_consistent_size;}){memory.sequentially_consistent_size=true;}
 memory.init_by_page_count(pages);
}
template<typename Memory,typename Invoke>void cases(Invoke invoke)
{
 Memory memory{};initialize(memory);util::execution_domain owner{16};waiting::wait_domain domain;
 auto lease=owner.try_enter();WAIT_CHECK(lease);waiting::execution_scope scope{domain,lease.cancellation_token()};
 WAIT_CHECK(invoke.template operator()<0>(memory,4,516,0,0)==0);++checks;
 auto failure=[&]<unsigned Operation>(std::uint64_t offset,std::uint64_t address,unsigned reason,bool shared=true,bool cancel=false)
 {
  auto pid=fork();WAIT_CHECK(pid>=0);
  if(!pid)
  {
   expected_trap=reason;expected_offset=offset;expected_address=address;expected_length=65536;expected_bytes=Operation==2?8:4;
   if constexpr(requires{memory.sequentially_consistent_size;}){memory.sequentially_consistent_size=shared;}
   if(cancel)
   {
    std::stop_source stop;stop.request_stop();waiting::execution_scope stopped{domain,stop.get_token()};
    (void)invoke.template operator()<Operation>(memory,offset,address,0,-1);
   }
   else{(void)invoke.template operator()<Operation>(memory,offset,address,0,0);}
   _Exit(94);
  }
  int status{};WAIT_CHECK(waitpid(pid,&status,0)==pid);
  if(!WIFEXITED(status)||WEXITSTATUS(status)!=92){std::fprintf(stderr,"operation=%u offset=%llx address=%llx trap=%u status=%x\n",Operation,(unsigned long long)offset,(unsigned long long)address,reason,status);std::abort();}++checks;
 };
 auto negatives=[&]<unsigned Operation>()
 {
  failure.template operator()<Operation>(0,1,1);
  failure.template operator()<Operation>(8,~0ull-7,2); // Carry would alias offset zero without u65 checking.
  failure.template operator()<Operation>(~0ull-7,8,2);
  failure.template operator()<Operation>(0,1ull<<32,2);
  failure.template operator()<Operation>(1ull<<32,0,2);
  failure.template operator()<Operation>(0,65536,2);
 };
 negatives.template operator()<0>();
 if constexpr(!requires{memory.sequentially_consistent_size;})
 {
  failure.template operator()<1>(0,0,3,false);failure.template operator()<2>(0,0,3,false);
  return; // This memory policy cannot represent a shared linear memory.
 }
 negatives.template operator()<1>();negatives.template operator()<2>();
 WAIT_CHECK(invoke.template operator()<1>(memory,4,516,1,0)==1);++checks;
 WAIT_CHECK(invoke.template operator()<1>(memory,4,516,0x100000000ull,0)==2);++checks;
 WAIT_CHECK(invoke.template operator()<2>(memory,4,516,0,0)==2);++checks;
 WAIT_CHECK(invoke.template operator()<2>(memory,4,516,0x100000000ull,0)==1);++checks;

 failure.template operator()<1>(0,0,3,false);failure.template operator()<2>(0,0,4,true,true);
 // Two widths share the same owner+effective-offset key. Allocator growth must
 // not retain a pin while a guest is sleeping or change the wait identity.
 std::array<unsigned,2> results{};std::latch ready{2};
 std::thread first([&]{auto entry=owner.try_enter();WAIT_CHECK(entry);waiting::execution_scope entered{domain,entry.cancellation_token()};ready.count_down();results[0]=invoke.template operator()<1>(memory,4,516,0,-1);});
 std::thread second([&]{auto entry=owner.try_enter();WAIT_CHECK(entry);waiting::execution_scope entered{domain,entry.cancellation_token()};ready.count_down();results[1]=invoke.template operator()<2>(memory,512,8,0,-1);});
 ready.wait();
 if constexpr(Memory::can_mmap||Memory::support_multi_thread){for(unsigned i=0;i<4;++i){WAIT_CHECK(memory.try_grow_silently(1));}}
 unsigned notified{};auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(notified!=2)
 {
  WAIT_CHECK(std::chrono::steady_clock::now()<until);
  auto count=invoke.template operator()<0>(memory,8,512,1,0);WAIT_CHECK(count<=1);notified+=count;std::this_thread::yield();
 }
 first.join();second.join();WAIT_CHECK(results[0]==0&&results[1]==0);++checks;
#if defined(UWVM_SUPPORT_MMAP)
 if constexpr(Memory::can_mmap&&sizeof(std::size_t)>=8)
 {
  Memory large{};initialize(large,65537);auto const high=0x100000000ull;
  WAIT_CHECK(invoke.template operator()<1>(large,high+4,516,0,0)==2);
  WAIT_CHECK(invoke.template operator()<2>(large,4,high+516,0,0)==2);
  std::latch admitted{2};std::array<unsigned,2> outcomes{};
  std::thread low_wait([&]{auto entry=owner.try_enter();waiting::execution_scope entered{domain,entry.cancellation_token()};admitted.count_down();outcomes[0]=invoke.template operator()<1>(large,4,516,0,-1);});
  std::thread high_wait([&]{auto entry=owner.try_enter();waiting::execution_scope entered{domain,entry.cancellation_token()};admitted.count_down();outcomes[1]=invoke.template operator()<2>(large,high+4,516,0,-1);});
  admitted.wait();unsigned low{},upper{};until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(low!=1||upper!=1)
  {
   WAIT_CHECK(std::chrono::steady_clock::now()<until);
   low+=invoke.template operator()<0>(large,4,516,0xffffffffull,0);
   upper+=invoke.template operator()<0>(large,4,high+516,0xffffffffull,0);
   WAIT_CHECK(low<=1&&upper<=1);std::this_thread::yield();
  }
  low_wait.join();high_wait.join();WAIT_CHECK(outcomes[0]==0&&outcomes[1]==0);checks+=3;
 }
#endif
}
}
