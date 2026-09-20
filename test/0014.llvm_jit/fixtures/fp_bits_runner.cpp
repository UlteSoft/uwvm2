// Call generated objects through raw/integer buffers, never through an ordinary
// C++ floating prototype for the private i386 ABI. Compile the production bridge
// with x87 and SSE2 separately: emitter features and native bridge flags may differ.
// Integer observations catch sNaN corruption even when optimized inlining would not.
// Linked with the i386 object emitted by fp_bits_lowering.cpp, not a standalone unit target.
#include <uwvm2/runtime/compiler/shared/strict_float_bits.h>
#include <uwvm2/runtime/lib/uwvm_runtime_wasm_fp_environment.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <type_traits>
#include "fp_rounding_oracle.h"
namespace fp = uwvm2::runtime::compiler::shared::strict_float_jit;

extern "C" std::uint64_t uwvm_strict_float_bits_v1(std::uint64_t a, std::uint64_t b, std::uint32_t op) { return fp::bridge(a, b, op); }

#define DECL(W, O) extern "C" void bits_##W##_##O(void const*, void const*, void*);
#define EACH(M, W)                                                                                                                                             \
    M(W, 0)                                                                                                                                                    \
    M(W, 1)                                                                                                                                                    \
    M(W, 2)                                                                                                                                                    \
    M(W, 3) M(W, 4) M(W, 5) M(W, 6) M(W, 7) M(W, 8) M(W, 9) M(W, 10) M(W, 11) M(W, 12) M(W, 13) M(W, 14) M(W, 15) M(W, 16) M(W, 17) M(W, 18) M(W, 19) M(W, 20) \
        M(W, 21) M(W, 22) M(W, 23) M(W, 24)
EACH(DECL, 32)
EACH(DECL, 64)
#undef DECL
using function = void (*)(void const*, void const*, void*);
#define ITEM(W, O) bits_##W##_##O,
function calls[2][25]{{EACH(ITEM, 32)}, {EACH(ITEM, 64)}};
#undef ITEM
#undef EACH

template <class UInt>
unsigned check(unsigned op, UInt x, UInt y, UInt expected)
{
    UInt actual{};
    calls[sizeof(UInt) == 8][op](&x, &y, &actual);
    if(actual == expected) { return 0; }
    std::fprintf(stderr,
                 "LLVM f%zu op=%u expected=%llx actual=%llx\n",
                 sizeof(UInt) * 8,
                 op,
                 static_cast<unsigned long long>(expected),
                 static_cast<unsigned long long>(actual));
    return 1;
}

template <class UInt>
unsigned sign_suite(UInt value)
{
    constexpr UInt sign = UInt{1} << (sizeof(UInt) * 8 - 1);
    unsigned errors{};
    for(UInt rhs: {UInt{}, sign})
    {
        errors += check(0, value, rhs, value);
        errors += check(1, value, rhs, value & ~sign);
        errors += check(2, value, rhs, value ^ sign);
        errors += check(3, value, rhs, (value & ~sign) | rhs);
    }
    return errors;
}

extern "C" void compare_32(void const*, void const*, void*);
extern "C" void compare_64(void const*, void const*, void*);
extern "C" void mixed_direct(void const*, void*);
extern "C" void mixed_indirect(void const*, void*);

unsigned aggregate_suite()
{
    unsigned errors{};
    for(std::uint32_t a: {0u, 0x80000000u, 1u, 0x7f800001u, 0xffc00123u})
    {
        for(std::uint64_t b: {0ull, 0x8000000000000000ull, 1ull, 0x7ff0000000000001ull, 0xfff8000000000123ull})
        {
            unsigned char input[24], output[24];
            std::uint64_t integer64{0xfedcba9876543210ull};
            std::uint32_t integer32{0x87654321u};
            std::memcpy(input, &a, 4);
            std::memcpy(input + 4, &b, 8);
            std::memcpy(input + 12, &integer64, 8);
            std::memcpy(input + 20, &integer32, 4);
            for(auto call: {mixed_direct, mixed_indirect})
            {
                std::memset(output, 0xaa, sizeof(output));
                call(input, output);
                if(std::memcmp(input, output, sizeof(input))) { ++errors; }
            }
        }
    }
    if(errors) { std::fprintf(stderr, "LLVM mixed-return failures=%u\n", errors); }
    return errors;
}

