/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <bit>
#include <cstdint>
#include <type_traits>

namespace uwvm2::runtime::compiler::shared::strict_float
{
    // A binary64 value evaluated in a 64-bit-significand x87/68881 register can double-round. SSE2 and native
    // IEEE binary32/64 targets keep their existing instructions: this is a compile-time target property.
#if (defined(__GNUC__) || defined(__clang__)) && \
    (((defined(__i386__) || defined(__x86_64__)) && !defined(__arm64ec__) && !defined(_M_ARM64EC) && !defined(_SOFT_FLOAT) && \
      (!defined(__SSE2_MATH__) || (defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__ != 0))) || \
     (defined(__m68k__) && defined(__HAVE_68881__)))
# define UWVM2_STRICT_FLOAT_EXTENDED 1
#else
# define UWVM2_STRICT_FLOAT_EXTENDED 0
#endif
    inline constexpr bool needs_extended_rounding{UWVM2_STRICT_FLOAT_EXTENDED != 0};

    // Disabling SSE on x86-64 does not change its C++ FP return convention.
    // Float-valued helper calls (even unused template fallthroughs at -O0)
    // therefore cannot implement this target. Keep operands/results in integer
    // bits and use the same explicitly rounded x87 memory operations instead.
#if defined(__x86_64__) && !defined(__SSE__) && !defined(__arm64ec__) && !defined(_M_ARM64EC)
    inline constexpr bool needs_integer_abi{true};
#else
    inline constexpr bool needs_integer_abi{false};
#endif

    // Separate three obligations: the runtime guard establishes FP controls, this
    // file implements arithmetic rounding/NaNs, and byte/integer transport preserves
    // non-arithmetic payloads. None replaces the others. In particular these
    // Float-valued arithmetic APIs are not raw sNaN transport APIs.
    // See documents/runtime/floating-point-change-rationale.md.
    // These ABIs use the pre-IEEE-754-2008, inverted signaling bit. Wasm always
    // uses the 2008 encoding, including on an older host or with software FP.
#if defined(__hppa__) || defined(__hppa) || defined(__sh__) || (defined(__mips__) && !defined(__mips_nan2008))
# define UWVM2_STRICT_FLOAT_LEGACY_NAN 1
#else
# define UWVM2_STRICT_FLOAT_LEGACY_NAN 0
#endif
    // 68881 and SPARC also produce an all-ones default NaN for invalid
    // operations; Wasm requires a canonical NaN when no noncanonical input exists.
#if UWVM2_STRICT_FLOAT_LEGACY_NAN || defined(__m68k__) || defined(__sparc__) || defined(__sparc)
    inline constexpr bool needs_nan_canonicalization{true};
#else
    inline constexpr bool needs_nan_canonicalization{false};
#endif

    enum class operation { add, sub, mul, div, sqrt };
    using u64 = ::std::uint64_t;
    using u32 = ::std::uint32_t;

    template <typename Float> using bits_t = ::std::conditional_t<sizeof(Float) == 4, u32, u64>;

    template <typename Float>
    [[nodiscard]] inline constexpr Float canonical_nan() noexcept
    {
        return ::std::bit_cast<Float>(static_cast<bits_t<Float>>(sizeof(Float) == 4 ? 0x7fc00000ull : 0x7ff8000000000000ull));
    }

    template <typename Float>
    [[nodiscard]] inline constexpr Float quiet_arithmetic_nan(Float value) noexcept
    {
        using bits = bits_t<Float>;
        constexpr bits signless{sizeof(Float) == 4 ? bits{0x7fffffffu} : static_cast<bits>(0x7fffffffffffffffull)};
        constexpr bits infinity{sizeof(Float) == 4 ? bits{0x7f800000u} : static_cast<bits>(0x7ff0000000000000ull)};
        constexpr bits quiet{bits{1} << (sizeof(Float) == 4 ? 22u : 51u)};
        auto raw{::std::bit_cast<bits>(value)};
        if((raw & signless) > infinity)
        {
            // Legacy hardware can change a canonical input's payload. Merely
            // setting the quiet bit again would not satisfy Wasm's canonical-NaN rule.
            if constexpr(needs_nan_canonicalization) { raw = infinity | quiet; }
            else { raw |= quiet; }
        }
        return ::std::bit_cast<Float>(raw);
    }

