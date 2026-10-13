// SOURCE-only finite actual OS-thread/TLS retirement component.
// ROOT keeper alone may compile/run this under its inherited bounded cgroup.
#include <atomic>
#include <chrono>
#include <memory>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/utils/thread/native_thread_join.h>
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && \
    ((defined(__linux__) && defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__BIONIC__)) || (defined(_WIN32) && !defined(__WINE__) && !defined(__CYGWIN__)) || defined(__APPLE__) || \
     (defined(__FreeBSD__) && __FreeBSD_version >= 1501000))
namespace threads = ::uwvm2::utils::thread;
namespace chrono = ::std::chrono;
struct shared_state
{
    ::std::atomic<bool> body_done{}, tls_entered{}, release_tls{}, tls_cleanup_finished{};
};
struct blocked_tls
{
    ::std::shared_ptr<shared_state> state{};
    ~blocked_tls() noexcept
    {
        if(!state) { return; }
        state->tls_entered.store(true, ::std::memory_order_release);
        while(!state->release_tls.load(::std::memory_order_acquire)) { ::fast_io::this_thread::yield(); }
        state->tls_cleanup_finished.store(true, ::std::memory_order_release);
        // This member's shared_ptr destructor still runs AFTER the marker.
        // Only OS try_join proves that this and all other TLS cleanup finished.
    }
};
#if defined(__APPLE__)
// Actual host rights accounting: no port names are printed or exposed to Wasm.
static ::std::size_t live_port_names()
{
    ::mach_port_name_array_t names{};
    ::mach_port_type_array_t types{};
    ::mach_msg_type_number_t names_count{}, types_count{};
    if(::mach_port_names(mach_task_self(), &names, &names_count, &types, &types_count)!=KERN_SUCCESS)
    { ::fast_io::fast_terminate(); }
    auto const count{names_count};
    if(::vm_deallocate(mach_task_self(), reinterpret_cast<::vm_address_t>(names), names_count*sizeof(*names))!=KERN_SUCCESS ||
       ::vm_deallocate(mach_task_self(), reinterpret_cast<::vm_address_t>(types), types_count*sizeof(*types))!=KERN_SUCCESS)
    { ::fast_io::fast_terminate(); }
    return count;
}
#endif
static bool exercise_owners()
{
    ::std::atomic<unsigned> completed{};
    for(unsigned i{};i!=64u;++i)
    {
        ::fast_io::native_thread first{[&]() noexcept { completed.fetch_add(1u); }};
        ::fast_io::native_thread second{[&]() noexcept { completed.fetch_add(1u); }};
        ::fast_io::native_thread moved{::std::move(first)};
        if(first.joinable()) { ::fast_io::fast_terminate(); }
        moved.swap(second);
        second.join(); // Ordinary join must retire the private observer as well.
        auto result{threads::join_native_thread_until(moved, chrono::steady_clock::now()+chrono::seconds{2})};
        if(result.actual.status!=::fast_io::thread_join_status::joined || moved.joinable() || second.joinable())
        { ::fast_io::fast_terminate(); }
    }
    return completed.load()==128u;
}
int main()
{
    auto state{::std::make_shared<shared_state>()};
    ::fast_io::native_thread owner{[state]() noexcept
    {
        thread_local blocked_tls proof{};
        proof.state = state;
        state->body_done.store(true, ::std::memory_order_release);
    }};
    auto const setup_deadline{chrono::steady_clock::now() + chrono::seconds{2}};
    while(!state->tls_entered.load(::std::memory_order_acquire) && chrono::steady_clock::now() < setup_deadline)
    { ::fast_io::this_thread::yield(); }
    bool const setup{state->body_done.load(::std::memory_order_acquire) && state->tls_entered.load(::std::memory_order_acquire)};
    auto const blocked{threads::join_native_thread_until(owner, chrono::steady_clock::now() + chrono::milliseconds{40})};
    bool const retained{owner.joinable() && !state->tls_cleanup_finished.load(::std::memory_order_acquire)};
    state->release_tls.store(true, ::std::memory_order_release); // ALWAYS release actual TLS, including setup failure.
    auto const joined{threads::join_native_thread_until(owner, chrono::steady_clock::now() + chrono::seconds{2})};
    if(joined.actual.status != ::fast_io::thread_join_status::joined)
    {
        ::fast_io::io::perrln("physical join unavailable/failed; owners remain live; no retirement ACK; blocked-status=", static_cast<unsigned>(blocked.actual.status), " blocked-native-error=", blocked.actual.native_error, " joined-status=", static_cast<unsigned>(joined.actual.status), " joined-native-error=", joined.actual.native_error);
        // Failure-only process containment. This is explicitly NOT an owner-
        // retirement ACK; the bounded keeper must record a failed component.
        ::fast_io::fast_terminate(); // keeper records FAILURE; this is not cleanup/retirement
    }
    if(!setup || !retained || !blocked.timed_out || blocked.actual.status != ::fast_io::thread_join_status::pending ||
       owner.joinable() || !state->tls_cleanup_finished.load(::std::memory_order_acquire)) { return 1; }
    if(owner.try_join().status != ::fast_io::thread_join_status::not_joinable) { return 3; }
    if(!exercise_owners()) { return 4; }
#if defined(__APPLE__)
    auto const before{live_port_names()};
    for(unsigned i{};i!=4u;++i) { if(!exercise_owners()) { return 5; } }
    auto const after{live_port_names()};
    // Allow a small libc cache variation; hundreds of leaked observers/rights
    // cannot be hidden by this bound after the warm-up batch.
    if(after>before+4u) { ::fast_io::io::perrln("Mach rights leaked: before=",before," after=",after);return 6; }
    ::fast_io::io::println("physical_native_thread_join: Mach names before=",before," after=",after);
#endif
    ::fast_io::io::println("physical_native_thread_join: body-return/TLS-block pending, released-TLS actual join, moved/swapped/ordinary owners qualified");
}
#else
int main() { ::fast_io::io::perr("native thread unavailable; no qualification\n"); return 77; }
#endif
#include <uwvm2/utils/macro/pop_macros.h>
