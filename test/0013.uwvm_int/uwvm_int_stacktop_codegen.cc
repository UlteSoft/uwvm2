#include <uwvm2/runtime/compiler/uwvm_int/optable/define.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <bit>
#include <type_traits>

#if defined(__aarch64__) || defined(__AARCH64__)
# include <arm_neon.h>
#endif

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

template <typename T>
[[gnu::always_inline]] inline void codegen_keep(T const& v) noexcept
{
#if defined(__clang__) || defined(__GNUC__)
    using U = ::std::remove_cvref_t<T>;
    if constexpr(::std::is_integral_v<U> || ::std::is_pointer_v<U>)
    {
        asm volatile("" : : "r"(v));
    }
    else if constexpr(::std::is_floating_point_v<U>)
    {
        asm volatile("" : : "w"(v));
    }
    else
    {
        // e.g. wasm_v128 (vector type on AArch64) -> keep in SIMD/FP reg class.
        asm volatile("" : : "w"(v));
    }
#else
    (void)v;
#endif
}

struct codegen_i32_sp_t
{
    wasm_i32 v;
    ::std::byte* sp;
};

// Expectation (no stacktop, single value):
// - Ideally becomes `ldr w0, [x1, #-4]!` (update sp) + ret, when sp is observed.
[[gnu::noinline]] codegen_i32_sp_t codegen_pop_i32_tuple1_no_stacktop_keep_sp(::std::byte const* op,
                                                                              ::std::byte* sp,
                                                                              ::std::byte* local_base) noexcept
{
    constexpr uwvm_int_optable::uwvm_interpreter_translate_option_t opt{};
    constexpr uwvm_int_optable::uwvm_interpreter_stacktop_currpos_t curr{};

    auto const vals = uwvm_int_optable::get_vals_from_operand_stack<opt, curr, ::fast_io::tuple<wasm_i32>>(op, sp, local_base);
    wasm_i32 const v = ::fast_io::get<0>(vals);
    codegen_keep(v);
    codegen_keep(sp);
    return codegen_i32_sp_t{.v = v, .sp = sp};
}

// Expectation (stacktop, single value): pure register move (no memory access).
[[gnu::noinline]] wasm_i32 codegen_pop_i32_tuple1_stacktop(::std::byte const* op,
                                                          ::std::byte* sp,
                                                          ::std::byte* local_base,
                                                          wasm_i32 r3) noexcept
{
    constexpr uwvm_int_optable::uwvm_interpreter_translate_option_t opt{
        .is_tail_call = false,
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

    constexpr uwvm_int_optable::uwvm_interpreter_stacktop_currpos_t curr{
        .i32_stack_top_curr_pos = 3uz,
        .i64_stack_top_curr_pos = SIZE_MAX,
        .f32_stack_top_curr_pos = SIZE_MAX,
        .f64_stack_top_curr_pos = SIZE_MAX,
        .v128_stack_top_curr_pos = SIZE_MAX};

    auto const vals = uwvm_int_optable::get_vals_from_operand_stack<opt, curr, ::fast_io::tuple<wasm_i32>>(op, sp, local_base, r3);
    wasm_i32 const v = ::fast_io::get<0>(vals);
    codegen_keep(v);
    return v;
}

[[gnu::noinline]] wasm_i32 codegen_pop_i32_from_operand_stack(::std::byte const* op, ::std::byte* sp, ::std::byte* local_base) noexcept
{
    return uwvm_int_optable::get_curr_val_from_operand_stack_cache<wasm_i32>(op, sp, local_base);
}

[[gnu::noinline]] codegen_i32_sp_t codegen_pop_i32_from_operand_stack_keep_sp(::std::byte const* op, ::std::byte* sp, ::std::byte* local_base) noexcept
{
    wasm_i32 const v = uwvm_int_optable::get_curr_val_from_operand_stack_cache<wasm_i32>(op, sp, local_base);
    codegen_keep(v);
    codegen_keep(sp);
    return codegen_i32_sp_t{.v = v, .sp = sp};
}

[[gnu::noinline]] wasm_i32 codegen_pop_i32_no_stacktop(::std::byte const* op, ::std::byte* sp, ::std::byte* local_base) noexcept
{
    constexpr uwvm_int_optable::uwvm_interpreter_translate_option_t opt{};
    auto const v = uwvm_int_optable::get_curr_val_from_operand_stack_top<opt, wasm_i32, 0uz>(op, sp, local_base);
    codegen_keep(v);
    return v;
}

// i32/i64 merge uses stacktop slots [3,5) -> indices 3 and 4.
// f32/f64/v128 merge uses stacktop slots [5,7) -> indices 5 and 6 (carrier = v128).
[[gnu::noinline]] wasm_i32 codegen_mixed_pop(::std::byte const* op,
                                            ::std::byte* sp,
                                            ::std::byte* local_base,
                                            uwvm_int_optable::wasm_stack_top_i32_with_i64_u s3,
                                            uwvm_int_optable::wasm_stack_top_i32_with_i64_u s4,
                                            wasm_v128 v5,
                                            wasm_v128 v6) noexcept
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

    // pop order: i32 (s3), i64 (s4), f32 (v5), v128 (v6), then i32/f32 from memory.
    auto const vals = uwvm_int_optable::get_vals_from_operand_stack<opt,
                                                                    curr,
                                                                    ::fast_io::tuple<wasm_i32, wasm_i64, wasm_f32, wasm_v128, wasm_i32, wasm_f32>>(
        op, sp, local_base, s3, s4, v5, v6);
    // Prevent DCE (otherwise the optimizer may simplify this function to `return s3.i32;`).
    codegen_keep(::fast_io::get<0>(vals));
    codegen_keep(::fast_io::get<1>(vals));
    codegen_keep(::fast_io::get<2>(vals));
    codegen_keep(::fast_io::get<3>(vals));
    codegen_keep(::fast_io::get<4>(vals));
    codegen_keep(::fast_io::get<5>(vals));
    codegen_keep(sp);
    return static_cast<wasm_i32>(::fast_io::get<0>(vals) + ::fast_io::get<4>(vals));
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
                                                                 wasm_v128 v5,
                                                                 wasm_v128 v6) noexcept
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
        op, sp, local_base, s3, s4, v5, v6);
    codegen_keep(::fast_io::get<4>(vals));
    codegen_keep(::fast_io::get<5>(vals));
    codegen_keep(sp);
    return codegen_mixed_mem_t{.i32_val = ::fast_io::get<4>(vals), .f32_val = ::fast_io::get<5>(vals), .sp = sp};
}

