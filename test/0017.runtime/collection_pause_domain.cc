#include <uwvm2/utils/thread/collection_pause_domain.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <latch>
#include <thread>
#include <utility>
#include <vector>

#define CHECK(...) do { if(!(__VA_ARGS__)) { ::fast_io::io::perrln("FAIL ", __LINE__, ": ", #__VA_ARGS__); ::fast_io::fast_terminate(); } } while(false)
namespace coord = ::uwvm2::utils::thread;
using domain_type = coord::collection_pause_domain;
using result = coord::collection_pause_result;
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{5}; }
struct context { ::std::size_t worker{}, counter{}; ::std::uint64_t witness{0xf00dcafe'87654321ull}; };

int main()
{
    {
        domain_type empty{0};
        CHECK(!empty.enter());
        auto ticket{empty.request_pause()}; CHECK(ticket);
        CHECK(empty.wait_until_paused(ticket, deadline()) == result::paused);
        CHECK(empty.while_stopped(ticket, [](auto view) noexcept
        {
            CHECK(view.participant_count() == 0uz);
            view.for_each([](auto) noexcept { CHECK(false); });
        }) == result::paused);
        ticket.reset(); CHECK(!empty.pause_requested());
    }
    // The allocating reader publishes itself without waiting for itself.
    {
        domain_type domain{1}, other{1}; auto self{domain.enter()}; CHECK(self);
        context root{9uz, 17uz};
        auto ticket{domain.request_pause(self, &root)}; CHECK(ticket);
        CHECK(!domain.request_pause(self, &root));
        CHECK(!other.request_pause(self, &root));
        CHECK(domain.wait_until_paused(ticket, deadline()) == result::paused);
        CHECK(other.wait_until_paused(ticket, deadline()) == result::stale_ticket);
        auto moved{::std::move(ticket)}; CHECK(moved && !ticket);
        CHECK(domain.while_stopped(moved, [&](auto view) noexcept
        {
            CHECK(view.participant_count() == 1uz);
            view.for_each([&](auto stopped) noexcept
            {
                CHECK(stopped.id == self.identifier() && stopped.collecting && !stopped.blocking);
                CHECK(stopped.root_context == &root);
            });
        }) == result::paused);
        moved.reset();
        CHECK(domain.wait_until_paused(moved, deadline()) == result::stale_ticket);
        CHECK(!domain.pause_requested());
    }
    // Readers may remain stopped across consecutive epochs; no partially
    // resumed root frame may enter the next stopped-world view.
    {
        domain_type domain{9}; auto self{domain.enter()}; CHECK(self);
        context root{8uz, 0uz};
        ::std::latch enrolled{8}; ::std::atomic_bool finish{};
        ::std::atomic<::std::uint64_t> progress{};
        ::std::vector<::std::thread> workers;
        for(::std::size_t index{}; index != 8uz; ++index)
        {
            workers.emplace_back([&, index]
            {
                auto lease{domain.enter()}; CHECK(lease);
                context local{index, 0uz}; enrolled.count_down();
                while(!finish.load(::std::memory_order_relaxed))
                {
                    ++local.counter;
                    lease.poll(&local);
                    progress.fetch_add(1u, ::std::memory_order_relaxed);
                }
            });
        }
        enrolled.wait(); CHECK(!domain.enter());
        for(::std::size_t epoch{}; epoch != 128uz; ++epoch)
        {
            root.counter = epoch;
            auto ticket{domain.request_pause(self, &root)}; CHECK(ticket);
            CHECK(domain.wait_until_paused(ticket, deadline()) == result::paused);
            auto const before{progress.load(::std::memory_order_relaxed)};
            CHECK(domain.while_stopped(ticket, [&](auto view) noexcept
            {
                CHECK(view.participant_count() == 9uz);
                ::std::array<bool, 9uz> seen{};
                view.for_each([&](auto stopped) noexcept
                {
                    CHECK(stopped.root_context != nullptr);
                    auto const& value{*static_cast<context const*>(stopped.root_context)};
                    CHECK(value.worker < seen.size() && !seen[value.worker]);
                    CHECK(value.witness == 0xf00dcafe'87654321ull && !stopped.blocking);
                    seen[value.worker] = true;
                    CHECK(stopped.collecting == (value.worker == 8uz));
                    if(stopped.collecting) { CHECK(value.counter == epoch); }
                });
                for(unsigned index{}; index != 1000u; ++index)
                { CHECK(progress.load(::std::memory_order_relaxed) == before); }
            }) == result::paused);
        }
        finish.store(true, ::std::memory_order_relaxed);
        for(auto& worker : workers) { worker.join(); }
    }
    // A manager cannot mark another still-running native reader as the
    // collector merely by holding a const reference to that reader's lease.
    {
        domain_type domain{1};
        ::std::latch enrolled{1}, finish{1};
        ::std::atomic<domain_type::participant const*> peer{};
        ::std::thread running{[&]
        {
            auto lease{domain.enter()}; CHECK(lease);
            peer.store(&lease, ::std::memory_order_release); enrolled.count_down();
            finish.wait();
        }};
        enrolled.wait(); context false_root{};
        auto stolen{domain.request_pause(*peer.load(::std::memory_order_acquire), &false_root)};
        CHECK(!stolen && !domain.pause_requested());
        finish.count_down(); running.join();
    }
    // Admission closes even when the collector is the only active reader.
    {
        domain_type domain{2}; auto self{domain.enter()}; context root{};
        auto ticket{domain.request_pause(self, &root)}; CHECK(ticket);
        ::std::latch attempting{1}; ::std::atomic_bool admitted{};
        ::std::thread newcomer{[&]
        {
            attempting.count_down(); auto lease{domain.enter()}; CHECK(lease);
            CHECK(lease.identifier() != self.identifier()); admitted.store(true);
        }};
        attempting.wait();
        CHECK(domain.while_stopped(ticket, [&](auto view) noexcept
        { CHECK(view.participant_count() == 1uz && !admitted.load()); }) == result::paused);
        ticket.reset(); newcomer.join(); CHECK(admitted.load());
    }
    // A sleeping reader publishes before the native wait. A native wakeup
    // waits for the collecting ticket before the guest is allowed to mutate.
    {
        domain_type domain{2}; auto self{domain.enter()}; context root{0uz, 4uz};
        ::std::latch parked{1}, wake{1}, finishing_block{1}; ::std::atomic_bool resumed{};
        ::std::thread sleeper{[&]
        {
            auto lease{domain.enter()}; CHECK(lease); context local{1uz, 27uz};
            auto blocked{lease.park_for_blocking(&local)}; CHECK(blocked);
            auto moved{::std::move(blocked)}; CHECK(moved && !blocked);
            parked.count_down(); wake.wait(); finishing_block.count_down();
            moved.reset(); ++local.counter; resumed.store(true);
        }};
        parked.wait();
        auto ticket{domain.request_pause(self, &root)}; CHECK(ticket);
        CHECK(domain.wait_until_paused(ticket, deadline()) == result::paused);
        wake.count_down(); finishing_block.wait();
        CHECK(domain.while_stopped(ticket, [&](auto view) noexcept
        {
            CHECK(!resumed.load() && view.participant_count() == 2uz);
            unsigned blocked_count{};
            view.for_each([&](auto stopped) noexcept
            {
                auto const& local{*static_cast<context const*>(stopped.root_context)};
                if(stopped.blocking) { ++blocked_count; CHECK(local.worker == 1uz && local.counter == 27uz && !stopped.collecting); }
            });
            CHECK(blocked_count == 1u);
        }) == result::paused);
        ticket.reset(); sleeper.join(); CHECK(resumed.load());
    }
    // A timeout never authorizes a sweep under an uncooperative native caller.
    {
        domain_type domain{2}; auto self{domain.enter()}; context root{};
        ::std::latch enrolled{1}, return_from_host{1};
        ::std::thread host{[&]
        { auto lease{domain.enter()}; CHECK(lease); enrolled.count_down(); return_from_host.wait(); }};
        enrolled.wait(); bool committed{};
        {
            auto ticket{domain.request_pause(self, &root)}; CHECK(ticket);
            CHECK(domain.wait_until_paused(ticket, ::std::chrono::steady_clock::now()) == result::timeout);
            CHECK(domain.while_stopped(ticket, [&](auto) noexcept { committed = true; }) == result::timeout);
        }
        CHECK(!committed && !domain.pause_requested());
        return_from_host.count_down(); host.join();
    }
    // A competing allocator does not create a second collector or wait for
    // itself: it first joins the winning request through its ordinary poll.
    {
        domain_type domain{2}; auto self{domain.enter()}; context root{};
        ::std::latch enrolled{1}, attempt{1}; ::std::atomic_bool rejected{};
        ::std::thread contender{[&]
        {
            auto lease{domain.enter()}; CHECK(lease); context local{1uz, 51uz};
            enrolled.count_down(); attempt.wait();
            auto second{domain.request_pause(lease, &local)}; CHECK(!second); rejected.store(true);
            lease.poll(&local);
        }};
        enrolled.wait(); auto ticket{domain.request_pause(self, &root)}; CHECK(ticket);
        attempt.count_down(); CHECK(domain.wait_until_paused(ticket, deadline()) == result::paused);
        CHECK(rejected.load()); ticket.reset(); contender.join();
    }
    // Cancellation/close cannot race the stopped commit; destruction drains
    // outstanding tickets even when no active native reader remains.
    {
        domain_type domain{0}; auto ticket{domain.request_pause()}; CHECK(ticket);
        ::std::latch attempting{1}; ::std::atomic_bool closed{}; ::std::thread closer;
        CHECK(domain.while_stopped(ticket, [&](auto) noexcept
        {
            closer = ::std::thread{[&] { attempting.count_down(); domain.close(); closed.store(true); }};
            attempting.wait(); CHECK(!closed.load());
        }) == result::paused);
        closer.join(); CHECK(closed.load());
        CHECK(domain.wait_until_paused(ticket, deadline()) == result::closed);
        CHECK(!domain.enter() && !domain.request_pause());
        ::std::latch draining{1}; ::std::atomic_bool drained{};
        ::std::thread drainer{[&] { draining.count_down(); domain.drain(); drained.store(true); }};
        draining.wait(); CHECK(!drained.load()); ticket.reset(); drainer.join(); CHECK(drained.load());
    }
    // Native EH unwinds the collecting ticket before its participant/frame.
    {
        domain_type domain{1};
        try
        {
            auto self{domain.enter()}; context root{};
            auto ticket{domain.request_pause(self, &root)}; CHECK(ticket);
            CHECK(domain.wait_until_paused(ticket, deadline()) == result::paused);
            throw 7;
        }
        catch(int code) { CHECK(code == 7); }
        domain.drain(); CHECK(!domain.pause_requested()); CHECK(domain.enter());
    }
    ::fast_io::io::println("collection pause PASS; self initiator, 8 readers x 128 epochs, blocked roots, admission, timeout, contenders, commit/close, ticket drain, native EH");
}
