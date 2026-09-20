#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/object/memory/linear/mmap.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>

#if defined(UWVM_SUPPORT_MMAP) && defined(UWVM_CPP_EXCEPTIONS) && defined(UWVM_TEST) && !defined(_WIN32) && !defined(__CYGWIN__) && !defined(__wasi__)
#include <csignal>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
    namespace memory = ::uwvm2::object::memory::linear;

    ::std::size_t injected_platform_page_size{};
    ::std::size_t injected_call_count{};
    bool injected_rollback_failure{};
    int injected_marker_fd{-1};

    [[noreturn]] void throw_injected_mprotect_failure()
    {
        throw ::fast_io::error{::fast_io::posix_domain_value, static_cast<::fast_io::error::value_type>(ENOMEM)};
    }

    void record_call(char marker)
    {
        if(injected_marker_fd == -1) { return; }
        if(::write(injected_marker_fd, &marker, 1u) != 1) { ::_exit(110); }
    }

    void inject_partial_mprotect_failure(void* address, ::std::size_t length, int protection)
    {
        ++injected_call_count;
        if(injected_call_count == 1u)
        {
            if(protection != (PROT_READ | PROT_WRITE) || length < injected_platform_page_size) { ::_exit(111); }

            // Model the weakest behavior allowed by POSIX: a writable prefix followed by an error result.
            ::fast_io::details::sys_mprotect(address, injected_platform_page_size, PROT_READ | PROT_WRITE);
            *static_cast<volatile unsigned char*>(address) = 0x5au;
            record_call('G');
            throw_injected_mprotect_failure();
        }
        if(injected_call_count == 2u)
        {
            if(protection != PROT_NONE) { ::_exit(112); }
            record_call('R');
            if(injected_rollback_failure) { throw_injected_mprotect_failure(); }
            ::fast_io::details::sys_mprotect(address, length, PROT_NONE);
            return;
        }
        ::_exit(113);
    }

    void configure_injection(::std::size_t platform_page_size, bool rollback_failure, int marker_fd = -1)
    {
        injected_platform_page_size = platform_page_size;
        injected_call_count = 0u;
        injected_rollback_failure = rollback_failure;
        injected_marker_fd = marker_fd;
        memory::details::strict_grow_mprotect_test_hook = inject_partial_mprotect_failure;
    }

    void clear_injection() noexcept
    {
        memory::details::strict_grow_mprotect_test_hook = nullptr;
        injected_marker_fd = -1;
    }

    [[nodiscard]] bool is_fast_termination(int status) noexcept
    {
        if(!WIFSIGNALED(status)) { return false; }
        auto const signal{WTERMSIG(status)};
        return signal == SIGILL || signal == SIGABRT || signal == SIGTRAP;
    }

    [[nodiscard]] int expect_termination(::std::size_t custom_page_size,
                                         ::std::size_t initial_page_count,
                                         ::std::size_t grow_page_count,
                                         ::std::size_t platform_page_size,
                                         bool rollback_failure,
                                         ::std::array<char, 2u> expected_markers,
                                         ::std::size_t expected_marker_count)
    {
        memory::mmap_memory_t mem{custom_page_size, memory::mmap_memory_status_t::wasm32};
        mem.init_by_page_count(initial_page_count);

        int marker_pipe[2]{};
        if(::pipe(marker_pipe) != 0) { return 20; }
        auto const child{::fork()};
        if(child < 0)
        {
            ::close(marker_pipe[0]);
            ::close(marker_pipe[1]);
            return 21;
        }
        if(child == 0)
        {
            ::close(marker_pipe[0]);
            configure_injection(platform_page_size, rollback_failure, marker_pipe[1]);
            static_cast<void>(mem.grow_strictly(grow_page_count));
            ::_exit(114);
        }

        ::close(marker_pipe[1]);
        int status{};
        if(::waitpid(child, &status, 0) != child)
        {
            ::close(marker_pipe[0]);
            return 22;
        }
        ::std::array<char, 3u> actual_markers{};
        auto const marker_count{::read(marker_pipe[0], actual_markers.data(), actual_markers.size())};
        ::close(marker_pipe[0]);
        if(marker_count != static_cast<ssize_t>(expected_marker_count)) { return 23; }
        for(::std::size_t index{}; index != expected_marker_count; ++index)
        {
            if(actual_markers[index] != expected_markers[index]) { return 24; }
        }
        return is_fast_termination(status) ? 0 : 25;
    }

    void on_expected_memory_fault(::uwvm2::object::memory::error::mmap_memory_error_t const&) noexcept { ::_exit(0); }

    [[nodiscard]] bool protected_page_rejects_write(memory::mmap_memory_t const& mem, ::std::size_t offset)
    {
        auto const child{::fork()};
        if(child < 0) { return false; }
        if(child == 0)
        {
            ::uwvm2::object::memory::signal::detail::mmap_memory_out_of_bounds_func = on_expected_memory_fault;
            auto* const address{reinterpret_cast<volatile unsigned char*>(reinterpret_cast<::std::uintptr_t>(mem.memory_begin) + offset)};
            *address = 0xa5u;
            ::_exit(115);
        }
        int status{};
        return ::waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    [[nodiscard]] int check_recoverable_rollback(::std::size_t platform_page_size)
    {
        memory::mmap_memory_t mem{1u, memory::mmap_memory_status_t::wasm32};
        mem.init_by_page_count(platform_page_size);
        configure_injection(platform_page_size, false);
        auto const grew{mem.grow_strictly(platform_page_size * 3u)};
        clear_injection();
        if(grew || injected_call_count != 2u) { return 30; }
        if(mem.memory_length_p->load(::std::memory_order_acquire) != platform_page_size) { return 31; }
        return protected_page_rejects_write(mem, platform_page_size) ? 0 : 32;
    }
}
#endif

int main()
{
#if defined(UWVM_SUPPORT_MMAP) && defined(UWVM_CPP_EXCEPTIONS) && defined(UWVM_TEST) && !defined(_WIN32) && !defined(__CYGWIN__) && !defined(__wasi__)
    auto const [page_size, page_ok]{::uwvm2::object::memory::platform_page::get_platform_page_size()};
    if(!page_ok || page_size == 0u) { return 1; }
    if(page_size > ::std::numeric_limits<::std::size_t>::max() / 3u) { return 2; }

    // Guard-backed accesses can race with the partially writable prefix, so even a possible rollback is not recoverable.
    auto result{expect_termination(page_size, 1u, 3u, page_size, false, {'G', 0}, 1u)};
    if(result != 0) { return result; }

    // Dynamically checked accesses may roll back, but a failed rollback must still terminate.
    result = expect_termination(1u, page_size, page_size * 3u, page_size, true, {'G', 'R'}, 2u);
    if(result != 0) { return result; }

    // A successful rollback remains the recoverable strict-growth path and must not publish the requested length.
    return check_recoverable_rollback(page_size);
#else
    // The regression requires the test-only POSIX mprotect interception point and C++ exception recovery. Non-exception
    // builds terminate inside fast_io at the first host protection failure; Windows uses different commit APIs.
    return 0;
#endif
}

#include <uwvm2/utils/macro/pop_macros.h>
