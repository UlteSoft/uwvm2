// Included in the existing compiler namespace after typed SIMD lowering.
// This is a semantic opcode-table adapter, never a raw byte scanner.
template<::uwvm2::runtime::compiler::shared::wasm1p1_simd_instruction_kind Kind,
         ::uwvm2::runtime::compiler::shared::wasm1p1_simd_scalar_kind ScalarKind,
         ::std::size_t LaneCount, ::std::uint_least32_t MaxAlign>
[[nodiscard]] inline constexpr ::uwvm2::validation::standard::wasm3::simd_event_descriptor
llvm_jit_simd_event_descriptor() noexcept
{
    namespace event = ::uwvm2::validation::standard::wasm3;
    namespace shared = ::uwvm2::runtime::compiler::shared;
    event::simd_event_descriptor descriptor{.lane_count = LaneCount, .max_alignment = MaxAlign};
    if constexpr(Kind == shared::wasm1p1_simd_instruction_kind::constant) { descriptor.kind = event::simd_event_kind::constant; }
    else if constexpr(Kind == shared::wasm1p1_simd_instruction_kind::shuffle) { descriptor.kind = event::simd_event_kind::shuffle; }
    else if constexpr(Kind == shared::wasm1p1_simd_instruction_kind::splat) { descriptor.kind = event::simd_event_kind::splat; }
    else if constexpr(Kind == shared::wasm1p1_simd_instruction_kind::extract_lane) { descriptor.kind = event::simd_event_kind::extract_lane; }
    else if constexpr(Kind == shared::wasm1p1_simd_instruction_kind::replace_lane) { descriptor.kind = event::simd_event_kind::replace_lane; }
    else if constexpr(Kind == shared::wasm1p1_simd_instruction_kind::unary) { descriptor.kind = event::simd_event_kind::unary; }
    else if constexpr(Kind == shared::wasm1p1_simd_instruction_kind::binary) { descriptor.kind = event::simd_event_kind::binary; }
    else if constexpr(Kind == shared::wasm1p1_simd_instruction_kind::ternary) { descriptor.kind = event::simd_event_kind::ternary; }
    else if constexpr(Kind == shared::wasm1p1_simd_instruction_kind::test) { descriptor.kind = event::simd_event_kind::test; }
    else if constexpr(Kind == shared::wasm1p1_simd_instruction_kind::shift) { descriptor.kind = event::simd_event_kind::shift; }
    else if constexpr(Kind == shared::wasm1p1_simd_instruction_kind::memory_load) { descriptor.kind = event::simd_event_kind::memory_load; }
    else if constexpr(Kind == shared::wasm1p1_simd_instruction_kind::memory_store) { descriptor.kind = event::simd_event_kind::memory_store; }
    if constexpr(ScalarKind == shared::wasm1p1_simd_scalar_kind::none) { descriptor.scalar_kind = event::simd_event_scalar_kind::none; }
    else if constexpr(ScalarKind == shared::wasm1p1_simd_scalar_kind::i32) { descriptor.scalar_kind = event::simd_event_scalar_kind::i32; }
    else if constexpr(ScalarKind == shared::wasm1p1_simd_scalar_kind::i64) { descriptor.scalar_kind = event::simd_event_scalar_kind::i64; }
    else if constexpr(ScalarKind == shared::wasm1p1_simd_scalar_kind::f32) { descriptor.scalar_kind = event::simd_event_scalar_kind::f32; }
    else if constexpr(ScalarKind == shared::wasm1p1_simd_scalar_kind::f64) { descriptor.scalar_kind = event::simd_event_scalar_kind::f64; }
    return descriptor;
}
