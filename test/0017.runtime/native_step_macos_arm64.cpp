#include "../../src/uwvm2/uwvm/debugger/native_step_macos.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <csignal>
#include <pthread.h>
#include <thread>
#include <unistd.h>

extern "C" int mac_step_guest();
extern "C" unsigned char mac_step_guest_pc[];
__asm__(
    ".globl _mac_step_guest\n"
    "_mac_step_guest:\n"
    ".globl _mac_step_guest_pc\n"
    "_mac_step_guest_pc:\n"
    "mov w0, #1\n"
    "add w0, w0, #2\n"
    "ret\n");

extern "C" int mac_step_guest_bridge();
extern "C" unsigned char mac_step_guest_bridge_pc[];
extern "C" void mac_step_bridge_wait(::std::uintptr_t);
extern "C" void mac_step_bridge_arm(::std::uintptr_t);
__asm__(
    ".globl _mac_step_guest_bridge\n"
    "_mac_step_guest_bridge:\n"
    "stp x29, x30, [sp, #-16]!\n"
    "mov x29, sp\n"
    "bl _mac_step_native_bridge\n"
    ".globl _mac_step_guest_bridge_pc\n"
    "_mac_step_guest_bridge_pc:\n"
    "mov w0, #4\n"
    "add w0, w0, #5\n"
    "ldp x29, x30, [sp], #16\n"
    "ret\n"
    ".globl _mac_step_native_bridge\n"
    "_mac_step_native_bridge:\n"
    "stp x29, x30, [sp, #-16]!\n"
    "mov x29, sp\n"
    "ldr x0, [sp, #8]\n"
    "bl _mac_step_bridge_wait\n"
    "ldr x0, [sp, #8]\n"
    "bl _mac_step_bridge_arm\n"
    "ldp x29, x30, [sp], #16\n"
    "ret\n");

namespace ns = ::uwvm2::uwvm::debugger::native_step_macos;
static ::std::atomic<::std::uint_least64_t> target_port{};
static ::std::atomic_bool launch{};
static ::std::atomic_int guest_result{};
static volatile ::sig_atomic_t unrelated_traps{};
static ::std::atomic<::std::uint_least64_t> bridge_target_port{};
static ::std::atomic_bool bridge_launch{};

extern "C" void mac_step_bridge_wait(::std::uintptr_t return_pc)
{
    if(return_pc != reinterpret_cast<::std::uintptr_t>(mac_step_guest_bridge_pc))
    { ::_exit(86); }
    bridge_target_port.store(::pthread_mach_thread_np(::pthread_self()),
        ::std::memory_order_release);
    while(!bridge_launch.load(::std::memory_order_acquire))
    { ::std::this_thread::yield(); }
}
extern "C" void mac_step_bridge_arm(::std::uintptr_t return_pc)
{
    if(!ns::arm_at_return(return_pc)) { ::_exit(87); }
}