#if defined(__aarch64__) || defined(__AARCH64__)
// AArch64 AAPCS64: floating-point and SIMD arguments share the same v0-v7 argument registers.
// This helper exists to make the codegen observable in `.s`:
// - Expect stores like `str s0, ...`, `str d1, ...`, `str q2, ...`, etc.
//
// NOTE: For stack-top caching, uwvm uses `wasm_v128` as the carrier when f32/f64/v128 ranges are identical.
// This function tests the ABI of passing those merged slots as `wasm_v128` (should use q0-q7 on AArch64).
[[gnu::noinline]] void codegen_aarch64_8_f32_f64_v128_abi(wasm_v128 a0,
                                                         wasm_v128 a1,
                                                         wasm_v128 a2,
                                                         wasm_v128 a3,
                                                         wasm_v128 a4,
                                                         wasm_v128 a5,
                                                         wasm_v128 a6,
                                                         wasm_v128 a7) noexcept
{
    // Volatile stores prevent the compiler from deleting the arg uses and make the incoming ABI visible.
    alignas(16) volatile wasm_v128 v0 = a0;
    alignas(16) volatile wasm_v128 v1 = a1;
    alignas(16) volatile wasm_v128 v2 = a2;
    alignas(16) volatile wasm_v128 v3 = a3;
    alignas(16) volatile wasm_v128 v4 = a4;
    alignas(16) volatile wasm_v128 v5 = a5;
    alignas(16) volatile wasm_v128 v6 = a6;
    alignas(16) volatile wasm_v128 v7 = a7;

    // Touch different views via the same extraction helpers used by the interpreter.
    codegen_keep(uwvm_int_optable::details::get_f32_low_from_v128_slot(v0));
    codegen_keep(uwvm_int_optable::details::get_f64_low_from_v128_slot(v1));
    codegen_keep(static_cast<wasm_v128>(v2));
    codegen_keep(uwvm_int_optable::details::get_f32_low_from_v128_slot(v3));
    codegen_keep(uwvm_int_optable::details::get_f64_low_from_v128_slot(v4));
    codegen_keep(static_cast<wasm_v128>(v5));
    codegen_keep(uwvm_int_optable::details::get_f32_low_from_v128_slot(v6));
    codegen_keep(uwvm_int_optable::details::get_f64_low_from_v128_slot(v7));
}

// Ensure "all-v128 args" can still be consumed as scalar f32/f64 with the expected register views:
// - a0 is passed in q0 -> its low f32 view is s0
// - a1 is passed in q1 -> its low f64 view is d1
// - a2 is passed in q2 -> full v128 is q2
[[gnu::noinline]] void codegen_aarch64_v128_args_view_as_s_d_q(wasm_v128 a0, wasm_v128 a1, wasm_v128 a2) noexcept
{
    volatile wasm_f32 s0 = uwvm_int_optable::details::get_f32_low_from_v128_slot(a0);
    volatile ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 d1 = uwvm_int_optable::details::get_f64_low_from_v128_slot(a1);
    alignas(16) volatile wasm_v128 q2 = a2;

    codegen_keep(s0);
    codegen_keep(d1);
    codegen_keep(q2);
}

