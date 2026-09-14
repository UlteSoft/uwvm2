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

    template <typename Float>
    [[nodiscard]] inline fp::u64 evaluate(fp::u64 left, fp::u64 right, unsigned opcode) noexcept
    {
        using bits = fp::bits_t<Float>;
        Float const lhs{::std::bit_cast<Float>(static_cast<bits>(left))};
        Float const rhs{::std::bit_cast<Float>(static_cast<bits>(right))};
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
        }
        return ::std::bit_cast<bits>(result);
    }

    // Integer-only C ABI: x87/68881 parameter and return registers must not introduce a second rounding.
    // The symbol is versioned and rebound even on an object-cache hit; no process address is persisted in IR.
    [[nodiscard]] inline fp::u64 bridge(fp::u64 left, fp::u64 right, fp::u32 opcode) noexcept
    { return (opcode & 16u) != 0u ? evaluate<double>(left, right, opcode & 15u) : evaluate<float>(left, right, opcode); }
}
