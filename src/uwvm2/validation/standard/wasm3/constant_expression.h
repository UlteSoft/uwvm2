/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#ifndef UWVM_MODULE
# include <cstdint>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Core 3.0, Validation / Constant Expressions and Execution / Numeric Instructions:
    // https://webassembly.github.io/spec/core/valid/instructions.html#constant-expressions
    // This leaf policy has no parser dependencies, so section parsers and initializers can
    // share it without introducing a parser/validator module dependency cycle.
    // Zero means that the opcode is not an extended integer constant instruction.
    [[nodiscard]] inline constexpr unsigned constant_integer_width(::std::uint_least8_t opcode) noexcept
    {
        switch(opcode)
        {
            case 0x6a: case 0x6b: case 0x6c: return 32;
            case 0x7c: case 0x7d: case 0x7e: return 64;
            default: return 0;
        }
    }

    // Precondition: constant_integer_width(opcode) != 0; checked before evaluation.
    // Unsigned arithmetic and an explicit width mask avoid host signed overflow and
    // preserve Wasm's modulo-2^N semantics, including hosts with wider least types.
    [[nodiscard]] inline constexpr ::std::uint_least64_t evaluate_constant_integer_binary(
        ::std::uint_least8_t opcode, ::std::uint_least64_t lhs, ::std::uint_least64_t rhs) noexcept
    {
        ::std::uint_least64_t result{};
        switch(opcode)
        {
            case 0x6a: case 0x7c: result = lhs + rhs; break;
            case 0x6b: case 0x7d: result = lhs - rhs; break;
            case 0x6c: case 0x7e: result = lhs * rhs; break;
            default: return 0;
        }
        return result & (constant_integer_width(opcode) == 32 ? 0xffffffffULL : 0xffffffffffffffffULL);
    }
}
