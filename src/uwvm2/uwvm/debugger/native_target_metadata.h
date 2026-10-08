/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::native_target_metadata
{
    // Capacity of one OWNED decoded instruction, not an ISA-length guess.
    // The actual engine's target triple supplies the ISA maximum for X86;
    // MCAsmInfo supplies alignment and other targets' decoder bounds.
    // A target exceeding this cold reply bound is explicitly unavailable.
    inline constexpr ::std::size_t max_instruction_bytes{32u};

    template<typename Array>
    [[nodiscard]] constexpr bool valid_string(Array const& bytes, ::std::size_t count) noexcept
    {
        if(count >= bytes.size() || bytes[count] != '\0') { return false; }
        for(::std::size_t index{}; index != count; ++index)
        {
            // [owned characters ... count][one terminator] array_end
            // [safe                                    ] count < capacity;
            //  ^^ scalar index is bounded before either byte or NUL test.
            if(bytes[index] == '\0') { return false; }
        }
        return true;
    }
    // Structural DATA validation only. Neither this public template nor an MC
    // decoder proves that a supplied description belongs to a current engine.
    // Runtime callers copy it inside their existing private code-owner query.
    template<typename Description>
    [[nodiscard]] constexpr bool valid(Description const& description) noexcept
    {
        auto const maximum{description.maximum_instruction_bytes};
        auto const alignment{description.minimum_instruction_alignment};
        return description.available && description.description_version == 1u && description.triple_size != 0u &&
               valid_string(description.triple, description.triple_size) &&
               valid_string(description.cpu, description.cpu_size) &&
               valid_string(description.features, description.features_size) &&
               (description.pointer_bits == 32u || description.pointer_bits == 64u) &&
               maximum != 0u && maximum <= max_instruction_bytes &&
               alignment != 0u && alignment <= maximum &&
               (alignment & (alignment - 1u)) == 0u;
    }
}
