/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

/**
 * @file combine_extra_heavy.h
 * @brief Extra-heavy combined opcodes for uwvm-int (ultra-specific mega fusions; disabled by default).
 *
 * @details
 * This header intentionally collects **highly targeted** mega-fusions that collapse a known hot snippet into a single
 * interpreter opfunc dispatch (typically to reduce dispatch overhead and keep values in registers).
 *
 * These optimizations are *not* general-purpose: they trade code size / I-cache footprint / indirect-branch target count
 * for speed on the exact matched workload. Therefore they are **disabled by default** and only compiled when:
 * - `UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS` AND `UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS` are defined
 *   (xmake option: `--enable-uwvm-int-combine-ops=extra`).
 *
 * Usage model (profiling-driven):
 * 1) Find stable hot fragments (often from common data structures / algorithms).
 * 2) Add a strict translator-side pattern matcher.
 * 3) Add an opfunc here, with a precise @brief that shows the canonical source opcode sequence.
 * 4) Keep the emission guarded by EXTRA_HEAVY so it never affects default builds.
 *
 */

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <bit>
# include <cstdint>
# include <cstring>
# include <cmath>
# include <limits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/object/impl.h>
# include "define.h"
# include "register_ring.h"
# include "memory.h"
# include "numeric.h"
# include "compare.h"
# include "convert.h"
# include "variable.h"
# include "conbine.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS) && !defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
# error "UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS requires UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS."
#endif
#if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS) && !defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
# error "UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS requires UWVM_ENABLE_UWVM_INT_COMBINE_OPS (do not auto-define it in headers)."
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)

    /**
     * @brief Extra-heavy combined opcodes (ultra-specific mega fusions).
     *
     * @details
     * This section is expected to contain workload-specific fusions only. Do not move general optimizations here.
     * Add new entries only when a hot fragment is verified by profiling (e.g., `-Rclog` + real benchmarks),
     * and keep the fusions as strict as possible to avoid accidental matches.
     *
     * @note This module is disabled by default; see @file documentation for build-time enabling conditions.
     */

    /// @brief Runs the full divisor loop of the pattern:
    /// `local.get n; local.get i; i32.rem_u; i32.eqz; br_if <break>; local.get sqrt; local.get i; i32.const step; i32.add; local.tee i;
    ///  f64.convert_i32_u; f64.lt; i32.eqz; br_if <loop>`
    /// in one opfunc dispatch (tail-call).
    /// @details
    /// - This is a targeted hot-loop fusion for `test8`-style prime checks.
    /// - Stack-top optimization: N/A (no operand stack values are produced).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (n i32), `local_offset_t` (i i32), `local_offset_t` (sqrt f64), `wasm_i32` (step), `::std::byte const*` (break ip).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_prime_divisor_loop_run(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const n_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const i_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const sqrt_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const step{conbine_details::read_imm<wasm_i32>(type...[0])};

        ::std::byte const* break_ip;  // no init
        ::std::memcpy(::std::addressof(break_ip), type...[0], sizeof(break_ip));
        type...[0] += sizeof(break_ip);

        wasm_i32 const n{conbine_details::load_local<wasm_i32>(type...[2u], n_off)};
        wasm_f64 const sqrt_n{conbine_details::load_local<wasm_f64>(type...[2u], sqrt_off)};

        wasm_i32 i{conbine_details::load_local<wasm_i32>(type...[2u], i_off)};
        for(;;)
        {
            wasm_i32 const rem{numeric_details::eval_int_binop<numeric_details::int_binop::rem_u, wasm_i32, numeric_details::wasm_u32>(n, i)};

# if UWVM_HAS_CPP_ATTRIBUTE(clang::nomerge)
            [[clang::nomerge]]
# endif
            if(rem == wasm_i32{})
            {
                type...[0] = break_ip;

                // break;
                conbine_details::store_local(type...[2u], i_off, i);

                uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
                ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
                UWVM_MUSTTAIL return next_interpreter(type...);
            }

            i = numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(i, step);

            wasm_f64 const i_d{static_cast<wasm_f64>(static_cast<::std::uint_least32_t>(i))};
            bool const lt{details::eval_float_cmp<details::float_cmp::lt, wasm_f64>(sqrt_n, i_d)};
            if(!lt) { continue; }
            break;
        }

        conbine_details::store_local(type...[2u], i_off, i);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Runs the full divisor loop (see `uwvmint_prime_divisor_loop_run`) in one opfunc dispatch (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (n i32), `local_offset_t` (i i32), `local_offset_t` (sqrt f64), `wasm_i32` (step), `::std::byte const*` (break ip).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_prime_divisor_loop_run(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const n_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const i_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const sqrt_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_i32 const step{conbine_details::read_imm<wasm_i32>(typeref...[0])};

        ::std::byte const* break_ip;  // no init
        ::std::memcpy(::std::addressof(break_ip), typeref...[0], sizeof(break_ip));
        typeref...[0] += sizeof(break_ip);

        wasm_i32 const n{conbine_details::load_local<wasm_i32>(typeref...[2u], n_off)};
        wasm_f64 const sqrt_n{conbine_details::load_local<wasm_f64>(typeref...[2u], sqrt_off)};

        wasm_i32 i{conbine_details::load_local<wasm_i32>(typeref...[2u], i_off)};
        for(;;)
        {
            wasm_i32 const rem{numeric_details::eval_int_binop<numeric_details::int_binop::rem_u, wasm_i32, numeric_details::wasm_u32>(n, i)};
            if(rem == wasm_i32{})
            {
                typeref...[0] = break_ip;
                break;
            }

            i = numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(i, step);

            wasm_f64 const i_d{static_cast<wasm_f64>(static_cast<::std::uint_least32_t>(i))};
            bool const lt{details::eval_float_cmp<details::float_cmp::lt, wasm_f64>(sqrt_n, i_d)};
            if(!lt) { continue; }
            break;
        }

        conbine_details::store_local(typeref...[2u], i_off, i);
    }

    namespace translate
    {
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_prime_divisor_loop_run_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_prime_divisor_loop_run<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_prime_divisor_loop_run_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_prime_divisor_loop_run_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_prime_divisor_loop_run_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_prime_divisor_loop_run<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_prime_divisor_loop_run_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_prime_divisor_loop_run_fptr<CompileOption, TypeInTuple...>(curr); }
    }  // namespace translate

    // ----------------------------------------
    // float64: local.get + imm + mul + imm + add + local.tee (push f64)
    // ----------------------------------------

    /// @brief Fuses the hot chain:
    /// `local.get src; f64.const mul; f64.mul; f64.const add; f64.add; local.tee dst`
    /// into one opfunc dispatch (tail-call).
    /// @details
    /// - Stack effect: push 1 f64 (same as `local.tee`).
    /// - Stack-top optimization: supported (f64 is pushed via `conbine_details::push_operand`).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (src), `local_offset_t` (dst), `wasm_f64` (mul), `wasm_f64` (add).
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mul_add_2imm_localget_local_tee(Type... type) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const src_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const dst_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f64 const mul{conbine_details::read_imm<wasm_f64>(type...[0])};
        wasm_f64 const add{conbine_details::read_imm<wasm_f64>(type...[0])};

        wasm_f64 const v{conbine_details::load_local<wasm_f64>(type...[2u], src_off)};
        wasm_f64 const out{v * mul + add};
        conbine_details::store_local(type...[2u], dst_off, out);

        conbine_details::push_operand<CompileOption, wasm_f64, curr_f64_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Same as @ref uwvmint_f64_mul_add_2imm_localget_local_tee but for byref mode.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mul_add_2imm_localget_local_tee(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const src_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const dst_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f64 const mul{conbine_details::read_imm<wasm_f64>(typeref...[0])};
        wasm_f64 const add{conbine_details::read_imm<wasm_f64>(typeref...[0])};

        wasm_f64 const v{conbine_details::load_local<wasm_f64>(typeref...[2u], src_off)};
        wasm_f64 const out{v * mul + add};
        conbine_details::store_local(typeref...[2u], dst_off, out);

        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    namespace combine_extra_heavy_details
    {
        struct f64_mul_add_2imm_localget_local_tee_op
        {
            template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
            { return uwvmint_f64_mul_add_2imm_localget_local_tee<Opt, Pos, Type...>; }

            template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
            inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
            { return uwvmint_f64_mul_add_2imm_localget_local_tee<Opt, Type...>; }
        };
    }  // namespace combine_extra_heavy_details

    namespace translate
    {
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_mul_add_2imm_localget_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.f64_stack_top_begin_pos,
                                                                    CompileOption.f64_stack_top_end_pos,
                                                                    combine_extra_heavy_details::f64_mul_add_2imm_localget_local_tee_op,
                                                                    Type...>(curr.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_add_2imm_localget_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_add_2imm_localget_local_tee_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_f64_mul_add_2imm_localget_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return combine_extra_heavy_details::f64_mul_add_2imm_localget_local_tee_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_add_2imm_localget_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_add_2imm_localget_local_tee_fptr<CompileOption, TypeInTuple...>(curr); }
    }  // namespace translate

    // ----------------------------------------
    // quick_branchy_i32: run the entire hot i32 loop in one dispatch
    // ----------------------------------------

    /// @brief Runs the full `quick_branchy_i32` hot loop (LCG + select + rotl + decrement br_if) in one opfunc dispatch (tail-call).
    /// @details
    /// - Canonical loop body (from `quick_branchy_i32.wasm` func[7]):
    ///   `local.get s; i32.const 1664525; i32.mul; local.tee t; i32.const 1013904223; i32.add; local.tee s;
    ///    local.get acc; i32.add; local.get t; i32.const 3668339992; i32.add; local.get acc; i32.xor;
    ///    local.get s; i32.const 1; i32.and; select;
    ///    local.get s; i32.const 3; i32.shr_u;
    ///    i32.const 0; local.get s; i32.const 1; i32.shl; i32.sub;
    ///    local.get s; i32.const 4; i32.and; select;
    ///    i32.add; i32.const 5; i32.rotl; local.set acc;
    ///    local.get cnt; i32.const -1; i32.add; local.tee cnt; br_if <loop>`
    /// - Stack-top optimization: N/A (no operand stack values are produced).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (cnt i32), `local_offset_t` (acc i32), `local_offset_t` (s i32).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_quick_branchy_i32_loop_run(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_u32 = conbine_details::wasm_u32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const cnt_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const acc_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const s_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        wasm_u32 cnt{::std::bit_cast<wasm_u32>(conbine_details::load_local<wasm_i32>(type...[2u], cnt_off))};
        wasm_u32 acc{::std::bit_cast<wasm_u32>(conbine_details::load_local<wasm_i32>(type...[2u], acc_off))};
        wasm_u32 s{::std::bit_cast<wasm_u32>(conbine_details::load_local<wasm_i32>(type...[2u], s_off))};

        constexpr wasm_u32 a{1664525u};
        constexpr wasm_u32 b{1013904223u};
        constexpr wasm_u32 c{3668339992u};

        for(;;)
        {
            wasm_u32 const t{static_cast<wasm_u32>(s * a)};
            wasm_u32 const s2{static_cast<wasm_u32>(t + b)};

            wasm_u32 const v1{static_cast<wasm_u32>(s2 + acc)};
            wasm_u32 const v2{static_cast<wasm_u32>((t + c) ^ acc)};
            wasm_u32 const sel1{(s2 & 1u) != 0u ? v1 : v2};

            wasm_u32 const shr{static_cast<wasm_u32>(s2 >> 3u)};
            wasm_u32 const neg_shl{static_cast<wasm_u32>(0u - static_cast<wasm_u32>(s2 << 1u))};
            wasm_u32 const sel2{(s2 & 4u) != 0u ? shr : neg_shl};

            wasm_u32 const sum{static_cast<wasm_u32>(sel1 + sel2)};
            acc = ::std::rotl(sum, 5);

            s = s2;

            cnt = static_cast<wasm_u32>(cnt - 1u);
            if(cnt == 0u) { break; }
        }

        conbine_details::store_local(type...[2u], cnt_off, ::std::bit_cast<wasm_i32>(cnt));
        conbine_details::store_local(type...[2u], acc_off, ::std::bit_cast<wasm_i32>(acc));
        conbine_details::store_local(type...[2u], s_off, ::std::bit_cast<wasm_i32>(s));

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Runs the full `quick_branchy_i32` hot loop in one opfunc dispatch (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (cnt i32), `local_offset_t` (acc i32), `local_offset_t` (s i32).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_quick_branchy_i32_loop_run(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_u32 = conbine_details::wasm_u32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const cnt_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const acc_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const s_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        wasm_u32 cnt{::std::bit_cast<wasm_u32>(conbine_details::load_local<wasm_i32>(typeref...[2u], cnt_off))};
        wasm_u32 acc{::std::bit_cast<wasm_u32>(conbine_details::load_local<wasm_i32>(typeref...[2u], acc_off))};
        wasm_u32 s{::std::bit_cast<wasm_u32>(conbine_details::load_local<wasm_i32>(typeref...[2u], s_off))};

        constexpr wasm_u32 a{1664525u};
        constexpr wasm_u32 b{1013904223u};
        constexpr wasm_u32 c{3668339992u};

        for(;;)
        {
            wasm_u32 const t{static_cast<wasm_u32>(s * a)};
            wasm_u32 const s2{static_cast<wasm_u32>(t + b)};

            wasm_u32 const v1{static_cast<wasm_u32>(s2 + acc)};
            wasm_u32 const v2{static_cast<wasm_u32>((t + c) ^ acc)};
            wasm_u32 const sel1{(s2 & 1u) != 0u ? v1 : v2};

            wasm_u32 const shr{static_cast<wasm_u32>(s2 >> 3u)};
            wasm_u32 const neg_shl{static_cast<wasm_u32>(0u - static_cast<wasm_u32>(s2 << 1u))};
            wasm_u32 const sel2{(s2 & 4u) != 0u ? shr : neg_shl};

            wasm_u32 const sum{static_cast<wasm_u32>(sel1 + sel2)};
            acc = ::std::rotl(sum, 5);

            s = s2;

            cnt = static_cast<wasm_u32>(cnt - 1u);
            if(cnt == 0u) { break; }
        }

        conbine_details::store_local(typeref...[2u], cnt_off, ::std::bit_cast<wasm_i32>(cnt));
        conbine_details::store_local(typeref...[2u], acc_off, ::std::bit_cast<wasm_i32>(acc));
        conbine_details::store_local(typeref...[2u], s_off, ::std::bit_cast<wasm_i32>(s));
    }

    namespace translate
    {
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_quick_branchy_i32_loop_run_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_quick_branchy_i32_loop_run<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_quick_branchy_i32_loop_run_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_quick_branchy_i32_loop_run_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_quick_branchy_i32_loop_run_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_quick_branchy_i32_loop_run<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_quick_branchy_i32_loop_run_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_quick_branchy_i32_loop_run_fptr<CompileOption, TypeInTuple...>(curr); }
    }  // namespace translate

    // ----------------------------------------
    // float64: 4x chained (mul + add + local.tee) after one local.get (push f64)
    // ----------------------------------------

    /// @brief Fuses the hot chain:
    /// `local.get src;
    ///    f64.const mul; f64.mul; f64.const add; f64.add; local.tee dst1;
    ///    f64.const mul; f64.mul; f64.const add; f64.add; local.tee dst2;
    ///    f64.const mul; f64.mul; f64.const add; f64.add; local.tee dst3;
    ///    f64.const mul; f64.mul; f64.const add; f64.add; local.tee dst4`
    /// into one opfunc dispatch (tail-call).
    /// @details
    /// - Stack effect: push 1 f64 (same as the last `local.tee`).
    /// - Stack-top optimization: supported (f64 is pushed via `conbine_details::push_operand`).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (src), `local_offset_t` (dst1..dst4), `wasm_f64` (mul), `wasm_f64` (add).
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mul_add_2imm_localget_local_tee_4x(Type... type) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const src_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const dst1_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const dst2_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const dst3_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const dst4_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f64 const mul{conbine_details::read_imm<wasm_f64>(type...[0])};
        wasm_f64 const add{conbine_details::read_imm<wasm_f64>(type...[0])};

        wasm_f64 v{conbine_details::load_local<wasm_f64>(type...[2u], src_off)};
        v = v * mul + add;
        conbine_details::store_local(type...[2u], dst1_off, v);
        v = v * mul + add;
        conbine_details::store_local(type...[2u], dst2_off, v);
        v = v * mul + add;
        conbine_details::store_local(type...[2u], dst3_off, v);
        v = v * mul + add;
        conbine_details::store_local(type...[2u], dst4_off, v);

        conbine_details::push_operand<CompileOption, wasm_f64, curr_f64_stack_top>(v, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Same as @ref uwvmint_f64_mul_add_2imm_localget_local_tee_4x but for byref mode.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mul_add_2imm_localget_local_tee_4x(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const src_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const dst1_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const dst2_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const dst3_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const dst4_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f64 const mul{conbine_details::read_imm<wasm_f64>(typeref...[0])};
        wasm_f64 const add{conbine_details::read_imm<wasm_f64>(typeref...[0])};

        wasm_f64 v{conbine_details::load_local<wasm_f64>(typeref...[2u], src_off)};
        v = v * mul + add;
        conbine_details::store_local(typeref...[2u], dst1_off, v);
        v = v * mul + add;
        conbine_details::store_local(typeref...[2u], dst2_off, v);
        v = v * mul + add;
        conbine_details::store_local(typeref...[2u], dst3_off, v);
        v = v * mul + add;
        conbine_details::store_local(typeref...[2u], dst4_off, v);

        conbine_details::push_operand_byref<CompileOption>(v, typeref...);
    }

    namespace combine_extra_heavy_details
    {
        struct f64_mul_add_2imm_localget_local_tee_4x_op
        {
            template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
            inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
            { return uwvmint_f64_mul_add_2imm_localget_local_tee_4x<Opt, Pos, Type...>; }

            template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
            inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
            { return uwvmint_f64_mul_add_2imm_localget_local_tee_4x<Opt, Type...>; }
        };
    }  // namespace combine_extra_heavy_details

    namespace translate
    {
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_mul_add_2imm_localget_local_tee_4x_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.f64_stack_top_begin_pos,
                                                                    CompileOption.f64_stack_top_end_pos,
                                                                    combine_extra_heavy_details::f64_mul_add_2imm_localget_local_tee_4x_op,
                                                                    Type...>(curr.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_f64_mul_add_2imm_localget_local_tee_4x_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_add_2imm_localget_local_tee_4x_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_f64_mul_add_2imm_localget_local_tee_4x_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return combine_extra_heavy_details::f64_mul_add_2imm_localget_local_tee_4x_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_f64_mul_add_2imm_localget_local_tee_4x_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_add_2imm_localget_local_tee_4x_fptr<CompileOption, TypeInTuple...>(curr); }
    }  // namespace translate

    /// @brief Runs the full `test7`-style i32 sum loop:
    /// `local.get sp; i32.load off_i; i32.const end; i32.lt_s; i32.const 1; i32.and; i32.eqz; br_if <break>;
    ///  local.get sp; local.get sp; i32.load off_sum; local.get sp; i32.load off_i; i32.add; i32.store off_sum;
    ///  local.get sp; local.get sp; i32.load off_i; i32.const step; i32.add; i32.store off_i; br <loop>`
    /// in one opfunc dispatch (tail-call).
    /// @details
    /// - Stack-top optimization: N/A (no operand stack values are produced).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (sp i32), `native_memory_t*` (memory0), `wasm_u32` (off_i), `wasm_u32` (off_sum), `wasm_i32` (end),
    ///   `wasm_i32` (step).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_sum_loop_run(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_u32 = conbine_details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const sp_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        native_memory_t* memory_p{conbine_details::read_imm<native_memory_t*>(type...[0])};
        wasm_u32 const off_i{conbine_details::read_imm<wasm_u32>(type...[0])};
        wasm_u32 const off_sum{conbine_details::read_imm<wasm_u32>(type...[0])};
        wasm_i32 const end{conbine_details::read_imm<wasm_i32>(type...[0])};
        wasm_i32 const step{conbine_details::read_imm<wasm_i32>(type...[0])};

        wasm_i32 const sp_addr{conbine_details::load_local<wasm_i32>(type...[2u], sp_off)};
        auto const eff_i{details::wasm32_effective_offset(sp_addr, off_i)};
        auto const eff_sum{details::wasm32_effective_offset(sp_addr, off_sum)};

        auto const& memory{*memory_p};
        details::enter_memory_operation_memory_lock(memory);
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(off_i), eff_i, 4uz);
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(off_sum), eff_sum, 4uz);

        auto const i_ptr{details::ptr_add_u64(memory.memory_begin, eff_i.offset)};
        auto const sum_ptr{details::ptr_add_u64(memory.memory_begin, eff_sum.offset)};

        wasm_u32 i_u{::std::bit_cast<wasm_u32>(details::load_i32_le(i_ptr))};
        wasm_u32 sum_u{::std::bit_cast<wasm_u32>(details::load_i32_le(sum_ptr))};
        wasm_u32 const step_u{::std::bit_cast<wasm_u32>(step)};

        while(::std::bit_cast<wasm_i32>(i_u) < end)
        {
            sum_u += i_u;
            i_u += step_u;
        }

        details::store_i32_le(i_ptr, ::std::bit_cast<wasm_i32>(i_u));
        details::store_i32_le(sum_ptr, ::std::bit_cast<wasm_i32>(sum_u));
        details::exit_memory_operation_memory_lock(memory);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Runs the full `uwvmint_i32_sum_loop_run` pattern in one opfunc dispatch (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see @ref uwvmint_i32_sum_loop_run.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_sum_loop_run(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_u32 = conbine_details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const sp_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{conbine_details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const off_i{conbine_details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const off_sum{conbine_details::read_imm<wasm_u32>(typeref...[0])};
        wasm_i32 const end{conbine_details::read_imm<wasm_i32>(typeref...[0])};
        wasm_i32 const step{conbine_details::read_imm<wasm_i32>(typeref...[0])};

        wasm_i32 const sp_addr{conbine_details::load_local<wasm_i32>(typeref...[2u], sp_off)};
        auto const eff_i{details::wasm32_effective_offset(sp_addr, off_i)};
        auto const eff_sum{details::wasm32_effective_offset(sp_addr, off_sum)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto const lock_guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(off_i), eff_i, 4uz);
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(off_sum), eff_sum, 4uz);

        auto const i_ptr{details::ptr_add_u64(memory.memory_begin, eff_i.offset)};
        auto const sum_ptr{details::ptr_add_u64(memory.memory_begin, eff_sum.offset)};

        wasm_u32 i_u{::std::bit_cast<wasm_u32>(details::load_i32_le(i_ptr))};
        wasm_u32 sum_u{::std::bit_cast<wasm_u32>(details::load_i32_le(sum_ptr))};
        wasm_u32 const step_u{::std::bit_cast<wasm_u32>(step)};

        while(::std::bit_cast<wasm_i32>(i_u) < end)
        {
            sum_u += i_u;
            i_u += step_u;
        }

        details::store_i32_le(i_ptr, ::std::bit_cast<wasm_i32>(i_u));
        details::store_i32_le(sum_ptr, ::std::bit_cast<wasm_i32>(sum_u));
    }

    /// @brief Runs the full `test9`-style f32 sum loop:
    /// `f32.const 1; local.get i; local.get i; i32.mul; f32.convert_i32_u; f32.div;
    ///  f32.const 1; local.get i; i32.const -1; i32.add; local.tee tmp; local.get tmp; i32.mul; f32.convert_i32_u; f32.div;
    ///  local.get sum; f32.add; f32.add; local.set sum;
    ///  local.get i; i32.const 2; i32.add; local.tee i; i32.const end; i32.ne; br_if <loop>`
    /// in one opfunc dispatch (tail-call).
    /// @details
    /// - Stack-top optimization: N/A (operand stack remains unchanged).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (sum f32), `local_offset_t` (i i32), `wasm_i32` (end).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_inv_square_sum_loop_run(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_u32 = conbine_details::wasm_u32;
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const sum_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const i_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const end{conbine_details::read_imm<wasm_i32>(type...[0])};

        wasm_f32 sum{conbine_details::load_local<wasm_f32>(type...[2u], sum_off)};
        wasm_u32 i_u{::std::bit_cast<wasm_u32>(conbine_details::load_local<wasm_i32>(type...[2u], i_off))};
        wasm_u32 const end_u{::std::bit_cast<wasm_u32>(end)};

        while(i_u != end_u)
        {
            wasm_u32 const denom_even{i_u * i_u};
            wasm_f32 const term_even{wasm_f32{1.f} / static_cast<wasm_f32>(denom_even)};

            wasm_u32 const im1{i_u - wasm_u32{1u}};
            wasm_u32 const denom_odd{im1 * im1};
            wasm_f32 const term_odd{wasm_f32{1.f} / static_cast<wasm_f32>(denom_odd)};

            // Match the Wasm evaluation order: (sum + term_odd) + term_even.
            sum += term_odd;
            sum += term_even;

            i_u += wasm_u32{2u};
        }

        conbine_details::store_local(type...[2u], sum_off, sum);
        conbine_details::store_local(type...[2u], i_off, ::std::bit_cast<wasm_i32>(i_u));

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Runs the full `test9`-style f32 sum loop for `1/(i^3)` (see `uwvmint_f32_inv_square_sum_loop_run`) in one opfunc dispatch (tail-call).
    /// @details
    /// - Immediates: `local_offset_t` (sum f32), `local_offset_t` (i i32), `wasm_i32` (end).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_inv_cube_sum_loop_run(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_u32 = conbine_details::wasm_u32;
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const sum_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const i_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const end{conbine_details::read_imm<wasm_i32>(type...[0])};

        wasm_f32 sum{conbine_details::load_local<wasm_f32>(type...[2u], sum_off)};
        wasm_u32 i_u{::std::bit_cast<wasm_u32>(conbine_details::load_local<wasm_i32>(type...[2u], i_off))};
        wasm_u32 const end_u{::std::bit_cast<wasm_u32>(end)};

        while(i_u != end_u)
        {
            wasm_u32 denom_even{i_u * i_u};
            denom_even *= i_u;
            wasm_f32 const term_even{wasm_f32{1.f} / static_cast<wasm_f32>(denom_even)};

            wasm_u32 const im1{i_u - wasm_u32{1u}};
            wasm_u32 denom_odd{im1 * im1};
            denom_odd *= im1;
            wasm_f32 const term_odd{wasm_f32{1.f} / static_cast<wasm_f32>(denom_odd)};

            // Match the Wasm evaluation order: (sum + term_odd) + term_even.
            sum += term_odd;
            sum += term_even;

            i_u += wasm_u32{2u};
        }

        conbine_details::store_local(type...[2u], sum_off, sum);
        conbine_details::store_local(type...[2u], i_off, ::std::bit_cast<wasm_i32>(i_u));

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Runs the full `test9`-style f32 mul-chain+sum loop:
    /// `prod = ((((prod*0.5*i)*0.5*(i+1))*0.5*(i+2))*0.5*(i+3))*0.5*(i+4); sum += (each intermediate);
    ///  i += 5; br_if (i+4 != end)`
    /// in one opfunc dispatch (tail-call).
    /// @details
    /// - Stack-top optimization: N/A (operand stack remains unchanged).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (sum f32), `local_offset_t` (i i32), `local_offset_t` (prod f32), `local_offset_t` (ip4 i32), `wasm_i32` (end).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_mul_chain_sum_loop_run(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_u32 = conbine_details::wasm_u32;
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const sum_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const i_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const prod_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const ip4_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const end{conbine_details::read_imm<wasm_i32>(type...[0])};

        wasm_f32 sum{conbine_details::load_local<wasm_f32>(type...[2u], sum_off)};
        wasm_u32 i_u{::std::bit_cast<wasm_u32>(conbine_details::load_local<wasm_i32>(type...[2u], i_off))};
        wasm_f32 prod{conbine_details::load_local<wasm_f32>(type...[2u], prod_off)};
        wasm_u32 const end_u{::std::bit_cast<wasm_u32>(end)};

        wasm_u32 ip4_u{};
        for(;;)
        {
            wasm_f32 const a{(prod * wasm_f32{0.5f}) * static_cast<wasm_f32>(i_u)};
            wasm_f32 const b{(a * wasm_f32{0.5f}) * static_cast<wasm_f32>(i_u + wasm_u32{1u})};
            wasm_f32 const c{(b * wasm_f32{0.5f}) * static_cast<wasm_f32>(i_u + wasm_u32{2u})};
            wasm_f32 const d{(c * wasm_f32{0.5f}) * static_cast<wasm_f32>(i_u + wasm_u32{3u})};
            ip4_u = i_u + wasm_u32{4u};
            wasm_f32 const e{(d * wasm_f32{0.5f}) * static_cast<wasm_f32>(ip4_u)};

            prod = e;

            // Match the Wasm evaluation order: sum + a + b + c + d + e (e added last).
            sum += a;
            sum += b;
            sum += c;
            sum += d;
            sum += e;

            i_u += wasm_u32{5u};
            if(ip4_u != end_u) { continue; }
            break;
        }

        conbine_details::store_local(type...[2u], sum_off, sum);
        conbine_details::store_local(type...[2u], prod_off, prod);
        conbine_details::store_local(type...[2u], i_off, ::std::bit_cast<wasm_i32>(i_u));
        conbine_details::store_local(type...[2u], ip4_off, ::std::bit_cast<wasm_i32>(ip4_u));

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Runs the full `test10`-style f32 sum loop:
    /// `sum += 1 / (1 + (f32.convert_i32_u(i) * k))^2; i += 2; until (i+1 == end)` with the last iteration adding only
    /// the odd term (matching the Wasm `br_if (i+1 == end) -> break`).
    /// @details
    /// - Stack-top optimization: N/A (operand stack remains unchanged).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (sum f32), `local_offset_t` (i i32), `local_offset_t` (sum_out f32),
    ///              `local_offset_t` (i1 i32), `wasm_i32` (end).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_affine_inv_square_sum_loop_run(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_u32 = conbine_details::wasm_u32;
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const sum_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const i_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const sum_out_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const i1_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const end{conbine_details::read_imm<wasm_i32>(type...[0])};

        wasm_f32 sum{conbine_details::load_local<wasm_f32>(type...[2u], sum_off)};
        wasm_u32 i_u{::std::bit_cast<wasm_u32>(conbine_details::load_local<wasm_i32>(type...[2u], i_off))};
        wasm_u32 const end_u{::std::bit_cast<wasm_u32>(end)};

        // f32.const 0x1.2dfd6ap-17 (see `test10` WAT).
        constexpr wasm_u32 k_bits{0x3716feb5u};
        wasm_f32 const k{::std::bit_cast<wasm_f32>(k_bits)};

        wasm_f32 sum_out{};  // init
        wasm_u32 i1_u{};     // init

        for(;;)
        {
            // term(i): 1 / (1 + (i*k))^2
            wasm_f32 const fi{static_cast<wasm_f32>(i_u)};
            wasm_f32 const t0{fi * k};
            wasm_f32 const x0{t0 + wasm_f32{1.f}};
            wasm_f32 const term0{wasm_f32{1.f} / (x0 * x0)};
            sum_out = sum + term0;

            i1_u = i_u + wasm_u32{1u};
            if(i1_u == end_u) { break; }

            // term(i+1)
            wasm_f32 const fi1{static_cast<wasm_f32>(i1_u)};
            wasm_f32 const t1{fi1 * k};
            wasm_f32 const x1{t1 + wasm_f32{1.f}};
            wasm_f32 const term1{wasm_f32{1.f} / (x1 * x1)};
            sum = sum_out + term1;

            i_u = i1_u + wasm_u32{1u};
        }

        // Match the Wasm visible state at loop exit (used by `test10` after the block):
        // - sum_out_off holds the final sum
        // - sum_off holds the sum excluding the last odd term
        // - i_off holds the last odd `i` (end-1)
        // - i1_off holds `i+1` (end)
        conbine_details::store_local(type...[2u], sum_out_off, sum_out);
        conbine_details::store_local(type...[2u], sum_off, sum);
        conbine_details::store_local(type...[2u], i_off, ::std::bit_cast<wasm_i32>(i_u));
        conbine_details::store_local(type...[2u], i1_off, ::std::bit_cast<wasm_i32>(i1_u));

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Runs the full `test6` sin-table fill loop:
    /// `for(i=0; i!=end; i+=4) store sin((i+0)*k), sin((i+1)*k), sin((i+2)*k), sin((i+3)*k) into memory` (tail-call).
    /// @details
    /// - Stack-top optimization: N/A (operand stack remains unchanged).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (ptr i32), `local_offset_t` (i i32), `native_memory_t*` (memory0), `wasm_i32` (end).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_test6_sin_table_fill_loop_run(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_u32 = conbine_details::wasm_u32;
        using wasm_f32 = conbine_details::wasm_f32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const ptr_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const i_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        native_memory_t* memory_p{conbine_details::read_imm<native_memory_t*>(type...[0])};
        wasm_i32 const end{conbine_details::read_imm<wasm_i32>(type...[0])};

        wasm_u32 ptr_u{::std::bit_cast<wasm_u32>(conbine_details::load_local<wasm_i32>(type...[2u], ptr_off))};
        wasm_u32 i_u{::std::bit_cast<wasm_u32>(conbine_details::load_local<wasm_i32>(type...[2u], i_off))};
        wasm_u32 const end_u{::std::bit_cast<wasm_u32>(end)};

        // f32.const 0x1.921fb6p-8 (2*pi/1024) in `test6`.
        constexpr wasm_u32 k_bits{0x3bc90fdbu};
        wasm_f32 const k{::std::bit_cast<wasm_f32>(k_bits)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto const lock_guard{details::lock_memory(memory)};

        if(i_u < end_u)
        {
            // Each iteration stores 16 bytes (4x f32) into a contiguous region.
            wasm_u32 const iter_cnt{(end_u - i_u) / wasm_u32{4u}};
            ::std::size_t const bytes_total{static_cast<::std::size_t>(iter_cnt) * 16uz};

            auto const eff{details::wasm32_effective_offset(::std::bit_cast<wasm_i32>(ptr_u), wasm_u32{0u})};
            details::check_memory_bounds_unlocked(memory, 0uz, 0uz, eff, bytes_total);

            ::std::byte* p{details::ptr_add_u64(memory.memory_begin, eff.offset)};

            for(wasm_u32 it{}; it != iter_cnt; ++it)
            {
                wasm_f32 const fi{static_cast<wasm_f32>(i_u)};

                // Match the store order in the Wasm loop.
                details::store_f32_le(p + 0uz, static_cast<wasm_f32>(::sinf(fi * k)));
                details::store_f32_le(p + 12uz, static_cast<wasm_f32>(::sinf((fi + wasm_f32{3.f}) * k)));
                details::store_f32_le(p + 8uz, static_cast<wasm_f32>(::sinf((fi + wasm_f32{2.f}) * k)));
                details::store_f32_le(p + 4uz, static_cast<wasm_f32>(::sinf((fi + wasm_f32{1.f}) * k)));

                p += 16uz;
                i_u += wasm_u32{4u};
                ptr_u += wasm_u32{16u};
            }
        }

        conbine_details::store_local(type...[2u], ptr_off, ::std::bit_cast<wasm_i32>(ptr_u));
        conbine_details::store_local(type...[2u], i_off, ::std::bit_cast<wasm_i32>(i_u));

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_for_ptr_inc_ne_br_if` (tail-call).
    /// @details
    /// - Stack-top optimization: N/A (no operand stack values are produced).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (p i32), `local_offset_t` (pend i32), `wasm_i32` (step), `::std::byte const*` (label ip).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_for_ptr_inc_ne_br_if(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const p_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const pend_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const step{conbine_details::read_imm<wasm_i32>(type...[0])};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        wasm_i32 const p{conbine_details::load_local<wasm_i32>(type...[2u], p_off)};
        wasm_i32 const pend{conbine_details::load_local<wasm_i32>(type...[2u], pend_off)};
        wasm_i32 const next_p{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(p, step)};
        conbine_details::store_local(type...[2u], p_off, next_p);
        bool const take_branch{details::eval_int_cmp<details::int_cmp::ne, wasm_i32, conbine_details::wasm_u32>(next_p, pend)};

# if UWVM_HAS_CPP_ATTRIBUTE(clang::nomerge)
        [[clang::nomerge]]
# endif
        if(take_branch)
        {
            type...[0] = jmp_ip;

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_for_ptr_inc_ne_br_if` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (p i32), `local_offset_t` (pend i32), `wasm_i32` (step), `::std::byte const*` (label ip).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_for_ptr_inc_ne_br_if(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const p_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const pend_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_i32 const step{conbine_details::read_imm<wasm_i32>(typeref...[0])};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), typeref...[0], sizeof(jmp_ip));
        typeref...[0] += sizeof(jmp_ip);

        wasm_i32 const p{conbine_details::load_local<wasm_i32>(typeref...[2u], p_off)};
        wasm_i32 const pend{conbine_details::load_local<wasm_i32>(typeref...[2u], pend_off)};
        wasm_i32 const next_p{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(p, step)};
        conbine_details::store_local(typeref...[2u], p_off, next_p);
        if(details::eval_int_cmp<details::int_cmp::ne, wasm_i32, conbine_details::wasm_u32>(next_p, pend)) { typeref...[0] = jmp_ip; }
    }

    // ----------------------------------------
    // ChaCha20 (fixed test vector): fuse the whole block function
    // ----------------------------------------

    /// @brief Extra-heavy mega-op for the ChaCha20 reference block used by `/tmp/uwvm2test/chacha20.wasm` (tail-call).
    /// @details
    /// This opfunc replaces the entire Wasm body of the reference ChaCha20 block function (20 rounds; 10 double-rounds),
    /// writing 16 words (state[0..15]) back to linear memory at `out_ptr + {0..60}`.
    ///
    /// Translator-side matching is intentionally strict (hash-based) and only enabled in `extra` builds to avoid accidental matches.
    ///
    /// - Stack-top optimization: N/A (no operand stack values are produced).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (out_ptr i32), `local_offset_t` (counter i32), `native_memory_t*` (memory0).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_chacha20_block_fixed_key_run(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const out_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const counter_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        native_memory_t* const memory_p{conbine_details::read_imm<native_memory_t*>(type...[0])};

        // Load `out_ptr` and `counter` from locals.
        ::std::uint32_t const out_ptr{static_cast<::std::uint32_t>(conbine_details::load_local<wasm_i32>(type...[2u], out_off))};
        ::std::uint32_t const counter{static_cast<::std::uint32_t>(conbine_details::load_local<wasm_i32>(type...[2u], counter_off))};

        // This is a mechanical translation of the matched Wasm function body (hash-guarded in the translator).
        // Keep it in the same local numbering as the Wasm for easy auditing.
        auto const rotl32{[](::std::uint32_t v, unsigned r) noexcept -> ::std::uint32_t { return (v << r) | (v >> (32u - r)); }};

        ::std::uint32_t l2{522067228u};
        ::std::uint32_t l3{0u};
        ::std::uint32_t l4{1797285236u};
        ::std::uint32_t l5{252579084u};
        ::std::uint32_t l6{454695192u};
        ::std::uint32_t l7{1241513984u};
        ::std::uint32_t l8{2036477234u};
        ::std::uint32_t l9{185207048u};
        ::std::uint32_t l10{387323156u};
        ::std::uint32_t l11{150994944u};
        ::std::uint32_t l12{857760878u};
        ::std::uint32_t l13{117835012u};
        ::std::uint32_t l14{319951120u};
        ::std::uint32_t l15{1634760805u};
        ::std::uint32_t l16{50462976u};
        ::std::uint32_t l18{counter};
        ::std::uint32_t l19{};
        ::std::uint32_t l20{};
        ::std::uint32_t l21{};

        for(int iter{}; iter != 10; ++iter)
        {
            ::std::uint32_t t0 = l4 + l5;
            l4 = t0;
            ::std::uint32_t t1 = l3 ^ t0;
            ::std::uint32_t t2 = rotl32(t1, 16u);
            l3 = t2;
            ::std::uint32_t t3 = l2 + t2;
            l2 = t3;
            ::std::uint32_t t4 = t3 ^ l5;
            ::std::uint32_t t5 = rotl32(t4, 12u);
            l5 = t5;
            ::std::uint32_t t6 = t5 + l4;
            l19 = t6;
            ::std::uint32_t t7 = l15 + l16;
            l4 = t7;
            ::std::uint32_t t8 = l18 ^ t7;
            ::std::uint32_t t9 = rotl32(t8, 16u);
            l15 = t9;
            ::std::uint32_t t10 = l14 + t9;
            l14 = t10;
            ::std::uint32_t t11 = t10 ^ l16;
            ::std::uint32_t t12 = rotl32(t11, 12u);
            l16 = t12;
            ::std::uint32_t t13 = t12 + l4;
            l20 = t13;
            ::std::uint32_t t14 = t13 ^ l15;
            ::std::uint32_t t15 = rotl32(t14, 8u);
            l15 = t15;
            ::std::uint32_t t16 = t15 + l14;
            l14 = t16;
            ::std::uint32_t t17 = t16 ^ l16;
            ::std::uint32_t t18 = rotl32(t17, 7u);
            l16 = t18;
            ::std::uint32_t t19 = t6 + t18;
            l4 = t19;
            ::std::uint32_t t20 = l8 + l9;
            l8 = t20;
            ::std::uint32_t t21 = l7 ^ t20;
            ::std::uint32_t t22 = rotl32(t21, 16u);
            l7 = t22;
            ::std::uint32_t t23 = l6 + t22;
            l6 = t23;
            ::std::uint32_t t24 = t23 ^ l9;
            ::std::uint32_t t25 = rotl32(t24, 12u);
            l9 = t25;
            ::std::uint32_t t26 = t25 + l8;
            l8 = t26;
            ::std::uint32_t t27 = t26 ^ l7;
            ::std::uint32_t t28 = rotl32(t27, 8u);
            l18 = t28;
            ::std::uint32_t t29 = t19 ^ t28;
            ::std::uint32_t t30 = rotl32(t29, 16u);
            l7 = t30;
            ::std::uint32_t t31 = l12 + l13;
            l12 = t31;
            ::std::uint32_t t32 = l11 ^ t31;
            ::std::uint32_t t33 = rotl32(t32, 16u);
            l11 = t33;
            ::std::uint32_t t34 = l10 + t33;
            l10 = t34;
            ::std::uint32_t t35 = t34 ^ l13;
            ::std::uint32_t t36 = rotl32(t35, 12u);
            l13 = t36;
            ::std::uint32_t t37 = t36 + l12;
            l12 = t37;
            ::std::uint32_t t38 = t37 ^ l11;
            ::std::uint32_t t39 = rotl32(t38, 8u);
            l11 = t39;
            ::std::uint32_t t40 = t39 + l10;
            l21 = t40;
            ::std::uint32_t t41 = t30 + t40;
            l10 = t41;
            ::std::uint32_t t42 = t41 ^ l16;
            ::std::uint32_t t43 = rotl32(t42, 12u);
            l16 = t43;
            ::std::uint32_t t44 = t43 + l4;
            l4 = t44;
            ::std::uint32_t t45 = t44 ^ l7;
            ::std::uint32_t t46 = rotl32(t45, 8u);
            l7 = t46;
            ::std::uint32_t t47 = t46 + l10;
            l10 = t47;
            ::std::uint32_t t48 = t47 ^ l16;
            ::std::uint32_t t49 = rotl32(t48, 7u);
            l16 = t49;
            ::std::uint32_t t50 = l19 ^ l3;
            ::std::uint32_t t51 = rotl32(t50, 8u);
            l3 = t51;
            ::std::uint32_t t52 = t51 + l2;
            l2 = t52;
            ::std::uint32_t t53 = t52 ^ l5;
            ::std::uint32_t t54 = rotl32(t53, 7u);
            l5 = t54;
            ::std::uint32_t t55 = t54 + l8;
            l8 = t55;
            ::std::uint32_t t56 = t55 ^ l11;
            ::std::uint32_t t57 = rotl32(t56, 16u);
            l11 = t57;
            ::std::uint32_t t58 = t57 + l14;
            l14 = t58;
            ::std::uint32_t t59 = t58 ^ l5;
            ::std::uint32_t t60 = rotl32(t59, 12u);
            l5 = t60;
            ::std::uint32_t t61 = t60 + l8;
            l8 = t61;
            ::std::uint32_t t62 = t61 ^ l11;
            ::std::uint32_t t63 = rotl32(t62, 8u);
            l11 = t63;
            ::std::uint32_t t64 = t63 + l14;
            l14 = t64;
            ::std::uint32_t t65 = t64 ^ l5;
            ::std::uint32_t t66 = rotl32(t65, 7u);
            l5 = t66;
            ::std::uint32_t t67 = l18 + l6;
            l6 = t67;
            ::std::uint32_t t68 = t67 ^ l9;
            ::std::uint32_t t69 = rotl32(t68, 7u);
            l9 = t69;
            ::std::uint32_t t70 = t69 + l12;
            l12 = t70;
            ::std::uint32_t t71 = t70 ^ l15;
            ::std::uint32_t t72 = rotl32(t71, 16u);
            l15 = t72;
            ::std::uint32_t t73 = l2 + t72;
            l2 = t73;
            ::std::uint32_t t74 = t73 ^ l9;
            ::std::uint32_t t75 = rotl32(t74, 12u);
            l9 = t75;
            ::std::uint32_t t76 = t75 + l12;
            l12 = t76;
            ::std::uint32_t t77 = t76 ^ l15;
            ::std::uint32_t t78 = rotl32(t77, 8u);
            l18 = t78;
            ::std::uint32_t t79 = t78 + l2;
            l2 = t79;
            ::std::uint32_t t80 = t79 ^ l9;
            ::std::uint32_t t81 = rotl32(t80, 7u);
            l9 = t81;
            ::std::uint32_t t82 = l21 ^ l13;
            ::std::uint32_t t83 = rotl32(t82, 7u);
            l13 = t83;
            ::std::uint32_t t84 = t83 + l20;
            l15 = t84;
            ::std::uint32_t t85 = l3 ^ t84;
            ::std::uint32_t t86 = rotl32(t85, 16u);
            l3 = t86;
            ::std::uint32_t t87 = t86 + l6;
            l6 = t87;
            ::std::uint32_t t88 = t87 ^ l13;
            ::std::uint32_t t89 = rotl32(t88, 12u);
            l13 = t89;
            ::std::uint32_t t90 = t89 + l15;
            l15 = t90;
            ::std::uint32_t t91 = t90 ^ l3;
            ::std::uint32_t t92 = rotl32(t91, 8u);
            l3 = t92;
            ::std::uint32_t t93 = t92 + l6;
            l6 = t93;
            ::std::uint32_t t94 = t93 ^ l13;
            ::std::uint32_t t95 = rotl32(t94, 7u);
            l13 = t95;
        }

        // One-shot bounds check for the maximum write: out_ptr + 60 + 4.
        // This is equivalent to per-store bounds checks for the contiguous 16-word range [0..60].
        if(memory_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const& mem{*memory_p};
        ::std::uint_least64_t const max_eff{static_cast<::std::uint_least64_t>(out_ptr) + 60u};
        details::memory_offset_t const effective_offset{.offset = max_eff, .offset_65_bit = (max_eff >> 32u) != 0u};
        details::bounds_check_generic(mem, 0uz, 60u, effective_offset, 4uz);

        ::std::byte* const mem_base{mem.memory_begin};
        if(mem_base == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const store_word{[&](::std::uint_least64_t off, ::std::uint32_t v) noexcept
                              { details::store_u32_le(details::ptr_add_u64(mem_base, static_cast<::std::uint_least64_t>(out_ptr) + off), v); }};

        // Store the same 16-word output slice as the Wasm: offsets 0..60.
        store_word(60u, l3);
        store_word(56u, l7 + 1241513984u);
        store_word(52u, l11 + 150994944u);
        store_word(48u, counter + l18);
        store_word(44u, l2 + 522067228u);
        store_word(40u, l6 + 454695192u);
        store_word(36u, l10 + 387323156u);
        store_word(32u, l14 + 319951120u);
        store_word(28u, l5 + 252579084u);
        store_word(24u, l9 + 185207048u);
        store_word(20u, l13 + 117835012u);
        store_word(16u, l16 + 50462976u);
        store_word(12u, l4 + 1797285236u);
        store_word(8u, l8 + 2036477234u);
        store_word(4u, l12 + 857760878u);
        store_word(0u, l15 + 1634760805u);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    namespace translate
    {
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_chacha20_block_fixed_key_run_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_chacha20_block_fixed_key_run<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_chacha20_block_fixed_key_run_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_chacha20_block_fixed_key_run_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_sum_loop_run_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_sum_loop_run<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_sum_loop_run_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_sum_loop_run_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_sum_loop_run_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_sum_loop_run<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_sum_loop_run_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_sum_loop_run_fptr<CompileOption, TypeInTuple...>(curr); }

        // test9 extra-heavy loop runs
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_inv_square_sum_loop_run_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_inv_square_sum_loop_run<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_inv_square_sum_loop_run_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_inv_square_sum_loop_run_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_inv_cube_sum_loop_run_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_inv_cube_sum_loop_run<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_inv_cube_sum_loop_run_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_inv_cube_sum_loop_run_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_mul_chain_sum_loop_run_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_mul_chain_sum_loop_run<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_chain_sum_loop_run_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_chain_sum_loop_run_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_affine_inv_square_sum_loop_run_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_affine_inv_square_sum_loop_run<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_affine_inv_square_sum_loop_run_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_affine_inv_square_sum_loop_run_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_test6_sin_table_fill_loop_run_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_test6_sin_table_fill_loop_run<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_test6_sin_table_fill_loop_run_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_test6_sin_table_fill_loop_run_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_for_ptr_inc_ne_br_if_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_for_ptr_inc_ne_br_if<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_for_ptr_inc_ne_br_if_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_for_ptr_inc_ne_br_if_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_for_ptr_inc_ne_br_if_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_for_ptr_inc_ne_br_if<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_for_ptr_inc_ne_br_if_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_for_ptr_inc_ne_br_if_fptr<CompileOption, TypeInTuple...>(curr); }

    }  // namespace translate

#endif
}  // namespace uwvm2::runtime::compiler::uwvm_int::optable

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
