// rwlock_bench.cc
//
// Performance comparison between uwvm2::utils::mutex::rwlock_t and folly::RWSpinLock style implementation.
// - Uses rwlock_t / rw_fair_shared_guard_t / rw_fair_unique_guard_t from the project
// - Embeds a renamed bench_folly::RWSpinLock implementation with folly dependencies removed

#include <uwvm2/utils/mutex/impl.h>
#include <uwvm2/utils/debug/timer.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

// ====================== Aliases: Using custom RW lock implementation ======================

namespace mylock
{
    using rwlock_t = ::uwvm2::utils::mutex::rwlock_t;

    using rw_shared_guard_t = ::uwvm2::utils::mutex::rw_shared_guard_t;
    using rw_unique_guard_t = ::uwvm2::utils::mutex::rw_unique_guard_t;

    using rw_writer_pref_shared_guard_t = ::uwvm2::utils::mutex::rw_writer_pref_shared_guard_t;
    using rw_writer_pref_unique_guard_t = ::uwvm2::utils::mutex::rw_writer_pref_unique_guard_t;

    using rw_writer_pref_no_starvation_shared_guard_t = ::uwvm2::utils::mutex::rw_writer_pref_no_starvation_shared_guard_t;
    using rw_writer_pref_no_starvation_unique_guard_t = ::uwvm2::utils::mutex::rw_writer_pref_no_starvation_unique_guard_t;

    using rw_fair_shared_guard_t = ::uwvm2::utils::mutex::rw_fair_shared_guard_t;
    using rw_fair_unique_guard_t = ::uwvm2::utils::mutex::rw_fair_unique_guard_t;

    using rw_fair_writer_pref_shared_guard_t = ::uwvm2::utils::mutex::rw_fair_writer_pref_shared_guard_t;
    using rw_fair_writer_pref_unique_guard_t = ::uwvm2::utils::mutex::rw_fair_writer_pref_unique_guard_t;
}  // namespace mylock

// =================== Simplified folly::RWSpinLock port ====================
// Based on the provided code, with folly header files and LIKELY macro dependencies removed.
// Note: Namespace changed to bench_folly to avoid conflicts with actual folly library.

namespace bench_folly
{

    class RWSpinLock
    {
        enum : std::int32_t
        {
            READER = 4,
            UPGRADED = 2,
            WRITER = 1
        };

    public:
        constexpr RWSpinLock() : bits_(0) {}

        RWSpinLock(RWSpinLock const&) = delete;
        RWSpinLock& operator= (RWSpinLock const&) = delete;

        // Exclusive lock
        void lock()
        {
            std::uint_fast32_t count{};
            while(!try_lock())
            {
                if(++count > 1000u) { std::this_thread::yield(); }
            }
        }

        void unlock()
        {
            static_assert(READER > WRITER + UPGRADED, "wrong bits!");
            bits_.fetch_and(~(WRITER | UPGRADED), std::memory_order_release);
        }

        // Shared lock
        void lock_shared()
        {
            std::uint_fast32_t count{};
            while(!try_lock_shared())
            {
                if(++count > 1000u) { std::this_thread::yield(); }
            }
        }

        void unlock_shared() { bits_.fetch_add(-READER, std::memory_order_release); }

        // Downgrade from write to read
        void unlock_and_lock_shared()
        {
            bits_.fetch_add(READER, std::memory_order_acquire);
            unlock();
        }

        // Upgrade lock
        void lock_upgrade()
        {
            std::uint_fast32_t count{};
            while(!try_lock_upgrade())
            {
                if(++count > 1000u) { std::this_thread::yield(); }
            }
        }

        void unlock_upgrade() { bits_.fetch_add(-UPGRADED, std::memory_order_acq_rel); }

        void unlock_upgrade_and_lock()
        {
            std::int64_t count{};
            while(!try_unlock_upgrade_and_lock())
            {
                if(++count > 1000) { std::this_thread::yield(); }
            }
        }

        void unlock_upgrade_and_lock_shared() { bits_.fetch_add(READER - UPGRADED, std::memory_order_acq_rel); }

