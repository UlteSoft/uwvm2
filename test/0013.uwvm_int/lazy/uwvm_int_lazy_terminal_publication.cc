#include "uwvm_int_lazy_common.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace
{
    using namespace ::uwvm2test::uwvm_int_lazy;
    namespace thread_utils = ::uwvm2::utils::thread;
    using state_t = thread_utils::lazy_compile_unit_state;

#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
    inline constexpr ::std::size_t mirrored_compile_unit_count{1uz};
#else
    // The interpreter supports multiple structural compile units for one whole-function materialization.
    inline constexpr ::std::size_t mirrored_compile_unit_count{3uz};
#endif

    struct publication_fixture
    {
        lazy_module_t storage{};
        ::std::vector<state_t*> observed_units{};
    };

    inline void initialize_publication_fixture(publication_fixture& fixture)
    {
        fixture.storage.functions.resize(1uz);
        fixture.storage.compile_units.resize(mirrored_compile_unit_count);

        auto& fn{fixture.storage.functions.index_unchecked(0uz)};
        fn.primary_cu_index = 0uz;
#if !defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
        fn.first_cu_index = 0uz;
        fn.cu_count = mirrored_compile_unit_count;
#endif

        fixture.observed_units.reserve(mirrored_compile_unit_count + 1uz);
        for(::std::size_t i{}; i != mirrored_compile_unit_count; ++i)
        {
            auto& unit{fixture.storage.compile_units.index_unchecked(i).state};
            unit.state.store(lazy_compile_state_t::compiling, ::std::memory_order_relaxed);
            fixture.observed_units.push_back(::std::addressof(unit));
        }
        fn.materialization_state.state.store(lazy_compile_state_t::compiling, ::std::memory_order_relaxed);
        fixture.observed_units.push_back(::std::addressof(fn.materialization_state));
    }

    [[nodiscard]] int test_terminal_publication_sequence() noexcept
    {
        publication_fixture fixture{};
        initialize_publication_fixture(fixture);
        auto& fn{fixture.storage.functions.index_unchecked(0uz)};

        ::std::array<state_t*, mirrored_compile_unit_count + 1uz> notification_order{};
        ::std::size_t notification_count{};
        bool notification_overflow{};
        bool stored_before_notification{true};
        auto record_notification{[&](state_t& unit) noexcept
                                 {
                                     if(unit.state.load(::std::memory_order_acquire) != lazy_compile_state_t::failed)
                                     {
                                         stored_before_notification = false;
                                     }
                                     if(notification_count < notification_order.size())
                                     {
                                         notification_order[notification_count] = ::std::addressof(unit);
                                     }
                                     else
                                     {
                                         notification_overflow = true;
                                     }
                                     ++notification_count;
                                 }};

        // The injected notifier makes the publication contract deterministic: every mirror must be stored and passed to the
        // notifier in compile-unit order, followed by the authoritative function state.  Production uses the same path with the
        // default notifier, whose only operation is lazy_compile_notify_unit().
        lazy::details::mark_function_compile_units_state(
            fixture.storage, fn, lazy_compile_state_t::failed, record_notification);

        UWVM2TEST_REQUIRE(!notification_overflow);
        UWVM2TEST_REQUIRE(notification_count == notification_order.size());
        UWVM2TEST_REQUIRE(stored_before_notification);
        for(::std::size_t i{}; i != notification_order.size(); ++i)
        {
            UWVM2TEST_REQUIRE(notification_order[i] == fixture.observed_units[i]);
        }
        return 0;
    }

    [[nodiscard]] int test_terminal_native_notify_smoke() noexcept
    {
        publication_fixture fixture{};
        initialize_publication_fixture(fixture);
        auto& fn{fixture.storage.functions.index_unchecked(0uz)};

        thread_utils::lazy_compile_scheduler waiter{};
        ::std::atomic_size_t entered_wait{};
        ::std::atomic_size_t completed_wait{};
        ::std::vector<::std::thread> waiters{};
        waiters.reserve(fixture.observed_units.size());

        for(::std::size_t i{}; i != fixture.observed_units.size(); ++i)
        {
            auto* const unit{fixture.observed_units[i]};
            waiters.emplace_back([&, unit]() noexcept
                                 {
                                     entered_wait.fetch_add(1uz, ::std::memory_order_release);
                                     waiter.wait_for_unit_event(*unit, lazy_compile_state_t::compiling);
                                     completed_wait.fetch_add(1uz, ::std::memory_order_release);
                                 });
        }

        auto const entry_deadline{::std::chrono::steady_clock::now() + ::std::chrono::seconds{5}};
        while(entered_wait.load(::std::memory_order_acquire) != fixture.observed_units.size() &&
              ::std::chrono::steady_clock::now() < entry_deadline)
        {
            ::std::this_thread::yield();
        }
        auto const all_waiters_entered{entered_wait.load(::std::memory_order_acquire) == fixture.observed_units.size()};

        // The production wait API has no waiter-registration probe.  The entry handshake plus a bounded scheduling handoff makes
        // every waiter reach atomic::wait before publication on platforms that provide it, while the rescue path below makes failure
        // finite.  Polling fallbacks run the same terminal-completion smoke without claiming to test an OS wake primitive.
        if(all_waiters_entered) { ::std::this_thread::sleep_for(::std::chrono::milliseconds{50}); }
        lazy::details::mark_function_compile_units_state(fixture.storage, fn, lazy_compile_state_t::failed);

        auto const notify_deadline{::std::chrono::steady_clock::now() + ::std::chrono::seconds{2}};
        while(completed_wait.load(::std::memory_order_acquire) != fixture.observed_units.size() &&
              ::std::chrono::steady_clock::now() < notify_deadline)
        {
            ::std::this_thread::yield();
        }
        auto const helper_woke_every_waiter{completed_wait.load(::std::memory_order_acquire) == fixture.observed_units.size()};

        if(!helper_woke_every_waiter)
        {
            // A store-without-notify regression must fail this assertion, but explicit rescue notifications keep join deterministic.
            for(auto* const unit: fixture.observed_units) { thread_utils::lazy_compile_notify_unit(*unit); }
        }
        for(auto& thread: waiters) { thread.join(); }

        UWVM2TEST_REQUIRE(all_waiters_entered);
        UWVM2TEST_REQUIRE(helper_woke_every_waiter);
        UWVM2TEST_REQUIRE(fn.materialization_state.state.load(::std::memory_order_acquire) == lazy_compile_state_t::failed);
        for(::std::size_t i{}; i != mirrored_compile_unit_count; ++i)
        {
            UWVM2TEST_REQUIRE(fixture.storage.compile_units.index_unchecked(i).state.state.load(::std::memory_order_acquire) ==
                              lazy_compile_state_t::failed);
        }
        return 0;
    }
}  // namespace

int main()
{
    if(auto const result{test_terminal_publication_sequence()}; result != 0) { return result; }
    return test_terminal_native_notify_smoke();
}
