#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include "memory_validation.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    enum class scalar_memory_value_kind : unsigned { i32, i64, f32, f64 };

    // Compiler DATA only. Neither a constructed event nor its copied offsets
    // authorize execution, native pointer access or a retained-plan publication.
    // The caller supplies this after the original typed transition succeeds;
    // a retained plan still needs its owning complete-function sealing proof.
    struct validated_scalar_memory_event
    {
        unsigned opcode{};
        ::std::uint_least32_t memory_index{}, alignment{};
        ::std::uint_least64_t offset{};
        storage_address_type address_type{};
        scalar_memory_value_kind value_kind{};
        ::std::uint_least8_t access_bytes{}, value_bytes{}, popped{}, pushed{};
        ::std::uint_least8_t pop_bytes{}, push_bytes{};
        bool store{}, signed_load{}, reachable{};
        ::std::size_t source_offset{}, source_bytes{}, control_depth{};
    };

    struct scalar_memory_descriptor
    {
        scalar_memory_value_kind value_kind{};
        ::std::uint_least8_t access_bytes{}, value_bytes{}, max_alignment{};
        bool store{}, signed_load{};
    };

    // Exactly Core scalar load/store primary opcodes 0x28..0x3e. No raw byte
    // cursor is accepted by this description or by the event carrier.
    [[nodiscard]] inline constexpr bool describe_scalar_memory_operation(
        unsigned opcode, scalar_memory_descriptor& out) noexcept
    {
        using value = scalar_memory_value_kind;
        switch(opcode)
        {
            case 0x28u: out = {value::i32, 4u, 4u, 2u, false, false}; break;
            case 0x29u: out = {value::i64, 8u, 8u, 3u, false, false}; break;
            case 0x2au: out = {value::f32, 4u, 4u, 2u, false, false}; break;
            case 0x2bu: out = {value::f64, 8u, 8u, 3u, false, false}; break;
            case 0x2cu: out = {value::i32, 1u, 4u, 0u, false, true}; break;
            case 0x2du: out = {value::i32, 1u, 4u, 0u, false, false}; break;
            case 0x2eu: out = {value::i32, 2u, 4u, 1u, false, true}; break;
            case 0x2fu: out = {value::i32, 2u, 4u, 1u, false, false}; break;
            case 0x30u: out = {value::i64, 1u, 8u, 0u, false, true}; break;
            case 0x31u: out = {value::i64, 1u, 8u, 0u, false, false}; break;
            case 0x32u: out = {value::i64, 2u, 8u, 1u, false, true}; break;
            case 0x33u: out = {value::i64, 2u, 8u, 1u, false, false}; break;
            case 0x34u: out = {value::i64, 4u, 8u, 2u, false, true}; break;
            case 0x35u: out = {value::i64, 4u, 8u, 2u, false, false}; break;
            case 0x36u: out = {value::i32, 4u, 4u, 2u, true, false}; break;
            case 0x37u: out = {value::i64, 8u, 8u, 3u, true, false}; break;
            case 0x38u: out = {value::f32, 4u, 4u, 2u, true, false}; break;
            case 0x39u: out = {value::f64, 8u, 8u, 3u, true, false}; break;
            case 0x3au: out = {value::i32, 1u, 4u, 0u, true, false}; break;
            case 0x3bu: out = {value::i32, 2u, 4u, 1u, true, false}; break;
            case 0x3cu: out = {value::i64, 1u, 8u, 0u, true, false}; break;
            case 0x3du: out = {value::i64, 2u, 8u, 1u, true, false}; break;
            case 0x3eu: out = {value::i64, 4u, 8u, 2u, true, false}; break;
            default: return false;
        }
        return true;
    }

    // No parsing and no operand validation occurs here. The original typed
    // decoder has already admitted memarg, selected declaration and operands.
    // The exact byte effects use the address AND value widths: i64-address
    // i32.store consumes 12 bytes, never two copies of the top i32 width.
    [[nodiscard]] inline constexpr bool complete_scalar_memory_event(
        validated_scalar_memory_event& out, unsigned opcode, typed_memory_argument const& checked,
        ::std::size_t source_offset, ::std::size_t source_bytes, ::std::size_t control_depth,
        bool reachable) noexcept
    {
        scalar_memory_descriptor descriptor{};
        if(!describe_scalar_memory_operation(opcode, descriptor) || source_bytes == 0uz ||
           checked.immediate.alignment > descriptor.max_alignment ||
           (checked.address_type != storage_address_type::i32 && checked.address_type != storage_address_type::i64) ||
           (checked.address_type == storage_address_type::i32 && checked.immediate.offset > 0xffff'ffffull)) { return false; }
        auto const address_bytes{checked.address_type == storage_address_type::i64 ? 8u : 4u};
        out = {.opcode = opcode, .memory_index = checked.immediate.memory_index,
            .alignment = checked.immediate.alignment, .offset = checked.immediate.offset,
            .address_type = checked.address_type, .value_kind = descriptor.value_kind,
            .access_bytes = descriptor.access_bytes, .value_bytes = descriptor.value_bytes,
            .popped = static_cast<::std::uint_least8_t>(descriptor.store ? 2u : 1u),
            .pushed = static_cast<::std::uint_least8_t>(descriptor.store ? 0u : 1u),
            .pop_bytes = static_cast<::std::uint_least8_t>(address_bytes + (descriptor.store ? descriptor.value_bytes : 0u)),
            .push_bytes = static_cast<::std::uint_least8_t>(descriptor.store ? 0u : descriptor.value_bytes),
            .store = descriptor.store, .signed_load = descriptor.signed_load, .reachable = reachable,
            .source_offset = source_offset, .source_bytes = source_bytes, .control_depth = control_depth};
        return true;
    }
}
