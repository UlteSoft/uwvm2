// Native-stack faults must be diagnosed without consuming the exhausted stack.
#include <uwvm2/runtime/lib/uwvm_runtime_native_stack_guard.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cerrno>

namespace ns = uwvm2::runtime::lib::native_stack;
static_assert(ns::is_guard_fault(0x10000, 0x20000, 4096, 0xffff, 0xffff));
static_assert(ns::is_guard_fault(0x10000, 0x20000, 4096, 0xffff, 0x18000)); // probe ahead
static_assert(ns::is_guard_fault(0x10000, 0x20000, 4096, 0xffff, 0xec00)); // probe above SP
static_assert(!ns::is_guard_fault(0x10000, 0x20000, 4096, 0, 0));
static_assert(ns::is_guard_fault(0x10000, 0x20000, 4096, 0xefff, 0xefff)); // large frame skipped first guard page
static_assert(!ns::is_guard_fault(0x10000, 0x20000, 4096, 0xefff, 0x8000)); // unrelated lower fault
static_assert(!ns::is_guard_fault(0x10000, 0x20000, 4096, 0xefff, 0x18000)); // SP still in stack
static_assert(!ns::is_guard_fault(0x10000, 0x20000, 4096, 0xffff, 0x20000));
static_assert(!ns::is_guard_fault(0, 0x20000, 4096, 0, 0));

#if (defined(__linux__) || defined(__APPLE__)) && !defined(_WIN32)
#include <sys/resource.h>
#include <sys/wait.h>
#include <sched.h>
#include "../posix_noexcept_abi.h"
namespace posix = ::uwvm2test::posix_abi;
#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
using recurse_t = unsigned (*)(unsigned);
static unsigned small(unsigned);
static unsigned large(unsigned);
static recurse_t volatile small_call{small}, large_call{large};
[[gnu::noinline]] static unsigned small(unsigned n)
{
    volatile unsigned local{n};
    auto value{small_call(n + 1u)};
    return value + local;
}
[[gnu::noinline]] static unsigned large(unsigned n)
{
    volatile unsigned char local[10000];
    local[n % sizeof(local)] = static_cast<unsigned char>(n);
    auto value{large_call(n + 1u)};
    return value + local[n % sizeof(local)];
}
static void previous_handler(int) { posix::_exit_noexcept(73); }

static void unexpected_fault(int, siginfo_t* info, void* context)
{
    // Test-only evidence, not the production signal reporting path.
    auto const* b{ns::active_bounds.load()};
    char text[256]{};
    int const size{std::snprintf(text, sizeof(text), "unexpected fault addr=%zx sp=%zx low=%zx page=%zu\n",
        reinterpret_cast<std::uintptr_t>(info->si_addr), ns::fault_stack_pointer(context), b == nullptr ? 0u : b->low,
        b == nullptr ? 0u : b->page)};
    if(size > 0 && static_cast<std::size_t>(size) < sizeof(text))
    { [[maybe_unused]] auto const written{posix::write_noexcept(2, text, static_cast<std::size_t>(size))}; }
    posix::_exit_noexcept(90);
}

static void* in_thread(void* argument)
{
    struct sigaction action{};
    action.sa_sigaction = unexpected_fault;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    posix::sigemptyset_noexcept(&action.sa_mask);
    posix::sigaction_noexcept(SIGSEGV, &action, nullptr);
    if(argument == nullptr)
    {
        // Small-frame exhaustion exercises a warmed cache; large frames still
        // exercise first-entry setup. Both must fault on the alternate stack.
        for(unsigned i{}; i != 8; ++i) { ns::scope warm; if(!warm.ready()) { posix::_exit_noexcept(91); } }
    }
    ns::scope guard;
    if(!guard.ready()) { posix::_exit_noexcept(91); }
    if(argument == nullptr) { static_cast<void>(small_call(1)); }
    else { static_cast<void>(large_call(1)); }
    posix::_exit_noexcept(92);
}

static bool concurrent_first_entries();

