#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/object/memory/linear/mmap.h>

#include <cstdint>

#if defined(UWVM_SUPPORT_MMAP) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__)
#include <csignal>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

int main()
{
#if defined(UWVM_SUPPORT_MMAP) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__)
    namespace memory = ::uwvm2::object::memory::linear;
    auto const [page_size, page_ok]{::uwvm2::object::memory::platform_page::get_platform_page_size()};
    if(!page_ok || page_size == 0u) { return 1; }
    memory::mmap_memory_t mem{page_size, memory::mmap_memory_status_t::wasm32};
    mem.init_by_page_count(1u);

    auto const child{::fork()};
    if(child < 0) { return 2; }
    if(child == 0)
    {
        // Fault injection is restricted to this child's own reservation. A hole in the middle of the grow range makes
        // mprotect(PROT_READ|PROT_WRITE) fail, and also makes the subsequent PROT_NONE rollback fail. Linux may have
        // already changed protection on a prefix before returning ENOMEM; the runtime must not continue in that state.
        auto const hole_address{reinterpret_cast<void*>(reinterpret_cast<::std::uintptr_t>(mem.memory_begin) + page_size * 2u)};
        if(::munmap(hole_address, page_size) != 0) { ::_exit(103); }
        static_cast<void>(mem.grow_strictly(3u));
        ::_exit(104);  // Returning, even with false, violates the fail-closed protection invariant.
    }
    int status{};
    if(::waitpid(child, &status, 0) != child) { return 3; }
    if(!WIFSIGNALED(status)) { return 4; }
    auto const signal{WTERMSIG(status)};
    return signal == SIGILL || signal == SIGABRT || signal == SIGTRAP ? 0 : 5;
#else
    // The regression specifically exercises the recoverable POSIX-error/rollback branch. Non-exception builds already
    // terminate at the first host protection failure, and other platforms use different commit APIs.
    return 0;
#endif
}

#include <uwvm2/utils/macro/pop_macros.h>
