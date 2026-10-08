// New qualification input: never replace the frozen legacy Windows fixture.
// Cross-compile and run only through the Windows/Linux keeper's shared
// 64 GiB cgroup. Compilation alone does not qualify native Windows stepping.
#include <uwvm2/uwvm/debugger/native_step_windows.h>
#include <fast_io.h>
#include <fast_io_dsal/string_view.h>
#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <windows.h>

#if !defined(_WIN32) || (!defined(_M_X64) && !defined(__x86_64__)) || (!defined(__clang__) && !defined(__GNUC__))
# error This fixture requires a GNU/Clang Windows x64 target and its real SDK closure.
#endif

namespace backend = ::uwvm2::uwvm::debugger::native_step_windows;
namespace control_abi = ::uwvm2::utils::control::win32_abi;
namespace fixture_abi
{
    // Only APIs absent from fast_io's existing Win32 declarations require
    // fixture-local aliases. SDK parameter types and calling conventions are
    // retained; no global C declaration is redeclared with a new exception spec.
    extern "C" __declspec(dllimport) ULONGLONG WINAPI tick_count() noexcept __asm__("GetTickCount64");
    extern "C" __declspec(dllimport) DWORD WINAPI module_path(HMODULE, LPSTR, DWORD) noexcept __asm__("GetModuleFileNameA");
    extern "C" __declspec(dllimport) LPVOID WINAPI virtual_alloc(LPVOID, SIZE_T, DWORD, DWORD) noexcept __asm__("VirtualAlloc");
    extern "C" __declspec(dllimport) BOOL WINAPI virtual_protect(LPVOID, SIZE_T, DWORD, PDWORD) noexcept __asm__("VirtualProtect");
    extern "C" __declspec(dllimport) BOOL WINAPI virtual_free(LPVOID, SIZE_T, DWORD) noexcept __asm__("VirtualFree");
    extern "C" __declspec(dllimport) BOOL WINAPI flush_instruction_cache(HANDLE, LPCVOID, SIZE_T) noexcept __asm__("FlushInstructionCache");
    extern "C" __declspec(dllimport) BOOL WINAPI set_thread_context(HANDLE, CONTEXT const*) noexcept __asm__("SetThreadContext");
}

namespace
{
    ::std::atomic_bool poll_seen{};
    ::std::atomic_bool may_return{};
    ::std::atomic<::std::uint_least32_t> worker_tid{};
    ::std::atomic<unsigned> unrelated_traps{};
    ::std::uint64_t counter{};
    void* executable{};
    struct parked_control
    {
        ::std::atomic_bool ready{};
        ::std::atomic_bool stop{};
        ::std::atomic<::std::uint_least32_t> tid{};
    };