    template <typename Float>
    [[nodiscard]] inline constexpr Float canonicalize_native_nan(Float value) noexcept
    {
        if constexpr(needs_nan_canonicalization) { return quiet_arithmetic_nan(value); }
        else { return value; }
    }

    // GCC 15's SSE2 ceil/floor/trunc fallback (without SSE4.1 ROUND*) may return
    // an sNaN unchanged. This is distinct from GCC -O0 ABI transport quieting:
    // arithmetic rounding MUST quiet NaNs, while transport MUST NOT change bits.
    // Integer rounding handles the fallback directly, avoiding a libm call plus
    // repair; targets with suitable native rounding retain their existing path.
    enum class integral_rounding { ceil, floor, trunc, nearest };

#if ((defined(__i386__) || defined(__x86_64__)) && defined(__SSE2__) && !defined(__SSE4_1__)) || \
    (defined(__x86_64__) && !defined(__SSE__) && !defined(__arm64ec__) && !defined(_M_ARM64EC)) || UWVM2_STRICT_FLOAT_LEGACY_NAN
    inline constexpr bool uses_integer_rounding{true};
#else
    inline constexpr bool uses_integer_rounding{false};
#endif

    // Without a native IEEE rounding instruction, work directly on the binary
    // representation. This also handles signed zero and arithmetic NaNs without
    // a libm call or an extra floating-point classification/quieting operation.
    template <integral_rounding Mode, typename UInt>
    [[nodiscard]] inline constexpr UInt round_integral_bits(UInt raw) noexcept
    {
        static_assert(::std::is_unsigned_v<UInt> && (sizeof(UInt) == 4 || sizeof(UInt) == 8));
        constexpr unsigned fraction{sizeof(UInt) == 4 ? 23u : 52u};
        constexpr unsigned bias{sizeof(UInt) == 4 ? 127u : 1023u};
        constexpr UInt sign{UInt{1} << (sizeof(UInt) * 8u - 1u)};
        constexpr UInt one{UInt{bias} << fraction};
        constexpr UInt infinity{static_cast<UInt>(sizeof(UInt) == 4 ? 0x7f800000ull : 0x7ff0000000000000ull)};
        UInt const magnitude{raw & ~sign};
        if(magnitude >= (UInt{bias + fraction} << fraction))
        {
            if(magnitude > infinity) { raw |= UInt{1} << (fraction - 1u); }
            return raw;
        }
        if(magnitude < one)
        {
            if(magnitude == 0u) { return raw; }
            UInt const zero{raw & sign};
            if constexpr(Mode == integral_rounding::ceil) { return zero != 0u ? zero : one; }
            else if constexpr(Mode == integral_rounding::floor) { return zero != 0u ? (zero | one) : zero; }
            else if constexpr(Mode == integral_rounding::trunc) { return zero; }
            else { return magnitude > (UInt{bias - 1u} << fraction) ? (zero | one) : zero; }
        }
        // Earlier branches handled zero, magnitudes below one, already-integral
        // large values, infinities and NaNs. Here shift is in [1, fraction], so
        // all integer shifts are defined. Remainder and retained parity implement
        // ties-to-even without depending on the host rounding mode or FP flags.
        unsigned const shift{bias + fraction - static_cast<unsigned>(magnitude >> fraction)};
        UInt const step{UInt{1} << shift};
        UInt const mask{step - 1u}, remainder{raw & mask};
        UInt result{raw & ~mask};
        if constexpr(Mode == integral_rounding::ceil) { if((raw & sign) == 0u && remainder != 0u) { result += step; } }
        else if constexpr(Mode == integral_rounding::floor) { if((raw & sign) != 0u && remainder != 0u) { result += step; } }
        else if constexpr(Mode == integral_rounding::nearest)
        {
            UInt const halfway{step >> 1u};
            if(remainder > halfway || (remainder == halfway && (result & step) != 0u)) { result += step; }
        }
        return result;
    }

    struct extended_value
    {
        u64 significand;
        unsigned sign_exponent;
    };

