#include <uwvm2/uwvm/io/impl.h>
#include <uwvm2/runtime/compiler/shared/wasm1p1_simd.h>
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>
#include <uwvm2/utils/macro/push_macros.h>
#include "../0014.llvm_jit/fixtures/fp_rounding_oracle.h"
#include <cstdio>
namespace s = uwvm2::runtime::compiler::shared::wasm1p1_simd_details;
using code = s::simd_code;

template <class UInt, unsigned Op>
UWVM_NOINLINE s::wasm_v128 evaluate(s::wasm_v128 x, s::wasm_v128 y)
{
#define CASES(S)                                                                                                                                               \
    if constexpr(Op == 0) return s::eval_full_unop<code::S##_ceil>(x);                                                                                         \
    if constexpr(Op == 1) return s::eval_full_unop<code::S##_floor>(x);                                                                                        \
    if constexpr(Op == 2) return s::eval_full_unop<code::S##_trunc>(x);                                                                                        \
    if constexpr(Op == 3) return s::eval_full_unop<code::S##_nearest>(x);                                                                                      \
    if constexpr(Op == 4) return s::eval_full_unop<code::S##_sqrt>(x);                                                                                         \
    if constexpr(Op == 5) return s::eval_full_binop<code::S##_add>(x, y);                                                                                      \
    if constexpr(Op == 6) return s::eval_full_binop<code::S##_sub>(x, y);                                                                                      \
    if constexpr(Op == 7) return s::eval_full_binop<code::S##_mul>(x, y);                                                                                      \
    if constexpr(Op == 8) return s::eval_full_binop<code::S##_div>(x, y);                                                                                      \
    if constexpr(Op == 9) return s::eval_full_binop<code::S##_min>(x, y);                                                                                      \
    if constexpr(Op == 10) return s::eval_full_binop<code::S##_max>(x, y)
    if constexpr(sizeof(UInt) == 4) { CASES(f32x4); }
    else
    {
        CASES(f64x2);
    }
#undef CASES
    if constexpr(Op == 11)
    {
        if constexpr(sizeof(UInt) == 4) { return s::eval_full_unop<code::f64x2_promote_low_f32x4>(x); }
        else
        {
            return s::eval_full_unop<code::f32x4_demote_f64x2_zero>(x);
        }
    }
    if constexpr(Op == 12)
    {
        if constexpr(sizeof(UInt) == 4) { return s::eval_v128_binop<s::v128_binop::f32x4_add>(x, y); }
        else
        {
            return s::eval_full_binop<code::f64x2_add>(x, y);
        }
    }
}

