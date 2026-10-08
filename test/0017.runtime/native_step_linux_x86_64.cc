// Controlled executable-code probe for the host-only Linux x86_64 native step backend.
// The generated leaf has two architecturally visible INC instructions after
// its safepoint call. The first SIGTRAP must see zero, the second exactly one.
#include <uwvm2/uwvm/debugger/native_step.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <thread>
#include <sys/mman.h>
#include <unistd.h>

#if !defined(__linux__) || !defined(__x86_64__)
# error This test must only be built on Linux x86_64.
#endif

namespace backend = ::uwvm2::uwvm::debugger::native_step;

namespace
{
    ::std::atomic_bool poll_seen{};
    ::std::atomic_bool may_return{};
    ::std::atomic<::std::uint_least64_t> worker_tid{};
    ::std::atomic<unsigned> unrelated_traps{};

    [[noreturn]] void fail(char const* name)
    {
        ::std::fprintf(stderr, "FAIL native-step: %s\n", name);
        ::std::abort();
    }
    void check(bool condition, char const* name)
    {
        if(!condition) { fail(name); }
    }
    bool wait_for(backend::session const& step, backend::phase wanted)
    {
        auto const deadline{::std::chrono::steady_clock::now() + ::std::chrono::seconds{5}};
        while(::std::chrono::steady_clock::now() < deadline)
        {
            auto const phase{step.state.load(::std::memory_order_acquire)};
            if(phase == wanted) { return true; }
            if(phase == backend::phase::failed) { return false; }
            ::std::this_thread::yield();
        }
        return false;
    }
    void prior_trap(int, ::siginfo_t*, void*) noexcept
    { unrelated_traps.fetch_add(1u, ::std::memory_order_relaxed); }
}

