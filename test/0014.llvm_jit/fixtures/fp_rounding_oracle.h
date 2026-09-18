// Expected results must not reuse round_integral_bits or the bridge: that would
// let the same bug validate itself. Integral/fractional decomposition supplies
// rounding expectations; integer conversion expectations decode bits instead
// of invoking a possibly out-of-range native FP-to-integer conversion.
#pragma once
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace fp_rounding_oracle
{
    template <class Float>
    using bits_t = std::conditional_t<sizeof(Float) == 4, std::uint32_t, std::uint64_t>;

    // Only exact floating operations: split x into integral/fractional components,
    // then adjust the integral part by one. No call to the rounding functions under test.
    template <class Float>
#if defined(__clang__) && defined(__mips__) && defined(__mips_isa_rev) && __mips_isa_rev >= 6
    // Clang 22.1.8 and the tested LLVM 23 snapshot can spill an FGR64CC
    // comparison result with opcode 0 (PHI) when this independent oracle is
    // inlined into the multi-lane test driver. The machine verifier reports
    // illegal PHI stores after register allocation, before the test can run.
    // Isolate only the oracle: its input/result remain integer bits, while
    // production SIMD evaluators and their caller still compile at O3. Do not
    // disable engine optimization or change expected values to hide this
    // compiler failure. Other targets keep their existing test code generation.
    [[gnu::noinline]]
#endif
    bits_t<Float> expected(bits_t<Float> input, unsigned op)
    {
        using bits = bits_t<Float>;
        constexpr bits sign = bits{1} << (sizeof(bits) * 8 - 1);
        constexpr bits infinity = sizeof(bits) == 4 ? bits{0x7f800000} : static_cast<bits>(0x7ff0000000000000ull);
        constexpr bits quiet = bits{1} << (sizeof(bits) == 4 ? 22 : 51);
        if((input & ~sign) > infinity) { return input | quiet; }
        if((input & ~sign) == infinity) { return input; }
        long double whole{};
        // Keep the independent modf-based oracle, but avoid returning Float
        // from bit_cast: x86-64 -mno-sse cannot implement that ABI at -O0.
        Float value;
        std::memcpy(&value, &input, sizeof(value));
        auto fraction = std::modf(static_cast<long double>(value), &whole);
        if(op == 0 && fraction > 0) { whole += 1; }
        if(op == 1 && fraction < 0) { whole -= 1; }
        if(op == 3 && (std::fabs(fraction) > 0.5L || (std::fabs(fraction) == 0.5L && std::fmod(whole, 2.0L) != 0))) { whole += fraction < 0 ? -1 : 1; }
        if(whole == 0) { return input & sign; }
        return std::bit_cast<bits>(static_cast<Float>(whole));
    }

    template <class Float>
    bool matches(bits_t<Float> input, bits_t<Float> actual, unsigned op)
    {
        using bits = bits_t<Float>;
        constexpr bits sign = bits{1} << (sizeof(bits) * 8 - 1);
        constexpr bits infinity = sizeof(bits) == 4 ? bits{0x7f800000} : static_cast<bits>(0x7ff0000000000000ull);
        constexpr bits quiet = bits{1} << (sizeof(bits) == 4 ? 22 : 51);
        if((input & ~sign) == (infinity | quiet)) { return (actual & ~sign) == (infinity | quiet); }
        return (input & ~sign) > infinity ? ((actual & (infinity | quiet)) == (infinity | quiet)) : actual == expected<Float>(input, op);
    }

    struct integer_result
    {
        std::uint64_t value;
        bool valid;
    };

    // Decode a binary rational with integer shifts. This oracle does not perform
    // any host float-to-integer conversion (including for trap/range decisions).
    template <class UInt>
    integer_result to_integer(UInt raw, unsigned width, bool sign_result)
    {
        constexpr unsigned frac = sizeof(UInt) == 4 ? 23 : 52;
        constexpr unsigned bias = sizeof(UInt) == 4 ? 127 : 1023;
        constexpr UInt exponent_mask = sizeof(UInt) == 4 ? 255 : 2047;
        bool negative = (raw >> (sizeof(UInt) * 8 - 1)) != 0;
        unsigned encoded = static_cast<unsigned>((raw >> frac) & exponent_mask);
        std::uint64_t fraction = raw & ((UInt{1} << frac) - 1);
        std::uint64_t limit = std::uint64_t{1} << (width - 1);
        std::uint64_t maximum = sign_result ? limit - 1 : width == 64 ? ~std::uint64_t{} : 0xffffffffu;
        if(encoded == exponent_mask && fraction != 0) { return {0, false}; }
        int exponent = static_cast<int>(encoded) - static_cast<int>(bias);
        if(exponent < 0) { return {0, true}; }
        if(exponent >= static_cast<int>(width)) { return {negative ? (sign_result ? limit : 0) : maximum, false}; }
        std::uint64_t significand = (std::uint64_t{1} << frac) | fraction;
        std::uint64_t magnitude = exponent < static_cast<int>(frac) ? significand >> (frac - exponent) : significand << (exponent - frac);
        if(negative)
        {
            if(!sign_result || magnitude > limit) { return {sign_result ? limit : 0, false}; }
            auto result = std::uint64_t{} - magnitude;
            return {width == 32 ? result & 0xffffffffu : result, true};
        }
        if(magnitude > maximum) { return {maximum, false}; }
        return {magnitude, true};
    }
}  // namespace fp_rounding_oracle
