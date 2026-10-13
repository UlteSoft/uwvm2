#include <uwvm2/utils/thread/keyed_wait_set.h>
#include <uwvm2/utils/thread/execution_lifetime.h>
#include <atomic>
#include <array>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <cstdio>
#include <latch>
#include <limits>
#include <thread>
#include <vector>
#define CHECK(condition) do { if(!(condition)) { std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#condition); std::abort(); } } while(false)
namespace thread = uwvm2::utils::thread;
using result = thread::keyed_wait_result;
int main()
{
    static_assert(thread::has_keyed_wait_set);
    unsigned owner_a{}, owner_b{};
    thread::wait_key const key{&owner_a,4}, adjacent{&owner_a,8}, separate{&owner_b,4};
    thread::keyed_wait_set waits{};
    CHECK(waits.wait(key,0,[]{return false;}) == result::not_equal);
    CHECK(waits.wait(key,0,[]{return true;}) == result::timed_out);
    CHECK(waits.wait(key,1,[]{return true;}) == result::timed_out);
    CHECK(waits.notify(key,100)==0);
    // Per-resource capacity is independent of timeout and of other keys.
    CHECK(waits.wait(key,-1,[]{return true;},{},0)==result::too_many_waiters);
    CHECK(waits.wait(key,0,[]{return true;},{},0)==result::timed_out);
    {
        std::latch queued{1};
        std::thread first_waiter{[&]{CHECK(waits.wait(key,-1,[&]{queued.count_down();return true;},{},1)==result::notified);}};
        queued.wait();
        CHECK(waits.wait(key,-1,[]{return true;},{},1)==result::too_many_waiters);
        CHECK(waits.wait(key,-1,[]{return false;},{},1)==result::not_equal);
        CHECK(waits.notify(key,1)==1);
        first_waiter.join();
    }
    std::latch registered{4};
    std::array<result,4> results{};
    std::array<thread::wait_key,4> keys{key,key,adjacent,separate};
    std::vector<std::thread> workers{};
    for(unsigned i{};i!=4;++i)
    {
        workers.emplace_back([&,i]{results[i]=waits.wait(keys[i],-1,[&]{registered.count_down();return true;});});
    }
    registered.wait(); // Notify serializes with the comparison AND queue insertion.
    CHECK(waits.notify(key,0)==0);
    CHECK(waits.notify(key,1)==1);
    CHECK(waits.notify(key,100)==1);
    CHECK(waits.notify(key,100)==0); // Already selected waiters cannot be counted twice.
    CHECK(waits.notify(adjacent,100)==1);
    CHECK(waits.notify(separate,100)==1);
    for(auto& worker:workers) {worker.join();}
    for(auto r:results) {CHECK(r==result::notified);}
    workers.clear();
    // Exercise compare/enqueue vs notify without arbitrary sleeps or polling.
    for(unsigned i{};i!=20;++i)
    {
        std::latch entered{1};
        result r{};
        std::thread worker{[&]{r=waits.wait(key,-1,[&]{entered.count_down();return true;});}};
        entered.wait();
        CHECK(waits.notify(key,1)==1);
        worker.join();
        CHECK(r==result::notified);
    }
    std::latch entered{2};
    for(unsigned i{};i!=2;++i)
    {
        workers.emplace_back([&,i]{results[i]=waits.wait(keys[i],i ? std::numeric_limits<std::int64_t>::max() : -1,
            [&]{entered.count_down();return true;});});
    }
    entered.wait();
    waits.close_and_drain();
    for(auto& worker:workers) {worker.join();}
    CHECK(results[0]==result::closed && results[1]==result::closed);
    CHECK(waits.is_closed());
    CHECK(waits.notify(key,100)==0);
    bool compared{};
    CHECK(waits.wait(key,-1,[&]{compared=true;return true;})==result::closed);
    CHECK(!compared);
    waits.close_and_drain(); // Idempotent; no stale stack nodes or events.
    // The owner closes admission before cancelling blocked work, then drains
    // leases before freeing resource identities. A host thread is never detached.
    thread::execution_lifetime executions{2};
    thread::keyed_wait_set owned_waits{};
    auto first{executions.try_enter()}, second{executions.try_enter()};
    CHECK(first && second && !executions.try_enter());
    std::latch active{2};
    std::atomic<unsigned> stopped{};
    auto execute=[&](thread::execution_lifetime::lease lease)
    {
        auto r=owned_waits.wait(key,-1,[&]{active.count_down();return true;});
        CHECK(r==result::closed && lease.stop_requested());
        stopped.fetch_add(1,std::memory_order_relaxed);
    };
    std::thread one{execute,std::move(first)}, two{execute,std::move(second)};
    CHECK(!first && !second);
    active.wait();
    executions.request_stop();
    CHECK(!executions.try_enter());
    owned_waits.close_and_drain();
    executions.drain();
    CHECK(stopped.load()==2);
    one.join();two.join();
    thread::execution_lifetime no_admission{0};
    CHECK(!no_admission.try_enter());
    // Two execution owners share a notification domain. Stopping one must not
    // close the shared registry or cancel the other owner's blocked operation.
    thread::execution_lifetime owner_one{1}, owner_two{1};
    thread::keyed_wait_set shared_waits{};
    std::latch shared_entered{2};
    auto lease_one{owner_one.try_enter()}, lease_two{owner_two.try_enter()};
    result cancelled{}, surviving{};
    auto shared_call=[&](thread::execution_lifetime::lease lease,result& output)
    {
        output=shared_waits.wait(key,-1,[&]{shared_entered.count_down();return true;},lease.cancellation_token());
    };
    std::thread third{shared_call,std::move(lease_one),std::ref(cancelled)};
    std::thread fourth{shared_call,std::move(lease_two),std::ref(surviving)};
    shared_entered.wait();
    owner_one.request_stop();
    owner_one.drain();
    third.join();
    CHECK(cancelled==result::cancelled && !shared_waits.is_closed());
    CHECK(shared_waits.notify(key,10)==1);
    fourth.join();
    CHECK(surviving==result::notified);
    owner_two.request_stop();owner_two.drain();
    std::stop_source already_stopped{};
    (void)already_stopped.request_stop();
    CHECK(shared_waits.wait(key,-1,[]{std::abort();return true;},already_stopped.get_token())==result::cancelled);
    // Stop vs compare/enqueue: notification cannot disappear before parking.
    for(unsigned i{};i!=20;++i)
    {
        std::latch comparing{1};std::stop_source stop{};
        result r{};
        std::thread worker{[&]{r=shared_waits.wait(key,-1,[&]{comparing.count_down();return true;},stop.get_token());}};
        comparing.wait();(void)stop.request_stop();worker.join();
        CHECK(r==result::cancelled);
    }
    shared_waits.close_and_drain();
    std::puts("PASS keyed wait set: exact wake counts, identity/offset isolation, timeout, enqueue race, shutdown/drain, per-owner cancellation");
}
