#pragma once
// Source-only private profile types. No ordinary memory policy/state is changed.
#include <cstddef>
#include <cstdint>
#include <exception>
#include <stop_token>
#if defined(__linux__)
# include <cerrno>
# include <pthread.h>
# include <sched.h>
# include <sys/mman.h>
#endif
namespace uwvm2::object::memory::linear
{
    // Stable phase DATA consumed by ChildLease::report_memory_failure. These
    // numbers are phase identifiers, NOT errno or a new Wasm instruction result.
    enum class threaded_grow_status : ::std::uint32_t
    {success=0u,limit=1u,cancelled=2u,invalid=3u,commit_failed=4u,rollback_failed=5u};
    namespace details
    {
#if defined(__linux__)
# if defined(UWVM_TEST)
        // Test SOURCE only: a Host fixture may perform a REAL partial mprotect
        // then inject its errno. This is absent from production code.
        using threaded_grow_mprotect_test_hook_t=int(*)(void*,::std::size_t,int) noexcept;
        inline threaded_grow_mprotect_test_hook_t threaded_grow_mprotect_test_hook{};
# endif
        [[nodiscard]] inline int threaded_profile_mprotect(void* begin,::std::size_t bytes,int flags) noexcept
        {
# if defined(UWVM_TEST)
            if(threaded_grow_mprotect_test_hook!=nullptr)return threaded_grow_mprotect_test_hook(begin,bytes,flags);
# endif
            return ::mprotect(begin,bytes,flags);
        }
#endif
    }
    struct threaded_grow_result final
    {
        threaded_grow_status status{threaded_grow_status::invalid};
        ::std::size_t old_pages{},old_length{},attempted_length{};
        int os_error{},rollback_error{};
        bool tainted{};
    };
}
