#include <uwvm2/runtime/compiler/uwvm_int/optable/register_ring.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

template <typename T>
[[gnu::always_inline]] inline void codegen_keep(T const& v) noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    using U = ::std::remove_cvref_t<T>;
    if constexpr(::std::is_integral_v<U> || ::std::is_pointer_v<U>) { asm volatile("" : : "r"(v)); }
    else
    {
        asm volatile("" : : "w"(v));
    }
#else
    (void)v;
#endif
}

// Intended for manual/CI codegen inspection under -O3.
//
// This probes an 8-slot i32 cache and selects the specialization using runtime `currpos` and `count`:
// - `currpos` in [3,11) (8 positions)
// - `count` in [1,8] (but only some pairs are valid; current implementation is contiguous descending, not wrap-around)
//
// Example:
// `clang++ --sysroot=$SYSROOT -O3 -S -std=c++26 -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -I third-parties/boost_unordered/include test/0013.uwvm_int/uwvm_int_register_ring_translate_codegen.cc -o /tmp/uwvm_int_register_ring_translate_codegen.s`
[[gnu::noinline]] auto codegen_select_spill_i32_8(::std::size_t currpos, ::std::size_t count) noexcept
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
                                                               .i32_stack_top_begin_pos = 3uz,
                                                               .i32_stack_top_end_pos = 11uz,
                                                               .i64_stack_top_begin_pos = SIZE_MAX,
                                                               .i64_stack_top_end_pos = SIZE_MAX,
                                                               .f32_stack_top_begin_pos = SIZE_MAX,
                                                               .f32_stack_top_end_pos = SIZE_MAX,
                                                               .f64_stack_top_begin_pos = SIZE_MAX,
                                                               .f64_stack_top_end_pos = SIZE_MAX,
                                                               .v128_stack_top_begin_pos = SIZE_MAX,
                                                               .v128_stack_top_end_pos = SIZE_MAX};

    using T0 = ::std::byte const*;
    using T1 = ::std::byte*;
    using T2 = ::std::byte*;

    using opfunc_t =
        optable::uwvm_interpreter_opfunc_t<T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>;

    optable::uwvm_interpreter_stacktop_currpos_t curr{};
    curr.i32_stack_top_curr_pos = currpos;
    optable::uwvm_interpreter_stacktop_remain_size_t remain{};
    remain.i32_stack_top_remain_size = count;

    opfunc_t f = optable::translate::get_uwvmint_stacktop_to_operand_stack_fptr<opt,
                                                                                wasm_i32,
                                                                                T0,
                                                                                T1,
                                                                                T2,
                                                                                wasm_i32,
                                                                                wasm_i32,
                                                                                wasm_i32,
                                                                                wasm_i32,
                                                                                wasm_i32,
                                                                                wasm_i32,
                                                                                wasm_i32,
                                                                                wasm_i32>(curr, remain);
    codegen_keep(f);
    return f;
}

[[gnu::noinline]] auto codegen_select_load_i32_8(::std::size_t currpos, ::std::size_t count) noexcept
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
                                                               .i32_stack_top_begin_pos = 3uz,
                                                               .i32_stack_top_end_pos = 11uz,
                                                               .i64_stack_top_begin_pos = SIZE_MAX,
                                                               .i64_stack_top_end_pos = SIZE_MAX,
                                                               .f32_stack_top_begin_pos = SIZE_MAX,
                                                               .f32_stack_top_end_pos = SIZE_MAX,
                                                               .f64_stack_top_begin_pos = SIZE_MAX,
                                                               .f64_stack_top_end_pos = SIZE_MAX,
                                                               .v128_stack_top_begin_pos = SIZE_MAX,
                                                               .v128_stack_top_end_pos = SIZE_MAX};

    using T0 = ::std::byte const*;
    using T1 = ::std::byte*;
    using T2 = ::std::byte*;

    using opfunc_t =
        optable::uwvm_interpreter_opfunc_t<T0, T1, T2, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32, wasm_i32>;

    optable::uwvm_interpreter_stacktop_currpos_t curr{};
    curr.i32_stack_top_curr_pos = currpos;
    optable::uwvm_interpreter_stacktop_remain_size_t remain{};
    remain.i32_stack_top_remain_size = count;

    opfunc_t f = optable::translate::get_uwvmint_operand_stack_to_stacktop_fptr<opt,
                                                                                wasm_i32,
                                                                                T0,
                                                                                T1,
                                                                                T2,
                                                                                wasm_i32,
                                                                                wasm_i32,
                                                                                wasm_i32,
                                                                                wasm_i32,
                                                                                wasm_i32,
                                                                                wasm_i32,
                                                                                wasm_i32,
                                                                                wasm_i32>(curr, remain);
    codegen_keep(f);
    return f;
}

int main() { return 0; }

