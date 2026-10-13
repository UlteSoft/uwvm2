// LLVM MC DATA regression, never proof of an executed Wasm/VM instruction.
#include <uwvm2/uwvm/debugger/native_wasm_call_continuation.h>
#include <uwvm2/uwvm/debugger/native_owned_instruction_semantics.h>
#include <fast_io.h>
namespace dbg = ::uwvm2::uwvm::debugger;
static void check(bool value, char const* text) noexcept
{
    if(!value) { ::fast_io::io::perrln("debug_native_call_continuation_mc: ", ::fast_io::mnp::os_c_str(text)); ::fast_io::fast_terminate(); }
}
int main()
{
#if (!defined(UWVM_USE_LLVM_JIT) && !defined(UWVM_USE_DEFAULT_JIT)) || (!defined(__x86_64__) && !defined(_M_X64))
    ::fast_io::io::println("debug_native_call_continuation_mc: UNQUALIFIED actual x86-64 MC target unavailable"); return 77;
#else
    dbg::native_disassembly::decoder display{};
    dbg::native_owned_instruction_semantics::decoder semantic{};
    check(bool(display) && bool(semantic), "actual LLVM MC contexts required");
    struct test_case { char const* name; ::std::uint8_t bytes[16u]; ::std::size_t size, offset, expected_continuation; };
    test_case const cases[]{
        {"actual RAX bridge nearCALL", {0x90u,0xffu,0xd0u,0x90u,0xc3u}, 5u,1u,3u},
        {"actual RBX bridge nearCALL", {0x90u,0xffu,0xd3u,0x90u,0xc3u}, 5u,1u,3u},
        {"actual R13 bridge nearCALL", {0x90u,0x41u,0xffu,0xd5u,0x90u,0xc3u}, 6u,1u,4u},
        {"nearCALL pcrel32 outside scalar owner", {0x90u,0xe8u,0xf0u,0xffu,0x7fu,0u,0x90u,0xc3u}, 8u,1u,6u},
        {"nearCALL memory operand DATA only", {0x90u,0xffu,0x55u,0xf0u,0x90u,0xc3u}, 6u,1u,4u},
        {"last nearCALL has no owned continuation", {0x90u,0xffu,0xd0u}, 3u,1u,0u},
        {"truncated nearCALL refuses", {0x90u,0xffu}, 2u,1u,0u},
        {"mid-instruction scalar PC refuses", {0x90u,0xe8u,0u,0u,0u,0u,0x90u,0xc3u}, 8u,2u,0u},
        {"ordinary is not nearCALL", {0x90u,0x90u,0xc3u}, 3u,1u,0u},
        {"return is not call-next", {0x90u,0xc3u,0x90u}, 3u,1u,0u},
        {"syscall is never call-next", {0x90u,0x0fu,0x05u,0x90u}, 4u,1u,0u},
        {"far call is not near ABI", {0x90u,0xffu,0xd8u,0x90u,0xc3u}, 5u,1u,0u},
        {"segment prefixed CALL refuses", {0x90u,0x64u,0xffu,0xd0u,0x90u,0xc3u}, 6u,1u,0u},
        {"segment memory CALL refuses", {0x90u,0x64u,0xffu,0x10u,0x90u,0xc3u}, 6u,1u,0u},
        {"address-size memory CALL refuses", {0x90u,0x67u,0xffu,0x10u,0x90u,0xc3u}, 6u,1u,0u},
        {"ignored branch-hint CALL refuses", {0x90u,0x3eu,0xffu,0xd0u,0x90u,0xc3u}, 6u,1u,0u},
        {"operand-size prefixed CALL refuses", {0x90u,0x66u,0xffu,0xd0u,0x90u,0xc3u}, 6u,1u,0u},
        {"REP prefixed CALL refuses", {0x90u,0xf3u,0xffu,0xd0u,0x90u,0xc3u}, 6u,1u,0u},
    };
    constexpr ::std::uintptr_t origin{0x1000u}; // scalar DATA only; never read as an address
    for(auto const& row : cases)
    {
        check(row.size != 0u && row.size <= sizeof(row.bytes) && row.offset < row.size, "fixed bounded MC corpus");
        // [owned complete fixture byte array ... checked row.size<=16] end
        // [safe                                                      ]
        //  ^^ scalar origin/PC are display metadata, never converted to pointers.
        auto const selected{dbg::native_wasm_call_continuation::prepare(display, semantic,
            {row.bytes, row.size}, origin, origin + row.size, origin + row.offset)};
        check(bool(selected) == (row.expected_continuation != 0u), row.name);
        if(selected) { check(selected.continuation == origin + row.expected_continuation && bool(selected.decoded), row.name); }
    }
    // Actual MC CALL64r encoding for all sixteen complete GPRs. These
    // buffers are DATA; no register context or native code is executed here.
    constexpr ::std::size_t snapshot_index[]{0u,2u,3u,1u,7u,6u,4u,5u,8u,9u,10u,11u,12u,13u,14u,15u};
    for(::std::size_t register_number{}; register_number != 16u; ++register_number)
    {
        ::std::uint8_t bytes[3u]{};
        auto const high{register_number >= 8u};
        auto const offset{high ? 1u : 0u};
        if(high) { bytes[0u] = 0x41u; }
        // [fixed owned bytes[3]][offset0 or1, opcode at offset+0/+1] end
        // [safe] register_number<16 selects REX.B and exact ModRM register;
        //  ^^ both writes fit the whole checked fixed byte owner.
        bytes[offset] = 0xffu;
        bytes[offset+1u] = static_cast<::std::uint8_t>(0xd0u + (register_number & 7u));
        auto const decoded{semantic.decode(origin, {bytes, offset+2u})};
        check(decoded && decoded.safe_for_call_continuation() &&
              decoded.call_register_index() == snapshot_index[register_number],
              "actual complete nearCALL GPR operand matches fixed kernel snapshot order");
    }
    for(auto const& row : cases)
    {
        check(row.size != 0u && row.size <= sizeof(row.bytes) && row.offset < row.size,
              "fixed bounded call-operand DATA corpus before suffix borrow");
        // [fixed complete owned bytes0 ... row.offset ... checked row.size<=16] end
        // [safe] offset<size was proved BEFORE advancing this bounded byte suffix;
        //  ^^ origin+offset remains scalar display DATA, never a pointer to read.
        auto const decoded{semantic.decode(origin+row.offset, {row.bytes+row.offset, row.size-row.offset})};
        if(row.bytes[row.offset] == 0xe8u || (row.size-row.offset >= 2u && row.bytes[row.offset] == 0xffu && row.bytes[row.offset+1u] == 0x55u))
        { check(decoded.call_register_index() == SIZE_MAX, "PC-relative/memory call cannot invent a GPR operand"); }
    }
    ::fast_io::io::println("debug_native_call_continuation_mc: PASS actual-MC nearCALL+exact-owned-continuation DATA; runtime-NI-qualified=no");
#endif
}