// f32/f64 merge carrier is f64: verify we can read f32 low bits in the FP register file.
// Expected on AArch64:
// - a0 is passed in d0 -> low f32 view is s0
// - a1 is passed in d1 -> f64 view is d1
// - a2 is passed in d2 -> low f32 view is s2
[[gnu::noinline]] void codegen_aarch64_f64_args_view_as_s_d_s(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 a0,
                                                              ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 a1,
                                                              ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 a2) noexcept
{
    volatile wasm_f32 s0 = uwvm_int_optable::details::get_f32_low_from_f64_slot(a0);
    volatile ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 d1 = a1;
    volatile wasm_f32 s2 = uwvm_int_optable::details::get_f32_low_from_f64_slot(a2);

    codegen_keep(s0);
    codegen_keep(d1);
    codegen_keep(s2);
}

// f32/f64 merge candidate (slot=8B):
// Use `wasm_f64` as the ABI carrier so args use d0-d7 on AArch64, then reinterpret low 32 bits as f32 when needed.
[[gnu::noinline]] void codegen_aarch64_8_f64_slot_for_f32_f64_merge_abi(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 a0,
                                                                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 a1,
                                                                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 a2,
                                                                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 a3,
                                                                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 a4,
                                                                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 a5,
                                                                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 a6,
                                                                        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 a7) noexcept
{
    // Keep ABI visible: expect `str d0, ...` etc.
    volatile ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 d0 = a0;
    volatile ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 d1 = a1;
    volatile ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 d2 = a2;
    volatile ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 d3 = a3;
    volatile ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 d4 = a4;
    volatile ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 d5 = a5;
    volatile ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 d6 = a6;
    volatile ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 d7 = a7;

    // Reinterpret low 32 bits as f32 (no numeric conversion).
    auto const u0 = ::std::bit_cast<::std::uint_least64_t>(a0);
    auto const u1 = ::std::bit_cast<::std::uint_least64_t>(a1);
    auto const u2 = ::std::bit_cast<::std::uint_least64_t>(a2);
    auto const u3 = ::std::bit_cast<::std::uint_least64_t>(a3);
    auto const f0 = ::std::bit_cast<wasm_f32>(static_cast<::std::uint_least32_t>(u0));
    auto const f1 = ::std::bit_cast<wasm_f32>(static_cast<::std::uint_least32_t>(u1));
    auto const f2 = ::std::bit_cast<wasm_f32>(static_cast<::std::uint_least32_t>(u2));
    auto const f3 = ::std::bit_cast<wasm_f32>(static_cast<::std::uint_least32_t>(u3));

    codegen_keep(d0);
    codegen_keep(d1);
    codegen_keep(d2);
    codegen_keep(d3);
    codegen_keep(d4);
    codegen_keep(d5);
    codegen_keep(d6);
    codegen_keep(d7);

    codegen_keep(f0);
    codegen_keep(f1);
    codegen_keep(f2);
    codegen_keep(f3);
}

// Same goal as above (reinterpret low 32 bits of an f64 slot as f32), but *without* FP->GPR->FP moves.
// On AArch64, `sN` and `dN` are just different views of the same `vN` register. We can copy the low 32 bits
// by using the `s` view in an FP-reg-to-FP-reg move.
[[gnu::noinline]] wasm_f32 codegen_aarch64_low32_f32_from_f64_reg_no_gpr(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 in) noexcept
{
    wasm_f32 out{};
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("fmov %s0, %s1" : "=w"(out) : "w"(in));
#else
    // fallback: keeps buildable on non-GNU/Clang toolchains (not a performance path)
    auto const u = ::std::bit_cast<::std::uint_least64_t>(in);
    out = ::std::bit_cast<wasm_f32>(static_cast<::std::uint_least32_t>(u));
#endif
    codegen_keep(out);
    return out;
}

// No inline asm: use NEON intrinsics to keep everything in the FP/SIMD register file.
// On AArch64 this should optimize to essentially just `ret` (return s0 view of the incoming d0/v0).
[[gnu::noinline]] wasm_f32 codegen_aarch64_low32_f32_from_f64_neon_no_asm(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 in) noexcept
{
#if defined(__aarch64__) || defined(__AARCH64__)
    float64x1_t v = vdup_n_f64(in);
    float32x2_t v2 = vreinterpret_f32_f64(v);
    wasm_f32 out = vget_lane_f32(v2, 0);
    codegen_keep(out);
    return out;
#else
    auto const u = ::std::bit_cast<::std::uint_least64_t>(in);
    wasm_f32 out = ::std::bit_cast<wasm_f32>(static_cast<::std::uint_least32_t>(u));
    codegen_keep(out);
    return out;
#endif
}

