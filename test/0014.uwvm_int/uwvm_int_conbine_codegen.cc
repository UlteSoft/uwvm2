#include <uwvm2/utils/macro/push_macros.h>

#include <uwvm2/runtime/compiler/uwvm_int/optable/conbine.h>

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

static constexpr opfunc_cached_t fp_i32_add_imm_localget =
    optable::translate::get_uwvmint_i32_add_imm_localget_fptr<opt_scalar_cache, T0, T1, T2, slot_scalar, slot_scalar>(currpos_scalar_cache);
static constexpr opfunc_cached_t fp_i32_add_2localget =
    optable::translate::get_uwvmint_i32_add_2localget_fptr<opt_scalar_cache, T0, T1, T2, slot_scalar, slot_scalar>(currpos_scalar_cache);
static constexpr opfunc_cached_t fp_i32_clz_localget =
    optable::translate::get_uwvmint_i32_clz_localget_fptr<opt_scalar_cache, T0, T1, T2, slot_scalar, slot_scalar>(currpos_scalar_cache);

static constexpr opfunc_cached_t fp_br_if_i32_eq =
    optable::translate::get_uwvmint_br_if_i32_eq_fptr<opt_scalar_cache, T0, T1, T2, slot_scalar, slot_scalar>(currpos_scalar_cache);
static constexpr opfunc_cached_t fp_br_if_i32_eq_imm =
    optable::translate::get_uwvmint_br_if_i32_eq_imm_fptr<opt_scalar_cache, T0, T1, T2, slot_scalar, slot_scalar>(currpos_scalar_cache);

static constexpr opfunc_cached_t fp_i32_load_localget_off =
    optable::translate::get_uwvmint_i32_load_localget_off_fptr<opt_scalar_cache, T0, T1, T2, slot_scalar, slot_scalar>(currpos_scalar_cache);

[[gnu::noinline]] void codegen_i32_add_imm_localget(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    codegen_keep(fp_i32_add_imm_localget);
    fp_i32_add_imm_localget(ip, sp, local_base, s3, s4);
}

[[gnu::noinline]] void codegen_i32_add_2localget(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    codegen_keep(fp_i32_add_2localget);
    fp_i32_add_2localget(ip, sp, local_base, s3, s4);
}

[[gnu::noinline]] void codegen_i32_clz_localget(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    codegen_keep(fp_i32_clz_localget);
    fp_i32_clz_localget(ip, sp, local_base, s3, s4);
}

[[gnu::noinline]] void codegen_br_if_i32_eq(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    (void)local_base;
    (void)s3;
    (void)s4;
    codegen_keep(fp_br_if_i32_eq);
    fp_br_if_i32_eq(ip, sp, local_base, s3, s4);
}

[[gnu::noinline]] void codegen_br_if_i32_eq_imm(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    codegen_keep(fp_br_if_i32_eq_imm);
    fp_br_if_i32_eq_imm(ip, sp, local_base, s3, s4);
}

[[gnu::noinline]] void codegen_i32_load_localget_off(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    codegen_keep(fp_i32_load_localget_off);
    fp_i32_load_localget_off(ip, sp, local_base, s3, s4);
}

int main()
{
    codegen_keep(fp_i32_add_imm_localget);
    codegen_keep(fp_br_if_i32_eq);
    codegen_keep(fp_i32_load_localget_off);
    return 0;
}

#include <uwvm2/utils/macro/pop_macros.h>

