// Native generated rounding must satisfy the same oracle for scalar/vector
// and ordinary/constrained IR. Install production Wasm controls (including
// auxiliary SIMD controls) so inherited host FTZ/RM is not mistaken for a
// lowering error; keep bit transport and arithmetic quieting expectations distinct.
#include "fp_rounding_oracle.h"
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>
#include <cstdio>
extern "C" std::uint32_t rounding32(std::uint32_t, unsigned);
extern "C" std::uint64_t rounding64(std::uint64_t, unsigned);
#define DECLARE(W, I) extern "C" void vector_round##W##_##I(void const*, void*);
#define WIDTH(W) DECLARE(W, 0) DECLARE(W, 1) DECLARE(W, 2) DECLARE(W, 3) DECLARE(W, 4)
WIDTH(32)
WIDTH(64)
#undef WIDTH
#undef DECLARE

template <class F>
unsigned check()
{
    using U = fp_rounding_oracle::bits_t<F>;
    constexpr unsigned fraction = sizeof(F) == 4 ? 23 : 52, exponents = sizeof(F) == 4 ? 256 : 2048;
    constexpr U sign = U{1} << (sizeof(F) * 8 - 1), quiet = U{1} << (fraction - 1);
    unsigned errors{};
    std::uint64_t state{0x6a09e667f3bcc909ull};
    using vector_fn = void (*)(void const*, void*);
#define WIDTH(W) vector_round##W##_0, vector_round##W##_1, vector_round##W##_2, vector_round##W##_3, vector_round##W##_4
    vector_fn vector32[]{WIDTH(32)}, vector64[]{WIDTH(64)};
#undef WIDTH
    for(unsigned e{}; e < exponents; ++e)
    {
        for(unsigned variant{}; variant < 8; ++variant)
        {
            for(unsigned neg{}; neg < 2; ++neg)
            {
                state ^= state << 13; state ^= state >> 7; state ^= state << 17;
                constexpr U mask = (U{1} << fraction) - 1;
                // Both sides of the midpoint, carry into the next binade, and
                // nontrivial deterministic payloads exercise integer IR shifts
                // and ties-to-even independently of the production algorithm.
                U payloads[]{0, 1, U(quiet - 1), quiet, U(quiet | 1), U(mask - 1), mask, U(state) & mask};
                U input = (U(e) << fraction) | (neg ? sign : 0) | payloads[variant];
                for(unsigned op{}; op < 5; ++op)
                {
                    U actual;
                    if constexpr(sizeof(F) == 4) { actual = rounding32(input, op); }
                    else
                    {
                        actual = rounding64(input, op);
                    }
                    if(!fp_rounding_oracle::matches<F>(input, actual, op == 4 ? 3 : op))
                    {
                        if(errors < 8)
                        {
                            std::printf("f%zu op=%u input=%llx actual=%llx\n", sizeof(F) * 8, op, (unsigned long long)input, (unsigned long long)actual);
                        }
                        ++errors;
                    }
                    if(e == 0 || e == exponents - 1 || (e > exponents / 2 - 4 && e < exponents / 2 + 4))
                    {
                        U lanes[16 / sizeof(F)]{}, out[16 / sizeof(F)]{};
                        for(unsigned lane{}; lane != 16 / sizeof(F); ++lane)
                        {
                            lanes[lane] = lane == 0 ? input : std::bit_cast<U>(static_cast<F>(lane * 0.25 - 0.5));
                        }
                        (sizeof(F) == 4 ? vector32 : vector64)[op](lanes, out);
                        for(unsigned lane{}; lane != 16 / sizeof(F); ++lane)
                        {
                            if(!fp_rounding_oracle::matches<F>(lanes[lane], out[lane], op == 4 ? 3 : op))
                            {
                                if(errors < 8)
                                {
                                    std::printf("vector f%zu lane=%u op=%u input=%llx actual=%llx\n",
                                                sizeof(F) * 8,
                                                lane,
                                                op,
                                                (unsigned long long)lanes[lane],
                                                (unsigned long long)out[lane]);
                                }
                                ++errors;
                            }
                        }
                    }
                }
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
    auto errors = check<float>() + check<double>();
#if defined(__riscv) && __riscv_xlen == 32
    // roundeven encodes a fixed mode, unlike nearbyint. Prove the new RV32
    // libcall-free f64 lowering and constrained-to-native f32 rewrite do not
    // accidentally depend on caller FRM. Check both scalar and every SIMD lane.
    for(int mode: {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO})
    {
        if(std::fesetround(mode) != 0) { return 3; }
        std::uint64_t inputs[]{0x3ff8000000000000ull, 0x4004000000000000ull, 0xbfe0000000000000ull, 0xc004000000000000ull};
        std::uint64_t expected[]{0x4000000000000000ull, 0x4000000000000000ull, 0x8000000000000000ull, 0xc000000000000000ull};
        for(unsigned i{}; i != 4; ++i) { errors += rounding64(inputs[i], 4) != expected[i]; }
        for(unsigned i{}; i != 4; i += 2)
        {
            std::uint64_t output[2]{};
            vector_round64_4(inputs + i, output);
            errors += output[0] != expected[i] || output[1] != expected[i + 1];
        }
        std::uint32_t inputs32[]{0x3fc00000u, 0x40200000u, 0xbf000000u, 0xc0200000u};
        std::uint32_t expected32[]{0x40000000u, 0x40000000u, 0x80000000u, 0xc0000000u};
        std::uint32_t output32[4]{};
        vector_round32_4(inputs32, output32);
        for(unsigned i{}; i != 4; ++i)
        { errors += rounding32(inputs32[i], 4) != expected32[i] || output32[i] != expected32[i]; }
    }
#endif
    std::printf("Native LLVM rounding: %u failures\n", errors);
    return errors != 0;
}
