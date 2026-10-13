// Actual Mach x86-64 producer test; no runtime/source-query authority is minted.
// Build against one real x86-64 macOS SDK/MIG/MC closure in keeper's 64-GiB lane.
#include <uwvm2/uwvm/debugger/native_step.h>
#include <uwvm2/uwvm/debugger/native_disassembly.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>
#if !defined(__APPLE__) || !defined(__x86_64__) || !TARGET_OS_OSX || \
    !defined(UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT) || UWVM2_ENABLE_DEBUG_NATIVE_STEP_MACOS_X64_PRODUCT != 1 || \
    !defined(UWVM_USE_LLVM_JIT)
# error This actual producer probe requires the exact enabled Mach x86-64/LLVM MC build.
#endif
namespace backend = ::uwvm2::uwvm::debugger::native_step;
namespace mach_backend = ::uwvm2::uwvm::debugger::native_step_macos;
namespace disasm = ::uwvm2::uwvm::debugger::native_disassembly;
namespace
{
    ::std::atomic_bool poll_seen{}, may_return{}, worker_done{}, may_exit{};
    ::std::atomic<::std::uint_least64_t> worker_port{}, final_flags{};
    alignas(8) ::std::atomic<::std::uint64_t> counter{};
    static_assert(sizeof(counter) == sizeof(::std::uint64_t) && decltype(counter)::is_always_lock_free);
    [[noreturn]] void fail(char const* message) noexcept
    { ::fast_io::io::perrln("FAIL Mach x86-64 native-step: ", message); ::fast_io::fast_terminate(); }
    void check(bool value, char const* message) noexcept { if(!value) { fail(message); } }
    template<typename Predicate> bool wait_for(Predicate predicate) noexcept
    {
        auto const end{::std::chrono::steady_clock::now() + ::std::chrono::seconds{5}};
        while(::std::chrono::steady_clock::now() < end)
        { if(predicate()) { return true; } ::std::this_thread::yield(); }
        return false;
    }
    bool wait_phase(backend::session const& session, backend::phase phase) noexcept
    { return wait_for([&]() noexcept { return session.state.load(::std::memory_order_acquire) == phase; }); }
}
extern "C" void mach_x64_step_poll() noexcept
{
    poll_seen.store(true, ::std::memory_order_release);
    while(!may_return.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
}
extern "C" int mach_x64_step_arm(::std::uintptr_t pc) noexcept
{ return backend::arm_at_return(pc) ? 1 : 0; }
__asm__(
    ".text\n\t"
    ".globl _mach_x64_step_bridge_entry\n\t"
    ".private_extern _mach_x64_step_bridge_entry\n"
    "_mach_x64_step_bridge_entry:\n\t"
    ".cfi_startproc\n\t"
    ".cfi_def_cfa %rsp, 8\n\t"
    ".cfi_offset %rip, -8\n\t"
    // [live SysV stack] original RSP
    // [safe          ] ^^ RSP -= 8 reserves the same host-call scratch as production.
    "subq $8, %rsp\n\t"
    ".cfi_adjust_cfa_offset 8\n\t"
    "call _mach_x64_step_poll\n\t"
    // [owned scratch slot] original RSP
    // [safe             ] ^^ RSP += 8 restores the actual return slot.
    "addq $8, %rsp\n\t"
    ".cfi_adjust_cfa_offset -8\n\t"
    "movq (%rsp), %rdi\n\t"
    // [same live stack] original RSP
    // [safe           ] ^^ RSP -= 8 reserves only the eight-byte alignment slot.
    "subq $8, %rsp\n\t"
    ".cfi_adjust_cfa_offset 8\n\t"
    "call _mach_x64_step_arm\n\t"
    // [same scratch slot] original RSP
    // [safe             ] ^^ RSP += 8 restores it before flag arming.
    "addq $8, %rsp\n\t"
    ".cfi_adjust_cfa_offset -8\n\t"
    "testl %eax, %eax\n\t"
    "je 1f\n\t"
    // [live stack below original RSP] owned eight-byte flags
    // [safe                        ] ^^ RSP -= 8 for PUSHFQ.
    "pushfq\n\t"
    ".cfi_adjust_cfa_offset 8\n\t"
    "orq $0x100, (%rsp)\n\t"
    // [complete flags carrier] actual return slot
    // [safe                  ] ^^ RSP += 8 for POPFQ, followed immediately by RET.
    "popfq\n\t"
    ".cfi_adjust_cfa_offset -8\n\t"
    "1: retq\n\t"
    ".cfi_endproc\n");
extern "C" __attribute__((naked)) void mach_x64_step_bridge() noexcept
{ __asm__ volatile("jmp _mach_x64_step_bridge_entry" ::: "memory"); }
extern "C" void mach_x64_step_leaf(::std::atomic<::std::uint64_t>*) noexcept;
extern "C" ::std::byte mach_x64_step_leaf_begin[], mach_x64_step_first[], mach_x64_step_leaf_end[];
__asm__(
    ".text\n\t"
    ".globl _mach_x64_step_leaf\n\t"
    ".globl _mach_x64_step_leaf_begin\n\t"
    ".globl _mach_x64_step_first\n\t"
    ".globl _mach_x64_step_leaf_end\n"
    "_mach_x64_step_leaf_begin:\n"
    "_mach_x64_step_leaf:\n\t"
    ".cfi_startproc\n\t"
    // [caller-owned live stack] ^^ RSP -= 8 saves nonvolatile RBX in a complete slot.
    "pushq %rbx\n\t"
    ".cfi_adjust_cfa_offset 8\n\t"
    ".cfi_offset %rbx, -16\n\t"
    "movq %rdi, %rbx\n\t"
    "call _mach_x64_step_bridge\n"
    "_mach_x64_step_first:\n\t"
    "lock incq (%rbx)\n\t"
    "lock incq (%rbx)\n\t"
    // [same owned RBX slot] ^^ RSP += 8 restores the original caller stack.
    "popq %rbx\n\t"
    ".cfi_adjust_cfa_offset -8\n\t"
    ".cfi_restore %rbx\n\t"
    "retq\n\t"
    ".cfi_endproc\n"
    "_mach_x64_step_leaf_end:\n");
int main()
{
    if(!backend::install())
    {
        ::fast_io::io::println("Mach x86-64 native-step: UNAVAILABLE actual native/entitlement/MIG admission rejected");
        return 77;
    }
    auto const begin{reinterpret_cast<::std::uintptr_t>(mach_x64_step_leaf_begin)};
    auto const end{reinterpret_cast<::std::uintptr_t>(mach_x64_step_leaf_end)};
    auto const first_pc{reinterpret_cast<::std::uintptr_t>(mach_x64_step_first)};
    check(begin != 0u && begin <= first_pc && first_pc < end && end - first_pc >= 8u, "actual fixed code-owner bounds");
    ::std::array<::std::uint8_t, 8u> code{};
    // [loader-owned static assembler leaf first_pc ... end] end
    // [safe                                               ] labels prove 8 available bytes; no guest address enters.
    ::std::memcpy(code.data(), mach_x64_step_first, code.size());
    auto const instruction1{disasm::decode_copied(first_pc, code)};
    check(instruction1 && instruction1.size == 4u && ::std::strstr(instruction1.text.data(), "inc") != nullptr,
          "actual first lock-inc MC boundary");
    // [owned eight-byte copy] end
    // [safe                 ] first decoded length=4 leaves four bytes in this same array.
    auto const instruction2{disasm::decode_copied(first_pc + instruction1.size, {code.data() + instruction1.size, 4u})};
    check(instruction2 && instruction2.size == 4u && ::std::strstr(instruction2.text.data(), "inc") != nullptr,
          "actual second lock-inc MC boundary");
    ::std::thread worker{[]() noexcept
    {
        worker_port.store(::uwvm2::uwvm::debugger::posix_abi::pthread_mach_thread_np_noexcept(
            ::uwvm2::uwvm::debugger::posix_abi::pthread_self_noexcept()), ::std::memory_order_release);
        mach_x64_step_leaf(::std::addressof(counter));
        ::std::uint64_t flags{};
        // [live worker stack] PUSHFQ reserves exactly one owned eight-byte flags slot (RSP -= 8);
        // [safe             ] POPQ consumes that same complete slot (RSP += 8), restoring RSP.
        __asm__ volatile("pushfq\n\tpopq %0" : "=r"(flags) :: "memory");
        final_flags.store(flags, ::std::memory_order_release);
        worker_done.store(true, ::std::memory_order_release);
        while(!may_exit.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
    }};
    check(wait_for([]() noexcept { return poll_seen.load(::std::memory_order_acquire); }), "real worker before-return gate");
    auto const native{worker_port.load(::std::memory_order_acquire)};
    backend::session session{};
    check(!backend::request(session, 0u, begin, end, first_pc), "zero native identity refused");
    check(!backend::request(session, native, begin, end, end), "one-past owner return refused");
    check(backend::request(session, native, begin, end, first_pc), "actual current worker/owner admission");
    // These inert callback buffers cannot mint a host request. Wrong thread,
    // Int3 and insufficient state are rejected before memcpy/wait or mutation.
    ::x86_thread_state64_t old{}, reply{};
    old.__rip = first_pc; old.__rflags = 0x100u;
    ::mach_exception_data_type_t event[2]{EXC_I386_SGL, 0};
    int flavor{x86_THREAD_STATE64};
    ::mach_msg_type_number_t reply_count{x86_THREAD_STATE64_COUNT};
    auto reject=[&](::std::uint64_t tid, ::mach_msg_type_number_t count) noexcept
    {
        // [complete owned SDK states + two owned exception words] end
        // [safe                                                 ] no cursor changes; bad count must reject before borrow.
        return mach_backend::details::handle_exception(tid, MACH_PORT_NULL, EXC_BREAKPOINT, event, 2u,
            ::std::addressof(flavor), reinterpret_cast<::thread_state_t>(::std::addressof(old)), count,
            reinterpret_cast<::thread_state_t>(::std::addressof(reply)), ::std::addressof(reply_count));
    };
    check(reject(0u, x86_THREAD_STATE64_COUNT) == KERN_FAILURE, "claimed unrelated thread cannot arm");
    event[0] = EXC_I386_BPT;
    check(reject(session.target_thread_id, x86_THREAD_STATE64_COUNT) == KERN_FAILURE, "guest Int3 does not become a step");
    event[0] = EXC_I386_SGL;
    flavor = x86_DEBUG_STATE64;
    check(reject(session.target_thread_id, x86_THREAD_STATE64_COUNT) == KERN_FAILURE, "wrong reply flavor refused");
    flavor = x86_THREAD_STATE64;
    old.__rflags = 0u;
    check(reject(session.target_thread_id, x86_THREAD_STATE64_COUNT) == KERN_FAILURE, "absent TF cannot be a step");
    old.__rflags = 0x100u;
    // [complete owned reply buffers] end; invalid receive port must reject before state use.
    check(catch_mach_exception_raise_state_identity_protected(MACH_PORT_NULL, session.target_thread_id,
        MACH_PORT_NULL, EXC_BREAKPOINT, event, 2u, ::std::addressof(flavor),
        reinterpret_cast<::thread_state_t>(::std::addressof(old)), x86_THREAD_STATE64_COUNT,
        reinterpret_cast<::thread_state_t>(::std::addressof(reply)), ::std::addressof(reply_count)) == KERN_FAILURE,
        "other receive port does not become an exception owner");
    check(reject(session.target_thread_id, x86_THREAD_STATE64_COUNT - 1u) == KERN_FAILURE, "truncated MIG state refused");
    check(session.state.load(::std::memory_order_acquire) == backend::phase::ready && counter.load() == 0u,
          "negative callbacks leave real pending transaction unchanged");
    may_return.store(true, ::std::memory_order_release);
    check(wait_phase(session, backend::phase::at_guest_pc), "true first TF-after-RET trap");
    check(session.first_pc == first_pc && counter.load() == 0u, "first trap executes zero guest instructions");
    unsigned register_callbacks{};
    check(!backend::with_owned_registers(reinterpret_cast<void const*>(::std::uintptr_t{1}),
        [&](auto...) noexcept { ++register_callbacks; }), "forged register session rejected before dereference");
    check(!backend::with_owned_registers(::std::addressof(session),
        [&](auto...) noexcept { ++register_callbacks; }), "first gate is not a completed native register stop");
    check(register_callbacks == 0u, "unauthorized register callback never called");
    check(backend::continue_one(session), "execute first instruction");
    check(wait_phase(session, backend::phase::trapped), "first guest-instruction trap");
    check(session.next_pc == first_pc + instruction1.size && counter.load() == 1u, "exact first MC boundary/count");
    ::uwvm2::uwvm::debugger::native_registers::snapshot copied_registers{};
    check(backend::with_owned_registers(::std::addressof(session),
        [&](auto actual_thread, auto actual_pc, auto actual_begin, auto actual_end, auto const& registers) noexcept
        {
            check(actual_thread == native && actual_pc == first_pc + instruction1.size && actual_begin == begin && actual_end == end,
                "register response belongs to exact selected native thread and code range");
            copied_registers = registers;
            ++register_callbacks;
        }), "read selected real native trap registers");
    check(copied_registers.size() == 18u && copied_registers.pc() == first_pc + instruction1.size && copied_registers.sp() != 0u &&
          copied_registers.values[1u] == reinterpret_cast<::std::uintptr_t>(::std::addressof(counter)) &&
          (copied_registers.values[17u] & 0x100u) == 0u && register_callbacks == 1u,
          "real RBX, SP, RIP and debugger-normalized RFLAGS");

    unsigned borrowed{};
    check(!backend::with_owned_trap(reinterpret_cast<void const*>(::std::uintptr_t{1}),
        [&](auto...) noexcept { ++borrowed; }), "foreign session does not gain code/read authority");
    check(borrowed == 0u, "foreign callback never called");
    check(backend::continue_from_trap(session), "execute second instruction");
    check(wait_phase(session, backend::phase::trapped), "second guest-instruction trap");
    check(session.next_pc == first_pc + instruction1.size + instruction2.size && counter.load() == 2u,
          "exact second MC boundary/count");
    check(backend::release(session), "release actual exception stop");
    check(wait_phase(session, backend::phase::released), "protected reply clears TF and restores port");
    check(wait_for([]() noexcept { return worker_done.load(::std::memory_order_acquire); }), "guest leaf exits with TF clear");
    check((final_flags.load(::std::memory_order_acquire) & 0x100u) == 0u, "TF cannot leak past release");
    check(backend::clear(session), "retire actual identity");
    check(!backend::with_owned_registers(::std::addressof(session),
        [&](auto...) noexcept { ++register_callbacks; }) && register_callbacks == 1u && copied_registers.size() == 18u,
        "retired register session rejected and retained value copy stays available");

    check(!backend::with_owned_trap(::std::addressof(session), [&](auto...) noexcept { ++borrowed; }),
          "retired session unavailable");
    may_exit.store(true, ::std::memory_order_release); worker.join();
    check(backend::uninstall(), "retire protected server/port");
    ::fast_io::io::println("Mach x86-64 native-step: PASS actual RET target, zero/one/two instruction counts, MC boundaries, TF retirement and negative identities");
}
