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
    using vector_fn = void (*)(void const*, void*);
#define WIDTH(W) vector_round##W##_0, vector_round##W##_1, vector_round##W##_2, vector_round##W##_3, vector_round##W##_4
    vector_fn vector32[]{WIDTH(32)}, vector64[]{WIDTH(64)};
#undef WIDTH
    for(unsigned e{}; e < exponents; ++e)
    {
        for(unsigned variant{}; variant < 4; ++variant)
        {
            for(unsigned neg{}; neg < 2; ++neg)
            {
                U input = (U(e) << fraction) | (neg ? sign : 0) | (variant == 1 ? 1 : variant == 2 ? quiet : variant == 3 ? quiet | 1 : 0);
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
    std::printf("Native LLVM rounding: %u failures\n", errors);
    return errors != 0;
}
