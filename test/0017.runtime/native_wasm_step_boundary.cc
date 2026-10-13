#include <uwvm2/uwvm/debugger/native_wasm_step_boundary.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fast_io.h>
namespace dbg = ::uwvm2::uwvm::debugger;
static void check(bool yes, char const* why) noexcept
{
    if(!yes) { ::fast_io::io::perrln("native_wasm_step_boundary: ", ::fast_io::mnp::os_c_str(why)); ::fast_io::fast_terminate(); }
}
struct counted_display
{
    dbg::native_disassembly::decoder actual{};
    ::std::size_t calls{};
    [[nodiscard]] explicit operator bool() const noexcept { return bool(actual); }
    dbg::native_disassembly::instruction decode(::std::uintptr_t pc,::std::span<::std::uint8_t const> bytes) noexcept
    { ++calls; return actual.decode(pc,bytes); }
};
int main()
{
    dbg::native_disassembly::decoder display{};
    dbg::native_owned_instruction_semantics::decoder semantics{};
    if(!display || !semantics) { ::fast_io::io::println("native_wasm_step_boundary: SKIP MC provider unavailable"); return 77; }
#if defined(__x86_64__) || defined(_M_X64)
    constexpr ::std::uintptr_t begin{0x1000u};
    constexpr ::std::array<::std::uint8_t, 10u> owner{0x31u,0xc0u,0x75u,0x02u,0x90u,0x90u,0x90u,0x90u,0x90u,0xc3u};
    auto prepare{[&](::std::uintptr_t pc, bool ni)
    { return dbg::native_wasm_step_boundary::prepare(display, semantics, owner, begin, begin+owner.size(), pc, ni); }};
    counted_display counted{};
    auto const same_walk{dbg::native_wasm_step_boundary::prepare(counted,semantics,owner,begin,begin+owner.size(),begin,false)};
    check(bool(same_walk) && same_walk.first_successor==begin+2u && counted.calls==2u,
          "current and ordinary successor use one actual MC entry walk");
    auto ordinary{prepare(begin,false)};
    check(bool(ordinary) && ordinary.first_successor==begin+2u && ordinary.second_successor==0u, "actual XOR bounded SI fallthrough");
    auto branch{prepare(begin+2u,false)};
    check(bool(branch) && branch.first_successor==begin+6u && branch.second_successor==begin+4u, "actual JNE BOTH owned boundary successors");
    auto const ni_branch{prepare(begin+2u,true)};
    check(bool(ni_branch) && ni_branch.first_successor == branch.first_successor &&
          ni_branch.second_successor == branch.second_successor,
          "NI uses exact SAME two owned boundary successors as SI");
    check(!prepare(begin+1u,false), "middle of variable-length XOR cannot be an instruction boundary");
    auto ret{prepare(begin+9u,false)};
    check(!ret && ret.unavailable_reason==dbg::native_next_policy::reason::caller_unwind_unavailable, "RET never reads guessed SP/caller");
    constexpr ::std::array<::std::uint8_t, 6u> host_call{0xe8u,0x00u,0x10u,0x00u,0x00u,0x90u};
    auto denied{dbg::native_wasm_step_boundary::prepare(display,semantics,host_call,begin,begin+host_call.size(),begin,false)};
    check(!denied && denied.unavailable_reason==dbg::native_next_policy::reason::call_continuation_unavailable, "host direct CALL cannot execute without actual Wasm callee proof");
    constexpr ::std::array<::std::uint8_t, 4u> bad_target{0xebu,0x01u,0x31u,0xc0u};
    for(bool next : {false, true})
    { check(!dbg::native_wasm_step_boundary::prepare(display,semantics,bad_target,begin,begin+bad_target.size(),begin,next), "direct JMP inside XOR refuses for SI and NI"); }
    constexpr ::std::array<::std::uint8_t, 3u> outside{0x75u,0x10u,0x90u};
    for(bool next : {false, true})
    { check(!dbg::native_wasm_step_boundary::prepare(display,semantics,outside,begin,begin+outside.size(),begin,next), "conditional outside-owner target refuses even with bounded fallthrough for SI and NI"); }
    constexpr ::std::array<::std::uint8_t, 3u> last_conditional{0x90u,0x75u,0xfdu};
    for(bool next : {false, true})
    { check(!dbg::native_wasm_step_boundary::prepare(display,semantics,last_conditional,begin,begin+last_conditional.size(),begin+1u,next), "conditional owner-end fallthrough refuses even with known taken target for SI and NI"); }
    constexpr ::std::array<::std::uint8_t, 5u> forward{0xebu,0x01u,0x90u,0x90u,0xc3u};
    constexpr ::std::array<::std::uint8_t, 4u> backward{0x90u,0xebu,0xfdu,0xc3u};
    for(bool next : {false, true})
    {
        auto const f{dbg::native_wasm_step_boundary::prepare(display,semantics,forward,begin,begin+forward.size(),begin,next)};
        auto const b{dbg::native_wasm_step_boundary::prepare(display,semantics,backward,begin,begin+backward.size(),begin+1u,next)};
        check(bool(f) && f.first_successor == begin+3u && f.second_successor == 0u,
              "exact same-owner forward unconditional JMP admits one machine instruction");
        check(bool(b) && b.first_successor == begin && b.second_successor == 0u,
              "exact same-owner backward unconditional JMP admits one machine instruction");
    }
    constexpr ::std::array<::std::uint8_t, 2u> final_ordinary{0x31u,0xc0u};
    check(!dbg::native_wasm_step_boundary::prepare(display,semantics,final_ordinary,begin,begin+final_ordinary.size(),begin,false), "ordinary owner-end escapes cannot step");
    constexpr ::std::array<::std::uint8_t, 3u> pop_flags{0x9du,0x90u,0xc3u};
    check(!dbg::native_wasm_step_boundary::prepare(display,semantics,pop_flags,begin,begin+pop_flags.size(),begin,false), "POPF cannot clear debugger TF");
    // Exact instruction bytes from all six reported VM bridge families.
    // Every instruction is first decoded by actual LLVM MC; refusal must not
    // depend on a fabricated classification or an undecodable byte sequence.
    auto check_call{[&](auto const& bytes)
    {
        auto const decoded{semantics.decode(begin, bytes)};
        check(bool(decoded) && decoded.semantics().kind == dbg::native_instruction_semantics::flow::call,
              "reported bridge CALL is an actual fully decoded MC instruction");
        for(bool next : {false, true})
        {
            auto const rejected{dbg::native_wasm_step_boundary::prepare(display, semantics, bytes,
                begin, begin + bytes.size(), begin, next)};
            check(!rejected && rejected.unavailable_reason == dbg::native_next_policy::reason::call_continuation_unavailable,
                  "reported VM bridge CALL refuses before SI and NI execution");
        }
    }};
    check_call(::std::array<::std::uint8_t, 3u>{0xffu,0xd0u,0x90u});
    check_call(::std::array<::std::uint8_t, 3u>{0xffu,0xd3u,0x90u});
    check_call(::std::array<::std::uint8_t, 4u>{0x41u,0xffu,0xd5u,0x90u});
    auto check_control{[&](auto const& bytes)
    {
        auto const decoded{semantics.decode(begin, bytes)};
        check(bool(decoded), "control corpus must decode with actual MC; unknown is no passing witness");
        for(bool next : {false, true})
        {
            check(!dbg::native_wasm_step_boundary::prepare(display, semantics, bytes,
                begin, begin + bytes.size(), begin, next), "control/trap/prefix instruction refuses before execution");
        }
    }};
    check_control(::std::array<::std::uint8_t, 3u>{0xffu,0xe0u,0x90u}); // indirect JMP
    check_control(::std::array<::std::uint8_t, 4u>{0xc2u,0x08u,0x00u,0x90u}); // RET imm16
    check_control(::std::array<::std::uint8_t, 2u>{0xc3u,0x90u}); // RET
    check_control(::std::array<::std::uint8_t, 3u>{0x0fu,0x05u,0x90u}); // SYSCALL
    check_control(::std::array<::std::uint8_t, 3u>{0x0fu,0x34u,0x90u}); // SYSENTER
    check_control(::std::array<::std::uint8_t, 3u>{0xcdu,0x80u,0x90u}); // INT
    check_control(::std::array<::std::uint8_t, 2u>{0xccu,0x90u}); // INT3
    check_control(::std::array<::std::uint8_t, 3u>{0x0fu,0x0bu,0x90u}); // UD2
    check_control(::std::array<::std::uint8_t, 3u>{0xf3u,0xa4u,0x90u}); // REP MOVSB
    check_control(::std::array<::std::uint8_t, 4u>{0xf0u,0x01u,0x00u,0x90u}); // LOCK ADD
    check_control(::std::array<::std::uint8_t, 3u>{0x8eu,0xd0u,0x90u}); // MOV SS
    check_control(::std::array<::std::uint8_t, 5u>{0x48u,0x0fu,0xb2u,0x00u,0x90u}); // LSS
    check_control(::std::array<::std::uint8_t, 2u>{0x9du,0x90u}); // POPF
    constexpr ::std::array<::std::uint8_t, 5u> vector{0x66u,0x0fu,0xefu,0xc0u,0x90u}; // PXOR XMM0,XMM0
    auto const simd{dbg::native_wasm_step_boundary::prepare(display,semantics,vector,begin,begin+vector.size(),begin,false)};
    check(bool(simd) && simd.decoded.size == 4u && simd.first_successor == begin+4u,
          "ordinary actual SIMD retains precisely its authenticated fallthrough");
    check(!dbg::native_wasm_step_boundary::prepare(display,semantics,owner,begin,begin+owner.size()-1u,begin,false), "different complete owner extent refuses");
    check(!dbg::native_wasm_step_boundary::prepare(display,semantics,owner,begin,begin+owner.size(),begin+owner.size(),false), "end PC refuses");
    check(!dbg::native_wasm_step_boundary::prepare(display,semantics,owner,begin,begin+owner.size(),0u,false), "zero PC refuses");
    ::fast_io::io::println("native_wasm_step_boundary: PASS actual MC DATA corpus; no trap/VM authority");
    return 0;
#else
    ::fast_io::io::println("native_wasm_step_boundary: SKIP exact current corpus is X86-64 only");
    return 77;
#endif
}
