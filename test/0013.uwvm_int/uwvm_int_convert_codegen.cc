#include <uwvm2/runtime/compiler/uwvm_int/optable/convert.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

using slot_scalar = optable::wasm_stack_top_i32_i64_f32_f64_u;
using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

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
// third-parties/boost_unordered/include test/0013.uwvm_int/uwvm_int_convert_codegen.cc -o /tmp/uwvm_int_convert_codegen.s`

using opfunc_cached_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2, slot_scalar, slot_scalar>;
using opfunc_stack_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2>;
using opfunc_i32_f32_disjoint_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2, wasm_i32, wasm_f32, wasm_f32>;
using opfunc_i32_only_t = optable::uwvm_interpreter_opfunc_t<T0, T1, T2, wasm_i32>;

[[gnu::noinline]] void end_cached(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    codegen_keep(ip);
    codegen_keep(sp);
    codegen_keep(local_base);
    codegen_keep(s3);
    codegen_keep(s4);
}

[[gnu::noinline]] void end_stack(T0 ip, T1 sp, T2 local_base)
{
    codegen_keep(ip);
    codegen_keep(sp);
    codegen_keep(local_base);
}

[[gnu::noinline]] void codegen_i64_extend_i32_u_cached(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
                                                               .i32_stack_top_begin_pos = 3uz,
                                                               .i32_stack_top_end_pos = 5uz,
                                                               .i64_stack_top_begin_pos = 3uz,
                                                               .i64_stack_top_end_pos = 5uz,
                                                               .f32_stack_top_begin_pos = 3uz,
                                                               .f32_stack_top_end_pos = 5uz,
                                                               .f64_stack_top_begin_pos = 3uz,
                                                               .f64_stack_top_end_pos = 5uz,
                                                               .v128_stack_top_begin_pos = SIZE_MAX,
                                                               .v128_stack_top_end_pos = SIZE_MAX};

    optable::uwvmint_i64_extend_i32_u<opt, 3uz>(ip, sp, local_base, s3, s4);
}

[[gnu::noinline]] void codegen_i32_trunc_f64_s_cached(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
                                                               .i32_stack_top_begin_pos = 3uz,
                                                               .i32_stack_top_end_pos = 5uz,
                                                               .i64_stack_top_begin_pos = 3uz,
                                                               .i64_stack_top_end_pos = 5uz,
                                                               .f32_stack_top_begin_pos = 3uz,
                                                               .f32_stack_top_end_pos = 5uz,
                                                               .f64_stack_top_begin_pos = 3uz,
                                                               .f64_stack_top_end_pos = 5uz,
                                                               .v128_stack_top_begin_pos = SIZE_MAX,
                                                               .v128_stack_top_end_pos = SIZE_MAX};

    optable::uwvmint_i32_trunc_f64_s<opt, 3uz>(ip, sp, local_base, s3, s4);
}

[[gnu::noinline]] void codegen_f64_promote_f32_cached(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
                                                               .i32_stack_top_begin_pos = 3uz,
                                                               .i32_stack_top_end_pos = 5uz,
                                                               .i64_stack_top_begin_pos = 3uz,
                                                               .i64_stack_top_end_pos = 5uz,
                                                               .f32_stack_top_begin_pos = 3uz,
                                                               .f32_stack_top_end_pos = 5uz,
                                                               .f64_stack_top_begin_pos = 3uz,
                                                               .f64_stack_top_end_pos = 5uz,
                                                               .v128_stack_top_begin_pos = SIZE_MAX,
                                                               .v128_stack_top_end_pos = SIZE_MAX};

    optable::uwvmint_f64_promote_f32<opt, 3uz>(ip, sp, local_base, s3, s4);
}

[[gnu::noinline]] void codegen_i32_reinterpret_f32_cached(T0 ip, T1 sp, T2 local_base, slot_scalar s3, slot_scalar s4)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
                                                               .i32_stack_top_begin_pos = 3uz,
                                                               .i32_stack_top_end_pos = 5uz,
                                                               .i64_stack_top_begin_pos = 3uz,
                                                               .i64_stack_top_end_pos = 5uz,
                                                               .f32_stack_top_begin_pos = 3uz,
                                                               .f32_stack_top_end_pos = 5uz,
                                                               .f64_stack_top_begin_pos = 3uz,
                                                               .f64_stack_top_end_pos = 5uz,
                                                               .v128_stack_top_begin_pos = SIZE_MAX,
                                                               .v128_stack_top_end_pos = SIZE_MAX};

    optable::uwvmint_i32_reinterpret_f32<opt, 3uz>(ip, sp, local_base, s3, s4);
}

// No stacktop cache: trunc reads f64 from operand stack memory and advances sp.
[[gnu::noinline]] void codegen_i32_trunc_f64_s_stack(T0 ip, T1 sp, T2 local_base)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
    optable::uwvmint_i32_trunc_f64_s<opt, 0uz>(ip, sp, local_base);
}

