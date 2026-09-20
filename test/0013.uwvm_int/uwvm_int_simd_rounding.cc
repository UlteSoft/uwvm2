// Exercise the actual shared interpreter evaluator with an independent bit oracle.
// In particular, SSE2 without SSE4.1 must quiet arithmetic sNaNs without a libm
// repair call. Non-arithmetic abs/neg must still preserve their payload bits.
// Parse the fixture's standard-library headers before importing the production
// BMI, matching production global-fragment ordering. With Clang 23/libc++,
// including <array> after this import can redeclare libc++'s __promote_t alias;
// that frontend/header-merge failure occurs before any evaluator codegen.
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#if defined(UWVM2TEST_SIMD_NAMED_MODULE)
import uwvm2.runtime.compiler.shared.wasm1p1_simd;
#else
#include <uwvm2/runtime/compiler/shared/wasm1p1_simd.h>
#endif

namespace v = uwvm2::runtime::compiler::shared::wasm1p1_simd_details;
using code = v::simd_code;
using vector = v::wasm_v128;

#define ROUND_WRAPPER(Name, Op) \
    extern "C" [[gnu::noinline]] void Name(vector* out, vector const* in) \
    { *out = v::eval_full_unop<code::Op>(*in); }
ROUND_WRAPPER(round_f32_ceil, f32x4_ceil)
ROUND_WRAPPER(round_f32_floor, f32x4_floor)
ROUND_WRAPPER(round_f32_trunc, f32x4_trunc)
ROUND_WRAPPER(round_f32_nearest, f32x4_nearest)
ROUND_WRAPPER(round_f64_ceil, f64x2_ceil)
ROUND_WRAPPER(round_f64_floor, f64x2_floor)
ROUND_WRAPPER(round_f64_trunc, f64x2_trunc)
ROUND_WRAPPER(round_f64_nearest, f64x2_nearest)
ROUND_WRAPPER(bits_f32_abs, f32x4_abs)
ROUND_WRAPPER(bits_f32_neg, f32x4_neg)
ROUND_WRAPPER(bits_f64_abs, f64x2_abs)
ROUND_WRAPPER(bits_f64_neg, f64x2_neg)
#undef ROUND_WRAPPER

using evaluate = void (*)(vector*, vector const*);
std::size_t checks{}, failures{};

template <typename Float>
void check_width()
{
    using UInt = std::conditional_t<sizeof(Float) == 4, std::uint32_t, std::uint64_t>;
    constexpr std::size_t lanes{16 / sizeof(UInt)};
    constexpr UInt sign{UInt{1} << (sizeof(UInt) * 8 - 1)};
    constexpr UInt infinity{static_cast<UInt>(sizeof(UInt) == 4 ? 0x7f800000ull : 0x7ff0000000000000ull)};
    constexpr UInt quiet{UInt{1} << (sizeof(UInt) == 4 ? 22 : 51)};
    constexpr auto bits{[](Float value) { return std::bit_cast<UInt>(value); }};
    struct sample { UInt input; UInt output[4]; unsigned nan; };
    // Output order is ceil, floor, trunc, nearest/ties-to-even. Constants do not
    // call the implementation under test or depend on the host rounding mode.
    sample const cases[]{
        {0, {0, 0, 0, 0}, 0},
        {sign, {sign, sign, sign, sign}, 0},
        {1, {bits(1), 0, 0, 0}, 0},
        {sign | 1, {sign, bits(-1), sign, sign}, 0},
        {bits(Float{0.5}), {bits(1), 0, 0, 0}, 0},
        {bits(Float{-0.5}), {sign, bits(-1), sign, sign}, 0},
        {bits(Float{0.75}), {bits(1), 0, 0, bits(1)}, 0},
        {bits(Float{-0.75}), {sign, bits(-1), sign, bits(-1)}, 0},
        {bits(Float{1.5}), {bits(2), bits(1), bits(1), bits(2)}, 0},
        {bits(Float{-1.5}), {bits(-1), bits(-2), bits(-1), bits(-2)}, 0},
        {bits(Float{2.5}), {bits(3), bits(2), bits(2), bits(2)}, 0},
        {bits(Float{-2.5}), {bits(-2), bits(-3), bits(-2), bits(-2)}, 0},
        {bits(Float{3.5}), {bits(4), bits(3), bits(3), bits(4)}, 0},
        {bits(Float{-3.5}), {bits(-3), bits(-4), bits(-3), bits(-4)}, 0},
        {infinity - 1, {infinity - 1, infinity - 1, infinity - 1, infinity - 1}, 0},
        {infinity, {infinity, infinity, infinity, infinity}, 0},
        {sign | infinity, {sign | infinity, sign | infinity, sign | infinity, sign | infinity}, 0},
        {infinity | 1, {}, 1},
        {sign | infinity | 1, {}, 1},
        {infinity | quiet | 0x123, {}, 1},
        {infinity | quiet, {}, 2},
        {sign | infinity | quiet, {}, 2},
    };
    evaluate const f32[]{round_f32_ceil, round_f32_floor, round_f32_trunc, round_f32_nearest};
    evaluate const f64[]{round_f64_ceil, round_f64_floor, round_f64_trunc, round_f64_nearest};
    auto const* functions{sizeof(Float) == 4 ? f32 : f64};
    for(unsigned op{}; op != 4; ++op)
    {
        for(std::size_t n{}; n != std::size(cases); ++n)
        {
            v::lane_array<UInt, lanes> raw;
            for(std::size_t lane{}; lane != lanes; ++lane) { raw.lane[lane] = cases[(n + lane) % std::size(cases)].input; }
            auto input{v::store_uint_lanes(raw)};
            vector output;
            functions[op](&output, &input);
            auto result{v::load_uint_lanes<UInt, lanes>(output)};
            for(std::size_t lane{}; lane != lanes; ++lane)
            {
                auto const& expected{cases[(n + lane) % std::size(cases)]};
                auto const actual{result.lane[lane]};
                bool const pass{expected.nan == 0 ? actual == expected.output[op] :
                    expected.nan == 2 ? (actual & ~sign) == (infinity | quiet) :
                    (actual & (infinity | quiet)) == (infinity | quiet)};
                ++checks;
                if(!pass)
                {
                    ++failures;
                    if(failures <= 12) { std::printf("FAIL width=%zu op=%u input=%llx actual=%llx\n", sizeof(Float) * 8, op,
                        static_cast<unsigned long long>(expected.input), static_cast<unsigned long long>(actual)); }
                }
            }
        }
    }
    evaluate const bit_functions[]{sizeof(Float) == 4 ? bits_f32_abs : bits_f64_abs,
                                   sizeof(Float) == 4 ? bits_f32_neg : bits_f64_neg};
    for(unsigned op{}; op != 2; ++op)
    {
        for(auto const& item: cases)
        {
            v::lane_array<UInt, lanes> raw;
            for(auto& lane: raw.lane) { lane = item.input; }
            auto input{v::store_uint_lanes(raw)};
            vector output;
            bit_functions[op](&output, &input);
            auto result{v::load_uint_lanes<UInt, lanes>(output)};
            UInt const expected{op == 0 ? item.input & ~sign : item.input ^ sign};
            for(auto actual: result.lane) { ++checks; if(actual != expected) { ++failures; } }
        }
    }
}

int main()
{
    check_width<float>();
    check_width<double>();
    std::printf("SIMD rounding/transport checks=%zu failures=%zu\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
