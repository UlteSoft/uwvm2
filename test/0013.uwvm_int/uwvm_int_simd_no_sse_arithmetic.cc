// Run normally and with -mno-sse -mno-sse2 -mfpmath=387, at O0 and O3.
// The expected IEEE encodings are fixed independently of strict_float. In
// particular, the first f64 cases distinguish one Wasm rounding from an x87
// intermediate rounded to nearest and then rounded to binary64 a second time.
#include <uwvm2/uwvm/io/impl.h>
#include <uwvm2/runtime/compiler/shared/wasm1p1_simd.h>
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>
#include <cstdio>

namespace s = uwvm2::runtime::compiler::shared::wasm1p1_simd_details;
using code = s::simd_code;

template <code Op, class UInt>
unsigned check(UInt lhs, UInt rhs, UInt expected)
{
    constexpr std::size_t count = 16 / sizeof(UInt);
    s::lane_array<UInt, count> left{}, right{};
    for (std::size_t i{}; i != count; ++i) { left.lane[i] = lhs; right.lane[i] = rhs; }
    auto result = s::eval_full_binop<Op>(s::store_uint_lanes<UInt, count>(left), s::store_uint_lanes<UInt, count>(right));
    unsigned errors{};
    for (auto lane : s::load_uint_lanes<UInt, count>(result).lane) { errors += lane != expected; }
    if (errors) { std::fprintf(stderr, "SIMD arithmetic op=%u expected=%llx\n", unsigned(Op), (unsigned long long)expected); }
    return errors;
}

template <code Op, class Input, class Output>
unsigned conversion(Input bits, Output expected)
{
    s::lane_array<Input, 16 / sizeof(Input)> in{};
    for (auto& lane : in.lane) { lane = bits; }
    auto result = s::eval_full_unop<Op>(s::store_uint_lanes<Input, 16 / sizeof(Input)>(in));
    auto out = s::load_uint_lanes<Output, 16 / sizeof(Output)>(result);
    unsigned errors = (out.lane[0] != expected) + (out.lane[1] != expected);
    if constexpr (sizeof(Output) == 4) { errors += out.lane[2] != 0 || out.lane[3] != 0; }
    return errors;
}

int main()
{
    bool active{};
    uwvm2::runtime::lib::details::scoped_llvm_wasm_fp_environment environment{active};
    if (!environment.ready()) { return 2; }
    unsigned errors{};
    errors += check<code::f64x2_add>(0x3ff0000000000000ull, 0x3ca0000000000001ull, 0x3ff0000000000001ull);
    errors += check<code::f64x2_sub>(0x3ff0000000000000ull, 0x3c90000000000001ull, 0x3fefffffffffffffull);
    errors += check<code::f64x2_mul>(0x0010000000000000ull, 0x3fe0000000000000ull, 0x0008000000000000ull);
    errors += check<code::f64x2_mul>(1ull, 0x4000000000000000ull, 2ull);
    errors += check<code::f64x2_div>(1ull, 0x4000000000000000ull, 0ull);
    errors += check<code::f64x2_div>(3ull, 0x4000000000000000ull, 2ull);
    errors += check<code::f64x2_mul>(0x7fefffffffffffffull, 0x4000000000000000ull, 0x7ff0000000000000ull);
    errors += check<code::f32x4_add>(0x3f800000u, 0x33800000u, 0x3f800000u);
    errors += check<code::f32x4_add>(0x3f800000u, 0x33800001u, 0x3f800001u);
    errors += check<code::f32x4_mul>(0x00800000u, 0x3f000000u, 0x00400000u);
    errors += check<code::f32x4_min>(0u, 0x80000000u, 0x80000000u);
    errors += check<code::f32x4_max>(0x80000000u, 0u, 0u);
    errors += check<code::f64x2_min>(0xbff0000000000000ull, 0xc000000000000000ull, 0xc000000000000000ull);
    errors += check<code::f64x2_max>(0xbff0000000000000ull, 0xc000000000000000ull, 0xbff0000000000000ull);
    errors += conversion<code::f64x2_promote_low_f32x4>(1u, 0x36a0000000000000ull);
    errors += conversion<code::f64x2_promote_low_f32x4>(0x007fffffu, 0x380fffffc0000000ull);
    errors += conversion<code::f64x2_promote_low_f32x4>(0x80000000u, 0x8000000000000000ull);
    errors += conversion<code::f32x4_demote_f64x2_zero>(0x3ff0000010000001ull, 0x3f800001u);
    errors += conversion<code::f32x4_demote_f64x2_zero>(0x3690000000000000ull, 0u);
    errors += conversion<code::f32x4_demote_f64x2_zero>(0x3690000000000001ull, 1u);
    std::printf("SIMD integer-ABI arithmetic: %u failures\n", errors);
    return errors != 0;
}
