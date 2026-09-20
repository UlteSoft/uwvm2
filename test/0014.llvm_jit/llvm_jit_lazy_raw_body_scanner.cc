#include <cstddef>
#include <cstdint>

#include <uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/translate.h>

namespace
{
    namespace lazy = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator;
    namespace feature = ::uwvm2::parser::wasm::standard::wasm1p1::features;

    template <::std::size_t N>
    [[nodiscard]] bool scan_one(::std::uint8_t const (&bytes)[N],
                                lazy::parser_feature_parameter_t const* policy,
                                bool expected,
                                ::std::size_t expected_consumed) noexcept
    {
        auto const* const begin{reinterpret_cast<::std::byte const*>(bytes)};
        auto cursor{begin};
        auto const* const end{begin + N};
        auto const accepted{lazy::details::skip_wasm_instruction_for_direct_call_scan(cursor, end, policy)};
        return accepted == expected && static_cast<::std::size_t>(cursor - begin) == expected_consumed;
    }

    template <::std::size_t N>
    [[nodiscard]] bool scan_all(::std::uint8_t const (&bytes)[N], lazy::parser_feature_parameter_t const* policy) noexcept
    {
        auto cursor{reinterpret_cast<::std::byte const*>(bytes)};
        auto const* const end{cursor + N};
        while(cursor != end)
        {
            if(!lazy::details::skip_wasm_instruction_for_direct_call_scan(cursor, end, policy)) { return false; }
        }
        return true;
    }
}

int main()
{
    // One deterministic stream covers typed-select, 0xfc, reference, SIMD fixed-width,
    // direct-call, and no-immediate instruction boundaries before the end opcode.
    constexpr ::std::uint8_t extended_stream[]{
        0x1cu, 0x01u, 0x7fu,              // select_t [i32]
        0xfcu, 0x11u, 0x00u,              // table.fill 0
        0xd0u, 0x70u, 0x1au,              // ref.null funcref; drop
        0xfdu, 0x0cu,                     // v128.const
        0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u,
        0x1au,                            // drop
        0x10u, 0x01u,                     // call 1
        0x0bu};                           // end
    if(!scan_all(extended_stream, nullptr)) { return 1; }

    // Reference Types/Core 2.0 use tableidx ::= u32, so a width-bounded
    // non-minimal ULEB128 spelling of zero is grammatical.
    constexpr ::std::uint8_t newer_nonminimal_zero[]{0x11u, 0x00u, 0x80u, 0x00u};
    if(!scan_one(newer_nonminimal_zero, nullptr, true, sizeof(newer_nonminimal_zero))) { return 2; }

    lazy::parser_feature_parameter_t mvp_policy{};
    feature::get_wasm1p1_parameter(mvp_policy).cli_mode = feature::wasm_feature_cli_mode::direct_wasmmvp;

    // Core 1.0 uses exactly one literal 0x00 after typeidx.
    constexpr ::std::uint8_t mvp_zero[]{0x11u, 0x00u, 0x00u};
    if(!scan_one(mvp_zero, ::std::addressof(mvp_policy), true, sizeof(mvp_zero))) { return 3; }
    if(!scan_one(newer_nonminimal_zero, ::std::addressof(mvp_policy), false, 0uz)) { return 4; }

    // Every malformed instruction below must fail transactionally: the outer
    // code_curr remains on the opcode and cannot reinterpret an immediate byte.
    constexpr ::std::uint8_t truncated_memarg[]{0x28u, 0x00u};
    constexpr ::std::uint8_t truncated_typed_select[]{0x1cu, 0x01u};
    constexpr ::std::uint8_t truncated_tableidx[]{0x11u, 0x00u, 0x80u};
    // The fifth-byte payload may carry only four u32 bits, and a fifth continuation byte is always too long.
    constexpr ::std::uint8_t overflowing_tableidx[]{0x11u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu, 0x1fu};
    constexpr ::std::uint8_t too_long_tableidx[]{0x11u, 0x00u, 0x80u, 0x80u, 0x80u, 0x80u, 0x80u, 0x00u};
    constexpr ::std::uint8_t truncated_v128_const[]{
        0xfdu, 0x0cu,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u};
    constexpr ::std::uint8_t unknown_opcode[]{0xffu};

    if(!scan_one(truncated_memarg, nullptr, false, 0uz)) { return 5; }
    if(!scan_one(truncated_typed_select, nullptr, false, 0uz)) { return 6; }
    if(!scan_one(truncated_tableidx, nullptr, false, 0uz)) { return 7; }
    if(!scan_one(truncated_v128_const, nullptr, false, 0uz)) { return 8; }
    if(!scan_one(unknown_opcode, nullptr, false, 0uz)) { return 9; }
    if(!scan_one(overflowing_tableidx, nullptr, false, 0uz)) { return 10; }
    if(!scan_one(too_long_tableidx, nullptr, false, 0uz)) { return 11; }
    return 0;
}