        void unlock_and_lock_upgrade()
        {
            bits_.fetch_or(UPGRADED, std::memory_order_acquire);
            bits_.fetch_add(-WRITER, std::memory_order_release);
        }

        // try APIs
        bool try_lock()
        {
            std::int32_t expect{0};
            return bits_.compare_exchange_strong(expect, WRITER, std::memory_order_acq_rel);
        }

        bool try_lock_shared()
        {
            std::int32_t value{bits_.fetch_add(READER, std::memory_order_acquire)};
            if((value & (WRITER | UPGRADED)) != 0)
            {
                bits_.fetch_add(-READER, std::memory_order_release);
                return false;
            }
            return true;
        }

        bool try_unlock_upgrade_and_lock()
        {
            std::int32_t expect{UPGRADED};
            return bits_.compare_exchange_strong(expect, WRITER, std::memory_order_acq_rel);
        }

        bool try_lock_upgrade()
        {
            std::int32_t value{bits_.fetch_or(UPGRADED, std::memory_order_acquire)};
            return ((value & (UPGRADED | WRITER)) == 0);
        }

        std::int32_t bits() const { return bits_.load(std::memory_order_acquire); }

    private:
        std::atomic<std::int32_t> bits_{};
    };

}  // namespace bench_folly

// =========================== Benchmark utilities ============================

// ------------------------- Single-threaded benchmark: read/write -------------------------

void bench_single_thread_mylock(std::size_t iters)
{
    mylock::rwlock_t lock{};

    using uwvm2::utils::debug::timer;

    {
        timer t{u8"mylock single-thread read"};
        for(std::size_t i{}; i < iters; ++i) { mylock::rw_fair_shared_guard_t g{lock}; }
    }

    {
        timer t{u8"mylock single-thread write"};
        for(std::size_t i{}; i < iters; ++i) { mylock::rw_fair_unique_guard_t g{lock}; }
    }
}

void bench_single_thread_folly(std::size_t iters)
{
    bench_folly::RWSpinLock lock{};

    using uwvm2::utils::debug::timer;

    {
        timer t{u8"folly single-thread read"};
        for(std::size_t i{}; i < iters; ++i)
        {
            lock.lock_shared();
            lock.unlock_shared();
        }
    }

    {
        timer t{u8"folly single-thread write"};
        for(std::size_t i{}; i < iters; ++i)
        {
            lock.lock();
            lock.unlock();
        }
    }
}

// -------------------- Multi-threaded contention benchmark: configurable read/write ratio --------

