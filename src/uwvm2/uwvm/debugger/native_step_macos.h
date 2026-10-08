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
# include <fast_io_freestanding.h>
# if defined(__APPLE__)
#  include <TargetConditionals.h>
# endif
# if defined(__APPLE__) && (defined(__aarch64__) || (defined(__x86_64__) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT == 1)) && TARGET_OS_OSX
#  include <mach/mach.h>
#  if defined(__aarch64__)
#   include <mach/arm/thread_status.h>
#   include <mach/arm/exception.h>
#  else
#   include <mach/i386/thread_status.h>
#   include <mach/i386/exception.h>
#   include <cerrno>
#  endif
#  include <mach/exc.h>
#  include <Security/SecTask.h>
#  include <CoreFoundation/CoreFoundation.h>
#  include <pthread.h>
#  include <unistd.h>
#  include "posix_abi.h"
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

// An opt-in, thread-scoped Mach exception server. The normal JIT execution
// path has no machine-step branch, debug register mutation, or exception port.
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_step_macos
{
    enum class phase : ::std::uint32_t
    {
        idle, ready, arming, at_guest_pc, executing, trapped,
        releasing, released, failed
    };
    struct session
    {
        ::std::atomic<phase> state{phase::idle};
        ::std::atomic_bool abort_requested{};
        ::std::uint_least64_t target_thread{};
        ::std::uintptr_t owner_begin{}, owner_end{}, expected_pc{};
        ::std::uintptr_t first_pc{}, next_pc{};
        native_registers::snapshot register_snapshot{};
        int first_exception_code{}, second_exception_code{};
#if defined(__APPLE__) && (defined(__aarch64__) || (defined(__x86_64__) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT == 1)) && TARGET_OS_OSX
        ::thread_act_t target_port{MACH_PORT_NULL};
        ::std::uint64_t target_thread_id{};
        ::semaphore_t first_gate{MACH_PORT_NULL}, second_gate{MACH_PORT_NULL};
        ::mach_port_t previous_port{MACH_PORT_NULL};
        ::exception_behavior_t previous_behavior{EXCEPTION_DEFAULT};
        ::thread_state_flavor_t previous_flavor{THREAD_STATE_NONE};
# if defined(__aarch64__)
        ::arm_debug_state64_t original_debug{};
# else
        ::x86_debug_state64_t original_debug{};
# endif
        bool previous_port_valid{};
#endif
    };
    static_assert(::std::atomic<phase>::is_always_lock_free);
    static_assert(::std::atomic_bool::is_always_lock_free);
    static_assert(::std::atomic<session*>::is_always_lock_free);

#if defined(__APPLE__) && (defined(__aarch64__) || (defined(__x86_64__) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT == 1)) && TARGET_OS_OSX
    // These are distinct C names bound directly to the SDK's Mach-O symbols.
    // The original SDK parameter/result types and callback ABI stay intact;
    // noexcept belongs to the called function itself, not a lambda around a
    // potentially throwing C declaration. mach_task_self() is a SDK data macro.
    namespace abi
    {
# if defined(__x86_64__)
        extern "C" int sysctlbyname_noexcept(char const*, void*, ::std::size_t*, void*, ::std::size_t) noexcept __asm__("_sysctlbyname");
        extern "C" int* error_noexcept() noexcept __asm__("___error");
# endif
        extern "C" ::SecTaskRef SecTaskCreateFromSelf_noexcept(::CFAllocatorRef) noexcept __asm__("_SecTaskCreateFromSelf");
        extern "C" ::CFTypeRef SecTaskCopyValueForEntitlement_noexcept(::SecTaskRef, ::CFStringRef, ::CFErrorRef*) noexcept
            __asm__("_SecTaskCopyValueForEntitlement");
        extern "C" void CFRelease_noexcept(::CFTypeRef) noexcept __asm__("_CFRelease");
        extern "C" ::CFTypeID CFGetTypeID_noexcept(::CFTypeRef) noexcept __asm__("_CFGetTypeID");
        extern "C" ::CFTypeID CFNumberGetTypeID_noexcept() noexcept __asm__("_CFNumberGetTypeID");
        extern "C" ::CFTypeID CFBooleanGetTypeID_noexcept() noexcept __asm__("_CFBooleanGetTypeID");
        extern "C" ::Boolean CFNumberGetValue_noexcept(::CFNumberRef, ::CFNumberType, void*) noexcept __asm__("_CFNumberGetValue");
        extern "C" ::kern_return_t task_threads_noexcept(::task_inspect_t, ::thread_act_array_t*, ::mach_msg_type_number_t*) noexcept
            __asm__("_task_threads");
        extern "C" ::kern_return_t thread_get_state_noexcept(::thread_read_t, ::thread_state_flavor_t, ::thread_state_t,
            ::mach_msg_type_number_t*) noexcept __asm__("_thread_get_state");
        extern "C" ::kern_return_t thread_set_state_noexcept(::thread_act_t, ::thread_state_flavor_t, ::thread_state_t,
            ::mach_msg_type_number_t) noexcept __asm__("_thread_set_state");
        extern "C" ::kern_return_t thread_info_noexcept(::thread_inspect_t, ::thread_flavor_t, ::thread_info_t,
            ::mach_msg_type_number_t*) noexcept __asm__("_thread_info");
        extern "C" ::kern_return_t thread_set_exception_ports_noexcept(::thread_act_t, ::exception_mask_t, ::mach_port_t,
            ::exception_behavior_t, ::thread_state_flavor_t) noexcept __asm__("_thread_set_exception_ports");
        extern "C" ::kern_return_t thread_swap_exception_ports_noexcept(::thread_act_t, ::exception_mask_t, ::mach_port_t,
            ::exception_behavior_t, ::thread_state_flavor_t, ::exception_mask_array_t, ::mach_msg_type_number_t*,
            ::exception_handler_array_t, ::exception_behavior_array_t, ::exception_flavor_array_t) noexcept
            __asm__("_thread_swap_exception_ports");
        extern "C" ::kern_return_t mach_port_allocate_noexcept(::ipc_space_t, ::mach_port_right_t, ::mach_port_name_t*) noexcept
            __asm__("_mach_port_allocate");
        extern "C" ::kern_return_t mach_port_insert_right_noexcept(::ipc_space_t, ::mach_port_name_t, ::mach_port_t,
            ::mach_msg_type_name_t) noexcept __asm__("_mach_port_insert_right");
        extern "C" ::kern_return_t mach_port_deallocate_noexcept(::ipc_space_t, ::mach_port_name_t) noexcept __asm__("_mach_port_deallocate");
        extern "C" ::kern_return_t mach_port_destruct_noexcept(::ipc_space_t, ::mach_port_name_t, ::mach_port_delta_t,
            ::mach_port_context_t) noexcept __asm__("_mach_port_destruct");
        extern "C" ::kern_return_t vm_deallocate_noexcept(::vm_map_t, ::vm_address_t, ::vm_size_t) noexcept __asm__("_vm_deallocate");
        extern "C" ::kern_return_t semaphore_create_noexcept(::task_t, ::semaphore_t*, int, int) noexcept __asm__("_semaphore_create");
        extern "C" ::kern_return_t semaphore_destroy_noexcept(::task_t, ::semaphore_t) noexcept __asm__("_semaphore_destroy");
        extern "C" ::kern_return_t semaphore_wait_noexcept(::semaphore_t) noexcept __asm__("_semaphore_wait");
        extern "C" ::kern_return_t semaphore_signal_noexcept(::semaphore_t) noexcept __asm__("_semaphore_signal");
        extern "C" ::mach_msg_return_t mach_msg_server_noexcept(
            ::boolean_t (*)(::mach_msg_header_t*, ::mach_msg_header_t*), ::mach_msg_size_t, ::mach_port_t,
            ::mach_msg_options_t) noexcept __asm__("_mach_msg_server");
    }

    namespace details
    {
        // Built from the system SDK's mach_exc.defs with
        // MACH_EXC_SERVER_TASKIDTOKEN_STATE=1. The protected-state callback
        // carries no task/thread control port; its reply changes only the
        // architecture step bit (ARM debug state or x86 RFLAGS.TF). No
        // thread_set_state is called from the exception callback.
        extern "C" ::boolean_t mach_exc_server(::mach_msg_header_t*, ::mach_msg_header_t*) noexcept;
        inline ::std::atomic<session*> active{};
        inline ::std::atomic_bool installed{};
        inline ::std::atomic_flag host_transition = ATOMIC_FLAG_INIT;
        inline ::mach_port_t exception_port{MACH_PORT_NULL};
        inline ::pthread_t server_thread{};
# if defined(__aarch64__)
        inline constexpr ::std::uint64_t single_step_bit{1u};
        inline constexpr ::std::uint64_t breakpoint_control{1u | (2u << 1u) | (0xFu << 5u)};
        inline constexpr auto exception_flavor{ARM_DEBUG_STATE64};
# else
        inline constexpr ::std::uint64_t single_step_bit{0x100u};
        inline constexpr auto exception_flavor{x86_THREAD_STATE64};
        [[nodiscard]] inline bool native_x86_process() noexcept
        {
            int translated{};
            ::std::size_t size{sizeof(translated)};
            // [owned scalar translated] end; syscall borrows exactly sizeof(int).
            // [safe                   ] returned size is checked before use; no cursor advances.
            if(abi::sysctlbyname_noexcept("sysctl.proc_translated", ::std::addressof(translated),
                ::std::addressof(size), nullptr, 0u) == 0)
            { return size == sizeof(translated) && translated == 0; }
            // [SDK-owned per-thread errno scalar] end
            // [safe                            ] only the actual libc errno address is borrowed.
            auto const* error{abi::error_noexcept()};
            return error != nullptr && *error == ENOENT;
        }
# endif

# if defined(__aarch64__)
        inline void capture_registers(native_registers::snapshot& out, ::arm_thread_state64_t const& context) noexcept
        {
            out.machine = native_registers::architecture::aarch64;
            // [complete ARM_THREAD_STATE64 checked by the Mach flavor/count] end
            // [safe                                                     ] exactly 29 SDK-owned __x elements;
            //  ^^ no captured register value is converted to a native-memory pointer.
            for(::std::size_t index{}; index < 29u; ++index) { out.values[index] = context.__x[index]; }
            // SDK accessors handle opaque/pointer-authenticated ARM64e state.
            // These display values never bypass the existing native-step platform gate.
            out.values[29u] = __darwin_arm_thread_state64_get_fp(context);
            out.values[30u] = __darwin_arm_thread_state64_get_lr(context);
            out.values[31u] = __darwin_arm_thread_state64_get_sp(context);
            out.values[32u] = __darwin_arm_thread_state64_get_pc(context);
            out.values[33u] = context.__cpsr;
        }
# else
        inline void capture_registers(native_registers::snapshot& out, ::x86_thread_state64_t const& context) noexcept
        {
            out.machine = native_registers::architecture::x86_64;
            // [complete x86_THREAD_STATE64 checked by the Mach flavor/count] end
            // [safe                                                     ] read named SDK scalar fields only;
            //  ^^ captured SP/FP/PC values stay integers; only the owned snapshot is written.
            out.values = {context.__rax, context.__rbx, context.__rcx, context.__rdx,
                context.__rsi, context.__rdi, context.__rbp, context.__rsp,
                context.__r8, context.__r9, context.__r10, context.__r11,
                context.__r12, context.__r13, context.__r14, context.__r15, context.__rip,
                context.__rflags & ~single_step_bit};
        }
# endif

        struct host_transition_guard
        {
            host_transition_guard() noexcept
            { while(host_transition.test_and_set(::std::memory_order_acquire)) {} }
            host_transition_guard(host_transition_guard const&) = delete;
            host_transition_guard& operator=(host_transition_guard const&) = delete;
            ~host_transition_guard() { host_transition.clear(::std::memory_order_release); }
        };
        [[noreturn]] inline void fail_closed() noexcept { ::uwvm2::uwvm::debugger::posix_abi::_exit_noexcept(134); }

        [[nodiscard]] inline bool enhanced_mach_restrictions_absent() noexcept
        {
            // Apple's Enhanced Security entitlement makes thread_set_state
            // crash the process with EXC_GUARD. The only such call in this
            // backend is the selected thread's cold initial breakpoint arm.
            // Reject admission when the entitlement is present or unreadable.
            auto* const task{abi::SecTaskCreateFromSelf_noexcept(kCFAllocatorDefault)};
            if(task == nullptr) { return false; }
            ::CFErrorRef error{};
            auto const value{abi::SecTaskCopyValueForEntitlement_noexcept(task,
                CFSTR("com.apple.security.hardened-process.platform-restrictions"),
                ::std::addressof(error))};
            abi::CFRelease_noexcept(task);
            if(error != nullptr)
            {
                abi::CFRelease_noexcept(error);
                if(value != nullptr) { abi::CFRelease_noexcept(value); }
                return false;
            }
            if(value == nullptr) { return true; }
            bool allowed{};
            if(abi::CFGetTypeID_noexcept(value) == abi::CFNumberGetTypeID_noexcept())
            {
                int level{};
                allowed = abi::CFNumberGetValue_noexcept(static_cast<::CFNumberRef>(value),
                    kCFNumberIntType, ::std::addressof(level)) && level < 1;
            }
            else if(abi::CFGetTypeID_noexcept(value) == abi::CFBooleanGetTypeID_noexcept())
            { allowed = value == kCFBooleanFalse; }
            abi::CFRelease_noexcept(value);
            return allowed;
        }

        [[nodiscard]] inline bool same_task_thread(::thread_act_t target) noexcept
        {
            ::thread_act_array_t threads{};
            ::mach_msg_type_number_t count{};
            if(abi::task_threads_noexcept(::mach_task_self(), ::std::addressof(threads), ::std::addressof(count)) != KERN_SUCCESS)
            { return false; }
            bool found{};
            for(::mach_msg_type_number_t i{}; i < count; ++i)
            {
                found |= threads[i] == target;
                static_cast<void>(abi::mach_port_deallocate_noexcept(::mach_task_self(), threads[i]));
            }
            if(threads != nullptr)
            {
                static_cast<void>(abi::vm_deallocate_noexcept(::mach_task_self(),
                    reinterpret_cast<::vm_address_t>(threads),
                    static_cast<::vm_size_t>(count) * sizeof(::thread_act_t)));
            }
            return found;
        }
# if defined(__aarch64__)
        [[nodiscard]] inline bool debug_state_available(::arm_debug_state64_t const& debug) noexcept
        {
            if((debug.__mdscr_el1 & single_step_bit) != 0u) { return false; }
            for(::std::size_t i{}; i < 16u; ++i)
            {
                if((debug.__bcr[i] & 1u) != 0u || (debug.__wcr[i] & 1u) != 0u) { return false; }
            }
            return true;
        }
        [[nodiscard]] inline bool read_debug_state(::thread_act_t target, ::arm_debug_state64_t& out) noexcept
        {
            ::mach_msg_type_number_t count{ARM_DEBUG_STATE64_COUNT};
            return abi::thread_get_state_noexcept(target, ARM_DEBUG_STATE64,
                reinterpret_cast<::thread_state_t>(::std::addressof(out)), ::std::addressof(count)) == KERN_SUCCESS &&
                count == ARM_DEBUG_STATE64_COUNT;
        }
        [[nodiscard]] inline bool write_debug_state(::thread_act_t target, ::arm_debug_state64_t const& state) noexcept
        {
            return abi::thread_set_state_noexcept(target, ARM_DEBUG_STATE64,
                reinterpret_cast<::thread_state_t>(const_cast<::arm_debug_state64_t*>(::std::addressof(state))),
                ARM_DEBUG_STATE64_COUNT) == KERN_SUCCESS;
        }
# else
        [[nodiscard]] inline bool debug_state_available(::x86_debug_state64_t const& debug) noexcept
        {
            // No local/global breakpoint, watchpoint, GD or reserved control
            // bits may belong to another debugger. DR7 bit 10 is fixed by XNU.
            return (debug.__dr7 & ~::std::uint64_t{0x400u}) == 0u;
        }
        [[nodiscard]] inline bool read_debug_state(::thread_act_t target, ::x86_debug_state64_t& out) noexcept
        {
            ::mach_msg_type_number_t count{x86_DEBUG_STATE64_COUNT};
            // [owned complete SDK x86 debug state] end
            // [safe                             ] exact word count is validated after the borrow.
            return abi::thread_get_state_noexcept(target, x86_DEBUG_STATE64,
                reinterpret_cast<::thread_state_t>(::std::addressof(out)), ::std::addressof(count)) == KERN_SUCCESS &&
                count == x86_DEBUG_STATE64_COUNT;
        }
        [[nodiscard]] inline bool trace_flag_available(::thread_act_t target) noexcept
        {
            ::x86_thread_state64_t gpr{};
            ::mach_msg_type_number_t count{x86_THREAD_STATE64_COUNT};
            // [owned complete SDK x86 general state] end
            // [safe                               ] the exact count proves the complete flags carrier.
            return abi::thread_get_state_noexcept(target, x86_THREAD_STATE64,
                reinterpret_cast<::thread_state_t>(::std::addressof(gpr)), ::std::addressof(count)) == KERN_SUCCESS &&
                count == x86_THREAD_STATE64_COUNT && (gpr.__rflags & single_step_bit) == 0u;
        }
# endif
        inline void wait_gate(::semaphore_t gate) noexcept
        {
            for(;;)
            {
                auto const result{abi::semaphore_wait_noexcept(gate)};
                if(result == KERN_SUCCESS) { return; }
                if(result != KERN_ABORTED) { fail_closed(); }
            }
        }
        inline void signal_gate(::semaphore_t gate) noexcept
        {
            if(abi::semaphore_signal_noexcept(gate) != KERN_SUCCESS) { fail_closed(); }
        }
        inline void restore_port_or_fail(session& step) noexcept
        {
            auto const port{step.previous_port_valid ? step.previous_port : MACH_PORT_NULL};
            auto const behavior{step.previous_port_valid ? step.previous_behavior :
                EXCEPTION_STATE_IDENTITY_PROTECTED | MACH_EXCEPTION_CODES};
            auto const flavor{step.previous_port_valid ? step.previous_flavor : exception_flavor};
            if(abi::thread_set_exception_ports_noexcept(step.target_port, EXC_MASK_BREAKPOINT,
                port, behavior, flavor) != KERN_SUCCESS) { fail_closed(); }
        }
        inline void publish_released(session& step) noexcept
        {
            restore_port_or_fail(step);
            step.state.store(phase::released, ::std::memory_order_release);
        }

        inline ::kern_return_t handle_exception(
            ::std::uint64_t thread_id, ::mach_port_t task_token,
            ::exception_type_t exception, ::mach_exception_data_t code,
            ::mach_msg_type_number_t code_count,
            int* flavor, ::thread_state_t old_state, ::mach_msg_type_number_t old_count,
            ::thread_state_t new_state, ::mach_msg_type_number_t* new_count) noexcept
        {
            if(task_token != MACH_PORT_NULL)
            { static_cast<void>(abi::mach_port_deallocate_noexcept(::mach_task_self(), task_token)); }
            auto* const step{active.load(::std::memory_order_acquire)};
# if defined(__aarch64__)
            if(step == nullptr || thread_id != step->target_thread_id ||
               exception != EXC_BREAKPOINT || code == nullptr || code_count != 2u ||
               code[0] != EXC_ARM_BREAKPOINT ||
               flavor == nullptr || *flavor != ARM_DEBUG_STATE64 ||
               old_state == nullptr || new_state == nullptr ||
               old_count != ARM_DEBUG_STATE64_COUNT || new_count == nullptr ||
               *new_count < ARM_DEBUG_STATE64_COUNT)
            { return KERN_FAILURE; }
            // The generated MIG stub decodes the flavor/count ABI; it is not
            // a sender credential. Our private receive port, actual selected
            // thread/session and phase select this bounded state buffer.
            // This exact-sized copy cannot read outside old_state.
            ::arm_debug_state64_t old_debug{};
            ::fast_io::freestanding::my_memcpy(::std::addressof(old_debug), old_state, sizeof(old_debug));
            ::arm_debug_state64_t reply{step->original_debug};
            // The protected exception does not transfer a thread control
            // port. Its kernel thread_id and our private per-thread receive
            // port select the parked guest; this read-only query obtains PC.
            ::arm_thread_state64_t gpr{};
            ::mach_msg_type_number_t gpr_count{ARM_THREAD_STATE64_COUNT};
            if(abi::thread_get_state_noexcept(step->target_port, ARM_THREAD_STATE64,
                reinterpret_cast<::thread_state_t>(::std::addressof(gpr)),
                ::std::addressof(gpr_count)) != KERN_SUCCESS ||
               gpr_count != ARM_THREAD_STATE64_COUNT) { fail_closed(); }
            auto const pc{static_cast<::std::uintptr_t>(__darwin_arm_thread_state64_get_pc(gpr))};
            auto const current{step->state.load(::std::memory_order_acquire)};
            if(current == phase::arming)
            {
                // The selected thread has hit our sole hardware breakpoint;
                // no guest instruction has executed at expected_pc yet.
                if(pc != step->expected_pc || pc < step->owner_begin ||
                   pc >= step->owner_end || static_cast<::std::uintptr_t>(code[1]) != pc ||
                   old_debug.__bvr[0] != pc ||
                   (old_debug.__bcr[0] & 1u) == 0u) { fail_closed(); }
                step->first_pc = pc;
                step->first_exception_code = static_cast<int>(code[0]);
                if(step->abort_requested.load(::std::memory_order_acquire))
                { publish_released(*step); }
                else
                {
                    step->state.store(phase::at_guest_pc, ::std::memory_order_release);
                    wait_gate(step->first_gate);
                    if(step->state.load(::std::memory_order_acquire) == phase::executing &&
                       !step->abort_requested.load(::std::memory_order_acquire))
                    { reply.__mdscr_el1 |= single_step_bit; }
                    else { publish_released(*step); }
                }
            }
            else if(current == phase::executing)
            {
                capture_registers(step->register_snapshot, gpr);
                // XNU clears MDSCR.SS before sending a software-step
                // exception. code[1] is zero for this event; the read-only
                // GPR query above supplies its exact after-instruction PC.
                if(code[1] != 0 || (old_debug.__mdscr_el1 & single_step_bit) != 0u)
                { fail_closed(); }
                step->next_pc = pc;
                step->second_exception_code = static_cast<int>(code[0]);
                if(step->abort_requested.load(::std::memory_order_acquire))
                { publish_released(*step); }
                else
                {
                    step->state.store(phase::trapped, ::std::memory_order_release);
                    wait_gate(step->second_gate);
                    if(step->state.load(::std::memory_order_acquire) == phase::executing &&
                       !step->abort_requested.load(::std::memory_order_acquire))
                    { reply.__mdscr_el1 |= single_step_bit; }
                    else { publish_released(*step); }
                }
            }
            else
            {
                // Another EXC_BREAKPOINT on this thread is not our step.
                // Returning failure preserves the task's existing handler.
                return KERN_FAILURE;
            }
            // The protected Mach reply itself installs the debug state. No
            // thread_set_state call occurs in any exception callback. The
            // generated MIG reply buffer has at least ARM_DEBUG_STATE64_COUNT
            // words by the capacity check above.
            ::fast_io::freestanding::my_memcpy(new_state, ::std::addressof(reply), sizeof(reply));
            *new_count = ARM_DEBUG_STATE64_COUNT;
            return KERN_SUCCESS;
# else
            if(step == nullptr || thread_id != step->target_thread_id ||
               exception != EXC_BREAKPOINT || code == nullptr || code_count != 2u ||
               code[0] != EXC_I386_SGL || code[1] != 0 || flavor == nullptr ||
               *flavor != x86_THREAD_STATE64 || old_state == nullptr || new_state == nullptr ||
               old_count != x86_THREAD_STATE64_COUNT || new_count == nullptr ||
               *new_count < x86_THREAD_STATE64_COUNT) { return KERN_FAILURE; }
            // [decoded exact-flavor buffers on our selected private receive port] end
            // [safe                                                        ] exact old/capacity new counts checked.
            //  ^^ copy borrows only these two bounded buffers; PC is never a memory pointer.
            ::x86_thread_state64_t reply{};
            ::fast_io::freestanding::my_memcpy(::std::addressof(reply), old_state, sizeof(reply));
            if((reply.__rflags & single_step_bit) == 0u) { return KERN_FAILURE; }
            auto const pc{static_cast<::std::uintptr_t>(reply.__rip)};
            // Only our bridge/previous protected reply can arm TF. Int3 uses
            // EXC_I386_BPT and never enters this transaction. Preserve every
            // kernel-supplied register including RIP; alter only our TF bit.
            reply.__rflags &= ~single_step_bit;
            auto const current{step->state.load(::std::memory_order_acquire)};
            if(current == phase::arming)
            {
                if(pc != step->expected_pc || pc < step->owner_begin || pc >= step->owner_end)
                { fail_closed(); }
                step->first_pc = pc;
                step->first_exception_code = static_cast<int>(code[0]);
                if(step->abort_requested.load(::std::memory_order_acquire)) { publish_released(*step); }
                else
                {
                    step->state.store(phase::at_guest_pc, ::std::memory_order_release);
                    wait_gate(step->first_gate);
                    if(step->state.load(::std::memory_order_acquire) == phase::executing &&
                       !step->abort_requested.load(::std::memory_order_acquire))
                    { reply.__rflags |= single_step_bit; }
                    else { publish_released(*step); }
                }
            }
            else if(current == phase::executing)
            {
                step->next_pc = pc;
                capture_registers(step->register_snapshot, reply);
                step->second_exception_code = static_cast<int>(code[0]);
                if(step->abort_requested.load(::std::memory_order_acquire)) { publish_released(*step); }
                else
                {
                    step->state.store(phase::trapped, ::std::memory_order_release);
                    wait_gate(step->second_gate);
                    if(step->state.load(::std::memory_order_acquire) == phase::executing &&
                       !step->abort_requested.load(::std::memory_order_acquire))
                    { reply.__rflags |= single_step_bit; }
                    else { publish_released(*step); }
                }
            }
            else { return KERN_FAILURE; }
            // [MIG-owned reply capacity >= complete SDK x86 state] end
            // [safe                                             ] sizeof(reply) matches the checked flavor count.
            //  ^^ new_state is never advanced, no thread_set_state runs in this callback.
            ::fast_io::freestanding::my_memcpy(new_state, ::std::addressof(reply), sizeof(reply));
            *new_count = x86_THREAD_STATE64_COUNT;
            return KERN_SUCCESS;
# endif
        }
        inline void* server_main(void*) noexcept
        {
            auto const result{abi::mach_msg_server_noexcept(mach_exc_server, 4096u, exception_port, 0u)};
            if(active.load(::std::memory_order_acquire) != nullptr) { fail_closed(); }
            return reinterpret_cast<void*>(static_cast<::std::uintptr_t>(result));
        }
        inline void destroy_gates(session& step) noexcept
        {
            if(step.first_gate != MACH_PORT_NULL)
            {
                if(abi::semaphore_destroy_noexcept(::mach_task_self(), step.first_gate) != KERN_SUCCESS) { fail_closed(); }
                step.first_gate = MACH_PORT_NULL;
            }
            if(step.second_gate != MACH_PORT_NULL)
            {
                if(abi::semaphore_destroy_noexcept(::mach_task_self(), step.second_gate) != KERN_SUCCESS) { fail_closed(); }
                step.second_gate = MACH_PORT_NULL;
            }
            if(step.previous_port_valid)
            {
                static_cast<void>(abi::mach_port_deallocate_noexcept(::mach_task_self(), step.previous_port));
                step.previous_port_valid = false;
                step.previous_port = MACH_PORT_NULL;
            }
        }
    }

    [[nodiscard]] inline constexpr bool platform_available() noexcept { return true; }
    [[nodiscard]] inline bool install() noexcept
    {
        details::host_transition_guard guard{};
        if(details::installed.load(::std::memory_order_acquire)) { return true; }
        if(!details::enhanced_mach_restrictions_absent()) { return false; }
# if defined(__x86_64__)
        if(!details::native_x86_process()) { return false; }
# endif
        ::mach_port_t port{MACH_PORT_NULL};
        if(abi::mach_port_allocate_noexcept(::mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
            ::std::addressof(port)) != KERN_SUCCESS) { return false; }
        if(abi::mach_port_insert_right_noexcept(::mach_task_self(), port, port,
            MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS)
        {
            static_cast<void>(abi::mach_port_destruct_noexcept(::mach_task_self(), port, 0, 0));
            return false;
        }
        details::exception_port = port;
        if(::uwvm2::uwvm::debugger::posix_abi::pthread_create_noexcept(::std::addressof(details::server_thread), nullptr,
            details::server_main, nullptr) != 0)
        {
            details::exception_port = MACH_PORT_NULL;
            static_cast<void>(abi::mach_port_destruct_noexcept(::mach_task_self(), port, -1, 0));
            return false;
        }
        details::installed.store(true, ::std::memory_order_release);
        return true;
    }
    [[nodiscard]] inline bool uninstall() noexcept
    {
        details::host_transition_guard guard{};
        if(details::active.load(::std::memory_order_acquire) != nullptr) { return false; }
        if(!details::installed.load(::std::memory_order_acquire)) { return true; }
        auto const port{details::exception_port};
        details::installed.store(false, ::std::memory_order_release);
        if(abi::mach_port_destruct_noexcept(::mach_task_self(), port, -1, 0) != KERN_SUCCESS)
        { details::installed.store(true, ::std::memory_order_release); return false; }
        if(::uwvm2::uwvm::debugger::posix_abi::pthread_join_noexcept(details::server_thread, nullptr) != 0) { return false; }
        details::exception_port = MACH_PORT_NULL;
        return true;
    }
    [[nodiscard]] inline bool request(session& step, ::std::uint_least64_t native_thread,
                                      ::std::uintptr_t owner_begin, ::std::uintptr_t owner_end,
                                      ::std::uintptr_t return_pc) noexcept
    {
        details::host_transition_guard guard{};
        if(!details::installed.load(::std::memory_order_acquire) ||
           details::active.load(::std::memory_order_acquire) != nullptr ||
           step.state.load(::std::memory_order_acquire) != phase::idle ||
           native_thread == 0u || native_thread > (::std::numeric_limits<::mach_port_t>::max)() ||
           owner_begin == 0u || owner_begin >= owner_end ||
           return_pc < owner_begin || return_pc >= owner_end) { return false; }
        auto const target{static_cast<::thread_act_t>(native_thread)};
        if(target == ::uwvm2::uwvm::debugger::posix_abi::pthread_mach_thread_np_noexcept(
                         ::uwvm2::uwvm::debugger::posix_abi::pthread_self_noexcept()) ||
           !details::same_task_thread(target)) { return false; }
        ::thread_identifier_info_data_t identity{};
        ::mach_msg_type_number_t identity_count{THREAD_IDENTIFIER_INFO_COUNT};
        if(abi::thread_info_noexcept(target, THREAD_IDENTIFIER_INFO,
            reinterpret_cast<::thread_info_t>(::std::addressof(identity)),
            ::std::addressof(identity_count)) != KERN_SUCCESS ||
           identity_count != THREAD_IDENTIFIER_INFO_COUNT || identity.thread_id == 0u)
        { return false; }
# if defined(__aarch64__)
        ::arm_debug_state64_t original{};
# else
        ::x86_debug_state64_t original{};
        if(!details::trace_flag_available(target)) { return false; }
# endif
        if(!details::read_debug_state(target, original) ||
           !details::debug_state_available(original)) { return false; }
        if(abi::semaphore_create_noexcept(::mach_task_self(), ::std::addressof(step.first_gate),
            SYNC_POLICY_FIFO, 0) != KERN_SUCCESS) { return false; }
        if(abi::semaphore_create_noexcept(::mach_task_self(), ::std::addressof(step.second_gate),
            SYNC_POLICY_FIFO, 0) != KERN_SUCCESS)
        { details::destroy_gates(step); return false; }
        ::exception_mask_t masks[EXC_TYPES_COUNT]{};
        ::mach_msg_type_number_t count{EXC_TYPES_COUNT};
        ::exception_handler_t prior_ports[EXC_TYPES_COUNT]{};
        ::exception_behavior_t behaviors[EXC_TYPES_COUNT]{};
        ::thread_state_flavor_t flavors[EXC_TYPES_COUNT]{};
        if(abi::thread_swap_exception_ports_noexcept(target, EXC_MASK_BREAKPOINT,
            details::exception_port,
            EXCEPTION_STATE_IDENTITY_PROTECTED | MACH_EXCEPTION_CODES, details::exception_flavor,
            masks, ::std::addressof(count), prior_ports, behaviors, flavors) != KERN_SUCCESS)
        { details::destroy_gates(step); return false; }
        step.target_thread = native_thread;
        step.target_port = target;
        step.target_thread_id = identity.thread_id;
        step.owner_begin = owner_begin;
        step.owner_end = owner_end;
        step.expected_pc = return_pc;
        step.first_pc = step.next_pc = 0u;
        step.register_snapshot = {};
        step.first_exception_code = step.second_exception_code = 0;
        step.original_debug = original;
        step.previous_port_valid = count != 0u && prior_ports[0] != MACH_PORT_NULL;
        if(step.previous_port_valid)
        {
            step.previous_port = prior_ports[0];
            step.previous_behavior = behaviors[0];
            step.previous_flavor = flavors[0];
            // An existing thread-specific debugger owns breakpoint delivery.
            // Restore it and reject this transaction rather than stealing it.
            details::restore_port_or_fail(step);
            details::destroy_gates(step);
            return false;
        }
        for(::mach_msg_type_number_t i{}; i < count; ++i)
        {
            if(prior_ports[i] != MACH_PORT_NULL)
            { static_cast<void>(abi::mach_port_deallocate_noexcept(::mach_task_self(), prior_ports[i])); }
        }
        step.abort_requested.store(false, ::std::memory_order_relaxed);
        step.state.store(phase::ready, ::std::memory_order_release);
        details::active.store(::std::addressof(step), ::std::memory_order_release);
        return true;
    }
    [[nodiscard]] inline bool arm_at_return(::std::uintptr_t return_pc) noexcept
    {
        if(details::active.load(::std::memory_order_acquire) == nullptr) { return false; }
        details::host_transition_guard guard{};
        auto* const step{details::active.load(::std::memory_order_acquire)};
        if(step == nullptr || step->target_port != ::uwvm2::uwvm::debugger::posix_abi::pthread_mach_thread_np_noexcept(
                                                          ::uwvm2::uwvm::debugger::posix_abi::pthread_self_noexcept()) ||
           return_pc != step->expected_pc || return_pc < step->owner_begin ||
           return_pc >= step->owner_end) { return false; }
        auto expected{phase::ready};
        if(!step->state.compare_exchange_strong(expected, phase::arming,
            ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return false; }
# if defined(__aarch64__)
        ::arm_debug_state64_t debug{step->original_debug};
        debug.__bvr[0] = return_pc;
        debug.__bcr[0] = details::breakpoint_control;
        if(!details::write_debug_state(step->target_port, debug)) { details::fail_closed(); }
# else
        // The exact native thread is still in the assembly bridge. It alone
        // sets TF with POPFQ immediately before RET; no Mach state is written.
# endif
        return true;
    }
    [[nodiscard]] inline bool continue_one(session& step) noexcept
    {
        details::host_transition_guard guard{};
        if(details::active.load(::std::memory_order_acquire) != ::std::addressof(step))
        { return false; }
        auto expected{phase::at_guest_pc};
        if(!step.state.compare_exchange_strong(expected, phase::executing,
            ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return false; }
        details::signal_gate(step.first_gate);
        return true;
    }
    [[nodiscard]] inline bool continue_from_trap(session& step) noexcept
    {
        details::host_transition_guard guard{};
        if(details::active.load(::std::memory_order_acquire) != ::std::addressof(step) ||
           step.next_pc < step.owner_begin || step.next_pc >= step.owner_end) { return false; }
        auto expected{phase::trapped};
        if(!step.state.compare_exchange_strong(expected, phase::executing,
            ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return false; }
        step.first_pc = step.next_pc;
        step.expected_pc = step.next_pc;
        details::signal_gate(step.second_gate);
        return true;
    }
    [[nodiscard]] inline bool release(session& step) noexcept
    {
        details::host_transition_guard guard{};
        if(details::active.load(::std::memory_order_acquire) != ::std::addressof(step))
        { return false; }
        auto const current{step.state.load(::std::memory_order_acquire)};
        if(current == phase::arming || current == phase::executing)
        {
            step.abort_requested.store(true, ::std::memory_order_release);
            return true;
        }
        if(current == phase::at_guest_pc)
        {
            step.state.store(phase::releasing, ::std::memory_order_release);
            details::signal_gate(step.first_gate);
            return true;
        }
        if(current == phase::trapped || current == phase::failed)
        {
            step.state.store(phase::releasing, ::std::memory_order_release);
            details::signal_gate(step.second_gate);
            return true;
        }
        return current == phase::releasing || current == phase::released;
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
        if(step->state.load(::std::memory_order_acquire) != phase::trapped || step->target_thread == 0u ||
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
        if(step->state.load(::std::memory_order_acquire) != phase::trapped || step->target_thread == 0u ||
           step->owner_begin == 0u || step->owner_end <= step->owner_begin ||
           step->next_pc < step->owner_begin || step->next_pc >= step->owner_end ||
           step->register_snapshot.size() == 0u || step->register_snapshot.pc() != step->next_pc)
        { return false; }
        // Borrow a local value copy, never the kernel context or mutable saved trap state.
        auto const copied{step->register_snapshot};
        inspect(step->target_thread, step->next_pc, step->owner_begin, step->owner_end, copied);
        return true;
    }
    [[nodiscard]] inline bool clear(session& step) noexcept
    {
        details::host_transition_guard guard{};
        auto const current{step.state.load(::std::memory_order_acquire)};
        if(current != phase::ready && current != phase::released) { return false; }
        auto* expected{::std::addressof(step)};
        if(!details::active.compare_exchange_strong(expected, nullptr,
            ::std::memory_order_acq_rel, ::std::memory_order_acquire)) { return false; }
        if(current == phase::ready) { details::restore_port_or_fail(step); }
        details::destroy_gates(step);
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

#if defined(__APPLE__) && (defined(__aarch64__) || (defined(__x86_64__) && defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) && UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT == 1)) && TARGET_OS_OSX
// The MIG server decodes messages received on our private exception port.
// Wasm has no Mach send/control port or native IPC capability. The selected
// thread/session/phase still supply the owner boundary; MIG/counts and the
// task-token alone are NOT sender credentials. A trusted native host in this
// same process is outside the Wasm isolation boundary.
extern "C" [[gnu::used]] inline ::kern_return_t catch_mach_exception_raise(
    ::mach_port_t, ::mach_port_t, ::mach_port_t, ::exception_type_t,
    ::mach_exception_data_t, ::mach_msg_type_number_t) noexcept { return KERN_FAILURE; }
extern "C" [[gnu::used]] inline ::kern_return_t catch_mach_exception_raise_state(
    ::mach_port_t, ::exception_type_t, ::mach_exception_data_t,
    ::mach_msg_type_number_t, int*, const ::thread_state_t,
    ::mach_msg_type_number_t, ::thread_state_t, ::mach_msg_type_number_t*) noexcept
{ return KERN_FAILURE; }
extern "C" [[gnu::used]] inline ::kern_return_t catch_mach_exception_raise_state_identity(
    ::mach_port_t, ::mach_port_t, ::mach_port_t,
    ::exception_type_t, ::mach_exception_data_t,
    ::mach_msg_type_number_t, int*,
    ::thread_state_t, ::mach_msg_type_number_t,
    ::thread_state_t, ::mach_msg_type_number_t*) noexcept
{ return KERN_FAILURE; }
extern "C" [[gnu::used]] inline ::kern_return_t catch_mach_exception_raise_state_identity_protected(
    ::mach_port_t received_port, ::std::uint64_t thread_id, ::mach_port_t task_token,
    ::exception_type_t exception, ::mach_exception_data_t code,
    ::mach_msg_type_number_t code_count, int* flavor,
    ::thread_state_t old_state, ::mach_msg_type_number_t old_count,
    ::thread_state_t new_state, ::mach_msg_type_number_t* new_count) noexcept
{
    namespace mach_step = ::uwvm2::uwvm::debugger::native_step_macos;
    if(received_port == MACH_PORT_NULL || received_port != mach_step::details::exception_port)
    {
        if(task_token != MACH_PORT_NULL)
        { static_cast<void>(mach_step::abi::mach_port_deallocate_noexcept(::mach_task_self(), task_token)); }
        return KERN_FAILURE;
    }
    return mach_step::details::handle_exception(
        thread_id, task_token, exception, code, code_count, flavor,
        old_state, old_count, new_state, new_count);
}
#endif