template <class UInt>
unsigned comparison_suite()
{
    using Float = std::conditional_t<sizeof(UInt) == 4, float, double>;
    UInt const infinity = sizeof(UInt) == 4 ? static_cast<UInt>(0x7f800000u) : static_cast<UInt>(0x7ff0000000000000ull);
    UInt const sign = UInt{1} << (sizeof(UInt) * 8 - 1);
    UInt const one = sizeof(UInt) == 4 ? static_cast<UInt>(0x3f800000u) : static_cast<UInt>(0x3ff0000000000000ull);
    UInt const values[]{0, sign, 1, sign | 1, one, sign | one, infinity, infinity | sign, infinity | 1, sign | infinity | 123};
    unsigned errors{};
    for(UInt a: values)
    {
        for(UInt c: values)
        {
            UInt x[2]{a, static_cast<UInt>(a ^ sign)}, y[2]{c, static_cast<UInt>(c ^ sign)};
            std::uint32_t actual[128]{};
            if constexpr(sizeof(UInt) == 4) { compare_32(x, y, actual); }
            else
            {
                compare_64(x, y, actual);
            }
            for(unsigned kind = 0; kind != 4; ++kind)
            {
                for(unsigned lane = 0; lane != 2; ++lane)
                {
                    unsigned index = kind & 1u ? lane : 0;
                    Float left, right;
                    std::memcpy(&left, &x[index], sizeof(Float));
                    std::memcpy(&right, &y[index], sizeof(Float));
                    bool unordered = std::isnan(left) || std::isnan(right), eq = left == right, lt = left<right, gt = left> right;
                    bool expected[]{false,
                                    eq,
                                    gt,
                                    gt || eq,
                                    lt,
                                    lt || eq,
                                    !unordered && !eq,
                                    !unordered,
                                    unordered,
                                    unordered || eq,
                                    unordered || gt,
                                    unordered || gt || eq,
                                    unordered || lt,
                                    unordered || lt || eq,
                                    !eq,
                                    true};
                    for(unsigned p = 0; p != 16; ++p)
                    {
                        if(actual[kind * 32 + p * 2 + lane] != expected[p])
                        {
                            ++errors;
                            std::fprintf(stderr, "comparison f%zu kind=%u p=%u lane=%u\n", sizeof(UInt) * 8, kind, p, lane);
                        }
                    }
                }
            }
        }
    }
    return errors;
}

template <class UInt>
unsigned rounding_suite()
{
    using Float = std::conditional_t<sizeof(UInt) == 4, float, double>;
    constexpr unsigned fraction_bits = sizeof(UInt) == 4 ? 23 : 52;
    constexpr unsigned exponent_count = sizeof(UInt) == 4 ? 256 : 2048;
    constexpr UInt sign = UInt{1} << (sizeof(UInt) * 8 - 1);
    unsigned errors{};
    auto sample = [&](UInt input)
    {
        for(unsigned op{}; op != 4; ++op)
        {
            UInt actual{};
            calls[sizeof(UInt) == 8][10 + op](&input, &input, &actual);
            if(!fp_rounding_oracle::matches<Float>(input, actual, op))
            {
                if(errors < 8)
                {
                    std::fprintf(stderr,
                                 "LLVM rounding f%zu op=%u in=%llx actual=%llx\n",
                                 sizeof(UInt) * 8,
                                 op,
                                 (unsigned long long)input,
                                 (unsigned long long)actual);
                }
                ++errors;
            }
        }
    };
    for(unsigned e{}; e != exponent_count; ++e)
    {
        UInt base = UInt{e} << fraction_bits;
        for(UInt v: {base, UInt(base + 1), UInt(base - (e != 0)), UInt(base | (UInt{1} << (fraction_bits - 1)))})
        {
            sample(v);
            sample(v | sign);
        }
    }
    for(unsigned i{}; i != 32; ++i)
    {
        UInt base = std::bit_cast<UInt>(static_cast<Float>(i + 0.5));
        for(UInt v: {UInt(base - 1), base, UInt(base + 1)})
        {
            sample(v);
            sample(v | sign);
        }
    }
    std::uint64_t state{0x6a09e667f3bcc909ull};
    for(unsigned i{}; i != 4096; ++i)
    {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        sample(static_cast<UInt>(state));
    }
    return errors;
}

