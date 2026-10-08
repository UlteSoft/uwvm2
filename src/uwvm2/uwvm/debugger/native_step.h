/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/push_macros.h>
#endif
#include "native_linux_platform.h"
#ifndef UWVM_MODULE
# include <atomic>
# include <cstddef>
# include <cstdint>
# include "native_registers.h"
# include <memory>
# if defined(__APPLE__)
#  include <TargetConditionals.h>
# endif
# if defined(__linux__) && defined(__x86_64__)
#  include <csignal>
#  include <linux/futex.h>
#  include <linux/perf_event.h>
#  include <sys/syscall.h>
#  include <ucontext.h>
#  include <unistd.h>
#  include "posix_abi.h"
#  include "native_perf_signal_linux.h"
#  include <uwvm2/runtime/lib/uwvm_runtime.h>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__)) && \
    defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT == 1
# ifndef UWVM_MODULE
#  include "native_step_windows.h"
# endif
// The experimental product switch is enabled only for the Windows VM
// qualification build. Ordinary Windows binaries remain fail-closed until
// the complete LLVM-full controller and JIT bridge are tested in that VM.
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_step
{
    using phase = ::uwvm2::uwvm::debugger::native_step_windows::phase;
    using session = ::uwvm2::uwvm::debugger::native_step_windows::session;
    using ::uwvm2::uwvm::debugger::native_step_windows::platform_available;
    using ::uwvm2::uwvm::debugger::native_step_windows::install;
    using ::uwvm2::uwvm::debugger::native_step_windows::uninstall;
    using ::uwvm2::uwvm::debugger::native_step_windows::request;
    using ::uwvm2::uwvm::debugger::native_step_windows::arm_at_return;
    using ::uwvm2::uwvm::debugger::native_step_windows::continue_one;
    using ::uwvm2::uwvm::debugger::native_step_windows::continue_from_trap;
    using ::uwvm2::uwvm::debugger::native_step_windows::release;
    using ::uwvm2::uwvm::debugger::native_step_windows::clear;
    using ::uwvm2::uwvm::debugger::native_step_windows::with_owned_trap;
    using ::uwvm2::uwvm::debugger::native_step_windows::with_owned_registers;
}
#elif defined(__APPLE__) && (defined(__aarch64__) || (defined(__x86_64__) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT == 1)) && TARGET_OS_OSX
# ifndef UWVM_MODULE
#  include "native_step_macos.h"
# endif
// Keep the controller's host-only protocol identical on qualified platforms.
// The Mach backend owns all exception-port and thread-state details; no Linux
// SIGTRAP state or platform fallback is instantiated in this configuration.
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_step
{
    using phase = ::uwvm2::uwvm::debugger::native_step_macos::phase;
    using session = ::uwvm2::uwvm::debugger::native_step_macos::session;
    using ::uwvm2::uwvm::debugger::native_step_macos::platform_available;
    using ::uwvm2::uwvm::debugger::native_step_macos::install;
    using ::uwvm2::uwvm::debugger::native_step_macos::uninstall;
    using ::uwvm2::uwvm::debugger::native_step_macos::request;
    using ::uwvm2::uwvm::debugger::native_step_macos::arm_at_return;
    using ::uwvm2::uwvm::debugger::native_step_macos::continue_one;
    using ::uwvm2::uwvm::debugger::native_step_macos::continue_from_trap;
    using ::uwvm2::uwvm::debugger::native_step_macos::release;
    using ::uwvm2::uwvm::debugger::native_step_macos::clear;
    using ::uwvm2::uwvm::debugger::native_step_macos::with_owned_trap;
    using ::uwvm2::uwvm::debugger::native_step_macos::with_owned_registers;
}
#elif UWVM2_DEBUG_NATIVE_LINUX_SOFTWARE
# include "native_step_linux.h"
#else
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_step
{
    // A machine step is an independently authenticated native operation. It is
    // never represented by the Wasm safe-point step protocol.
    enum class phase : ::std::uint32_t
    {
        idle, ready, arming, at_guest_pc, executing, trapped, releasing, released, failed,
        continuation_running, continuation_cancel_pending
    };
    struct session
    {
        ::std::atomic<phase> state{phase::idle};
        ::std::atomic<::std::uint32_t> first_gate{};
        ::std::atomic<::std::uint32_t> second_gate{};
        ::std::atomic_bool abort_requested{};
        ::std::uint_least64_t target_thread{};
        ::std::uintptr_t owner_begin{}, owner_end{}, expected_pc{};
        ::std::uintptr_t first_pc{}, next_pc{};
        native_registers::snapshot register_snapshot{};
#if defined(__linux__) && defined(__x86_64__)
        // Actual kernel trap episode only: PC/SP may repeat inside a loop.
        // This sequence is NEVER reset by request/continue/clear, and no manager
        // command can advance it. The selected worker writes only after genuine
        // bounds + physical activation witness; trapped's SC publication makes
        // the complete GPR copy AND this checked nonzero revision visible.
        ::std::uint64_t trap_revision{};
#endif
        int first_signal_code{}, second_signal_code{};
        // Host-owned continuation event data. Cookie/PC/SP/FD are written only
        // with the actual trap gate closed, before state release publication.
        ::std::uintptr_t continuation_pc{}, continuation_origin_sp{};
        ::std::uint64_t continuation_cookie{};
        int continuation_descriptor{-1};
        ::std::atomic_bool continuation_event_retired{true};
#if defined(__linux__) && defined(__x86_64__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
        ::uwvm2::runtime::lib::llvm_jit_debug_native_activation_provider activation_provider{};
#endif
    };
    static_assert(::std::atomic<phase>::is_always_lock_free);
    static_assert(::std::atomic<::std::uint32_t>::is_always_lock_free);
    static_assert(::std::atomic_bool::is_always_lock_free);

#if defined(__linux__) && defined(__x86_64__)
    namespace details
    {
        inline ::std::atomic<session*> active{};
        // Sanitized host diagnostics for the sole actual continuation session.
        // No native addresses/registers/stack bytes are inspection output.
        inline ::std::atomic_uint continuation_failure{}, continuation_other_stack_hits{};
        inline constinit thread_local session* current_thread_session{};
        // Set only around a callback from this real kernel-supplied Wasm trap.
        // A caller with copied PC/SP/session data cannot manufacture this TLS.
        inline constinit thread_local session const* kernel_witness_session{};
        inline constinit thread_local ::std::uintptr_t kernel_witness_pc{}, kernel_witness_sp{};
        [[nodiscard]] inline bool witness_physical_activation(session& step,
            ::std::uintptr_t pc, ::std::uintptr_t sp) noexcept
        {
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
            if(!step.activation_provider.valid()) { return true; } // standalone legacy SI component; NEVER NI
            kernel_witness_session = ::std::addressof(step); kernel_witness_pc = pc; kernel_witness_sp = sp;
            bool const valid{step.activation_provider.witness(::std::addressof(step), step.target_thread, pc, sp)};
            kernel_witness_session = nullptr; kernel_witness_pc = kernel_witness_sp = 0u;
            return valid;
#else
            (void)step; (void)pc; (void)sp; return false;
#endif
        }
        // Late kernel task_work carries an integer cookie, never a session
        // pointer. Keep its last retired identity in OS-thread scalar TLS so
        // canceled kernel work cannot borrow freed controller/session storage.
        inline constinit thread_local ::std::uint64_t retired_continuation_cookie{};
        // If this worker ACKs with SIGTRAP blocked or the kernel barrier fails,
        // retain its one canceled cookie and refuse every later native arm on
        // this SAME OS thread. A second session must not overwrite the identity
        // needed to consume still-pending old kernel work. Ordinary Wasm runs.
        inline constinit thread_local bool retired_continuation_delivery_uncertain{};
        inline struct ::sigaction previous_action{};
        inline ::std::atomic_bool installed{};
        // A trusted host may block SIGTRAP across execution-scope exit. Such
        // a canceled cookie remains scalar TLS, but pending kernel signals
        // cannot be proven drained: keep this disposition instead of restoring
        // a default handler underneath known pending hardware work.
        inline ::std::atomic_bool continuation_disposition_retained{};
        // Serializes host request/arm/cancel and handler installation. The
        // SIGTRAP handler never acquires this flag. In particular, arm must
        // re-read active while holding it before dereferencing a session that
        // the host may cancel and destroy from phase::ready.
        inline ::std::atomic_flag host_transition = ATOMIC_FLAG_INIT;
        struct host_transition_guard
        {
            host_transition_guard() noexcept
            { while(host_transition.test_and_set(::std::memory_order_acquire)) {} }
            host_transition_guard(host_transition_guard const&) = delete;
            host_transition_guard& operator=(host_transition_guard const&) = delete;
            ~host_transition_guard() { host_transition.clear(::std::memory_order_release); }
        };
        inline constexpr ::std::uintptr_t trap_flag{0x100u};

        inline void capture_kernel_fp_registers(native_registers::fp_snapshot& out,
            ::ucontext_t const& context) noexcept
        {
            out = {};
#if __SIZEOF_POINTER__ == 8
            // Linux x86-64 UAPI FXSAVE base is a complete 512-byte object even
            // when a larger XSAVE frame follows it. This is called ONLY after
            // the actual kernel TRAP_TRACE/TRAP_PERF + private session/PC proof;
            // no callback, public command or guest address supplies this pointer.
            auto const* saved{context.uc_mcontext.fpregs};
            if(saved == nullptr) { return; }
            if constexpr(sizeof(*saved) == 512u)
            {
                // [actual kernel fpregs complete FXSAVE base ... 512 bytes] end
                // [safe                                                   ] typed
                //  ^^ the kernel signal ABI guarantees this prefix, not the
                // embedded glibc __fpregs_mem address nor an inferred guest SP.
                auto const* bytes{reinterpret_cast<unsigned char const*>(saved)};
                for(::std::size_t index{}; index != 30u; ++index)
                {
                    auto const width{native_registers::fp_width(index)};
                    auto const offset{index < 16u ? 160u + index * 16u : index < 24u ?
                        32u + (index - 16u) * 16u : index < 28u ? (index - 24u) * 2u : 24u + (index - 28u) * 4u};
                    // [actual kernel base: 0 ... checked offset,width ... 512] end
                    // [safe                                                 ] all
                    //  ^^ metadata is fixed by this native adapter; prove source
                    // AND destination widths before forming ANY subscript borrow.
                    if(width == 0u || width > out.values[index].bytes.size() || offset > 512u || width > 512u - offset)
                    { out = {}; return; }
                    for(::std::size_t part{}; part != width; ++part)
                    {
                        // [kernel FXSAVE offset ... offset+width<=512][owned value width<=16]
                        // [safe                                    ][safe                 ]
                        //  ^^ checked scalar indices copy VALUES only. Never copy
                        // x87 RIP/RDP, padding, reserved fields or extended pointers.
                        out.values[index].bytes[part] = bytes[offset + part];
                    }
                    if(index == 27u)
                    {
                        // [owned FOP value bytes0..1] end
                        // [safe                      ] this fixed field's checked
                        //  ^^ width is two; Intel defines only its lower11 bits.
                        // Clear reserved upper5 bits, never shift its byte6 offset
                        // when the preceding abridged FTW field is shortened.
                        out.values[index].bytes[1u] &= 0x07u;
                    }
                    out.values[index].width = static_cast<::std::uint8_t>(width);
                }
                out.available = true; // Publish only after the whole bounded copy.
            }
#endif
        }

        inline void capture_registers(native_registers::snapshot& out, ::ucontext_t const& context) noexcept
        {
            out.machine = native_registers::architecture::x86_64;
            // [kernel-supplied complete ucontext.gregs] [owned fixed 34-register snapshot]
            // [safe                                 ] SDK REG_* constants select this typed context;
            //  ^^ read scalar registers only, never dereference a captured SP/FP/PC value.
            out.values = {
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_RAX]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_RBX]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_RCX]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_RDX]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_RSI]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_RDI]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_RBP]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_RSP]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_R8]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_R9]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_R10]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_R11]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_R12]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_R13]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_R14]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_R15]),
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_RIP]),
                // TF is debugger-owned state, not an instruction's architectural result.
                static_cast<::std::uint64_t>(context.uc_mcontext.gregs[REG_EFL]) & ~trap_flag};
            capture_kernel_fp_registers(out.floating, context);
        }


        [[nodiscard]] inline long raw_system_call(long number, long first = 0, long second = 0,
                                                  long third = 0, long fourth = 0) noexcept
        {
            register long r10 __asm__("r10") = fourth;
            register long r8 __asm__("r8") = 0;
            register long r9 __asm__("r9") = 0;
            long result{};
            __asm__ volatile("syscall" : "=a"(result)
                : "a"(number), "D"(first), "S"(second), "d"(third), "r"(r10), "r"(r8), "r"(r9)
                : "rcx", "r11", "memory");
            return result;
        }

        inline void wait_for_gate(::std::atomic<::std::uint32_t>& gate) noexcept
        {
            // The kernel reads the four-byte lock-free atomic object as a
            // futex word. Only the trusted host controller can wake this gate.
            static_assert(sizeof(gate) == sizeof(::std::uint32_t));
            auto const address{reinterpret_cast<::std::uintptr_t>(::std::addressof(gate))};
            while(gate.load(::std::memory_order_acquire) == 0u)
            {
                static_cast<void>(raw_system_call(SYS_futex, static_cast<long>(address),
                    FUTEX_WAIT_PRIVATE, 0u, 0u));
            }
        }
        inline void wake_gate(::std::atomic<::std::uint32_t>& gate) noexcept
        {
            gate.store(1u, ::std::memory_order_release);
            auto const address{reinterpret_cast<::std::uintptr_t>(::std::addressof(gate))};
            static_cast<void>(raw_system_call(SYS_futex, static_cast<long>(address),
                FUTEX_WAKE_PRIVATE, 1u, 0u));
        }

        inline void retire_current_thread_session(session& step) noexcept
        {
            if(step.continuation_cookie != 0u) { retired_continuation_cookie = step.continuation_cookie; }
            current_thread_session = nullptr;
            step.state.store(phase::released, ::std::memory_order_seq_cst);
        }

        // Unrelated SIGTRAPs retain the process's pre-existing disposition.
        // A previous custom handler is responsible for its own signal safety,
        // exactly as it was before native stepping was installed.
        inline void forward_trap(int number, ::siginfo_t* info, void* context) noexcept
        {
            auto const prior{previous_action};
            if(prior.sa_handler == SIG_IGN) { return; }
            if(prior.sa_handler == SIG_DFL)
            {
                static_cast<void>(posix_abi::sigaction_noexcept(number, ::std::addressof(prior), nullptr));
                static_cast<void>(posix_abi::raise_noexcept(number));
                posix_abi::_exit_noexcept(128 + number);
            }
            if((prior.sa_flags & SA_SIGINFO) != 0) { prior.sa_sigaction(number, info, context); }
            else { prior.sa_handler(number); }
        }

        // Precise execution event only. This is neither sampling nor TF
        // stepping through a host/VM callee. Unrelated perf signals retain the
        // previous disposition; our canceled cookie is consumed without views.
        [[nodiscard]] inline bool handle_continuation_trap(int number, ::siginfo_t* info, void* context) noexcept
        {
# if __SIZEOF_POINTER__ == 8 && defined(PERF_ATTR_SIZE_VER7) && !defined(__ILP32__)
            auto const payload{native_perf_signal_linux::read(info)};
            if(number != SIGTRAP || context == nullptr || !payload.valid ||
               payload.type != PERF_TYPE_BREAKPOINT) { return false; }
            auto const cookie{payload.cookie};
            if(cookie != 0u && cookie == retired_continuation_cookie) { return true; }
            auto* const step{current_thread_session};
            if(step == nullptr || active.load(::std::memory_order_acquire) != step ||
               cookie == 0u || cookie != step->continuation_cookie) { return false; }
            auto const state{step->state.load(::std::memory_order_seq_cst)};
            if(state == phase::continuation_cancel_pending || step->abort_requested.load(::std::memory_order_seq_cst))
            { return true; } // No PC/GPR copy or native stop from canceled work.
            if(state != phase::continuation_running) { return false; }
            if(payload.flags != 0u)
            {
                continuation_failure.store(1u,::std::memory_order_relaxed);
                step->abort_requested.store(true, ::std::memory_order_seq_cst);
                step->state.store(phase::continuation_cancel_pending, ::std::memory_order_seq_cst);
                return true; // asynchronous/deferred kernel delivery supplies NO precise stop
            }
            auto* const saved{static_cast<::ucontext_t*>(context)};
            auto const pc{static_cast<::std::uintptr_t>(saved->uc_mcontext.gregs[REG_RIP])};
            auto const sp{static_cast<::std::uintptr_t>(saved->uc_mcontext.gregs[REG_RSP])};
            auto& flags{saved->uc_mcontext.gregs[REG_EFL]};
            if(reinterpret_cast<::std::uintptr_t>(info->si_addr) != step->continuation_pc ||
               pc != step->continuation_pc || pc < step->owner_begin || pc >= step->owner_end ||
               (flags & static_cast<::greg_t>(trap_flag)) != 0 ||
               static_cast<::std::uint_least64_t>(raw_system_call(SYS_gettid)) != step->target_thread)
            {
                step->abort_requested.store(true, ::std::memory_order_seq_cst);
                step->state.store(phase::continuation_cancel_pending, ::std::memory_order_seq_cst);
                continuation_failure.store(2u,::std::memory_order_relaxed);
                return true; // Protocol mismatch never publishes a VM/host stop.
            }
            // A recursive/reentrant activation may hit the SAME code address.
            // The kernel already sets RF for this execute fault. Ignore it with
            // TF still clear until the exact original physical activation SP.
            // Never dereference the saved stack pointer or synthesize a frame.
            if(sp != step->continuation_origin_sp)
            { continuation_other_stack_hits.fetch_add(1u,::std::memory_order_relaxed); return true; }
            if(raw_system_call(SYS_ioctl, step->continuation_descriptor,
                    static_cast<long>(PERF_EVENT_IOC_DISABLE), 0u) != 0)
            {
                step->abort_requested.store(true, ::std::memory_order_seq_cst);
                step->state.store(phase::continuation_cancel_pending, ::std::memory_order_seq_cst);
                continuation_failure.store(3u,::std::memory_order_relaxed);
                return true;
            }
            // The FD remains owned until this borrower has actually retired.
            // Host cancellation may disable the same FD concurrently, but may
            // not close/reuse it before the selected worker's real ACK.
            if(step->abort_requested.load(::std::memory_order_seq_cst)) { return true; }
            if(!witness_physical_activation(*step, pc, sp))
            {
                continuation_failure.store(4u,::std::memory_order_relaxed);
                step->abort_requested.store(true, ::std::memory_order_seq_cst);
                step->state.store(phase::continuation_cancel_pending, ::std::memory_order_seq_cst);
                return true;
            }
            step->next_pc = pc;
            capture_registers(step->register_snapshot, *saved);
            step->second_signal_code = info->si_code;
            if(step->trap_revision == UINT64_MAX)
            {
                continuation_failure.store(5u,::std::memory_order_relaxed);
                step->abort_requested.store(true, ::std::memory_order_seq_cst);
                auto running{phase::continuation_running};
                if(!step->state.compare_exchange_strong(running, phase::continuation_cancel_pending,
                    ::std::memory_order_seq_cst, ::std::memory_order_seq_cst) && running == phase::releasing)
                { retire_current_thread_session(*step); }
                return true; // Never overwrite a manager's release and lose actual ACK.
            }
            ++step->trap_revision; // Genuine bounded kernel witness; never a caller label.
            // Cancellation can discard this genuine trap after consuming a
            // revision. Gaps grant no authority; reuse of a revision is forbidden.
            auto expected{phase::continuation_running};
            if(!step->state.compare_exchange_strong(expected, phase::trapped,
                ::std::memory_order_seq_cst, ::std::memory_order_seq_cst)) { return true; }
            // cancel may have sampled running immediately before this CAS.
            // The manager rechecks state after publishing abort; the reciprocal
            // check closes its running -> trapped acknowledgement race.
            if(step->abort_requested.load(::std::memory_order_seq_cst))
            {
                auto trapped{phase::trapped};
                if(step->state.compare_exchange_strong(trapped, phase::continuation_cancel_pending,
                    ::std::memory_order_seq_cst, ::std::memory_order_seq_cst)) { return true; }
                // Manager won trapped -> releasing and opened the real gate.
                // Do not overwrite it with cancel_pending and lose its ACK.
                if(trapped == phase::releasing) { retire_current_thread_session(*step); return true; }
                return true;
            }
            wait_for_gate(step->second_gate);
            auto const next{step->state.load(::std::memory_order_seq_cst)};
            if(next == phase::executing || next == phase::continuation_running)
            {
                step->second_gate.store(0u, ::std::memory_order_relaxed);
                if(next == phase::executing) { flags |= static_cast<::greg_t>(trap_flag); }
                else { flags &= ~static_cast<::greg_t>(trap_flag); }
                return true;
            }
            retire_current_thread_session(*step);
            return true;
# else
            (void)number; (void)info; (void)context; return false;
# endif
        }

        inline void trap_handler(int number, ::siginfo_t* info, void* context) noexcept
        {
            if(handle_continuation_trap(number, info, context)) { return; }
            auto* const step{current_thread_session};
            if(number != SIGTRAP || info == nullptr || context == nullptr ||
               info->si_code != TRAP_TRACE || step == nullptr ||
               active.load(::std::memory_order_acquire) != step)
            { forward_trap(number, info, context); return; }
            auto* const saved{static_cast<::ucontext_t*>(context)};
            auto const pc{static_cast<::std::uintptr_t>(saved->uc_mcontext.gregs[REG_RIP])};
            auto& flags{saved->uc_mcontext.gregs[REG_EFL]};
            auto const state{step->state.load(::std::memory_order_seq_cst)};
            if(state == phase::arming)
            {
                step->first_pc = pc;
                step->first_signal_code = info->si_code;
                flags &= ~static_cast<::greg_t>(trap_flag);
                if(pc != step->expected_pc || pc < step->owner_begin || pc >= step->owner_end)
                {
                    step->state.store(phase::failed, ::std::memory_order_seq_cst);
                    if(step->abort_requested.load(::std::memory_order_seq_cst))
                    { retire_current_thread_session(*step); return; }
                    wait_for_gate(step->second_gate);
                    retire_current_thread_session(*step);
                    return;
                }
                if(step->abort_requested.load(::std::memory_order_seq_cst))
                {
                    retire_current_thread_session(*step);
                    return;
                }
                capture_registers(step->register_snapshot, *saved);
                if(!witness_physical_activation(*step, pc, static_cast<::std::uintptr_t>(step->register_snapshot.sp())))
                {
                    step->state.store(phase::failed, ::std::memory_order_seq_cst);
                    if(step->abort_requested.load(::std::memory_order_seq_cst))
                    { retire_current_thread_session(*step); return; }
                    wait_for_gate(step->second_gate); retire_current_thread_session(*step); return;
                }
                step->state.store(phase::at_guest_pc, ::std::memory_order_seq_cst);
                if(step->abort_requested.load(::std::memory_order_seq_cst))
                { retire_current_thread_session(*step); return; }
                wait_for_gate(step->first_gate);
                if(step->state.load(::std::memory_order_seq_cst) == phase::releasing)
                {
                    retire_current_thread_session(*step);
                    return;
                }
                flags |= static_cast<::greg_t>(trap_flag);
                return;
            }
            if(state == phase::executing)
            {
                step->next_pc = pc;
                capture_registers(step->register_snapshot, *saved);
                step->second_signal_code = info->si_code;
                flags &= ~static_cast<::greg_t>(trap_flag);
                if(step->abort_requested.load(::std::memory_order_seq_cst))
                {
                    retire_current_thread_session(*step);
                    return;
                }
                if(pc < step->owner_begin || pc >= step->owner_end ||
                   !witness_physical_activation(*step, pc, static_cast<::std::uintptr_t>(step->register_snapshot.sp())))
                {
                    step->state.store(phase::failed, ::std::memory_order_seq_cst);
                    if(step->abort_requested.load(::std::memory_order_seq_cst))
                    { retire_current_thread_session(*step); return; }
                    wait_for_gate(step->second_gate); retire_current_thread_session(*step); return;
                }
                if(step->trap_revision == UINT64_MAX)
                {
                    step->state.store(phase::failed, ::std::memory_order_seq_cst);
                    if(step->abort_requested.load(::std::memory_order_seq_cst))
                    { retire_current_thread_session(*step); return; }
                    wait_for_gate(step->second_gate); retire_current_thread_session(*step); return;
                }
                ++step->trap_revision; // Complete successful physical kernel trap, BEFORE phase publication.
                step->state.store(phase::trapped, ::std::memory_order_seq_cst);
                if(step->abort_requested.load(::std::memory_order_seq_cst))
                { retire_current_thread_session(*step); return; }
                wait_for_gate(step->second_gate);
                auto const next{step->state.load(::std::memory_order_seq_cst)};
                if(next == phase::executing || next == phase::continuation_running)
                {
                    // A second host command owns the same stopped native PC.
                    // Native SI rearms TF; true call next returns TF-OFF and
                    // relies solely on the exact owned continuation event.
                    step->second_gate.store(0u, ::std::memory_order_relaxed);
                    if(next == phase::executing) { flags |= static_cast<::greg_t>(trap_flag); }
                    else { flags &= ~static_cast<::greg_t>(trap_flag); }
                    return;
                }
                retire_current_thread_session(*step);
                return;
            }
            // An unexpected trap in the selected thread cannot be mistaken for
            // completion of a machine instruction. Retain the prior disposition.
            forward_trap(number, info, context);
        }
    }

    [[nodiscard]] inline constexpr bool platform_available() noexcept { return true; }

    // Install only on explicit LLVM-full debugger admission. The default VM
    // never installs a SIGTRAP handler or executes a native-step branch.
    [[nodiscard]] inline bool install() noexcept
    {
        details::host_transition_guard guard{};
        if(details::installed.load(::std::memory_order_acquire)) { return true; }
        struct ::sigaction prior{};
        if(posix_abi::sigaction_noexcept(SIGTRAP, nullptr, ::std::addressof(prior)) != 0) { return false; }
        details::previous_action = prior;
        struct ::sigaction action{};
        action.sa_sigaction = details::trap_handler;
        action.sa_flags = SA_SIGINFO | SA_ONSTACK;
        posix_abi::sigemptyset_noexcept(::std::addressof(action.sa_mask));
        if(posix_abi::sigaction_noexcept(SIGTRAP, ::std::addressof(action), nullptr) != 0) { return false; }
        details::installed.store(true, ::std::memory_order_release);
        return true;
    }
    [[nodiscard]] inline bool uninstall() noexcept
    {
        details::host_transition_guard guard{};
        if(details::active.load(::std::memory_order_acquire) != nullptr ||
           details::continuation_disposition_retained.load(::std::memory_order_acquire)) { return false; }
        if(!details::installed.load(::std::memory_order_acquire)) { return true; }
        if(posix_abi::sigaction_noexcept(SIGTRAP, ::std::addressof(details::previous_action), nullptr) != 0) { return false; }
        details::installed.store(false, ::std::memory_order_release);
        return true;
    }
    [[nodiscard]] inline bool request(session& step, ::std::uint_least64_t native_thread,
                                      ::std::uintptr_t owner_begin, ::std::uintptr_t owner_end,
                                      ::std::uintptr_t return_pc
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
                                      , ::uwvm2::runtime::lib::llvm_jit_debug_native_activation_provider provider = {}
# endif
                                      ) noexcept
    {
        details::host_transition_guard guard{};
        if(!details::installed.load(::std::memory_order_acquire) || native_thread == 0u ||
           owner_begin == 0u || owner_begin >= owner_end ||
           return_pc < owner_begin || return_pc >= owner_end ||
           step.state.load(::std::memory_order_seq_cst) != phase::idle ||
           step.trap_revision == UINT64_MAX) { return false; }
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
        if(provider.valid() && !provider.bound_to(::std::addressof(step), native_thread, owner_begin, owner_end, return_pc)) { return false; }
        step.activation_provider = provider;
# endif
        step.target_thread = native_thread;
        step.owner_begin = owner_begin;
        step.owner_end = owner_end;
        step.expected_pc = return_pc;
        step.first_pc = step.next_pc = 0u;
        step.register_snapshot = {};
        step.first_signal_code = step.second_signal_code = 0;
        step.continuation_pc = step.continuation_origin_sp = 0u;
        step.continuation_cookie = 0u; step.continuation_descriptor = -1;
        step.continuation_event_retired.store(true, ::std::memory_order_relaxed);
        step.first_gate.store(0u, ::std::memory_order_relaxed);
        step.second_gate.store(0u, ::std::memory_order_relaxed);
        step.abort_requested.store(false, ::std::memory_order_seq_cst);
        step.state.store(phase::ready, ::std::memory_order_seq_cst);
        session* expected{};
        if(!details::active.compare_exchange_strong(expected, ::std::addressof(step),
            ::std::memory_order_acq_rel, ::std::memory_order_acquire))
        { step.state.store(phase::idle, ::std::memory_order_seq_cst); return false; }
        return true;
    }
    // Only the real worker can inspect its own mask. This is used by cold
    // genuine before-park capture, and repeated immediately before TF admission.
    // Ordinary Wasm memory/code has no additional syscall or branch.
    [[nodiscard]] inline bool can_arm_on_current_thread() noexcept
    {
        if(details::retired_continuation_delivery_uncertain) { return false; }
        ::std::uint64_t blocked{};
        // [actual worker-owned full kernel signal mask: eight bytes] mask_end
        // [safe                                                  ] rt_sigprocmask
        //  ^^ writes exactly this owned output; no guest/stack address is read.
        auto const result{details::raw_system_call(SYS_rt_sigprocmask, SIG_BLOCK, 0u,
            static_cast<long>(reinterpret_cast<::std::uintptr_t>(::std::addressof(blocked))), sizeof(blocked))};
        return result == 0 && (blocked & (1ull << (SIGTRAP - 1u))) == 0u;
    }
    [[nodiscard]] inline bool arm_at_return(::std::uintptr_t return_pc) noexcept
    {
        if(details::current_thread_session != nullptr ||
           details::active.load(::std::memory_order_acquire) == nullptr || !can_arm_on_current_thread()) { return false; }
        details::host_transition_guard guard{};
        auto* const step{details::active.load(::std::memory_order_acquire)};
        if(step == nullptr || details::retired_continuation_delivery_uncertain || return_pc != step->expected_pc ||
           return_pc < step->owner_begin || return_pc >= step->owner_end ||
           static_cast<::std::uint_least64_t>(details::raw_system_call(SYS_gettid)) != step->target_thread)
        { return false; }
        auto expected{phase::ready};
        if(!step->state.compare_exchange_strong(expected, phase::arming,
            ::std::memory_order_seq_cst, ::std::memory_order_seq_cst)) { return false; }
        details::current_thread_session = step;
        return true;
    }
    [[nodiscard]] inline bool continue_one(session& step) noexcept
    {
        details::host_transition_guard guard{};
        if(details::active.load(::std::memory_order_acquire) != ::std::addressof(step) ||
           step.state.load(::std::memory_order_seq_cst) != phase::at_guest_pc ||
           step.trap_revision == UINT64_MAX) { return false; }
        // Observe the actual parked phase BEFORE any plain handler-written
        // revision/GPR field; the signal handler never takes the host guard.
        auto expected{phase::at_guest_pc};
        if(!step.state.compare_exchange_strong(expected, phase::executing,
            ::std::memory_order_seq_cst, ::std::memory_order_seq_cst)) { return false; }
        details::wake_gate(step.first_gate);
        return true;
    }
    // Called only by the host manager after it has accounted for the target
    // as running. The second trap remains parked across debugger commands.
    // A branch/return outside the original authenticated function fails closed.
    [[nodiscard]] inline bool continue_from_trap(session& step) noexcept
    {
        details::host_transition_guard guard{};
        if(details::active.load(::std::memory_order_acquire) != ::std::addressof(step) ||
           step.state.load(::std::memory_order_seq_cst) != phase::trapped ||
           step.next_pc < step.owner_begin || step.next_pc >= step.owner_end ||
           step.trap_revision == UINT64_MAX) { return false; }
        auto expected{phase::trapped};
        if(!step.state.compare_exchange_strong(expected, phase::executing,
            ::std::memory_order_seq_cst, ::std::memory_order_seq_cst)) { return false; }
        step.first_pc = step.next_pc;
        step.expected_pc = step.next_pc;
        details::wake_gate(step.second_gate);
        return true;
    }
    // Called ONLY after the private runtime authenticated the current actual
    // trap, immutable Wasm owner and exact forward MC continuation boundary,
    // and after its actual TID's owned execute event was enabled while parked.
    [[nodiscard]] inline bool continue_to_continuation(session& step, ::std::uintptr_t continuation,
        ::std::uint64_t cookie, int owned_descriptor) noexcept
    {
# if __SIZEOF_POINTER__ == 8 && defined(PERF_ATTR_SIZE_VER7) && !defined(__ILP32__)
        details::host_transition_guard guard{};
        if(!native_perf_signal_linux::available ||
           details::active.load(::std::memory_order_acquire) != ::std::addressof(step) ||
           step.state.load(::std::memory_order_seq_cst) != phase::trapped || step.target_thread == 0u ||
           step.next_pc < step.owner_begin || step.next_pc >= step.owner_end ||
           continuation <= step.next_pc || continuation >= step.owner_end || cookie == 0u || owned_descriptor < 0 ||
           step.register_snapshot.machine != native_registers::architecture::x86_64 ||
           step.register_snapshot.pc() != step.next_pc || step.register_snapshot.sp() == 0u ||
           step.trap_revision == UINT64_MAX
#  if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
           || !step.activation_provider.valid()
#  else
           || true
#  endif
           )
        { return false; }
        step.continuation_pc = continuation;
        step.continuation_origin_sp = static_cast<::std::uintptr_t>(step.register_snapshot.sp());
        step.continuation_cookie = cookie; step.continuation_descriptor = owned_descriptor;
        step.continuation_event_retired.store(false, ::std::memory_order_relaxed);
        step.abort_requested.store(false, ::std::memory_order_seq_cst);
        step.first_pc = step.expected_pc = step.next_pc;
        step.state.store(phase::continuation_running, ::std::memory_order_seq_cst);
        details::wake_gate(step.second_gate);
        return true;
# else
        (void)step; (void)continuation; (void)cookie; (void)owned_descriptor; return false;
# endif
    }

# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // No raw cross-owner PC/SP entry exists. Only the runtime-issued seal may
    // switch the provider/range while the genuine origin kernel gate is closed.
    [[nodiscard]] inline bool continue_to_return(session& step,
        ::uwvm2::runtime::lib::llvm_jit_debug_native_return_event const& event,
        ::std::uint64_t cookie, int owned_descriptor) noexcept
    {
        return ::uwvm2::runtime::lib::llvm_jit_debug_continue_native_return_event_host_api(
            ::std::addressof(step),event,cookie,owned_descriptor);
    }
# endif
    [[nodiscard]] inline bool in_owned_kernel_activation_witness(void const* identity,
        ::std::uint_least64_t thread, ::std::uintptr_t pc, ::std::uintptr_t sp) noexcept
    {
        auto const* step{details::current_thread_session};
        return step != nullptr && static_cast<void const*>(step) == identity &&
            details::active.load(::std::memory_order_acquire) == step && details::kernel_witness_session == step &&
            details::kernel_witness_pc == pc && details::kernel_witness_sp == sp &&
            step->target_thread == thread &&
            static_cast<::std::uint_least64_t>(details::raw_system_call(SYS_gettid)) == thread;
    }
    inline void report_activation_leave_on_worker(::std::uint64_t incarnation, unsigned kind) noexcept
    {
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
        auto* const step{details::current_thread_session};
        if(step != nullptr && details::active.load(::std::memory_order_acquire) == step &&
           step->state.load(::std::memory_order_seq_cst) == phase::continuation_running)
        { step->activation_provider.report_leave(step, incarnation, kind); }
# else
        (void)incarnation; (void)kind;
# endif
    }

    // Debug-only selected worker hook before a cooperative poll. Ordinary JIT
    // code has no new branch. NI must execute VM safe-point calls with TF=0;
    // polling cannot park before the actual native continuation has been hit.
    [[nodiscard]] inline bool skip_pause_for_continuation() noexcept
    {
        auto* const step{details::current_thread_session};
        if(step == nullptr || details::active.load(::std::memory_order_acquire) != step) { return false; }
        auto const state{step->state.load(::std::memory_order_seq_cst)};
        if(state == phase::continuation_running && !step->abort_requested.load(::std::memory_order_seq_cst)) { return true; }
        if(state != phase::continuation_cancel_pending) { return false; }
        // The manager disabled the actual owned event, but keeps its FD alive
        // until this actual worker ACK. No simulated park or code-owner drain.
        while(!step->continuation_event_retired.load(::std::memory_order_acquire))
        { static_cast<void>(details::raw_system_call(SYS_sched_yield)); }
        // [actual worker-owned eight-byte kernel signal-set output] end
        // [safe                                                   ] LP64
        // Linux rt_sigprocmask writes exactly this complete checked object.
        // The real syscall return also runs this thread's queued task_work;
        // with SIGTRAP unblocked, owned canceled signals run before userspace
        // resumes. Never infer draining from FD close or a manager sleep.
        ::std::uint64_t blocked{};
        auto const barrier{details::raw_system_call(SYS_rt_sigprocmask, SIG_BLOCK, 0u,
            static_cast<long>(reinterpret_cast<::std::uintptr_t>(::std::addressof(blocked))), sizeof(blocked))};
        if(barrier != 0 || (blocked & (1ull << (SIGTRAP - 1u))) != 0u)
        {
            details::retired_continuation_delivery_uncertain = true;
            details::continuation_disposition_retained.store(true, ::std::memory_order_release);
        }
        details::retire_current_thread_session(*step);
        return false;
    }
    // Called on the real selected worker BEFORE EH abandons its normal
    // continuation; no PC/GPR/code copy or native pause is produced here.
    inline void cancel_continuation_on_worker() noexcept
    {
        auto* const step{details::current_thread_session};
        if(step == nullptr || details::active.load(::std::memory_order_acquire) != step) { return; }
        details::host_transition_guard guard{};
        if(details::active.load(::std::memory_order_acquire) != step ||
           step->state.load(::std::memory_order_seq_cst) != phase::continuation_running) { return; }
        step->abort_requested.store(true, ::std::memory_order_seq_cst);
        step->state.store(phase::continuation_cancel_pending, ::std::memory_order_seq_cst);
    }
    inline void acknowledge_continuation_execution_exit() noexcept
    {
        cancel_continuation_on_worker();
        static_cast<void>(skip_pause_for_continuation());
    }
    // Manager publishes quiescence only AFTER actual kernel disable succeeded.
    // Descriptor close/reuse and fixed-session clear require the worker ACK.
    inline void acknowledge_continuation_event_retired(session& step) noexcept
    { step.continuation_event_retired.store(true, ::std::memory_order_release); }

    [[nodiscard]] inline bool release(session& step) noexcept
    {
        details::host_transition_guard guard{};
        if(details::active.load(::std::memory_order_acquire) != ::std::addressof(step)) { return false; }
        // SC pairs phase publication with cancellation. Merely reciprocal
        // release/acquire loads of DIFFERENT atomics permit both old values
        // (store buffering), losing a genuine handler's gate wake.
        step.abort_requested.store(true, ::std::memory_order_seq_cst);
        for(;;)
        {
            auto state{step.state.load(::std::memory_order_seq_cst)};
            if(state == phase::continuation_running)
            {
                if(step.state.compare_exchange_strong(state, phase::continuation_cancel_pending,
                    ::std::memory_order_seq_cst, ::std::memory_order_seq_cst)) { return true; }
                continue; // competing real perf trap/cancel transition; never overwrite it
            }
            if(state == phase::continuation_cancel_pending) { return true; }
            if(state == phase::arming || state == phase::executing)
            { return true; } // reciprocal SC handler check retires before sleeping
            if(state == phase::at_guest_pc || state == phase::trapped || state == phase::failed)
            {
                auto const first{state == phase::at_guest_pc};
                if(!step.state.compare_exchange_strong(state, phase::releasing,
                    ::std::memory_order_seq_cst, ::std::memory_order_seq_cst)) { continue; }
                if(first) { details::wake_gate(step.first_gate); }
                else { details::wake_gate(step.second_gate); }
                return true;
            }
            return state == phase::releasing || state == phase::released;
        }
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
        if(step->state.load(::std::memory_order_seq_cst) != phase::trapped ||
           step->abort_requested.load(::std::memory_order_seq_cst) || step->target_thread == 0u ||
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
    [[nodiscard]] inline bool with_owned_registers_and_revision(void const* identity, Inspect inspect) noexcept
    {
        details::host_transition_guard guard{};
        auto const* step{details::active.load(::std::memory_order_acquire)};
        if(step == nullptr || static_cast<void const*>(step) != identity) { return false; }
        // [actual controller-owned active session] host-transition guard
        // [safe                                  ] identity is compared before dereferencing;
        //  ^^ state acquire observes the complete register copy made by the trap handler.
        if(step->state.load(::std::memory_order_seq_cst) != phase::trapped ||
           step->abort_requested.load(::std::memory_order_seq_cst) || step->target_thread == 0u ||
           step->owner_begin == 0u || step->owner_end <= step->owner_begin ||
           step->next_pc < step->owner_begin || step->next_pc >= step->owner_end ||
           step->register_snapshot.size() == 0u || step->register_snapshot.pc() != step->next_pc ||
           step->trap_revision == 0u)
        { return false; }
        // Borrow a local value copy, never the kernel context or mutable saved trap state.
        auto const copied{step->register_snapshot};
        auto const revision{step->trap_revision};
        inspect(step->target_thread, step->next_pc, step->owner_begin, step->owner_end, copied, revision);
        return true;
    }
    // Preserve the original read-only DATA API. Closed runtime execution
    // capabilities instead bind the revision-bearing SAME guarded query.
    template<typename Inspect>
    [[nodiscard]] inline bool with_owned_registers(void const* identity, Inspect inspect) noexcept
    {
        return with_owned_registers_and_revision(identity,
            [&](auto thread, auto pc, auto begin, auto end, auto const& copied, auto) noexcept
            { inspect(thread, pc, begin, end, copied); });
    }
    [[nodiscard]] inline bool clear(session& step) noexcept
    {
        details::host_transition_guard guard{};
        auto const state{step.state.load(::std::memory_order_seq_cst)};
        if(state != phase::ready && state != phase::released) { return false; }
        auto* expected{::std::addressof(step)};
        if(!details::active.compare_exchange_strong(expected, nullptr,
            ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return false; }
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
        step.activation_provider = {};
# endif
        step.state.store(phase::idle, ::std::memory_order_seq_cst);
        return true;
    }
#else
    [[nodiscard]] inline constexpr bool platform_available() noexcept { return false; }
    [[nodiscard]] inline constexpr bool install() noexcept { return false; }
    [[nodiscard]] inline constexpr bool uninstall() noexcept { return true; }
    [[nodiscard]] inline constexpr bool request(session&, ::std::uint_least64_t,
        ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t) noexcept { return false; }
    [[nodiscard]] inline constexpr bool arm_at_return(::std::uintptr_t) noexcept { return false; }
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
#endif

#if defined(__linux__) && defined(__x86_64__) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::lib
{
    extern "C++" inline bool llvm_jit_debug_continue_native_return_event_host_api(
        void* identity, llvm_jit_debug_native_return_event const& event,
        ::std::uint64_t cookie, int owned_descriptor) noexcept
    {
        namespace backend = ::uwvm2::uwvm::debugger::native_step;
#  if __SIZEOF_POINTER__ == 8 && defined(PERF_ATTR_SIZE_VER7) && !defined(__ILP32__)
        backend::details::host_transition_guard guard{};
        auto* const actual{backend::details::active.load(::std::memory_order_acquire)};
        if(!identity || actual == nullptr || static_cast<void*>(actual) != identity) { return false; }
        // Compare the actual owned identity BEFORE reading caller-supplied memory.
        auto& step{*actual};
        if(!::uwvm2::uwvm::debugger::native_perf_signal_linux::available || cookie == 0u || owned_descriptor < 0 ||
           backend::details::active.load(::std::memory_order_acquire) != ::std::addressof(step) ||
           step.state.load(::std::memory_order_seq_cst) != backend::phase::trapped ||
           step.target_thread == 0u || step.next_pc < step.owner_begin || step.next_pc >= step.owner_end ||
           step.trap_revision == UINT64_MAX || !step.activation_provider.valid() ||
           step.register_snapshot.machine != ::uwvm2::uwvm::debugger::native_registers::architecture::x86_64 ||
           step.register_snapshot.pc() != step.next_pc || step.register_snapshot.sp() == 0u) { return false; }
        bool const accepted{event.install(::std::addressof(step), step.target_thread, step.next_pc,
            static_cast<::std::uintptr_t>(step.register_snapshot.sp()), step.owner_begin, step.owner_end, step.trap_revision,
            [&](auto const& provider, auto begin, auto end, auto target, auto stack) noexcept
        {
            // Host-only shared ownership update BEFORE waking the worker. The
            // new provider itself retains the old activation through real ACK.
            step.activation_provider = provider; step.owner_begin = begin; step.owner_end = end;
            step.continuation_pc = target; step.continuation_origin_sp = stack;
        })};
        if(!accepted) { return false; } // Existing trap/TLS/owner stay unchanged.
        backend::details::continuation_failure.store(0u,::std::memory_order_relaxed);
        backend::details::continuation_other_stack_hits.store(0u,::std::memory_order_relaxed);
        step.continuation_cookie = cookie; step.continuation_descriptor = owned_descriptor;
        step.continuation_event_retired.store(false, ::std::memory_order_relaxed);
        step.abort_requested.store(false, ::std::memory_order_seq_cst);
        step.first_pc = step.expected_pc = step.next_pc;
        step.state.store(backend::phase::continuation_running, ::std::memory_order_seq_cst);
        backend::details::wake_gate(step.second_gate); return true;
#  else
        (void)identity; (void)event; (void)cookie; (void)owned_descriptor; return false;
#  endif
    }
}
#endif