extern "C" void native_step_test_poll() noexcept
{
    poll_seen.store(true, ::std::memory_order_release);
    while(!may_return.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
}
extern "C" int native_step_test_arm(::std::uintptr_t pc) noexcept
{ return backend::arm_at_return(pc) ? 1 : 0; }
// This is the same naked/CFI wrapper form used by the production debug-only
// bridge. The final POPFQ/RET pair arms TF; no host epilogue can consume it.
extern "C" __attribute__((naked)) void native_step_test_bridge() noexcept
{
    __asm__ volatile(
        "movq (%%rsp), %%r9\n\t"
        "subq $8, %%rsp\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "call native_step_test_poll\n\t"
        "addq $8, %%rsp\n\t"
        ".cfi_adjust_cfa_offset -8\n\t"
        "movq (%%rsp), %%rdi\n\t"
        "subq $8, %%rsp\n\t"
        ".cfi_adjust_cfa_offset 8\n\t"
        "call native_step_test_arm\n\t"
        "addq $8, %%rsp\n\t"
        ".cfi_adjust_cfa_offset -8\n\t"
        "testl %%eax, %%eax\n\t"
        "je 1f\n\t"
        "pushfq\n\t"
        "orq $0x100, (%%rsp)\n\t"
        "popfq\n\t"
        "1: retq\n\t" ::: "memory");
}

int main()
{
    check(::std::strcmp(::std::getenv("UWVM_TEST_CPUSET") == nullptr ? "" : ::std::getenv("UWVM_TEST_CPUSET"),
                        "0,2,4,6,16-31") == 0, "cgroup cpuset environment");
    struct ::sigaction old_action{};
    struct ::sigaction prior{};
    prior.sa_sigaction = prior_trap;
    prior.sa_flags = SA_SIGINFO;
    ::sigemptyset(::std::addressof(prior.sa_mask));
    check(::sigaction(SIGTRAP, ::std::addressof(prior), ::std::addressof(old_action)) == 0, "prior handler");
    check(backend::install(), "install");

    constexpr ::std::size_t page_size{4096u};
    auto* const code{static_cast<::std::uint8_t*>(::mmap(nullptr, page_size,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0))};
    check(code != MAP_FAILED, "mmap RW");
    // push rbx; mov rbx,rdi; movabs rax,bridge; call rax;
    // incq (%rbx); incq (%rbx); pop rbx; ret.
    constexpr ::std::uint8_t prefix[]{0x53, 0x48, 0x89, 0xfb, 0x48, 0xb8};
    constexpr ::std::uint8_t suffix[]{0xff, 0xd0, 0x48, 0xff, 0x03, 0x48, 0xff, 0x03, 0x5b, 0xc3};
    ::std::memcpy(code, prefix, sizeof(prefix));
    auto const bridge{reinterpret_cast<::std::uintptr_t>(native_step_test_bridge)};
    ::std::memcpy(code + sizeof(prefix), ::std::addressof(bridge), sizeof(bridge));
    ::std::memcpy(code + sizeof(prefix) + sizeof(bridge), suffix, sizeof(suffix));
    auto const code_size{sizeof(prefix) + sizeof(bridge) + sizeof(suffix)};
    check(code_size == 24u, "encoded code size");
    __builtin___clear_cache(reinterpret_cast<char*>(code), reinterpret_cast<char*>(code + code_size));
    check(::mprotect(code, page_size, PROT_READ | PROT_EXEC) == 0, "mprotect RX");

    backend::session denied{};
    check(!backend::request(denied, 1u, reinterpret_cast<::std::uintptr_t>(code),
        reinterpret_cast<::std::uintptr_t>(code + code_size),
        reinterpret_cast<::std::uintptr_t>(code + code_size)), "owner end excluded");
    check(!backend::arm_at_return(reinterpret_cast<::std::uintptr_t>(code + 16u)), "no active target");

    ::std::uint64_t counter{};
    backend::session step{};
    ::std::thread worker{[&]
    {
        worker_tid.store(static_cast<::std::uint_least64_t>(backend::details::raw_system_call(SYS_gettid)),
            ::std::memory_order_release);
        auto const function{reinterpret_cast<void(*)(::std::uint64_t*)>(code)};
        function(::std::addressof(counter));
    }};
    while(!poll_seen.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
    auto const tid{worker_tid.load(::std::memory_order_acquire)};
    auto const first{reinterpret_cast<::std::uintptr_t>(code + 16u)};
    check(backend::request(step, tid, reinterpret_cast<::std::uintptr_t>(code),
        reinterpret_cast<::std::uintptr_t>(code + code_size), first), "authorized request");
    check(!backend::arm_at_return(first), "wrong native thread refused");
    may_return.store(true, ::std::memory_order_release);
    check(wait_for(step, backend::phase::at_guest_pc), "first trap at guest PC");
    check(step.first_pc == first && counter == 0u && step.first_signal_code == TRAP_TRACE,
          "zero guest instructions at first trap");
    unsigned register_callbacks{};
    check(!backend::with_owned_registers(reinterpret_cast<void const*>(::std::uintptr_t{1}),
        [&](auto...) noexcept { ++register_callbacks; }), "forged register session rejected before dereference");
    check(!backend::with_owned_registers(::std::addressof(step),
        [&](auto...) noexcept { ++register_callbacks; }), "first gate is not a completed native register stop");
    check(register_callbacks == 0u, "unauthorized register callback never called");
    check(backend::continue_one(step), "continue first instruction");
    check(wait_for(step, backend::phase::trapped), "single native instruction trap");
    check(step.next_pc == first + 3u && counter == 1u && step.second_signal_code == TRAP_TRACE,
          "exactly one guest instruction");
    ::uwvm2::uwvm::debugger::native_registers::snapshot copied_registers{};
    check(backend::with_owned_registers(::std::addressof(step),
        [&](auto actual_thread, auto actual_pc, auto actual_begin, auto actual_end, auto const& registers) noexcept
        {
            check(actual_thread == tid && actual_pc == first + 3u && actual_begin == reinterpret_cast<::std::uintptr_t>(code) && actual_end == reinterpret_cast<::std::uintptr_t>(code + code_size),
                "register response belongs to exact selected native thread and code range");
            copied_registers = registers;
            ++register_callbacks;
        }), "read selected real native trap registers");
    check(copied_registers.size() == 18u && copied_registers.pc() == first + 3u && copied_registers.sp() != 0u &&
          copied_registers.values[1u] == reinterpret_cast<::std::uintptr_t>(::std::addressof(counter)) &&
          (copied_registers.values[17u] & 0x100u) == 0u && register_callbacks == 1u,
          "real RBX, SP, RIP and debugger-normalized RFLAGS");

    ::std::thread other{[] { ::raise(SIGTRAP); }};
    other.join();
    check(unrelated_traps.load(::std::memory_order_acquire) == 1u,
          "non-target signal forwarded");
    check(counter == 1u, "target stayed stopped during non-target trap");
    check(backend::continue_from_trap(step), "continue second instruction from trapped PC");
    check(wait_for(step, backend::phase::trapped), "second native instruction trap");
    check(step.next_pc == first + 6u && counter == 2u && step.second_signal_code == TRAP_TRACE,
          "exactly one more guest instruction");
    check(backend::release(step), "release trapped target");
    worker.join();
    check(counter == 2u && step.state.load(::std::memory_order_acquire) == backend::phase::released,
          "release resumes without an extra increment");
    check(backend::clear(step), "clear owned session");
    check(!backend::with_owned_registers(::std::addressof(step),
        [&](auto...) noexcept { ++register_callbacks; }) && register_callbacks == 1u && copied_registers.size() == 18u,
        "retired register session rejected and retained value copy stays available");

    // Cancel while the first SIGTRAP gate still holds the target at the
    // authenticated JIT return PC. No guest machine instruction may execute
    // before the host opens that gate, and TF must be cleared on release.
    poll_seen.store(false, ::std::memory_order_release);
    may_return.store(false, ::std::memory_order_release);
    worker_tid.store(0u, ::std::memory_order_release);
    counter = 0u;
    backend::session abort_first{};
    ::std::thread abort_worker{[&]
    {
        worker_tid.store(static_cast<::std::uint_least64_t>(backend::details::raw_system_call(SYS_gettid)),
            ::std::memory_order_release);
        auto const function{reinterpret_cast<void(*)(::std::uint64_t*)>(code)};
        function(::std::addressof(counter));
    }};
    while(!poll_seen.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
    check(backend::request(abort_first, worker_tid.load(::std::memory_order_acquire),
        reinterpret_cast<::std::uintptr_t>(code),
        reinterpret_cast<::std::uintptr_t>(code + code_size), first), "first-trap abort request");
    may_return.store(true, ::std::memory_order_release);
    check(wait_for(abort_first, backend::phase::at_guest_pc), "first-trap abort arrived");
    check(abort_first.first_pc == first && counter == 0u, "abort gate has executed zero guest instructions");
    check(backend::release(abort_first), "release first trap without stepping");
    check(wait_for(abort_first, backend::phase::released), "first-trap abort released");
    check(backend::clear(abort_first), "clear first-trap abort session");
    abort_worker.join();
    check(counter == 2u, "guest completed both instructions only after abort release");
    // A host may cancel a ready session while an unrelated thread enters the
    // return shim. Re-reading active under the host transition guard must
    // prevent any borrow after clear removes and destroys the old session.
    for(unsigned round{}; round!=256u; ++round)
    {
        auto cancelled=::std::make_unique<backend::session>();
        check(backend::request(*cancelled, (::std::numeric_limits<::std::uint_least64_t>::max)(),
            reinterpret_cast<::std::uintptr_t>(code),
            reinterpret_cast<::std::uintptr_t>(code + code_size), first), "cancel-ready request");
        ::std::atomic_bool go{};
        ::std::thread contender{[&]
        {
            while(!go.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
            check(!backend::arm_at_return(first), "cancelled/non-target arm refused");
        }};
        go.store(true, ::std::memory_order_release);
        check(backend::clear(*cancelled), "cancel ready under arm race");
        cancelled.reset();
        contender.join();
    }
    check(backend::uninstall(), "uninstall");
    check(::sigaction(SIGTRAP, ::std::addressof(old_action), nullptr) == 0, "restore original disposition");
    check(::munmap(code, page_size) == 0, "munmap");
    ::std::puts("PASS native step: zero/one/one instruction, first-trap abort, persistent trap, owner bound, selected thread, signal forwarding, ready cancellation race");
}
