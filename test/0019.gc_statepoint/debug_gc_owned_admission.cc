// Genuine two native owner threads exercise the existing admission word.
// This utility success is not a VM root/cohort/restore issuer.
#include <uwvm2/runtime/gc/entry_admission.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
namespace gc = ::uwvm2::runtime::gc;
static void require(bool ok, unsigned line)
{
    if(ok) { return; }
    ::fast_io::io::perrln("debug GC owned admission FAIL line=", line);
    ::fast_io::fast_terminate();
}
#define REQUIRE(x) require(bool(x), __LINE__)
template<class Predicate> static bool until(Predicate&& predicate) noexcept
{
    auto const deadline{::std::chrono::steady_clock::now() + ::std::chrono::seconds{5}};
    do { if(predicate()) { return true; } ::std::this_thread::yield(); }
    while(::std::chrono::steady_clock::now() < deadline);
    return predicate();
}
int main()
{
    using admission = gc::managed_entry_admission;
    admission word{}, foreign{};
    REQUIRE(word.active_count() == 0u);
    { auto zero{word.try_exclusive(0u)}; REQUIRE(zero && !word.try_enter()); }
    REQUIRE(word.try_enter()); // temporary is an actual issued owner, retired at semicolon
    REQUIRE(word.active_count() == 0u);
    for(unsigned attempt{}; attempt != 16u; ++attempt)
    {
        ::std::array<::std::atomic<admission::shared_lease const*>, 2u> published{};
        ::std::atomic_uint ready{};
        ::std::atomic_bool finish{};
        auto body{[&](::std::size_t index)
        {
            auto actual{word.enter()};
            // [actual stable native stack lease] end; thread retains unchanged
            // until exclusion dies and finish is published by its native owner.
            published[index].store(::std::addressof(actual), ::std::memory_order_release);
            ready.fetch_add(1u, ::std::memory_order_release);
            while(!finish.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
        }};
        ::std::jthread first{body, 0u}, second{body, 1u};
        REQUIRE(until([&] { return ready.load(::std::memory_order_acquire) == 2u; }));
        ::std::array<admission::shared_lease const*, 2u> owners{
            published[0u].load(::std::memory_order_acquire), published[1u].load(::std::memory_order_acquire)};
        REQUIRE(owners[0u] && owners[1u] && owners[0u] != owners[1u]);
        REQUIRE(word.active_count() == 2u && !word.try_exclusive(1u));
        REQUIRE(!word.try_exclusive_owned({}));
        REQUIRE(!word.try_exclusive_owned({owners.data(), 1u})); // omitted real owner
        {
            ::std::array<admission::shared_lease const*, 2u> duplicate{owners[0u], owners[0u]};
            REQUIRE(!word.try_exclusive_owned(duplicate));
            auto foreign_owner{foreign.enter()};
            ::std::array<admission::shared_lease const*, 2u> wrong{owners[0u], ::std::addressof(foreign_owner)};
            REQUIRE(!word.try_exclusive_owned(wrong));
            admission::shared_lease empty{};
            wrong[1u] = ::std::addressof(empty);
            REQUIRE(!word.try_exclusive_owned(wrong));
            wrong[1u] = nullptr;
            REQUIRE(!word.try_exclusive_owned(wrong));
        }
        {
            auto omitted_reader{word.try_enter()};
            REQUIRE(omitted_reader && word.active_count() == 3u);
            REQUIRE(!word.try_exclusive_owned(owners));
        }
        auto exclusion{word.try_exclusive_owned(owners)};
        REQUIRE(exclusion && word.active_count() == 2u);
        REQUIRE(!word.try_enter() && !word.try_exclusive_owned(owners));
        ::std::atomic_bool started{}, entered{}, release{};
        ::std::jthread entrant{[&]
        {
            started.store(true, ::std::memory_order_release);
            auto actual{word.enter()};
            entered.store(true, ::std::memory_order_release);
            while(!release.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
        }};
        REQUIRE(until([&] { return started.load(::std::memory_order_acquire); }));
        REQUIRE(!entered.load(::std::memory_order_acquire) && word.active_count() == 2u);
        exclusion.reset(); // before any native owner exits
        REQUIRE(until([&] { return entered.load(::std::memory_order_acquire); }));
        REQUIRE(word.active_count() == 3u && !word.try_exclusive_owned(owners));
        release.store(true, ::std::memory_order_release); entrant.join();
        REQUIRE(word.active_count() == 2u);
        finish.store(true, ::std::memory_order_release); first.join(); second.join();
        REQUIRE(word.active_count() == 0u); // no dereference of now-expired borrows
    }
    {
        auto owner{word.enter()}; auto moved{::std::move(owner)};
        ::std::array<admission::shared_lease const*, 1u> old{::std::addressof(owner)};
        REQUIRE(!word.try_exclusive_owned(old));
        old[0u] = ::std::addressof(moved);
        auto exclusion{word.try_exclusive_owned(old)}; REQUIRE(exclusion);
    }
    REQUIRE(word.active_count() == 0u);
    ::fast_io::io::println("debug GC owned admission: actual native two-owner controls complete");
}
