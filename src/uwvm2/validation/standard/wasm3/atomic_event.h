#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include "threads.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Original bounded FE decoder's owned result AFTER its complete typed stack
    // transition. Contains no raw expression pointer or native memory authority.
    struct validated_atomic_event
    {
        typed_atomic_instruction decoded{};
        ::std::uint_least8_t popped{}, pushed{}, pop_bytes{}, push_bytes{};
        bool reachable{};
        ::std::size_t source_offset{}, source_bytes{}, control_depth{};
    };

    [[nodiscard]] inline constexpr bool complete_atomic_event(
        validated_atomic_event& out, typed_atomic_instruction const& decoded,
        ::std::size_t source_offset, ::std::size_t source_bytes,
        ::std::size_t control_depth, bool reachable) noexcept
    {
        auto const& instruction{decoded.immediate};
        auto const& descriptor{instruction.descriptor};
        if(instruction.error != atomic_immediate_error::ok || source_bytes == 0uz ||
           descriptor.kind == atomic_instruction_kind::invalid || descriptor.operand_count > 3u) { return false; }
        unsigned bytes{};
        if(descriptor.kind != atomic_instruction_kind::fence)
        {
            if(decoded.address_type != storage_address_type::i32 && decoded.address_type != storage_address_type::i64) { return false; }
            bytes = decoded.address_type == storage_address_type::i64 ? 8u : 4u;
            bool const wait{descriptor.kind == atomic_instruction_kind::wait32 || descriptor.kind == atomic_instruction_kind::wait64};
            for(unsigned operand{1u}; operand < descriptor.operand_count; ++operand)
            { bytes += descriptor.value_i64 || (wait && operand == 1u) ? 8u : 4u; }
        }
        out = {.decoded = decoded, .popped = static_cast<::std::uint_least8_t>(descriptor.operand_count),
            .pushed = static_cast<::std::uint_least8_t>(descriptor.has_result ? 1u : 0u),
            .pop_bytes = static_cast<::std::uint_least8_t>(bytes),
            .push_bytes = static_cast<::std::uint_least8_t>(descriptor.has_result ? (descriptor.result_i64 ? 8u : 4u) : 0u),
            .reachable = reachable, .source_offset = source_offset, .source_bytes = source_bytes,
            .control_depth = control_depth};
        return true;
    }
}
