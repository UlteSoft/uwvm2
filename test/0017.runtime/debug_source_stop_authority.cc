#include <uwvm2/utils/thread/cooperative_pause_domain.h>
#include <fast_io.h>
#include <atomic>
#include <chrono>
#include <latch>
#include <thread>
namespace thread = uwvm2::utils::thread;
using domain = thread::cooperative_pause_domain;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln("debug_source_stop_authority: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static auto deadline() { return ::std::chrono::steady_clock::now() + ::std::chrono::seconds{10}; }
int main()
{
    domain control{1u}, other{1u};
    ::std::latch entered{1u}, begin{1u}, released{1u}, finish{1u};
    ::std::atomic<::std::uint_least64_t> id{};
    ::std::thread guest{[&]
    {
        auto participant{control.enter()}; check(static_cast<bool>(participant), "real participant admission");
        id.store(participant.identifier()); entered.count_down(); begin.wait();
        participant.poll({11u, 22u, 33u, 44u}); released.count_down(); finish.wait();
    }};
    entered.wait(); auto ticket{control.request_pause()}; check(static_cast<bool>(ticket), "real ticket");
    bool called{};
    check(!control.with_stopped_participant(ticket, id.load(), [&](auto) { called = true; }) && !called, "request alone is not stopped");
    begin.count_down(); check(control.wait_until_paused(ticket, deadline()) == thread::cooperative_pause_result::paused, "actual park");
    check(control.with_stopped_participant(ticket, id.load(), [&](auto actual)
    { called = true; check(actual == thread::cooperative_pause_location{11u, 22u, 33u, 44u}, "location comes from actual slot"); }), "actual slot query");
    check(called, "actual callback");
    called = false;
    check(!control.with_stopped_participant(ticket, 0u, [&](auto) { called = true; }), "zero participant rejected");
    check(!control.with_stopped_participant(ticket, id.load() + 1u, [&](auto) { called = true; }), "unknown participant rejected");
    check(!other.with_stopped_participant(ticket, id.load(), [&](auto) { called = true; }), "foreign domain ticket rejected");
    check(!called, "rejections do not invoke callback");
    // Resume cannot change the actual location during the one guarded callback.
    ::std::latch resume_attempted{1u}; ::std::atomic<bool> resumed{}; ::std::thread manager;
    check(control.with_stopped_participant(ticket, id.load(), [&](auto)
    {
        manager = ::std::thread{[&] { resume_attempted.count_down(); check(control.resume(ticket), "resume after guard"); resumed.store(true); }};
        resume_attempted.wait(); check(!resumed.load(), "one guard excludes concurrent resume");
    }), "guarded query");
    manager.join(); released.wait(); called = false;
    check(!control.with_stopped_participant(ticket, id.load(), [&](auto) { called = true; }) && !called, "resumed ticket unusable");
    // Simulate ONLY domain accounting for a host-authenticated native park. This
    // is not a claim that a kernel SIGTRAP/native instruction was exercised.
    auto native_ticket{control.request_pause()};
    check(control.external_park(native_ticket, id.load(), {11u, 22u, 34u, 44u}), "external accounting park");
    check(control.wait_until_paused(native_ticket, deadline()) == thread::cooperative_pause_result::paused, "external set accounted stopped");
    check(!control.with_stopped_participant(native_ticket, id.load(), [&](auto) { called = true; }) && !called, "native park cannot authorize old Wasm position");
    control.close();
    check(!control.with_stopped_participant(native_ticket, id.load(), [&](auto) { called = true; }) && !called, "closed domain rejects old ticket");
    finish.count_down(); guest.join(); control.drain();
    ::fast_io::io::println("debug_source_stop_authority: PASS");
}