static int child_case(int which)
{
    if(which == 6) { return concurrent_first_entries() ? 0 : 98; }
    if(which == 5 || which == 7)
    {
        // First guard in this fork: exhaust TSD keys and require the original
        // uncached allocation path to remain fully usable.
        pthread_key_t keys[4096];
        unsigned count{};
        while(count != 4096 && posix::pthread_key_create_noexcept(&keys[count], nullptr) == 0) { ++count; }
        bool ready{};
        if(count != 4096)
        {
            ns::scope guard;
            ready = guard.ready() && ns::reusable_alternate == nullptr;
            if(ready && which == 7) { static_cast<void>(large_call(1)); }
        }
        while(count != 0) { posix::pthread_key_delete_noexcept(keys[--count]); }
        return ready ? 0 : 97;
    }
    if(which == 0)
    {
        struct sigaction action{};
        action.sa_handler = previous_handler;
        posix::sigemptyset_noexcept(&action.sa_mask);
        posix::sigaction_noexcept(SIGSEGV, &action, nullptr);
        ns::scope guard;
        if(!guard.ready()) { return 91; }
        posix::raise_noexcept(SIGSEGV); // User-raised signals are not native stack faults.
        return 92;
    }
    if(which <= 2)
    {
        in_thread(which == 1 ? nullptr : reinterpret_cast<void*>(1));
    }
    else
    {
        pthread_attr_t attributes;
        if(posix::pthread_attr_init_noexcept(&attributes) != 0) { return 93; }
#if defined(UWVM_TEST_EXPLICIT_THREAD_STACK)
        // Some QEMU-user/glibc combinations acknowledge MADV_GUARD_INSTALL
        // without delivering a fault at that guest guard. Keep default-pthread
        // tests on native hosts; use an explicit PROT_NONE guard in guest tests.
        auto const page{static_cast<std::size_t>(posix::sysconf_noexcept(_SC_PAGESIZE))};
        auto* allocation{static_cast<std::byte*>(posix::mmap_noexcept(nullptr, 128 * 1024 + 2 * page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0))};
        if(allocation == MAP_FAILED || posix::mprotect_noexcept(allocation + page, 128 * 1024, PROT_READ | PROT_WRITE) != 0) { return 94; }
        if(posix::pthread_attr_setstack_noexcept(&attributes, allocation + page, 128 * 1024) != 0) { return 94; }
#else
        if(posix::pthread_attr_setstacksize_noexcept(&attributes, 128 * 1024) != 0) { return 94; }
#endif
        pthread_t thread;
        if(posix::pthread_create_noexcept(&thread, &attributes, in_thread, which == 3 ? nullptr : reinterpret_cast<void*>(1)) != 0) { return 95; }
        posix::pthread_attr_destroy_noexcept(&attributes);
        posix::pthread_join_noexcept(thread, nullptr);
    }
    return 96;
}

struct cache_thread_context
{
    std::uintptr_t last_payload{};
    pthread_key_t late_key{};
    bool use_late_key{};
    unsigned late_calls{};
    bool repeat_late_reentry{};
    bool failed{};
};

struct concurrent_state
{
    std::atomic<unsigned> ready{};
    std::atomic<bool> leave{};
};
struct concurrent_entry
{
    concurrent_state* state;
    std::uintptr_t payload{};
    bool passed{};
};
static void* concurrent_worker(void* argument)
{
    auto& entry{*static_cast<concurrent_entry*>(argument)};
    ns::scope outer;
    stack_t stack{};
    auto const* bounds{ns::active_bounds.load()};
    auto const here{reinterpret_cast<std::uintptr_t>(&outer)};
    entry.passed = outer.ready() && posix::sigaltstack_noexcept(nullptr, &stack) == 0 && bounds != nullptr &&
                   here >= bounds->low && here < bounds->high;
    entry.payload = reinterpret_cast<std::uintptr_t>(stack.ss_sp);
    entry.state->ready.fetch_add(1, std::memory_order_release);
    while(!entry.state->leave.load(std::memory_order_acquire)) { posix::sched_yield_noexcept(); }
    ns::scope nested;
    entry.passed = entry.passed && nested.ready() && ns::active_bounds.load() == bounds;
    return nullptr;
}
static bool concurrent_first_entries()
{
    concurrent_state state{};
    concurrent_entry entries[8]{};
    pthread_t threads[8]{};
    unsigned started{};
    for(; started != 8; ++started)
    {
        entries[started].state = &state;
        if(posix::pthread_create_noexcept(&threads[started], nullptr, concurrent_worker, &entries[started]) != 0) { break; }
    }
    while(state.ready.load(std::memory_order_acquire) != started) { posix::sched_yield_noexcept(); }
    bool ok{started == 8 && ns::active_bounds.load() == nullptr};
    for(unsigned i{}; i != started; ++i)
    {
        if(!entries[i].passed || entries[i].payload == 0) { ok = false; }
        for(unsigned j{}; j != i; ++j) { if(entries[i].payload == entries[j].payload) { ok = false; } }
    }
    state.leave.store(true, std::memory_order_release);
    for(unsigned i{}; i != started; ++i)
    {
        if(posix::pthread_join_noexcept(threads[i], nullptr) != 0 || !entries[i].passed) { ok = false; }
    }
    return ok;
}

