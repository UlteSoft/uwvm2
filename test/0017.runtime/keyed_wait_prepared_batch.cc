#include <uwvm2/utils/thread/keyed_wait_set.h>
#include <uwvm2/utils/thread/cooperative_pause_domain.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <chrono>
#include <latch>
#include <span>
#include <thread>
#include <type_traits>
namespace th=::uwvm2::utils::thread;
using waits=th::keyed_wait_set;
using domain=th::cooperative_pause_domain;
using ready=waits::prepared_wait_completion;
using spec=waits::prepared_wait_spec;
using outcome=th::keyed_wait_result;
static unsigned checks{};
static void require(bool value,char const* text)
{
    ++checks;
    if(!value)
    {
        ::fast_io::io::perrln("FAIL prepared wait batch: ",::fast_io::mnp::os_c_str(text));
        ::fast_io::fast_terminate();
    }
}
static auto deadline() noexcept
{ return ::std::chrono::steady_clock::now()+::std::chrono::seconds{5}; }
struct actual_policy
{
    domain& control;domain::participant const& participant;th::wait_key key;
    waits::suspension_snapshot* publication{};
    bool abort_after_resume{};
    [[nodiscard]] auto register_waker(void* context,void (*wake)(void*) noexcept) const noexcept
    { return participant.register_pause_waker(context,wake); }
    [[nodiscard]] bool requested() const noexcept { return control.wait_interrupt_requested(); }
    [[nodiscard]] bool suspend(waits::suspension_borrow const& actual) noexcept
    {
        if(publication != nullptr)
        { if(!actual.inspect(key,[&](auto const& copy) noexcept { *publication=copy; })) { ::fast_io::fast_terminate(); } }
        participant.poll({13u,17u,key.position,1u});return !abort_after_resume && !control.is_closed();
    }
};
int main()
{
    static_assert(th::has_keyed_wait_set);
    static_assert(!::std::is_move_constructible_v<waits::prepared_wait_batch>);
    unsigned resource{},other{};
    th::wait_key const key{::std::addressof(resource),32u},different{::std::addressof(other),32u};
    {
        waits registry{};domain control{2u};
        ::std::array<spec,2u> input{{{key,-1,0u,ready::pending},{key,-1,1u,ready::pending}}};
        auto batch{registry.prepare_ordered_waits(input)};
        require(batch && batch->size()==2u,"allocate and register two actual ordered heap nodes");
        auto participant{control.enter()};actual_policy owner_policy{control,participant,key};
        auto before_open{batch->consume(0u,key,owner_policy)};
        require(before_open.aborted_by_suspension,"unarmed startup cannot consume a queue node");
        batch->start_deadlines();
        auto wrong_key{batch->consume(0u,different,owner_policy)};
        auto bad_index{batch->consume(2u,key,owner_policy)};
        require(wrong_key.aborted_by_suspension && bad_index.aborted_by_suspension,"actual key and full index must match before claim");
        participant.reset();
        // Native consumer 1 arrives FIRST. The prelinked queue must still select
        // source node0, even though no native thread has entered consume(0).
        ::std::latch entered{1u};waits::suspension_snapshot snapshot{};
        waits::suspension_result first_arrival{},late_arrival{};
        ::std::thread worker{[&]
        {
            auto lease{control.enter()};if(!lease) { ::fast_io::fast_terminate(); }
            actual_policy policy{control,lease,key,::std::addressof(snapshot)};entered.count_down();
            first_arrival=batch->consume(1u,key,policy);
        }};
        entered.wait();require(registry.notify(key,1u)==1u,"notify selects saved source head before head's native arrival");
        auto ticket{control.request_pause()};
        require(control.wait_until_paused(ticket,deadline())==th::cooperative_pause_result::paused,"first native arrival remains actual pending participant");
        auto roster{control.capture(ticket)};
        require(roster.participants.size()==1u,"notification did not incorrectly complete native arrival1");
        require(!snapshot.notified && snapshot.pending_predecessors==0u,"fresh queue borrow excludes already-selected pending head");
        require(control.resume(ticket),"resume source ordinal1 pending operation");
        ::std::thread late{[&]
        {
            auto lease{control.enter()};if(!lease) { ::fast_io::fast_terminate(); }
            actual_policy policy{control,lease,key};late_arrival=batch->consume(0u,key,policy);
        }};
        late.join();require(late_arrival.outcome==outcome::notified && !late_arrival.aborted_by_suspension,"late head consumes actual previously selected notification");
        require(registry.notify(key,1u)==1u,"second notification selects remaining source ordinal1");
        worker.join();control.drain();
        require(first_arrival.outcome==outcome::notified && !first_arrival.aborted_by_suspension,"reverse native arrival preserves saved FIFO");
        require(registry.notify(key,1u)==0u,"all actual consumed nodes unlinked");
        auto empty{control.enter()};actual_policy policy{control,empty,key};
        require(batch->consume(0u,key,policy).aborted_by_suspension,"detached completed node is one-shot DATA only");
        empty.reset();batch.reset();registry.close_and_drain();control.close();control.drain();
    }
    {
        waits registry{};domain control{1u};auto participant{control.enter()};actual_policy policy{control,participant,key};
        ::std::array<spec,2u> ready_nodes{{{key,-1,UINT64_MAX,ready::notified},{different,0,UINT64_MAX,ready::timed_out}}};
        auto batch{registry.prepare_ordered_waits(ready_nodes)};
        require(batch && registry.notify(key,9u)==0u && registry.notify(different,9u)==0u,"recorded ready outcomes never enter pending queue");
        batch->start_deadlines();
        auto notified{batch->consume(0u,key,policy)},expired{batch->consume(1u,different,policy)};
        require(notified.outcome==outcome::notified && !notified.aborted_by_suspension,"actual owned ready0 consumed without new comparison");
        require(expired.outcome==outcome::timed_out && !expired.aborted_by_suspension,"actual owned ready2 consumed without new timer or insertion");
        batch.reset();participant.reset();registry.close_and_drain();control.close();control.drain();
    }
    {
        waits registry{};domain control{1u};
        ::std::array<spec,2u> pending{{{key,-1,0u,ready::pending},{key,-1,1u,ready::pending}}};
        auto batch{registry.prepare_ordered_waits(pending)};require(static_cast<bool>(batch),"actual failure-cleanup batch admitted");batch->start_deadlines();
        ::std::latch entered{1u};waits::suspension_result completion{};
        ::std::thread worker{[&]
        {
            auto participant{control.enter()};actual_policy policy{control,participant,key};entered.count_down();
            completion=batch->consume(0u,key,policy);
        }};
        entered.wait();auto ticket{control.request_pause()};
        require(control.wait_until_paused(ticket,deadline())==th::cooperative_pause_result::paused,"claimed prepared node truly paused before startup cleanup");
        batch->abort_unconsumed(); // Removes unclaimed1; wakes claimed0, does not fake an OS ACK.
        require(control.capture(ticket).participants.size()==1u,"abort request leaves actual claimed participant live until its own cleanup");
        require(control.resume(ticket),"actual owner releases pause before joining cancelled startup");worker.join();control.drain();
        require(completion.aborted_by_suspension && completion.outcome==outcome::cancelled,"claimed node receives HOST cleanup after actual resume");
        require(registry.notify(key,9u)==0u,"unclaimed and claimed nodes both genuinely unlinked");
        batch.reset();registry.close_and_drain();
    }
    {
        waits registry{};
        ::std::array<spec,2u> ordered{{{key,-1,0u,ready::pending},{key,-1,1u,ready::pending}}};
        auto bad{ordered};bad[1].pending_ordinal=0u;
        require(!registry.prepare_ordered_waits(bad),"duplicate pending ordinal refused before queue publication");
        bad=ordered;bad[0].pending_ordinal=1u;
        require(!registry.prepare_ordered_waits(bad),"missing queue head ordinal refused");
        bad=ordered;bad[0].key.resource=nullptr;
        require(!registry.prepare_ordered_waits(bad),"null native label cannot select an actual resource");
        bad=ordered;bad[0].remaining_nanoseconds=-2;
        require(!registry.prepare_ordered_waits(bad),"noncanonical negative remaining budget refused");
        bad=ordered;bad[0].completion=static_cast<ready>(255u);
        require(!registry.prepare_ordered_waits(bad),"unknown ready shape refused");
        require(!registry.prepare_ordered_waits(ordered,1u) && !registry.prepare_ordered_waits(ordered,4097u),"quota checked before heap allocation/publication");
        require(registry.notify(key,9u)==0u,"all malformed preparations leave real queue unchanged");
        auto batch{registry.prepare_ordered_waits(ordered)};require(static_cast<bool>(batch),"valid batch following malformed inputs");
        require(!registry.prepare_ordered_waits(ordered),"existing real same-key queue cannot be adopted or reordered");
        batch->abort_unconsumed();registry.close_and_drain(); // Batch still exists: counted nodes already actually released.
        require(registry.is_closed(),"unconsumed node abort releases actual drain counts");batch.reset();
    }
    {
        waits registry{};domain control{1u};auto participant{control.enter()};actual_policy policy{control,participant,key};
        ::std::array<spec,1u> expired_pending{{{key,0,0u,ready::pending}}};
        auto batch{registry.prepare_ordered_waits(expired_pending)};
        require(static_cast<bool>(batch),"zero remaining does not manufacture an already-committed ready2");batch->start_deadlines();
        require(registry.notify(key,1u)==1u,"genuine notification may select actual expired pending node before consumption");
        auto selected{batch->consume(0u,key,policy)};
        require(selected.outcome==outcome::notified && !selected.aborted_by_suspension,"zero-budget pending node keeps notify before timeout arbitration");
        batch.reset();batch=registry.prepare_ordered_waits(expired_pending);
        require(static_cast<bool>(batch),"a second true expired pending batch can be prepared after actual unlink");batch->start_deadlines();
        auto expired{batch->consume(0u,key,policy)};
        require(expired.outcome==outcome::timed_out && !expired.aborted_by_suspension,"unselected expired pending consumes actual timeout once");
        batch.reset();participant.reset();registry.close_and_drain();control.close();control.drain();
    }
    {
        waits registry{};domain control{1u};
        ::std::array<spec,1u> finite{{{key,50'000'000LL,0u,ready::pending}}};
        auto batch{registry.prepare_ordered_waits(finite)};require(static_cast<bool>(batch),"finite actual batch allocated");
        ::std::this_thread::sleep_for(::std::chrono::milliseconds{80}); // Before true startup clock begins.
        batch->start_deadlines();auto const began{::std::chrono::steady_clock::now()};
        auto participant{control.enter()};actual_policy policy{control,participant,key};
        auto completion{batch->consume(0u,key,policy)};auto const elapsed{::std::chrono::steady_clock::now()-began};
        require(completion.outcome==outcome::timed_out && !completion.aborted_by_suspension,"prepared pending finite timer consumed once");
        require(elapsed >= ::std::chrono::milliseconds{40},"remaining budget begins at explicit actual open rather than allocation time");
        participant.reset();batch.reset();registry.close_and_drain();control.close();control.drain();
    }
    ::fast_io::io::println("PASS prepared wait batch: ",checks," utility checks; VM restore not qualified");
}
