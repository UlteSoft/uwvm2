// Real cooperative cohort/episode semantics. This TU does not link runtime,
// private captures, host gate, database assets, GC export or restore dispatch.
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <thread>
#include <utility>
#include <fast_io.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/utils/thread/cooperative_pause_domain.h>
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
static void check(bool value, char const* text)
{
    if(!value)
    {
        ::fast_io::io::perrln("cooperative_stopped_cohort: ", ::fast_io::mnp::os_c_str(text));
        ::fast_io::fast_terminate();
    }
}
template<typename Ready> static void wait(Ready&& ready)
{
    auto const deadline{::std::chrono::steady_clock::now() + ::std::chrono::seconds{3}};
    while(!ready())
    {
        check(::std::chrono::steady_clock::now() < deadline, "bounded actual thread rendezvous");
        ::std::this_thread::yield();
    }
}
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{3}; }
int main()
{
    namespace thread = ::uwvm2::utils::thread;
    using domain = thread::cooperative_pause_domain;
    using result = thread::cooperative_pause_result;
    using record = domain::stopped_participant;
    {
        domain empty{0u}; auto const ticket{empty.request_pause()};
        check(static_cast<bool>(ticket), "real empty pause ticket");
        check(empty.wait_until_paused(ticket, deadline()) == result::paused, "empty display stop is legal");
        ::std::array<record, 1u> scratch{}; bool inspected{};
        check(!empty.with_cooperatively_stopped_cohort(ticket, scratch,
            [&](auto, auto const&) noexcept { inspected = true; }) && !inspected,
            "an empty display pause cannot masquerade as a captured guest cohort");
        check(empty.resume(ticket), "empty request retired");
    }
    {
        domain actual{2u}, other{1u};
        ::std::array<domain::pause_ticket, 2u> episodes{}, captures{};
        ::std::array<record, 2u> scratch{};
        ::std::atomic<unsigned> admitted{}, phase{}, returned{};
        ::std::array<::std::jthread, 2u> guests{};
        for(::std::size_t i{}; i != guests.size(); ++i)
        {
            guests[i] = ::std::jthread{[&, i]() noexcept
            {
                auto participant{actual.enter()}; check(static_cast<bool>(participant), "real guest admission");
                admitted.fetch_add(1u, ::std::memory_order_release);
                for(unsigned round{1u}; round != 3u; ++round)
                {
                    wait([&]() noexcept { return phase.load(::std::memory_order_acquire) >= round; });
                    // The exact same PC in both episodes is intentional: only a
                    // real current-ticket/control identity may accept the copy.
                    participant.poll({17u, 100u + i, 23u, 42u}, [&]() noexcept
                    { captures[i] = episodes[round - 1u]; });
                    returned.fetch_add(1u, ::std::memory_order_release);
                }
            }};
        }
        wait([&]() noexcept { return admitted.load(::std::memory_order_acquire) == 2u; });
        auto const foreign{other.request_pause()}; check(static_cast<bool>(foreign), "different domain real ticket");
        episodes[0] = actual.request_pause(); check(static_cast<bool>(episodes[0]), "first actual request");
        phase.store(1u, ::std::memory_order_release);
        check(actual.wait_until_paused(episodes[0], deadline()) == result::paused, "first two guests cooperatively parked");
        bool inspected{};
        ::std::array<record, 1u> too_small{}; too_small[0] = {777u, {1u, 2u, 3u, 4u}};
        check(!actual.with_cooperatively_stopped_cohort(episodes[0], too_small,
            [&](auto, auto const&) noexcept { inspected = true; }) && !inspected && too_small[0].id == 777u,
            "bounded scratch decline does not invoke consumer or partially overwrite cells");
        check(!actual.with_cooperatively_stopped_cohort(foreign, scratch,
            [&](auto, auto const&) noexcept { inspected = true; }) && !inspected,
            "foreign ticket cannot enter the real cohort transaction");
        check(actual.with_cooperatively_stopped_cohort(episodes[0], scratch,
            [&](::std::span<record const> actual_slots, auto const& current) noexcept
            {
                inspected = true; check(actual_slots.size() == 2u, "whole actual bounded roster");
                check(current(captures[0]) && current(captures[1]) && !current(foreign) && !current(domain::pause_ticket{}),
                    "predicate compares actual privately constructed domain episodes");
                ::std::array<bool, 2u> seen{};
                for(auto const& slot : actual_slots)
                {
                    check(slot.id != 0u && slot.location.code_unit == 17u && slot.location.code_generation == 42u &&
                        slot.location.offset == 23u && slot.location.function >= 100u && slot.location.function < 102u,
                        "copy supplied directly by real leased slots");
                    auto const i{static_cast<::std::size_t>(slot.location.function - 100u)};
                    check(!seen[i], "duplicate participant location not fabricated"); seen[i] = true;
                }
            }) && inspected, "first complete current cohort guarded");
        check(actual.resume(episodes[0]), "first actual request retired");
        wait([&]() noexcept { return returned.load(::std::memory_order_acquire) == 2u; });
        episodes[1] = actual.request_pause(); check(static_cast<bool>(episodes[1]), "second actual request");
        phase.store(2u, ::std::memory_order_release);
        check(actual.wait_until_paused(episodes[1], deadline()) == result::paused, "same PCs new episode cooperatively parked");
        inspected = false;
        check(!actual.with_cooperatively_stopped_cohort(episodes[0], scratch,
            [&](auto, auto const&) noexcept { inspected = true; }) && !inspected,
            "old same-PC ticket cannot enter the new pause");
        ::std::atomic_bool attempting{}, resumed{}; ::std::jthread resumer;
        check(actual.with_cooperatively_stopped_cohort(episodes[1], scratch,
            [&](auto, auto const& current) noexcept
            {
                check(current(captures[0]) && current(captures[1]) && !current(episodes[0]),
                    "each capture must belong to the new current episode");
                // Only this test callback waits for a bounded contention marker;
                // production consumers cannot wait or reenter the domain.
                resumer = ::std::jthread{[&]() noexcept
                {
                    attempting.store(true, ::std::memory_order_release);
                    check(actual.resume(episodes[1]), "real resume after coherent guard");
                    resumed.store(true, ::std::memory_order_release);
                }};
                wait([&]() noexcept { return attempting.load(::std::memory_order_acquire); });
                check(!resumed.load(::std::memory_order_acquire), "resume cannot invalidate an in-progress cohort callback");
            }), "second actual roster and capture episodes remain stable within ONE mutex");
        resumer.join(); check(resumed.load(::std::memory_order_acquire), "contending resume completed after guard");
        for(auto& guest : guests) { guest.join(); }
        actual.drain(); check(other.resume(foreign), "foreign request retired independently");
    }
    {
        domain actual{1u}; ::std::atomic_bool admitted{}, begin{}, released{}, finish{};
        ::std::atomic<::std::uint_least64_t> selected{};
        ::std::jthread guest{[&]() noexcept
        {
            auto participant{actual.enter()}; check(static_cast<bool>(participant), "real native-step guest admission");
            selected.store(participant.identifier(), ::std::memory_order_release);
            admitted.store(true, ::std::memory_order_release);
            wait([&]() noexcept { return begin.load(::std::memory_order_acquire); });
            participant.poll({31u, 41u, 51u, 61u});
            released.store(true, ::std::memory_order_release);
            wait([&]() noexcept { return finish.load(::std::memory_order_acquire); });
        }};
        wait([&]() noexcept { return admitted.load(::std::memory_order_acquire); });
        auto const ticket{actual.request_pause()}; begin.store(true, ::std::memory_order_release);
        check(actual.wait_until_paused(ticket, deadline()) == result::paused, "real cooperative pre-step park");
        ::std::array<record, 1u> scratch{}; bool inspected{};
        check(actual.with_cooperatively_stopped_cohort(ticket, scratch,
            [&](auto, auto const&) noexcept { inspected = true; }) && inspected, "cooperative origin allowed");
        check(actual.release_one_for_native_step(ticket, selected.load(::std::memory_order_acquire)), "real selected slot transferred");
        wait([&]() noexcept { return released.load(::std::memory_order_acquire); });
        check(actual.external_park(ticket, selected.load(::std::memory_order_acquire), {31u, 41u, 52u, 61u}),
            "test host reports an external park through actual domain API");
        check(actual.wait_until_paused(ticket, deadline()) == result::paused && actual.while_stopped(ticket, []() noexcept {}),
            "general display park intentionally permits native trap");
        inspected = false;
        check(!actual.with_cooperatively_stopped_cohort(ticket, scratch,
            [&](auto, auto const&) noexcept { inspected = true; }) && !inspected,
            "native trap cannot reuse an old logical Wasm continuation despite all parked");
        check(actual.resume(ticket), "actual native-step request retired");
        finish.store(true, ::std::memory_order_release); guest.join(); actual.drain();
        actual.close(); inspected = false;
        check(!actual.with_cooperatively_stopped_cohort(ticket, scratch,
            [&](auto, auto const&) noexcept { inspected = true; }) && !inspected, "closed old cohort rejected");
    }
    ::fast_io::io::println("PASS actual coherent cooperative roster, ticket episodes, scratch bounds and native-trap refusal");
}
#else
int main() { return 77; }
#endif
#include <uwvm2/utils/macro/pop_macros.h>
