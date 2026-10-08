// Actual linked infinite waits must observe a durable pause-domain close.
// Old wake-only close is demonstrated with UWVM2TEST_OLD_PAUSE_WAKE_PREDICATE.
#include <uwvm2/utils/thread/cooperative_pause_domain.h>
#include <uwvm2/utils/thread/keyed_wait_set.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <chrono>
#include <latch>
#include <thread>
namespace th = ::uwvm2::utils::thread;
using domain = th::cooperative_pause_domain;
using waits = th::keyed_wait_set;
struct policy {
    domain& control; domain::participant const& participant;
    ::std::atomic_uint& visits;
    auto register_waker(void* p, void (*wake)(void*) noexcept) const noexcept
    { return participant.register_pause_waker(p, wake); }
    bool requested() const noexcept {
#if defined(UWVM2TEST_OLD_PAUSE_WAKE_PREDICATE)
        return control.pause_requested();
#else
        return control.wait_interrupt_requested();
#endif
    }
    bool suspend(waits::suspension_borrow const&) noexcept {
        visits.fetch_add(1u);
        participant.poll({1u, 2u, 16u, 1u});
        return !control.is_closed();
    }
};
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{3}; }
static void require(bool b) { if(!b) { ::fast_io::fast_terminate(); } }
int main() {
    unsigned resource{};
    th::wait_key key{::std::addressof(resource), 16u};
    unsigned cells{};
    for(bool prepared : {false,true}) for(unsigned round{}; round != 20u; ++round) {
        domain control{2u}; waits registry{};
        ::std::array<waits::prepared_wait_spec,2u> specs{{
            {key,-1,0u,waits::prepared_wait_completion::pending},
            {key,-1,1u,waits::prepared_wait_completion::pending}}};
        auto batch{prepared ? registry.prepare_ordered_waits(specs) : nullptr};
        if(prepared) { require(bool(batch)); batch->start_deadlines(); }
        ::std::atomic_uint completed{}, visits{}, comparisons{};
        ::std::latch entered{2};
        ::std::array<waits::suspension_result,2u> outcomes{};
        ::std::array<::std::thread,2u> workers;
        for(unsigned i{}; i != 2u; ++i) workers[i] = ::std::thread{[&,i] {
            auto participant{control.enter()}; require(bool(participant));
            policy actual{control,participant,visits};
            if(prepared) { entered.count_down(); outcomes[i] = batch->consume(i,key,actual); }
            else outcomes[i] = registry.wait_suspendable(key,-1,[&] {
                comparisons.fetch_add(1u); entered.count_down(); return true; },actual);
            completed.fetch_add(1u);
        }};
        entered.wait();
        for(unsigned episode{}; episode != 2u; ++episode) {
            auto ticket{control.request_pause()}; require(bool(ticket));
            require(control.wait_until_paused(ticket,deadline()) == th::cooperative_pause_result::paused);
            require(control.capture(ticket).participants.size() == 2u);
            require(completed.load() == 0u);
            require(control.resume(ticket));
            // Ensure the workers can re-enter the real infinite native wait,
            // rather than testing only close while still parked in poll().
            ::std::this_thread::sleep_for(::std::chrono::milliseconds{25});
        }
        require(visits.load() == 4u && !control.pause_requested());
        control.close();
#ifndef UWVM2TEST_OLD_PAUSE_WAKE_PREDICATE
        require(control.wait_interrupt_requested() && !control.pause_requested());
#endif
        auto until{::std::chrono::steady_clock::now()+::std::chrono::milliseconds{300}};
        while(completed.load()!=2u && ::std::chrono::steady_clock::now()<until)
            ::std::this_thread::yield();
        bool const finished{completed.load()==2u};
        // Failure cleanup joins genuine workers; this rescue is never counted
        // as passing cancellation and cannot manufacture a Wasm result.
        if(!finished) { (void)registry.notify(key,2u); }
        for(auto& worker : workers) worker.join();
        control.drain();
        if(!finished) {
            ::fast_io::io::perrln("FAIL domain close resumed infinite waits prepared=",prepared);
            return 1;
        }
        for(auto const& outcome : outcomes)
            require(outcome.outcome==th::keyed_wait_result::cancelled && outcome.aborted_by_suspension);
        require(comparisons.load()==(prepared ? 0u : 2u));
        require(registry.notify(key,2u)==0u);
        batch.reset(); registry.close_and_drain(); ++cells;
    }
    ::fast_io::io::println("PASS durable domain close: ",cells," cells, two genuine workers, two pauses, normal/prepared queues, no compare replay");
}
