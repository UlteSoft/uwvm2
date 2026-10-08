#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <cstddef>
# include <cstdint>
# include "memory_validation.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    enum class simd_event_kind : unsigned
    { constant, shuffle, splat, extract_lane, replace_lane, unary, binary, ternary, test, shift, memory_load, memory_store };
    enum class simd_event_scalar_kind : unsigned { none, i32, i64, f32, f64 };
    enum class simd_event_value_kind : unsigned { i32, i64, f32, f64, v128 };
    struct simd_event_descriptor
    {
        simd_event_kind kind{};
        simd_event_scalar_kind scalar_kind{};
        ::std::size_t lane_count{};
        ::std::uint_least32_t max_alignment{};
        friend constexpr bool operator==(simd_event_descriptor const&, simd_event_descriptor const&) noexcept = default;
    };

    // Compiler DATA, never an instruction/source/native-access permission.
    // Immediate bytes are owned; no borrowed byte cursor survives the decoder.
    // Operand types/effects follow the already successful authoritative typed
    // transition. Execution/publication still requires the original owner.
    struct validated_simd_event
    {
        ::std::uint_least32_t opcode{};
        simd_event_descriptor descriptor{};
        typed_memory_argument memory{};
        ::std::uint_least32_t lane{};
        ::std::array<::std::byte,16uz> vector_bytes{};
        ::std::array<simd_event_value_kind,3uz> inputs{};
        ::std::array<simd_event_value_kind,1uz> outputs{};
        ::std::uint_least8_t popped{}, pushed{}, pop_bytes{}, push_bytes{}, access_bytes{};
        bool has_lane{}, has_vector{}, has_memory{}, reachable{};
        ::std::size_t source_offset{}, source_bytes{}, control_depth{};
    };

    [[nodiscard]] inline constexpr simd_event_value_kind simd_scalar_value_kind(simd_event_scalar_kind kind) noexcept
    {
        switch(kind)
        {
            case simd_event_scalar_kind::i32: return simd_event_value_kind::i32;
            case simd_event_scalar_kind::i64: return simd_event_value_kind::i64;
            case simd_event_scalar_kind::f32: return simd_event_value_kind::f32;
            case simd_event_scalar_kind::f64: return simd_event_value_kind::f64;
            default: return simd_event_value_kind::v128;
        }
    }
    [[nodiscard]] inline constexpr ::std::uint_least8_t simd_event_value_bytes(simd_event_value_kind kind) noexcept
    {
        switch(kind)
        {
            case simd_event_value_kind::i32: case simd_event_value_kind::f32: return 4u;
            case simd_event_value_kind::i64: case simd_event_value_kind::f64: return 8u;
            default: return 16u;
        }
    }

    // No byte parsing or operand validation here. This completes a copied DATA
    // event after the original SIMD visitor checked immediates and operands.
    // A failed internal description declines lowering, never module validation.
    [[nodiscard]] inline constexpr bool complete_simd_event(
        validated_simd_event& out, simd_event_descriptor descriptor,
        ::std::size_t source_offset, ::std::size_t source_bytes,
        ::std::size_t control_depth, bool reachable) noexcept
    {
        using kind = simd_event_kind;
        using value = simd_event_value_kind;
        bool const memory{descriptor.kind == kind::memory_load || descriptor.kind == kind::memory_store};
        bool const vector{descriptor.kind == kind::constant || descriptor.kind == kind::shuffle};
        bool const lane{descriptor.kind == kind::extract_lane || descriptor.kind == kind::replace_lane ||
                        (memory && descriptor.lane_count != 0uz)};
        if(source_bytes < 2uz || (lane && (descriptor.lane_count == 0uz || out.lane >= descriptor.lane_count))) { return false; }
        if(memory && (descriptor.max_alignment > 4u ||
            (lane && (descriptor.max_alignment == 4u || descriptor.lane_count != (16uz >> descriptor.max_alignment))) ||
            out.memory.immediate.alignment > descriptor.max_alignment ||
            (out.memory.address_type != storage_address_type::i32 && out.memory.address_type != storage_address_type::i64) ||
            (out.memory.address_type == storage_address_type::i32 && out.memory.immediate.offset > 0xffff'ffffull))) { return false; }
        bool const scalar{descriptor.kind == kind::splat || descriptor.kind == kind::extract_lane || descriptor.kind == kind::replace_lane};
        if(scalar && descriptor.scalar_kind == simd_event_scalar_kind::none) { return false; }
        out.descriptor = descriptor;
        out.has_lane = lane; out.has_vector = vector; out.has_memory = memory;
        out.source_offset = source_offset; out.source_bytes = source_bytes;
        out.control_depth = control_depth; out.reachable = reachable;
        out.inputs = {}; out.outputs = {}; out.popped = 0u; out.pushed = 1u;
        out.outputs[0] = value::v128; out.pop_bytes = 0u; out.push_bytes = 16u; out.access_bytes = 0u;
        auto const scalar_type{simd_scalar_value_kind(descriptor.scalar_kind)};
        switch(descriptor.kind)
        {
            case kind::constant: break;
            case kind::shuffle: case kind::binary: out.inputs[0] = value::v128; out.inputs[1] = value::v128; out.popped = 2u; break;
            case kind::unary: out.inputs[0] = value::v128; out.popped = 1u; break;
            case kind::ternary: out.inputs[0] = value::v128; out.inputs[1] = value::v128; out.inputs[2] = value::v128; out.popped = 3u; break;
            case kind::test: out.inputs[0] = value::v128; out.popped = 1u; out.outputs[0] = value::i32; out.push_bytes = 4u; break;
            case kind::shift: out.inputs[0] = value::v128; out.inputs[1] = value::i32; out.popped = 2u; break;
            case kind::splat: out.inputs[0] = scalar_type; out.popped = 1u; break;
            case kind::extract_lane: out.inputs[0] = value::v128; out.popped = 1u; out.outputs[0] = scalar_type; out.push_bytes = simd_event_value_bytes(scalar_type); break;
            case kind::replace_lane: out.inputs[0] = value::v128; out.inputs[1] = scalar_type; out.popped = 2u; break;
            case kind::memory_load: case kind::memory_store:
                out.inputs[0] = out.memory.address_type == storage_address_type::i64 ? value::i64 : value::i32;
                out.popped = 1u;
                if(lane || descriptor.kind == kind::memory_store) { out.inputs[1] = value::v128; out.popped = 2u; }
                out.access_bytes = static_cast<::std::uint_least8_t>(1u << descriptor.max_alignment);
                if(descriptor.kind == kind::memory_store) { out.pushed = 0u; out.push_bytes = 0u; }
                break;
            default: return false;
        }
        // [owned input tuple0..popped) | capacity3]; the switch above bounds popped.
        for(::std::size_t i{}; i != out.popped; ++i) { out.pop_bytes += simd_event_value_bytes(out.inputs[i]); }
        return true;
    }
}
