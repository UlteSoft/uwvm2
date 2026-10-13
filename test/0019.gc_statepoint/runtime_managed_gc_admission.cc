// Private uncompiled native boundary fixture. It exercises the actual candidate
// admission/phase/state types, not generated LLVM code or VM root sufficiency.
#include <atomic>
#include <chrono>
#include <exception>
#include <latch>
#include <thread>
#include <type_traits>
#include <fast_io.h>
#include <uwvm2/runtime/gc/entry_admission.h>
#include <uwvm2/runtime/gc/instance_phase.h>
#include <uwvm2/runtime/gc/managed_collection.h>

namespace gc = ::uwvm2::runtime::gc;
namespace
{
    unsigned checks{};
    void require(bool ok, char8_t const* name)
    {
        ++checks;
        if(!ok)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"[FAIL] managed GC native boundary: ", ::fast_io::mnp::os_c_str(name));
            ::fast_io::fast_terminate();
        }
    }
    // A timer makes a protocol failure fail rather than hang the runner.
    template<class Predicate>
    bool wait_for(Predicate&& predicate) noexcept
    {
        auto const until{::std::chrono::steady_clock::now() + ::std::chrono::seconds{2}};
        do
        {
            if(predicate()) { return true; }
            ::std::this_thread::yield();
        } while(::std::chrono::steady_clock::now() < until);
        return predicate();
    }
    void enter_and_throw(gc::managed_entry_admission& domain)
    {
        auto lease{domain.enter()};
        require(domain.active_count() == 1uz, u8"native C++ throw has live lease");
        throw 37;
    }
}
int main()
{
    static_assert(!::std::is_copy_constructible_v<gc::managed_entry_admission::shared_lease>);
    static_assert(!::std::is_copy_constructible_v<gc::managed_entry_admission::exclusive_lease>);
    static_assert(::std::is_trivially_copyable_v<gc::instance_collection_phase>);
    gc::managed_entry_admission admission;
    require(admission.active_count() == 0uz, u8"new admission empty");
    {
        auto policy{admission.try_exclusive(0uz)};
        require(bool(policy), u8"cold policy publication owns exclusive zero");
        require(!admission.try_exclusive(0uz), u8"exclusive cannot nest");
    }
    {
        auto owner{admission.enter()};
        auto moved{::std::move(owner)};
        require(!owner && moved && admission.active_count() == 1uz, u8"move preserves exactly one shared owner");
        auto peer{admission.enter()};
        require(admission.active_count() == 2uz, u8"two native owners coexist");
        require(!admission.try_exclusive(1uz), u8"peer execution skips collection without fatal");
        peer.reset();
        auto transaction{admission.try_exclusive(1uz)};
        require(bool(transaction), u8"sole counted owner acquires short collection");
        require(admission.active_count() == 1uz, u8"exclusive retains owner count");
        // Transaction retires before its owning shared lease: reverse C++ LIFO.
    }
    require(admission.active_count() == 0uz, u8"LIFO releases transaction before owner");
    try { enter_and_throw(admission); }
    catch(int value) { require(value == 37, u8"genuine C++ propagation is unchanged"); }
    require(admission.active_count() == 0uz, u8"C++ EH retires cold shared lease");
    for(unsigned iteration{}; iteration != 32u; ++iteration)
    {
        auto owner{admission.enter()};
        auto transaction{admission.try_exclusive(1uz)};
        require(bool(transaction), u8"concurrent fixture begins exclusively");
        ::std::atomic_bool about_to_enter{}, entered{}, release_peer{};
        ::std::jthread peer{[&]
        {
            about_to_enter.store(true, ::std::memory_order_release);
            auto peer_lease{admission.enter()};
            entered.store(true, ::std::memory_order_release);
            while(!release_peer.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
        }};
        require(wait_for([&] { return about_to_enter.load(::std::memory_order_acquire); }), u8"peer reaches actual admission");
        require(!entered.load(::std::memory_order_acquire) && admission.active_count() == 1uz,
            u8"new outer entry cannot publish roots during sweep");
        transaction.reset();
        require(wait_for([&] { return entered.load(::std::memory_order_acquire); }), u8"peer resumes after short transaction");
        require(admission.active_count() == 2uz && !admission.try_exclusive(1uz),
            u8"resumed peer runs normally and blocks only collection");
        release_peer.store(true, ::std::memory_order_release);
        peer.join();
        require(admission.active_count() == 1uz, u8"peer has retired completely before next collection");
    }
    require(admission.active_count() == 0uz, u8"all concurrent owners retired");
    gc::instance_collection_phase phase{};
    auto const first{gc::begin_owned_initializer()};
    require(!phase.initialized_for(first) && !phase.ready_for(first), u8"external/default storage is unqualified");
    phase.publish_initialized(first);
    require(phase.initialized_for(first) && !phase.ready_for(first), u8"initialized storage still needs actual segment phase");
    phase.publish_active_segments(true);
    gc::publish_owned_initializer(first);
    require(phase.ready_for(first) && !phase.ready_for(first + 1u), u8"ready stamp is actual generation-specific");
    phase.publish_active_segments(false);
    require(!phase.ready_for(first), u8"real segment mutation revokes readiness before writing");
    phase.publish_active_segments(true);
    require(!gc::is_cli_gc_execution(), u8"native entry does not implicitly have CLI capability");
    {
        gc::scoped_cli_gc_execution launch;
        require(gc::is_cli_gc_execution(), u8"actual launch scope owns thread-local native capability");
        { gc::scoped_cli_gc_execution nested; require(gc::is_cli_gc_execution(), u8"nested scope retains outer capability"); }
        ::std::atomic_bool peer_is_cli{true};
        ::std::jthread peer{[&] { peer_is_cli.store(gc::is_cli_gc_execution(), ::std::memory_order_release); }};
        peer.join();
        require(!peer_is_cli.load(::std::memory_order_acquire), u8"native peer does not inherit CLI launch token");
    }
    require(!gc::is_cli_gc_execution(), u8"launch capability retired");
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
    gc::managed_collection_state state;
    require(state.request_cli(first), u8"native configure requests roots before code publication");
    state.note_native_escape();
    require(state.disabled() && state.metrics().rejection == gc::managed_gc_rejection::native_roots_unknown,
        u8"unknown host roots disqualify current instance before execution");
    state.reset_after_execution_drain();
    require(state.request_cli(first) && state.disabled(), u8"backend reset cannot revoke retained native handles");
    state.reset_after_execution_drain();
    auto const next{gc::begin_owned_initializer()};
    gc::publish_owned_initializer(next);
    require(next != first && state.request_cli(next) && !state.disabled(), u8"fresh actual initializer creates new eligibility generation");
    state.reset_after_execution_drain();
#else
# error "Native state fixture requires native thread support and genuine C++ EH; unsupported profile must be recorded separately."
#endif
    ::fast_io::io::println(::fast_io::u8out(), u8"[PASS] managed GC native boundary checks=", checks);
}