    [[nodiscard]] inline constexpr u64 round_shift_even(u64 value, unsigned shift) noexcept
    {
        if(shift > 64u) { return 0u; }
        if(shift == 64u) { return value > (u64{1} << 63u); }
        if(shift == 0u) { return value; }
        u64 const truncated{value >> shift};
        u64 const remainder{value & ((u64{1} << shift) - 1u)};
        u64 const halfway{u64{1} << (shift - 1u)};
        return truncated + (remainder > halfway || (remainder == halfway && (truncated & 1u) != 0u));
    }

    template <typename Float>
    [[nodiscard]] inline constexpr bits_t<Float> round_extended(extended_value value) noexcept
    {
        static_assert(sizeof(Float) == 4 || sizeof(Float) == 8);
        constexpr unsigned precision{sizeof(Float) == 4 ? 24u : 53u};
        constexpr unsigned max_exponent{sizeof(Float) == 4 ? 255u : 2047u};
        constexpr int bias{sizeof(Float) == 4 ? 127 : 1023};
        u64 const sign{static_cast<u64>(value.sign_exponent >> 15u) << (sizeof(Float) * 8u - 1u)};
        unsigned const exponent{value.sign_exponent & 0x7fffu};
        if(exponent == 0x7fffu)
        {
            u64 const payload{(value.significand & ~(u64{1} << 63u)) == 0u ? 0u : u64{1} << (precision - 2u)};
            return static_cast<bits_t<Float>>(sign | (static_cast<u64>(max_exponent) << (precision - 1u)) | payload);
        }
        if(value.significand == 0u) { return static_cast<bits_t<Float>>(sign); }
        int result_exponent{static_cast<int>(exponent) - 16383 + bias};
        if(result_exponent <= 0)
        {
            return static_cast<bits_t<Float>>(sign | round_shift_even(value.significand,
                64u - precision + static_cast<unsigned>(1 - result_exponent)));
        }
        u64 significand{round_shift_even(value.significand, 64u - precision)};
        if(significand == (u64{1} << precision)) { significand >>= 1u; ++result_exponent; }
        if(result_exponent >= static_cast<int>(max_exponent))
        { return static_cast<bits_t<Float>>(sign | (static_cast<u64>(max_exponent) << (precision - 1u))); }
        return static_cast<bits_t<Float>>(sign | (static_cast<u64>(result_exponent) << (precision - 1u)) |
                                          (significand & ((u64{1} << (precision - 1u)) - 1u)));
    }

    // Round the extended operation toward zero, then jam its inexact bit into the significand's low bit.
    // Round-to-odd followed by RN-even gives one correctly rounded binary32/64 result, including subnormals.
    // All finite binary32/64 add/mul/div/sqrt results fit the extended exponent range, so no earlier underflow
    // loses this sticky information. FP controls are restored before returning; Wasm exception flags are unobservable.
    // See Boldo/Melquiond, "When double rounding is odd". No long-double C++ layout or ambient precision assumption.
    template <operation Op, typename Float>
    [[nodiscard]] inline extended_value evaluate_extended([[maybe_unused]] Float lhs, [[maybe_unused]] Float rhs = {}) noexcept
    {
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__)) && !defined(__arm64ec__) && !defined(_M_ARM64EC)
        struct raw_extended { unsigned char bytes[10]; } raw;
        unsigned short saved_control, status;
        unsigned short const truncate_extended{0x0f7fu};
