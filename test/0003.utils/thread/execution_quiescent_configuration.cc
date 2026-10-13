// Actual execution-domain concurrency semantics. This TU imports NO runtime
// private bridge, source publication, checkpoint world-stop or registry issuer.
#include <atomic>
#include <chrono>
#include <cstddef>
#include <thread>
#include <fast_io.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/utils/thread/execution_domain.h>
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
static void check(bool value, char const* text)
{
    if(!value)
    {
        ::fast_io::io::perrln("execution_quiescent_configuration: ", ::fast_io::mnp::os_c_str(text));
        ::fast_io::fast_terminate();
    }
}
template<typename Ready> static void wait(Ready&& ready)
{
    auto const deadline{::std::chrono::steady_clock::now() + ::std::chrono::seconds{2}};
    while(!ready())
    {
        check(::std::chrono::steady_clock::now() < deadline, "bounded actual thread rendezvous");
        ::std::this_thread::yield();
    }
}
int main()
{
    using domain = ::uwvm2::utils::thread::execution_domain;
    domain actual{4u}; ::std::size_t configurations{};
    check(actual.with_quiescent_configuration([&]() noexcept { ++configurations; }) && configurations == 1u,
        "real idle generation permits one bounded internal configuration");
    auto running{actual.try_enter()}; check(static_cast<bool>(running), "actual admitted execution");
    check(!actual.with_quiescent_configuration([&]() noexcept { ++configurations; }) && configurations == 1u,
        "an admitted guest/setup lease immediately declines without running setter");
    running.reset();
    // Race uses the REAL admission mutex, not a supplied active-count flag.
    // The test-only callback waits for an attempt marker to exercise the lock;
    // production callbacks are strictly bounded setters and must never wait.
    ::std::atomic_bool inside{}, attempted{}, admitted{}, publication{};
    ::std::jthread entrant{[&]() noexcept
    {
        wait([&]() noexcept { return inside.load(::std::memory_order_acquire); });
        attempted.store(true, ::std::memory_order_release);
        auto lease{actual.try_enter()}; check(static_cast<bool>(lease), "real entry follows configure");
        admitted.store(true, ::std::memory_order_release);
        check(publication.load(::std::memory_order_acquire), "entry cannot miss completed profile installation");
    }};
    check(actual.with_quiescent_configuration([&]() noexcept
    {
        inside.store(true, ::std::memory_order_release);
        wait([&]() noexcept { return attempted.load(::std::memory_order_acquire); });
        check(!admitted.load(::std::memory_order_acquire), "racing entry is excluded before internal setter publishes");
        publication.store(true, ::std::memory_order_release); ++configurations;
    }), "actual empty generation configures while racing entry waits");
    entrant.join(); check(configurations == 2u, "one race setter only");
    auto held{actual.try_enter()}; check(static_cast<bool>(held), "actual lease pins generation during reset");
    ::std::atomic_bool reset_done{};
    ::std::jthread resetting{[&]() noexcept
    { actual.reset([&]() noexcept { reset_done.store(true, ::std::memory_order_release); }); }};
    wait([&]() noexcept { return held.stop_requested(); });
    check(!reset_done.load(::std::memory_order_acquire), "real reset waits for its old lease");
    check(!actual.with_quiescent_configuration([&]() noexcept { ++configurations; }) && configurations == 2u,
        "ongoing maintenance declines immediately without waiting on parked execution");
    held.reset(); resetting.join();
    check(reset_done.load(::std::memory_order_acquire), "reset completes only after actual lease release");
    check(actual.with_quiescent_configuration([&]() noexcept { ++configurations; }) && configurations == 3u,
        "fresh actual generation reopens quiescent configuration");
    actual.stop_and_drain();
    check(!actual.with_quiescent_configuration([&]() noexcept { ++configurations; }) && configurations == 3u,
        "closed/stopping generation never runs configuration even after zero active leases");
    ::fast_io::io::println("PASS real quiescent configuration, entry race and maintenance refusal");
}
#else
int main() { return 77; }
#endif
#include <uwvm2/utils/macro/pop_macros.h>
