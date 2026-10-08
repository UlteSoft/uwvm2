/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <atomic>
# include <bit>
# include <csignal>
# include <cstring>
# include <limits>
# include <linux/futex.h>
# include <sys/mman.h>
# include <sys/syscall.h>
# include <unistd.h>
# include <fast_io.h>
# include <uwvm2/runtime/lib/native_activation.h>
# include "posix_abi.h"
# include "native_linux_context.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_step
{
    // Ordinary N64 R2 return traps use native-endian instruction bytes and
    // the same closed kernel/stack proof on BE and LE. Public Wasm caller
    // admission remains separately gated by the qualified runtime controller.
    enum class phase : ::std::uint32_t
    { idle, ready, arming, at_guest_pc, executing, trapped, releasing, released, failed, continuation_running, continuation_cancel_pending };
    struct session
    {
        ::std::atomic<phase> state{phase::idle};
        ::std::atomic<::std::uint32_t> first_gate{}, second_gate{};
        ::std::atomic_bool abort_requested{};
        ::std::uint_least64_t target_thread{};
        ::std::uintptr_t owner_begin{}, owner_end{}, expected_pc{}, first_pc{}, next_pc{};
        native_registers::snapshot register_snapshot{};
        ::std::uint64_t trap_revision{};
        bool trap_delay_slot{}; // Fact of the current accepted trap, never a staged successor.
        int first_signal_code{}, second_signal_code{};
        ::std::uintptr_t continuation_pc{}, continuation_origin_sp{};
        bool continuation_call{}, continuation_call_escape{};
        ::std::uint64_t continuation_cookie{};
        int continuation_descriptor{-1};
        ::std::atomic_bool continuation_event_retired{true};
        ::uwvm2::runtime::lib::llvm_jit_debug_native_activation_provider activation_provider{};
        ::uwvm2::runtime::lib::llvm_jit_debug_native_breakpoint_plan plan{};
        ::std::array<bool, 3u> patched{};
#if ((defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))
        ::std::uintptr_t return_skip_pc{};
        ::std::array<unsigned char,4u> return_original{}, return_skip_original{};
        bool return_patched{}, return_skip_patched{};
#endif
    };
    static_assert(::std::atomic<phase>::is_always_lock_free);
    static_assert(::std::atomic<::std::uint32_t>::is_always_lock_free);
    static_assert(::std::atomic_bool::is_always_lock_free);
    namespace details
    {
#if ((defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))
        // Private actual-signal telemetry; no native address or public API.
        inline ::std::atomic_uint return_other_stack_hits{}, return_skip_hits{};
        static_assert(::std::atomic_uint::is_always_lock_free);
#endif
        inline ::std::atomic<session*> active{};
#if defined(UWVM2_TEST_NATIVE_CALL_CONTINUATION_WITNESS)
        inline ::std::atomic<::std::uint32_t> call_descendant_hits{},call_escape_hits{},call_return_hits{};
#endif
        inline constinit thread_local session* current_thread_session{};
        inline constinit thread_local session const* kernel_witness_session{};
        inline constinit thread_local ::std::uintptr_t kernel_witness_pc{}, kernel_witness_sp{};
        inline ::std::atomic_flag host_transition = ATOMIC_FLAG_INIT;
        inline ::std::atomic_bool installed{};
        inline struct ::sigaction previous_action{}, previous_ill_action{};
        inline ::std::size_t page_size{};
        struct host_transition_guard
        {
            host_transition_guard() noexcept { while(host_transition.test_and_set(::std::memory_order_acquire)) {} }
            ~host_transition_guard() { host_transition.clear(::std::memory_order_release); }
            host_transition_guard(host_transition_guard const&) = delete;
        };
        // The runtime only uses this compatibility entry to read the actual TID.
        [[nodiscard]] inline long raw_system_call(long number) noexcept
        { return number == SYS_gettid ? ::fast_io::system_call<SYS_gettid, long>() : -1; }
        [[nodiscard]] inline constexpr unsigned breakpoint_width() noexcept
        {
#if defined(__i386__)
            return 1u;
#elif defined(__s390x__) || (defined(__riscv) && defined(__riscv_compressed))
            return 2u;
#else
            return 4u;
#endif
        }
        [[nodiscard]] inline constexpr ::std::array<unsigned char,4u> breakpoint_bytes() noexcept
        {
#if defined(__aarch64__)
            constexpr ::std::uint32_t instruction{0xd4200000u};
#elif defined(__powerpc__)
            constexpr ::std::uint32_t instruction{0x7fe00008u};
#elif defined(__mips__)
            constexpr ::std::uint32_t instruction{0x0000000du};
#elif defined(__sparc__)
            constexpr ::std::uint32_t instruction{0x91d02001u};
#elif defined(__loongarch64)
            constexpr ::std::uint32_t instruction{0x002a0005u};
#elif defined(__riscv) && defined(__riscv_compressed)
            constexpr ::std::uint32_t instruction{0x9002u};
#elif defined(__riscv)
            constexpr ::std::uint32_t instruction{0x00100073u};
#elif defined(__s390x__)
            constexpr ::std::uint32_t instruction{0x0001u};
#elif defined(__arm__)
            constexpr ::std::uint32_t instruction{0xe1200070u};
#elif defined(__i386__)
            constexpr ::std::uint32_t instruction{0xccu};
#endif
            ::std::array<unsigned char,4u> out{};
            for(unsigned i{}; i != breakpoint_width(); ++i)
            { out[i] = static_cast<unsigned char>(instruction >> (8u * (::std::endian::native == ::std::endian::little ? i : breakpoint_width()-i-1u))); }
            return out;
        }
        inline void wake_gate(::std::atomic<::std::uint32_t>& gate) noexcept
        {
            gate.fetch_add(1u, ::std::memory_order_seq_cst);
            static_cast<void>(::fast_io::system_call<SYS_futex,long>(&gate, static_cast<long>(FUTEX_WAKE_PRIVATE), 1l, nullptr, nullptr, 0l));
        }
        inline void wait_gate(::std::atomic<::std::uint32_t>& gate, ::std::uint32_t value) noexcept
        {
            while(gate.load(::std::memory_order_seq_cst) == value)
            { static_cast<void>(::fast_io::system_call<SYS_futex,long>(&gate, static_cast<long>(FUTEX_WAIT_PRIVATE), static_cast<long>(value), nullptr, nullptr, 0l)); }
        }
        // Only a sealed plan supplies addresses. Check the complete byte range
        // before forming a pointer; no guest register, SP or display PC is read.
        // JIT pages temporarily become RW (never RWX), then regain RX. All other
        // cooperative participants remain parked throughout this operation.
        [[nodiscard]] inline bool write_patch(session& step, unsigned index, bool restore) noexcept
        {
            auto const pc{step.plan.site(index)}; auto const width{step.plan.width()};
            if(index >= step.plan.count() || index >= 3u || width != breakpoint_width() ||
               page_size == 0u || pc < step.owner_begin || pc >= step.owner_end || width > step.owner_end - pc) { return false; }
            auto const first{pc - pc % page_size}; auto const last{pc + width - 1u};
            auto const end_page{last - last % page_size};
            if(end_page > ::std::numeric_limits<::std::uintptr_t>::max() - page_size) { return false; }
            auto const length{end_page + page_size - first};
            auto const original{step.plan.original(index)}; auto const trap{breakpoint_bytes()};
            auto* bytes{reinterpret_cast<unsigned char*>(pc)};
            for(unsigned i{}; i != width; ++i) { if(bytes[i] != (restore ? trap[i] : original[i])) { return false; } }
            if(::fast_io::system_call<SYS_mprotect,long>(reinterpret_cast<void*>(first), length, static_cast<long>(PROT_READ | PROT_WRITE)) != 0) { return false; }
            for(unsigned i{}; i != width; ++i) { bytes[i] = restore ? original[i] : trap[i]; }
            __builtin___clear_cache(reinterpret_cast<char*>(pc), reinterpret_cast<char*>(pc + width));
            if(::fast_io::system_call<SYS_mprotect,long>(reinterpret_cast<void*>(first), length, static_cast<long>(PROT_READ | PROT_EXEC)) != 0)
            { ::fast_io::fast_terminate(); } // Never return with live JIT permissions damaged.
            step.patched[index] = !restore;
            return true;
        }
#if ((defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))
        // Only the closed runtime-issued return seal selects these bytes.
        // All cooperative peers are parked. Never make a JIT page RWX.
        [[nodiscard]] inline bool write_return_patch(session& step, bool skip, bool restore) noexcept
        {
            auto const pc{skip ? step.return_skip_pc : step.continuation_pc};
            auto const width{breakpoint_width()};
            auto const& original{skip ? step.return_skip_original : step.return_original};
            if(page_size==0u || pc<step.owner_begin || pc>=step.owner_end || width>step.owner_end-pc) { return false; }
            auto const first{pc-pc%page_size}, last{pc+width-1u};
            auto const end{last-last%page_size};
            if(end>UINTPTR_MAX-page_size) { return false; }
            auto* bytes{reinterpret_cast<unsigned char*>(pc)};auto const trap{breakpoint_bytes()};
            for(unsigned b{};b!=width;++b) { if(bytes[b]!=(restore?trap[b]:original[b])) { return false; } }
            if(::fast_io::system_call<SYS_mprotect,long>(reinterpret_cast<void*>(first),end+page_size-first,
                static_cast<long>(PROT_READ|PROT_WRITE))!=0) { return false; }
            for(unsigned b{};b!=width;++b) { bytes[b]=restore?original[b]:trap[b]; }
            __builtin___clear_cache(reinterpret_cast<char*>(pc),reinterpret_cast<char*>(pc+width));
            if(::fast_io::system_call<SYS_mprotect,long>(reinterpret_cast<void*>(first),end+page_size-first,
                static_cast<long>(PROT_READ|PROT_EXEC))!=0) { ::fast_io::fast_terminate(); }
            (skip?step.return_skip_patched:step.return_patched)=!restore;return true;
        }
#endif
        [[nodiscard]] inline bool restore_all(session& step) noexcept
        {
            bool okay{true};
            for(unsigned i{}; i != 3u; ++i) { if(step.patched[i] && !write_patch(step,i,true)) { okay = false; } }
#if ((defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))
            if(step.return_patched && !write_return_patch(step,false,true)) { okay=false; }
            if(step.return_skip_patched && !write_return_patch(step,true,true)) { okay=false; }
#endif
            return okay;
        }
        [[nodiscard]] inline bool install_successors(session& step, bool initial) noexcept
        {
            unsigned const begin{initial ? 1u : 0u};
            if(step.plan.count() <= begin || step.plan.count() > 3u) { return false; }
            for(unsigned i{begin}; i != step.plan.count(); ++i)
            {
                if(!write_patch(step,i,false))
                { if(!restore_all(step)) { ::fast_io::fast_terminate(); } return false; }
            }
            return true;
        }
        inline void forward_trap(int number, ::siginfo_t* info, void* context) noexcept
        {
            auto const& previous_action{
#if defined(__s390x__)
                number == SIGILL ? details::previous_ill_action :
#endif
                details::previous_action};
            if(previous_action.sa_handler == SIG_IGN) { return; }
            if(previous_action.sa_handler != SIG_DFL)
            {
                if((previous_action.sa_flags & SA_SIGINFO) != 0) { previous_action.sa_sigaction(number,info,context); }
                else { previous_action.sa_handler(number); }
                return;
            }
            static_cast<void>(posix_abi::sigaction_noexcept(number,&previous_action,nullptr));
            static_cast<void>(posix_abi::raise_noexcept(number));
        }
        [[nodiscard]] inline bool breakpoint_signal(int number, int code) noexcept
        {
#if defined(__s390x__)
            return number == SIGILL && (code == ILL_ILLOPC || code == ILL_ILLOPN);
#elif defined(__i386__)
            return number == SIGTRAP && (code == TRAP_BRKPT || code == SI_KERNEL);
#else
            return number == SIGTRAP && code == TRAP_BRKPT;
#endif
        }
        inline void retire_execution_on_worker() noexcept
        {
            auto* const step{current_thread_session};
            if(!step || active.load(::std::memory_order_acquire) != step) { return; }
            auto const state{step->state.load(::std::memory_order_seq_cst)};
            if(state != phase::arming && state != phase::executing) { return; }
            // A genuine Wasm trap/unwind may bypass the patched successor.
            // Retire from this actual worker, restore all owned bytes, and
            // publish the ACK only after no worker borrow remains.
            if(!restore_all(*step)) { ::fast_io::fast_terminate(); }
            step->abort_requested.store(true,::std::memory_order_seq_cst);
            step->register_snapshot = {}; current_thread_session = nullptr;
            step->state.store(phase::released,::std::memory_order_seq_cst);
        }
        inline void handler(int number, ::siginfo_t* info, void* pointer) noexcept
        {
            auto* const step{current_thread_session};
            if((number != SIGTRAP
#if defined(__s390x__)
                && number != SIGILL
#endif
                ) || !info || !breakpoint_signal(number,info->si_code) || !pointer || !step ||
               active.load(::std::memory_order_acquire) != step ||
               static_cast<::std::uint_least64_t>(raw_system_call(SYS_gettid)) != step->target_thread)
            { forward_trap(number,info,pointer); return; }
            auto const state{step->state.load(::std::memory_order_seq_cst)};
            if(state != phase::arming && state != phase::executing &&
               state != phase::continuation_running && state != phase::continuation_cancel_pending) { forward_trap(number,info,pointer); return; }
            auto& context{*static_cast<native_linux_context::kernel_context*>(pointer)};
            auto raw{native_linux_context::capture(context)};
            auto pc{static_cast<::std::uintptr_t>(raw.pc())};
#if defined(__i386__)
            if(pc == 0u) { forward_trap(number,info,pointer); return; } --pc;
#elif defined(__s390x__)
            if(pc < 2u) { forward_trap(number,info,pointer); return; } pc -= 2u;
#endif
            bool found{};
            for(unsigned i{}; i != step->plan.count(); ++i) { if(step->patched[i] && step->plan.site(i) == pc) { found = true; } }
            bool const continuing{state==phase::continuation_running || state==phase::continuation_cancel_pending};
            if(continuing && step->continuation_call)
            {
                bool const hit_return{step->patched[0u] && pc==step->plan.site(0u)};
                bool hit_escape{};
                for(unsigned i{1u};i<step->plan.count();++i)
                { hit_escape=hit_escape || (step->patched[i] && pc==step->plan.site(i)); }
                if(!found || (!hit_return && !hit_escape) || pc<step->owner_begin || pc>=step->owner_end || raw.sp()==0u)
                { forward_trap(number,info,pointer);return; }
                // Restore the intercepted instruction and architectural PC
                // before cancellation or a private recursion escape resumes.
                if(!restore_all(*step)) { ::fast_io::fast_terminate(); }
                native_linux_context::set_pc(context,pc);raw.values[native_registers::pc_index(raw.machine)]=pc;
                if(state==phase::continuation_cancel_pending || step->abort_requested.load(::std::memory_order_seq_cst))
                {
                    current_thread_session=nullptr;step->register_snapshot={};
                    step->state.store(phase::released,::std::memory_order_seq_cst);return;
                }
                // TailCC may pop an outgoing argument area on return. Stack
                // equality alone neither proves this caller nor distinguishes
                // recursion. The real original incarnation chain does both.
                bool const kernel_pair{
#if defined(__sparc__) && defined(__arch64__)
                    hit_return ? (pc<=UINTPTR_MAX-4u && raw.values[33u]==pc+4u) :
                        step->plan.accepts_kernel_npc(pc,raw.values[33u],false)
#else
                    true
#endif
                };
                kernel_witness_session=step;kernel_witness_pc=pc;kernel_witness_sp=raw.sp();
                bool const original_caller{hit_return && !step->continuation_call_escape && kernel_pair &&
                    step->activation_provider.witness(step,step->target_thread,pc,raw.sp())};
                kernel_witness_session=nullptr;kernel_witness_pc=kernel_witness_sp=0u;
                if(!original_caller)
                {
                    kernel_witness_session=step;kernel_witness_pc=pc;kernel_witness_sp=raw.sp();
                    bool const descendant{kernel_pair &&
                        step->activation_provider.descendant(step,step->target_thread,pc,raw.sp())};
                    kernel_witness_session=nullptr;kernel_witness_pc=kernel_witness_sp=0u;
                    if(!descendant || hit_escape!=step->continuation_call_escape)
                    {
                        step->abort_requested.store(true,::std::memory_order_seq_cst);
                        current_thread_session=nullptr;step->register_snapshot={};
                        step->state.store(phase::released,::std::memory_order_seq_cst);return;
                    }
                    // Neither descendant snapshot is published. Both sites and
                    // their intervening ordinary instruction were sealed before
                    // the original worker was woken.
#if defined(UWVM2_TEST_NATIVE_CALL_CONTINUATION_WITNESS)
                    (hit_return?call_descendant_hits:call_escape_hits).fetch_add(1u,::std::memory_order_relaxed);
#endif
                    step->continuation_call_escape=hit_return;
                    if(hit_return)
                    {
                        for(unsigned i{1u};i<step->plan.count();++i)
                        { if(!write_patch(*step,i,false)) { ::fast_io::fast_terminate(); } }
                    }
                    else if(!write_patch(*step,0u,false)) { ::fast_io::fast_terminate(); }
                    return;
                }
                if(step->continuation_call_escape) { ::fast_io::fast_terminate(); }
#if defined(UWVM2_TEST_NATIVE_CALL_CONTINUATION_WITNESS)
                call_return_hits.fetch_add(1u,::std::memory_order_relaxed);
#endif
            }
#if ((defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))
            bool const returning{continuing && !step->continuation_call};
            if(returning)
            {
                bool const hit_return{step->return_patched && pc==step->continuation_pc};
                bool const hit_skip{step->return_skip_patched && pc==step->return_skip_pc};
                if((!hit_return && !hit_skip) || pc<step->owner_begin || pc>=step->owner_end ||
                   (raw.machine!=native_registers::architecture::riscv64 && raw.machine!=native_registers::architecture::aarch64 && raw.machine!=native_registers::architecture::i686 && raw.machine!=native_registers::architecture::loongarch64 && raw.machine!=native_registers::architecture::mips64) || raw.sp()==0u)
                { forward_trap(number,info,pointer);return; }
                // i386 INT3 reports EIP after the patched byte. Every private
                // replay/cancellation resumes at the original instruction;
                // no descendant register snapshot is published.
                native_linux_context::set_pc(context,pc);
                raw.values[native_registers::pc_index(raw.machine)]=pc;
                if(state==phase::continuation_cancel_pending || step->abort_requested.load(::std::memory_order_seq_cst))
                {
                    if(!restore_all(*step)) { ::fast_io::fast_terminate(); }
                    current_thread_session=nullptr;step->register_snapshot={};step->state.store(phase::released,::std::memory_order_seq_cst);return;
                }
                auto const authentic_descendant{[&]() noexcept
                {
                    kernel_witness_session=step;kernel_witness_pc=pc;kernel_witness_sp=raw.sp();
                    bool const actual{step->activation_provider.descendant(step,step->target_thread,pc,raw.sp())};
                    kernel_witness_session=nullptr;kernel_witness_pc=kernel_witness_sp=0u;
                    return actual;
                }};
                if((hit_skip || raw.sp()!=step->continuation_origin_sp) && !authentic_descendant())
                {
                    if(!restore_all(*step)) { ::fast_io::fast_terminate(); }
                    step->abort_requested.store(true,::std::memory_order_seq_cst);
                    current_thread_session=nullptr;step->register_snapshot={};step->state.store(phase::released,::std::memory_order_seq_cst);return;
                }
                if(hit_skip)
                {
                    if(!write_return_patch(*step,true,true) || !write_return_patch(*step,false,false)) { ::fast_io::fast_terminate(); }
                    return_skip_hits.fetch_add(1u,::std::memory_order_relaxed);
                    return; // Real ordinary instruction passed; re-arm exact PC.
                }
                if(raw.sp()!=step->continuation_origin_sp)
                {
                    if(!write_return_patch(*step,false,true) || !write_return_patch(*step,true,false)) { ::fast_io::fast_terminate(); }
                    return_other_stack_hits.fetch_add(1u,::std::memory_order_relaxed);
                    return; // Another recursion incarnation, never a parent stop.
                }
                found=true;
            }
#endif
            if(!found || pc < step->owner_begin || pc >= step->owner_end || raw.size() == 0u || raw.sp() == 0u)
            { forward_trap(number,info,pointer); return; }
            if(!restore_all(*step)) { ::fast_io::fast_terminate(); }
            native_linux_context::set_pc(context,pc); raw.values[native_registers::pc_index(raw.machine)] = pc;
            kernel_witness_session = step; kernel_witness_pc = pc; kernel_witness_sp = raw.sp();
            bool const witness{
#if defined(__sparc__)
                (continuing && step->continuation_call
                    ? (pc<=UINTPTR_MAX-4u && raw.values[33u]==pc+4u)
                    : step->plan.accepts_kernel_npc(pc,raw.values[33u],state==phase::arming)) &&
#endif
                step->activation_provider.witness(step,step->target_thread,pc,raw.sp())};
            kernel_witness_session = nullptr; kernel_witness_pc = kernel_witness_sp = 0u;
            auto const first{state == phase::arming};
            if(!witness || step->trap_revision == UINT64_MAX || step->abort_requested.load(::std::memory_order_seq_cst))
            {
                step->register_snapshot = {}; current_thread_session = nullptr;
                step->state.store(phase::released,::std::memory_order_seq_cst); return;
            }
            if(first) { step->first_pc = pc; step->first_signal_code = info->si_code; }
            else { step->next_pc = pc; step->second_signal_code = info->si_code; ++step->trap_revision; }
            step->register_snapshot = raw;
            step->trap_delay_slot = false;
#if defined(__sparc__) && defined(__arch64__)
            step->trap_delay_slot = !first && step->plan.in_delay_slot(pc,raw.values[33u]);
#endif
            auto& gate{first ? step->first_gate : step->second_gate};
            auto const value{gate.load(::std::memory_order_seq_cst)};
            auto const parked{first ? phase::at_guest_pc : phase::trapped};
            step->state.store(parked,::std::memory_order_seq_cst);
            // Reciprocal cancellation check closes publish-before-sleep races.
            if(step->abort_requested.load(::std::memory_order_seq_cst))
            { auto expected{parked}; static_cast<void>(step->state.compare_exchange_strong(expected,phase::releasing,::std::memory_order_seq_cst)); }
            if(step->state.load(::std::memory_order_seq_cst) == parked) { wait_gate(gate,value); }
            if(step->state.load(::std::memory_order_seq_cst) != phase::executing &&
               step->state.load(::std::memory_order_seq_cst) != phase::continuation_running)
            {
                if(!restore_all(*step)) { ::fast_io::fast_terminate(); }
                current_thread_session = nullptr; step->register_snapshot = {};
                step->state.store(phase::released,::std::memory_order_seq_cst);
            }
        }
    }
    [[nodiscard]] inline constexpr bool platform_available() noexcept { return true; }
    [[nodiscard]] inline bool install() noexcept
    {
        details::host_transition_guard lock{};
        if(details::installed.load(::std::memory_order_acquire)) { return true; }
        auto const page{::sysconf(_SC_PAGESIZE)};
        if(page <= 0 || (static_cast<unsigned long>(page) & (static_cast<unsigned long>(page)-1u)) != 0u) { return false; }
        details::page_size = static_cast<::std::size_t>(page);
#if defined(__i386__)
        unsigned eax{},ebx{},ecx{},edx{};
        native_linux_context::kernel_has_fxsr = __get_cpuid(1u,&eax,&ebx,&ecx,&edx) && (edx & (1u << 24u)) != 0u;
#endif
#if defined(__powerpc__)
        native_linux_context::kernel_has_altivec = (::getauxval(AT_HWCAP) & 0x10000000ul) != 0u;
#endif
        // Resolve generic syscall/cache ABI paths on this cold host operation.
        static_cast<void>(details::raw_system_call(SYS_gettid));
        struct ::sigaction action{}; action.sa_sigaction = details::handler; action.sa_flags = SA_SIGINFO | SA_ONSTACK;
        posix_abi::sigemptyset_noexcept(&action.sa_mask);
        if(posix_abi::sigaction_noexcept(SIGTRAP,&action,&details::previous_action) != 0) { return false; }
#if defined(__s390x__)
        if(posix_abi::sigaction_noexcept(SIGILL,&action,&details::previous_ill_action) != 0)
        { static_cast<void>(posix_abi::sigaction_noexcept(SIGTRAP,&details::previous_action,nullptr)); return false; }
#endif
        details::installed.store(true,::std::memory_order_release); return true;
    }
    [[nodiscard]] inline bool uninstall() noexcept
    {
        details::host_transition_guard lock{};
        if(details::active.load(::std::memory_order_acquire)) { return false; }
        if(!details::installed.load(::std::memory_order_acquire)) { return true; }
        if(posix_abi::sigaction_noexcept(SIGTRAP,&details::previous_action,nullptr) != 0) { return false; }
#if defined(__s390x__)
        if(posix_abi::sigaction_noexcept(SIGILL,&details::previous_ill_action,nullptr) != 0) { return false; }
#endif
        details::installed.store(false,::std::memory_order_release); return true;
    }
    [[nodiscard]] inline bool can_arm_on_current_thread() noexcept
    {
        ::sigset_t blocked{};
        // libc's complete ABI handles MIPS's larger kernel signal mask too.
        extern int sigprocmask_sdk_noexcept(int, ::sigset_t const*, ::sigset_t*) noexcept __asm__("sigprocmask");
        extern int sigismember_sdk_noexcept(::sigset_t const*, int) noexcept __asm__("sigismember");
        return sigprocmask_sdk_noexcept(SIG_BLOCK,nullptr,&blocked) == 0 && sigismember_sdk_noexcept(&blocked,SIGTRAP) == 0
#if defined(__s390x__)
            && sigismember_sdk_noexcept(&blocked,SIGILL) == 0
#endif
            ;
    }
    [[nodiscard]] inline bool request(session& step, ::std::uint_least64_t thread, ::std::uintptr_t begin,
        ::std::uintptr_t end, ::std::uintptr_t pc,
        ::uwvm2::runtime::lib::llvm_jit_debug_native_activation_provider provider,
        ::uwvm2::runtime::lib::llvm_jit_debug_native_breakpoint_plan const& plan) noexcept
    {
        details::host_transition_guard lock{};
        if(!details::installed.load(::std::memory_order_acquire) || details::active.load(::std::memory_order_acquire) ||
           step.state.load(::std::memory_order_seq_cst) != phase::idle || thread == 0u || begin == 0u || end <= begin ||
           pc < begin || pc >= end || !provider.bound_to(&step,thread,begin,end,pc) ||
           !plan.bound_to(&step,thread,begin,end,pc,step.trap_revision,true) || plan.site(0u) != pc) { return false; }
        step.target_thread = thread; step.owner_begin = begin; step.owner_end = end; step.expected_pc = pc;
        step.first_pc = step.next_pc = 0u; step.register_snapshot = {}; step.trap_delay_slot = false;
        step.first_signal_code = step.second_signal_code = 0;
        step.first_gate.store(0u,::std::memory_order_relaxed); step.second_gate.store(0u,::std::memory_order_relaxed);
        step.abort_requested.store(false,::std::memory_order_seq_cst); step.activation_provider = provider; step.plan = plan;
        if(!details::write_patch(step,0u,false)) { step.plan = {}; step.activation_provider = {}; return false; }
        step.state.store(phase::ready,::std::memory_order_seq_cst); details::active.store(&step,::std::memory_order_release); return true;
    }
    // Integer-only requests can never patch executable memory.
    [[nodiscard]] inline bool request(session&, ::std::uint_least64_t, ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t) noexcept { return false; }
    [[nodiscard]] inline bool arm_at_return(::std::uintptr_t pc) noexcept
    {
        if(details::current_thread_session || !can_arm_on_current_thread()) { return false; }
        details::host_transition_guard lock{}; auto* const step{details::active.load(::std::memory_order_acquire)};
        if(!step || pc != step->expected_pc || static_cast<::std::uint_least64_t>(details::raw_system_call(SYS_gettid)) != step->target_thread) { return false; }
        auto expected{phase::ready};
        if(!step->state.compare_exchange_strong(expected,phase::arming,::std::memory_order_seq_cst)) { return false; }
        details::current_thread_session = step; return true;
    }
    [[nodiscard]] inline bool stage_plan(session& step, ::uwvm2::runtime::lib::llvm_jit_debug_native_breakpoint_plan const& plan) noexcept
    {
        details::host_transition_guard lock{};
        if(details::active.load(::std::memory_order_acquire) != &step || step.state.load(::std::memory_order_seq_cst) != phase::trapped ||
           step.abort_requested.load(::std::memory_order_seq_cst) ||
           !plan.bound_to(&step,step.target_thread,step.owner_begin,step.owner_end,step.next_pc,step.trap_revision,false)) { return false; }
        step.plan = plan; return true;
    }
    [[nodiscard]] inline bool continue_one(session& step) noexcept
    {
        details::host_transition_guard lock{};
        if(details::active.load(::std::memory_order_acquire) != &step || step.state.load(::std::memory_order_seq_cst) != phase::at_guest_pc ||
           step.abort_requested.load(::std::memory_order_seq_cst) || step.trap_revision == UINT64_MAX ||
           !details::install_successors(step,true)) { return false; }
        step.state.store(phase::executing,::std::memory_order_seq_cst); details::wake_gate(step.first_gate); return true;
    }
    [[nodiscard]] inline bool continue_from_trap(session& step) noexcept
    {
        details::host_transition_guard lock{};
        if(details::active.load(::std::memory_order_acquire) != &step || step.state.load(::std::memory_order_seq_cst) != phase::trapped ||
           step.abort_requested.load(::std::memory_order_seq_cst) || step.trap_revision == UINT64_MAX ||
           !step.plan.bound_to(&step,step.target_thread,step.owner_begin,step.owner_end,step.next_pc,step.trap_revision,false) ||
           !details::install_successors(step,false)) { return false; }
        step.state.store(phase::executing,::std::memory_order_seq_cst); details::wake_gate(step.second_gate); return true;
    }
    [[nodiscard]] inline bool in_owned_kernel_activation_witness(void const* identity, ::std::uint_least64_t thread,
        ::std::uintptr_t pc, ::std::uintptr_t sp) noexcept
    { return details::kernel_witness_session && identity == details::kernel_witness_session &&
        thread == details::kernel_witness_session->target_thread && pc == details::kernel_witness_pc && sp == details::kernel_witness_sp; }
    inline void report_activation_leave_on_worker(::std::uint64_t incarnation, unsigned kind) noexcept
    { auto* const step{details::current_thread_session}; if(step) { step->activation_provider.report_leave(step,incarnation,kind); details::retire_execution_on_worker(); } }
    [[nodiscard]] inline bool skip_pause_for_continuation() noexcept
    {
        auto* step{details::current_thread_session};
        if(!step || details::active.load(::std::memory_order_acquire)!=step) { return false; }
        auto const state{step->state.load(::std::memory_order_seq_cst)};
        if(state==phase::continuation_running && !step->abort_requested.load(::std::memory_order_seq_cst)) { return true; }
        if(state==phase::continuation_cancel_pending)
        {
            while(!step->continuation_event_retired.load(::std::memory_order_acquire))
            { static_cast<void>(::fast_io::system_call<SYS_sched_yield,long>()); }
            // Software traps have no queued perf delivery. Only this actual
            // worker restores every owned byte before publishing its ACK.
            if(!details::restore_all(*step)) { ::fast_io::fast_terminate(); }
            details::current_thread_session=nullptr;step->register_snapshot={};
            step->state.store(phase::released,::std::memory_order_seq_cst);
        }
        return false;
    }
    inline void cancel_continuation_on_worker() noexcept
    {
        auto* step{details::current_thread_session};
        if(step && details::active.load(::std::memory_order_acquire)==step)
        {
            auto expected{phase::continuation_running};
            if(step->state.compare_exchange_strong(expected,phase::continuation_cancel_pending,::std::memory_order_seq_cst))
            { step->abort_requested.store(true,::std::memory_order_seq_cst);return; }
        }
        details::retire_execution_on_worker();
    }
    inline void acknowledge_continuation_execution_exit() noexcept
    { cancel_continuation_on_worker();static_cast<void>(skip_pause_for_continuation()); }
#if ((defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))
    [[nodiscard]] inline bool continue_to_return(session& step,
        ::uwvm2::runtime::lib::llvm_jit_debug_native_return_event const& event, ::std::uint64_t cookie,int descriptor) noexcept
    { return ::uwvm2::runtime::lib::llvm_jit_debug_continue_native_return_event_host_api(&step,event,cookie,descriptor); }
#endif
    inline void acknowledge_continuation_event_retired(session& step) noexcept { step.continuation_event_retired.store(true,::std::memory_order_release); }
    [[nodiscard]] inline bool release(session& step) noexcept
    {
        details::host_transition_guard lock{};
        if(details::active.load(::std::memory_order_acquire) != &step) { return false; }
        step.abort_requested.store(true,::std::memory_order_seq_cst);
        for(;;)
        {
            auto state{step.state.load(::std::memory_order_seq_cst)};
            if(state == phase::ready)
            {
                if(!details::restore_all(step)) { ::fast_io::fast_terminate(); }
                step.state.store(phase::released,::std::memory_order_seq_cst); return true;
            }
            if(state == phase::arming || state == phase::executing) { return true; }
            if(state==phase::continuation_running)
            { if(step.state.compare_exchange_strong(state,phase::continuation_cancel_pending,::std::memory_order_seq_cst)) { return true; }continue; }
            if(state==phase::continuation_cancel_pending) { return true; }
            if(state == phase::at_guest_pc || state == phase::trapped || state == phase::failed)
            {
                bool const first{state == phase::at_guest_pc};
                if(!step.state.compare_exchange_strong(state,phase::releasing,::std::memory_order_seq_cst)) { continue; }
                details::wake_gate(first ? step.first_gate : step.second_gate); return true;
            }
            return state == phase::releasing || state == phase::released;
        }
    }
    template<typename Inspect>
    [[nodiscard]] inline bool with_owned_registers_and_revision(void const* identity, Inspect inspect) noexcept
    {
        details::host_transition_guard lock{}; auto const* step{details::active.load(::std::memory_order_acquire)};
        if(!step || identity != step || step->state.load(::std::memory_order_seq_cst) != phase::trapped ||
           step->abort_requested.load(::std::memory_order_seq_cst) || step->trap_revision == 0u ||
           step->next_pc < step->owner_begin || step->next_pc >= step->owner_end ||
           step->register_snapshot.size() == 0u || step->register_snapshot.pc() != step->next_pc) { return false; }
        auto const snapshot{step->register_snapshot};
        if constexpr(requires { inspect(step->target_thread,step->next_pc,step->owner_begin,step->owner_end,snapshot,step->trap_revision,step->trap_delay_slot); })
        { inspect(step->target_thread,step->next_pc,step->owner_begin,step->owner_end,snapshot,step->trap_revision,step->trap_delay_slot); }
        else { inspect(step->target_thread,step->next_pc,step->owner_begin,step->owner_end,snapshot,step->trap_revision); }
        return true;
    }
    template<typename Inspect>
    [[nodiscard]] inline bool with_owned_registers(void const* identity, Inspect inspect) noexcept
    { return with_owned_registers_and_revision(identity,[&](auto t,auto p,auto b,auto e,auto const& r,auto) noexcept { inspect(t,p,b,e,r); }); }
    template<typename Inspect>
    [[nodiscard]] inline bool with_owned_trap(void const* identity, Inspect inspect) noexcept
    { return with_owned_registers(identity,[&](auto t,auto p,auto b,auto e,auto const&) noexcept { inspect(t,p,b,e); }); }
    [[nodiscard]] inline bool clear(session& step) noexcept
    {
        details::host_transition_guard lock{};
        if(details::active.load(::std::memory_order_acquire) != &step || step.state.load(::std::memory_order_seq_cst) != phase::released) { return false; }
        for(auto patched : step.patched) { if(patched) { return false; } }
#if ((defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))
        if(step.return_patched || step.return_skip_patched) { return false; }
        step.return_skip_pc=0u;step.return_original={};step.return_skip_original={};
#endif
        step.continuation_call=step.continuation_call_escape=false;
        step.continuation_pc=step.continuation_origin_sp=step.continuation_cookie=0u;
        details::active.store(nullptr,::std::memory_order_release); step.plan = {}; step.activation_provider = {}; step.register_snapshot = {}; step.trap_delay_slot = false;
        step.state.store(phase::idle,::std::memory_order_seq_cst); return true;
    }
}