template <class UInt>
unsigned check()
{
    using Float = std::conditional_t<sizeof(UInt) == 4, s::wasm_f32, s::wasm_f64>;
    constexpr unsigned count = 16 / sizeof(UInt);
    constexpr UInt sign = UInt{1} << (sizeof(UInt) * 8 - 1);
    constexpr UInt inf = sizeof(UInt) == 4 ? UInt{0x7f800000u} : static_cast<UInt>(0x7ff0000000000000ull);
    constexpr UInt quiet = UInt{1} << (sizeof(UInt) == 4 ? 22 : 51);
    constexpr UInt one = sizeof(UInt) == 4 ? UInt{0x3f800000u} : static_cast<UInt>(0x3ff0000000000000ull);
    using fn = s::wasm_v128 (*)(s::wasm_v128, s::wasm_v128);
#define ITEM(I) evaluate<UInt, I>
    fn functions[]{ITEM(0), ITEM(1), ITEM(2), ITEM(3), ITEM(4), ITEM(5), ITEM(6), ITEM(7), ITEM(8), ITEM(9), ITEM(10), ITEM(11), ITEM(12)};
#undef ITEM
    unsigned errors{};
    for(UInt pattern: {UInt(inf | 1), UInt(inf | quiet), UInt(inf | quiet | 123), UInt(sign | inf | 1), UInt(sign | inf | quiet)})
    {
        s::lane_array<UInt, count> x{}, y{};
        for(unsigned lane{}; lane != count; ++lane)
        {
            x.lane[lane] = pattern;
            y.lane[lane] = one;
        }
        for(unsigned op{}; op != 13; ++op)
        {
            auto result = functions[op](s::store_uint_lanes<UInt, count>(x), s::store_uint_lanes<UInt, count>(y));
            if(op == 11)
            {
                using Other = std::conditional_t<sizeof(UInt) == 4, std::uint64_t, std::uint32_t>;
                constexpr Other other_sign = Other{1} << (sizeof(Other) * 8 - 1);
                constexpr Other other_nan = sizeof(Other) == 4 ? Other{0x7fc00000} : static_cast<Other>(0x7ff8000000000000ull);
                auto out = s::load_uint_lanes<Other, 16 / sizeof(Other)>(result);
                for(unsigned lane{}; lane != 2; ++lane)
                {
                    bool canonical = (pattern & ~sign) == (inf | quiet);
                    if(canonical ? (out.lane[lane] & ~other_sign) != other_nan : (out.lane[lane] & other_nan) != other_nan) { ++errors; }
                }
                if constexpr(sizeof(UInt) == 8)
                {
                    if(out.lane[2] || out.lane[3]) { ++errors; }
                }
            }
            else
            {
                auto out = s::load_uint_lanes<UInt, count>(result);
                for(auto value: out.lane)
                {
                    if(!fp_rounding_oracle::matches<Float>(pattern, value, 0))
                    {
                        if(errors < 10)
                        {
                            std::fprintf(stderr,
                                         "SIMD NaN f%zu op=%u input=%llx got=%llx\n",
                                         sizeof(UInt) * 8,
                                         op,
                                         (unsigned long long)pattern,
                                         (unsigned long long)value);
                        }
                        ++errors;
                    }
                }
            }
        }
    }
    // Invalid operations without NaN operands must also produce canonical NaNs.
    // This catches the native all-ones default NaN on 68881/SPARC.
    for(unsigned op: {4u, 5u, 6u, 7u, 8u})
    {
        s::lane_array<UInt, count> x{}, y{};
        for(unsigned lane{}; lane != count; ++lane)
        {
            x.lane[lane] = op == 4 ? sign | one : op < 7 ? inf : UInt{};
            y.lane[lane] = op == 5 ? sign | inf : op < 8 ? inf : UInt{};
        }
        auto out = s::load_uint_lanes<UInt, count>(functions[op](s::store_uint_lanes<UInt, count>(x), s::store_uint_lanes<UInt, count>(y)));
        for(auto value: out.lane)
        {
            if((value & ~sign) != (inf | quiet))
            {
                if(errors < 10) { std::fprintf(stderr, "SIMD invalid f%zu op=%u got=%llx\n", sizeof(UInt) * 8, op, (unsigned long long)value); }
                ++errors;
            }
        }
    }
    // Mixed finite lanes exercise each scalarized/vector rounding path.
    for(unsigned i{}; i != 128; ++i)
    {
        s::lane_array<UInt, count> x{};
        for(unsigned lane{}; lane != count; ++lane) { x.lane[lane] = std::bit_cast<UInt>(static_cast<Float>((static_cast<int>(i) - 64) * 0.5 + lane * 0.25)); }
        for(unsigned op{}; op != 4; ++op)
        {
            auto raw = s::store_uint_lanes<UInt, count>(x);
            auto out = s::load_uint_lanes<UInt, count>(functions[op](raw, raw));
            for(unsigned lane{}; lane != count; ++lane)
            {
                if(!fp_rounding_oracle::matches<Float>(x.lane[lane], out.lane[lane], op)) { ++errors; }
            }
        }
    }
    return errors;
}

int main()
{
    bool active{};
    uwvm2::runtime::lib::details::scoped_llvm_wasm_fp_environment environment{active};
    if(!environment.ready()) { return 2; }
    auto errors = check<std::uint32_t>() + check<std::uint64_t>();
    std::printf("SIMD NaN encoding: %u failures\n", errors);
    return errors != 0;
}