static bool host_alternate_restoration()
{
    stack_t original{};
    if(posix::sigaltstack_noexcept(nullptr, &original) != 0) { return false; }
    auto const bytes{ns::alternate_stack_bytes * 2u};
    void* memory{posix::mmap_noexcept(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)};
    if(memory == MAP_FAILED) { return false; }
    bool ok{true};
    // A host can change its signal stack between independent VM invocations.
    // Small host stacks must be replaced temporarily; adequate ones are reused.
    for(unsigned kind{}; kind != 2; ++kind)
    {
        stack_t host{};
        host.ss_sp = memory;
        host.ss_size = kind == 0 ? static_cast<std::size_t>(MINSIGSTKSZ) : bytes;
        if(posix::sigaltstack_noexcept(&host, nullptr) != 0) { ok = false; break; }
        for(unsigned repeat{}; repeat != 8; ++repeat)
        {
            {
                ns::scope guard;
                stack_t active{};
                if(!guard.ready() || posix::sigaltstack_noexcept(nullptr, &active) != 0 ||
                   active.ss_size < ns::alternate_stack_bytes ||
                   (host.ss_size >= ns::alternate_stack_bytes && active.ss_sp != host.ss_sp)) { ok = false; }
                ns::scope nested;
                stack_t inner{};
                if(!nested.ready() || posix::sigaltstack_noexcept(nullptr, &inner) != 0 || inner.ss_sp != active.ss_sp) { ok = false; }
            }
            stack_t after{};
            if(posix::sigaltstack_noexcept(nullptr, &after) != 0 || after.ss_sp != host.ss_sp ||
               after.ss_size != host.ss_size || after.ss_flags != host.ss_flags) { ok = false; }
        }
    }
#if defined(__APPLE__)
    if((original.ss_flags & SS_DISABLE) && original.ss_size < MINSIGSTKSZ) { original.ss_size = MINSIGSTKSZ; }
#endif
    if(posix::sigaltstack_noexcept(&original, nullptr) != 0) { return false; } // Do not free a registered stack.
    return posix::munmap_noexcept(memory, bytes) == 0 && ok;
}

static void late_reentry(void* argument)
{
    auto& context{*static_cast<cache_thread_context*>(argument)};
    ns::scope guard;
    stack_t alternate{};
    if(!guard.ready() || posix::sigaltstack_noexcept(nullptr, &alternate) != 0) { context.failed = true; return; }
    context.last_payload = reinterpret_cast<std::uintptr_t>(alternate.ss_sp);
    ++context.late_calls;
    // Keep re-arming this host destructor through the implementation's final
    // TSD pass. Re-entry must not leave a newly cached mapping behind when
    // pthreads stops running destructors after PTHREAD_DESTRUCTOR_ITERATIONS.
    if(context.repeat_late_reentry && posix::pthread_setspecific_noexcept(context.late_key, &context) != 0) { context.failed = true; }
}