# define UWVM2_STRICT_X87(Load, Opcode) \
        __asm__ volatile("fnstcw %[saved]\n\tfnclex\n\tfldcw %[control]\n\t" Load " %[left]\n\t" Opcode \
                         "\n\tfstpt %[result]\n\tfnstsw %[status]\n\tfnclex\n\tfldcw %[saved]" \
                         : [saved] "=&m"(saved_control), [result] "=m"(raw), [status] "=&a"(status) \
                         : [control] "m"(truncate_extended), [left] "m"(lhs), [right] "m"(rhs) : "st", "memory")
        if constexpr(sizeof(Float) == 4)
        {
            if constexpr(Op == operation::add) { UWVM2_STRICT_X87("flds", "fadds %[right]"); }
            else if constexpr(Op == operation::sub) { UWVM2_STRICT_X87("flds", "fsubs %[right]"); }
            else if constexpr(Op == operation::mul) { UWVM2_STRICT_X87("flds", "fmuls %[right]"); }
            else if constexpr(Op == operation::div) { UWVM2_STRICT_X87("flds", "fdivs %[right]"); }
            else { UWVM2_STRICT_X87("flds", "fsqrt"); }
        }
        else
        {
            if constexpr(Op == operation::add) { UWVM2_STRICT_X87("fldl", "faddl %[right]"); }
            else if constexpr(Op == operation::sub) { UWVM2_STRICT_X87("fldl", "fsubl %[right]"); }
            else if constexpr(Op == operation::mul) { UWVM2_STRICT_X87("fldl", "fmull %[right]"); }
            else if constexpr(Op == operation::div) { UWVM2_STRICT_X87("fldl", "fdivl %[right]"); }
            else { UWVM2_STRICT_X87("fldl", "fsqrt"); }
        }
# undef UWVM2_STRICT_X87
        u64 significand{};
        for(unsigned i{}; i != 8u; ++i) { significand |= static_cast<u64>(raw.bytes[i]) << (i * 8u); }
        unsigned const sign_exponent{static_cast<unsigned>(raw.bytes[8]) | (static_cast<unsigned>(raw.bytes[9]) << 8u)};
        return {significand | ((status & 0x20u) != 0u), sign_exponent};
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__m68k__) && defined(__HAVE_68881__)
        struct raw_extended { unsigned char bytes[12]; } raw;
        unsigned saved_control, status;
        unsigned const truncate_extended{0x10u};
        unsigned const clear_status{};
# define UWVM2_STRICT_M68K(Load, Opcode) \
        __asm__ volatile("fmove.l %%fpcr,%[saved]\n\tfmove.l %[control],%%fpcr\n\tfmove.l %[clear],%%fpsr\n\t" \
                         Load " %[left],%%fp0\n\t" Opcode "\n\tfmove.x %%fp0,%[result]\n\tfmove.l %%fpsr,%[status]" \
                         "\n\tfmove.l %[clear],%%fpsr\n\tfmove.l %[saved],%%fpcr" \
                         : [saved] "=&dm"(saved_control), [result] "=m"(raw), [status] "=&dm"(status) \
                         : [control] "dm"(truncate_extended), [clear] "dm"(clear_status), [left] "m"(lhs), [right] "m"(rhs) \
                         : "fp0", "cc", "memory")
        if constexpr(sizeof(Float) == 4)
        {
            if constexpr(Op == operation::add) { UWVM2_STRICT_M68K("fmove.s", "fadd.s %[right],%%fp0"); }
            else if constexpr(Op == operation::sub) { UWVM2_STRICT_M68K("fmove.s", "fsub.s %[right],%%fp0"); }
            else if constexpr(Op == operation::mul) { UWVM2_STRICT_M68K("fmove.s", "fmul.s %[right],%%fp0"); }
            else if constexpr(Op == operation::div) { UWVM2_STRICT_M68K("fmove.s", "fdiv.s %[right],%%fp0"); }
            else { UWVM2_STRICT_M68K("fmove.s", "fsqrt.x %%fp0,%%fp0"); }
        }
        else
        {
            if constexpr(Op == operation::add) { UWVM2_STRICT_M68K("fmove.d", "fadd.d %[right],%%fp0"); }
            else if constexpr(Op == operation::sub) { UWVM2_STRICT_M68K("fmove.d", "fsub.d %[right],%%fp0"); }
            else if constexpr(Op == operation::mul) { UWVM2_STRICT_M68K("fmove.d", "fmul.d %[right],%%fp0"); }
            else if constexpr(Op == operation::div) { UWVM2_STRICT_M68K("fmove.d", "fdiv.d %[right],%%fp0"); }
            else { UWVM2_STRICT_M68K("fmove.d", "fsqrt.x %%fp0,%%fp0"); }
        }
