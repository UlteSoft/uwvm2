#include <uwvm2/utils/thread/immutable_snapshot.h>
#include <atomic>
#include <array>
#include <thread>
#include <latch>
#include <vector>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#define CHECK(condition) do { if(!(condition)) { std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#condition); std::abort(); } } while(false)
struct payload
{
    inline static std::atomic<unsigned> live{};
    unsigned sequence{},inverse{~0u};
    std::array<unsigned,64> values{};
    payload() {++live;}
    payload(payload const& source) : sequence{source.sequence},inverse{source.inverse},values{source.values} {++live;}
    ~payload() {--live;}
    void set(unsigned value) {sequence=value;inverse=~value;values.fill(value);}
    bool valid() const {if(inverse!=~sequence) {return false;}for(auto value:values) {if(value!=sequence) {return false;}}return true;}
};
using domain=uwvm2::utils::thread::immutable_snapshot<payload>;
int main()
{
    {
        domain snapshots{};
        {
            domain::reader first{snapshots},second{snapshots};
            CHECK(first.acquire()==nullptr);
            snapshots.update([](auto& data){data.set(1);});
            auto const one=first.acquire();CHECK(one && one->valid() && one->sequence==1);
            snapshots.update([](auto& data){data.set(2);});
            auto const two=second.acquire();CHECK(two && two->valid() && two->sequence==2);
            for(unsigned i=3;i!=128;++i) {snapshots.update([=](auto& data){data.set(i);});CHECK(payload::live.load()==3);}
            CHECK(one->valid() && one->sequence==1 && two->valid() && two->sequence==2);
            try {snapshots.update([](auto& data){data.set(999);throw std::runtime_error("rollback");});std::abort();}
            catch(std::runtime_error const&) {}
            CHECK(first.acquire()->sequence==127); // Replaces only this reader's prior hazard.
            CHECK(two->sequence==2);
            snapshots.collect();CHECK(payload::live.load()==2);
            snapshots.clear();CHECK(first.acquire()==nullptr); // A different reader's borrow survives clear.
            CHECK(two->valid() && two->sequence==2);
            {payload replacement{};replacement.set(42);snapshots.replace(replacement);}
            CHECK(two->valid() && two->sequence==2);
            CHECK(first.acquire()->sequence==42);
            snapshots.clear();first.release();
            second.release();snapshots.collect();CHECK(payload::live.load()==0);
        }
        snapshots.update([](auto& data){data.set(0);});
        constexpr unsigned readers=4,writers=2,updates=2000;
        std::latch ready{readers+writers};
        std::atomic<unsigned> finished{},reads{};
        std::vector<std::thread> workers{};
        for(unsigned i{};i!=readers;++i)
        {
            workers.emplace_back([&]{domain::reader access{snapshots};ready.count_down();ready.wait();
                do {auto const* data=access.acquire();CHECK(data && data->valid());++reads;}
                while(finished.load()!=writers);});
        }
        for(unsigned i{};i!=writers;++i)
        {
            workers.emplace_back([&]{ready.count_down();ready.wait();
                for(unsigned n{};n!=updates;++n) {snapshots.update([](auto& data){data.set(data.sequence+1);});}
                ++finished;});
        }
        for(auto& worker:workers) {worker.join();}
        CHECK(reads.load()>=readers);
        snapshots.collect();CHECK(payload::live.load()==1);
        {domain::reader final{snapshots};CHECK(final.acquire()->sequence==writers*updates);}
    }
    CHECK(payload::live.load()==0);
    std::puts("PASS immutable snapshots: retained readers, bounded reclamation, clear, exception rollback, concurrent publication/acquire");
}
