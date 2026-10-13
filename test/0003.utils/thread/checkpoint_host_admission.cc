// Standalone gate component semantics, NOT runtime checkpoint qualification.
// This test-only friend implementation has no runtime/source/registry headers,
// execution lease, pause ticket, cohort census or publish/restore capability.
// Never link this TU with the future actual runtime bridge implementation.
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <fast_io.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/utils/thread/checkpoint_host_admission.h>
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
extern "C++" { namespace uwvm2::runtime::lib
{
    class runtime_checkpoint_host_bridge final
    {
        using gate = ::uwvm2::utils::thread::checkpoint_host_admission;
    public:
        using operation = gate::host_operation;
        using closed = gate::closed_admission;
        static ::std::unique_ptr<gate> make_component() { return ::std::unique_ptr<gate>{new gate}; }
        static auto enter(gate& domain, operation& out) noexcept { return domain.try_enter(out); }
        static auto close(gate& domain, closed& out) noexcept { return domain.try_close(out); }
        static bool current(gate const& domain, closed const& out) noexcept { return domain.current(out); }
        static void escape(gate& domain) noexcept { domain.record_untracked_host(); }
        static void set_next_for_component(gate& domain, ::std::uint64_t value) noexcept
        { ::std::lock_guard lock{domain.state_->mutex}; domain.state_->next_serial = value; }
    };
} }
namespace
{
    using bridge = ::uwvm2::runtime::lib::runtime_checkpoint_host_bridge;
    using gate = ::uwvm2::utils::thread::checkpoint_host_admission;
    using status = ::uwvm2::utils::thread::checkpoint_host_admission_status;
    void require(bool value, unsigned line)
    { if(!value) { ::fast_io::io::perr("checkpoint host gate component assertion at ", line, "\n"); ::fast_io::fast_terminate(); } }
# define REQUIRE(...) require(static_cast<bool>(__VA_ARGS__), __LINE__)
}
static_assert(!::std::is_default_constructible_v<gate>);
static_assert(!::std::is_copy_constructible_v<gate> && !::std::is_move_constructible_v<gate>);
static_assert(!::std::is_constructible_v<bridge::closed, bool>);
static_assert(!::std::is_constructible_v<bridge::closed, ::std::uint64_t>);
static_assert(!::std::is_copy_constructible_v<bridge::operation> && !::std::is_copy_constructible_v<bridge::closed>);
int main()
{
    auto domain{bridge::make_component()};
    bridge::closed stopped{}; bridge::operation first{}, second{};
    REQUIRE(!stopped && !first && domain->observe().active_operations == 0u);
    REQUIRE(bridge::enter(*domain, first) == status::ok);
    REQUIRE(bridge::enter(*domain, second) == status::ok);
    REQUIRE(domain->observe().active_operations == 2u);
    REQUIRE(bridge::close(*domain, stopped) == status::busy && !stopped);
    bridge::operation moved{::std::move(first)};
    REQUIRE(!first && moved && domain->observe().active_operations == 2u);
    REQUIRE(bridge::enter(*domain, moved) == status::invalid_context);
    second = {}; REQUIRE(bridge::close(*domain, stopped) == status::busy);
    moved = {}; REQUIRE(bridge::close(*domain, stopped) == status::ok);
    REQUIRE(stopped && bridge::current(*domain, stopped));
    REQUIRE(bridge::enter(*domain, first) == status::admission_closed && !first);
    bridge::closed extra{}; REQUIRE(bridge::close(*domain, extra) == status::admission_closed);
    REQUIRE(bridge::close(*domain, stopped) == status::invalid_context);
    auto foreign{bridge::make_component()}; REQUIRE(!bridge::current(*foreign, stopped));
    bridge::closed owned{::std::move(stopped)}; REQUIRE(!stopped && owned && bridge::current(*domain, owned));
    owned = {}; REQUIRE(!domain->observe().admission_closed);
    REQUIRE(bridge::enter(*domain, first) == status::ok); first = {};

    // Actual mutex race: an admitted host setup stays live while a manager tries
    // to close. No wait for that operation occurs in close (same reentry shape).
    ::std::mutex coordination; ::std::condition_variable changed;
    bool entered{}, release{};
    ::std::thread worker{[&]
    {
        bridge::operation host_setup{}; REQUIRE(bridge::enter(*domain, host_setup) == status::ok);
        ::std::unique_lock lock{coordination}; entered = true; changed.notify_all();
        changed.wait(lock, [&] { return release; });
    }};
    { ::std::unique_lock lock{coordination}; changed.wait(lock, [&] { return entered; }); }
    REQUIRE(bridge::close(*domain, stopped) == status::busy && !stopped);
    { ::std::lock_guard lock{coordination}; release = true; changed.notify_all(); }
    worker.join(); REQUIRE(bridge::close(*domain, stopped) == status::ok);
    bridge::escape(*domain); REQUIRE(!stopped && !bridge::current(*domain, stopped));
    stopped = {}; REQUIRE(!domain->observe().admission_closed && domain->observe().untracked_host);
    REQUIRE(bridge::close(*domain, stopped) == status::untracked_host && !stopped);
    // Ineligibility cannot change the native program's ordinary host-call policy.
    REQUIRE(bridge::enter(*domain, first) == status::ok); first = {};

    // Last serial is usable once; no guard or later closure resurrects serial 1.
    auto last{bridge::make_component()};
    bridge::set_next_for_component(*last, (::std::numeric_limits<::std::uint64_t>::max)());
    REQUIRE(bridge::close(*last, stopped) == status::ok && stopped);
    stopped = {}; REQUIRE(bridge::close(*last, stopped) == status::serial_exhausted && !stopped);
    REQUIRE(bridge::enter(*last, first) == status::ok); first = {};

    // A retained state pin is safe to retire after its immovable gate owner.
    // This deliberately tests ONLY gate lifetime, not VM-resource retirement.
    auto retired{bridge::make_component()};
    REQUIRE(bridge::enter(*retired, first) == status::ok); retired.reset(); first = {};
    retired = bridge::make_component(); REQUIRE(bridge::close(*retired, stopped) == status::ok);
    retired.reset(); REQUIRE(!stopped); stopped = {};
    ::fast_io::io::print("checkpoint host admission component semantics passed; no runtime issuer\n");
}
#else
int main() { return 77; } // Unsupported native-thread provider is not a pass.
#endif
#include <uwvm2/utils/macro/pop_macros.h>