# undef UWVM2_STRICT_M68K
        u64 significand{};
        for(unsigned i{4u}; i != 12u; ++i) { significand = (significand << 8u) | raw.bytes[i]; }
        unsigned const sign_exponent{(static_cast<unsigned>(raw.bytes[0]) << 8u) | raw.bytes[1]};
        return {significand | ((status & 8u) != 0u), sign_exponent};
#else
        static_assert(sizeof(Float) == 0, "extended arithmetic is only instantiated on supported x87/68881 targets");
        return {};
#endif
    }

    // Keep the uncommon control switch and integer packer out of every interpreter opfunc.
    template <operation Op, typename Float>
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((__noinline__))
#endif
    [[nodiscard]] inline Float slow_operation(Float lhs, Float rhs = {}) noexcept
    { return ::std::bit_cast<Float>(round_extended<Float>(evaluate_extended<Op>(lhs, rhs))); }

    // RN extended results can only double-round to binary64 when the intermediate is exactly a
    // binary64 midpoint (or in the destination subnormal range). Check that rare case before using
    // the stored hardware result. Binary32 has enough guard precision with either PC=53 or PC=64
    // for these binary32-input operations. Stores are explicit: C++ excess-precision temporaries
    // must not leak into the next Wasm instruction. A cheap control read also protects header-only
    // users and platforms whose FE_DFL_ENV selects a narrower precision.
    // The explicit fixed-environment contract may elide that read: it promises RN, masked exceptions,
    // and PC=53/64 on these targets. It never elides the binary64 midpoint/subnormal correctness test.
    // As with any FP API, operands must arrive intact: m68k FMOVE itself obeys FPCR precision.
    // The production byte-ABI entry establishes the Wasm environment before loading FP operands.
    template <operation Op, typename Float>
    [[nodiscard]] inline bool try_nearest_fast(Float const& lhs, Float const& rhs, Float& result) noexcept
    {
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__)) && !defined(__arm64ec__) && !defined(_M_ARM64EC)
# if !defined(UWVM_ASSUME_FIXED_WASM_FP_ENVIRONMENT)
        unsigned short control;
        __asm__ volatile("fnstcw %0" : "=m"(control) : : "memory");
        if((control & 0x0c3fu) != 0x003fu) { return false; }
        unsigned const precision{control & 0x0300u};
        if(precision != 0x0300u && precision != 0x0200u) { return false; }
# endif
        struct raw_extended { unsigned char bytes[10]; } raw;
# define UWVM2_NEAREST_X87(Load, Opcode, Store) \
        __asm__ volatile(Load " %[left]\n\t" Opcode "\n\t" Store \
                         : [result] "=m"(result), [extended] "=m"(raw) \
                         : [left] "m"(lhs), [right] "m"(rhs) : "st", "memory")
        if constexpr(sizeof(Float) == 4)
        {
            if constexpr(Op == operation::add) { UWVM2_NEAREST_X87("flds", "fadds %[right]", "fstps %[result]"); }
            else if constexpr(Op == operation::sub) { UWVM2_NEAREST_X87("flds", "fsubs %[right]", "fstps %[result]"); }
            else if constexpr(Op == operation::mul) { UWVM2_NEAREST_X87("flds", "fmuls %[right]", "fstps %[result]"); }
            else if constexpr(Op == operation::div) { UWVM2_NEAREST_X87("flds", "fdivs %[right]", "fstps %[result]"); }
            else { UWVM2_NEAREST_X87("flds", "fsqrt", "fstps %[result]"); }
            return true;
        }
        else
        {
            if constexpr(Op == operation::add) { UWVM2_NEAREST_X87("fldl", "faddl %[right]", "fstl %[result]\n\tfstpt %[extended]"); }
            else if constexpr(Op == operation::sub) { UWVM2_NEAREST_X87("fldl", "fsubl %[right]", "fstl %[result]\n\tfstpt %[extended]"); }
            else if constexpr(Op == operation::mul) { UWVM2_NEAREST_X87("fldl", "fmull %[right]", "fstl %[result]\n\tfstpt %[extended]"); }
            else if constexpr(Op == operation::div) { UWVM2_NEAREST_X87("fldl", "fdivl %[right]", "fstl %[result]\n\tfstpt %[extended]"); }
            else { UWVM2_NEAREST_X87("fldl", "fsqrt", "fstl %[result]\n\tfstpt %[extended]"); }
            unsigned const exponent{(static_cast<unsigned>(raw.bytes[8]) | (static_cast<unsigned>(raw.bytes[9]) << 8u)) & 0x7fffu};
            unsigned const low{(static_cast<unsigned>(raw.bytes[0]) | (static_cast<unsigned>(raw.bytes[1]) << 8u)) & 2047u};
            // The finite operands cannot underflow extended precision; exponent zero therefore means exact zero.
            return exponent == 0u || (exponent >= 15361u && exponent < 17407u && low != 1024u);
        }
