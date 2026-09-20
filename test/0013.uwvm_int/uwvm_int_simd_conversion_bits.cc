// Fixed IEEE encodings are independent of the production rounding helpers.
// Run at O0/O3 both with and without SSE: inlining can otherwise hide a
// Float-returning helper whose ABI requires a disabled XMM register.
#include <uwvm2/runtime/compiler/shared/wasm1p1_simd.h>
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>
#include <cstdio>

namespace s = uwvm2::runtime::compiler::shared::wasm1p1_simd_details;
using code = s::simd_code;
using u32 = s::u32;
using u64 = s::u64;

template <code Op, class Input, class Output>
unsigned check(Input bits, Output expected)
{
    s::lane_array<Input, 16 / sizeof(Input)> input{};
    for (auto& lane : input.lane) { lane = bits; }
    auto value = s::eval_full_unop<Op>(s::store_uint_lanes<Input, 16 / sizeof(Input)>(input));
    auto result = s::load_uint_lanes<Output, 16 / sizeof(Output)>(value);
    constexpr unsigned used = sizeof(Input) == 8 || sizeof(Output) == 8 ? 2 : 4;
    unsigned errors{};
    for (unsigned i{}; i < 16 / sizeof(Output); ++i) { errors += result.lane[i] != (i < used ? expected : Output{}); }
    // These two opcodes have a separate compact evaluator, used by opfuncs.
    if constexpr (Op == code::f32x4_convert_i32x4_s || Op == code::f32x4_convert_i32x4_u)
    {
        constexpr auto compact = Op == code::f32x4_convert_i32x4_s ? s::v128_unop::f32x4_convert_i32x4_s : s::v128_unop::f32x4_convert_i32x4_u;
        auto compact_value = s::eval_v128_unop<compact>(s::store_uint_lanes<Input, 4>(input));
        for (auto lane : s::load_uint_lanes<u32, 4>(compact_value).lane) { errors += lane != expected; }
    }
    if (errors) { std::fprintf(stderr, "conversion op=%u input=%llx expected=%llx errors=%u\n", unsigned(Op),
                              (unsigned long long)bits, (unsigned long long)expected, errors); }
    return errors;
}

template <code Op, class UInt>
unsigned compare_case(UInt lhs, UInt rhs, bool expected)
{
    constexpr unsigned count = 16 / sizeof(UInt);
    s::lane_array<UInt, count> a{}, b{};
    for (unsigned i{}; i != count; ++i) { a.lane[i] = lhs; b.lane[i] = rhs; }
    auto left = s::store_uint_lanes<UInt, count>(a), right = s::store_uint_lanes<UInt, count>(b);
    auto result = s::eval_full_binop<Op>(left, right);
    UInt const mask = expected ? ~UInt{} : UInt{};
    unsigned errors{};
    for (auto lane : s::load_uint_lanes<UInt, count>(result).lane) { errors += lane != mask; }
    if constexpr (Op == code::f32x4_eq)
    {
        auto compact = s::eval_v128_binop<s::v128_binop::f32x4_eq>(left, right);
        for (auto lane : s::load_uint_lanes<u32, 4>(compact).lane) { errors += lane != mask; }
    }
    return errors;
}

template <class UInt>
unsigned comparisons()
{
    // The finite entries are in numeric order; the two zero entries have
    // equal rank. Expected comparisons depend on that independent ordering,
    // not the integer-key comparison implementation under test.
    constexpr u32 f32[]{0xff800000u, 0xbf800000u, 0x80000001u, 0x80000000u, 0,
                        1, 0x3f800000u, 0x7f800000u, 0x7f800001u, 0xff800001u, 0x7fc12345u};
    constexpr u64 f64[]{0xfff0000000000000ull, 0xbff0000000000000ull, 0x8000000000000001ull, 0x8000000000000000ull, 0,
                        1, 0x3ff0000000000000ull, 0x7ff0000000000000ull, 0x7ff0000000000001ull, 0xfff0000000000001ull, 0x7ff8123456789abcull};
    unsigned errors{};
    for (unsigned i{}; i != 11; ++i) for (unsigned j{}; j != 11; ++j)
    {
        UInt const a = static_cast<UInt>(sizeof(UInt) == 4 ? f32[i] : f64[i]);
        UInt const b = static_cast<UInt>(sizeof(UInt) == 4 ? f32[j] : f64[j]);
        bool const ordered = i < 8 && j < 8;
        unsigned const left = i == 4 ? 3 : i, right = j == 4 ? 3 : j;
#define CMP(NAME, EXPR) errors += compare_case<sizeof(UInt) == 4 ? code::f32x4_##NAME : code::f64x2_##NAME>(a, b, EXPR)
        CMP(eq, ordered && left == right);
        CMP(ne, !ordered || left != right);
        CMP(lt, ordered && left < right);
        CMP(gt, ordered && left > right);
        CMP(le, ordered && left <= right);
        CMP(ge, ordered && left >= right);
#undef CMP
    }
    return errors;
}

