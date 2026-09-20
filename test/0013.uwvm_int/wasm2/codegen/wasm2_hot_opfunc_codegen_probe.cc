/*************************************************************
 * UWVM2 WebAssembly 2.0 interpreter code-generation probe. *
 * This translation unit is compiled to assembly only.       *
 *************************************************************/

#define UWVM_USE_UWVM_INT
#define UWVM_DISABLE_JIT

#include <cstddef>
#include <cstdint>

#include <uwvm2/runtime/compiler/uwvm_int/optable/impl.h>

namespace uwvm2test::wasm2_codegen
{
    namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

    using byte = ::std::byte;
    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm_funcref = ::uwvm2::object::global::wasm_funcref_t;
    using simd_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_simd;

    // No cached stack slots: this makes every probe use the common two-register
    // interpreter ABI (instruction pointer + operand-stack pointer), so the
    // resulting assembly is directly comparable across targets.
    inline constexpr optable::uwvm_interpreter_translate_option_t tail_option{.is_tail_call = true};

    extern "C" [[gnu::used, gnu::noinline]] void wasm2_codegen_ref_is_null(byte const* ip, byte* sp) noexcept
    {
        return optable::uwvmint_ref_is_null<tail_option, 0uz, wasm_funcref>(ip, sp);
    }

    extern "C" [[gnu::used, gnu::noinline]] void wasm2_codegen_i32_trunc_sat_f32_s(byte const* ip, byte* sp) noexcept
    {
        return optable::uwvmint_trunc_sat_typed<tail_option, wasm_f32, wasm_i32, true, 0uz>(ip, sp);
    }

    extern "C" [[gnu::used, gnu::noinline]] void wasm2_codegen_f32x4_add(byte const* ip, byte* sp) noexcept
    {
        return optable::uwvmint_simd_full_binop<tail_option, simd_code::f32x4_add>(ip, sp);
    }

    extern "C" [[gnu::used, gnu::noinline]] void wasm2_codegen_v128_const(byte const* ip, byte* sp) noexcept
    {
        return optable::uwvmint_v128_const<tail_option, 0uz>(ip, sp);
    }

    extern "C" [[gnu::used, gnu::noinline]] void wasm2_codegen_table_get_funcref(byte const* ip, byte* sp) noexcept
    {
        return optable::uwvmint_table_get_funcref<tail_option>(ip, sp);
    }

    extern "C" [[gnu::used, gnu::noinline]] void wasm2_codegen_memory_fill(byte const* ip, byte* sp) noexcept
    {
        return optable::uwvmint_memory_fill<tail_option>(ip, sp);
    }
}
