// Deterministically reproduce a stale T1 publisher, then test the production
// cold publication scope in both writer orders and under concurrent pressure.
#include <uwvm2/runtime/lib/uwvm_runtime_tiered_publication.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <latch>
#include <thread>

namespace
{
    using scope = uwvm2::runtime::lib::details::tiered_entry_publication_scope;
    void check(bool value) { if(!value) { std::abort(); } }
    void yield() noexcept { std::this_thread::yield(); }
    struct publication
    {
        std::uint_least8_t lock{};
        std::atomic<bool> full{};
        std::atomic<unsigned> raw{1u}, typed{1u};
        void lazy() noexcept
        {
            scope transaction{lock, yield};
            auto const selected{full.load(std::memory_order_acquire) ? 2u : 1u};
            raw.store(selected, std::memory_order_release);
            typed.store(selected, std::memory_order_release);
        }
        void promote() noexcept
        {
            scope transaction{lock, yield};
            raw.store(2u, std::memory_order_release);
            typed.store(2u, std::memory_order_release);
            full.store(true, std::memory_order_release);
        }
        void check_full() const
        {
            check(full.load(std::memory_order_acquire));
            check(raw.load(std::memory_order_acquire) == 2u);
            check(typed.load(std::memory_order_acquire) == 2u);
        }
    };
}
int main()
{
    // This old schedule is deterministic, not dependent on winning a rare race:
    // T1 reads false; both old T2 publication passes finish; delayed T1 overwrites.
    publication old;
    auto const stale{old.full.load()};
    old.promote();
    old.raw.store(2u); old.typed.store(2u); // old redundant second pass
    if(!stale) { old.raw.store(1u); old.typed.store(1u); }
    check(old.full.load() && old.raw.load() == 1u && old.typed.load() == 1u);

    publication delayed;
    std::latch observed{1}, resume{1};
    std::thread t1{[&]
    {
        check(!delayed.full.load()); // represents the earlier optimistic probe
        observed.count_down(); resume.wait();
        delayed.lazy(); // rechecks readiness under the real production guard
    }};
    observed.wait(); delayed.promote(); resume.count_down(); t1.join(); delayed.check_full();

    publication first;
    std::latch held{1}, release{1}, contended{1};
    std::thread early{[&]
    {
        scope transaction{first.lock, yield};
        check(!first.full.load()); held.count_down(); release.wait();
        first.raw.store(1u); first.typed.store(1u);
    }};
    held.wait();
    std::thread later{[&]
    {
        bool reported{};
        scope transaction{first.lock, [&]() noexcept
        {
            if(!reported) { reported = true; contended.count_down(); }
            yield();
        }};
        first.raw.store(2u); first.typed.store(2u); first.full.store(true);
    }};
    contended.wait(); check(!first.full.load()); release.count_down();
    early.join(); later.join(); first.check_full();

    publication concurrent;
    std::latch start{1};
    std::array<std::thread, 8> publishers;
    for(auto& thread : publishers)
    {
        thread = std::thread{[&]
        {
            start.wait();
            for(unsigned iteration{}; iteration != 10000u; ++iteration)
            {
                concurrent.lazy();
                if(concurrent.full.load(std::memory_order_acquire)) { concurrent.check_full(); }
            }
        }};
    }
    start.count_down(); concurrent.promote();
    for(auto& thread : publishers) { thread.join(); }
    concurrent.check_full();
    std::puts("PASS tiered publication: stale-T1 reproduction, both deterministic writer orders, 80000 concurrent callbacks");
}
