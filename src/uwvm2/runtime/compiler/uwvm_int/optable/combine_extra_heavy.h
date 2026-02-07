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

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <cstring>
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

    // ========================
    // Extra-heavy combined opcodes (ultra-specific mega fusions)
    // ========================

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
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_prime_divisor_loop_run(Type... type) UWVM_THROWS
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
            if(rem == wasm_i32{})
            {
                type...[0] = break_ip;
                break;
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
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_prime_divisor_loop_run(TypeRef & ... typeref) UWVM_THROWS
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
    // loop_fuse: for-style tight loop skeletons
    // ----------------------------------------

    /// @brief Fused combined opcode entrypoint `uwvmint_for_i32_inc_lt_u_br_if` (tail-call).
    /// @details
    /// - Stack-top optimization: N/A (no operand stack values are produced).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (i i32), `wasm_i32` (step), `wasm_i32` (end), `::std::byte const*` (label ip).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_for_i32_inc_lt_u_br_if(Type... type) UWVM_THROWS
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
        if(details::eval_int_cmp<details::int_cmp::lt_u, wasm_i32, conbine_details::wasm_u32>(next_i, end)) { type...[0] = jmp_ip; }

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
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_for_i32_inc_lt_u_br_if(TypeRef & ... typeref) UWVM_THROWS
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
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_for_ptr_inc_ne_br_if(Type... type) UWVM_THROWS
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
        if(details::eval_int_cmp<details::int_cmp::ne, wasm_i32, conbine_details::wasm_u32>(next_p, pend)) { type...[0] = jmp_ip; }

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
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_for_ptr_inc_ne_br_if(TypeRef & ... typeref) UWVM_THROWS
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

    /// @brief Fused `local.get(f64 sqrt); local.get(i32 i); i32.const step; i32.add; local.tee i; f64.convert_i32_u; f64.lt; i32.eqz; br_if <L>` (tail-call).
    /// @details
    /// - Stack-top optimization: N/A (no operand stack values are produced).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (sqrt f64), `local_offset_t` (i i32), `wasm_i32` (step), `::std::byte const*` (label ip).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_for_i32_inc_f64_lt_u_eqz_br_if(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const sqrt_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const i_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const step{conbine_details::read_imm<wasm_i32>(type...[0])};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        wasm_f64 const sqrt_n{conbine_details::load_local<wasm_f64>(type...[2u], sqrt_off)};
        wasm_i32 const i{conbine_details::load_local<wasm_i32>(type...[2u], i_off)};
        wasm_i32 const next_i{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(i, step)};
        conbine_details::store_local(type...[2u], i_off, next_i);

        wasm_f64 const next_i_d{static_cast<wasm_f64>(static_cast<::std::uint_least32_t>(next_i))};
        bool const lt{details::eval_float_cmp<details::float_cmp::lt, wasm_f64>(sqrt_n, next_i_d)};
        if(!lt) { type...[0] = jmp_ip; }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get(f64 sqrt); local.get(i32 i); i32.const step; i32.add; local.tee i; f64.convert_i32_u; f64.lt; i32.eqz; br_if <L>` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (sqrt f64), `local_offset_t` (i i32), `wasm_i32` (step), `::std::byte const*` (label ip).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_for_i32_inc_f64_lt_u_eqz_br_if(TypeRef & ... typeref) UWVM_THROWS
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

        auto const sqrt_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const i_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_i32 const step{conbine_details::read_imm<wasm_i32>(typeref...[0])};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), typeref...[0], sizeof(jmp_ip));
        typeref...[0] += sizeof(jmp_ip);

        wasm_f64 const sqrt_n{conbine_details::load_local<wasm_f64>(typeref...[2u], sqrt_off)};
        wasm_i32 const i{conbine_details::load_local<wasm_i32>(typeref...[2u], i_off)};
        wasm_i32 const next_i{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(i, step)};
        conbine_details::store_local(typeref...[2u], i_off, next_i);

        wasm_f64 const next_i_d{static_cast<wasm_f64>(static_cast<::std::uint_least32_t>(next_i))};
        bool const lt{details::eval_float_cmp<details::float_cmp::lt, wasm_f64>(sqrt_n, next_i_d)};
        if(!lt) { typeref...[0] = jmp_ip; }
    }

    namespace translate
    {
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

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_for_i32_inc_f64_lt_u_eqz_br_if_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_for_i32_inc_f64_lt_u_eqz_br_if<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_for_i32_inc_f64_lt_u_eqz_br_if_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_for_i32_inc_f64_lt_u_eqz_br_if_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_for_i32_inc_f64_lt_u_eqz_br_if_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_for_i32_inc_f64_lt_u_eqz_br_if<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_for_i32_inc_f64_lt_u_eqz_br_if_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_for_i32_inc_f64_lt_u_eqz_br_if_fptr<CompileOption, TypeInTuple...>(curr); }
    }  // namespace translate

#endif
}  // namespace uwvm2::runtime::compiler::uwvm_int::optable

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