static void* cached_thread(void* argument)
{
    auto& context{*static_cast<cache_thread_context*>(argument)};
    stack_t before{};
    if(posix::sigaltstack_noexcept(nullptr, &before) != 0) { context.failed = true; return nullptr; }
    for(unsigned i{}; i != 16; ++i)
    {
        {
            ns::scope guard;
            stack_t alternate{};
            if(!guard.ready() || posix::sigaltstack_noexcept(nullptr, &alternate) != 0) { context.failed = true; return nullptr; }
            auto const payload{reinterpret_cast<std::uintptr_t>(alternate.ss_sp)};
            if(i != 0 && payload != context.last_payload) { context.failed = true; return nullptr; }
            context.last_payload = payload;
        }
        stack_t after{};
        if(posix::sigaltstack_noexcept(nullptr, &after) != 0 || before.ss_flags != after.ss_flags ||
           (!(before.ss_flags & SS_DISABLE) && (before.ss_sp != after.ss_sp || before.ss_size != after.ss_size)))
        { context.failed = true; return nullptr; }
    }
    if(context.use_late_key && posix::pthread_setspecific_noexcept(context.late_key, &context) != 0) { context.failed = true; }
    return nullptr;
}

static bool cache_cleanup_case(bool late, bool repeat = false)
{
    cache_thread_context context{};
    context.use_late_key = late;
    context.repeat_late_reentry = repeat;
    if(late && posix::pthread_key_create_noexcept(&context.late_key, late_reentry) != 0) { return false; }
    pthread_t thread{};
    if(posix::pthread_create_noexcept(&thread, nullptr, cached_thread, &context) != 0)
    {
        if(late) { posix::pthread_key_delete_noexcept(context.late_key); }
        return false;
    }
    bool const joined{posix::pthread_join_noexcept(thread, nullptr) == 0};
    if(late) { posix::pthread_key_delete_noexcept(context.late_key); }
    if(!joined || context.failed || context.last_payload == 0 ||
       (late && (repeat ? context.late_calls < 4u : context.late_calls != 1u)))
    {
        std::fprintf(stderr, "cache worker failed: late=%d joined=%d failed=%d payload=%zx callbacks=%u\n",
                     late, joined, context.failed, context.last_payload, context.late_calls);
        return false;
    }
#if defined(__APPLE__)
    // Darwin mincore reports residency, not an ENOMEM guarantee for holes.
    // Query the mapping itself when checking that thread teardown released it.
    mach_vm_address_t address{context.last_payload};
    mach_vm_size_t bytes{};
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t count{VM_REGION_BASIC_INFO_COUNT_64};
    mach_port_t object{};
    auto const status{mach_vm_region(mach_task_self(), &address, &bytes, VM_REGION_BASIC_INFO_64,
                                    reinterpret_cast<vm_region_info_t>(&info), &count, &object)};
    if(object != MACH_PORT_NULL) { mach_port_deallocate(mach_task_self(), object); }
    if(status == KERN_INVALID_ADDRESS || (status == KERN_SUCCESS && address > context.last_payload)) { return true; }
    std::fprintf(stderr, "cache mapping not released: late=%d status=%d address=%llx payload=%zx\n",
                 late, status, static_cast<unsigned long long>(address), context.last_payload);
    return false;
#else
    unsigned char resident{};
    errno = 0;
    auto const status{posix::mincore_noexcept(reinterpret_cast<void*>(context.last_payload), 1, &resident)};
    if(status != -1 || errno != ENOMEM)
    {
        std::fprintf(stderr, "cache mapping not released: late=%d status=%d errno=%d\n", late, status, errno);
        return false;
    }
    return true;
#endif
}

