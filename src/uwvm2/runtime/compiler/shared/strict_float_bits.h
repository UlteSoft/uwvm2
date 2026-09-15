/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once

#include "strict_float.h"
#include <cmath>

namespace uwvm2::runtime::compiler::shared::strict_float_jit
{
    namespace fp = strict_float;

    // i386's ordinary and fastcall FP result ABIs use ST0 even when SSE2 is enabled. Generated
    // Wasm uses the no-x87 bit-preserving ABI; every operation that could require a native FP
    // result libcall must therefore go through this integer-only bridge instead.
#if defined(__i386__) || defined(_M_IX86)
    inline constexpr bool needs_bit_preserving_abi{true};
#else
    inline constexpr bool needs_bit_preserving_abi{false};
#endif
    inline constexpr bool needs_lowering{fp::needs_extended_rounding || needs_bit_preserving_abi};

    template <typename Float>
    [[nodiscard]] inline fp::u64 evaluate(fp::u64 left, fp::u64 right, unsigned opcode) noexcept
    {
        using bits = fp::bits_t<Float>;
        Float const lhs{::std::bit_cast<Float>(static_cast<bits>(left))};
        Float const rhs{::std::bit_cast<Float>(static_cast<bits>(right))};
        if((opcode & 15u) == 13u || (opcode & 15u) == 14u)
        {
            bool const signed_result{(opcode & 15u) == 13u};
            if((opcode & 32u) != 0u)
            {
                unsigned const width{(opcode & 64u) != 0u ? 64u : 32u};
                if(::std::isnan(lhs)) { return 0u; }
                Float const limit{::std::ldexp(Float{1}, static_cast<int>(width - (signed_result ? 1u : 0u)))};
                if(lhs >= limit) { return signed_result ? ((fp::u64{1u} << (width - 1u)) - 1u) : (width == 64u ? ~fp::u64{} : 0xffffffffu); }
                if(lhs <= (signed_result ? -limit : Float{})) { return signed_result ? (fp::u64{1u} << (width - 1u)) : 0u; }
            }
            // Non-saturating Wasm conversions have already passed the generated
            // finite/range trap checks before reaching this integer-return bridge.
            return signed_result ? ::std::bit_cast<fp::u64>(static_cast<::std::int64_t>(lhs)) : static_cast<fp::u64>(lhs);
        }
        Float result{};
        switch(opcode)
        {
            case 0: result = fp::binary<fp::operation::add>(lhs, rhs); break;
            case 1: result = fp::binary<fp::operation::sub>(lhs, rhs); break;
            case 2: result = fp::binary<fp::operation::mul>(lhs, rhs); break;
            case 3: result = fp::binary<fp::operation::div>(lhs, rhs); break;
            case 4:
                if constexpr(fp::needs_extended_rounding) { result = fp::square_root(lhs); }
                else { result = ::std::sqrt(lhs); }
                break;
            case 5: result = fp::convert<Float>(::std::bit_cast<::std::int64_t>(left)); break;
            case 6: result = fp::convert<Float>(left); break;
            case 7: result = fp::convert<Float>(::std::bit_cast<double>(left)); break;
            case 8: result = static_cast<Float>(::std::bit_cast<float>(static_cast<fp::u32>(left))); break;
            case 9: result = ::std::ceil(lhs); break;
            case 10: result = ::std::floor(lhs); break;
            case 11: result = ::std::trunc(lhs); break;
            // The public Wasm scope establishes RN-even and callbacks restore it.
            case 12: result = ::std::nearbyint(lhs); break;
        }
        return ::std::bit_cast<bits>(result);
    }

    // Integer-only C ABI: x87/68881 parameter and return registers must not introduce a second rounding.
    // The symbol is versioned and rebound even on an object-cache hit; no process address is persisted in IR.
    [[nodiscard]] inline fp::u64 bridge(fp::u64 left, fp::u64 right, fp::u32 opcode) noexcept
    { return (opcode & 16u) != 0u ? evaluate<double>(left, right, opcode & ~16u) : evaluate<float>(left, right, opcode); }
}
