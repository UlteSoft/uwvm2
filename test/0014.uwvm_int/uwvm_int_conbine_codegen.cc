#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
#define UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS
#endif

#include <uwvm2/runtime/compiler/uwvm_int/optable/conbine.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/conbine_heavy.h>

#include <cstddef>
#include <type_traits>

namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

using slot_scalar = optable::wasm_stack_top_i32_i64_f32_f64_u;

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

using T0 = ::std::byte const*;
using T1 = ::std::byte*;
using T2 = ::std::byte*;

// Intended for manual/CI codegen inspection under -O3.
//
// Example:
// `clang++ --sysroot=$SYSROOT -O3 -S -fuse-ld=lld -std=c++26 -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -I
// third-parties/boost_unordered/include test/0014.uwvm_int/uwvm_int_conbine_codegen.cc -o /tmp/uwvm_int_conbine_codegen.s`

static constexpr optable::uwvm_interpreter_translate_option_t opt_scalar_cache{
    .is_tail_call = true,
    .i32_stack_top_begin_pos = 3uz,
    .i32_stack_top_end_pos = 5uz,
    .i64_stack_top_begin_pos = 3uz,
    .i64_stack_top_end_pos = 5uz,
    .f32_stack_top_begin_pos = 3uz,
    .f32_stack_top_end_pos = 5uz,
    .f64_stack_top_begin_pos = 3uz,
    .f64_stack_top_end_pos = 5uz,
    .v128_stack_top_begin_pos = SIZE_MAX,
    .v128_stack_top_end_pos = SIZE_MAX,
};

static constexpr optable::uwvm_interpreter_stacktop_currpos_t currpos_scalar_cache{
    .i32_stack_top_curr_pos = 3uz,
    .i64_stack_top_curr_pos = 3uz,
    .f32_stack_top_curr_pos = 3uz,
    .f64_stack_top_curr_pos = 3uz,
    .v128_stack_top_curr_pos = SIZE_MAX,
};

using opfunc_cached_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2, slot_scalar, slot_scalar>;

[[gnu::noinline]] void keep_all_fps()
{
#define UWVM_KEEP_SELECTOR(name) \
    codegen_keep(optable::translate::name<opt_scalar_cache, T0, T1, T2, slot_scalar, slot_scalar>(currpos_scalar_cache))

    // AUTO-GENERATED list from `conbine.h` tail-call selectors returning `uwvm_interpreter_opfunc_t`.
    // NOTE: We intentionally exclude selectors that require additional runtime parameters (e.g. `native_memory_t const&`),
    // and a few selectors that require a different `Type...` signature than this TU uses.
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_f32_eq_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_f32_ge_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_f32_gt_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_f32_le_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_f32_lt_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_f32_ne_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_f64_eq_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_f64_lt_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i32_and_nz_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i32_eq_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i32_eq_imm_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i32_eqz_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i32_ge_u_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i32_ge_u_imm_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i32_gt_s_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i32_gt_u_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i32_le_s_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i32_le_u_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i32_lt_u_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i32_lt_u_imm_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i32_ne_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i64_eqz_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i64_gt_u_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i64_lt_u_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_i64_ne_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_local_eqz_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_br_if_local_tee_nz_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_2mul_add_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_abs_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_add_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_add_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_div_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_max_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_max_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_min_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_min_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_mul_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_mul_add_3localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_mul_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_mul_sub_3localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_neg_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_select_local_set_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_select_local_tee_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_sqrt_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f32_sub_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_2mul_add_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_abs_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_add_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_add_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_div_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_max_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_max_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_min_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_min_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_mul_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_mul_add_3localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_mul_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_mul_sub_3localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_neg_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_f64_sub_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_for_i32_inc_lt_u_br_if_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_for_ptr_inc_ne_br_if_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_add_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_add_imm_local_set_same_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_add_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_add_mul_imm_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_add_shl_imm_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_and_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_and_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_clz_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_ctz_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_eq_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_eqz_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_ge_u_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_lt_u_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_mul_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_mul_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_ne_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_or_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_or_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_popcnt_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_rot_xor_add_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_rotl_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_rotr_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_select_local_set_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_shl_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_shr_s_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_shr_u_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_sub_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_sub_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_xor_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_xor_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i32_xorshift_mix_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i64_add_2localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i64_add_imm_localget_fptr);
    UWVM_KEEP_SELECTOR(get_uwvmint_i64_and_imm_localget_fptr);

#undef UWVM_KEEP_SELECTOR
}

int main()
{
    keep_all_fps();
    return 0;
}

#include <uwvm2/utils/macro/pop_macros.h>