    [[noreturn]] void fail(char const* name) noexcept
    {
        // A callback can call check(). Reporting failure must never unwind
        // through a VEH, thread entry point or a native-step noexcept callback.
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            ::fast_io::io::perrln("FAIL Windows native-step RAII: ", ::fast_io::mnp::os_c_str(name));
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) {}
#endif
        ::fast_io::fast_terminate();
    }
    void check(bool condition, char const* name) noexcept
    { if(!condition) { fail(name); } }

    bool owners_empty(backend::session const& step) noexcept
    {
        return !step.target_owner && !step.first_gate_owner && !step.second_gate_owner &&
            step.target_handle == nullptr && step.first_gate == nullptr && step.second_gate == nullptr;
    }
    void check_owned_handles(backend::session const& step) noexcept
    {
        check(step.target_owner && step.first_gate_owner && step.second_gate_owner,
              "selected thread and both private gates have owners");
        check(step.target_owner.native_handle() == step.target_handle &&
              step.first_gate_owner.native_handle() == step.first_gate &&
              step.second_gate_owner.native_handle() == step.second_gate &&
              step.first_gate != step.second_gate,
              "VEH borrows exactly the three live RAII-owned handles");
        for(auto const handle : {step.target_handle, step.first_gate, step.second_gate})
        {
            ::std::uint_least32_t flags{};
            check(::fast_io::win32::GetHandleInformation(handle, ::std::addressof(flags)) != 0 &&
                  (flags & HANDLE_FLAG_INHERIT) == 0u,
                  "owned thread and gates are valid and noninheritable");
        }
    }
    void close_probe_handle(::fast_io::nt_file& owner) noexcept
    {
        check(static_cast<bool>(owner), "fatal probe owns handle before invalidating it");
        // One deliberate corruption in an isolated child. Release the owner
        // before NT close, so no later RAII destructor can double-close it.
        auto* const handle{owner.release()};
        check(::fast_io::win32::nt::nt_close<false>(handle) == 0u, "fatal probe NT close");
        check(!owner, "fatal probe owner is empty after one close");
    }
    void create_probe_gates(backend::session& step) noexcept
    {
        step.first_gate_owner.reset(control_abi::uwvm_CreateEventW(nullptr, TRUE, FALSE, nullptr));
        step.second_gate_owner.reset(control_abi::uwvm_CreateEventW(nullptr, FALSE, FALSE, nullptr));
        step.first_gate = step.first_gate_owner.native_handle();
        step.second_gate = step.second_gate_owner.native_handle();
        check(step.first_gate_owner && step.second_gate_owner, "fatal probe gate owners");
    }
    bool wait_for(backend::session const& step, backend::phase wanted) noexcept
    {
        auto const deadline{fixture_abi::tick_count() + 5000u};
        while(fixture_abi::tick_count() < deadline)
        {
            auto const state{step.state.load(::std::memory_order_acquire)};
            if(state == wanted) { return true; }
            if(state == backend::phase::failed) { return false; }
            ::fast_io::win32::Sleep(1u);
        }
        return false;
    }
    void wait_for_bridge(char const* name) noexcept
    {
        auto const deadline{fixture_abi::tick_count() + 5000u};
        while(!poll_seen.load(::std::memory_order_acquire) && fixture_abi::tick_count() < deadline)
        { ::fast_io::win32::Sleep(1u); }
        check(poll_seen.load(::std::memory_order_acquire), name);
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
    // Match fast_io::win32::CreateThread's exact callback return type. DWORD
    // can instead be unsigned long in a Windows SDK; no function-pointer cast
    // is used to hide that distinct C++ type.
    ::std::uint_least32_t WINAPI worker(void*) noexcept
    {
        worker_tid.store(::fast_io::win32::GetCurrentThreadId(), ::std::memory_order_release);
        auto const function{reinterpret_cast<void(*)(::std::uint64_t*)>(executable)};
        function(::std::addressof(counter));
        return 0u;
    }
    ::std::uint_least32_t WINAPI contender(void* parameter) noexcept
    {
        auto* const ready{static_cast<::std::atomic_bool*>(parameter)};
        while(!ready->load(::std::memory_order_acquire)) { ::fast_io::win32::SwitchToThread(); }
        auto const pc{reinterpret_cast<::std::uintptr_t>(executable) + 20u};
        if(backend::arm_at_return(pc)) { fail("cancelled/non-target arm refused"); }
        return 0u;
    }
    ::std::uint_least32_t WINAPI parked(void* parameter) noexcept
    {
        auto& control{*static_cast<parked_control*>(parameter)};
        control.tid.store(::fast_io::win32::GetCurrentThreadId(), ::std::memory_order_release);
        control.ready.store(true, ::std::memory_order_release);
        while(!control.stop.load(::std::memory_order_acquire)) { ::fast_io::win32::SwitchToThread(); }
        return 0u;
    }
    void set_hardware_breakpoint(void* thread, ::std::uintptr_t pc, bool enabled) noexcept
    {
        check(control_abi::uwvm_SuspendThread(thread) != (::std::numeric_limits<DWORD>::max)(),
              "SuspendThread breakpoint setup");
        ::CONTEXT context{};
        context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        check(control_abi::uwvm_GetThreadContext(thread, ::std::addressof(context)) != 0,
              "GetThreadContext breakpoint setup");
        context.Dr0 = pc;
        context.Dr7 = (context.Dr7 & ~0xffu) | (enabled ? 1u : 0u);
        check(fixture_abi::set_thread_context(thread, ::std::addressof(context)) != 0,
              "SetThreadContext breakpoint setup");
        ::CONTEXT verified{};
        verified.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        check(control_abi::uwvm_GetThreadContext(thread, ::std::addressof(verified)) != 0,
              "GetThreadContext breakpoint verification");
        check((verified.Dr7 & 1u) == (enabled ? 1u : 0u), "Dr7 breakpoint verification");
        // The caller still owns its thread handle; never close it while this
        // deliberately suspended target still requires a host resume.
        check(control_abi::uwvm_ResumeThread(thread) != (::std::numeric_limits<DWORD>::max)(),
              "ResumeThread breakpoint setup");
    }
}