#if ((defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips))) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::lib
{
    extern "C++" inline bool llvm_jit_debug_continue_native_return_event_host_api(
        void* identity,llvm_jit_debug_native_return_event const& event,::std::uint64_t cookie,int descriptor) noexcept
    {
        namespace b=::uwvm2::uwvm::debugger::native_step;
        b::details::host_transition_guard guard{};
        auto* actual{b::details::active.load(::std::memory_order_acquire)};
        if(!identity || !actual || static_cast<void*>(actual)!=identity || cookie==0u || descriptor!=-1) { return false; }
        auto& step{*actual};
        if(step.state.load(::std::memory_order_seq_cst)!=b::phase::trapped ||
           step.abort_requested.load(::std::memory_order_seq_cst) || step.trap_revision==UINT64_MAX ||
           (step.register_snapshot.machine!=::uwvm2::uwvm::debugger::native_registers::architecture::riscv64 &&
            step.register_snapshot.machine!=::uwvm2::uwvm::debugger::native_registers::architecture::aarch64 &&
            step.register_snapshot.machine!=::uwvm2::uwvm::debugger::native_registers::architecture::i686 &&
            step.register_snapshot.machine!=::uwvm2::uwvm::debugger::native_registers::architecture::loongarch64 &&
            step.register_snapshot.machine!=::uwvm2::uwvm::debugger::native_registers::architecture::mips64) ||
           step.register_snapshot.pc()!=step.next_pc || step.register_snapshot.sp()==0u ||
           step.return_patched || step.return_skip_patched) { return false; }
        bool installed{};
        bool const accepted{event.install(&step,step.target_thread,step.next_pc,step.register_snapshot.sp(),
            step.owner_begin,step.owner_end,step.trap_revision,
            [&](auto const& provider,auto begin,auto end,auto target,auto stack,auto skip,auto const& original,auto const& skip_original) noexcept
        {
            constexpr auto width{b::details::breakpoint_width()};
            if(skip<=target || target<begin || skip>=end || width>skip-target || width>end-skip) { return; }
            auto const old_begin{step.owner_begin},old_end{step.owner_end};
            step.owner_begin=begin;step.owner_end=end;step.continuation_pc=target;
            step.return_skip_pc=skip;step.return_original=original;step.return_skip_original=skip_original;
            if(!b::details::write_return_patch(step,false,false))
            { step.owner_begin=old_begin;step.owner_end=old_end;step.continuation_pc=step.return_skip_pc=0u;return; }
            step.activation_provider=provider;step.continuation_origin_sp=stack;installed=true;
        })};
        if(!accepted || !installed) { return false; }
        step.continuation_call=step.continuation_call_escape=false;
        step.continuation_cookie=cookie;step.continuation_descriptor=-1;
        step.continuation_event_retired.store(false,::std::memory_order_relaxed);
        step.first_pc=step.expected_pc=step.next_pc;
        step.state.store(b::phase::continuation_running,::std::memory_order_seq_cst);
        b::details::wake_gate(step.second_gate);return true;
    }
}
#endif

