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
 * 中文说明：
 * - 这里放的是“针对性/定制化”的超重融合优化，用于把某些数据结构/算法里反复出现的热点片段整合成复杂组合。
 * - 这类优化默认不启用（避免代码体积/ICache/BTB 压力影响通用场景），只在明确知道收益时才开启。
 */

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
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

    // ----------------------------------------
    // loop_fuse: for-style tight loop skeletons
    // ----------------------------------------

    /// @brief Fused combined opcode entrypoint `uwvmint_for_i32_inc_lt_u_br_if` (tail-call).
    /// @details
    /// - Stack-top optimization: N/A (no operand stack values are produced).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (i i32), `wasm_i32` (step), `wasm_i32` (end), `::std::byte const*` (label ip).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_for_i32_inc_lt_u_br_if(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const i_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const step{conbine_details::read_imm<wasm_i32>(type...[0])};
        wasm_i32 const end{conbine_details::read_imm<wasm_i32>(type...[0])};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        wasm_i32 const i{conbine_details::load_local<wasm_i32>(type...[2u], i_off)};
        wasm_i32 const next_i{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(i, step)};
        conbine_details::store_local(type...[2u], i_off, next_i);
        bool const take_branch{details::eval_int_cmp<details::int_cmp::lt_u, wasm_i32, conbine_details::wasm_u32>(next_i, end)};

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

    /// @brief Fused combined opcode entrypoint `uwvmint_for_i32_inc_lt_u_br_if` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (i i32), `wasm_i32` (step), `wasm_i32` (end), `::std::byte const*` (label ip).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_for_i32_inc_lt_u_br_if(TypeRef & ... typeref) UWVM_THROWS
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

        auto const i_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_i32 const step{conbine_details::read_imm<wasm_i32>(typeref...[0])};
        wasm_i32 const end{conbine_details::read_imm<wasm_i32>(typeref...[0])};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), typeref...[0], sizeof(jmp_ip));
        typeref...[0] += sizeof(jmp_ip);

        wasm_i32 const i{conbine_details::load_local<wasm_i32>(typeref...[2u], i_off)};
        wasm_i32 const next_i{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(i, step)};
        conbine_details::store_local(typeref...[2u], i_off, next_i);
        if(details::eval_int_cmp<details::int_cmp::lt_u, wasm_i32, conbine_details::wasm_u32>(next_i, end)) { typeref...[0] = jmp_ip; }
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

    namespace translate
    {
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

        // loop skeletons
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_for_i32_inc_lt_u_br_if_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_for_i32_inc_lt_u_br_if<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_for_i32_inc_lt_u_br_if_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_for_i32_inc_lt_u_br_if_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_for_i32_inc_lt_u_br_if_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_for_i32_inc_lt_u_br_if<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_for_i32_inc_lt_u_br_if_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_for_i32_inc_lt_u_br_if_fptr<CompileOption, TypeInTuple...>(curr); }

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