extern "C" void native_step_test_poll() noexcept
{
    poll_seen.store(true, ::std::memory_order_release);
    while(!may_return.load(::std::memory_order_acquire)) { ::fast_io::win32::SwitchToThread(); }
}
extern "C" int native_step_test_arm(::std::uintptr_t pc) noexcept
{ return backend::arm_at_return(pc) ? 1 : 0; }

// Win64 shadow space and alignment. The final POPFQ/RET sets TF only for
// returning into authenticated code, before any C++ epilogue can consume it.
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
    auto const argument{argc == 2 ? ::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[1])} : ::fast_io::cstring_view{}};
    if(argument == "--invalid-gate")
    {
        backend::session broken{};
        check(backend::install(), "invalid-gate install");
        constexpr ::std::uintptr_t expected_pc{0x1800u};
        broken.target_thread = ::fast_io::win32::GetCurrentThreadId();
        broken.owner_begin = 0x1000u;
        broken.owner_end = 0x2000u;
        broken.expected_pc = expected_pc;
        create_probe_gates(broken);
        broken.state.store(backend::phase::arming, ::std::memory_order_release);
        backend::details::active.store(::std::addressof(broken), ::std::memory_order_release);
        backend::details::current_thread_session = ::std::addressof(broken);
        close_probe_handle(broken.first_gate_owner);
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
    if(argument == "--invalid-set-event")
    {
        backend::session broken{};
        check(backend::install(), "invalid-event install");
        create_probe_gates(broken);
        close_probe_handle(broken.first_gate_owner);
        broken.first_gate = INVALID_HANDLE_VALUE;
        broken.state.store(backend::phase::at_guest_pc, ::std::memory_order_release);
        backend::details::active.store(::std::addressof(broken), ::std::memory_order_release);
        static_cast<void>(backend::continue_one(broken));
        fail("invalid-event signal returned to guest");
    }
    if(argument == "--invalid-first-pc")
    {
        backend::session broken{};
        check(backend::install(), "invalid-pc install");
        broken.target_thread = ::fast_io::win32::GetCurrentThreadId();
        broken.owner_begin = 0x1000u;
        broken.owner_end = 0x2000u;
        broken.expected_pc = 0x1800u;
        create_probe_gates(broken);
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
    if(argument == "--invalid-live-destruction")
    {
        // This actual isolated process must terminate from session's destructor
        // before member owners can close a gate still reachable through active.
        {
            backend::session broken{};
            create_probe_gates(broken);
            broken.state.store(backend::phase::ready, ::std::memory_order_release);
            backend::details::active.store(::std::addressof(broken), ::std::memory_order_release);
        }
        fail("active session destructor returned after destroying live gates");
    }
    check(argc == 1, "only the four exact death-probe arguments are accepted");

    char self[MAX_PATH]{};
    auto const self_length{fixture_abi::module_path(nullptr, self, MAX_PATH)};
    check(self_length > 0u && self_length < MAX_PATH, "bounded GetModuleFileNameA");
    struct fatal_probe { char const* name; ::std::uint_least32_t expected_exit; };
    constexpr fatal_probe probes[]{
        {"--invalid-gate", backend::details::invalid_gate_exit_code},
        {"--invalid-set-event", backend::details::invalid_gate_exit_code},
        {"--invalid-first-pc", backend::details::invalid_pc_exit_code},
        {"--invalid-live-destruction", backend::details::invalid_gate_exit_code}
    };
    static_assert(sizeof(::fast_io::win32::startupinfoa) == sizeof(::STARTUPINFOA));
    static_assert(sizeof(::fast_io::win32::process_information) == sizeof(::PROCESS_INFORMATION));
    for(auto const& probe : probes)
    {
        // The reported path is bounded before forming its string view. Keep
        // the mutable fast_io-built command alive until CreateProcess returns.
        auto child_command{::fast_io::concat_std("\"", ::fast_io::basic_io_scatter_t<char>{self, self_length}, "\" ",
            ::fast_io::mnp::os_c_str(probe.name))};
        ::fast_io::win32::startupinfoa startup{};
        startup.cb = sizeof(startup);
        ::fast_io::win32::process_information child{};
        auto const created{::fast_io::win32::CreateProcessA(self, child_command.data(), nullptr, nullptr,
            FALSE, CREATE_NO_WINDOW, nullptr, nullptr, ::std::addressof(startup), ::std::addressof(child))};
        // Adopt both outputs before any check or potentially throwing output.
        ::fast_io::nt_file child_process{child.hProcess};
        ::fast_io::nt_file child_thread{child.hThread};
        check(created != 0 && child_process && child_thread, "CreateProcessA fatal child owners");
        check(::fast_io::win32::WaitForSingleObject(child_process.native_handle(), 5000u) == WAIT_OBJECT_0,
              "fatal child exited");
        ::std::uint_least32_t child_exit{};
        check(::fast_io::win32::GetExitCodeProcess(child_process.native_handle(), ::std::addressof(child_exit)) != 0 &&
              child_exit == probe.expected_exit, "fatal ownership failure terminated with exact exit code");
        ::fast_io::io::println("Windows RAII isolated death: ", ::fast_io::mnp::os_c_str(probe.name),
                              " exit=", ::fast_io::mnp::hex(child_exit));
    }

    check(backend::platform_available(), "x64 platform enabled");
    auto* const prior{control_abi::uwvm_AddVectoredExceptionHandler(0u, prior_handler)};
    check(prior != nullptr, "prior VEH");
    check(backend::install(), "install");

    constexpr ::std::size_t page_size{4096u};
    auto* const code{static_cast<::std::uint8_t*>(fixture_abi::virtual_alloc(nullptr, page_size,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))};
    check(code != nullptr, "VirtualAlloc RW");
    // Win64: push rbx; mov rbx,rcx; sub rsp,32; movabs rax,bridge; call rax;
    // incq (%rbx); add rsp,32; incq (%rbx); pop rbx; ret. The exact bridge
    // return is the first INC, before stack cleanup. No RWX mapping is created.
    constexpr ::std::uint8_t prefix[]{0x53, 0x48, 0x89, 0xcb, 0x48, 0x83, 0xec, 0x20, 0x48, 0xb8};
    constexpr ::std::uint8_t suffix[]{0xff, 0xd0, 0x48, 0xff, 0x03,
        0x48, 0x83, 0xc4, 0x20, 0x48, 0xff, 0x03, 0x5b, 0xc3};
    constexpr auto code_size{sizeof(prefix) + sizeof(::std::uintptr_t) + sizeof(suffix)};
    static_assert(code_size == 32u && code_size <= page_size);
    __builtin_memcpy(code, prefix, sizeof(prefix));
    auto const bridge{reinterpret_cast<::std::uintptr_t>(native_step_test_bridge)};
    // [owned allocation: page_size bytes] [prefix][bridge address][suffix]
    // [safe                             ] constexpr extents fit page_size;
    //  ^^ pointer advances below remain inside this one owned allocation.
    __builtin_memcpy(code + sizeof(prefix), ::std::addressof(bridge), sizeof(bridge));
    __builtin_memcpy(code + sizeof(prefix) + sizeof(bridge), suffix, sizeof(suffix));
    DWORD old_protection{};
    check(fixture_abi::virtual_protect(code, page_size, PAGE_EXECUTE_READ, ::std::addressof(old_protection)) != 0,
          "VirtualProtect RX");
    check(fixture_abi::flush_instruction_cache(control_abi::uwvm_GetCurrentProcess(), code, code_size) != 0,
          "FlushInstructionCache");
    executable = code;
    auto const owner_begin{reinterpret_cast<::std::uintptr_t>(code)};
    // [owned page] code_size=32 < page_size; 20 < code_size.
    // [safe      ] both pointers are formed only after VirtualAlloc succeeded.
    auto const owner_end{reinterpret_cast<::std::uintptr_t>(code + code_size)};
    auto const first{reinterpret_cast<::std::uintptr_t>(code + 20u)};

    backend::session denied{};
    check(owners_empty(denied), "default session has no kernel capability");
    check(!backend::request(denied, 1u, owner_begin, owner_end, owner_end) && owners_empty(denied),
          "owner end excluded and denied session owners remain empty");
    check(!backend::request(denied, ::fast_io::win32::GetCurrentThreadId(), owner_begin, owner_end, first) &&
          owners_empty(denied), "controller thread rejected without retaining handles");
    check(!backend::arm_at_return(first), "no active target");

    backend::session step{};
    ::fast_io::nt_file thread{::fast_io::win32::CreateThread(nullptr, 0u, worker, nullptr, 0u, nullptr)};
    check(static_cast<bool>(thread), "CreateThread worker owner");
    wait_for_bridge("worker reached bridge");
    auto const tid{worker_tid.load(::std::memory_order_acquire)};
    set_hardware_breakpoint(thread.native_handle(), first, true);
    backend::session breakpoint_denied{};
    check(!backend::request(breakpoint_denied, tid, owner_begin, owner_end, first) &&
          owners_empty(breakpoint_denied), "hardware breakpoint refusal leaves every owner empty");
    set_hardware_breakpoint(thread.native_handle(), first, false);
    backend::session preowned_denied{};
    preowned_denied.first_gate_owner.reset(control_abi::uwvm_CreateEventW(nullptr, TRUE, FALSE, nullptr));
    preowned_denied.first_gate = preowned_denied.first_gate_owner.native_handle();
    check(static_cast<bool>(preowned_denied.first_gate_owner), "preowned idle rejection input");
    auto* const original_gate{preowned_denied.first_gate};
    check(!backend::request(preowned_denied, tid, owner_begin, owner_end, first) &&
          preowned_denied.first_gate_owner.native_handle() == original_gate &&
          preowned_denied.first_gate == original_gate && !preowned_denied.target_owner &&
          !preowned_denied.second_gate_owner,
          "request refuses an already owned idle session without overwriting caller ownership");
    close_probe_handle(preowned_denied.first_gate_owner);
    preowned_denied.first_gate = nullptr;
    check(owners_empty(preowned_denied), "caller retires unpublished preowned session before destruction");
    check(backend::request(step, tid, owner_begin, owner_end, first), "authorized request");
    check_owned_handles(step);
    backend::session competing_denied{};
    check(!backend::request(competing_denied, tid, owner_begin, owner_end, first) &&
          owners_empty(competing_denied), "another active session refusal leaves every owner empty");
    check(!backend::arm_at_return(first), "wrong native thread refused");
    may_return.store(true, ::std::memory_order_release);
    auto const reached_first{wait_for(step, backend::phase::at_guest_pc)};
    if(!reached_first)
    {
        // A timed-out worker may still be publishing its non-atomic trap
        // fields. Only read atomic state and host-owned constants here.
        ::fast_io::io::perrln("first trap detail: state=", ::fast_io::mnp::dec(static_cast<unsigned>(step.state.load(::std::memory_order_acquire))),
            " expected=", ::fast_io::mnp::hex(first), " tid=", ::fast_io::mnp::dec(tid));
    }
    check(reached_first, "first trap at guest PC");
    check_owned_handles(step);
    check(step.first_pc == first && counter == 0u && step.first_exception_code == EXCEPTION_SINGLE_STEP,
          "zero guest instructions at first trap");
    check(!backend::clear(step), "cannot close owners while first VEH gate is parked");
    check_owned_handles(step);
    unsigned register_callbacks{};
    check(!backend::with_owned_registers(reinterpret_cast<void const*>(::std::uintptr_t{1}),
        [&](auto...) noexcept { ++register_callbacks; }), "forged register session rejected before dereference");
    check(!backend::with_owned_registers(::std::addressof(step),
        [&](auto...) noexcept { ++register_callbacks; }), "first gate is not a completed register stop");
    check(register_callbacks == 0u, "unauthorized register callback never called");
    check(backend::continue_one(step), "first instruction resume");
    check(wait_for(step, backend::phase::trapped), "single machine instruction trap");
    check_owned_handles(step);
    check(step.next_pc == first + 3u && counter == 1u && step.second_exception_code == EXCEPTION_SINGLE_STEP,
          "exactly one guest instruction");
    ::uwvm2::uwvm::debugger::native_registers::snapshot copied_registers{};
    check(backend::with_owned_registers(::std::addressof(step),
        [&](auto actual_thread, auto actual_pc, auto actual_begin, auto actual_end, auto const& registers) noexcept
        {
            check(actual_thread == tid && actual_pc == first + 3u && actual_begin == owner_begin && actual_end == owner_end,
                  "register response belongs to exact selected thread and code range");
            copied_registers = registers;
            ++register_callbacks;
        }), "selected real native trap registers");
    check(copied_registers.size() == 18u && copied_registers.pc() == first + 3u && copied_registers.sp() != 0u &&
          copied_registers.values[1u] == reinterpret_cast<::std::uintptr_t>(::std::addressof(counter)) &&
          (copied_registers.values[17u] & 0x100u) == 0u && register_callbacks == 1u,
          "real RBX, SP, RIP and debugger-normalized RFLAGS");
    check(!backend::clear(step), "cannot close owners while second VEH gate is parked");
    check_owned_handles(step);
    check(backend::continue_from_trap(step), "second instruction resume");
    check(wait_for(step, backend::phase::trapped), "second machine instruction trap");
    check(step.next_pc == first + 7u && counter == 1u && step.second_exception_code == EXCEPTION_SINGLE_STEP,
          "exactly two guest instructions");
    ::fast_io::io::println("Windows single-step context: first Dr6=", ::fast_io::mnp::hex(step.first_dr6),
        " EFlags=", ::fast_io::mnp::hex(step.first_eflags), "; second Dr6=", ::fast_io::mnp::hex(step.second_dr6),
        " EFlags=", ::fast_io::mnp::hex(step.second_eflags));
    ::fast_io::win32::RaiseException(EXCEPTION_SINGLE_STEP, 0u, 0u, nullptr);
    check(unrelated_traps.load(::std::memory_order_acquire) == 1u, "non-target VEH chaining");
    check(counter == 1u, "target stayed stopped during non-target trap");
    check(backend::release(step), "second-trap release");
    check(::fast_io::win32::WaitForSingleObject(thread.native_handle(), 5000u) == WAIT_OBJECT_0, "worker joined");
    check(counter == 2u && step.state.load(::std::memory_order_acquire) == backend::phase::released,
          "release executes second guest increment");
    check_owned_handles(step);
    check(backend::clear(step) && owners_empty(step), "clear revokes all three owners and borrow fields");
    check(!backend::with_owned_registers(::std::addressof(step),
        [&](auto...) noexcept { ++register_callbacks; }) && register_callbacks == 1u && copied_registers.size() == 18u,
        "retired session rejected while owned register value copy remains available");

    poll_seen.store(false, ::std::memory_order_release);
    may_return.store(false, ::std::memory_order_release);
    counter = 0u;
    backend::session cancelled_at_first{};
    ::fast_io::nt_file cancelled_worker{::fast_io::win32::CreateThread(nullptr, 0u, worker, nullptr, 0u, nullptr)};
    check(static_cast<bool>(cancelled_worker), "CreateThread first-stop cancel owner");
    wait_for_bridge("first-stop cancel worker ready");
    check(backend::request(cancelled_at_first, worker_tid.load(::std::memory_order_acquire),
        owner_begin, owner_end, first), "first-stop cancel request");
    check_owned_handles(cancelled_at_first);
    may_return.store(true, ::std::memory_order_release);
    check(wait_for(cancelled_at_first, backend::phase::at_guest_pc), "first-stop cancel observed");
    check(backend::release(cancelled_at_first), "first-trap release");
    check(::fast_io::win32::WaitForSingleObject(cancelled_worker.native_handle(), 5000u) == WAIT_OBJECT_0,
          "first-stop cancel worker joined");
    check(counter == 2u && cancelled_at_first.state.load(::std::memory_order_acquire) == backend::phase::released,
          "first-stop cancellation completes without TF");
    check_owned_handles(cancelled_at_first);
    check(backend::clear(cancelled_at_first) && owners_empty(cancelled_at_first),
          "first-stop cancellation closes every owner only after released");

    // A ready cancellation races a non-target return shim. Each session's
    // destruction follows clear(), which must leave no owner or borrowed handle.
    parked_control parked_state{};
    ::fast_io::nt_file parked_thread{::fast_io::win32::CreateThread(nullptr, 0u, parked,
        ::std::addressof(parked_state), 0u, nullptr)};
    check(static_cast<bool>(parked_thread), "CreateThread parked target owner");
    auto const parked_deadline{fixture_abi::tick_count() + 5000u};
    while(!parked_state.ready.load(::std::memory_order_acquire) && fixture_abi::tick_count() < parked_deadline)
    { ::fast_io::win32::Sleep(1u); }
    check(parked_state.ready.load(::std::memory_order_acquire), "parked target ready");
    for(unsigned round{}; round != 256u; ++round)
    {
        auto cancelled{::std::make_unique<backend::session>()};
        check(backend::request(*cancelled, parked_state.tid.load(::std::memory_order_acquire),
            owner_begin, owner_end, first), "cancel-ready request");
        check_owned_handles(*cancelled);
        ::std::atomic_bool go{};
        ::fast_io::nt_file contender_thread{::fast_io::win32::CreateThread(nullptr, 0u, contender,
            ::std::addressof(go), 0u, nullptr)};
        check(static_cast<bool>(contender_thread), "CreateThread contender owner");
        go.store(true, ::std::memory_order_release);
        check(backend::clear(*cancelled) && owners_empty(*cancelled), "ready cancellation revokes owners under arm race");
        cancelled.reset();
        check(::fast_io::win32::WaitForSingleObject(contender_thread.native_handle(), 5000u) == WAIT_OBJECT_0,
              "contender joined before its owner closes");
    }
    parked_state.stop.store(true, ::std::memory_order_release);
    check(::fast_io::win32::WaitForSingleObject(parked_thread.native_handle(), 5000u) == WAIT_OBJECT_0,
          "parked target joined before its owner closes");
    check(backend::uninstall(), "uninstall after all owned sessions cleared");
    ::fast_io::win32::RaiseException(EXCEPTION_SINGLE_STEP, 0u, 0u, nullptr);
    check(unrelated_traps.load(::std::memory_order_acquire) == 2u, "prior VEH retained after uninstall");
    check(control_abi::uwvm_RemoveVectoredExceptionHandler(prior) != 0u, "remove prior VEH");
    check(fixture_abi::virtual_free(code, 0u, MEM_RELEASE) != 0, "VirtualFree after worker joins");
    ::fast_io::io::println("PASS Windows x64 native-step RAII: real zero/one/two instruction traps, owned GPR copy, exact handle mirrors, failed-request emptiness, clear retirement, 256 ready races and four isolated ownership deaths");
}