#if defined(__linux__)
static bool stack_limit_refresh()
{
    rlimit original{};
    if(posix::getrlimit_noexcept(RLIMIT_STACK, &original) != 0) { return false; }
    rlimit changed{original};
    changed.rlim_cur = original.rlim_cur == RLIM_INFINITY ? 8u * 1024u * 1024u : original.rlim_cur / 2u;
    if(changed.rlim_cur < 512u * 1024u || posix::setrlimit_noexcept(RLIMIT_STACK, &changed) != 0) { return false; }
    rlimit effective{};
    if(posix::getrlimit_noexcept(RLIMIT_STACK, &effective) != 0 || effective.rlim_cur != changed.rlim_cur)
    {
        if(posix::setrlimit_noexcept(RLIMIT_STACK, &original) != 0) { return false; }
#if defined(UWVM_TEST_QEMU_USER)
        // The tested QEMU-user build reports its fixed guest stack size even
        // after accepting setrlimit. Do not count that as a live limit test.
        if(effective.rlim_cur == original.rlim_cur)
        {
            std::puts("SKIP live RLIMIT change: guest reports unchanged stack limit; native hosts test this separately");
            // Still exercise cache invalidation deterministically in the guest.
            ns::cached_bounds.stack_limit = 0;
            ns::scope refreshed;
            return refreshed.ready() && ns::cached_bounds.stack_limit == original.rlim_cur;
        }
#endif
        return false;
    }
    bool ok{};
    {
        ns::scope guard;
        pthread_attr_t attributes{};
        if(guard.ready() && posix::pthread_getattr_np_noexcept(posix::pthread_self_noexcept(), &attributes) == 0)
        {
            void* base{};
            std::size_t size{};
            auto const status{posix::pthread_attr_getstack_noexcept(&attributes, &base, &size)};
            posix::pthread_attr_destroy_noexcept(&attributes);
            auto const* active{ns::active_bounds.load()};
            auto const low{reinterpret_cast<std::uintptr_t>(base)};
            ok = status == 0 && active != nullptr && active->low == low && active->high == low + size &&
                 ns::cached_bounds.valid && ns::cached_bounds.stack_limit == changed.rlim_cur;
            if(!ok)
            {
                std::fprintf(stderr, "limit refresh mismatch: status=%d low=%zx active=%zx valid=%d fixed=%d cached=%llu requested=%llu\n",
                             status, low, active == nullptr ? 0u : active->low, ns::cached_bounds.valid, ns::cached_bounds.fixed_extent,
                             static_cast<unsigned long long>(ns::cached_bounds.stack_limit), static_cast<unsigned long long>(changed.rlim_cur));
            }
        }
        else { std::fprintf(stderr, "limit refresh setup failed: ready=%d errno=%d\n", guard.ready(), errno); }
    }
    if(posix::setrlimit_noexcept(RLIMIT_STACK, &original) != 0) { return false; }
    ns::scope restored;
    return ok && restored.ready() && ns::cached_bounds.stack_limit == original.rlim_cur;
}
#endif

int main()
{
    // A failed regression must not leave a multi-gigabyte native core dump.
    rlimit const core_limit{0, 0};
    if(posix::setrlimit_noexcept(RLIMIT_CORE, &core_limit) != 0) { return 1; }
    for(int which = 0; which != 8; ++which)
    {
        auto const pid{posix::fork_noexcept()};
        if(pid < 0) { return 1; }
        if(pid == 0) { posix::_exit_noexcept(child_case(which)); }
        int status{};
        int const expected{which == 0 ? 73 : (which == 5 || which == 6) ? 0 : 127};
        if(posix::waitpid_noexcept(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != expected)
        {
            std::fprintf(stderr, "native-stack case %d failed: status=%d\n", which, status);
            return 1;
        }
    }

    stack_t before{}, after{};
    if(posix::sigaltstack_noexcept(nullptr, &before) != 0) { return 1; }
    {
        ns::scope outer;
        if(!outer.ready() || ns::active_bounds == nullptr) { return 1; }
        auto const* original{ns::active_bounds.load()};
        { ns::scope inner; if(!inner.ready() || ns::active_bounds != original) { return 1; } }
        if(ns::active_bounds != original) { return 1; }
    }
    if(ns::active_bounds != nullptr || posix::sigaltstack_noexcept(nullptr, &after) != 0) { return 1; }
    if(before.ss_flags != after.ss_flags) { return 1; }
    if(!(before.ss_flags & SS_DISABLE) && (before.ss_sp != after.ss_sp || before.ss_size != after.ss_size)) { return 1; }
    if(!cache_cleanup_case(false) || !cache_cleanup_case(true) || !cache_cleanup_case(true, true)) { return 1; }
    if(!host_alternate_restoration()) { std::fputs("host alternate restoration failed\n", stderr); return 1; }
#if defined(__linux__)
    if(!stack_limit_refresh()) { std::fputs("stack limit refresh failed\n", stderr); return 1; }
#endif
    std::puts("native-stack cache: reuse, cleanup, late re-entry and restoration passed");
    std::puts("native-stack guard: all checks passed");
}
#else
int main() { std::puts("native-stack guard: classification only; platform handler unsupported"); }
#endif
