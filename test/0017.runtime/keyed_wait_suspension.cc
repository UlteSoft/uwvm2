#include <uwvm2/utils/thread/keyed_wait_set.h>
#include <uwvm2/utils/thread/cooperative_pause_domain.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <chrono>
#include <latch>
#include <stop_token>
#include <thread>
#include <type_traits>

namespace th = ::uwvm2::utils::thread;
using domain = th::cooperative_pause_domain;
using waits = th::keyed_wait_set;
using result = th::keyed_wait_result;
static unsigned checks{};
static void require(bool value, char const* text)
{
    ++checks;
    if(!value)
    {
        ::fast_io::io::perrln("FAIL managed wait suspension: ",::fast_io::mnp::os_c_str(text));
        ::fast_io::fast_terminate();
    }
}
static auto deadline() noexcept
{ return ::std::chrono::steady_clock::now()+::std::chrono::seconds{5}; }

// A real utility-level participant and live wait-key owner. This deliberately
// grants no VM checkpoint/compiler/source/host-call authority.
struct actual_policy
{
    domain& control;
    domain::participant const& participant;
    th::wait_key key;
    void const* wrong_resource;
    waits::suspension_snapshot captured{};
    bool have_snapshot{}, refused_wrong{};
    bool abort_after_resume{};
    unsigned visits{};
    [[nodiscard]] auto register_waker(void* context,void (*wake)(void*) noexcept) const noexcept
    { return participant.register_pause_waker(context,wake); }
    [[nodiscard]] bool requested() const noexcept { return control.wait_interrupt_requested(); }
    [[nodiscard]] bool suspend(waits::suspension_borrow const& borrow) noexcept
    {
        ++visits;
        refused_wrong=!borrow.inspect({wrong_resource,key.position},[](auto const&) noexcept {});
        have_snapshot=borrow.inspect(key,[&](auto const& copy) noexcept { captured=copy; });
        participant.poll({7u,11u,key.position,1u});
        return !abort_after_resume && !control.is_closed();
    }
};

