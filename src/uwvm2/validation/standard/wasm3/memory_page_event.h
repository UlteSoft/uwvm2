#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include "address_limits.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Compiler DATA after the selected memory declaration and typed transition
    // succeed. This value grants no execution, native memory or retained-plan
    // authority, and contains neither an expression cursor nor a borrowed pointer.
    struct validated_memory_page_event
    {
        unsigned opcode{};
        ::std::uint_least32_t memory_index{};
        storage_address_type address_type{};
        ::std::uint_least8_t popped{}, pushed{}, pop_bytes{}, push_bytes{};
        bool grow{}, reachable{};
        ::std::size_t source_offset{}, source_bytes{}, control_depth{};
    };

    // Core 3 memory.size : [] -> [at]; memory.grow : [at] -> [at],
    // where at is i32 or i64 from the already selected declaration.
    // No immediate parser or second semantic validator runs in this helper.
    [[nodiscard]] inline constexpr bool complete_memory_page_event(
        validated_memory_page_event& out, unsigned opcode,
        ::std::uint_least32_t memory_index, storage_address_type address_type,
        ::std::size_t source_offset, ::std::size_t source_bytes,
        ::std::size_t control_depth, bool reachable) noexcept
    {
        if((opcode != 0x3fu && opcode != 0x40u) || source_bytes == 0uz ||
           (address_type != storage_address_type::i32 && address_type != storage_address_type::i64)) { return false; }
        auto const grow{opcode == 0x40u};
        auto const bytes{static_cast<::std::uint_least8_t>(address_type == storage_address_type::i64 ? 8u : 4u)};
        out = {.opcode = opcode, .memory_index = memory_index, .address_type = address_type,
            .popped = static_cast<::std::uint_least8_t>(grow ? 1u : 0u), .pushed = 1u,
            .pop_bytes = static_cast<::std::uint_least8_t>(grow ? bytes : 0u), .push_bytes = bytes,
            .grow = grow, .reachable = reachable, .source_offset = source_offset,
            .source_bytes = source_bytes, .control_depth = control_depth};
        return true;
    }
}
