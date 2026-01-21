#include <uwvm2/runtime/compiler/uwvm_int/optable/define.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace uwvm_int_optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

// This file is intended for manual codegen inspection under -O3, e.g.:
// `clang++ --sysroot=$SYSROOT -O3 -ffast-math -S test/0013.uwvm_int/codegen.cc -o /tmp/uwvm_int_codegen.s`
//
// Focus points:
// - get_curr_val_from_operand_stack_cache should compile to pointer adjust + load.
// - get_curr_val_from_operand_stack_top (stacktop) should compile to pure register moves.
// - get_vals_from_operand_stack should inline and avoid loops/branches in the hot path.

using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
using wasm_v128 = ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128;

struct codegen_i32_sp_t
{
    wasm_i32 v;
    ::std::byte* sp;
};

[[gnu::noinline]] wasm_i32 codegen_pop_i32_from_operand_stack(::std::byte const* op, ::std::byte* sp, ::std::byte* local_base) noexcept
{
    return uwvm_int_optable::get_curr_val_from_operand_stack_cache<wasm_i32>(op, sp, local_base);
}

[[gnu::noinline]] codegen_i32_sp_t codegen_pop_i32_from_operand_stack_keep_sp(::std::byte const* op, ::std::byte* sp, ::std::byte* local_base) noexcept
{
    wasm_i32 const v = uwvm_int_optable::get_curr_val_from_operand_stack_cache<wasm_i32>(op, sp, local_base);
    return codegen_i32_sp_t{.v = v, .sp = sp};
}

[[gnu::noinline]] wasm_i32 codegen_pop_i32_no_stacktop(::std::byte const* op, ::std::byte* sp, ::std::byte* local_base) noexcept
{
    constexpr uwvm_int_optable::uwvm_interpreter_translate_option_t opt{};
    return uwvm_int_optable::get_curr_val_from_operand_stack_top<opt, wasm_i32, 0uz>(op, sp, local_base);
}

// i32/i64 merge uses stacktop slots [3,5) -> indices 3 and 4.
// f32/v128 merge uses stacktop slots [5,7) -> indices 5 and 6.
[[gnu::noinline]] wasm_i32 codegen_mixed_pop(::std::byte const* op,
                                            ::std::byte* sp,
                                            ::std::byte* local_base,
                                            uwvm_int_optable::wasm_stack_top_i32_with_i64_u s3,
                                            uwvm_int_optable::wasm_stack_top_i32_with_i64_u s4,
                                            uwvm_int_optable::wasm_stack_top_f32_f64_v128 f5,
                                            uwvm_int_optable::wasm_stack_top_f32_f64_v128 f6) noexcept
{
    constexpr uwvm_int_optable::uwvm_interpreter_translate_option_t opt{
        .is_tail_call = false,
        .i32_stack_top_begin_pos = 3uz,
        .i32_stack_top_end_pos = 5uz,
        .i64_stack_top_begin_pos = 3uz,
        .i64_stack_top_end_pos = 5uz,
        .f32_stack_top_begin_pos = 5uz,
        .f32_stack_top_end_pos = 7uz,
        .f64_stack_top_begin_pos = 5uz,
        .f64_stack_top_end_pos = 7uz,
        .v128_stack_top_begin_pos = 5uz,
        .v128_stack_top_end_pos = 7uz};

    constexpr uwvm_int_optable::uwvm_interpreter_stacktop_currpos_t curr{
        .i32_stack_top_curr_pos = 3uz,
        .i64_stack_top_curr_pos = 3uz,
        .f32_stack_top_curr_pos = 5uz,
        .f64_stack_top_curr_pos = 5uz,
        .v128_stack_top_curr_pos = 5uz};

    // pop order: i32 (s3), i64 (s4), f32 (f5), v128 (f6), then i32/f32 from memory.
    auto const vals = uwvm_int_optable::get_vals_from_operand_stack<opt,
                                                                    curr,
                                                                    ::fast_io::tuple<wasm_i32, wasm_i64, wasm_f32, wasm_v128, wasm_i32, wasm_f32>>(
        op, sp, local_base, s3, s4, f5, f6);
    return ::fast_io::get<0>(vals);
}

struct codegen_mixed_mem_t
{
    wasm_i32 i32_val;
    wasm_f32 f32_val;
    ::std::byte* sp;
};

[[gnu::noinline]] codegen_mixed_mem_t codegen_mixed_pop_mem_pair(::std::byte const* op,
                                                                 ::std::byte* sp,
                                                                 ::std::byte* local_base,
                                                                 uwvm_int_optable::wasm_stack_top_i32_with_i64_u s3,
                                                                 uwvm_int_optable::wasm_stack_top_i32_with_i64_u s4,
                                                                 uwvm_int_optable::wasm_stack_top_f32_f64_v128 f5,
                                                                 uwvm_int_optable::wasm_stack_top_f32_f64_v128 f6) noexcept
{
    constexpr uwvm_int_optable::uwvm_interpreter_translate_option_t opt{
        .is_tail_call = false,
        .i32_stack_top_begin_pos = 3uz,
        .i32_stack_top_end_pos = 5uz,
        .i64_stack_top_begin_pos = 3uz,
        .i64_stack_top_end_pos = 5uz,
        .f32_stack_top_begin_pos = 5uz,
        .f32_stack_top_end_pos = 7uz,
        .f64_stack_top_begin_pos = 5uz,
        .f64_stack_top_end_pos = 7uz,
        .v128_stack_top_begin_pos = 5uz,
        .v128_stack_top_end_pos = 7uz};

    constexpr uwvm_int_optable::uwvm_interpreter_stacktop_currpos_t curr{
        .i32_stack_top_curr_pos = 3uz,
        .i64_stack_top_curr_pos = 3uz,
        .f32_stack_top_curr_pos = 5uz,
        .f64_stack_top_curr_pos = 5uz,
        .v128_stack_top_curr_pos = 5uz};

    auto const vals = uwvm_int_optable::get_vals_from_operand_stack<opt,
                                                                    curr,
                                                                    ::fast_io::tuple<wasm_i32, wasm_i64, wasm_f32, wasm_v128, wasm_i32, wasm_f32>>(
        op, sp, local_base, s3, s4, f5, f6);
    return codegen_mixed_mem_t{.i32_val = ::fast_io::get<4>(vals), .f32_val = ::fast_io::get<5>(vals), .sp = sp};
}

int main()
{
    // This binary is for compile/codegen tests only.
    return 0;
}
