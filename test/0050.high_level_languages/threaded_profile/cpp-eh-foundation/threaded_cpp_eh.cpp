// SOURCE ONLY. Build/run only on SSH Linux in the existing 64GiB cgroup.
// This is a real C++ std::thread/TLS/exception foundation, not an rclcpp pass.
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <thread>
#include <wasi/api.h>

#if !defined(__wasm32__) || !defined(__wasm_atomics__)
# error "The actual wasm32 WASIp1-threads producer is required"
#endif
#if !defined(__cpp_exceptions)
# error "Build the Guest and actual C++ libraries with standard Wasm exceptions"
#endif

namespace
{
    constexpr unsigned count{2u};
    std::atomic<unsigned> arrived{}, release_workers{}, caught{}, unwound{}, tls_destroyed{}, completed{};
    std::atomic<unsigned> writer_error{};
    std::atomic<std::uint64_t> answers[count]{};

    bool write_literal(char const* bytes, std::size_t size) noexcept
    {
        __wasi_ciovec_t iov{reinterpret_cast<std::uint8_t const*>(bytes), size};
        __wasi_size_t written{};
        auto const status{__wasi_fd_write(1u, &iov, 1u, &written)};
        if(status != 0u || written != size)
        {
            writer_error.store(1u, std::memory_order_release);
            return false;
        }
        return true;
    }

    template<std::size_t N>
    bool marker(char const (&text)[N]) noexcept { return write_literal(text, N - 1u); }

    struct UnwindWitness
    {
        ~UnwindWitness() { unwound.fetch_add(1u, std::memory_order_acq_rel); }
    };

    struct ThreadLocalWitness
    {
        unsigned owner{};
        explicit ThreadLocalWitness(unsigned selected) noexcept : owner{selected} {}
        ~ThreadLocalWitness() { tls_destroyed.fetch_add(1u, std::memory_order_acq_rel); }
    };

    [[noreturn, gnu::noinline]] void throw_from_other_frame(unsigned index)
    {
        UnwindWitness scope;
        if(index == 0u) { throw std::runtime_error("thread-zero"); }
        throw std::logic_error("thread-one");
    }

    void worker(unsigned index)
    {
        // A distinct instance/TLS base is required for each real Guest worker.
        // Reusing another thread's native/linear TLS makes the owner check fail.
        thread_local ThreadLocalWitness local{index + 1u};
        arrived.fetch_add(1u, std::memory_order_acq_rel);
        arrived.notify_all();
        while(release_workers.load(std::memory_order_acquire) == 0u)
        {
            release_workers.wait(0u, std::memory_order_acquire);
        }
        if(local.owner != index + 1u)
        {
            writer_error.store(1u, std::memory_order_release);
            return;
        }
        bool matched{};
        try { throw_from_other_frame(index); }
        catch(std::runtime_error const& error)
        {
            matched = index == 0u && std::strcmp(error.what(), "thread-zero") == 0;
        }
        catch(std::logic_error const& error)
        {
            matched = index == 1u && std::strcmp(error.what(), "thread-one") == 0;
        }
        catch(...) { matched = false; }
        if(!matched)
        {
            writer_error.store(1u, std::memory_order_release);
            return;
        }
        caught.fetch_add(1u, std::memory_order_acq_rel);
        answers[index].store(UINT64_C(0x100000000) + index + 1u, std::memory_order_release);
        completed.fetch_add(1u, std::memory_order_acq_rel);
    }
}

int main()
{
    if(!marker("UWVM_CPP_THREADS_EH_STARTED\n")) { return 21; }
    try
    {
        std::thread first{worker, 0u};
        std::thread second;
        try { second = std::thread{worker, 1u}; }
        catch(...)
        {
            release_workers.store(1u, std::memory_order_release);
            release_workers.notify_all();
            first.join();
            throw;
        }
        for(;;)
        {
            auto const seen{arrived.load(std::memory_order_acquire)};
            if(seen == count) { break; }
            arrived.wait(seen, std::memory_order_acquire);
        }
        if(!marker("UWVM_CPP_THREADS_EH_CHILDREN_ENTERED=2\n"))
        {
            release_workers.store(1u, std::memory_order_release);
            release_workers.notify_all();
            first.join(); second.join();
            return 22;
        }
        release_workers.store(1u, std::memory_order_release);
        release_workers.notify_all();
        first.join(); second.join();
        // C++ Guest TLS destructors, not Host pthread destructor telemetry.
        if(writer_error.load(std::memory_order_acquire) != 0u ||
           caught.load(std::memory_order_acquire) != count ||
           unwound.load(std::memory_order_acquire) != count ||
           tls_destroyed.load(std::memory_order_acquire) != count ||
           completed.load(std::memory_order_acquire) != count ||
           answers[0].load(std::memory_order_acquire) != UINT64_C(0x100000001) ||
           answers[1].load(std::memory_order_acquire) != UINT64_C(0x100000002))
        { return 23; }
    }
    catch(...)
    {
        (void)marker("UWVM_CPP_THREADS_EH_FAILURE\n");
        return 24;
    }
    return marker("UWVM_CPP_THREADS_EH_PASS children=2 caught=2 unwound=2 guest_tls_dtors=2\n") ? 0 : 25;
}
