#include <uwvm2/utils/thread/execution_domain.h>
#include <uwvm2/utils/thread/keyed_wait_set.h>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <latch>
#include <stop_token>
#include <thread>
#include <utility>
#define CHECK(condition) do { if(!(condition)) { std::fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#condition); std::abort(); } } while(false)
namespace thread = uwvm2::utils::thread;
int main()
{
    thread::execution_domain domain{1};
    thread::keyed_wait_set waits{};
    unsigned memory_identity{};
    thread::wait_key key{&memory_identity, 8};
    for(unsigned round{}; round != 20; ++round)
    {
        auto lease{domain.try_enter()};
        CHECK(lease && !domain.try_enter());
        auto old_token{lease.cancellation_token()};
        std::latch waiting{1}, cancelled{1}, may_exit{1}, cleanup{1}, may_publish{1};
        std::atomic<bool> exited{}, reset_done{}, cleanup_entered{};
        // Cancellation callbacks can query admission without deadlocking its mutex.
        std::stop_callback callback{old_token,[&]{CHECK(!domain.try_enter());}};
        std::thread worker{[&,execution=std::move(lease)]() mutable
        {
            CHECK(waits.wait(key,-1,[&]{waiting.count_down();return true;},execution.cancellation_token())
                == thread::keyed_wait_result::cancelled);
            CHECK(execution.stop_requested());
            cancelled.count_down();
            may_exit.wait();
            exited.store(true,std::memory_order_release);
            // Runtime cleanup (TLS/native stack restoration) must happen before
            // this admission is released. The reset callback observes that ordering.
            execution.reset();
        }};
        waiting.wait();
        std::thread resetter{[&]
        {
            domain.reset([&]
            {
                CHECK(exited.load(std::memory_order_acquire));
                CHECK(!domain.try_enter());
                cleanup_entered.store(true,std::memory_order_release);
                cleanup.count_down();
                may_publish.wait();
            });
            reset_done.store(true,std::memory_order_release);
        }};
        cancelled.wait();
        CHECK(!domain.try_enter());
        CHECK(!cleanup_entered.load(std::memory_order_acquire));
        CHECK(!reset_done.load(std::memory_order_acquire));
        may_exit.count_down();
        cleanup.wait();
        CHECK(!domain.try_enter());
        CHECK(!reset_done.load(std::memory_order_acquire));
        may_publish.count_down();
        worker.join(); resetter.join();
        CHECK(reset_done.load(std::memory_order_acquire));
        auto next{domain.try_enter()};
        CHECK(next && !next.stop_requested());
        CHECK(old_token.stop_requested());
        CHECK(next.cancellation_token() != old_token);
    }
    // Explicit stop closes admission even without any currently active execution.
    domain.request_stop();
    CHECK(!domain.try_enter());
    domain.reset([]{});
    CHECK(domain.try_enter());
    // Failure cannot reopen partially replaced resources. A later administrative
    // reset can still recover the stopped, drained domain.
    try {domain.reset([]{throw 17;}); CHECK(false);}
    catch(int error) {CHECK(error==17);}
    CHECK(!domain.try_enter());
    domain.reset([]{});
    CHECK(domain.try_enter());
    domain.stop_and_drain();
    CHECK(!domain.try_enter());
    // Producer shutdown and execution drain are one maintenance operation. A
    // reset racing the callback must not reopen admission before it completes.
    for(unsigned round{};round!=16;++round)
    {
        domain.reset([]{});
        auto admission{domain.try_enter()};
        CHECK(admission);
        std::latch cancelled{1}, may_shutdown{1}, shutdown_started{1}, reset_started{1};
        std::atomic<bool> released{}, shutdown_finished{}, reset_finished{};
        std::stop_callback stop{admission.cancellation_token(),[&]{cancelled.count_down();}};
        std::thread stopper{[&]
        {
            domain.stop_and_drain([&]
            {
                CHECK(released.load(std::memory_order_acquire));
                CHECK(!domain.try_enter());
                shutdown_started.count_down();
                may_shutdown.wait();
                CHECK(!reset_finished.load(std::memory_order_acquire));
                shutdown_finished.store(true,std::memory_order_release);
            });
        }};
        cancelled.wait();
        CHECK(!shutdown_finished.load(std::memory_order_acquire));
        released.store(true,std::memory_order_release);
        admission.reset();
        shutdown_started.wait();
        std::thread resetter{[&]
        {
            reset_started.count_down();
            domain.reset([&]{CHECK(shutdown_finished.load(std::memory_order_acquire));});
            reset_finished.store(true,std::memory_order_release);
        }};
        reset_started.wait();
        CHECK(!domain.try_enter());
        may_shutdown.count_down();
        stopper.join();resetter.join();
        CHECK(reset_finished.load(std::memory_order_acquire));
        CHECK(domain.try_enter());
    }
    try {domain.stop_and_drain([]{throw 23;});CHECK(false);}
    catch(int error) {CHECK(error==23);}
    CHECK(!domain.try_enter());
    domain.reset([]{});
    CHECK(domain.try_enter());
    thread::execution_domain no_admissions{0};
    CHECK(!no_admissions.try_enter());
    no_admissions.reset([]{});
    CHECK(!no_admissions.try_enter());
    std::puts("execution domain cancellation, drain-before-reset, generation isolation, producer shutdown serialization, fail-closed recovery: PASS");
}