#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::lib
{
    extern "C++" inline bool llvm_jit_debug_continue_native_call_event_host_api(
        void* identity,llvm_jit_debug_native_call_event const& event) noexcept
    {
        namespace b=::uwvm2::uwvm::debugger::native_step;
        b::details::host_transition_guard guard{};
        auto* actual{b::details::active.load(::std::memory_order_acquire)};
        if(!identity || !actual || static_cast<void*>(actual)!=identity) { return false; }
        auto& step{*actual};
        if(step.state.load(::std::memory_order_seq_cst)!=b::phase::trapped ||
           step.abort_requested.load(::std::memory_order_seq_cst) || step.trap_revision==UINT64_MAX ||
           step.register_snapshot.pc()!=step.next_pc || step.register_snapshot.sp()==0u) { return false; }
        for(auto patched:step.patched) { if(patched) { return false; } }
#if ((defined(__riscv) && __riscv_xlen == 64) || (defined(__aarch64__) && defined(__AARCH64EL__) && __SIZEOF_POINTER__ == 8) || (defined(__loongarch64) && __SIZEOF_POINTER__ == 8) || (defined(__i386__) && __SIZEOF_POINTER__ == 4) || (defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8 && defined(__mips_isa_rev) && __mips_isa_rev == 2 && !defined(__mips16) && !defined(__mips_micromips)))
        if(step.return_patched || step.return_skip_patched) { return false; }
#endif
        bool const installed{event.install(&step,step.target_thread,step.owner_begin,step.owner_end,
            step.next_pc,step.register_snapshot.sp(),step.trap_revision,[&](auto const& plan,auto stack) noexcept
        {
            auto const width{b::details::breakpoint_width()};
            if(plan.count()<2u || plan.count()>3u || plan.width()!=width) { return false; }
            for(unsigned i{};i<plan.count();++i)
            {
                auto const pc{plan.site(i)};
                if(pc<step.owner_begin || pc>=step.owner_end || width>step.owner_end-pc) { return false; }
                for(unsigned j{};j<i;++j)
                { if(pc>plan.site(j) ? pc-plan.site(j)<width : plan.site(j)-pc<width) { return false; } }
            }
            auto const old{step.plan};step.plan=plan;
            if(!b::details::write_patch(step,0u,false)) { step.plan=old;return false; }
            step.continuation_origin_sp=stack;step.continuation_pc=plan.site(0u);
            step.continuation_call=true;step.continuation_call_escape=false;return true;
        })};
        if(!installed) { return false; }
        step.continuation_event_retired.store(true,::std::memory_order_release);
        step.first_pc=step.expected_pc=step.next_pc;
        step.state.store(b::phase::continuation_running,::std::memory_order_seq_cst);
        b::details::wake_gate(step.second_gate);return true;
    }
}
#endif
