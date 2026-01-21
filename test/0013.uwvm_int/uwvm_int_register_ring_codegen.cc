#include <uwvm2/runtime/compiler/uwvm_int/optable/register_ring.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

template <typename T>
[[gnu::always_inline]] inline void codegen_keep(T const& v) noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    using U = ::std::remove_cvref_t<T>;
    if constexpr(::std::is_integral_v<U> || ::std::is_pointer_v<U>)
    {
        asm volatile("" : : "r"(v));
    }
    else
    {
        asm volatile("" : : "w"(v));
    }
#else
    (void)v;
#endif
}

// Intended for manual/CI codegen inspection under -O3:
// - Expect a single pointer adjust for `sp` and two stores for the two cached i32 slots.
//
// Example:
// `clang++ --sysroot=$SYSROOT -O3 -S -fuse-ld=lld -std=c++26 -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -I third-parties/boost_unordered/include test/0013.uwvm_int/uwvm_int_register_ring_codegen.cc -o /tmp/uwvm_int_register_ring_codegen.s`
[[gnu::noinline]] ::std::byte* codegen_spill_i32_2(::std::byte const* op,
                                                   ::std::byte* sp,
                                                   ::std::byte* local_base,
                                                   wasm_i32 r3,
                                                   wasm_i32 r4) noexcept
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{
        .is_tail_call = false,
        .i32_stack_top_begin_pos = 3uz,
        .i32_stack_top_end_pos = 5uz,
        .i64_stack_top_begin_pos = SIZE_MAX,
        .i64_stack_top_end_pos = SIZE_MAX,
        .f32_stack_top_begin_pos = SIZE_MAX,
        .f32_stack_top_end_pos = SIZE_MAX,
        .f64_stack_top_begin_pos = SIZE_MAX,
        .f64_stack_top_end_pos = SIZE_MAX,
        .v128_stack_top_begin_pos = SIZE_MAX,
        .v128_stack_top_end_pos = SIZE_MAX};

    optable::spill_stacktop_to_operand_stack<opt, 4uz, 2uz>(op, sp, local_base, r3, r4);
    codegen_keep(sp);
    return sp;
}

// i32/i64 merged ring spill, two cached slots:
// - Expect a single pointer adjust for `sp` (+12) and two stores (i32 then i64) from the cached slots.
[[gnu::noinline]] ::std::byte* codegen_spill_i32_i64_merge(::std::byte const* op,
                                                           ::std::byte* sp,
                                                           ::std::byte* local_base,
                                                           optable::wasm_stack_top_i32_with_i64_u s3,
                                                           optable::wasm_stack_top_i32_with_i64_u s4) noexcept
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{
        .is_tail_call = false,
        .i32_stack_top_begin_pos = 3uz,
        .i32_stack_top_end_pos = 5uz,
        .i64_stack_top_begin_pos = 3uz,
        .i64_stack_top_end_pos = 5uz,
        .f32_stack_top_begin_pos = SIZE_MAX,
        .f32_stack_top_end_pos = SIZE_MAX,
        .f64_stack_top_begin_pos = SIZE_MAX,
        .f64_stack_top_end_pos = SIZE_MAX,
        .v128_stack_top_begin_pos = SIZE_MAX,
        .v128_stack_top_end_pos = SIZE_MAX};

    optable::spill_stacktop_to_operand_stack<opt, 4uz, wasm_i64, wasm_i32>(op, sp, local_base, s3, s4);
    codegen_keep(sp);
    return sp;
}

int main() { return 0; }
