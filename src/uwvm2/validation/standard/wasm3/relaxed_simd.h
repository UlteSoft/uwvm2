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
    // Core 3.0 binary/instructions and exec/numerics, relaxed operations. Every listed
    // instruction has no immediate and consumes only v128 operands. Unknown/reserved
    // encodings are deliberately not recognized. This helper never reads a byte cursor.
    [[nodiscard]] inline constexpr unsigned relaxed_simd_operand_count(::std::uint_least32_t opcode) noexcept
    {
        if(opcode < 0x100u || opcode > 0x113u) { return 0u; }
        if(opcode >= 0x101u && opcode <= 0x104u) { return 1u; }
        if((opcode >= 0x105u && opcode <= 0x10cu) || opcode == 0x113u) { return 3u; }
        return 2u;
    }

    // Select the strict projection for swizzle, conversions, min/max, q15 and bitselect.
    // Both backends share this normalization AFTER checking the original feature gate.
    // Madd/nmadd and signed saturating dot products have separate lowering paths.
    [[nodiscard]] inline constexpr ::std::uint_least32_t relaxed_simd_canonical_opcode(::std::uint_least32_t opcode) noexcept
    {
        switch(opcode)
        {
            case 0x100u: return 0x0eu;
            case 0x101u: return 0xf8u;
            case 0x102u: return 0xf9u;
            case 0x103u: return 0xfcu;
            case 0x104u: return 0xfdu;
            case 0x109u: case 0x10au: case 0x10bu: case 0x10cu: return 0x52u;
            case 0x10du: return 0xe8u;
            case 0x10eu: return 0xe9u;
            case 0x10fu: return 0xf4u;
            case 0x110u: return 0xf5u;
            case 0x111u: return 0x82u;
            default: return opcode;
        }
    }
}
