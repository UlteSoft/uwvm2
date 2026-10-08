#include <uwvm2/utils/thread/cooperative_pause_domain.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <latch>
#include <thread>
#include <utility>
#include <vector>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::abort(); } } while(false)
namespace thread = uwvm2::utils::thread;
using domain_type = thread::cooperative_pause_domain;
using result = thread::cooperative_pause_result;
auto deadline() { return std::chrono::steady_clock::now()+std::chrono::seconds{5}; }
int main()
{
    {
        domain_type empty{0};
        CHECK(!empty.enter());
        auto ticket=empty.request_pause(); CHECK(ticket);
        CHECK(empty.wait_until_paused(ticket,deadline())==result::paused);
        CHECK(empty.capture(ticket).participants.empty());
        bool committed{}; CHECK(empty.while_stopped(ticket,[&]{committed=true;})); CHECK(committed);
        CHECK(empty.resume(ticket)); CHECK(!empty.resume(ticket));
    }
    // A pause blocks admission even with no active executions.
    {
        domain_type domain{1}; auto ticket=domain.request_pause();
        CHECK(domain.wait_until_paused(ticket,deadline())==result::paused);
        std::latch attempting{1}; std::atomic<bool> admitted{};
        std::thread newcomer{[&]{attempting.count_down();auto lease=domain.enter();CHECK(lease);admitted=true;}};
        attempting.wait();
        CHECK(domain.while_stopped(ticket,[&]{CHECK(!admitted.load());}));
        CHECK(domain.resume(ticket)); newcomer.join(); CHECK(admitted.load());
    }
    {
        domain_type domain{8}, other{1};
        std::latch entered{8}; std::atomic<bool> finish{}; std::atomic<unsigned long long> progress{};
        std::vector<std::thread> workers;
        for(unsigned i{};i!=8;++i)
        {
            workers.emplace_back([&,i]
            {
                auto lease=domain.enter(); CHECK(lease); entered.count_down();
                while(!finish.load(std::memory_order_relaxed))
                {
                    lease.poll({17,i,23+i,42});
                    progress.fetch_add(1,std::memory_order_relaxed);
                }
            });
        }
        entered.wait(); CHECK(!domain.enter());
        domain_type::pause_ticket previous;
        for(unsigned round{};round!=128;++round)
        {
            auto ticket=domain.request_pause(); CHECK(ticket); CHECK(!domain.request_pause());
            CHECK(domain.wait_until_paused(ticket,deadline())==result::paused);
            CHECK(other.wait_until_paused(ticket,deadline())==result::stale_ticket);
            CHECK(!other.resume(ticket)); CHECK(!domain.resume(previous));
            auto snapshot=domain.capture(ticket); CHECK(snapshot.result==result::paused);
            CHECK(snapshot.participants.size()==8);
            std::array<bool,8> seen{};
            for(auto const& participant:snapshot.participants)
            {
                auto const& where=participant.location;
                CHECK(participant.id!=0 && where.code_unit==17 && where.code_generation==42);
                CHECK(where.function<8 && where.offset==23+where.function && !seen[where.function]);
                seen[where.function]=true;
            }
            auto before=progress.load();
            for(unsigned i{};i!=1000;++i) { CHECK(progress.load()==before); }
            CHECK(domain.while_stopped(ticket,[&]{CHECK(progress.load()==before);}));
            previous=ticket;
            CHECK(domain.resume(ticket)); CHECK(domain.capture(ticket).result==result::stale_ticket);
        }
        finish=true; for(auto& worker:workers) {worker.join();}
        domain.drain(); CHECK(domain.enter());
    }
    // Host work is not a safe point; timeout leaves the request active. Once the
    // host returns, a real poll completes the same request with its exact location.
    {
        domain_type domain{1}; std::latch host_entered{1}, host_release{1};
        std::thread worker{[&]{auto lease=domain.enter();CHECK(lease);host_entered.count_down();host_release.wait();lease.poll({1,2,3,4});}};
        host_entered.wait(); auto ticket=domain.request_pause();
        CHECK(domain.wait_until_paused(ticket,std::chrono::steady_clock::now())==result::timeout);
        CHECK(domain.capture(ticket).result==result::timeout);
        bool committed{}; CHECK(!domain.while_stopped(ticket,[&]{committed=true;})); CHECK(!committed);
        host_release.count_down(); CHECK(domain.wait_until_paused(ticket,deadline())==result::paused);
        CHECK(domain.capture(ticket).participants[0].location==(thread::cooperative_pause_location{1,2,3,4}));
        CHECK(domain.resume(ticket)); worker.join();
    }
    // Deterministic pause-after-observer window: a request arriving after the
    // ordinary observer's decision still crosses the cold hook before parking.
    {
        domain_type domain{1}; std::latch observer_returned{1}, enter_poll{1};
        std::atomic<unsigned> captures{};
        std::thread worker{[&]
        {
            auto lease=domain.enter();CHECK(lease);
            lease.poll({1,2,3,4},[&]() noexcept {captures.fetch_add(1);});
            CHECK(captures.load()==0); // No capture/allocation on the running path.
            observer_returned.count_down();enter_poll.wait();
            lease.poll({5,6,7,8},[&]() noexcept {captures.fetch_add(1);});
        }};
        observer_returned.wait();auto ticket=domain.request_pause();CHECK(ticket);enter_poll.count_down();
        CHECK(domain.wait_until_paused(ticket,deadline())==result::paused);
        CHECK(captures.load()==1);
        CHECK(domain.capture(ticket).participants[0].location==(thread::cooperative_pause_location{5,6,7,8}));
        CHECK(domain.resume(ticket));worker.join();
    }
    // Native stepping releases only the selected participant. The host can
    // count it as parked again only after independently proving a SIGTRAP stop;
    // each one-instruction resume removes that external park first.
    {
        domain_type domain{2};
        std::latch entered{2}, begin_poll{1}, target_released{1}, finish{1};
        std::atomic<std::uint_least64_t> target_id{};
        std::thread target{[&]
        {
            auto lease=domain.enter();CHECK(lease);
            target_id.store(lease.identifier());entered.count_down();begin_poll.wait();
            lease.poll({31,41,51,61});target_released.count_down();finish.wait();
        }};
        std::thread other{[&]
        {
            auto lease=domain.enter();CHECK(lease);entered.count_down();begin_poll.wait();
            lease.poll({32,42,52,62});
        }};
        entered.wait();auto ticket=domain.request_pause();CHECK(ticket);begin_poll.count_down();
        CHECK(domain.wait_until_paused(ticket,deadline())==result::paused);
        auto const id=target_id.load();CHECK(id!=0);
        CHECK(!domain.release_one_for_native_step(ticket,0));
        CHECK(domain.release_one_for_native_step(ticket,id));
        target_released.wait();
        CHECK(domain.wait_until_paused(ticket,std::chrono::steady_clock::now())==result::timeout);
        CHECK(!domain.while_stopped(ticket,[]{}));
        CHECK(domain.external_park(ticket,id,{31,41,52,61}));
        CHECK(!domain.external_park(ticket,id,{31,41,52,61}));
        CHECK(domain.wait_until_paused(ticket,deadline())==result::paused);
        auto snapshot=domain.capture(ticket);CHECK(snapshot.participants.size()==2);
        bool saw_target{};
        for(auto const& participant:snapshot.participants)
        { if(participant.id==id) {saw_target=true;CHECK(participant.location==(thread::cooperative_pause_location{31,41,52,61}));} }
        CHECK(saw_target);
        CHECK(domain.external_unpark(ticket,id));
        CHECK(!domain.external_unpark(ticket,id));
        CHECK(domain.wait_until_paused(ticket,std::chrono::steady_clock::now())==result::timeout);
        CHECK(domain.external_park(ticket,id,{31,41,53,61}));
        CHECK(domain.wait_until_paused(ticket,deadline())==result::paused);
        CHECK(domain.resume(ticket));
        CHECK(!domain.external_park(ticket,id,{31,41,54,61}));
        finish.count_down();target.join();other.join();domain.drain();
        auto second=domain.request_pause();CHECK(second);
        CHECK(domain.wait_until_paused(second,deadline())==result::paused);
        CHECK(domain.capture(second).participants.empty());CHECK(domain.resume(second));
    }
    // Exceptional exits release participation, even when no final poll runs.
    {
        domain_type domain{1}; std::latch entered{1}, release{1};
        std::thread worker{[&]{try {auto lease=domain.enter();CHECK(lease);entered.count_down();release.wait();throw 7;} catch(int value){CHECK(value==7);}}};
        entered.wait(); auto ticket=domain.request_pause(); release.count_down();
        CHECK(domain.wait_until_paused(ticket,deadline())==result::paused);
        CHECK(domain.capture(ticket).participants.empty());
        worker.join(); CHECK(domain.resume(ticket));
    }
    // Resume cannot race a stopped publication. The callback does not re-enter
    // the domain; a manager contending for its mutex continues only after commit.
    {
        domain_type domain{1}; auto ticket=domain.request_pause();
        std::latch attempting{1}; std::atomic<bool> resumed{}; std::thread manager;
        CHECK(domain.while_stopped(ticket,[&]
        {
            manager=std::thread{[&]{attempting.count_down();CHECK(domain.resume(ticket));resumed=true;}};
            attempting.wait(); CHECK(!resumed.load());
        }));
        manager.join(); CHECK(resumed.load());
    }
    // Move assignment releases its old slot; close wakes both parked participants
    // and admission waiters, while drain still waits for the owning executions.
    {
        domain_type domain{2}; auto first=domain.enter();auto second=domain.enter();CHECK(first&&second);
        first=std::move(second); CHECK(first&&!second); CHECK(domain.enter()); first.reset();
        std::latch entered{1}, leaving{1}, may_leave{1}, attempting{1};
        std::atomic<bool> rejected{};
        std::thread worker{[&]{auto lease=domain.enter();entered.count_down();lease.poll({});leaving.count_down();may_leave.wait();}};
        // Poll may occur before the request; explicitly cycle a dedicated worker below.
        entered.wait(); leaving.wait(); auto ticket=domain.request_pause();
        std::thread newcomer{[&]{attempting.count_down();CHECK(!domain.enter());rejected=true;}};
        attempting.wait(); domain.close();
        CHECK(domain.wait_until_paused(ticket,deadline())==result::closed);
        CHECK(domain.capture(ticket).result==result::closed); CHECK(!domain.resume(ticket)); CHECK(!domain.request_pause());
        newcomer.join(); CHECK(rejected.load()); may_leave.count_down(); worker.join(); domain.drain(); CHECK(!domain.enter());
    }
    for(unsigned round{};round!=32;++round)
    {
        domain_type domain{1}; std::latch entered{1}, poll{1};
        std::thread worker{[&]{auto lease=domain.enter();entered.count_down();poll.wait();lease.poll({});}};
        entered.wait();auto ticket=domain.request_pause();poll.count_down();
        CHECK(domain.wait_until_paused(ticket,deadline())==result::paused);
        domain.close(); worker.join(); domain.drain();
    }
    std::puts("PASS cooperative pauses: 8 participants x 128 rounds, stable snapshots, admission, timeout, stale/foreign tickets, exception cleanup, publication/resume and shutdown");
}
