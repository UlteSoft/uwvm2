/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once

#include <cstddef>
#include <cstdint>

namespace uwvm2::runtime::lib::details
{
    // The runtime uses strides 4, 8 and 16. Sample the high byte of a Weyl
    // sequence: sampling only the low bits of ++state can permanently miss
    // both functions in a repeated two-function call chain whose phases and
    // call positions have opposite parity. The odd step walks all 32-bit
    // states, while carries mix the high byte even for periodic call chains.
    // This keeps the existing single thread-owned state read/write; no atomic,
    // lock, extra counter allocation, or guest-memory check is introduced.
    [[nodiscard]] inline constexpr bool tiered_entry_sample_advance(::std::uint_least32_t& state,
                                                                    ::std::size_t local_function_index,
                                                                    ::std::uint_least32_t stride) noexcept
    {
        if(stride <= 1u) { return true; }
        // The caller owns this scalar TLS state for the whole operation. There
        // is no borrowed pointer into guest, function, or compilation storage.
        state += 0x9e3779b9u;
        auto const phase{static_cast<::std::uint_least32_t>(local_function_index * 0x9e3779b1uz)};
        return (((state >> 24u) ^ phase) & (stride - 1u)) == 0u;
    }
}