# undef UWVM2_NEAREST_X87
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__m68k__) && defined(__HAVE_68881__)
# if !defined(UWVM_ASSUME_FIXED_WASM_FP_ENVIRONMENT)
        unsigned control;
        __asm__ volatile("fmove.l %%fpcr,%0" : "=dm"(control) : : "memory");
        if((control & 0xff30u) != 0u) { return false; }
        unsigned const precision{control & 0xc0u};
        if(precision != 0u && precision != 0x80u) { return false; }
# endif
        struct raw_extended { unsigned char bytes[12]; } raw;
# define UWVM2_NEAREST_M68K(Load, Opcode, Store) \
        __asm__ volatile(Load " %[left],%%fp0\n\t" Opcode "\n\t" Store \
                         : [result] "=m"(result), [extended] "=m"(raw) \
                         : [left] "m"(lhs), [right] "m"(rhs) : "fp0", "cc", "memory")
        if constexpr(sizeof(Float) == 4)
        {
            if constexpr(Op == operation::add) { UWVM2_NEAREST_M68K("fmove.s", "fadd.s %[right],%%fp0", "fmove.s %%fp0,%[result]"); }
            else if constexpr(Op == operation::sub) { UWVM2_NEAREST_M68K("fmove.s", "fsub.s %[right],%%fp0", "fmove.s %%fp0,%[result]"); }
            else if constexpr(Op == operation::mul) { UWVM2_NEAREST_M68K("fmove.s", "fmul.s %[right],%%fp0", "fmove.s %%fp0,%[result]"); }
            else if constexpr(Op == operation::div) { UWVM2_NEAREST_M68K("fmove.s", "fdiv.s %[right],%%fp0", "fmove.s %%fp0,%[result]"); }
            else { UWVM2_NEAREST_M68K("fmove.s", "fsqrt.x %%fp0,%%fp0", "fmove.s %%fp0,%[result]"); }
            return true;
        }
        else
        {
            if constexpr(Op == operation::add) { UWVM2_NEAREST_M68K("fmove.d", "fadd.d %[right],%%fp0", "fmove.d %%fp0,%[result]\n\tfmove.x %%fp0,%[extended]"); }
            else if constexpr(Op == operation::sub) { UWVM2_NEAREST_M68K("fmove.d", "fsub.d %[right],%%fp0", "fmove.d %%fp0,%[result]\n\tfmove.x %%fp0,%[extended]"); }
            else if constexpr(Op == operation::mul) { UWVM2_NEAREST_M68K("fmove.d", "fmul.d %[right],%%fp0", "fmove.d %%fp0,%[result]\n\tfmove.x %%fp0,%[extended]"); }
            else if constexpr(Op == operation::div) { UWVM2_NEAREST_M68K("fmove.d", "fdiv.d %[right],%%fp0", "fmove.d %%fp0,%[result]\n\tfmove.x %%fp0,%[extended]"); }
            else { UWVM2_NEAREST_M68K("fmove.d", "fsqrt.x %%fp0,%%fp0", "fmove.d %%fp0,%[result]\n\tfmove.x %%fp0,%[extended]"); }
            unsigned const exponent{((static_cast<unsigned>(raw.bytes[0]) << 8u) | raw.bytes[1]) & 0x7fffu};
            unsigned const low{((static_cast<unsigned>(raw.bytes[10]) << 8u) | raw.bytes[11]) & 2047u};
            return exponent == 0u || (exponent >= 15361u && exponent < 17407u && low != 1024u);
        }
