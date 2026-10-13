// Real domain accounting/transaction unit; NOT a kernel native permission test.
// The external worker gate below models only the owner's synchronous wake seam.
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <fast_io.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/utils/thread/cooperative_pause_domain.h>
#if !defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# error Actual native-thread support required; unavailable is not PASS.
#endif
using domain = ::uwvm2::utils::thread::cooperative_pause_domain;
using result = ::uwvm2::utils::thread::cooperative_pause_result;
using clock_type = ::std::chrono::steady_clock;
static_assert(!::std::is_copy_constructible_v<domain::external_resume_borrow>);
static_assert(!::std::is_move_constructible_v<domain::external_resume_borrow>);
static void require(bool value, char const* message) noexcept
{
    if(!value) { ::fast_io::io::perrln("external resume: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static auto deadline() { return clock_type::now() + ::std::chrono::seconds{10}; }
template<typename Predicate> static void bounded_wait(Predicate&& predicate)
{
    auto const until{deadline()};
    while(!predicate()) { require(clock_type::now() < until, "finite actual participant/host transition wait"); ::std::this_thread::yield(); }
}
struct true_external_participant
{
    domain& actual;
    ::std::atomic<::std::uint_least64_t> id{};
    ::std::atomic_bool poll_now{}, at_external_gate{}, gate_open{}, resumed{}, finish{};
    ::std::thread worker;
    explicit true_external_participant(domain& owner) : actual{owner}, worker{[this]
    {
        auto participant{actual.enter()}; require(bool(participant), "actual lease enrolled");
        id.store(participant.identifier(), ::std::memory_order_release);
        bounded_wait([&] { return poll_now.load(::std::memory_order_acquire); });
        participant.poll({2u,3u,5u,7u});
        at_external_gate.store(true, ::std::memory_order_release);
        bounded_wait([&] { return gate_open.load(::std::memory_order_acquire); });
        resumed.store(true, ::std::memory_order_release);
        // Keep the real running participant enrolled until the competing ALLN
        // mutator checks the actual accounting. Empty-cohort admission cannot
        // replace a running-thread refusal in this test.
        bounded_wait([&] { return finish.load(::std::memory_order_acquire); });
    }} {}
    void park(domain::pause_ticket const& ticket)
    {
        bounded_wait([&] { return id.load(::std::memory_order_acquire) != 0u; });
        poll_now.store(true, ::std::memory_order_release);
        require(actual.wait_until_paused(ticket, deadline()) == result::paused, "genuine cooperative participant parked");
        require(actual.release_one_for_native_step(ticket, id.load(::std::memory_order_acquire)), "actual selected cooperative release");
        bounded_wait([&] { return at_external_gate.load(::std::memory_order_acquire); });
        require(actual.external_park(ticket, id.load(::std::memory_order_acquire), {2u,3u,5u,7u}), "actual manager accounts external park");
    }
    void join()
    {
        gate_open.store(true, ::std::memory_order_release); finish.store(true, ::std::memory_order_release); worker.join();
    }
};
int main()
{
    domain actual{1u}, foreign{1u};
    true_external_participant participant{actual};
    bounded_wait([&] { return participant.id.load(::std::memory_order_acquire) != 0u; });
    auto ticket{actual.request_pause()}, other{foreign.request_pause()};
    require(bool(ticket) && bool(other), "independent genuine pause episodes"); participant.park(ticket);
    auto const id{participant.id.load(::std::memory_order_acquire)};
    unsigned preflight_callbacks{}, false_wakes{}, second_wakes{};
    auto inspect_only{[&](auto location, auto&) noexcept
    { require(location.code_unit == 2u && location.code_generation == 7u, "actual domain supplied location"); ++preflight_callbacks; }};
    require(!actual.with_externally_parked_participant_for_resume(other,id,inspect_only) && preflight_callbacks == 0u,
        "wrong real domain ticket cannot enter callback");
    require(!actual.with_externally_parked_participant_for_resume(ticket,0u,inspect_only) && preflight_callbacks == 0u,
        "invalid participant cannot enter callback");
    require(!actual.with_externally_parked_participant_for_resume(ticket,id,inspect_only) && preflight_callbacks == 1u &&
            actual.capture(ticket).result == result::paused && !participant.resumed.load(::std::memory_order_acquire),
        "true callback return without commit grants no resume and retains ALLN");
    require(!actual.with_externally_parked_participant_for_resume(ticket,id,[&](auto, auto& resume) noexcept
    {
        require(!resume.commit([&]() noexcept { ++false_wakes; return false; }), "real backend false restores actual park");
        require(!resume.commit([&]() noexcept { ++second_wakes; return true; }), "same borrow is one-shot after false");
    }) && false_wakes == 1u && second_wakes == 0u && actual.capture(ticket).result == result::paused &&
        !participant.resumed.load(::std::memory_order_acquire), "failed commit restores both flags/count before unlock");
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    struct preflight_exception {};
    bool threw{};
    try { static_cast<void>(actual.with_externally_parked_participant_for_resume(ticket,id,
        [](auto, auto&) { throw preflight_exception{}; })); }
    catch(preflight_exception const&) { threw = true; }
    require(threw && actual.capture(ticket).result == result::paused && !participant.resumed.load(::std::memory_order_acquire),
        "throw before commit leaves exact original external park and unlocks domain");
#endif
    ::std::mutex publication{}; unsigned generation{3u};
    ::std::atomic_bool start_mutator{}, attempted_mutator{}, completed_mutator{};
    bool mutator_admitted{}, mutator_callback{}, woke_under_actual_generation{};
    ::std::thread mutator{[&]
    {
        bounded_wait([&] { return start_mutator.load(::std::memory_order_acquire); });
        attempted_mutator.store(true, ::std::memory_order_release);
        mutator_admitted = actual.while_stopped(ticket,[&]
        { ::std::lock_guard lock{publication}; mutator_callback = true; generation = 4u; });
        completed_mutator.store(true, ::std::memory_order_release);
    }};
    require(actual.with_externally_parked_participant_for_resume(ticket,id,[&](auto, auto& resume)
    {
        ::std::lock_guard protected_publication{publication};
        require(generation == 3u, "actual publication remains authenticated inside ONE domain");
        start_mutator.store(true, ::std::memory_order_release);
        require(resume.commit([&]() noexcept
        {
            woke_under_actual_generation = generation == 3u;
            participant.gate_open.store(true, ::std::memory_order_release); return true;
        }), "actual single selected wake committed under original publication");
        require(!resume.commit([]() noexcept { return true; }), "committed borrow cannot wake twice");
    }), "leaf return reports its genuine successful backend wake");
    bounded_wait([&] { return attempted_mutator.load(::std::memory_order_acquire) && completed_mutator.load(::std::memory_order_acquire) &&
                            participant.resumed.load(::std::memory_order_acquire); });
    mutator.join();
    require(woke_under_actual_generation && generation == 3u && !mutator_admitted && !mutator_callback,
        "independent ALLN publication cannot cross actual proof/park-accounting/wake transaction");
    require(actual.capture(ticket).result == result::timeout,
        "real selected running participant is no longer reported ALLN stopped");
    require(!actual.with_externally_parked_participant_for_resume(ticket,id,inspect_only),
        "running original participant cannot remint external resume borrow");
    actual.close(); participant.join(); foreign.close();
    ::fast_io::io::println("external_resume_borrow: PASS real domain leases/external accounting, wrong-domain refusal, bounded preflight, rollback, one-shot, concurrent ALLN publication refusal; kernel-native-permission=NOT-QUALIFIED");
}
