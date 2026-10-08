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
    enum class bulk_memory_instruction_kind : unsigned { init, data_drop, copy, fill };

    // Owned metadata copied from the FIRST bounded FC decoder and its checked
    // data/memory-index resolvers. These indices/types confer no runtime authority.
    struct decoded_bulk_memory_instruction
    {
        bulk_memory_instruction_kind kind{};
        ::std::uint_least32_t data_index{}, destination_memory_index{}, source_memory_index{};
        storage_address_type destination_address{}, source_address{};
    };

    [[nodiscard]] inline constexpr unsigned bulk_memory_operand_count(
        decoded_bulk_memory_instruction const& instruction) noexcept
    { return instruction.kind == bulk_memory_instruction_kind::data_drop ? 0u : 3u; }

    // Pop order is top-first. Init's data source/length stay i32. Copy's length
    // is the minimum address width, so only an i64/i64 pair consumes i64 length.
    [[nodiscard]] inline constexpr storage_address_type bulk_memory_expected_operand_address(
        decoded_bulk_memory_instruction const& instruction, unsigned top_ordinal) noexcept
    {
        if(top_ordinal == 2u) { return instruction.destination_address; }
        if(instruction.kind == bulk_memory_instruction_kind::copy)
        {
            if(top_ordinal == 1u) { return instruction.source_address; }
            return instruction.destination_address == storage_address_type::i64 &&
                instruction.source_address == storage_address_type::i64 ? storage_address_type::i64 : storage_address_type::i32;
        }
        if(instruction.kind == bulk_memory_instruction_kind::fill && top_ordinal == 0u)
        { return instruction.destination_address; }
        return storage_address_type::i32;
    }

    struct validated_bulk_memory_event
    {
        decoded_bulk_memory_instruction decoded{};
        ::std::uint_least8_t popped{}, pop_bytes{};
        bool reachable{};
        ::std::size_t source_offset{}, source_bytes{}, control_depth{};
    };

    [[nodiscard]] inline constexpr bool complete_bulk_memory_event(
        validated_bulk_memory_event& out, decoded_bulk_memory_instruction const& decoded,
        ::std::size_t source_offset, ::std::size_t source_bytes,
        ::std::size_t control_depth, bool reachable) noexcept
    {
        if(source_bytes == 0uz || static_cast<unsigned>(decoded.kind) > static_cast<unsigned>(bulk_memory_instruction_kind::fill))
        { return false; }
        if(decoded.kind != bulk_memory_instruction_kind::data_drop &&
           decoded.destination_address != storage_address_type::i32 && decoded.destination_address != storage_address_type::i64)
        { return false; }
        if(decoded.kind == bulk_memory_instruction_kind::copy &&
           decoded.source_address != storage_address_type::i32 && decoded.source_address != storage_address_type::i64)
        { return false; }
        auto const count{bulk_memory_operand_count(decoded)};
        unsigned bytes{};
        for(unsigned ordinal{}; ordinal != count; ++ordinal)
        { bytes += bulk_memory_expected_operand_address(decoded, ordinal) == storage_address_type::i64 ? 8u : 4u; }
        out = {.decoded = decoded, .popped = static_cast<::std::uint_least8_t>(count),
            .pop_bytes = static_cast<::std::uint_least8_t>(bytes), .reachable = reachable,
            .source_offset = source_offset, .source_bytes = source_bytes, .control_depth = control_depth};
        return true;
    }
}
