// This test must run inside the Windows x64 guest whose QEMU process shares
// the 64 GiB/20-CPU Linux cgroup used to cross-compile this test executable.
#include <uwvm2/uwvm/debugger/native_step_windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <windows.h>

#if !defined(_WIN32) || (!defined(_M_X64) && !defined(__x86_64__))
# error This test requires native Windows x64.
#endif

namespace backend = ::uwvm2::uwvm::debugger::native_step_windows;
namespace
{
    ::std::atomic_bool poll_seen{};
    ::std::atomic_bool may_return{};
    ::std::atomic<::std::uint32_t> worker_tid{};
    ::std::atomic<unsigned> unrelated_traps{};
    ::std::uint64_t counter{};
    void* executable{};
    struct parked_control
    {
        ::std::atomic_bool ready{};
        ::std::atomic_bool stop{};
        ::std::atomic<DWORD> tid{};
    };

    [[noreturn]] void fail(char const* name)
    {
        ::std::fprintf(stderr, "FAIL Windows native step: %s\n", name);
        ::std::abort();
    }
    void check(bool condition, char const* name)
    { if(!condition) { fail(name); } }
    bool wait_for(backend::session const& step, backend::phase wanted)
    {
        auto const deadline{::GetTickCount64() + 5000u};
        while(::GetTickCount64() < deadline)
        {
            auto const state{step.state.load(::std::memory_order_acquire)};
            if(state == wanted) { return true; }
            if(state == backend::phase::failed) { return false; }
            ::Sleep(1u);
        }
        return false;
    }
    LONG CALLBACK prior_handler(::EXCEPTION_POINTERS* pointers) noexcept
    {
        if(pointers != nullptr && pointers->ExceptionRecord != nullptr &&
           pointers->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP)
        {
            unrelated_traps.fetch_add(1u, ::std::memory_order_relaxed);
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
    DWORD WINAPI worker(void*) noexcept
    {
        worker_tid.store(::GetCurrentThreadId(), ::std::memory_order_release);
        auto const function{reinterpret_cast<void(*)(::std::uint64_t*)>(executable)};
        function(::std::addressof(counter));
        return 0u;
    }
    DWORD WINAPI contender(void* parameter) noexcept
    {
        auto* const ready{static_cast<::std::atomic_bool*>(parameter)};
        while(!ready->load(::std::memory_order_acquire)) { ::SwitchToThread(); }
        auto const pc{reinterpret_cast<::std::uintptr_t>(executable) + 20u};
        if(backend::arm_at_return(pc)) { fail("cancelled/non-target arm refused"); }
        return 0u;
    }
    DWORD WINAPI parked(void* parameter) noexcept
    {
        auto& control{*static_cast<parked_control*>(parameter)};
        control.tid.store(::GetCurrentThreadId(), ::std::memory_order_release);
        control.ready.store(true, ::std::memory_order_release);
        while(!control.stop.load(::std::memory_order_acquire)) { ::SwitchToThread(); }
        return 0u;
    }
    void set_hardware_breakpoint(HANDLE thread, ::std::uintptr_t pc, bool enabled)
    {
        check(::SuspendThread(thread) != DWORD(-1), "SuspendThread breakpoint setup");
        ::CONTEXT context{};
        context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        check(::GetThreadContext(thread, ::std::addressof(context)) != 0,
              "GetThreadContext breakpoint setup");
        context.Dr0 = pc;
        context.Dr7 = (context.Dr7 & ~0xffu) | (enabled ? 1u : 0u);
        check(::SetThreadContext(thread, ::std::addressof(context)) != 0,
              "SetThreadContext breakpoint setup");
        ::CONTEXT verified{};
        verified.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        check(::GetThreadContext(thread, ::std::addressof(verified)) != 0,
              "GetThreadContext breakpoint verification");
        check((verified.Dr7 & 1u) == (enabled ? 1u : 0u), "Dr7 breakpoint verification");
        check(::ResumeThread(thread) != DWORD(-1), "ResumeThread breakpoint setup");
    }
}

extern "C" void native_step_test_poll() noexcept
{
    poll_seen.store(true, ::std::memory_order_release);
    while(!may_return.load(::std::memory_order_acquire)) { ::SwitchToThread(); }
}
extern "C" int native_step_test_arm(::std::uintptr_t pc) noexcept
{ return backend::arm_at_return(pc) ? 1 : 0; }

// The final POPFQ/RET sets TF only for the return into authenticated JIT
// code. The 40-byte stack reservation supplies Win64's shadow space and
// alignment; no compiled C++ epilogue can consume the first single-step trap.
extern "C" __attribute__((naked)) void native_step_test_bridge() noexcept
{
    __asm__ volatile(
        "subq $40, %%rsp\n\t"
        "call native_step_test_poll\n\t"
        "addq $40, %%rsp\n\t"
        "movq (%%rsp), %%rcx\n\t"
        "subq $40, %%rsp\n\t"
        "call native_step_test_arm\n\t"
        "addq $40, %%rsp\n\t"
        "testl %%eax, %%eax\n\t"
        "je 1f\n\t"
        "pushfq\n\t"
        "orq $0x100, (%%rsp)\n\t"
        "popfq\n\t"
        "1: retq\n\t" ::: "memory");
}

int main(int argc, char** argv)
{
    if(argc == 2 && ::std::strcmp(argv[1], "--invalid-gate") == 0)
    {
        // Isolated subprocess probes the otherwise fatal internal ownership
        // failure. The parent must observe the fail-closed exit code.
        backend::session broken{};
        check(backend::install(), "invalid-gate install");
        constexpr ::std::uintptr_t expected_pc{0x1800u};
        broken.target_thread = ::GetCurrentThreadId();
        broken.owner_begin = 0x1000u;
        broken.owner_end = 0x2000u;
        broken.expected_pc = expected_pc;
        broken.first_gate = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        broken.second_gate = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        check(broken.first_gate != nullptr && broken.second_gate != nullptr,
              "invalid-gate events");
        broken.state.store(backend::phase::arming, ::std::memory_order_release);
        backend::details::active.store(::std::addressof(broken), ::std::memory_order_release);
        backend::details::current_thread_session = ::std::addressof(broken);
        check(::CloseHandle(broken.first_gate) != 0, "invalidate gate handle");
        broken.first_gate = INVALID_HANDLE_VALUE;
        ::EXCEPTION_RECORD record{};
        record.ExceptionCode = EXCEPTION_SINGLE_STEP;
        ::CONTEXT context{};
        context.Rip = expected_pc;
        context.EFlags = 0x100u;
        ::EXCEPTION_POINTERS pointers{::std::addressof(record), ::std::addressof(context)};
        static_cast<void>(backend::details::veh_handler(::std::addressof(pointers)));
        fail("invalid-gate handler returned to guest");
    }
    if(argc == 2 && ::std::strcmp(argv[1], "--invalid-set-event") == 0)
    {
        backend::session broken{};
        check(backend::install(), "invalid-event install");
        broken.first_gate = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        broken.second_gate = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        check(broken.first_gate != nullptr && broken.second_gate != nullptr,
              "invalid-event events");
        check(::CloseHandle(broken.first_gate) != 0, "invalidate event handle");
        broken.first_gate = INVALID_HANDLE_VALUE;
        broken.state.store(backend::phase::at_guest_pc, ::std::memory_order_release);
        backend::details::active.store(::std::addressof(broken), ::std::memory_order_release);
        static_cast<void>(backend::continue_one(broken));
        fail("invalid-event signal returned to guest");
    }
    if(argc == 2 && ::std::strcmp(argv[1], "--invalid-first-pc") == 0)
    {
        backend::session broken{};
        check(backend::install(), "invalid-pc install");
        broken.target_thread = ::GetCurrentThreadId();
        broken.owner_begin = 0x1000u;
        broken.owner_end = 0x2000u;
        broken.expected_pc = 0x1800u;
        broken.first_gate = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        broken.second_gate = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        check(broken.first_gate != nullptr && broken.second_gate != nullptr,
              "invalid-pc events");
        broken.state.store(backend::phase::arming, ::std::memory_order_release);
        backend::details::active.store(::std::addressof(broken), ::std::memory_order_release);
        backend::details::current_thread_session = ::std::addressof(broken);
        ::EXCEPTION_RECORD record{};
        record.ExceptionCode = EXCEPTION_SINGLE_STEP;
        ::CONTEXT context{};
        context.Rip = 0x1801u;
        context.EFlags = 0x100u;
        ::EXCEPTION_POINTERS pointers{::std::addressof(record), ::std::addressof(context)};
        static_cast<void>(backend::details::veh_handler(::std::addressof(pointers)));
        fail("invalid-pc handler returned to guest");
    }
    char self[MAX_PATH]{};
    auto const self_length{::GetModuleFileNameA(nullptr, self, MAX_PATH)};
    check(self_length > 0u && self_length < MAX_PATH, "GetModuleFileNameA");
    struct fatal_probe { char const* name; DWORD expected_exit; };
    constexpr fatal_probe probes[]{
        {"--invalid-gate", backend::details::invalid_gate_exit_code},
        {"--invalid-set-event", backend::details::invalid_gate_exit_code},
        {"--invalid-first-pc", backend::details::invalid_pc_exit_code}
    };
    for(auto const& probe : probes)
    {
        char child_command[MAX_PATH + 64]{};
        check(::std::snprintf(child_command, sizeof(child_command), "\"%s\" %s", self, probe.name) > 0,
              "fatal child command");
        ::STARTUPINFOA startup{};
        startup.cb = sizeof(startup);
        ::PROCESS_INFORMATION child{};
        check(::CreateProcessA(self, child_command, nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                               nullptr, nullptr, ::std::addressof(startup), ::std::addressof(child)) != 0,
              "CreateProcessA fatal child");
        check(::WaitForSingleObject(child.hProcess, 5000u) == WAIT_OBJECT_0,
              "fatal child exited");
        DWORD child_exit{};
        check(::GetExitCodeProcess(child.hProcess, ::std::addressof(child_exit)) != 0 &&
              child_exit == probe.expected_exit,
              "invalid event failed closed before guest execution");
        check(::CloseHandle(child.hThread) != 0, "CloseHandle fatal child thread");
        check(::CloseHandle(child.hProcess) != 0, "CloseHandle fatal child process");
    }

    check(backend::platform_available(), "x64 platform enabled");
    auto* const prior{::AddVectoredExceptionHandler(0u, prior_handler)};
    check(prior != nullptr, "prior VEH");
    check(backend::install(), "install");

    constexpr ::std::size_t page_size{4096u};
    auto* const code{static_cast<::std::uint8_t*>(::VirtualAlloc(nullptr, page_size,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))};
    check(code != nullptr, "VirtualAlloc RW");
    // Win64: push rbx; mov rbx,rcx; sub rsp,32; movabs rax,bridge; call rax;
    // incq (%rbx); add rsp,32; incq (%rbx); pop rbx; ret. The first INC is
    // deliberately the exact return PC from the bridge, before stack cleanup.
    constexpr ::std::uint8_t prefix[]{0x53, 0x48, 0x89, 0xcb, 0x48, 0x83, 0xec, 0x20, 0x48, 0xb8};
    constexpr ::std::uint8_t suffix[]{0xff, 0xd0, 0x48, 0xff, 0x03,
        0x48, 0x83, 0xc4, 0x20, 0x48, 0xff, 0x03, 0x5b, 0xc3};
    ::std::memcpy(code, prefix, sizeof(prefix));
    auto const bridge{reinterpret_cast<::std::uintptr_t>(native_step_test_bridge)};
    ::std::memcpy(code + sizeof(prefix), ::std::addressof(bridge), sizeof(bridge));
    ::std::memcpy(code + sizeof(prefix) + sizeof(bridge), suffix, sizeof(suffix));
    constexpr auto code_size{sizeof(prefix) + sizeof(bridge) + sizeof(suffix)};
    static_assert(code_size == 32u);
    DWORD old_protection{};
    check(::VirtualProtect(code, page_size, PAGE_EXECUTE_READ, ::std::addressof(old_protection)) != 0,
          "VirtualProtect RX");
    check(::FlushInstructionCache(::GetCurrentProcess(), code, code_size) != 0,
          "FlushInstructionCache");
    executable = code;

    backend::session denied{};
    auto const first{reinterpret_cast<::std::uintptr_t>(code + 20u)};
    check(!backend::request(denied, 1u, reinterpret_cast<::std::uintptr_t>(code),
        reinterpret_cast<::std::uintptr_t>(code + code_size),
        reinterpret_cast<::std::uintptr_t>(code + code_size)), "owner end excluded");
    check(!backend::arm_at_return(first), "no active target");

    backend::session step{};
    auto* const thread{::CreateThread(nullptr, 0u, worker, nullptr, 0u, nullptr)};
    check(thread != nullptr, "CreateThread worker");
    auto const deadline{::GetTickCount64() + 5000u};
    while(!poll_seen.load(::std::memory_order_acquire) && ::GetTickCount64() < deadline)
    { ::Sleep(1u); }
    check(poll_seen.load(::std::memory_order_acquire), "worker reached bridge");
    auto const tid{worker_tid.load(::std::memory_order_acquire)};
    set_hardware_breakpoint(thread, first, true);
    backend::session breakpoint_denied{};
    check(!backend::request(breakpoint_denied, tid,
        reinterpret_cast<::std::uintptr_t>(code),
        reinterpret_cast<::std::uintptr_t>(code + code_size), first),
        "active hardware breakpoint rejected");
    set_hardware_breakpoint(thread, first, false);
    check(backend::request(step, tid, reinterpret_cast<::std::uintptr_t>(code),
        reinterpret_cast<::std::uintptr_t>(code + code_size), first), "authorized request");
    check(!backend::arm_at_return(first), "wrong native thread refused");
    may_return.store(true, ::std::memory_order_release);
    auto const reached_first{wait_for(step, backend::phase::at_guest_pc)};
    if(!reached_first)
    {
        ::std::fprintf(stderr,
            "first trap detail: state=%u expected=%llx observed=%llx dr6=%llx eflags=%x counter=%llu tid=%lu\n",
            static_cast<unsigned>(step.state.load(::std::memory_order_acquire)),
            static_cast<unsigned long long>(first),
            static_cast<unsigned long long>(step.first_pc),
            static_cast<unsigned long long>(step.first_dr6), step.first_eflags,
            static_cast<unsigned long long>(counter), static_cast<unsigned long>(tid));
    }
    check(reached_first, "first trap at guest PC");
    check(step.first_pc == first && counter == 0u &&
          step.first_exception_code == EXCEPTION_SINGLE_STEP,
          "zero guest instructions at first trap");
    unsigned register_callbacks{};
    check(!backend::with_owned_registers(reinterpret_cast<void const*>(::std::uintptr_t{1}),
        [&](auto...) noexcept { ++register_callbacks; }), "forged register session rejected before dereference");
    check(!backend::with_owned_registers(::std::addressof(step),
        [&](auto...) noexcept { ++register_callbacks; }), "first gate is not a completed native register stop");
    check(register_callbacks == 0u, "unauthorized register callback never called");
    check(backend::continue_one(step), "first instruction resume");
    check(wait_for(step, backend::phase::trapped), "single machine instruction trap");
    check(step.next_pc == first + 3u && counter == 1u &&
          step.second_exception_code == EXCEPTION_SINGLE_STEP,
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

    check(backend::continue_from_trap(step), "second instruction resume");
    check(wait_for(step, backend::phase::trapped), "second machine instruction trap");
    check(step.next_pc == first + 7u && counter == 1u &&
          step.second_exception_code == EXCEPTION_SINGLE_STEP,
          "exactly two guest instructions");
    ::std::printf("Windows single-step context: first Dr6=%llx EFlags=%x; second Dr6=%llx EFlags=%x\n",
                  static_cast<unsigned long long>(step.first_dr6), step.first_eflags,
                  static_cast<unsigned long long>(step.second_dr6), step.second_eflags);
    ::RaiseException(EXCEPTION_SINGLE_STEP, 0u, 0u, nullptr);
    check(unrelated_traps.load(::std::memory_order_acquire) == 1u,
          "non-target VEH chaining");
    check(counter == 1u, "target stayed stopped during non-target trap");
    check(backend::release(step), "second-trap release");
    check(::WaitForSingleObject(thread, 5000u) == WAIT_OBJECT_0, "worker release");
    check(counter == 2u && step.state.load(::std::memory_order_acquire) == backend::phase::released,
          "release executes second guest instruction");
    check(::CloseHandle(thread) != 0, "CloseHandle worker");
    check(backend::clear(step), "clear owned session");
    check(!backend::with_owned_registers(::std::addressof(step),
        [&](auto...) noexcept { ++register_callbacks; }) && register_callbacks == 1u && copied_registers.size() == 18u,
        "retired register session rejected and retained value copy stays available");


    // Releasing from the first stop must clear TF and let the whole JIT
    // function finish, without exposing a half-released session to clear().
    poll_seen.store(false, ::std::memory_order_release);
    may_return.store(false, ::std::memory_order_release);
    counter = 0u;
    backend::session cancelled_at_first{};
    auto* const cancelled_worker{::CreateThread(nullptr, 0u, worker, nullptr, 0u, nullptr)};
    check(cancelled_worker != nullptr, "CreateThread first-stop cancel");
    auto const cancelled_deadline{::GetTickCount64() + 5000u};
    while(!poll_seen.load(::std::memory_order_acquire) &&
          ::GetTickCount64() < cancelled_deadline) { ::Sleep(1u); }
    check(poll_seen.load(::std::memory_order_acquire), "first-stop cancel worker ready");
    check(backend::request(cancelled_at_first,
        worker_tid.load(::std::memory_order_acquire),
        reinterpret_cast<::std::uintptr_t>(code),
        reinterpret_cast<::std::uintptr_t>(code + code_size), first),
        "first-stop cancel request");
    may_return.store(true, ::std::memory_order_release);
    check(wait_for(cancelled_at_first, backend::phase::at_guest_pc),
          "first-stop cancel observed");
    check(backend::release(cancelled_at_first), "first-trap release");
    check(::WaitForSingleObject(cancelled_worker, 5000u) == WAIT_OBJECT_0,
          "first-stop cancel worker released");
    check(counter == 2u &&
        cancelled_at_first.state.load(::std::memory_order_acquire) == backend::phase::released,
        "first-stop cancel completes without TF");
    check(::CloseHandle(cancelled_worker) != 0, "CloseHandle first-stop cancel worker");
    check(backend::clear(cancelled_at_first), "clear first-stop cancelled session");

    // Host cancellation of a ready session may race an unrelated return shim.
    // arm_at_return must re-read active under its transition lock before it
    // borrows a pointer that the host may destroy immediately after clear.
    parked_control parked_state{};
    auto* const parked_thread{::CreateThread(nullptr, 0u, parked,
        ::std::addressof(parked_state), 0u, nullptr)};
    check(parked_thread != nullptr, "CreateThread parked target");
    auto const parked_deadline{::GetTickCount64() + 5000u};
    while(!parked_state.ready.load(::std::memory_order_acquire) &&
          ::GetTickCount64() < parked_deadline) { ::Sleep(1u); }
    check(parked_state.ready.load(::std::memory_order_acquire), "parked target ready");
    for(unsigned round{}; round != 256u; ++round)
    {
        auto cancelled{::std::make_unique<backend::session>()};
        check(backend::request(*cancelled,
            parked_state.tid.load(::std::memory_order_acquire),
            reinterpret_cast<::std::uintptr_t>(code),
            reinterpret_cast<::std::uintptr_t>(code + code_size), first),
            "cancel-ready request");
        ::std::atomic_bool go{};
        auto* const contender_thread{::CreateThread(nullptr, 0u, contender, ::std::addressof(go), 0u, nullptr)};
        check(contender_thread != nullptr, "CreateThread contender");
        go.store(true, ::std::memory_order_release);
        check(backend::clear(*cancelled), "cancel ready under arm race");
        cancelled.reset();
        check(::WaitForSingleObject(contender_thread, 5000u) == WAIT_OBJECT_0,
              "contender joined");
        check(::CloseHandle(contender_thread) != 0, "CloseHandle contender");
    }
    parked_state.stop.store(true, ::std::memory_order_release);
    check(::WaitForSingleObject(parked_thread, 5000u) == WAIT_OBJECT_0,
          "parked target joined");
    check(::CloseHandle(parked_thread) != 0, "CloseHandle parked target");
    check(backend::uninstall(), "uninstall");
    ::RaiseException(EXCEPTION_SINGLE_STEP, 0u, 0u, nullptr);
    check(unrelated_traps.load(::std::memory_order_acquire) == 2u,
          "prior VEH retained after uninstall");
    check(::RemoveVectoredExceptionHandler(prior) != 0u, "remove prior VEH");
    check(::VirtualFree(code, 0u, MEM_RELEASE) != 0, "VirtualFree");
    ::std::puts("PASS Windows x64 native step: zero/one/two guest instructions, owner/TID, VEH chaining, cancellation");
}
