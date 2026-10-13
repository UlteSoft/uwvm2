// Real participant parking and management transfer; no copied roster or token
// can substitute for the private domain's actual current cooperative episode.
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>
#include <fast_io.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/utils/thread/cooperative_pause_domain.h>
#if !defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# error This fixture requires actual native-thread support; never report a skipped run as PASS.
#endif
#if !defined(__cpp_exceptions) && !defined(__EXCEPTIONS) && !defined(_CPPUNWIND)
# error This fixture requires actual C++ exceptions to exercise unwinding; never skip that case as PASS.
#endif
using domain = ::uwvm2::utils::thread::cooperative_pause_domain;
using result = ::uwvm2::utils::thread::cooperative_pause_result;
static void require(bool value, char const* message)
{
    if(!value)
    {
        ::fast_io::io::perrln("pause transition: ", ::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }
}
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{10}; }
struct preflight_exception {};
struct unwind_observer
{
    bool& unwound;
    ~unwind_observer() { unwound = true; }
};
struct parked_group
{
    domain& owner;
    ::std::mutex mutex{};
    ::std::condition_variable changed{};
    unsigned ready{};
    bool poll_now{};
    ::std::array<::std::uint_least64_t, 2u> ids{};
    ::std::atomic_uint left{};
    ::std::array<::std::thread, 2u> guests{};
    explicit parked_group(domain& actual) : owner{actual}
    {
        for(unsigned index{}; index != guests.size(); ++index)
        {
            guests[index] = ::std::thread{[this, index]
            {
                auto participant{owner.enter()}; require(bool(participant), "real participant admitted");
                {
                    ::std::unique_lock lock{mutex};
                    ids[index] = participant.identifier(); ++ready; changed.notify_all();
                    require(changed.wait_until(lock, deadline(), [&] { return poll_now; }), "finite poll dispatch");
                }
                participant.poll({2u, 3u + index, 5u, 7u});
                {
                    ::std::lock_guard lock{mutex};
                    left.fetch_add(1u, ::std::memory_order_release); changed.notify_all();
                }
            }};
        }
        ::std::unique_lock lock{mutex};
        require(changed.wait_until(lock, deadline(), [&] { return ready == guests.size(); }), "whole actual roster admitted");
    }
    void dispatch()
    {
        ::std::lock_guard lock{mutex}; poll_now = true; changed.notify_all();
    }
    void join()
    {
        {
            ::std::unique_lock lock{mutex};
            require(changed.wait_until(lock, deadline(), [&] { return left.load(::std::memory_order_acquire) == guests.size(); }),
                "finite wait for every original poll to return");
        }
        for(auto& guest : guests) { guest.join(); }
    }
};
int main()
{
    domain actual{2u}, foreign{2u};
    ::std::array<domain::stopped_participant, 2u> scratch{};
    domain::paused_transition transition{};
    {
        parked_group group{actual};
        auto ticket{actual.request_pause()}; require(bool(ticket), "original ticket requested");
        group.dispatch(); require(actual.wait_until_paused(ticket, deadline()) == result::paused, "both real guests cooperatively parked");
        bool refused_callback{};
        require(!actual.transfer_cooperatively_stopped_cohort(ticket, scratch, transition,
            [&](auto const participants, auto const& current)
            {
                require(participants.size() == 2u && current(ticket), "actual complete roster and current episode");
                refused_callback = true; return false;
            }) && refused_callback && !transition, "failed preflight never mints a transition");
        bool threw{}, unwound{};
        try
        {
            static_cast<void>(actual.transfer_cooperatively_stopped_cohort(ticket, scratch, transition,
                [&](auto const participants, auto const& current) -> bool
                {
                    require(participants.size() == 2u && current(ticket), "throwing preflight still owns the actual whole roster");
                    unwind_observer observer{unwound};
                    throw preflight_exception{};
                }));
        }
        catch(preflight_exception const&) { threw = true; }
        require(threw && unwound && !transition && actual.pause_requested(),
            "throw unwinds synchronous resources without a transition or guest resume");
        require(actual.capture(ticket).result == result::paused && group.left.load(::std::memory_order_acquire) == 0u,
            "throw keeps both original participants and original ticket parked");
        bool after_throw{};
        require(actual.while_stopped(ticket, [&] { after_throw = true; }) && after_throw,
            "throw releases the domain mutex and leaves no hidden management hold");
        ::std::array<domain::stopped_participant, 1u> short_scratch{};
        bool short_callback{};
        require(!actual.transfer_cooperatively_stopped_cohort(ticket, short_scratch, transition,
            [&](auto, auto const&) { short_callback = true; return true; }) && !short_callback,
            "short scratch rejected before callback or partial-copy authority");
        require(actual.transfer_cooperatively_stopped_cohort(ticket, scratch, transition,
            [&](auto const participants, auto const& current)
            {
                require(participants.size() == 2u && current(ticket), "same authentic retained episode");
                for(auto const& member : participants)
                {
                    require(member.id == group.ids[0u] || member.id == group.ids[1u], "actual enrolled identity");
                    require(member.location.code_unit == 2u && member.location.code_generation == 7u,
                        "actual copied cooperative location");
                }
                return true;
            }) && transition, "successful coherent preparation seals the pause before unlock");
        // Use a real competing manager and the NON-transfer cohort API: its
        // callback is allowed to mutate host state when admitted, so a held
        // transition must decline it before calling even a bool-returning body.
        ::std::mutex competitor_mutex{}; ::std::condition_variable competitor_changed{};
        bool competitor_finished{}, competitor_admitted{}, competitor_callback{};
        ::std::uint_least64_t protected_state{0x1234u};
        ::std::thread competitor{[&]
        {
            ::std::array<domain::stopped_participant, 2u> competitor_scratch{};
            competitor_admitted = actual.with_cooperatively_stopped_cohort(ticket, competitor_scratch,
                [&](auto participants, auto const& current) -> bool
                {
                    competitor_callback = true;
                    require(participants.size() == 2u && current(ticket), "competing mutator requires a complete current cohort");
                    protected_state = 0x5678u;
                    return true;
                });
            {
                ::std::lock_guard lock{competitor_mutex};
                competitor_finished = true; competitor_changed.notify_all();
            }
        }};
        {
            ::std::unique_lock lock{competitor_mutex};
            require(competitor_changed.wait_until(lock, deadline(), [&] { return competitor_finished; }),
                "finite competing nontransfer cohort callback dispatch");
        }
        competitor.join();
        require(!competitor_admitted && !competitor_callback && protected_state == 0x1234u,
            "retained transition rejects a real competing nontransfer mutator before any state change");
        require(!actual.request_pause(), "held transition never rotates the original pause serial");
        require(!actual.resume(ticket), "copied original ticket cannot wake an owned transition");
        require(!actual.release_one_for_native_step(ticket, group.ids[0u]), "native-step release blocked before original poll wake");
        require(!actual.external_unpark(ticket, group.ids[0u]), "no external resume can bypass the transfer");
        bool committed{};
        require(!actual.while_stopped(ticket, [&] { committed = true; }) && !committed,
            "unrelated mutating stopped transaction cannot invalidate the prepared state");
        require(!foreign.resume_transition(transition) && transition, "wrong domain cannot consume the private lease");
        auto moved{::std::move(transition)};
        require(moved && !transition, "move preserves one real management lease");
        require(group.left.load(::std::memory_order_acquire) == 0u, "neither guest resumed through denied management operations");
        moved.reset(); require(!moved && actual.pause_requested(), "cancel only drops management ownership; original pause retained");
        require(actual.resume(ticket), "original ticket works again after cancellation");
        group.join(); require(group.left.load(::std::memory_order_acquire) == 2u, "both original native participants retired normally");
    }
    {
        parked_group group{actual}; auto ticket{actual.request_pause()}; require(bool(ticket), "fresh episode issued");
        group.dispatch(); require(actual.wait_until_paused(ticket, deadline()) == result::paused, "fresh genuine park");
        require(actual.transfer_cooperatively_stopped_cohort(ticket, scratch, transition,
            [](auto const participants, auto const& current) { (void)current; return participants.size() == 2u; }),
            "fresh complete episode transfer");
        require(actual.resume_transition(transition) && !transition, "exact lease explicitly resumes its own cohort");
        require(!actual.resume_transition(transition) && !actual.resume(ticket), "consumed transition and stale ticket cannot resume again");
        group.join(); require(group.left.load(::std::memory_order_acquire) == 2u, "explicit transition release executed each poll exactly once");
    }
    {
        parked_group group{actual}; auto ticket{actual.request_pause()}; require(bool(ticket), "shutdown episode issued");
        group.dispatch(); require(actual.wait_until_paused(ticket, deadline()) == result::paused, "shutdown genuine park");
        require(actual.transfer_cooperatively_stopped_cohort(ticket, scratch, transition,
            [](auto participants, auto const&) { return participants.size() == 2u; }), "shutdown retained transition");
        actual.close(); require(!actual.resume_transition(transition) && transition,
            "closed domain invalidates execution resumption but retains actual lifetime lease");
        // Closing is not permission to resume this saved cohort. Keep the
        // actual transition alive while a real manager tries to drain; both
        // original native polls must remain blocked until retirement is armed
        // and this retained hold is explicitly released by its owner.
        require(group.left.load(::std::memory_order_acquire) == 0u,
            "close leaves both saved native polls parked while their transition is retained");
        ::std::mutex drain_mutex{}; ::std::condition_variable drain_changed{};
        bool drain_attempted{}, drain_returned{};
        ::std::thread drainer{[&]
        {
            {
                ::std::lock_guard lock{drain_mutex}; drain_attempted = true; drain_changed.notify_all();
            }
            actual.drain();
            {
                ::std::lock_guard lock{drain_mutex}; drain_returned = true; drain_changed.notify_all();
            }
        }};
        {
            ::std::unique_lock lock{drain_mutex};
            require(drain_changed.wait_until(lock, deadline(), [&] { return drain_attempted; }),
                "finite concurrent drain dispatch");
            auto const observation_deadline{::std::chrono::steady_clock::now() + ::std::chrono::milliseconds{100}};
            require(!drain_changed.wait_until(lock, observation_deadline, [&] { return drain_returned; }),
                "concurrent close drain stays blocked while the actual transition and original cohort are retained");
        }
        require(transition && group.left.load(::std::memory_order_acquire) == 0u,
            "closed hold never lets either saved guest run during the bounded drain observation");
        transition.reset();
        group.join();
        require(group.left.load(::std::memory_order_acquire) == 2u,
            "dropping the closed transition wakes each original native poll exactly once");
        {
            ::std::unique_lock lock{drain_mutex};
            require(drain_changed.wait_until(lock, deadline(), [&] { return drain_returned; }),
                "dropping the actual transition wakes the concurrently blocked drain");
        }
        drainer.join();
        require(!actual.enter(), "close never reopens admission through transition cancellation");
    }
    ::fast_io::io::println("paused_transition: PASS actual cohort, denied concurrent nontransfer mutation/resume/step/write, cancellation, throwing preflight, exact release and close-held drain shutdown");
}