# undef UWVM2_NEAREST_M68K
#else
        static_cast<void>(lhs); static_cast<void>(rhs); static_cast<void>(result);
        return false;
#endif
    }

    // Integer ABI adapter, not software arithmetic: try_nearest_fast and
    // evaluate_extended only use sizeof(T) and explicit memory operands. An
    // unsigned T supplies the same IEEE bytes without a Float return register.
    // Retain the fast RN path and the binary64 midpoint/subnormal repair; never
    // replace this with an ordinary x87 expression, which can double-round.
    template <operation Op, typename UInt>
    [[nodiscard]] inline UInt extended_operation_bits(UInt lhs, UInt rhs = {}) noexcept
    {
        static_assert(::std::is_unsigned_v<UInt> && (sizeof(UInt) == 4 || sizeof(UInt) == 8));
        UInt result;
        if(try_nearest_fast<Op>(lhs, rhs, result)) { return result; }
        using Float = ::std::conditional_t<sizeof(UInt) == 4, float, double>;
        return round_extended<Float>(evaluate_extended<Op>(lhs, rhs));
    }

    // Decode binary32/64 exactly into the packer's representation. No FP load
    // or return is needed, so width conversions work with -mno-sse at -O0 too.
    // Infinities remain infinities; every NaN is quieted by round_extended.
    template <typename UInt>
    [[nodiscard]] inline constexpr extended_value ieee_bits_to_extended(UInt raw) noexcept
    {
        static_assert(::std::is_unsigned_v<UInt> && (sizeof(UInt) == 4 || sizeof(UInt) == 8));
        constexpr unsigned fraction{sizeof(UInt) == 4 ? 23u : 52u};
        constexpr unsigned bias{sizeof(UInt) == 4 ? 127u : 1023u};
        constexpr unsigned max_exp{sizeof(UInt) == 4 ? 255u : 2047u};
        unsigned const sign{static_cast<unsigned>(raw >> (sizeof(UInt) * 8u - 1u)) << 15u};
        unsigned const exponent{static_cast<unsigned>((raw >> fraction) & max_exp)};
        u64 const payload{static_cast<u64>(raw) & ((u64{1} << fraction) - 1u)};
        if(exponent == max_exp) { return {(u64{1} << 63u) | (payload << (63u - fraction)), sign | 0x7fffu}; }
        if(exponent == 0u)
        {
            if(payload == 0u) { return {0u, sign}; }
            unsigned const shift{static_cast<unsigned>(::std::countl_zero(payload))};
            return {payload << shift, sign | (16383u + 64u - shift - bias - fraction)};
        }
        return {((u64{1} << fraction) | payload) << (63u - fraction), sign | (exponent + 16383u - bias)};
    }

    template <bool Minimum, typename UInt>
    [[nodiscard]] inline constexpr UInt minmax_bits(UInt lhs, UInt rhs) noexcept
    {
        constexpr UInt sign{UInt{1} << (sizeof(UInt) * 8u - 1u)};
        constexpr UInt infinity{static_cast<UInt>(sizeof(UInt) == 4 ? 0x7f800000ull : 0x7ff0000000000000ull)};
        constexpr UInt quiet{UInt{1} << (sizeof(UInt) == 4 ? 22u : 51u)};
        if((lhs & ~sign) > infinity || (rhs & ~sign) > infinity) { return infinity | quiet; }
        if(((lhs | rhs) & ~sign) == 0u) { return Minimum ? (lhs | rhs) : (lhs & rhs); }
        auto const lkey{(lhs & sign) != 0u ? ~lhs : (lhs ^ sign)};
        auto const rkey{(rhs & sign) != 0u ? ~rhs : (rhs ^ sign)};
        return (Minimum ? lkey < rkey : lkey > rkey) ? lhs : rhs;
    }

    template <operation Op, typename Float>
    [[nodiscard]] inline constexpr Float binary(Float lhs, Float rhs) noexcept
    {
#if defined(__clang__) && (defined(__arm__) || defined(_M_ARM))
        // The caller's FP pragma does not apply to arithmetic defined in this helper.
        // Retain scalar VFP semantics after inlining: ARM32 NEON flushes FP32 subnormals.
# pragma clang fp exceptions(strict)
#endif
        if constexpr(needs_extended_rounding)
        {
            if(!::std::is_constant_evaluated())
            {
                Float result;
                if(try_nearest_fast<Op>(lhs, rhs, result)) { return canonicalize_native_nan(result); }
                return slow_operation<Op>(lhs, rhs);
            }
        }
        if constexpr(Op == operation::add) { return canonicalize_native_nan(static_cast<Float>(lhs + rhs)); }
        else if constexpr(Op == operation::sub) { return canonicalize_native_nan(static_cast<Float>(lhs - rhs)); }
        else if constexpr(Op == operation::mul) { return canonicalize_native_nan(static_cast<Float>(lhs * rhs)); }
        else { return canonicalize_native_nan(static_cast<Float>(lhs / rhs)); }
    }

    template <typename Float>
    [[nodiscard]] inline Float square_root(Float value) noexcept
    {
        Float result;
        if(try_nearest_fast<operation::sqrt>(value, Float{}, result)) { return canonicalize_native_nan(result); }
        return slow_operation<operation::sqrt>(value);
    }

    template <typename Out, typename In>
    [[nodiscard]] inline constexpr Out convert(In value) noexcept
    {
        if constexpr(needs_nan_canonicalization && ::std::is_floating_point_v<In> && ::std::is_floating_point_v<Out>)
        {
            using bits = bits_t<In>;
            constexpr bits magnitude{static_cast<bits>(sizeof(In) == 4 ? 0x7fffffffull : 0x7fffffffffffffffull)};
            constexpr bits infinity{static_cast<bits>(sizeof(In) == 4 ? 0x7f800000ull : 0x7ff0000000000000ull)};
            // A legacy demotion can discard every payload bit and produce infinity.
            // Classify the input before the host conversion, not just its result.
            if((::std::bit_cast<bits>(value) & magnitude) > infinity) { return canonical_nan<Out>(); }
        }
#if defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__)
        if constexpr(::std::is_floating_point_v<In> && ::std::is_floating_point_v<Out> && sizeof(In) == 4 && sizeof(Out) == 8)
        {
            // Scalar PPC lfs/xscvspdpn promotion does not quiet a signaling NaN.
            // Bit classification survives optimization and leaves finite values on the native conversion path.
            if((::std::bit_cast<u32>(value) & 0x7fffffffu) > 0x7f800000u)
            { return ::std::bit_cast<Out>(u64{0x7ff8000000000000ull}); }
        }
#endif
        if constexpr(needs_extended_rounding)
        {
            if constexpr(::std::is_integral_v<In>)
            {
                bool const negative{::std::is_signed_v<In> && value < 0};
                u64 const magnitude{negative ? u64{} - static_cast<u64>(value) : static_cast<u64>(value)};
                if(magnitude == 0u) { return Out{}; }
                unsigned const leading{static_cast<unsigned>(::std::countl_zero(magnitude))};
                return ::std::bit_cast<Out>(round_extended<Out>({magnitude << leading,
                    (negative ? 0x8000u : 0u) | (16383u + 63u - leading)}));
            }
            else if constexpr(sizeof(In) == 8 && sizeof(Out) == 4)
            {
                u64 const bits{::std::bit_cast<u64>(value)};
                unsigned const exponent{static_cast<unsigned>((bits >> 52u) & 2047u)};
                u64 const fraction{bits & ((u64{1} << 52u) - 1u)};
                unsigned const sign{static_cast<unsigned>(bits >> 63u) << 15u};
                if(exponent == 2047u)
                { return ::std::bit_cast<Out>(round_extended<Out>({(u64{1} << 63u) | (fraction << 11u), sign | 0x7fffu})); }
                // Every binary64 subnormal is below half the smallest binary32 subnormal.
                if(exponent == 0u) { return ::std::bit_cast<Out>(static_cast<u32>(sign) << 16u); }
                return ::std::bit_cast<Out>(round_extended<Out>({((u64{1} << 52u) | fraction) << 11u,
                    sign | (exponent + 16383u - 1023u)}));
            }
        }
        return static_cast<Out>(value);
    }
}