int main()
{
    bool active{};
    uwvm2::runtime::lib::details::scoped_llvm_wasm_fp_environment environment{active};
    if (!environment.ready()) { return 2; }
    unsigned errors{};
    struct integer_case { u32 input, f32_s, f32_u; u64 f64_s, f64_u; };
    constexpr integer_case integers[]{
        {0, 0, 0, 0, 0},
        {1, 0x3f800000u, 0x3f800000u, 0x3ff0000000000000ull, 0x3ff0000000000000ull},
        {0x01000001u, 0x4b800000u, 0x4b800000u, 0x4170000010000000ull, 0x4170000010000000ull},
        {0x01000003u, 0x4b800002u, 0x4b800002u, 0x4170000030000000ull, 0x4170000030000000ull},
        {0x7fffffffu, 0x4f000000u, 0x4f000000u, 0x41dfffffffc00000ull, 0x41dfffffffc00000ull},
        {0x80000000u, 0xcf000000u, 0x4f000000u, 0xc1e0000000000000ull, 0x41e0000000000000ull},
        {0xffffffffu, 0xbf800000u, 0x4f800000u, 0xbff0000000000000ull, 0x41efffffffe00000ull},
        {0xfeffffffu, 0xcb800000u, 0x4f7f0000u, 0xc170000010000000ull, 0x41efdfffffe00000ull},
    };
    for (auto c : integers)
    {
        errors += check<code::f32x4_convert_i32x4_s>(c.input, c.f32_s);
        errors += check<code::f32x4_convert_i32x4_u>(c.input, c.f32_u);
        errors += check<code::f64x2_convert_low_i32x4_s>(c.input, c.f64_s);
        errors += check<code::f64x2_convert_low_i32x4_u>(c.input, c.f64_u);
    }
    struct f32_case { u32 input, signed_result, unsigned_result; };
    constexpr f32_case singles[]{
        {0, 0, 0}, {0x80000000u, 0, 0}, {1, 0, 0}, {0x807fffffu, 0, 0},
        {0x7f800001u, 0, 0}, {0xff800001u, 0, 0}, {0x7fc12345u, 0, 0},
        {0x7f800000u, 0x7fffffffu, 0xffffffffu}, {0xff800000u, 0x80000000u, 0},
        {0x3fffffffu, 1, 1}, {0xbfffffffu, 0xffffffffu, 0},
        {0x4effffffu, 0x7fffff80u, 0x7fffff80u}, {0x4f000000u, 0x7fffffffu, 0x80000000u},
        {0xcf000000u, 0x80000000u, 0}, {0xcf000001u, 0x80000000u, 0},
        {0x4f7fffffu, 0x7fffffffu, 0xffffff00u}, {0x4f800000u, 0x7fffffffu, 0xffffffffu},
    };
    for (auto c : singles)
    {
        errors += check<code::i32x4_trunc_sat_f32x4_s>(c.input, c.signed_result);
        errors += check<code::i32x4_trunc_sat_f32x4_u>(c.input, c.unsigned_result);
    }
    struct f64_case { u64 input; u32 signed_result, unsigned_result; };
    constexpr f64_case doubles[]{
        {0, 0, 0}, {0x8000000000000000ull, 0, 0}, {1, 0, 0}, {0x800fffffffffffffull, 0, 0},
        {0x7ff0000000000001ull, 0, 0}, {0xfff0000000000001ull, 0, 0}, {0x7ff8123456789abcull, 0, 0},
        {0x7ff0000000000000ull, 0x7fffffffu, 0xffffffffu}, {0xfff0000000000000ull, 0x80000000u, 0},
        {0x3fffffffffffffffull, 1, 1}, {0xbfffffffffffffffull, 0xffffffffu, 0},
        {0x41dfffffffc00000ull, 0x7fffffffu, 0x7fffffffu}, {0x41dfffffffffffffull, 0x7fffffffu, 0x7fffffffu},
        {0x41e0000000000000ull, 0x7fffffffu, 0x80000000u}, {0xc1e0000000000000ull, 0x80000000u, 0},
        {0xc1e0000000000001ull, 0x80000000u, 0}, {0x41efffffffe00000ull, 0x7fffffffu, 0xffffffffu},
        {0x41f0000000000000ull, 0x7fffffffu, 0xffffffffu},
    };
    for (auto c : doubles)
    {
        errors += check<code::i32x4_trunc_sat_f64x2_s_zero>(c.input, c.signed_result);
        errors += check<code::i32x4_trunc_sat_f64x2_u_zero>(c.input, c.unsigned_result);
    }
    errors += comparisons<u32>();
    errors += comparisons<u64>();
    std::printf("SIMD conversions/comparisons: %u failures\n", errors);
    return errors != 0;
}
