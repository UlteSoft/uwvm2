/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <atomic>
# include <cstdint>
# include "native_registers.h"
# include <limits>
# include <memory>
# include <utility>
# if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
#  include <fast_io.h>
#  include <windows.h>
#  include <uwvm2/utils/control/win32_abi.h>
#  if defined(_MSC_VER) && !defined(__clang__)
#   include <intrin.h>
#  endif
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

// This adapter intentionally has no dependency on the Linux backend. Its
// persistent two-trap protocol matches the shared host controller exactly.
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_step_windows
{
    enum class phase : ::std::uint32_t
    {
        idle, ready, arming, at_guest_pc, executing, trapped,
        releasing, released, failed, continuation_running, continuation_escape, continuation_cancel_pending
    };
    struct session
    {
        ::std::atomic<phase> state{phase::idle};
        ::std::atomic_bool abort_requested{};
        ::std::uint_least64_t target_thread{};
        ::std::uintptr_t owner_begin{}, owner_end{}, expected_pc{};
        ::std::uintptr_t first_pc{}, next_pc{};
        native_registers::snapshot register_snapshot{};
        ::std::uint64_t trap_revision{};
        ::std::uintptr_t continuation_pc{}, continuation_origin_sp{};
        ::std::atomic_bool continuation_disarmed{true};
        bool retained_entry_stop{}; // Host guard; first real trap remains on its original gate.
        ::std::uint_least64_t first_dr6{}, second_dr6{};
        ::std::uint32_t first_eflags{}, second_eflags{};
        ::std::uint32_t first_exception_code{}, second_exception_code{};
        // Pins the selected native thread identity until clear(). The thread
        // must be cooperatively parked at the trusted bridge during request().
        void* target_handle{}; // Borrow only; target_owner owns the kernel handle.
        // Created by the host before the session is published. They have no
        // name, no inheritable handle and no guest-visible capability.
        void* first_gate{}, *second_gate{}; // VEH borrows: it never closes or moves them.
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
        // Adopt exact host-created kernel handles. No filesystem open/duplicate,
        // inheritance or guest-visible name is introduced by these owners.
        // The controller retains this entire session until clear() observes
        // ready/released; destructing a live trapped session is unsupported.
        ::fast_io::nt_file target_owner{}, first_gate_owner{}, second_gate_owner{};
        ::CONTEXT original_debug{};
        ~session() noexcept;
#endif
    };
    static_assert(::std::atomic<phase>::is_always_lock_free);
    static_assert(::std::atomic_bool::is_always_lock_free);
    static_assert(::std::atomic<session*>::is_always_lock_free);

#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
    namespace details
    {
        inline ::std::atomic<session*> active{};
        inline constinit thread_local session* current_thread_session{};
        inline void* handler_handle{};
        inline ::std::atomic_bool installed{};
        // Only host operations take this lock. The VEH callback cannot wait
        // for a host operation, especially one cancelling a ready request.
        inline ::std::atomic_flag host_transition = ATOMIC_FLAG_INIT;
        struct host_transition_guard
        {
            host_transition_guard() noexcept
            { while(host_transition.test_and_set(::std::memory_order_acquire)) {} }
            host_transition_guard(host_transition_guard const&) = delete;
            host_transition_guard& operator=(host_transition_guard const&) = delete;
            ~host_transition_guard() { host_transition.clear(::std::memory_order_release); }
        };
        inline constexpr ::std::uint32_t trap_flag{0x100u};

        inline void capture_registers(native_registers::snapshot& out, ::CONTEXT const& context) noexcept
        {
            out.machine = native_registers::architecture::x86_64;
            // [kernel/VEH-supplied complete x64 CONTEXT] [owned fixed 34-register snapshot]
            // [safe                                  ] read named SDK scalar fields only;
            //  ^^ captured addresses are display values, never native-memory observers.
            out.values = {context.Rax, context.Rbx, context.Rcx, context.Rdx, context.Rsi, context.Rdi,
                context.Rbp, context.Rsp, context.R8, context.R9, context.R10, context.R11,
                context.R12, context.R13, context.R14, context.R15, context.Rip,
                static_cast<::std::uint64_t>(context.EFlags) & ~static_cast<::std::uint64_t>(trap_flag)};
        }

        inline constexpr ::std::uint32_t invalid_gate_exit_code{0xE0000DB6u};
        inline constexpr ::std::uint32_t invalid_pc_exit_code{0xE0000DB7u};

        [[noreturn]] inline void fail_with_code(::std::uint32_t code) noexcept
        {
            // A broken gate or a bridge landing at an unexpected PC is a
            // trusted-host invariant failure. Guest execution cannot resume.
            ::fast_io::win32::TerminateProcess(::uwvm2::utils::control::win32_abi::uwvm_GetCurrentProcess(), code);
#if defined(_MSC_VER) && !defined(__clang__)
            __fastfail(FAST_FAIL_FATAL_APP_EXIT);
#else
            __builtin_trap();
#endif
        }
        [[noreturn]] inline void fail_invalid_gate() noexcept
        { fail_with_code(invalid_gate_exit_code); }
        [[noreturn]] inline void fail_invalid_pc() noexcept
        { fail_with_code(invalid_pc_exit_code); }
        inline void wait_for_gate(void* gate) noexcept
        {
            // INVALID_HANDLE_VALUE is -1, also a Win32 pseudo-handle for the
            // current process. Waiting on it would hang forever instead of
            // reporting the corrupted session handle.
            if(gate == nullptr || gate == INVALID_HANDLE_VALUE)
            { fail_invalid_gate(); }
            if(::fast_io::win32::WaitForSingleObject(static_cast<HANDLE>(gate), INFINITE) != WAIT_OBJECT_0)
            { fail_invalid_gate(); }
        }
        inline void close_owned_kernel_handle(::fast_io::nt_file& owner) noexcept
        {
            if(!owner) { return; }
            // Transfer exactly once out of the RAII owner before the fast_io
            // error-return NT close. A failure remains a trusted-host fatal;
            // no throwing file close can run inside VEH/cancellation paths.
            auto* const handle{owner.release()};
            if(::fast_io::win32::nt::nt_close<false>(handle) != 0u) { fail_invalid_gate(); }
        }
        struct suspended_target
        {
            ::fast_io::nt_file owner{};
            HANDLE handle{}; // Borrow from owner until successful session transfer.
            bool suspended{};
            explicit suspended_target(DWORD id) noexcept
            {
                owner.reset(::uwvm2::utils::control::win32_abi::uwvm_OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT |
                    THREAD_QUERY_LIMITED_INFORMATION, FALSE, id));
                handle = owner.native_handle();
                if(handle != nullptr && ::uwvm2::utils::control::win32_abi::uwvm_GetProcessIdOfThread(handle) == ::fast_io::win32::GetCurrentProcessId() &&
                   ::uwvm2::utils::control::win32_abi::uwvm_SuspendThread(handle) != (::std::numeric_limits<DWORD>::max)())
                { suspended = true; }
            }
            suspended_target(suspended_target const&) = delete;
            suspended_target& operator=(suspended_target const&) = delete;
            ~suspended_target()
            {
                if(handle != nullptr)
                {
                    if(suspended && ::uwvm2::utils::control::win32_abi::uwvm_ResumeThread(handle) == (::std::numeric_limits<DWORD>::max)())
                    { fail_invalid_gate(); }
                    close_owned_kernel_handle(owner);
                }
            }
        };

        // The first exception is caused by TF on the return instruction of a
        // dedicated assembly bridge. No guest instruction has executed yet.
        // The second exception is caused by TF on exactly one guest instruction.
        // Unrelated exceptions naturally chain to other VEH/SEH handlers.
        [[nodiscard]] inline LONG CALLBACK veh_handler(::EXCEPTION_POINTERS* pointers) noexcept
        {
            if(pointers == nullptr || pointers->ExceptionRecord == nullptr ||
               pointers->ContextRecord == nullptr ||
               pointers->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
            { return EXCEPTION_CONTINUE_SEARCH; }
            auto* const step{current_thread_session};
            if(step == nullptr ||
               active.load(::std::memory_order_acquire) != step ||
               static_cast<::std::uint_least64_t>(::fast_io::win32::GetCurrentThreadId()) != step->target_thread)
            { return EXCEPTION_CONTINUE_SEARCH; }

            auto* const context{pointers->ContextRecord};
            auto const pc{static_cast<::std::uintptr_t>(context->Rip)};
            auto const state{step->state.load(::std::memory_order_relaxed)};
            if(state == phase::arming)
            {
                step->first_pc = pc;
                capture_registers(step->register_snapshot, *context);
                if(++step->trap_revision==0u) { fail_invalid_gate(); }
                step->first_dr6 = context->Dr6;
                step->first_eflags = context->EFlags;
                step->first_exception_code = pointers->ExceptionRecord->ExceptionCode;
                context->EFlags &= ~trap_flag;
                if(pc != step->expected_pc || pc < step->owner_begin || pc >= step->owner_end)
                { fail_invalid_pc(); }
                if(step->abort_requested.load(::std::memory_order_acquire))
                {
                    current_thread_session = nullptr;
                    step->state.store(phase::released, ::std::memory_order_release);
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
                step->state.store(phase::at_guest_pc, ::std::memory_order_release);
                wait_for_gate(step->first_gate);
                auto const resumed_state{step->state.load(::std::memory_order_acquire)};
                if(resumed_state == phase::releasing ||
                   step->abort_requested.load(::std::memory_order_acquire))
                {
                    current_thread_session = nullptr;
                    step->state.store(phase::released, ::std::memory_order_release);
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
                if(resumed_state == phase::continuation_running)
                { context->Dr0=step->continuation_pc;context->Dr6=0u;context->Dr7=(context->Dr7 & ~0xf000fu) | 1u;return EXCEPTION_CONTINUE_EXECUTION; }
                if(resumed_state != phase::executing) { fail_invalid_gate(); }
                context->EFlags |= trap_flag;
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            if(state == phase::executing || state == phase::continuation_running || state == phase::continuation_escape || state == phase::continuation_cancel_pending)
            {
                bool const continuation{state!=phase::executing};bool const escape{state==phase::continuation_escape};
                if(continuation && !escape)
                {
                    if(pc!=step->continuation_pc || context->Dr0!=pc || (context->Dr7&1u)==0u ||
                       pc<step->owner_begin || pc>=step->owner_end) { return EXCEPTION_CONTINUE_SEARCH; }
                    context->Dr0=step->original_debug.Dr0;context->Dr6=step->original_debug.Dr6;context->Dr7=step->original_debug.Dr7;
                    if(!step->abort_requested.load(::std::memory_order_acquire) && context->Rsp!=step->continuation_origin_sp)
                    {
                        if(context->Rsp>=step->continuation_origin_sp) { fail_invalid_pc(); }
                        step->state.store(phase::continuation_escape,::std::memory_order_release);context->EFlags|=trap_flag;
                        return EXCEPTION_CONTINUE_EXECUTION;
                    }
                }
                if(escape && !step->abort_requested.load(::std::memory_order_acquire))
                {
                    if(pc<step->owner_begin || pc>=step->owner_end) { fail_invalid_pc(); }
                    context->EFlags&=~trap_flag;context->Dr0=step->continuation_pc;context->Dr6=0u;context->Dr7=(context->Dr7&~0xf000fu)|1u;
                    step->state.store(phase::continuation_running,::std::memory_order_release);return EXCEPTION_CONTINUE_EXECUTION;
                }
                step->next_pc = pc;
                capture_registers(step->register_snapshot, *context);
                if(++step->trap_revision==0u) { fail_invalid_gate(); }
                step->second_dr6 = context->Dr6;
                step->second_eflags = context->EFlags;
                step->second_exception_code = pointers->ExceptionRecord->ExceptionCode;
                context->EFlags &= ~trap_flag;
                if(step->abort_requested.load(::std::memory_order_acquire))
                {
                    current_thread_session = nullptr;
                    step->state.store(phase::released, ::std::memory_order_release);
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
                step->state.store(phase::trapped, ::std::memory_order_release);
                wait_for_gate(step->second_gate);
                auto const resumed_state{step->state.load(::std::memory_order_acquire)};
                if(resumed_state == phase::executing &&
                   !step->abort_requested.load(::std::memory_order_acquire))
                {
                    // The auto-reset gate has closed after waking this VEH.
                    // One later host command owns exactly the next instruction.
                    context->EFlags |= trap_flag;
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
                if(resumed_state==phase::continuation_running && !step->abort_requested.load(::std::memory_order_acquire))
                { context->Dr0=step->continuation_pc;context->Dr6=0u;context->Dr7=(context->Dr7&~0xf000fu)|1u;return EXCEPTION_CONTINUE_EXECUTION; }
                if(resumed_state != phase::releasing && resumed_state != phase::executing)
                { fail_invalid_gate(); }
                current_thread_session = nullptr;
                step->state.store(phase::released, ::std::memory_order_release);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }

    [[nodiscard]] inline constexpr bool platform_available() noexcept { return true; }

    inline session::~session() noexcept
    {
        // An active session is itself a capability borrowed by the VEH. Its
        // owner must drain/release/clear before destruction, including host
        // exception exits. Closing live gates cannot safely replace that drain.
        // Fail closed before any member-owner destructor can close a live gate
        // or leave active/current_thread_session pointing at freed storage.
        if(state.load(::std::memory_order_acquire) != phase::idle ||
           target_owner || first_gate_owner || second_gate_owner) { details::fail_invalid_gate(); }
    }

    // These are cold host-only operations, admitted only by LLVM-full debug.
    [[nodiscard]] inline bool install() noexcept
    {
        details::host_transition_guard guard{};
        if(details::installed.load(::std::memory_order_acquire)) { return true; }
        auto* const handler{::uwvm2::utils::control::win32_abi::uwvm_AddVectoredExceptionHandler(1u, details::veh_handler)};
        if(handler == nullptr) { return false; }
        details::handler_handle = handler;
        details::installed.store(true, ::std::memory_order_release);
        return true;
    }
    [[nodiscard]] inline bool uninstall() noexcept
    {
        details::host_transition_guard guard{};
        if(details::active.load(::std::memory_order_acquire) != nullptr) { return false; }
        if(!details::installed.load(::std::memory_order_acquire)) { return true; }
        if(::uwvm2::utils::control::win32_abi::uwvm_RemoveVectoredExceptionHandler(details::handler_handle) == 0u) { return false; }
        details::handler_handle = nullptr;
        details::installed.store(false, ::std::memory_order_release);
        return true;
    }
    [[nodiscard]] inline bool request(session& step, ::std::uint_least64_t native_thread,
                                      ::std::uintptr_t owner_begin, ::std::uintptr_t owner_end,
                                      ::std::uintptr_t return_pc) noexcept
    {
        if(native_thread == 0u ||
           native_thread > (::std::numeric_limits<::std::uint32_t>::max)() ||
           native_thread == ::fast_io::win32::GetCurrentThreadId() ||
           owner_begin == 0u || owner_begin >= owner_end ||
           return_pc < owner_begin || return_pc >= owner_end ||
           step.target_owner || step.first_gate_owner || step.second_gate_owner ||
           step.state.load(::std::memory_order_acquire) != phase::idle ||
           !details::installed.load(::std::memory_order_acquire) ||
           details::active.load(::std::memory_order_acquire) != nullptr) { return false; }
        // Windows does not populate DR6.BS in the VEH CONTEXT for an observed
        // TF trap (both real test traps report DR6 == 0). The host therefore
        // checks DR7 while this cooperatively parked worker is suspended and
        // rejects any active hardware breakpoint before publishing the step.
        // A guest cannot change debug registers; an external native debugger
        // that mutates this thread afterward already controls the process.
        details::suspended_target target{static_cast<DWORD>(native_thread)};
        ::CONTEXT debug_context{};
        debug_context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if(!target.suspended || ::uwvm2::utils::control::win32_abi::uwvm_GetThreadContext(target.handle, ::std::addressof(debug_context)) == 0 ||
           (debug_context.Dr7 & 0xffu) != 0u) { return false; }
        details::host_transition_guard guard{};
        if(!details::installed.load(::std::memory_order_acquire) ||
           step.state.load(::std::memory_order_acquire) != phase::idle ||
           details::active.load(::std::memory_order_acquire) != nullptr) { return false; }
        ::fast_io::nt_file first_owner{::uwvm2::utils::control::win32_abi::uwvm_CreateEventW(nullptr, TRUE, FALSE, nullptr)};
        if(!first_owner) { return false; }
        ::fast_io::nt_file second_owner{::uwvm2::utils::control::win32_abi::uwvm_CreateEventW(nullptr, FALSE, FALSE, nullptr)};
        if(!second_owner) { details::close_owned_kernel_handle(first_owner); return false; }
        auto* const first{first_owner.native_handle()};
        auto* const second{second_owner.native_handle()};
        step.original_debug = debug_context;
        step.continuation_disarmed.store(true,::std::memory_order_relaxed);
        step.target_thread = native_thread;
        step.owner_begin = owner_begin;
        step.owner_end = owner_end;
        step.expected_pc = return_pc;
        step.first_pc = step.next_pc = 0u; step.retained_entry_stop = false;
        step.register_snapshot = {};
        step.first_dr6 = step.second_dr6 = 0u;
        step.first_eflags = step.second_eflags = 0u;
        step.first_exception_code = step.second_exception_code = 0u;
        step.abort_requested.store(false, ::std::memory_order_relaxed);
        step.target_handle = target.handle;
        step.first_gate = first;
        step.second_gate = second;
        step.first_gate_owner = ::std::move(first_owner);
        step.second_gate_owner = ::std::move(second_owner);
        step.state.store(phase::ready, ::std::memory_order_release);
        session* expected{};
        if(!details::active.compare_exchange_strong(expected, ::std::addressof(step),
            ::std::memory_order_acq_rel, ::std::memory_order_acquire))
        {
            step.state.store(phase::idle, ::std::memory_order_release);
            step.target_handle = nullptr;
            step.first_gate = step.second_gate = nullptr;
            details::close_owned_kernel_handle(step.first_gate_owner);
            details::close_owned_kernel_handle(step.second_gate_owner);
            return false;
        }
        // Transfer only after successful publication. On CAS failure the
        // suspended_target still owns/resumes/closes the selected thread.
        step.target_owner = ::std::move(target.owner);
        if(::uwvm2::utils::control::win32_abi::uwvm_ResumeThread(target.handle) == (::std::numeric_limits<DWORD>::max)())
        { details::fail_invalid_gate(); }
        target.suspended = false;
        target.handle = nullptr;
        return true;
    }
    [[nodiscard]] inline bool arm_at_return(::std::uintptr_t return_pc) noexcept
    {
        if(details::active.load(::std::memory_order_acquire) == nullptr) { return false; }
        details::host_transition_guard guard{};
        auto* const step{details::active.load(::std::memory_order_acquire)};
        if(step == nullptr || return_pc != step->expected_pc ||
           return_pc < step->owner_begin || return_pc >= step->owner_end ||
           static_cast<::std::uint_least64_t>(::fast_io::win32::GetCurrentThreadId()) != step->target_thread)
        { return false; }
        auto expected{phase::ready};
        if(!step->state.compare_exchange_strong(expected, phase::arming,
            ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return false; }
        details::current_thread_session = step;
        return true;
    }
    // Publish a zero-instruction native stop only from the actual backend's
    // first kernel trap. A supplied PC/session label cannot manufacture it.
    [[nodiscard]] inline bool retain_initial_trap(session& step) noexcept
    {
        details::host_transition_guard guard{};
        if(details::active.load(::std::memory_order_acquire) != ::std::addressof(step) ||
           step.state.load(::std::memory_order_acquire) != phase::at_guest_pc ||
           step.abort_requested.load(::std::memory_order_acquire) || step.retained_entry_stop ||
           step.target_thread == 0u || step.first_pc != step.expected_pc ||
           step.first_pc < step.owner_begin || step.first_pc >= step.owner_end ||
           step.register_snapshot.size() == 0u || step.register_snapshot.pc() != step.first_pc ||
           step.register_snapshot.sp() == 0u) { return false; }
        step.next_pc = step.first_pc;
        step.retained_entry_stop = true;
        return true; // No gate wake, TF change, instruction execution or register write.
    }
    [[nodiscard]] inline bool continue_one(session& step) noexcept
    {
        details::host_transition_guard guard{};
        if(details::active.load(::std::memory_order_acquire) != ::std::addressof(step)) { return false; }
        auto expected{phase::at_guest_pc};
        if(!step.state.compare_exchange_strong(expected, phase::executing,
            ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return false; }
        if(::uwvm2::utils::control::win32_abi::uwvm_SetEvent(step.first_gate) == 0) { details::fail_invalid_gate(); }
        return true;
    }
    // The second VEH remains parked after the first instruction. A repeated
    // request re-arms TF in that same saved context, without running a C++
    // epilogue or another bridge instruction between debugger commands.
    [[nodiscard]] inline bool continue_from_trap(session& step) noexcept
    {
        details::host_transition_guard guard{};
        if(details::active.load(::std::memory_order_acquire) != ::std::addressof(step))
        { return false; }
        if(step.next_pc < step.owner_begin || step.next_pc >= step.owner_end) { return false; }
        auto expected{step.state.load(::std::memory_order_acquire)};
        bool const first{expected == phase::at_guest_pc};
        if(expected != phase::trapped && !(first && step.retained_entry_stop)) { return false; }
        if(!step.state.compare_exchange_strong(expected, phase::executing,
            ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return false; }
        step.first_pc = step.next_pc;
        step.expected_pc = step.next_pc;
        if(::uwvm2::utils::control::win32_abi::uwvm_SetEvent(first ? step.first_gate : step.second_gate) == 0) { details::fail_invalid_gate(); }
        return true;
    }
    [[nodiscard]] inline bool continue_to_continuation(session& step, ::std::uintptr_t pc) noexcept
    {
        details::host_transition_guard guard{};
        auto const state{step.state.load(::std::memory_order_acquire)};
        bool const first{state==phase::at_guest_pc && step.retained_entry_stop};
        if(details::active.load(::std::memory_order_acquire)!=&step || (state!=phase::trapped && !first) ||
           step.abort_requested.load(::std::memory_order_acquire) || pc<=step.next_pc || pc>=step.owner_end || step.register_snapshot.sp()==0u) { return false; }
        step.continuation_pc=pc;step.continuation_origin_sp=step.register_snapshot.sp();step.first_pc=step.next_pc;
        step.continuation_disarmed.store(false,::std::memory_order_release);
        step.state.store(phase::continuation_running,::std::memory_order_release);
        if(::uwvm2::utils::control::win32_abi::uwvm_SetEvent(first ? step.first_gate : step.second_gate)==0) { details::fail_invalid_gate(); }
        return true;
    }
    [[nodiscard]] inline bool skip_pause_for_continuation() noexcept
    {
        auto* const step{details::current_thread_session};
        if(step==nullptr || details::active.load(::std::memory_order_acquire)!=step) { return false; }
        auto const state{step->state.load(::std::memory_order_acquire)};
        if(state==phase::continuation_running && !step->abort_requested.load(::std::memory_order_acquire)) { return true; }
        if(state!=phase::continuation_cancel_pending) { return false; }
        // The manager first suspends this actual thread and removes DR0. Keep
        // its fixed session alive until the worker acknowledges that removal.
        while(!step->continuation_disarmed.load(::std::memory_order_acquire)) {}
        details::current_thread_session=nullptr;
        step->state.store(phase::released,::std::memory_order_release);return false;
    }
    inline void cancel_continuation_on_worker() noexcept
    {
        auto* const step{details::current_thread_session};
        if(step==nullptr || details::active.load(::std::memory_order_acquire)!=step) { return; }
        details::host_transition_guard guard{};
        if(step->state.load(::std::memory_order_acquire)==phase::continuation_running)
        { step->abort_requested.store(true,::std::memory_order_release);step->state.store(phase::continuation_cancel_pending,::std::memory_order_release); }
    }
    inline void acknowledge_continuation_execution_exit() noexcept
    { cancel_continuation_on_worker();static_cast<void>(skip_pause_for_continuation()); }
    [[nodiscard]] inline bool release(session& step) noexcept
    {
        details::host_transition_guard guard{};
        if(details::active.load(::std::memory_order_acquire) != ::std::addressof(step))
        { return false; }
        auto state{step.state.load(::std::memory_order_acquire)};
        if(state==phase::continuation_running || state==phase::continuation_cancel_pending)
        {
            details::suspended_target target{static_cast<DWORD>(step.target_thread)};
            if(!target.suspended) { return false; }
            state=step.state.load(::std::memory_order_acquire);
            if(state==phase::continuation_running || state==phase::continuation_cancel_pending)
            {
                ::CONTEXT debug{};debug.ContextFlags=CONTEXT_DEBUG_REGISTERS;
                if(!::uwvm2::utils::control::win32_abi::uwvm_GetThreadContext(target.handle,&debug)) { return false; }
                debug.Dr0=step.original_debug.Dr0;debug.Dr6=step.original_debug.Dr6;debug.Dr7=step.original_debug.Dr7;
                if(!::uwvm2::utils::control::win32_abi::uwvm_SetThreadContext(target.handle,&debug)) { return false; }
                step.abort_requested.store(true,::std::memory_order_release);
                step.state.store(phase::continuation_cancel_pending,::std::memory_order_release);
                step.continuation_disarmed.store(true,::std::memory_order_release);return true;
            }
        }
        if(state == phase::arming || state == phase::executing || state==phase::continuation_escape)
        {
            // The target can be between publishing its trap and entering a
            // gate wait. Signal both private gates so a cancellation in that
            // interval cannot strand the VEH. The handler also checks the
            // abort bit after waking, before re-arming TF.
            step.abort_requested.store(true, ::std::memory_order_release);
            if(::uwvm2::utils::control::win32_abi::uwvm_SetEvent(step.first_gate) == 0 || ::uwvm2::utils::control::win32_abi::uwvm_SetEvent(step.second_gate) == 0)
            { details::fail_invalid_gate(); }
            return true;
        }
        if(state == phase::at_guest_pc || state == phase::trapped || state == phase::failed)
        {
            step.abort_requested.store(true, ::std::memory_order_release);
            step.state.store(phase::releasing, ::std::memory_order_release);
            if(::uwvm2::utils::control::win32_abi::uwvm_SetEvent(step.first_gate) == 0 || ::uwvm2::utils::control::win32_abi::uwvm_SetEvent(step.second_gate) == 0)
            { details::fail_invalid_gate(); }
            return true;
        }
        return state == phase::releasing || state == phase::released;
    }
    // Cold host inspection only. Compare the supplied identity with the REAL
    // active session before dereferencing it. Keep host-transition ownership
    // through the bounded callback so release/continue cannot open its gate.
    // This copies integer metadata; the callback must not re-enter native-step.
    template<typename Inspect>
    [[nodiscard]] inline bool with_owned_trap(void const* identity, Inspect inspect) noexcept
    {
        details::host_transition_guard guard{};
        auto const* step{details::active.load(::std::memory_order_acquire)};
        if(step == nullptr || static_cast<void const*>(step) != identity) { return false; }
        // [actual active controller-owned session] host-transition guard
        // [safe                                 ] excludes release/clear;
        //  ^^ identity was compared before any supplied object was read.
        if((step->state.load(::std::memory_order_acquire) != phase::trapped &&
            !(step->state.load(::std::memory_order_acquire) == phase::at_guest_pc && step->retained_entry_stop)) || step->target_thread == 0u ||
           step->owner_begin == 0u || step->owner_end <= step->owner_begin ||
           step->next_pc < step->owner_begin || step->next_pc >= step->owner_end) { return false; }
        inspect(step->target_thread, step->next_pc, step->owner_begin, step->owner_end);
        return true;
    }
    // Read-only host GPR query. The selected thread published this fixed copy
    // before phase::trapped's release-store. Hold the same transition ownership
    // as disassembly so another command cannot resume or clear its session.
    // The callback must only copy/display values and must not re-enter this API.
    template<typename Inspect>
    [[nodiscard]] inline bool with_owned_registers(void const* identity, Inspect inspect) noexcept
    {
        details::host_transition_guard guard{};
        auto const* step{details::active.load(::std::memory_order_acquire)};
        if(step == nullptr || static_cast<void const*>(step) != identity) { return false; }
        // [actual controller-owned active session] host-transition guard
        // [safe                                  ] identity is compared before dereferencing;
        //  ^^ state acquire observes the complete register copy made by the trap handler.
        if((step->state.load(::std::memory_order_acquire) != phase::trapped &&
            !(step->state.load(::std::memory_order_acquire) == phase::at_guest_pc && step->retained_entry_stop)) || step->target_thread == 0u ||
           step->owner_begin == 0u || step->owner_end <= step->owner_begin ||
           step->next_pc < step->owner_begin || step->next_pc >= step->owner_end ||
           step->register_snapshot.size() == 0u || step->register_snapshot.pc() != step->next_pc)
        { return false; }
        // Borrow a local value copy, never the kernel context or mutable saved trap state.
        auto const copied{step->register_snapshot};
        inspect(step->target_thread, step->next_pc, step->owner_begin, step->owner_end, copied);
        return true;
    }
    template<typename Inspect>
    [[nodiscard]] inline bool with_owned_registers_and_revision(void const* identity, Inspect inspect) noexcept
    {
        return with_owned_registers(identity,[&](auto thread,auto pc,auto begin,auto end,auto const& registers) noexcept
        { auto const* step{details::active.load(::std::memory_order_acquire)};inspect(thread,pc,begin,end,registers,step->trap_revision); });
    }
    [[nodiscard]] inline bool clear(session& step) noexcept
    {
        details::host_transition_guard guard{};
        auto const state{step.state.load(::std::memory_order_acquire)};
        if(state != phase::ready && state != phase::released) { return false; }
        auto* expected{::std::addressof(step)};
        if(!details::active.compare_exchange_strong(expected, nullptr,
            ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return false; }
        // The guest has released both VEH gate waits (or never armed). Revoke
        // these exact owners only here, after active publication is withdrawn.
        if(step.first_gate_owner.native_handle() != step.first_gate ||
           step.second_gate_owner.native_handle() != step.second_gate ||
           step.target_owner.native_handle() != step.target_handle) { details::fail_invalid_gate(); }
        details::close_owned_kernel_handle(step.first_gate_owner);
        details::close_owned_kernel_handle(step.second_gate_owner);
        details::close_owned_kernel_handle(step.target_owner);
        step.first_gate = step.second_gate = nullptr;
        step.target_handle = nullptr;
        step.state.store(phase::idle, ::std::memory_order_release);
        return true;
    }
#else
    [[nodiscard]] inline constexpr bool platform_available() noexcept { return false; }
    [[nodiscard]] inline constexpr bool install() noexcept { return false; }
    [[nodiscard]] inline constexpr bool uninstall() noexcept { return true; }
    [[nodiscard]] inline constexpr bool request(session&, ::std::uint_least64_t,
        ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t) noexcept { return false; }
    [[nodiscard]] inline constexpr bool arm_at_return(::std::uintptr_t) noexcept { return false; }
    [[nodiscard]] inline constexpr bool retain_initial_trap(session&) noexcept { return false; }
    [[nodiscard]] inline constexpr bool continue_one(session&) noexcept { return false; }
    [[nodiscard]] inline constexpr bool continue_from_trap(session&) noexcept { return false; }
    [[nodiscard]] inline constexpr bool release(session&) noexcept { return false; }
    template<typename Inspect>
    [[nodiscard]] inline constexpr bool with_owned_trap(void const*, Inspect) noexcept { return false; }
    template<typename Inspect>
    [[nodiscard]] inline constexpr bool with_owned_registers(void const*, Inspect) noexcept { return false; }
    [[nodiscard]] inline constexpr bool clear(session&) noexcept { return false; }
#endif
}