[[gnu::noinline]] void codegen_f32_convert_i32_u_disjoint(T0 ip, T1 sp, T2 local_base, wasm_i32 r3, wasm_f32 r4, wasm_f32 r5)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
                                                               .i32_stack_top_begin_pos = 3uz,
                                                               .i32_stack_top_end_pos = 4uz,
                                                               .i64_stack_top_begin_pos = SIZE_MAX,
                                                               .i64_stack_top_end_pos = SIZE_MAX,
                                                               .f32_stack_top_begin_pos = 4uz,
                                                               .f32_stack_top_end_pos = 6uz,
                                                               .f64_stack_top_begin_pos = SIZE_MAX,
                                                               .f64_stack_top_end_pos = SIZE_MAX,
                                                               .v128_stack_top_begin_pos = SIZE_MAX,
                                                               .v128_stack_top_end_pos = SIZE_MAX};

    optable::uwvmint_f32_convert_i32_u<opt, 3uz, 4uz>(ip, sp, local_base, r3, r4, r5);
}

[[gnu::noinline]] void codegen_translate_f32_convert_i32_u_disjoint(T0 ip, T1 sp, T2 local_base, wasm_i32 r3, wasm_f32 r4, wasm_f32 r5)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
                                                               .i32_stack_top_begin_pos = 3uz,
                                                               .i32_stack_top_end_pos = 4uz,
                                                               .i64_stack_top_begin_pos = SIZE_MAX,
                                                               .i64_stack_top_end_pos = SIZE_MAX,
                                                               .f32_stack_top_begin_pos = 4uz,
                                                               .f32_stack_top_end_pos = 6uz,
                                                               .f64_stack_top_begin_pos = SIZE_MAX,
                                                               .f64_stack_top_end_pos = SIZE_MAX,
                                                               .v128_stack_top_begin_pos = SIZE_MAX,
                                                               .v128_stack_top_end_pos = SIZE_MAX};

    constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{.i32_stack_top_curr_pos = 3uz,
                                                                .i64_stack_top_curr_pos = SIZE_MAX,
                                                                .f32_stack_top_curr_pos = 4uz,
                                                                .f64_stack_top_curr_pos = SIZE_MAX,
                                                                .v128_stack_top_curr_pos = SIZE_MAX};

    opfunc_i32_f32_disjoint_t fn = optable::translate::get_uwvmint_f32_convert_i32_u_fptr<opt, T0, T1, T2, wasm_i32, wasm_f32, wasm_f32>(curr);
    fn(ip, sp, local_base, r3, r4, r5);
}

[[gnu::noinline]] void codegen_i32_trunc_f32_s_disjoint(T0 ip, T1 sp, T2 local_base, wasm_i32 r3, wasm_f32 r4, wasm_f32 r5)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
                                                               .i32_stack_top_begin_pos = 3uz,
                                                               .i32_stack_top_end_pos = 4uz,
                                                               .i64_stack_top_begin_pos = SIZE_MAX,
                                                               .i64_stack_top_end_pos = SIZE_MAX,
                                                               .f32_stack_top_begin_pos = 4uz,
                                                               .f32_stack_top_end_pos = 6uz,
                                                               .f64_stack_top_begin_pos = SIZE_MAX,
                                                               .f64_stack_top_end_pos = SIZE_MAX,
                                                               .v128_stack_top_begin_pos = SIZE_MAX,
                                                               .v128_stack_top_end_pos = SIZE_MAX};

    optable::uwvmint_i32_trunc_f32_s<opt, 4uz, 3uz>(ip, sp, local_base, r3, r4, r5);
}

[[gnu::noinline]] void codegen_translate_i32_trunc_f32_s_out_only(T0 ip, T1 sp, T2 local_base, wasm_i32 r3)
{
    constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true,
                                                               .i32_stack_top_begin_pos = 3uz,
                                                               .i32_stack_top_end_pos = 4uz,
                                                               .i64_stack_top_begin_pos = SIZE_MAX,
                                                               .i64_stack_top_end_pos = SIZE_MAX,
                                                               .f32_stack_top_begin_pos = SIZE_MAX,
                                                               .f32_stack_top_end_pos = SIZE_MAX,
                                                               .f64_stack_top_begin_pos = SIZE_MAX,
                                                               .f64_stack_top_end_pos = SIZE_MAX,
                                                               .v128_stack_top_begin_pos = SIZE_MAX,
                                                               .v128_stack_top_end_pos = SIZE_MAX};

    constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{.i32_stack_top_curr_pos = 3uz,
                                                                .i64_stack_top_curr_pos = SIZE_MAX,
                                                                .f32_stack_top_curr_pos = SIZE_MAX,
                                                                .f64_stack_top_curr_pos = SIZE_MAX,
                                                                .v128_stack_top_curr_pos = SIZE_MAX};

    opfunc_i32_only_t fn = optable::translate::get_uwvmint_i32_trunc_f32_s_fptr<opt, T0, T1, T2, wasm_i32>(curr);
    fn(ip, sp, local_base, r3);
}

int main()
{
    // Force emission of the opfunc signature types.
    opfunc_cached_t cached_end = &end_cached;
    opfunc_stack_t stack_end = &end_stack;
    opfunc_i32_f32_disjoint_t disjoint_end = &codegen_translate_f32_convert_i32_u_disjoint;
    opfunc_i32_only_t out_only_end = &codegen_translate_i32_trunc_f32_s_out_only;
    codegen_keep(cached_end);
    codegen_keep(stack_end);
    codegen_keep(disjoint_end);
    codegen_keep(out_only_end);
    return 0;
}
