#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/object/memory/linear/impl.h>
#include <atomic>
#include <array>
#include <algorithm>
#include <thread>
#include <latch>
#include <cstdio>
#include <cstdlib>
#include <utility>
#define CHECK(c) do {if(!(c)){std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#c);std::abort();}}while(false)
// The new policy must not move existing access metadata or double mmap records.
namespace linear=uwvm2::object::memory::linear;
struct allocator_layout_before
{
    std::byte* memory_begin; std::size_t memory_length; unsigned custom_page_size_log2;
    std::atomic_flag* growing_flag_p; std::atomic_size_t* active_ops_p; std::size_t diagnostic_owner_memory_index;
};
static_assert(offsetof(linear::allocator_memory_t,growing_flag_p)==offsetof(allocator_layout_before,growing_flag_p));
static_assert(offsetof(linear::allocator_memory_t,active_ops_p)==offsetof(allocator_layout_before,active_ops_p));
static_assert(offsetof(linear::allocator_memory_t,diagnostic_owner_memory_index)==offsetof(allocator_layout_before,diagnostic_owner_memory_index));
#if defined(UWVM_SUPPORT_MMAP)
struct alignas(64) mmap_layout_before
{
    std::byte* reserved_begin; std::byte* memory_begin; std::atomic_size_t* memory_length_p;
    unsigned custom_page_size_log2; bool require_dynamic_determination_memory_size_cached;
    linear::mmap_memory_status_t status; uwvm2::utils::mutex::mutex_t* growing_mutex_p;
    std::size_t reservation_limit_bytes; std::size_t diagnostic_owner_memory_index;
};
static_assert(sizeof(linear::mmap_memory_t)==sizeof(mmap_layout_before));
static_assert(offsetof(linear::mmap_memory_t,status)==offsetof(mmap_layout_before,status));
static_assert(offsetof(linear::mmap_memory_t,growing_mutex_p)==offsetof(mmap_layout_before,growing_mutex_p));
static_assert(offsetof(linear::mmap_memory_t,diagnostic_owner_memory_index)==offsetof(mmap_layout_before,diagnostic_owner_memory_index));
#endif
template<typename Memory>
void check(bool shared)
{
    Memory memory{};memory.sequentially_consistent_size=shared;memory.init_by_page_count(1);
    CHECK(memory.get_page_size()==1);
    std::array<std::array<std::size_t,8>,4> old{};std::array<std::thread,4> growers{};
    std::latch start{5};std::atomic<bool> done{};
    std::thread observer{[&]
    {
        start.count_down();start.wait();std::size_t previous=1;
        do {auto size=memory.get_page_size();CHECK(size>=previous && size<=33);previous=size;}
        while(!done.load(std::memory_order_acquire));
    }};
    for(unsigned i{};i!=4;++i) {growers[i]=std::thread{[&,i]
    {
        start.count_down();start.wait();
        for(auto& before:old[i]) {CHECK(memory.try_grow_silently(1,33*65536,&before));}
    }};}
    for(auto& thread:growers){thread.join();}done.store(true,std::memory_order_release);observer.join();
    std::array<std::size_t,32> sorted{};std::size_t k{};
    for(auto const& row:old) for(auto n:row){sorted[k++]=n;}
    std::sort(sorted.begin(),sorted.end());for(k=0;k!=32;++k){CHECK(sorted[k]==k+1);}
    CHECK(memory.get_page_size()==33);
    std::size_t before{};CHECK(memory.try_grow_silently(0,33*65536,&before) && before==33);
    CHECK(!memory.try_grow_silently(1,33*65536,&before) && memory.get_page_size()==33);
    CHECK(memory.grow_strictly(1,34*65536,&before) && before==33 && memory.get_page_size()==34);
    Memory moved{std::move(memory)};CHECK(moved.sequentially_consistent_size==shared && moved.get_page_size()==34);
    memory=std::move(moved);CHECK(memory.sequentially_consistent_size==shared && memory.get_page_size()==34);
}
int main()
{
    for(bool shared:{false,true})
    {
#if defined(UWVM_SUPPORT_MMAP)
        check<uwvm2::object::memory::linear::mmap_memory_t>(shared);
#endif
        check<uwvm2::object::memory::linear::allocator_memory_t>(shared);
    }
    std::puts("PASS shared/unshared memory sizes: 4 concurrent growers, monotonic observer, unique old sizes, zero/failed/strict grow, move policy");
}
