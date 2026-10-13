#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/wasm_threads/impl.h>
#include <atomic>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <latch>
#define CHECK(c) do {if(!(c)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#c);std::abort();}}while(false)
namespace wasm=uwvm2::runtime::wasm_threads;
namespace util=uwvm2::utils::thread;
using status=wasm::wait_status;
template<typename Memory>
void check()
{
    Memory memory{},other{};
    memory.sequentially_consistent_size=true;other.sequentially_consistent_size=true;
    memory.init_by_page_count(1);other.init_by_page_count(1);
    util::execution_domain owner{16};wasm::wait_domain waits{};
    {
        auto lease=owner.try_enter();wasm::execution_scope scope{waits,lease.cancellation_token()};
        CHECK(wasm::memory_wait<4>(memory,true,0,1,0).status==status::not_equal);
        CHECK(wasm::memory_wait<8>(memory,true,0,0,0).status==status::timed_out);
        CHECK(wasm::memory_wait<4>(memory,false,0,0,-1).status==status::not_shared);
        CHECK(wasm::memory_wait<8>(memory,true,4,0,0).status==status::unaligned);
        CHECK(wasm::memory_wait<4>(memory,true,65536,0,0).status==status::out_of_bounds);
        CHECK(wasm::memory_wait<8>(memory,true,UINT64_MAX-7,0,0).status==status::out_of_bounds);
        CHECK(wasm::memory_notify(memory,2,0).status==status::unaligned);
        CHECK(wasm::memory_notify(memory,65536,0).status==status::out_of_bounds);
        CHECK(wasm::memory_notify(memory,0,UINT32_MAX).notified==0);
    }
    CHECK(wasm::current_execution==nullptr);
    CHECK(wasm::memory_wait<4>(memory,true,0,0,0).status==status::unavailable);
    // wait32 and wait64 share a location, but other offsets/resources do not.
    std::array<status,4> results{};std::array<std::thread,4> threads{};
    std::latch admitted{4};
    for(unsigned i{};i!=4;++i)
    {
        threads[i]=std::thread{[&,i]
        {
            auto lease=owner.try_enter();CHECK(lease);
            wasm::execution_scope scope{waits,lease.cancellation_token()};admitted.count_down();
            if(i==1){results[i]=wasm::memory_wait<8>(memory,true,0,0,-1).status;}
            else{results[i]=wasm::memory_wait<4>(i==3 ? other : memory,true,i==2 ? 8 : 0,0,-1).status;}
        }};
    }
    admitted.wait();
    // A blocking wait must release allocator pins before suspension. Repeated
    // relocation preserves both the wait key and zeroed contents at offset 0.
    for(unsigned i{};i!=8;++i) {CHECK(memory.try_grow_silently(1));}
    {
        auto lease=owner.try_enter();wasm::execution_scope scope{waits,lease.cancellation_token()};
        unsigned counts[3]{};auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(counts[0]!=2 || counts[1]!=1 || counts[2]!=1)
        {
            CHECK(std::chrono::steady_clock::now()<until);
            auto a=wasm::memory_notify(memory,0,1);CHECK(a.status==status::notified && a.notified<=1);counts[0]+=a.notified;
            counts[1]+=wasm::memory_notify(memory,8,UINT32_MAX).notified;
            counts[2]+=wasm::memory_notify(other,0,1).notified;
            std::this_thread::yield();
        }
        CHECK(wasm::memory_notify(memory,0,UINT32_MAX).notified==0);
    }
    for(auto& t:threads){t.join();}for(auto result:results){CHECK(result==status::notified);}
    // Reset owns cancellation and drains all wait nodes before reusing a memory
    // identity. Cancellation never becomes an invented Wasm wait return value.
    std::latch blocked{1};status stopped{};
    std::thread waiter{[&]{auto lease=owner.try_enter();CHECK(lease);wasm::execution_scope scope{waits,lease.cancellation_token()};
        blocked.count_down();stopped=wasm::memory_wait<4>(memory,true,0,0,-1).status;}};
    blocked.wait();owner.reset([]{});waiter.join();CHECK(stopped==status::cancelled);
    {
        auto lease=owner.try_enter();CHECK(lease);wasm::execution_scope scope{waits,lease.cancellation_token()};
        CHECK(wasm::memory_wait<4>(memory,true,0,0,1).status==status::timed_out);
        auto outer=wasm::current_execution;
        {wasm::execution_scope nested{waits,lease.cancellation_token()};CHECK(wasm::current_execution!=outer);}
        CHECK(wasm::current_execution==outer);
    }
    CHECK(wasm::current_execution==nullptr);
}
int main()
{
#if defined(UWVM_SUPPORT_MMAP)
    check<uwvm2::object::memory::linear::mmap_memory_t>();
    std::puts("mmap wait checks PASS");
#endif
    check<uwvm2::object::memory::linear::allocator_memory_t>();
    std::puts("PASS Wasm wait layer: allocator, wait32/64, bounds/alignment, grow, notify identity/count, VM reset cancellation");
}
