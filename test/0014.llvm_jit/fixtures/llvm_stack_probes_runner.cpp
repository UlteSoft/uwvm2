// Cross-target fixture; link with llvm_jit_stack_probes.cc's emitted object.
#include <uwvm2/runtime/lib/uwvm_runtime_native_stack_guard.h>
#include <cstdio>
#include <cstdlib>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
extern "C" void large_frame();
extern "C" void small_frame();
static bool boundary_entry;
extern "C" [[gnu::noinline]] void consume_stack(void* space)
{
    if(boundary_entry) { _exit(93); } // The prologue skipped the protected page.
    // Keep the whole alloca observable and stop the native C++ compiler from
    // turning the callback into a sibling call.
    asm volatile("" : : "r"(space) : "memory");
    large_frame();
    asm volatile("" : : "r"(space) : "memory");
}
#if defined(__riscv) || defined(__aarch64__)
// Deliberately put the entry SP immediately above a single protected page,
// with writable memory below it. Ordinary recursive tests can miss this
// alignment-dependent gap between a split CSR save and the first probe.
extern "C" [[gnu::naked, noreturn]] void enter_near_guard(void*, void (*)())
{
#if defined(__riscv)
    asm volatile("mv sp, a0\njalr a1\nunimp");
#else
    asm volatile("mov sp, x0\nblr x1\nbrk #0");
#endif
}
static bool boundary_case(unsigned offset)
{
    auto const pid{fork()};
    if(pid < 0) { return false; }
    if(pid == 0)
    {
        namespace ns = uwvm2::runtime::lib::native_stack;
        ns::scope guard;
        if(!guard.ready()) { _exit(91); }
        auto const page{static_cast<std::size_t>(sysconf(_SC_PAGESIZE))};
        auto* area{static_cast<unsigned char*>(mmap(nullptr, 1024u * 1024u, PROT_READ | PROT_WRITE,
                                                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0))};
        if(area == MAP_FAILED || mprotect(area + 512u * 1024u, page, PROT_NONE)) { _exit(94); }
        auto const low{reinterpret_cast<std::uintptr_t>(area + 512u * 1024u + page)};
        ns::bounds const bounds{low, reinterpret_cast<std::uintptr_t>(area + 1024u * 1024u), page};
        // Test-only synthetic bounds: production entry does not accept a
        // manually switched stack. The alternate signal stack stays native.
        ns::active_bounds.store(&bounds, std::memory_order_release);
        boundary_entry = true;
        enter_near_guard(reinterpret_cast<void*>(low + offset), large_frame);
    }
    int status{};
    if(waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 127)
    { std::fprintf(stderr, "LLVM guard-boundary failed: offset=%u status=%d\n", offset, status); return false; }
    return true;
}
#endif
int main(int argc, char** argv)
{
    struct rlimit no_core{};
    if(setrlimit(RLIMIT_CORE, &no_core) != 0 || argc > 2) { return 2; }
    auto const frame{argc == 2 ? static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10)) : 20000u};
    if(frame == 0 || frame > 131072u) { return 2; }
    auto const pid{fork()};
    if(pid < 0) { return 1; }
    if(pid == 0)
    {
        uwvm2::runtime::lib::native_stack::scope guard;
        if(!guard.ready()) { _exit(91); }
        small_frame();
        large_frame();
        _exit(92);
    }
    int status{};
    if(waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 127)
    { std::fprintf(stderr, "LLVM stack probes failed: status=%d\n", status); return 1; }
    std::printf("LLVM-generated %u-byte stack frame: guard caught\n", frame);
#if defined(__riscv) || defined(__aarch64__)
    auto const page{static_cast<unsigned>(sysconf(_SC_PAGESIZE))};
    auto const aligned{(frame + 15u) & ~15u};
    auto const limit{aligned < page ? aligned : page};
    for(unsigned offset{16}; offset <= limit; offset += 16)
    { if(!boundary_case(offset)) { return 1; } }
    std::printf("LLVM guard-boundary: all %u entry alignments caught\n", limit / 16u);
#endif
}
