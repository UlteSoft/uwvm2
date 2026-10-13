// Real cold execution-domain maintenance ownership and actual drain, no VM
// permission mocks. Intended only for the guarded Linux/QEMU native keeper.
#include <uwvm2/utils/thread/execution_domain.h>
#include <fast_io.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
using domain = ::uwvm2::utils::thread::execution_domain;
static void require(bool value, char const* reason)
{
    if(!value) { ::fast_io::io::perrln("execution maintenance transition: ", ::fast_io::mnp::os_c_str(reason)); ::fast_io::fast_terminate(); }
}
int main()
{
    domain actual{2u}, foreign{1u};
    auto maintenance{actual.try_maintenance_transition()}; require(bool(maintenance), "real actual maintenance mutex retained");
    domain::maintenance_transition empty{};
    bool foreign_called{};
    require(!foreign.reset_owned_transition(maintenance, [&] { foreign_called = true; }) && !foreign_called,
        "actual owner mismatch declines before admission mutation or callback");
    auto foreign_lease{foreign.try_enter()}; require(bool(foreign_lease), "wrong owner refusal preserves other actual admission"); foreign_lease.reset();
    std::mutex mutex; std::condition_variable changed;
    bool admitted{}, permit_exit{}, worker_left{}, reset_attempting{}, other_reset_done{}, first_drain{};
    std::thread worker{[&]
    {
        auto lease{actual.try_enter()}; require(bool(lease), "actual entry lease admitted under independent admission mutex");
        {
            std::unique_lock lock{mutex}; admitted = true; changed.notify_all();
            require(changed.wait_until(lock, std::chrono::steady_clock::now() + std::chrono::seconds{10}, [&] { return permit_exit; }),
                "finite real worker release signal");
        }
        require(lease.stop_requested(), "actual generation stop published before release");
        lease.reset();
        { std::lock_guard lock{mutex}; worker_left = true; changed.notify_all(); }
    }};
    {
        std::unique_lock lock{mutex};
        require(changed.wait_until(lock, std::chrono::steady_clock::now() + std::chrono::seconds{10}, [&] { return admitted; }),
            "finite actual admitted worker");
    }
    std::thread external_reset{[&]
    {
        { std::lock_guard lock{mutex}; reset_attempting = true; changed.notify_all(); }
        actual.reset([&]
        {
            std::lock_guard lock{mutex}; require(first_drain, "external reset cannot steal original old-lease drain interval");
            other_reset_done = true; changed.notify_all();
        });
    }};
    {
        std::unique_lock lock{mutex};
        require(changed.wait_until(lock, std::chrono::steady_clock::now() + std::chrono::seconds{10}, [&] { return reset_attempting; }),
            "actual external reset attempts same maintenance mutex");
    }
    actual.request_stop(); require(!actual.try_enter(), "real stop closes admission while retained worker lives");
    { std::lock_guard lock{mutex}; permit_exit = true; changed.notify_all(); }
    require(actual.reset_owned_transition(maintenance, [&]
    {
        // Actual drain may precede the worker's final test bookkeeping, but its
        // leased generation is already gone. Joining here proves the entire
        // native test worker lifetime before checking its separate marker.
        worker.join(); std::lock_guard lock{mutex};
        require(worker_left && !other_reset_done, "actual old lease drained and competing resource reset excluded");
        first_drain = true;
    }), "same actual maintenance token drains original execution and resets admission");
    auto fresh{actual.try_enter()}; require(bool(fresh) && !fresh.stop_requested(), "actual fresh execution admission opened after drain"); fresh.reset();
    maintenance = domain::maintenance_transition{}; // actual mutex unlock; no borrowing owner survives transfer
    external_reset.join();
    require(other_reset_done, "competing real reset runs only after actual transition releases maintenance");
    require(!actual.reset_owned_transition(empty, [] { ::fast_io::fast_terminate(); }), "empty holder cannot request drain");
    ::fast_io::io::println("execution_maintenance_transition: PASS actual maintenance exclusion and old-generation drain");
}
#else
int main() { return 77; }
#endif