int main()
{
    static_assert(th::has_keyed_wait_set);
    static_assert(!::std::is_copy_constructible_v<waits::suspension_borrow>);
    static_assert(!::std::is_move_constructible_v<waits::suspension_borrow>);
    static_assert(!::std::is_copy_constructible_v<domain::pause_waker>);
    static_assert(!::std::is_move_constructible_v<domain::pause_waker>);
    unsigned resource{},wrong{};
    th::wait_key const key{::std::addressof(resource),64u};
    {
        // Publish in source order using the original compare/insertion lock.
        // Both real linked nodes remain in FIFO order while ALL is parked.
        domain control{2u}; waits registry{};
        ::std::array<::std::latch,2u> compared{::std::latch{1u},::std::latch{1u}};
        ::std::array<waits::suspension_result,2u> outcomes{};
        ::std::array<waits::suspension_snapshot,2u> snapshots{};
        ::std::array<unsigned,2u> compare_count{},visits{};
        ::std::array<bool,2u> snap_ok{},refused{};
        auto execute=[&](unsigned index)
        {
            auto participant{control.enter()};
            if(!participant) { ::fast_io::fast_terminate(); }
            actual_policy policy{control,participant,key,::std::addressof(wrong)};
            outcomes[index]=registry.wait_suspendable(key,-1,[&]
            { ++compare_count[index];compared[index].count_down();return true; },policy);
            snapshots[index]=policy.captured;visits[index]=policy.visits;
            snap_ok[index]=policy.have_snapshot;refused[index]=policy.refused_wrong;
        };
        ::std::thread first{execute,0u};compared[0].wait();
        ::std::thread second{execute,1u};compared[1].wait();
        auto ticket{control.request_pause()};require(static_cast<bool>(ticket),"genuine request wakes infinite native waits");
        require(control.wait_until_paused(ticket,deadline())==th::cooperative_pause_result::paused,"two actual participants parked without removing queue nodes");
        auto roster{control.capture(ticket)};
        require(roster.participants.size()==2u,"actual domain census has both waits");
        require(registry.notify(key,0u)==0u,"zero notify retains real queue");
        require(registry.notify(key,1u)==1u,"first notify selects exactly queue head while paused");
        require(registry.notify(key,1u)==1u,"second notify excludes already selected head");
        require(registry.notify(key,2u)==0u,"paused selected nodes cannot count twice");
        require(control.resume(ticket),"resume real parked cohort");first.join();second.join();control.drain();
        require(outcomes[0].outcome==result::notified && outcomes[1].outcome==result::notified,"notifications survive real pause");
        require(!outcomes[0].aborted_by_suspension && !outcomes[1].aborted_by_suspension,"normal resume is original guest result");
        require(compare_count==::std::array<unsigned,2u>{1u,1u},"paused wait never repeats memory comparison");
        require(visits==::std::array<unsigned,2u>{1u,1u},"one actual suspension per participant");
        require(snap_ok[0] && snap_ok[1] && refused[0] && refused[1],"real borrow refuses wrong resource identity");
        require(snapshots[0].position==64u && snapshots[1].position==64u,"DATA retains actual offset only");
        require(snapshots[0].pending_predecessors==0u && snapshots[1].pending_predecessors==1u,"actual FIFO predecessor rank");
        require(snapshots[0].remaining_nanoseconds==-1 && snapshots[1].remaining_nanoseconds==-1,"infinite timeout typed marker");
        require(!snapshots[0].notified && !snapshots[1].notified,"prepark snapshot precedes controlled notify");
        require(registry.notify(key,9u)==0u,"returned stack nodes are unlinked");
    }
    {
        domain control{1u};waits registry{};::std::latch compared{1u};::std::stop_source stop{};
        waits::suspension_result outcome{};unsigned comparisons{};bool snapshot_ok{};
        ::std::thread worker{[&]
        {
            auto participant{control.enter()};actual_policy policy{control,participant,key,::std::addressof(wrong)};
            policy.abort_after_resume=true;
            outcome=registry.wait_suspendable(key,-1,[&]{++comparisons;compared.count_down();return true;},policy,stop.get_token());
            snapshot_ok=policy.have_snapshot;
        }};
        compared.wait();auto ticket{control.request_pause()};
        require(control.wait_until_paused(ticket,deadline())==th::cooperative_pause_result::paused,"cancellable wait truly parks");
        require(stop.request_stop(),"original cancellation callback can wake node while parked");
        require(control.resume(ticket),"resume cancellation cohort");worker.join();control.drain();
        require(outcome.outcome==result::cancelled && !outcome.aborted_by_suspension,"real stop token wins simultaneous HOST abort");
        require(comparisons==1u && snapshot_ok && registry.notify(key,1u)==0u,"cancelled node and callbacks fully removed");
    }
    {
        domain control{1u};waits registry{};::std::latch compared{1u};waits::suspension_result outcome{};
        waits::suspension_snapshot snapshot{};
        ::std::thread worker{[&]
        {
            auto participant{control.enter()};actual_policy policy{control,participant,key,::std::addressof(wrong)};
            policy.abort_after_resume=true;
            outcome=registry.wait_suspendable(key,5'000'000'000LL,[&]{compared.count_down();return true;},policy);
            snapshot=policy.captured;
        }};
        compared.wait();auto ticket{control.request_pause()};
        require(control.wait_until_paused(ticket,deadline())==th::cooperative_pause_result::paused,"finite wait truly parks");
        registry.close();require(control.resume(ticket),"resume closed registry participant");worker.join();control.drain();
        require(outcome.outcome==result::closed && !outcome.aborted_by_suspension,"registry close wins simultaneous HOST abort");
        require(snapshot.remaining_nanoseconds>0 && snapshot.remaining_nanoseconds<=5'000'000'000LL,"finite snapshot is bounded actual remaining time");
        require(registry.notify(key,1u)==0u,"closed pending node not exposed");
    }
    {
        domain control{1u};waits registry{};::std::latch compared{1u};waits::suspension_result outcome{};
        ::std::thread worker{[&]
        {
            auto participant{control.enter()};actual_policy policy{control,participant,key,::std::addressof(wrong)};
            policy.abort_after_resume=true;
            outcome=registry.wait_suspendable(key,-1,[&]{compared.count_down();return true;},policy);
        }};
        compared.wait();auto ticket{control.request_pause()};
        require(control.wait_until_paused(ticket,deadline())==th::cooperative_pause_result::paused,"abort policy first performs real pause");
        require(control.resume(ticket),"host resume precedes explicit cleanup decision");worker.join();control.drain();
        require(outcome.aborted_by_suspension && outcome.outcome==result::cancelled,"explicit HOST abort is distinct from guest cancellation");
        require(registry.notify(key,1u)==0u,"HOST abort unlinks before caller receives outcome");
        // Existing ordinary wait stays independent of optional suspension policy.
        require(registry.wait(key,0,[]{return false;})==result::not_equal,"ordinary compare result unchanged");
        require(registry.wait(key,0,[]{return true;})==result::timed_out,"ordinary zero timeout unchanged");
    }
    {
        domain control{1u};waits registry{};::std::latch compared{1u};waits::suspension_result outcome{};
        ::std::thread worker{[&]
        {
            auto participant{control.enter()};actual_policy policy{control,participant,key,::std::addressof(wrong)};
            policy.abort_after_resume=true;
            outcome=registry.wait_suspendable(key,-1,[&]{compared.count_down();return true;},policy);
        }};
        compared.wait();auto ticket{control.request_pause()};
        require(control.wait_until_paused(ticket,deadline())==th::cooperative_pause_result::paused,"notify plus abort has a real parked window");
        require(registry.notify(key,1u)==1u,"actual FIFO node is selected while HOST abort policy is suspended");
        require(control.resume(ticket),"release same real notify plus abort window");worker.join();control.drain();
        require(outcome.outcome==result::notified && !outcome.aborted_by_suspension,"already selected notification wins simultaneous HOST abort");
        require(registry.notify(key,1u)==0u,"notify plus abort leaves no stale linked node");
    }
    {
        domain control{1u};waits registry{};::std::latch compared{1u};waits::suspension_result outcome{};
        ::std::thread worker{[&]
        {
            auto participant{control.enter()};actual_policy policy{control,participant,key,::std::addressof(wrong)};
            policy.abort_after_resume=true;
            outcome=registry.wait_suspendable(key,1'000'000'000LL,[&]{compared.count_down();return true;},policy);
        }};
        compared.wait();auto ticket{control.request_pause()};
        require(control.wait_until_paused(ticket,deadline())==th::cooperative_pause_result::paused,"deadline plus abort has a real parked window");
        // Actual monotonic time expires while the real node stays linked and
        // its participant remains parked. No synthetic timeout/result flag.
        ::std::this_thread::sleep_for(::std::chrono::milliseconds{1100});
        require(control.resume(ticket),"release same real expired deadline plus abort window");worker.join();control.drain();
        require(outcome.outcome==result::timed_out && !outcome.aborted_by_suspension,"actual expired deadline wins simultaneous HOST abort");
        require(registry.notify(key,1u)==0u,"deadline plus abort leaves no stale linked node");
    }
    {
        domain control{1u};waits registry{};auto participant{control.enter()};
        actual_policy policy{control,participant,key,::std::addressof(wrong)};unsigned comparisons{};
        auto not_equal{registry.wait_suspendable(key,0,[&]{++comparisons;return false;},policy)};
        auto timeout{registry.wait_suspendable(key,0,[&]{++comparisons;return true;},policy)};
        auto capacity{registry.wait_suspendable(key,-1,[&]{++comparisons;return true;},policy,{},0u)};
        require(not_equal.outcome==result::not_equal && !not_equal.aborted_by_suspension,"managed comparison fails before queue publication");
        require(timeout.outcome==result::timed_out && !timeout.aborted_by_suspension,"managed zero timeout is original timed out");
        require(capacity.outcome==result::too_many_waiters && !capacity.aborted_by_suspension,"managed capacity refusal is original HOST utility status");
        require(comparisons==3u && policy.visits==0u,"immediate results do not manufacture suspension");
        participant.reset();auto denied{registry.wait_suspendable(key,-1,[&]{++comparisons;return true;},policy)};
        require(denied.aborted_by_suspension && comparisons==3u,"empty real participant cannot register managed waiter");
        control.close();control.drain();
    }
    {
        // A wake subscription is real domain lifetime work, even if its
        // participant has already reset. drain must wait for its actual RAII.
        domain control{1u};auto participant{control.enter()};::std::atomic<unsigned> wakes{};
        ::std::atomic<bool> drained{};::std::latch attempt{1u};::std::thread manager;
        {
            auto subscription{participant.register_pause_waker(::std::addressof(wakes),+[](void* context) noexcept
            { static_cast<::std::atomic<unsigned>*>(context)->fetch_add(1u); })};
            require(static_cast<bool>(subscription),"real bounded listener admission");
            auto ticket{control.request_pause()};require(static_cast<bool>(ticket) && wakes.load()==1u,"request synchronously invokes current registered listener");
            require(control.resume(ticket),"resume listener-only request");participant.reset();control.close();
            require(wakes.load()==2u,"actual close wakes subscription once");
            manager=::std::thread{[&]{attempt.count_down();control.drain();drained.store(true);}};
            attempt.wait();require(!drained.load(),"listener remains genuine drain lifetime pin");
        }
        manager.join();require(drained.load(),"unregister releases domain drain");
    }
    ::fast_io::io::println("PASS managed wait suspension: ",checks," utility checks; original wait untouched; VM restore not qualified");
}
