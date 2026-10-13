/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
namespace uwvm2::runtime::lib::details::native_loaded_provenance
{
    struct register_location { unsigned number{}; bool available{}; };
    // Exact register-only DWARF expression DATA. ARM D registers (256+) and
    // PPC VMX registers (1124+) require a multi-byte ULEB. No dereference,
    // frame-relative address, bit-piece or trailing operation is accepted.
    template<typename Bytes>
    [[nodiscard]] constexpr register_location exact_register_location(Bytes const& expression) noexcept
    {
        if(expression.size() == 1u && expression[0u] >= 0x50u && expression[0u] <= 0x6fu)
        { return {static_cast<unsigned>(expression[0u] - 0x50u),true}; }
        if(expression.size() < 2u || expression.size() > 6u || expression[0u] != 0x90u) { return {}; }
        unsigned value{};
        for(::std::size_t index{1u}; index != expression.size(); ++index)
        {
            auto const byte{static_cast<unsigned>(expression[index])};
            unsigned const shift{static_cast<unsigned>((index - 1u) * 7u)};
            auto const payload{byte & 0x7fu};
            if(shift >= ::std::numeric_limits<unsigned>::digits ||
               payload > (::std::numeric_limits<unsigned>::max)() >> shift) { return {}; }
            value |= payload << shift;
            if((byte & 0x80u) == 0u)
            {
                if(index + 1u != expression.size() || (index != 1u && payload == 0u)) { return {}; }
                return {value,true};
            }
        }
        return {};
    }
}