template <class UInt>
unsigned conversion_suite()
{
    constexpr unsigned frac = sizeof(UInt) == 4 ? 23 : 52;
    constexpr unsigned bias = sizeof(UInt) == 4 ? 127 : 1023;
    constexpr UInt sign = UInt{1} << (sizeof(UInt) * 8 - 1);
    unsigned errors{};
    auto sample = [&](UInt input)
    {
        for(unsigned op = 17; op != 25; ++op)
        {
            bool result64 = op == 19 || op == 20 || op == 23 || op == 24;
            auto expected = fp_rounding_oracle::to_integer(input, result64 ? 64 : 32, (op & 1) != 0);
            if(op < 21 && !expected.valid) { continue; }
            std::uint64_t out64{};
            std::uint32_t out32{};
            calls[sizeof(UInt) == 8][op](&input, &input, result64 ? static_cast<void*>(&out64) : &out32);
            auto actual = result64 ? out64 : out32;
            if(actual != expected.value)
            {
                if(errors < 8)
                {
                    std::fprintf(stderr,
                                 "LLVM integer f%zu op=%u in=%llx expected=%llx actual=%llx\n",
                                 sizeof(UInt) * 8,
                                 op,
                                 (unsigned long long)input,
                                 (unsigned long long)expected.value,
                                 (unsigned long long)actual);
                }
                ++errors;
            }
        }
    };
    // Around zero, +/-1, and both integer-width limits, including negative fractions.
    for(unsigned e{}; e != (sizeof(UInt) == 4 ? 256u : 2048u); ++e)
    {
        UInt base = UInt{e} << frac;
        for(UInt v: {base, UInt(base + 1), UInt(base - (e != 0)), UInt(base | (UInt{1} << (frac - 1)))})
        {
            sample(v);
            sample(v | sign);
        }
    }
    for(unsigned e: {bias, bias + 31, bias + 32, bias + 63, bias + 64})
    {
        UInt base = UInt{e} << frac;
        for(unsigned delta{}; delta != 256; ++delta)
        {
            sample(base + delta);
            sample(base - delta);
            sample((base + delta) | sign);
            sample((base - delta) | sign);
        }
    }
    std::uint64_t state{0xbb67ae8584caa73bull};
    for(unsigned i{}; i != 16384; ++i)
    {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        sample(static_cast<UInt>(state));
    }
    return errors;
}