// -------- ABI probes for other merged-union slot types (AArch64) --------

[[gnu::noinline]] void codegen_aarch64_8_i32_i64_union_abi(uwvm_int_optable::wasm_stack_top_i32_with_i64_u a0,
                                                          uwvm_int_optable::wasm_stack_top_i32_with_i64_u a1,
                                                          uwvm_int_optable::wasm_stack_top_i32_with_i64_u a2,
                                                          uwvm_int_optable::wasm_stack_top_i32_with_i64_u a3,
                                                          uwvm_int_optable::wasm_stack_top_i32_with_i64_u a4,
                                                          uwvm_int_optable::wasm_stack_top_i32_with_i64_u a5,
                                                          uwvm_int_optable::wasm_stack_top_i32_with_i64_u a6,
                                                          uwvm_int_optable::wasm_stack_top_i32_with_i64_u a7) noexcept
{
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_with_i64_u v0 = a0;
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_with_i64_u v1 = a1;
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_with_i64_u v2 = a2;
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_with_i64_u v3 = a3;
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_with_i64_u v4 = a4;
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_with_i64_u v5 = a5;
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_with_i64_u v6 = a6;
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_with_i64_u v7 = a7;

    codegen_keep(v0.i32);
    codegen_keep(v1.i64);
    codegen_keep(v2.i32);
    codegen_keep(v3.i64);
    codegen_keep(v4.i32);
    codegen_keep(v5.i64);
    codegen_keep(v6.i32);
    codegen_keep(v7.i64);
}

[[gnu::noinline]] void codegen_aarch64_8_i32_f32_union_abi(uwvm_int_optable::wasm_stack_top_i32_with_f32_u a0,
                                                          uwvm_int_optable::wasm_stack_top_i32_with_f32_u a1,
                                                          uwvm_int_optable::wasm_stack_top_i32_with_f32_u a2,
                                                          uwvm_int_optable::wasm_stack_top_i32_with_f32_u a3,
                                                          uwvm_int_optable::wasm_stack_top_i32_with_f32_u a4,
                                                          uwvm_int_optable::wasm_stack_top_i32_with_f32_u a5,
                                                          uwvm_int_optable::wasm_stack_top_i32_with_f32_u a6,
                                                          uwvm_int_optable::wasm_stack_top_i32_with_f32_u a7) noexcept
{
    alignas(4) volatile uwvm_int_optable::wasm_stack_top_i32_with_f32_u v0 = a0;
    alignas(4) volatile uwvm_int_optable::wasm_stack_top_i32_with_f32_u v1 = a1;
    alignas(4) volatile uwvm_int_optable::wasm_stack_top_i32_with_f32_u v2 = a2;
    alignas(4) volatile uwvm_int_optable::wasm_stack_top_i32_with_f32_u v3 = a3;
    alignas(4) volatile uwvm_int_optable::wasm_stack_top_i32_with_f32_u v4 = a4;
    alignas(4) volatile uwvm_int_optable::wasm_stack_top_i32_with_f32_u v5 = a5;
    alignas(4) volatile uwvm_int_optable::wasm_stack_top_i32_with_f32_u v6 = a6;
    alignas(4) volatile uwvm_int_optable::wasm_stack_top_i32_with_f32_u v7 = a7;

    codegen_keep(v0.i32);
    codegen_keep(v1.f32);
    codegen_keep(v2.i32);
    codegen_keep(v3.f32);
    codegen_keep(v4.i32);
    codegen_keep(v5.f32);
    codegen_keep(v6.i32);
    codegen_keep(v7.f32);
}

[[gnu::noinline]] void codegen_aarch64_8_i32_i64_f32_f64_union_abi(uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u a0,
                                                                  uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u a1,
                                                                  uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u a2,
                                                                  uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u a3,
                                                                  uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u a4,
                                                                  uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u a5,
                                                                  uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u a6,
                                                                  uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u a7) noexcept
{
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u v0 = a0;
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u v1 = a1;
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u v2 = a2;
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u v3 = a3;
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u v4 = a4;
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u v5 = a5;
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u v6 = a6;
    alignas(8) volatile uwvm_int_optable::wasm_stack_top_i32_i64_f32_f64_u v7 = a7;

    // Mix views to show the slot is treated as a scalar/aggregate ABI-wise.
    codegen_keep(v0.i32);
    codegen_keep(v1.i64);
    codegen_keep(v2.f32);
    codegen_keep(v3.f64);
    codegen_keep(v4.i32);
    codegen_keep(v5.i64);
    codegen_keep(v6.f32);
    codegen_keep(v7.f64);
}
#endif

int main()
{
    // This binary is for compile/codegen tests only.
    return 0;
}