template <typename Lock, typename ReadOp, typename WriteOp>
void bench_contention(char const* name,
                      std::size_t thread_count,
                      std::size_t iters_per_thread,
                      unsigned read_ratio_x10,  // 0..10 => 0%..100% reads
                      ReadOp read_op,
                      WriteOp write_op)
{
    Lock lock{};

    std::atomic<bool> start_flag{false};

    auto worker = [&](unsigned)
    {
        while(!start_flag.load(std::memory_order_acquire)) { /* spin */ }

        for(std::size_t i{}; i < iters_per_thread; ++i)
        {
            auto const bucket{static_cast<unsigned>(i % 10u)};
            if(bucket < read_ratio_x10) { read_op(lock); }
            else
            {
                write_op(lock);
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for(std::size_t i{}; i < thread_count; ++i) { threads.emplace_back(worker, static_cast<unsigned>(i)); }
    start_flag.store(true, std::memory_order_release);
    for(auto& t: threads) { t.join(); }

    double const read_ratio{static_cast<double>(read_ratio_x10) / 10.0};
    std::printf("[%s] contention: threads=%zu, iters/thread=%zu, read_ratio=%.1f, write_ratio=%.1f\n",
                name,
                thread_count,
                iters_per_thread,
                read_ratio,
                1.0 - read_ratio);
}

int main()
{
    constexpr std::size_t single_iters{1000000};

    // Single-threaded benchmarks
    bench_single_thread_mylock(single_iters);
    bench_single_thread_folly(single_iters);

    // Multi-threaded contention: thread count and iterations can be adjusted as needed
    constexpr std::size_t iters_per_thread{200000};
    constexpr std::size_t thread_count{8};

    using uwvm2::utils::debug::timer;

    for(unsigned read_ratio_x10 = 0; read_ratio_x10 <= 10; ++read_ratio_x10)
    {
        double const rf{static_cast<double>(read_ratio_x10) / 10.0};

        // Reader-preferred (non-fair) variant
        {
            static constexpr ::uwvm2::utils::container::u8string_view labels[] = {
                u8"mylock rw_shared/rw_unique r=0.0",
                u8"mylock rw_shared/rw_unique r=0.1",
                u8"mylock rw_shared/rw_unique r=0.2",
                u8"mylock rw_shared/rw_unique r=0.3",
                u8"mylock rw_shared/rw_unique r=0.4",
                u8"mylock rw_shared/rw_unique r=0.5",
                u8"mylock rw_shared/rw_unique r=0.6",
                u8"mylock rw_shared/rw_unique r=0.7",
                u8"mylock rw_shared/rw_unique r=0.8",
                u8"mylock rw_shared/rw_unique r=0.9",
                u8"mylock rw_shared/rw_unique r=1.0",
            };
            timer t{labels[read_ratio_x10]};
            bench_contention<mylock::rwlock_t>(
                "mylock.read_pref",
                thread_count,
                iters_per_thread,
                read_ratio_x10,
                [](mylock::rwlock_t& lk) { mylock::rw_shared_guard_t g{lk}; },
                [](mylock::rwlock_t& lk) { mylock::rw_unique_guard_t g{lk}; });
        }

        // Writer-preferred (non-fair) variant
        {
            static constexpr ::uwvm2::utils::container::u8string_view labels[] = {
                u8"mylock rw_writer_pref r=0.0",
                u8"mylock rw_writer_pref r=0.1",
                u8"mylock rw_writer_pref r=0.2",
                u8"mylock rw_writer_pref r=0.3",
                u8"mylock rw_writer_pref r=0.4",
                u8"mylock rw_writer_pref r=0.5",
                u8"mylock rw_writer_pref r=0.6",
                u8"mylock rw_writer_pref r=0.7",
                u8"mylock rw_writer_pref r=0.8",
                u8"mylock rw_writer_pref r=0.9",
                u8"mylock rw_writer_pref r=1.0",
            };
            timer t{labels[read_ratio_x10]};
            bench_contention<mylock::rwlock_t>(
                "mylock.writer_pref",
                thread_count,
                iters_per_thread,
                read_ratio_x10,
                [](mylock::rwlock_t& lk) { mylock::rw_writer_pref_shared_guard_t g{lk}; },
                [](mylock::rwlock_t& lk) { mylock::rw_writer_pref_unique_guard_t g{lk}; });
        }

        // Writer-preferred, writer-starvation-free variant
        {
            static constexpr ::uwvm2::utils::container::u8string_view labels[] = {
                u8"mylock rw_writer_pref_no_starvation r=0.0",
                u8"mylock rw_writer_pref_no_starvation r=0.1",
                u8"mylock rw_writer_pref_no_starvation r=0.2",
                u8"mylock rw_writer_pref_no_starvation r=0.3",
                u8"mylock rw_writer_pref_no_starvation r=0.4",
                u8"mylock rw_writer_pref_no_starvation r=0.5",
                u8"mylock rw_writer_pref_no_starvation r=0.6",
                u8"mylock rw_writer_pref_no_starvation r=0.7",
                u8"mylock rw_writer_pref_no_starvation r=0.8",
                u8"mylock rw_writer_pref_no_starvation r=0.9",
                u8"mylock rw_writer_pref_no_starvation r=1.0",
            };
            timer t{labels[read_ratio_x10]};
            bench_contention<mylock::rwlock_t>(
                "mylock.writer_pref_no_starvation",
                thread_count,
                iters_per_thread,
                read_ratio_x10,
                [](mylock::rwlock_t& lk) { mylock::rw_writer_pref_no_starvation_shared_guard_t g{lk}; },
                [](mylock::rwlock_t& lk) { mylock::rw_writer_pref_no_starvation_unique_guard_t g{lk}; });
        }

        // Fair writer-preferred (phase-fair) variant
        {
            static constexpr ::uwvm2::utils::container::u8string_view labels[] = {
                u8"mylock rw_fair r=0.0",
                u8"mylock rw_fair r=0.1",
                u8"mylock rw_fair r=0.2",
                u8"mylock rw_fair r=0.3",
                u8"mylock rw_fair r=0.4",
                u8"mylock rw_fair r=0.5",
                u8"mylock rw_fair r=0.6",
                u8"mylock rw_fair r=0.7",
                u8"mylock rw_fair r=0.8",
                u8"mylock rw_fair r=0.9",
                u8"mylock rw_fair r=1.0",
            };
            timer t{labels[read_ratio_x10]};
            bench_contention<mylock::rwlock_t>(
                "mylock.fair",
                thread_count,
                iters_per_thread,
                read_ratio_x10,
                [](mylock::rwlock_t& lk) { mylock::rw_fair_shared_guard_t g{lk}; },
                [](mylock::rwlock_t& lk) { mylock::rw_fair_unique_guard_t g{lk}; });
        }

        // Fair, writer-biased variant
        {
            static constexpr ::uwvm2::utils::container::u8string_view labels[] = {
                u8"mylock rw_fair_writer_pref r=0.0",
                u8"mylock rw_fair_writer_pref r=0.1",
                u8"mylock rw_fair_writer_pref r=0.2",
                u8"mylock rw_fair_writer_pref r=0.3",
                u8"mylock rw_fair_writer_pref r=0.4",
                u8"mylock rw_fair_writer_pref r=0.5",
                u8"mylock rw_fair_writer_pref r=0.6",
                u8"mylock rw_fair_writer_pref r=0.7",
                u8"mylock rw_fair_writer_pref r=0.8",
                u8"mylock rw_fair_writer_pref r=0.9",
                u8"mylock rw_fair_writer_pref r=1.0",
            };
            timer t{labels[read_ratio_x10]};
            bench_contention<mylock::rwlock_t>(
                "mylock.fair_writer_pref",
                thread_count,
                iters_per_thread,
                read_ratio_x10,
                [](mylock::rwlock_t& lk) { mylock::rw_fair_writer_pref_shared_guard_t g{lk}; },
                [](mylock::rwlock_t& lk) { mylock::rw_fair_writer_pref_unique_guard_t g{lk}; });
        }

        // Folly-style baseline
        {
            static constexpr ::uwvm2::utils::container::u8string_view labels[] = {
                u8"folly RWSpinLock r=0.0",
                u8"folly RWSpinLock r=0.1",
                u8"folly RWSpinLock r=0.2",
                u8"folly RWSpinLock r=0.3",
                u8"folly RWSpinLock r=0.4",
                u8"folly RWSpinLock r=0.5",
                u8"folly RWSpinLock r=0.6",
                u8"folly RWSpinLock r=0.7",
                u8"folly RWSpinLock r=0.8",
                u8"folly RWSpinLock r=0.9",
                u8"folly RWSpinLock r=1.0",
            };
            timer t{labels[read_ratio_x10]};
            bench_contention<bench_folly::RWSpinLock>(
                "folly",
                thread_count,
                iters_per_thread,
                read_ratio_x10,
                [](bench_folly::RWSpinLock& lk)
                {
                    lk.lock_shared();
                    lk.unlock_shared();
                },
                [](bench_folly::RWSpinLock& lk)
                {
                    lk.lock();
                    lk.unlock();
                });
        }

        (void)rf;  // suppress unused warning if timers are disabled
    }

    return 0;
}

// From project root:
// clang++   benchmark/0001.utils/0001.mutex/RWSpinLock.cc -o benchmark/0001.utils/0001.mutex/RWSpinLock -std=c++2c -O3 -ffast-math -march=native -fno-rtti
// -fno-unwind-tables -fno-asynchronous-unwind-tables -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -I
// third-parties/boost_unordered/include g++       benchmark/0001.utils/0001.mutex/RWSpinLock.cc -o benchmark/0001.utils/0001.mutex/RWSpinLock -std=c++2c -O3
// -ffast-math -march=native -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables -I src -I third-parties/fast_io/include -I
// third-parties/bizwen/include -I third-parties/boost_unordered/include