int main()
{
    bool active{};
    uwvm2::runtime::lib::details::scoped_llvm_wasm_fp_environment environment{active};
    if(!environment.ready()) { return 2; }
    unsigned errors{};
    for(std::uint32_t value: {0u, 0x80000000u, 1u, 0x80000001u, 0x7f800001u, 0xff800123u, 0x7fc00123u}) { errors += sign_suite(value); }
    for(std::uint64_t value: {0ull, 0x8000000000000000ull, 1ull, 0x8000000000000001ull, 0x7ff0000000000001ull, 0xfff0000000000123ull})
    {
        errors += sign_suite(value);
    }
    errors += check(4, std::uint32_t{0x7f800001u}, std::uint32_t{}, std::uint32_t{0x7f800001u});
    errors += check(4, std::uint64_t{0x7ff0000000000001ull}, std::uint64_t{}, std::uint64_t{0x7ff0000000000001ull});
    for(unsigned wide = 0; wide != 2; ++wide)
    {
        for(unsigned op = 5; op != 17; ++op)
        {
            std::uint64_t x = wide ? 0x4002000000000000ull : 0x40100000u;  // 2.25
            std::uint64_t y = wide ? 0x3ff8000000000000ull : 0x3fc00000u;  // 1.5
            if(op == 14) { x = wide ? 0x40100000u : 0x4002000000000000ull; }
            if(op >= 15) { x = 0xffffffffffffffffull; }
            unsigned bridge_op = op <= 8 ? op - 5 : op == 9 ? 4 : op <= 13 ? op - 1 : op == 14 ? (wide ? 8 : 7) : op - 10;
            auto expected = fp::bridge(x, y, bridge_op | (wide ? 16u : 0u));
            // Keep source widths explicit on big-endian hosts too.
            std::uint32_t narrow = static_cast<std::uint32_t>(x), narrow_y = static_cast<std::uint32_t>(y), out32{};
            std::uint64_t out64{};
            void const* a = (op == 14 ? wide : (!wide && op < 15)) ? static_cast<void const*>(&narrow) : &x;
            void const* b = wide || op >= 14 ? static_cast<void const*>(&y) : &narrow_y;
            calls[wide][op](a, b, wide ? static_cast<void*>(&out64) : &out32);
            auto actual = wide ? out64 : out32;
            if(actual != expected)
            {
                ++errors;
                std::fprintf(stderr, "LLVM numeric f%u op=%u expected=%llx actual=%llx\n", wide ? 64 : 32, op, expected, actual);
            }
        }
    }
    for(unsigned wide = 0; wide != 2; ++wide)
    {
        for(unsigned op = 17; op != 25; ++op)
        {
            for(unsigned sample = 0; sample != 5; ++sample)
            {
                if(op < 21 && sample != 0)
                {
                    continue;  // Non-saturating conversion inputs must be valid.
                }
                std::uint64_t const values64[]{0x4002000000000000ull,
                                               0x7ff0000000000001ull,
                                               0x7ff0000000000000ull,
                                               0xfff0000000000000ull,
                                               0xc3f0000000000000ull};
                std::uint32_t const values32[]{0x40100000u, 0x7f800001u, 0x7f800000u, 0xff800000u, 0xdf800000u};
                bool const result64 = op == 19 || op == 20 || op == 23 || op == 24;
                bool const signed_result = op & 1u;
                std::uint64_t expected =
                    sample == 0     ? 2
                    : sample == 1   ? 0
                    : sample == 2   ? (signed_result ? (result64 ? 0x7fffffffffffffffull : 0x7fffffffull) : (result64 ? ~std::uint64_t{} : 0xffffffffull))
                    : signed_result ? (result64 ? 0x8000000000000000ull : 0x80000000ull)
                                    : 0;
                std::uint64_t out64{};
                std::uint32_t out32{};
                void const* input = wide ? static_cast<void const*>(&values64[sample]) : &values32[sample];
                calls[wide][op](input, input, result64 ? static_cast<void*>(&out64) : &out32);
                auto actual = result64 ? out64 : out32;
                if(actual != expected)
                {
                    ++errors;
                    std::fprintf(stderr, "LLVM convert f%u op=%u sample=%u expected=%llx actual=%llx\n", wide ? 64 : 32, op, sample, expected, actual);
                }
            }
        }
    }
    errors += comparison_suite<std::uint32_t>() + comparison_suite<std::uint64_t>();
    errors += rounding_suite<std::uint32_t>() + rounding_suite<std::uint64_t>();
    errors += conversion_suite<std::uint32_t>() + conversion_suite<std::uint64_t>();
    errors += aggregate_suite();
    std::printf("LLVM FP bits: %u failures\n", errors);
    return errors != 0;
}