static void unrelated_trap(int) noexcept
{ unrelated_traps = static_cast<::sig_atomic_t>(unrelated_traps + 1); }
static bool await_phase(ns::session& step, ns::phase desired) noexcept
{
    auto const deadline{::std::chrono::steady_clock::now() + ::std::chrono::seconds(3)};
    while(::std::chrono::steady_clock::now() < deadline)
    {
        if(step.state.load(::std::memory_order_acquire) == desired) { return true; }
        ::std::this_thread::yield();
    }
    return false;
}
static void target_main()
{
    target_port.store(::pthread_mach_thread_np(::pthread_self()), ::std::memory_order_release);
    while(!launch.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
    ::raise(SIGTRAP);
    if(!ns::arm_at_return(reinterpret_cast<::std::uintptr_t>(mac_step_guest_pc)))
    { guest_result.store(-1, ::std::memory_order_release); return; }
    guest_result.store(mac_step_guest(), ::std::memory_order_release);
}

int main()
{
    ::alarm(20);
    struct ::sigaction prior{}, signal_action{};
    signal_action.sa_handler = unrelated_trap;
    sigemptyset(::std::addressof(signal_action.sa_mask));
    if(::sigaction(SIGTRAP, ::std::addressof(signal_action),
        ::std::addressof(prior)) != 0) { return 1; }
    if(!ns::platform_available() || !ns::install()) { return 2; }
    ns::session step{};
    ::std::thread target(target_main);
    while(target_port.load(::std::memory_order_acquire) == 0u) { ::std::this_thread::yield(); }
    auto const pc{reinterpret_cast<::std::uintptr_t>(mac_step_guest_pc)};
    if(ns::request(step, 0u, pc, pc + 12u, pc) ||
       ns::request(step, target_port.load(::std::memory_order_acquire),
           pc + 4u, pc + 12u, pc) ||
       ns::request(step, ::pthread_mach_thread_np(::pthread_self()),
           pc, pc + 12u, pc)) { return 11; }
    if(!ns::request(step, target_port.load(::std::memory_order_acquire),
        pc, pc + 12u, pc)) { return 3; }
    if(ns::request(step, target_port.load(::std::memory_order_acquire),
        pc, pc + 12u, pc)) { return 4; }
    launch.store(true, ::std::memory_order_release);
    if(!await_phase(step, ns::phase::at_guest_pc) || step.first_pc != pc ||
       unrelated_traps != 1 ||
       guest_result.load(::std::memory_order_acquire) != 0) { return 5; }
    ::raise(SIGTRAP);
    if(unrelated_traps != 2) { return 6; }
    unsigned register_callbacks{};
    if(ns::with_owned_registers(reinterpret_cast<void const*>(::std::uintptr_t{1}),
        [&](auto...) noexcept { ++register_callbacks; }) ||
       ns::with_owned_registers(::std::addressof(step),
        [&](auto...) noexcept { ++register_callbacks; }) || register_callbacks != 0u) { return 30; }
    if(!ns::continue_one(step) || !await_phase(step, ns::phase::trapped) ||
       step.next_pc != pc + 4u ||
       guest_result.load(::std::memory_order_acquire) != 0) { return 7; }
    ::uwvm2::uwvm::debugger::native_registers::snapshot first_registers{};
    bool valid_register_owner{};
    if(!ns::with_owned_registers(::std::addressof(step),
        [&](auto actual_thread, auto actual_pc, auto actual_begin, auto actual_end, auto const& registers) noexcept
        {
            valid_register_owner = actual_thread == target_port.load(::std::memory_order_acquire) &&
                actual_pc == pc + 4u && actual_begin == pc && actual_end == pc + 12u;
            first_registers = registers;
            ++register_callbacks;
        }) || !valid_register_owner || first_registers.size() != 34u || first_registers.pc() != pc + 4u ||
        first_registers.values[0u] != 1u || first_registers.sp() == 0u || register_callbacks != 1u)
    { return 31; }
    ::std::printf("FIRST_PC=%p AFTER_ONE_PC=%p\n",
        reinterpret_cast<void*>(step.first_pc), reinterpret_cast<void*>(step.next_pc));
    if(!ns::continue_from_trap(step) || !await_phase(step, ns::phase::trapped) ||
       step.next_pc != pc + 8u || step.first_pc != pc + 4u) { return 8; }
    ::std::printf("AFTER_TWO_PC=%p\n", reinterpret_cast<void*>(step.next_pc));
    bool valid_second_registers{};
    if(!ns::with_owned_registers(::std::addressof(step),
        [&](auto, auto actual_pc, auto, auto, auto const& registers) noexcept
        { valid_second_registers = actual_pc == pc + 8u && registers.pc() == actual_pc && registers.values[0u] == 3u; }) ||
        !valid_second_registers || first_registers.values[0u] != 1u) { return 32; }
    if(!ns::release(step) || !await_phase(step, ns::phase::released) ||
       !ns::clear(step)) { return 9; }
    target.join();
    if(ns::with_owned_registers(::std::addressof(step),
        [&](auto...) noexcept { ++register_callbacks; }) || register_callbacks != 1u || first_registers.values[0u] != 1u)
    { return 33; }
    auto const value{guest_result.load(::std::memory_order_acquire)};
    if(value != 3) { return 10; }

    // This call chain mimics JIT BL -> naked ABI bridge -> guest continuation.
    // Its return address is captured and the hardware breakpoint is armed
    // only after the bridge's cooperative wait has released the target.
    {
        ns::session bridged{};
        ::std::atomic_int result{};
        ::std::thread worker([&]
        { result.store(mac_step_guest_bridge(), ::std::memory_order_release); });
        while(bridge_target_port.load(::std::memory_order_acquire) == 0u)
        { ::std::this_thread::yield(); }
        auto const bridge_pc{reinterpret_cast<::std::uintptr_t>(mac_step_guest_bridge_pc)};
        if(!ns::request(bridged, bridge_target_port.load(::std::memory_order_acquire),
            bridge_pc, bridge_pc + 16u, bridge_pc)) { return 18; }
        bridge_launch.store(true, ::std::memory_order_release);
        if(!await_phase(bridged, ns::phase::at_guest_pc) ||
           bridged.first_pc != bridge_pc || result.load(::std::memory_order_acquire) != 0 ||
           !ns::continue_one(bridged) ||
           !await_phase(bridged, ns::phase::trapped) ||
           bridged.next_pc != bridge_pc + 4u ||
           !ns::continue_from_trap(bridged) ||
           !await_phase(bridged, ns::phase::trapped) ||
           bridged.next_pc != bridge_pc + 8u ||
           !ns::release(bridged) || !await_phase(bridged, ns::phase::released) ||
           !ns::clear(bridged)) { return 19; }
        worker.join();
        if(result.load(::std::memory_order_acquire) != 9) { return 20; }
        ::std::puts("MACOS_ARM64_BRIDGE_STEP_PASS");
    }

    // A first-stop cancellation restores the original debug state and
    // exception port before any guest instruction runs.
    {
        ns::session cancel{};
        ::std::atomic<::std::uint_least64_t> port{};
        ::std::atomic_bool go{};
        ::std::atomic_int result{};
        ::std::thread worker([&]
        {
            port.store(::pthread_mach_thread_np(::pthread_self()), ::std::memory_order_release);
            while(!go.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
            if(!ns::arm_at_return(pc)) { result.store(-1, ::std::memory_order_release); return; }
            result.store(mac_step_guest(), ::std::memory_order_release);
        });
        while(port.load(::std::memory_order_acquire) == 0u) { ::std::this_thread::yield(); }
        if(!ns::request(cancel, port.load(::std::memory_order_acquire), pc, pc + 12u, pc))
        { return 12; }
        go.store(true, ::std::memory_order_release);
        if(!await_phase(cancel, ns::phase::at_guest_pc) ||
           !ns::release(cancel) || !await_phase(cancel, ns::phase::released) ||
           !ns::clear(cancel)) { return 13; }
        worker.join();
        if(result.load(::std::memory_order_acquire) != 3) { return 14; }
    }

    // Ready cancellation cannot race the selected thread's arm transition:
    // arm_at_return either owns an active request or returns without touching
    // its debug state. Repeating this checks port/gate lifetime bookkeeping.
    for(unsigned round{}; round < 64u; ++round)
    {
        ns::session cancel{};
        ::std::atomic<::std::uint_least64_t> port{};
        ::std::atomic_bool go{}, armed{};
        ::std::thread worker([&]
        {
            port.store(::pthread_mach_thread_np(::pthread_self()), ::std::memory_order_release);
            while(!go.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
            armed.store(ns::arm_at_return(pc), ::std::memory_order_release);
        });
        while(port.load(::std::memory_order_acquire) == 0u) { ::std::this_thread::yield(); }
        if(!ns::request(cancel, port.load(::std::memory_order_acquire), pc, pc + 12u, pc) ||
           !ns::clear(cancel)) { return 15; }
        go.store(true, ::std::memory_order_release);
        worker.join();
        if(armed.load(::std::memory_order_acquire)) { return 16; }
    }
    auto const uninstalled{ns::uninstall()};
    ::std::printf("GUEST_RESULT=%d UNINSTALLED=%d\n", value, static_cast<int>(uninstalled));
    if(!uninstalled) { return 17; }
    static_cast<void>(::sigaction(SIGTRAP, ::std::addressof(prior), nullptr));
    ::std::puts("MACOS_ARM64_NATIVE_STEP_PASS");
}
