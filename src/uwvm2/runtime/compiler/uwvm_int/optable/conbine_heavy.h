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
#ifdef UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS

    // ========================
    // Heavy combined opcodes (dense_compute / rare patterns)
    // ========================

    // rotate_imm: local.get + imm + rotl/rotr
    /// @brief Fused `local.get` + immediate + `i32.rotl` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_rotl_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_imm_localget<CompileOption, numeric_details::int_binop::rotl>(typeref...); }

    /// @brief Fused `local.get` + immediate + `i32.rotr` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_rotr_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_imm_localget<CompileOption, numeric_details::int_binop::rotr>(typeref...); }

    // bit_unary: local.get + unop (push i32)
    /// @brief Fused `local.get` + `i32.unop` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_unop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_unop_localget(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const x{conbine_details::load_local<wasm_i32>(type...[2u], local_off)};
        wasm_i32 const out{numeric_details::eval_int_unop<Op, wasm_i32, numeric_details::wasm_u32>(x)};
        conbine_details::push_operand<CompileOption, wasm_i32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `i32.unop` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_unop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_unop_localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_i32 const x{conbine_details::load_local<wasm_i32>(typeref...[2u], local_off)};
        wasm_i32 const out{numeric_details::eval_int_unop<Op, wasm_i32, numeric_details::wasm_u32>(x)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + `i32.popcnt` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_popcnt_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_unop_localget<CompileOption, numeric_details::int_unop::popcnt>(typeref...); }

    /// @brief Fused `local.get` + `i32.clz` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_clz_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_unop_localget<CompileOption, numeric_details::int_unop::clz>(typeref...); }

    /// @brief Fused `local.get` + `i32.ctz` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_ctz_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_unop_localget<CompileOption, numeric_details::int_unop::ctz>(typeref...); }

    // ----------------------------------------
    // float: local.get + imm + binop (push T)
    // ----------------------------------------

    /// @brief Fused `local.get` + immediate + `f32.binop` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`, `wasm_f32`.

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_binop_imm_localget(Type... type) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f32 const rhs{conbine_details::read_imm<wasm_f32>(type...[0])};
        wasm_f32 const lhs{conbine_details::load_local<wasm_f32>(type...[2u], local_off)};
        wasm_f32 const out{numeric_details::eval_float_binop<Op, wasm_f32>(lhs, rhs)};

        conbine_details::push_operand<CompileOption, wasm_f32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + immediate + `f32.binop` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_f32`.

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_binop_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f32 const rhs{conbine_details::read_imm<wasm_f32>(typeref...[0])};
        wasm_f32 const lhs{conbine_details::load_local<wasm_f32>(typeref...[2u], local_off)};
        wasm_f32 const out{numeric_details::eval_float_binop<Op, wasm_f32>(lhs, rhs)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + immediate + `f32.add` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_f32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_add_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop_imm_localget<CompileOption, numeric_details::float_binop::add>(typeref...); }

    /// @brief Fused `local.get` + immediate + `f32.mul` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_f32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_mul_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop_imm_localget<CompileOption, numeric_details::float_binop::mul>(typeref...); }

    /// @brief Fused `local.get` + immediate + `f32.min` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_f32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_min_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop_imm_localget<CompileOption, numeric_details::float_binop::min>(typeref...); }

    /// @brief Fused `local.get` + immediate + `f32.max` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_f32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_max_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop_imm_localget<CompileOption, numeric_details::float_binop::max>(typeref...); }

    // compound_math: imm/x, imm-x (fast-math patterns)
    /// @brief Fused `f32.const <imm>; local.get; f32.div` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`, `wasm_f32 imm`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_div_from_imm_localget(Type... type) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f32 const imm{conbine_details::read_imm<wasm_f32>(type...[0])};
        wasm_f32 const x{conbine_details::load_local<wasm_f32>(type...[2u], local_off)};
        wasm_f32 const out{numeric_details::eval_float_binop<numeric_details::float_binop::div, wasm_f32>(imm, x)};
        conbine_details::push_operand<CompileOption, wasm_f32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `f32.const <imm>; local.get; f32.div` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_f32 imm`.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_div_from_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f32 const imm{conbine_details::read_imm<wasm_f32>(typeref...[0])};
        wasm_f32 const x{conbine_details::load_local<wasm_f32>(typeref...[2u], local_off)};
        wasm_f32 const out{numeric_details::eval_float_binop<numeric_details::float_binop::div, wasm_f32>(imm, x)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `f32.const <imm>; local.get; f32.div; local.tee` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (src), `wasm_f32 imm`, `local_offset_t` (dst).
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_div_from_imm_localtee(Type... type) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const src_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f32 const imm{conbine_details::read_imm<wasm_f32>(type...[0])};
        auto const dst_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        wasm_f32 const x{conbine_details::load_local<wasm_f32>(type...[2u], src_off)};
        wasm_f32 const out{numeric_details::eval_float_binop<numeric_details::float_binop::div, wasm_f32>(imm, x)};
        conbine_details::store_local(type...[2u], dst_off, out);
        conbine_details::push_operand<CompileOption, wasm_f32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `f32.const <imm>; local.get; f32.div; local.tee` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (src), `wasm_f32 imm`, `local_offset_t` (dst).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_div_from_imm_localtee(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const src_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f32 const imm{conbine_details::read_imm<wasm_f32>(typeref...[0])};
        auto const dst_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        wasm_f32 const x{conbine_details::load_local<wasm_f32>(typeref...[2u], src_off)};
        wasm_f32 const out{numeric_details::eval_float_binop<numeric_details::float_binop::div, wasm_f32>(imm, x)};
        conbine_details::store_local(typeref...[2u], dst_off, out);
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `f32.const <imm>; local.get; f32.sub` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`, `wasm_f32 imm`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_sub_from_imm_localget(Type... type) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f32 const imm{conbine_details::read_imm<wasm_f32>(type...[0])};
        wasm_f32 const x{conbine_details::load_local<wasm_f32>(type...[2u], local_off)};
        wasm_f32 const out{numeric_details::eval_float_binop<numeric_details::float_binop::sub, wasm_f32>(imm, x)};
        conbine_details::push_operand<CompileOption, wasm_f32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `f32.const <imm>; local.get; f32.sub` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_f32 imm`.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_sub_from_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f32 const imm{conbine_details::read_imm<wasm_f32>(typeref...[0])};
        wasm_f32 const x{conbine_details::load_local<wasm_f32>(typeref...[2u], local_off)};
        wasm_f32 const out{numeric_details::eval_float_binop<numeric_details::float_binop::sub, wasm_f32>(imm, x)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + immediate + `f64.binop` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`, `wasm_f64`.

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_binop_imm_localget(Type... type) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f64 const rhs{conbine_details::read_imm<wasm_f64>(type...[0])};
        wasm_f64 const lhs{conbine_details::load_local<wasm_f64>(type...[2u], local_off)};
        wasm_f64 const out{numeric_details::eval_float_binop<Op, wasm_f64>(lhs, rhs)};

        conbine_details::push_operand<CompileOption, wasm_f64, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + immediate + `f64.binop` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_f64`.

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_binop_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f64 const rhs{conbine_details::read_imm<wasm_f64>(typeref...[0])};
        wasm_f64 const lhs{conbine_details::load_local<wasm_f64>(typeref...[2u], local_off)};
        wasm_f64 const out{numeric_details::eval_float_binop<Op, wasm_f64>(lhs, rhs)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + immediate + `f64.add` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_f64`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_add_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop_imm_localget<CompileOption, numeric_details::float_binop::add>(typeref...); }

    /// @brief Fused `local.get` + immediate + `f64.mul` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_f64`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mul_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop_imm_localget<CompileOption, numeric_details::float_binop::mul>(typeref...); }

    /// @brief Fused `local.get` + immediate + `f64.min` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_f64`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_min_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop_imm_localget<CompileOption, numeric_details::float_binop::min>(typeref...); }

    /// @brief Fused `local.get` + immediate + `f64.max` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_f64`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_max_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop_imm_localget<CompileOption, numeric_details::float_binop::max>(typeref...); }

    // ----------------------------------------
    // float: local.get + local.get + binop
    // ----------------------------------------

    /// @brief Fused `local.get` + `local.get` + `f32.binop` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_binop_2localget(Type... type) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const lhs_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const rhs_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f32 const lhs{conbine_details::load_local<wasm_f32>(type...[2u], lhs_off)};
        wasm_f32 const rhs{conbine_details::load_local<wasm_f32>(type...[2u], rhs_off)};
        wasm_f32 const out{numeric_details::eval_float_binop<Op, wasm_f32>(lhs, rhs)};
        conbine_details::push_operand<CompileOption, wasm_f32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `local.get` + `f32.binop` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_binop_2localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const lhs_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const rhs_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f32 const lhs{conbine_details::load_local<wasm_f32>(typeref...[2u], lhs_off)};
        wasm_f32 const rhs{conbine_details::load_local<wasm_f32>(typeref...[2u], rhs_off)};
        wasm_f32 const out{numeric_details::eval_float_binop<Op, wasm_f32>(lhs, rhs)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + `local.get` + `f32.add` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_add_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop_2localget<CompileOption, numeric_details::float_binop::add>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `f32.sub` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_sub_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop_2localget<CompileOption, numeric_details::float_binop::sub>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `f32.mul` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_mul_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop_2localget<CompileOption, numeric_details::float_binop::mul>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `f32.div` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_div_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop_2localget<CompileOption, numeric_details::float_binop::div>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `f32.min` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_min_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop_2localget<CompileOption, numeric_details::float_binop::min>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `f32.max` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_max_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop_2localget<CompileOption, numeric_details::float_binop::max>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `f64.binop` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_binop_2localget(Type... type) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const lhs_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const rhs_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f64 const lhs{conbine_details::load_local<wasm_f64>(type...[2u], lhs_off)};
        wasm_f64 const rhs{conbine_details::load_local<wasm_f64>(type...[2u], rhs_off)};
        wasm_f64 const out{numeric_details::eval_float_binop<Op, wasm_f64>(lhs, rhs)};
        conbine_details::push_operand<CompileOption, wasm_f64, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `local.get` + `f64.binop` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_binop_2localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const lhs_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const rhs_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f64 const lhs{conbine_details::load_local<wasm_f64>(typeref...[2u], lhs_off)};
        wasm_f64 const rhs{conbine_details::load_local<wasm_f64>(typeref...[2u], rhs_off)};
        wasm_f64 const out{numeric_details::eval_float_binop<Op, wasm_f64>(lhs, rhs)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + `local.get` + `f64.add` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_add_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop_2localget<CompileOption, numeric_details::float_binop::add>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `f64.sub` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_sub_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop_2localget<CompileOption, numeric_details::float_binop::sub>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `f64.mul` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mul_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop_2localget<CompileOption, numeric_details::float_binop::mul>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `f64.div` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_div_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop_2localget<CompileOption, numeric_details::float_binop::div>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `f64.min` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_min_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop_2localget<CompileOption, numeric_details::float_binop::min>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `f64.max` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_max_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop_2localget<CompileOption, numeric_details::float_binop::max>(typeref...); }

    // ----------------------------------------
    // float: local.get + unop (push T)
    // ----------------------------------------

    /// @brief Fused `local.get` + `f32.unop` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_unop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_unop_localget(Type... type) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f32 const x{conbine_details::load_local<wasm_f32>(type...[2u], local_off)};
        wasm_f32 const out{numeric_details::eval_float_unop<Op, wasm_f32>(x)};
        conbine_details::push_operand<CompileOption, wasm_f32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `f32.unop` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_unop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_unop_localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f32 const x{conbine_details::load_local<wasm_f32>(typeref...[2u], local_off)};
        wasm_f32 const out{numeric_details::eval_float_unop<Op, wasm_f32>(x)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + `f32.abs` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_abs_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_unop_localget<CompileOption, numeric_details::float_unop::abs>(typeref...); }

    /// @brief Fused `local.get` + `f32.neg` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_neg_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_unop_localget<CompileOption, numeric_details::float_unop::neg>(typeref...); }

    /// @brief Fused `local.get` + `f32.sqrt` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_sqrt_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_unop_localget<CompileOption, numeric_details::float_unop::sqrt>(typeref...); }

    /// @brief Fused `local.get` + `f64.unop` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_unop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_unop_localget(Type... type) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f64 const x{conbine_details::load_local<wasm_f64>(type...[2u], local_off)};
        wasm_f64 const out{numeric_details::eval_float_unop<Op, wasm_f64>(x)};
        conbine_details::push_operand<CompileOption, wasm_f64, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `f64.unop` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_unop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_unop_localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f64 const x{conbine_details::load_local<wasm_f64>(typeref...[2u], local_off)};
        wasm_f64 const out{numeric_details::eval_float_unop<Op, wasm_f64>(x)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + `f64.abs` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_abs_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_unop_localget<CompileOption, numeric_details::float_unop::abs>(typeref...); }

    /// @brief Fused `local.get` + `f64.neg` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_neg_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_unop_localget<CompileOption, numeric_details::float_unop::neg>(typeref...); }

    /// @brief Fused `local.get` + `f64.sqrt` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_sqrt_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_unop_localget<CompileOption, numeric_details::float_unop::sqrt>(typeref...); }

    // ----------------------------------------
    // convert: local.get + int/float convert
    // ----------------------------------------

    /// @brief Fused `local.get` + `f32.convert_i32_s` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_from_i32_s(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const x{conbine_details::load_local<wasm_i32>(type...[2u], local_off)};
        wasm_f32 const out{static_cast<wasm_f32>(static_cast<::std::int_least32_t>(x))};
        conbine_details::push_operand<CompileOption, wasm_f32, curr_f32_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `f32.convert_i32_s` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_from_i32_s(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_i32 const x{conbine_details::load_local<wasm_i32>(typeref...[2u], local_off)};
        wasm_f32 const out{static_cast<wasm_f32>(static_cast<::std::int_least32_t>(x))};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + `f32.convert_i32_u` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_from_i32_u(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const x{conbine_details::load_local<wasm_i32>(type...[2u], local_off)};
        wasm_f32 const out{static_cast<wasm_f32>(static_cast<::std::uint_least32_t>(x))};
        conbine_details::push_operand<CompileOption, wasm_f32, curr_f32_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `f32.convert_i32_u` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_from_i32_u(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_i32 const x{conbine_details::load_local<wasm_i32>(typeref...[2u], local_off)};
        wasm_f32 const out{static_cast<wasm_f32>(static_cast<::std::uint_least32_t>(x))};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + `i32.trunc_f32_s` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_from_f32_trunc_s(Type... type) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f32 const x{conbine_details::load_local<wasm_f32>(type...[2u], local_off)};
        ::std::int_least32_t const out32{details::trunc_float_to_int_s<::std::int_least32_t>(x)};
        wasm_i32 const out{static_cast<wasm_i32>(out32)};
        conbine_details::push_operand<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `i32.trunc_f32_s` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_from_f32_trunc_s(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f32 const x{conbine_details::load_local<wasm_f32>(typeref...[2u], local_off)};
        ::std::int_least32_t const out32{details::trunc_float_to_int_s<::std::int_least32_t>(x)};
        wasm_i32 const out{static_cast<wasm_i32>(out32)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + `i32.trunc_f32_u` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_from_f32_trunc_u(Type... type) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f32 const x{conbine_details::load_local<wasm_f32>(type...[2u], local_off)};
        ::std::uint_least32_t const u32{details::trunc_float_to_int_u<::std::uint_least32_t>(x)};
        wasm_i32 const out{details::from_u32_bits<wasm_i32>(u32)};
        conbine_details::push_operand<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `i32.trunc_f32_u` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_from_f32_trunc_u(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f32 const x{conbine_details::load_local<wasm_f32>(typeref...[2u], local_off)};
        ::std::uint_least32_t const u32{details::trunc_float_to_int_u<::std::uint_least32_t>(x)};
        wasm_i32 const out{details::from_u32_bits<wasm_i32>(u32)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + `i32.trunc_f64_s` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_from_f64_trunc_s(Type... type) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f64 const x{conbine_details::load_local<wasm_f64>(type...[2u], local_off)};
        ::std::int_least32_t const out32{details::trunc_float_to_int_s<::std::int_least32_t>(x)};
        wasm_i32 const out{static_cast<wasm_i32>(out32)};
        conbine_details::push_operand<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `i32.trunc_f64_s` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_from_f64_trunc_s(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f64 const x{conbine_details::load_local<wasm_f64>(typeref...[2u], local_off)};
        ::std::int_least32_t const out32{details::trunc_float_to_int_s<::std::int_least32_t>(x)};
        wasm_i32 const out{static_cast<wasm_i32>(out32)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + `i32.trunc_f64_u` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_from_f64_trunc_u(Type... type) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_f64 const x{conbine_details::load_local<wasm_f64>(type...[2u], local_off)};
        ::std::uint_least32_t const u32{details::trunc_float_to_int_u<::std::uint_least32_t>(x)};
        wasm_i32 const out{details::from_u32_bits<wasm_i32>(u32)};
        conbine_details::push_operand<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `i32.trunc_f64_u` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_from_f64_trunc_u(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_f64 const x{conbine_details::load_local<wasm_f64>(typeref...[2u], local_off)};
        ::std::uint_least32_t const u32{details::trunc_float_to_int_u<::std::uint_least32_t>(x)};
        wasm_i32 const out{details::from_u32_bits<wasm_i32>(u32)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    // ----------------------------------------
    // fma-like: local.get * local.get (+/-) local.get
    // ----------------------------------------

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_mul_addsub_3localget` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, bool Sub, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_mul_addsub_3localget(Type... type) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const c_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        wasm_f32 const a{conbine_details::load_local<wasm_f32>(type...[2u], a_off)};
        wasm_f32 const b{conbine_details::load_local<wasm_f32>(type...[2u], b_off)};
        wasm_f32 const c{conbine_details::load_local<wasm_f32>(type...[2u], c_off)};

        wasm_f32 const mul{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f32>(a, b)};
        wasm_f32 const out{numeric_details::eval_float_binop<Sub ? numeric_details::float_binop::sub : numeric_details::float_binop::add, wasm_f32>(mul, c)};
        conbine_details::push_operand<CompileOption, wasm_f32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_mul_addsub_3localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, bool Sub, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_mul_addsub_3localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const c_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        wasm_f32 const a{conbine_details::load_local<wasm_f32>(typeref...[2u], a_off)};
        wasm_f32 const b{conbine_details::load_local<wasm_f32>(typeref...[2u], b_off)};
        wasm_f32 const c{conbine_details::load_local<wasm_f32>(typeref...[2u], c_off)};

        wasm_f32 const mul{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f32>(a, b)};
        wasm_f32 const out{numeric_details::eval_float_binop<Sub ? numeric_details::float_binop::sub : numeric_details::float_binop::add, wasm_f32>(mul, c)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_mul_add_3localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_mul_add_3localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_mul_addsub_3localget<CompileOption, false>(typeref...); }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_mul_sub_3localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_mul_sub_3localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_mul_addsub_3localget<CompileOption, true>(typeref...); }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_mul_addsub_3localget` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, bool Sub, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mul_addsub_3localget(Type... type) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const c_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        wasm_f64 const a{conbine_details::load_local<wasm_f64>(type...[2u], a_off)};
        wasm_f64 const b{conbine_details::load_local<wasm_f64>(type...[2u], b_off)};
        wasm_f64 const c{conbine_details::load_local<wasm_f64>(type...[2u], c_off)};

        wasm_f64 const mul{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f64>(a, b)};
        wasm_f64 const out{numeric_details::eval_float_binop<Sub ? numeric_details::float_binop::sub : numeric_details::float_binop::add, wasm_f64>(mul, c)};
        conbine_details::push_operand<CompileOption, wasm_f64, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_mul_addsub_3localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, bool Sub, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mul_addsub_3localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const c_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        wasm_f64 const a{conbine_details::load_local<wasm_f64>(typeref...[2u], a_off)};
        wasm_f64 const b{conbine_details::load_local<wasm_f64>(typeref...[2u], b_off)};
        wasm_f64 const c{conbine_details::load_local<wasm_f64>(typeref...[2u], c_off)};

        wasm_f64 const mul{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f64>(a, b)};
        wasm_f64 const out{numeric_details::eval_float_binop<Sub ? numeric_details::float_binop::sub : numeric_details::float_binop::add, wasm_f64>(mul, c)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_mul_add_3localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mul_add_3localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_mul_addsub_3localget<CompileOption, false>(typeref...); }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_mul_sub_3localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mul_sub_3localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_mul_addsub_3localget<CompileOption, true>(typeref...); }

    // ----------------------------------------
    // two-mul: a*b (+/-) c*d
    // ----------------------------------------

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_2mul_addsub_4localget` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, bool Sub, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_2mul_addsub_4localget(Type... type) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const c_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const d_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        wasm_f32 const a{conbine_details::load_local<wasm_f32>(type...[2u], a_off)};
        wasm_f32 const b{conbine_details::load_local<wasm_f32>(type...[2u], b_off)};
        wasm_f32 const c{conbine_details::load_local<wasm_f32>(type...[2u], c_off)};
        wasm_f32 const d{conbine_details::load_local<wasm_f32>(type...[2u], d_off)};

        wasm_f32 const m1{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f32>(a, b)};
        wasm_f32 const m2{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f32>(c, d)};
        wasm_f32 const out{numeric_details::eval_float_binop<Sub ? numeric_details::float_binop::sub : numeric_details::float_binop::add, wasm_f32>(m1, m2)};

        conbine_details::push_operand<CompileOption, wasm_f32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_2mul_addsub_4localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, bool Sub, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_2mul_addsub_4localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const c_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const d_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        wasm_f32 const a{conbine_details::load_local<wasm_f32>(typeref...[2u], a_off)};
        wasm_f32 const b{conbine_details::load_local<wasm_f32>(typeref...[2u], b_off)};
        wasm_f32 const c{conbine_details::load_local<wasm_f32>(typeref...[2u], c_off)};
        wasm_f32 const d{conbine_details::load_local<wasm_f32>(typeref...[2u], d_off)};

        wasm_f32 const m1{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f32>(a, b)};
        wasm_f32 const m2{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f32>(c, d)};
        wasm_f32 const out{numeric_details::eval_float_binop<Sub ? numeric_details::float_binop::sub : numeric_details::float_binop::add, wasm_f32>(m1, m2)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_mul_add_2mul_4localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_mul_add_2mul_4localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_2mul_addsub_4localget<CompileOption, false>(typeref...); }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_mul_sub_2mul_4localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_mul_sub_2mul_4localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_2mul_addsub_4localget<CompileOption, true>(typeref...); }

    // aliases (dense_compute)
    /// @brief Fused combined opcode entrypoint `uwvmint_f32_2mul_add` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_2mul_add(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_mul_add_2mul_4localget<CompileOption>(typeref...); }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_2mul_addsub_4localget` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, bool Sub, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_2mul_addsub_4localget(Type... type) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const c_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const d_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        wasm_f64 const a{conbine_details::load_local<wasm_f64>(type...[2u], a_off)};
        wasm_f64 const b{conbine_details::load_local<wasm_f64>(type...[2u], b_off)};
        wasm_f64 const c{conbine_details::load_local<wasm_f64>(type...[2u], c_off)};
        wasm_f64 const d{conbine_details::load_local<wasm_f64>(type...[2u], d_off)};

        wasm_f64 const m1{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f64>(a, b)};
        wasm_f64 const m2{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f64>(c, d)};
        wasm_f64 const out{numeric_details::eval_float_binop<Sub ? numeric_details::float_binop::sub : numeric_details::float_binop::add, wasm_f64>(m1, m2)};

        conbine_details::push_operand<CompileOption, wasm_f64, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_2mul_addsub_4localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, bool Sub, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_2mul_addsub_4localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const c_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const d_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        wasm_f64 const a{conbine_details::load_local<wasm_f64>(typeref...[2u], a_off)};
        wasm_f64 const b{conbine_details::load_local<wasm_f64>(typeref...[2u], b_off)};
        wasm_f64 const c{conbine_details::load_local<wasm_f64>(typeref...[2u], c_off)};
        wasm_f64 const d{conbine_details::load_local<wasm_f64>(typeref...[2u], d_off)};

        wasm_f64 const m1{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f64>(a, b)};
        wasm_f64 const m2{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f64>(c, d)};
        wasm_f64 const out{numeric_details::eval_float_binop<Sub ? numeric_details::float_binop::sub : numeric_details::float_binop::add, wasm_f64>(m1, m2)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_mul_add_2mul_4localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mul_add_2mul_4localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_2mul_addsub_4localget<CompileOption, false>(typeref...); }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_mul_sub_2mul_4localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mul_sub_2mul_4localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_2mul_addsub_4localget<CompileOption, true>(typeref...); }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_2mul_add` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_2mul_add(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_mul_add_2mul_4localget<CompileOption>(typeref...); }

    // ----------------------------------------
    // update_local: acc += x*y (set/tee)
    // ----------------------------------------

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_mac_local_settee_acc` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, bool Tee, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_mac_local_settee_acc(Type... type) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const x_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const y_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const acc_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        wasm_f32 const x{conbine_details::load_local<wasm_f32>(type...[2u], x_off)};
        wasm_f32 const y{conbine_details::load_local<wasm_f32>(type...[2u], y_off)};
        wasm_f32 const acc{conbine_details::load_local<wasm_f32>(type...[2u], acc_off)};

        wasm_f32 const prod{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f32>(x, y)};
        wasm_f32 const out{numeric_details::eval_float_binop<numeric_details::float_binop::add, wasm_f32>(acc, prod)};
        conbine_details::store_local(type...[2u], acc_off, out);

        if constexpr(Tee) { conbine_details::push_operand<CompileOption, wasm_f32, curr_stack_top>(out, type...); }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_mac_local_settee_acc` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, bool Tee, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_mac_local_settee_acc(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const x_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const y_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const acc_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        wasm_f32 const x{conbine_details::load_local<wasm_f32>(typeref...[2u], x_off)};
        wasm_f32 const y{conbine_details::load_local<wasm_f32>(typeref...[2u], y_off)};
        wasm_f32 const acc{conbine_details::load_local<wasm_f32>(typeref...[2u], acc_off)};

        wasm_f32 const prod{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f32>(x, y)};
        wasm_f32 const out{numeric_details::eval_float_binop<numeric_details::float_binop::add, wasm_f32>(acc, prod)};
        conbine_details::store_local(typeref...[2u], acc_off, out);

        if constexpr(Tee) { conbine_details::push_operand_byref<CompileOption>(out, typeref...); }
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_mac_local_set_acc` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_mac_local_set_acc(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_mac_local_settee_acc<CompileOption, false>(typeref...); }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_mac_local_tee_acc` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_mac_local_tee_acc(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_mac_local_settee_acc<CompileOption, true>(typeref...); }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_mac_local_set_acc` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mac_local_set_acc(Type... type) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const x_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const y_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const acc_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        wasm_f64 const x{conbine_details::load_local<wasm_f64>(type...[2u], x_off)};
        wasm_f64 const y{conbine_details::load_local<wasm_f64>(type...[2u], y_off)};
        wasm_f64 const acc{conbine_details::load_local<wasm_f64>(type...[2u], acc_off)};

        wasm_f64 const prod{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f64>(x, y)};
        wasm_f64 const out{numeric_details::eval_float_binop<numeric_details::float_binop::add, wasm_f64>(acc, prod)};
        conbine_details::store_local(type...[2u], acc_off, out);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_mac_local_set_acc` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mac_local_set_acc(TypeRef & ... typeref) UWVM_THROWS
    {
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

        auto const x_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const y_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const acc_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        wasm_f64 const x{conbine_details::load_local<wasm_f64>(typeref...[2u], x_off)};
        wasm_f64 const y{conbine_details::load_local<wasm_f64>(typeref...[2u], y_off)};
        wasm_f64 const acc{conbine_details::load_local<wasm_f64>(typeref...[2u], acc_off)};

        wasm_f64 const prod{numeric_details::eval_float_binop<numeric_details::float_binop::mul, wasm_f64>(x, y)};
        wasm_f64 const out{numeric_details::eval_float_binop<numeric_details::float_binop::add, wasm_f64>(acc, prod)};
        conbine_details::store_local(typeref...[2u], acc_off, out);
    }

    // i32 / i64 integer MAC (acc += x*y)
    /// @brief Fused combined opcode entrypoint `uwvmint_i32_mac_local_set_acc` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_mac_local_set_acc(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const x_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const y_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const acc_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        wasm_i32 const x{conbine_details::load_local<wasm_i32>(type...[2u], x_off)};
        wasm_i32 const y{conbine_details::load_local<wasm_i32>(type...[2u], y_off)};
        wasm_i32 const acc{conbine_details::load_local<wasm_i32>(type...[2u], acc_off)};

        wasm_i32 const prod{numeric_details::eval_int_binop<numeric_details::int_binop::mul, wasm_i32, numeric_details::wasm_u32>(x, y)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(acc, prod)};
        conbine_details::store_local(type...[2u], acc_off, out);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_mac_local_set_acc` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_mac_local_set_acc(TypeRef & ... typeref) UWVM_THROWS
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

        auto const x_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const y_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const acc_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        wasm_i32 const x{conbine_details::load_local<wasm_i32>(typeref...[2u], x_off)};
        wasm_i32 const y{conbine_details::load_local<wasm_i32>(typeref...[2u], y_off)};
        wasm_i32 const acc{conbine_details::load_local<wasm_i32>(typeref...[2u], acc_off)};

        wasm_i32 const prod{numeric_details::eval_int_binop<numeric_details::int_binop::mul, wasm_i32, numeric_details::wasm_u32>(x, y)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(acc, prod)};
        conbine_details::store_local(typeref...[2u], acc_off, out);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i64_mac_local_set_acc` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_mac_local_set_acc(Type... type) UWVM_THROWS
    {
        using wasm_i64 = conbine_details::wasm_i64;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const x_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const y_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const acc_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        wasm_i64 const x{conbine_details::load_local<wasm_i64>(type...[2u], x_off)};
        wasm_i64 const y{conbine_details::load_local<wasm_i64>(type...[2u], y_off)};
        wasm_i64 const acc{conbine_details::load_local<wasm_i64>(type...[2u], acc_off)};

        wasm_i64 const prod{numeric_details::eval_int_binop<numeric_details::int_binop::mul, wasm_i64, numeric_details::wasm_u64>(x, y)};
        wasm_i64 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i64, numeric_details::wasm_u64>(acc, prod)};
        conbine_details::store_local(type...[2u], acc_off, out);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i64_mac_local_set_acc` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_mac_local_set_acc(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i64 = conbine_details::wasm_i64;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const x_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const y_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const acc_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        wasm_i64 const x{conbine_details::load_local<wasm_i64>(typeref...[2u], x_off)};
        wasm_i64 const y{conbine_details::load_local<wasm_i64>(typeref...[2u], y_off)};
        wasm_i64 const acc{conbine_details::load_local<wasm_i64>(typeref...[2u], acc_off)};

        wasm_i64 const prod{numeric_details::eval_int_binop<numeric_details::int_binop::mul, wasm_i64, numeric_details::wasm_u64>(x, y)};
        wasm_i64 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i64, numeric_details::wasm_u64>(acc, prod)};
        conbine_details::store_local(typeref...[2u], acc_off, out);
    }

    // ----------------------------------------
    // select_fuse: local selects -> local set/tee
    // ----------------------------------------

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_select_local_settee` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, bool Tee, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_select_local_settee(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const cond_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const dst_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        wasm_f32 const a{conbine_details::load_local<wasm_f32>(type...[2u], a_off)};
        wasm_f32 const b{conbine_details::load_local<wasm_f32>(type...[2u], b_off)};
        wasm_i32 const cond{conbine_details::load_local<wasm_i32>(type...[2u], cond_off)};

        wasm_f32 const out{cond != wasm_i32{} ? a : b};
        conbine_details::store_local(type...[2u], dst_off, out);
        if constexpr(Tee) { conbine_details::push_operand<CompileOption, wasm_f32, curr_f32_stack_top>(out, type...); }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_select_local_settee` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, bool Tee, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_select_local_settee(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const cond_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const dst_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        wasm_f32 const a{conbine_details::load_local<wasm_f32>(typeref...[2u], a_off)};
        wasm_f32 const b{conbine_details::load_local<wasm_f32>(typeref...[2u], b_off)};
        wasm_i32 const cond{conbine_details::load_local<wasm_i32>(typeref...[2u], cond_off)};

        wasm_f32 const out{cond != wasm_i32{} ? a : b};
        conbine_details::store_local(typeref...[2u], dst_off, out);
        if constexpr(Tee) { conbine_details::push_operand_byref<CompileOption>(out, typeref...); }
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_select_local_set` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_select_local_set(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_select_local_settee<CompileOption, false>(typeref...); }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_select_local_tee` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_select_local_tee(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_select_local_settee<CompileOption, true>(typeref...); }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_select_local_set` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_select_local_set(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const cond_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const dst_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        wasm_i32 const a{conbine_details::load_local<wasm_i32>(type...[2u], a_off)};
        wasm_i32 const b{conbine_details::load_local<wasm_i32>(type...[2u], b_off)};
        wasm_i32 const cond{conbine_details::load_local<wasm_i32>(type...[2u], cond_off)};

        wasm_i32 const out{cond != wasm_i32{} ? a : b};
        conbine_details::store_local(type...[2u], dst_off, out);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_select_local_set` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_select_local_set(TypeRef & ... typeref) UWVM_THROWS
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

        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const cond_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const dst_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        wasm_i32 const a{conbine_details::load_local<wasm_i32>(typeref...[2u], a_off)};
        wasm_i32 const b{conbine_details::load_local<wasm_i32>(typeref...[2u], b_off)};
        wasm_i32 const cond{conbine_details::load_local<wasm_i32>(typeref...[2u], cond_off)};

        wasm_i32 const out{cond != wasm_i32{} ? a : b};
        conbine_details::store_local(typeref...[2u], dst_off, out);
    }

    // ----------------------------------------
    // br_if fusions: small hot control-flow
    // ----------------------------------------

    /// @brief Fused `local.get a; local.get b; i32.rem_u; i32.eqz; br_if <L>` (tail-call).
    /// @details
    /// - Stack-top optimization: N/A (no operand stack values are produced).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (a), `local_offset_t` (b), `::std::byte const*` (label ip).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_rem_u_eqz_2localget(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        wasm_i32 const a{conbine_details::load_local<wasm_i32>(type...[2u], a_off)};
        wasm_i32 const b{conbine_details::load_local<wasm_i32>(type...[2u], b_off)};
        wasm_i32 const rem{numeric_details::eval_int_binop<numeric_details::int_binop::rem_u, wasm_i32, numeric_details::wasm_u32>(a, b)};
        if(rem == wasm_i32{}) { type...[0] = jmp_ip; }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get a; local.get b; i32.rem_u; i32.eqz; br_if <L>` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (a), `local_offset_t` (b), `::std::byte const*` (label ip).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_rem_u_eqz_2localget(TypeRef & ... typeref) UWVM_THROWS
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

        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), typeref...[0], sizeof(jmp_ip));
        typeref...[0] += sizeof(jmp_ip);

        wasm_i32 const a{conbine_details::load_local<wasm_i32>(typeref...[2u], a_off)};
        wasm_i32 const b{conbine_details::load_local<wasm_i32>(typeref...[2u], b_off)};
        wasm_i32 const rem{numeric_details::eval_int_binop<numeric_details::int_binop::rem_u, wasm_i32, numeric_details::wasm_u32>(a, b)};
        if(rem == wasm_i32{}) { typeref...[0] = jmp_ip; }
    }

    /// @brief Fused `local.get(f64 sqrt); local.get(i32 i); i32.const step; i32.add; local.tee i; f64.convert_i32_u; f64.lt; i32.eqz; br_if <L>` (tail-call).
    /// @details
    /// - Stack-top optimization: N/A (no operand stack values are produced).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (sqrt f64), `local_offset_t` (i i32), `wasm_i32` (step), `::std::byte const*` (label ip).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_for_i32_inc_f64_lt_u_eqz_br_if(Type... type) UWVM_THROWS
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
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_for_i32_inc_f64_lt_u_eqz_br_if(TypeRef & ... typeref) UWVM_THROWS
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

    // ----------------------------------------
    // bit_mix: small integer mixers (local.get based)
    // ----------------------------------------

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_xorshift_mix` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_xorshift_mix(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const x_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const a{conbine_details::read_imm<wasm_i32>(type...[0])};
        wasm_i32 const b{conbine_details::read_imm<wasm_i32>(type...[0])};

        wasm_i32 const x{conbine_details::load_local<wasm_i32>(type...[2u], x_off)};

        wasm_i32 const xshr{numeric_details::eval_int_binop<numeric_details::int_binop::shr_u, wasm_i32, numeric_details::wasm_u32>(x, a)};
        wasm_i32 const t1{numeric_details::eval_int_binop<numeric_details::int_binop::xor_, wasm_i32, numeric_details::wasm_u32>(x, xshr)};
        wasm_i32 const xshl{numeric_details::eval_int_binop<numeric_details::int_binop::shl, wasm_i32, numeric_details::wasm_u32>(x, b)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::xor_, wasm_i32, numeric_details::wasm_u32>(t1, xshl)};

        conbine_details::push_operand<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_xorshift_mix` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_xorshift_mix(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const x_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_i32 const a{conbine_details::read_imm<wasm_i32>(typeref...[0])};
        wasm_i32 const b{conbine_details::read_imm<wasm_i32>(typeref...[0])};

        wasm_i32 const x{conbine_details::load_local<wasm_i32>(typeref...[2u], x_off)};

        wasm_i32 const xshr{numeric_details::eval_int_binop<numeric_details::int_binop::shr_u, wasm_i32, numeric_details::wasm_u32>(x, a)};
        wasm_i32 const t1{numeric_details::eval_int_binop<numeric_details::int_binop::xor_, wasm_i32, numeric_details::wasm_u32>(x, xshr)};
        wasm_i32 const xshl{numeric_details::eval_int_binop<numeric_details::int_binop::shl, wasm_i32, numeric_details::wasm_u32>(x, b)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::xor_, wasm_i32, numeric_details::wasm_u32>(t1, xshl)};

        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_rot_xor_add` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_rot_xor_add(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const x_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const y_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const r{conbine_details::read_imm<wasm_i32>(type...[0])};
        wasm_i32 const c{conbine_details::read_imm<wasm_i32>(type...[0])};

        wasm_i32 const x{conbine_details::load_local<wasm_i32>(type...[2u], x_off)};
        wasm_i32 const y{conbine_details::load_local<wasm_i32>(type...[2u], y_off)};

        wasm_i32 const rot{numeric_details::eval_int_binop<numeric_details::int_binop::rotl, wasm_i32, numeric_details::wasm_u32>(x, r)};
        wasm_i32 const xored{numeric_details::eval_int_binop<numeric_details::int_binop::xor_, wasm_i32, numeric_details::wasm_u32>(rot, y)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(xored, c)};

        conbine_details::push_operand<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_rot_xor_add` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_rot_xor_add(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const x_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const y_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_i32 const r{conbine_details::read_imm<wasm_i32>(typeref...[0])};
        wasm_i32 const c{conbine_details::read_imm<wasm_i32>(typeref...[0])};

        wasm_i32 const x{conbine_details::load_local<wasm_i32>(typeref...[2u], x_off)};
        wasm_i32 const y{conbine_details::load_local<wasm_i32>(typeref...[2u], y_off)};

        wasm_i32 const rot{numeric_details::eval_int_binop<numeric_details::int_binop::rotl, wasm_i32, numeric_details::wasm_u32>(x, r)};
        wasm_i32 const xored{numeric_details::eval_int_binop<numeric_details::int_binop::xor_, wasm_i32, numeric_details::wasm_u32>(rot, y)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(xored, c)};

        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    // ----------------------------------------
    // branch_fuse: float compare+branch
    // ----------------------------------------

    /// @brief Fused operand-stack compare + `br_if` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, details::float_cmp Cmp, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f32_cmp(Type... type) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        if constexpr(conbine_details::stacktop_enabled_for<CompileOption, wasm_f32>())
        {
            constexpr ::std::size_t begin{conbine_details::range_begin<CompileOption, wasm_f32>()};
            constexpr ::std::size_t end{conbine_details::range_end<CompileOption, wasm_f32>()};
            static_assert(begin <= curr_f32_stack_top && curr_f32_stack_top < end);
            constexpr ::std::size_t next_pos{details::ring_next_pos(curr_f32_stack_top, begin, end)};

            wasm_f32 const rhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, curr_f32_stack_top>(type...)};
            wasm_f32 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, next_pos>(type...)};
            if(details::eval_float_cmp<Cmp, wasm_f32>(lhs, rhs)) { type...[0] = jmp_ip; }
        }
        else
        {
            wasm_f32 const rhs{get_curr_val_from_operand_stack_cache<wasm_f32>(type...)};
            wasm_f32 const lhs{get_curr_val_from_operand_stack_cache<wasm_f32>(type...)};
            if(details::eval_float_cmp<Cmp, wasm_f32>(lhs, rhs)) { type...[0] = jmp_ip; }
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused operand-stack compare + `br_if` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, details::float_cmp Cmp, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f32_cmp(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 1uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), typeref...[0], sizeof(jmp_ip));
        typeref...[0] += sizeof(jmp_ip);

        wasm_f32 const rhs{get_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
        wasm_f32 const lhs{get_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
        if(details::eval_float_cmp<Cmp, wasm_f32>(lhs, rhs)) { typeref...[0] = jmp_ip; }
    }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_f32_eq` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f32_eq(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_f32_cmp<CompileOption, details::float_cmp::eq>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_f32_lt` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f32_lt(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_f32_cmp<CompileOption, details::float_cmp::lt>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_f32_le` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f32_le(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_f32_cmp<CompileOption, details::float_cmp::le>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_f32_ge` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f32_ge(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_f32_cmp<CompileOption, details::float_cmp::ge>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_f32_gt` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f32_gt(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_f32_cmp<CompileOption, details::float_cmp::gt>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_f32_ne` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f32_ne(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_f32_cmp<CompileOption, details::float_cmp::ne>(typeref...); }

    /// @brief Fused operand-stack compare + `br_if` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, details::float_cmp Cmp, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f64_cmp(Type... type) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        if constexpr(conbine_details::stacktop_enabled_for<CompileOption, wasm_f64>())
        {
            constexpr ::std::size_t begin{conbine_details::range_begin<CompileOption, wasm_f64>()};
            constexpr ::std::size_t end{conbine_details::range_end<CompileOption, wasm_f64>()};
            static_assert(begin <= curr_f64_stack_top && curr_f64_stack_top < end);
            constexpr ::std::size_t next_pos{details::ring_next_pos(curr_f64_stack_top, begin, end)};

            wasm_f64 const rhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, curr_f64_stack_top>(type...)};
            wasm_f64 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, next_pos>(type...)};
            if(details::eval_float_cmp<Cmp, wasm_f64>(lhs, rhs)) { type...[0] = jmp_ip; }
        }
        else
        {
            wasm_f64 const rhs{get_curr_val_from_operand_stack_cache<wasm_f64>(type...)};
            wasm_f64 const lhs{get_curr_val_from_operand_stack_cache<wasm_f64>(type...)};
            if(details::eval_float_cmp<Cmp, wasm_f64>(lhs, rhs)) { type...[0] = jmp_ip; }
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused operand-stack compare + `br_if` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, details::float_cmp Cmp, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f64_cmp(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 1uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), typeref...[0], sizeof(jmp_ip));
        typeref...[0] += sizeof(jmp_ip);

        wasm_f64 const rhs{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        wasm_f64 const lhs{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        if(details::eval_float_cmp<Cmp, wasm_f64>(lhs, rhs)) { typeref...[0] = jmp_ip; }
    }

    /// @brief Fused operand-stack compare + `i32.eqz` + `br_if` (tail-call).
    /// @details
    /// Equivalent to `br_if (i32.eqz (cmp(lhs, rhs)))`, which branches when the compare is false.
    /// This is **not** always expressible as an inverted float compare due to NaN semantics, so we provide a dedicated op.
    ///
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, details::float_cmp Cmp, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f64_cmp_eqz(Type... type) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        if constexpr(conbine_details::stacktop_enabled_for<CompileOption, wasm_f64>())
        {
            constexpr ::std::size_t begin{conbine_details::range_begin<CompileOption, wasm_f64>()};
            constexpr ::std::size_t end{conbine_details::range_end<CompileOption, wasm_f64>()};
            static_assert(begin <= curr_f64_stack_top && curr_f64_stack_top < end);
            constexpr ::std::size_t next_pos{details::ring_next_pos(curr_f64_stack_top, begin, end)};

            wasm_f64 const rhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, curr_f64_stack_top>(type...)};
            wasm_f64 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, next_pos>(type...)};
            if(!details::eval_float_cmp<Cmp, wasm_f64>(lhs, rhs)) { type...[0] = jmp_ip; }
        }
        else
        {
            wasm_f64 const rhs{get_curr_val_from_operand_stack_cache<wasm_f64>(type...)};
            wasm_f64 const lhs{get_curr_val_from_operand_stack_cache<wasm_f64>(type...)};
            if(!details::eval_float_cmp<Cmp, wasm_f64>(lhs, rhs)) { type...[0] = jmp_ip; }
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused operand-stack compare + `i32.eqz` + `br_if` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, details::float_cmp Cmp, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f64_cmp_eqz(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 1uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), typeref...[0], sizeof(jmp_ip));
        typeref...[0] += sizeof(jmp_ip);

        wasm_f64 const rhs{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        wasm_f64 const lhs{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        if(!details::eval_float_cmp<Cmp, wasm_f64>(lhs, rhs)) { typeref...[0] = jmp_ip; }
    }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_f64_eq` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f64_eq(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_f64_cmp<CompileOption, details::float_cmp::eq>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_f64_lt` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f64_lt(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_f64_cmp<CompileOption, details::float_cmp::lt>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_f64_lt_eqz` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_f64_lt_eqz(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_f64_cmp_eqz<CompileOption, details::float_cmp::lt>(typeref...); }

    namespace translate
    {
        template <typename OpWrapper, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_stacktop_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept;

        template <typename OpWrapper, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_stacktop_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept;

        namespace details
        {

            // Heavy combined op wrappers (dense_compute / rare patterns)

            struct i32_rotl_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::rotl, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::rotl, Type...>; }
            };

            struct i32_rotr_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::rotr, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::rotr, Type...>; }
            };

            struct i32_popcnt_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_unop_localget<Opt, numeric_details::int_unop::popcnt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_unop_localget<Opt, numeric_details::int_unop::popcnt, Type...>; }
            };

            struct i32_clz_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_unop_localget<Opt, numeric_details::int_unop::clz, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_unop_localget<Opt, numeric_details::int_unop::clz, Type...>; }
            };

            struct i32_ctz_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_unop_localget<Opt, numeric_details::int_unop::ctz, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_unop_localget<Opt, numeric_details::int_unop::ctz, Type...>; }
            };

            // convert localget fusions
            struct f32_from_i32_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_from_i32_s<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_from_i32_s<Opt, Type...>; }
            };

            struct f32_from_i32_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_from_i32_u<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_from_i32_u<Opt, Type...>; }
            };

            struct i32_from_f32_trunc_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_from_f32_trunc_s<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_from_f32_trunc_s<Opt, Type...>; }
            };

            struct i32_from_f32_trunc_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_from_f32_trunc_u<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_from_f32_trunc_u<Opt, Type...>; }
            };

            struct i32_from_f64_trunc_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_from_f64_trunc_s<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_from_f64_trunc_s<Opt, Type...>; }
            };

            struct i32_from_f64_trunc_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_from_f64_trunc_u<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_from_f64_trunc_u<Opt, Type...>; }
            };

            // f32 localget fusions
            struct f32_add_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop_imm_localget<Opt, numeric_details::float_binop::add, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop_imm_localget<Opt, numeric_details::float_binop::add, Type...>; }
            };

            struct f32_mul_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop_imm_localget<Opt, numeric_details::float_binop::mul, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop_imm_localget<Opt, numeric_details::float_binop::mul, Type...>; }
            };

            struct f32_min_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop_imm_localget<Opt, numeric_details::float_binop::min, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop_imm_localget<Opt, numeric_details::float_binop::min, Type...>; }
            };

            struct f32_max_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop_imm_localget<Opt, numeric_details::float_binop::max, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop_imm_localget<Opt, numeric_details::float_binop::max, Type...>; }
            };

            struct f32_div_from_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_div_from_imm_localget<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_div_from_imm_localget<Opt, Type...>; }
            };

            struct f32_div_from_imm_localtee_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_div_from_imm_localtee<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_div_from_imm_localtee<Opt, Type...>; }
            };

            struct f32_sub_from_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_sub_from_imm_localget<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_sub_from_imm_localget<Opt, Type...>; }
            };

            struct f32_add_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop_2localget<Opt, numeric_details::float_binop::add, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop_2localget<Opt, numeric_details::float_binop::add, Type...>; }
            };

            struct f32_sub_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop_2localget<Opt, numeric_details::float_binop::sub, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop_2localget<Opt, numeric_details::float_binop::sub, Type...>; }
            };

            struct f32_mul_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop_2localget<Opt, numeric_details::float_binop::mul, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop_2localget<Opt, numeric_details::float_binop::mul, Type...>; }
            };

            struct f32_div_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop_2localget<Opt, numeric_details::float_binop::div, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop_2localget<Opt, numeric_details::float_binop::div, Type...>; }
            };

            struct f32_min_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop_2localget<Opt, numeric_details::float_binop::min, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop_2localget<Opt, numeric_details::float_binop::min, Type...>; }
            };

            struct f32_max_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop_2localget<Opt, numeric_details::float_binop::max, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop_2localget<Opt, numeric_details::float_binop::max, Type...>; }
            };

            struct f32_abs_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_unop_localget<Opt, numeric_details::float_unop::abs, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_unop_localget<Opt, numeric_details::float_unop::abs, Type...>; }
            };

            struct f32_neg_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_unop_localget<Opt, numeric_details::float_unop::neg, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_unop_localget<Opt, numeric_details::float_unop::neg, Type...>; }
            };

            struct f32_sqrt_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_unop_localget<Opt, numeric_details::float_unop::sqrt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_unop_localget<Opt, numeric_details::float_unop::sqrt, Type...>; }
            };

            struct f32_mul_add_3localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_mul_addsub_3localget<Opt, false, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_mul_addsub_3localget<Opt, false, Type...>; }
            };

            struct f32_mul_sub_3localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_mul_addsub_3localget<Opt, true, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_mul_addsub_3localget<Opt, true, Type...>; }
            };

            struct f32_mul_add_2mul_4localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_2mul_addsub_4localget<Opt, false, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_2mul_addsub_4localget<Opt, false, Type...>; }
            };

            struct f32_mul_sub_2mul_4localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_2mul_addsub_4localget<Opt, true, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_2mul_addsub_4localget<Opt, true, Type...>; }
            };

            struct f32_2mul_add_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_2mul_addsub_4localget<Opt, false, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_2mul_addsub_4localget<Opt, false, Type...>; }
            };

            // f64 localget fusions
            struct f64_add_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop_imm_localget<Opt, numeric_details::float_binop::add, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop_imm_localget<Opt, numeric_details::float_binop::add, Type...>; }
            };

            struct f64_mul_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop_imm_localget<Opt, numeric_details::float_binop::mul, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop_imm_localget<Opt, numeric_details::float_binop::mul, Type...>; }
            };

            struct f64_min_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop_imm_localget<Opt, numeric_details::float_binop::min, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop_imm_localget<Opt, numeric_details::float_binop::min, Type...>; }
            };

            struct f64_max_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop_imm_localget<Opt, numeric_details::float_binop::max, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop_imm_localget<Opt, numeric_details::float_binop::max, Type...>; }
            };

            struct f64_add_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop_2localget<Opt, numeric_details::float_binop::add, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop_2localget<Opt, numeric_details::float_binop::add, Type...>; }
            };

            struct f64_sub_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop_2localget<Opt, numeric_details::float_binop::sub, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop_2localget<Opt, numeric_details::float_binop::sub, Type...>; }
            };

            struct f64_mul_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop_2localget<Opt, numeric_details::float_binop::mul, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop_2localget<Opt, numeric_details::float_binop::mul, Type...>; }
            };

            struct f64_div_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop_2localget<Opt, numeric_details::float_binop::div, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop_2localget<Opt, numeric_details::float_binop::div, Type...>; }
            };

            struct f64_min_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop_2localget<Opt, numeric_details::float_binop::min, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop_2localget<Opt, numeric_details::float_binop::min, Type...>; }
            };

            struct f64_max_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop_2localget<Opt, numeric_details::float_binop::max, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop_2localget<Opt, numeric_details::float_binop::max, Type...>; }
            };

            struct f64_abs_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_unop_localget<Opt, numeric_details::float_unop::abs, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_unop_localget<Opt, numeric_details::float_unop::abs, Type...>; }
            };

            struct f64_neg_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_unop_localget<Opt, numeric_details::float_unop::neg, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_unop_localget<Opt, numeric_details::float_unop::neg, Type...>; }
            };

            struct f64_sqrt_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_unop_localget<Opt, numeric_details::float_unop::sqrt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_unop_localget<Opt, numeric_details::float_unop::sqrt, Type...>; }
            };

            struct f64_mul_add_3localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_mul_addsub_3localget<Opt, false, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_mul_addsub_3localget<Opt, false, Type...>; }
            };

            struct f64_mul_sub_3localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_mul_addsub_3localget<Opt, true, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_mul_addsub_3localget<Opt, true, Type...>; }
            };

            struct f64_mul_add_2mul_4localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_2mul_addsub_4localget<Opt, false, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_2mul_addsub_4localget<Opt, false, Type...>; }
            };

            struct f64_mul_sub_2mul_4localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_2mul_addsub_4localget<Opt, true, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_2mul_addsub_4localget<Opt, true, Type...>; }
            };

            struct f64_2mul_add_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_2mul_addsub_4localget<Opt, false, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_2mul_addsub_4localget<Opt, false, Type...>; }
            };

            // select_fuse
            struct f32_mac_local_tee_acc_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_mac_local_settee_acc<Opt, true, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_mac_local_settee_acc<Opt, true, Type...>; }
            };

            struct f32_select_local_tee_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_select_local_settee<Opt, true, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_select_local_settee<Opt, true, Type...>; }
            };

            // bit_mix
            struct i32_xorshift_mix_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_xorshift_mix<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_xorshift_mix<Opt, Type...>; }
            };

            struct i32_rot_xor_add_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_rot_xor_add<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_rot_xor_add<Opt, Type...>; }
            };

            // br_if float compare fusions (operand-stack based)
            struct br_if_f32_eq_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_f32_cmp<Opt, op_details::float_cmp::eq, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_f32_cmp<Opt, op_details::float_cmp::eq, Type...>; }
            };

            struct br_if_f32_lt_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_f32_cmp<Opt, op_details::float_cmp::lt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_f32_cmp<Opt, op_details::float_cmp::lt, Type...>; }
            };

            struct br_if_f32_le_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_f32_cmp<Opt, op_details::float_cmp::le, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_f32_cmp<Opt, op_details::float_cmp::le, Type...>; }
            };

            struct br_if_f32_ge_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_f32_cmp<Opt, op_details::float_cmp::ge, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_f32_cmp<Opt, op_details::float_cmp::ge, Type...>; }
            };

            struct br_if_f32_gt_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_f32_cmp<Opt, op_details::float_cmp::gt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_f32_cmp<Opt, op_details::float_cmp::gt, Type...>; }
            };

            struct br_if_f32_ne_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_f32_cmp<Opt, op_details::float_cmp::ne, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_f32_cmp<Opt, op_details::float_cmp::ne, Type...>; }
            };

            struct br_if_f64_eq_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_f64_cmp<Opt, op_details::float_cmp::eq, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_f64_cmp<Opt, op_details::float_cmp::eq, Type...>; }
            };

            struct br_if_f64_lt_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_f64_cmp<Opt, op_details::float_cmp::lt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_f64_cmp<Opt, op_details::float_cmp::lt, Type...>; }
            };

            struct br_if_f64_lt_eqz_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_f64_cmp_eqz<Opt, op_details::float_cmp::lt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_f64_cmp_eqz<Opt, op_details::float_cmp::lt, Type...>; }
            };

        }  // namespace details

        // ========================
        // Heavy combined opcodes
        // ========================

        // i32 rotate/unary localget
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_rotl_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_rotl_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rotl_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rotl_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_rotl_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_rotl_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rotl_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rotl_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_rotr_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_rotr_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rotr_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rotr_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_rotr_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_rotr_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rotr_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rotr_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename OpWrapper, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_unary_localget_fptr_impl(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    OpWrapper,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_popcnt_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_i32_unary_localget_fptr_impl<CompileOption, details::i32_popcnt_localget_op, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_popcnt_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_popcnt_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_popcnt_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_popcnt_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_popcnt_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_popcnt_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_clz_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_i32_unary_localget_fptr_impl<CompileOption, details::i32_clz_localget_op, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_clz_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_clz_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_clz_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_clz_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_clz_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_clz_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_ctz_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_i32_unary_localget_fptr_impl<CompileOption, details::i32_ctz_localget_op, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ctz_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ctz_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_ctz_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_ctz_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ctz_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ctz_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // convert localget fusions
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_from_i32_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_from_i32_s_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_from_i32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_from_i32_s_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_from_i32_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_from_i32_s_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_from_i32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_from_i32_s_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_from_i32_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_from_i32_u_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_from_i32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_from_i32_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_from_i32_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_from_i32_u_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_from_i32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_from_i32_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_from_f32_trunc_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_i32_unary_localget_fptr_impl<CompileOption, details::i32_from_f32_trunc_s_op, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_from_f32_trunc_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_from_f32_trunc_s_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_from_f32_trunc_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_from_f32_trunc_s_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_from_f32_trunc_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_from_f32_trunc_s_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_from_f32_trunc_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_i32_unary_localget_fptr_impl<CompileOption, details::i32_from_f32_trunc_u_op, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_from_f32_trunc_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_from_f32_trunc_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_from_f32_trunc_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_from_f32_trunc_u_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_from_f32_trunc_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_from_f32_trunc_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_from_f64_trunc_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_i32_unary_localget_fptr_impl<CompileOption, details::i32_from_f64_trunc_s_op, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_from_f64_trunc_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_from_f64_trunc_s_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_from_f64_trunc_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_from_f64_trunc_s_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_from_f64_trunc_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_from_f64_trunc_s_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_from_f64_trunc_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_i32_unary_localget_fptr_impl<CompileOption, details::i32_from_f64_trunc_u_op, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_from_f64_trunc_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_from_f64_trunc_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_from_f64_trunc_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_from_f64_trunc_u_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_from_f64_trunc_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_from_f64_trunc_u_fptr<CompileOption, TypeInTuple...>(curr); }

        // float localget ops helper
        template <typename T>
        inline constexpr ::std::size_t stacktop_currpos_for(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(::std::same_as<T, conbine_details::wasm_f32>) { return curr.f32_stack_top_curr_pos; }
            else
            {
                return curr.f64_stack_top_curr_pos;
            }
        }

        template <typename T, typename OpWrapper, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_float_localget_fptr_impl(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(::std::same_as<T, conbine_details::wasm_f32>)
            {
                return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                        CompileOption.f32_stack_top_begin_pos,
                                                                        CompileOption.f32_stack_top_end_pos,
                                                                        OpWrapper,
                                                                        Type...>(curr.f32_stack_top_curr_pos);
            }
            else
            {
                return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                        CompileOption.f64_stack_top_begin_pos,
                                                                        CompileOption.f64_stack_top_end_pos,
                                                                        OpWrapper,
                                                                        Type...>(curr.f64_stack_top_curr_pos);
            }
        }

        // f32 localget binops/unops
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_add_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_float_localget_fptr_impl<conbine_details::wasm_f32, details::f32_add_imm_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_add_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_add_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_add_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_add_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_add_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_add_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_mul_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_float_localget_fptr_impl<conbine_details::wasm_f32, details::f32_mul_imm_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_mul_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_mul_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_min_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_float_localget_fptr_impl<conbine_details::wasm_f32, details::f32_min_imm_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_min_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_min_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_min_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_min_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_min_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_min_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_max_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_float_localget_fptr_impl<conbine_details::wasm_f32, details::f32_max_imm_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_max_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_max_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_max_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_max_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_max_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_max_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_div_from_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_float_localget_fptr_impl<conbine_details::wasm_f32, details::f32_div_from_imm_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_div_from_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_div_from_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_f32_div_from_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_div_from_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_div_from_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_div_from_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_div_from_imm_localtee_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_float_localget_fptr_impl<conbine_details::wasm_f32, details::f32_div_from_imm_localtee_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_div_from_imm_localtee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_div_from_imm_localtee_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_f32_div_from_imm_localtee_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_div_from_imm_localtee_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_div_from_imm_localtee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_div_from_imm_localtee_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_sub_from_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_float_localget_fptr_impl<conbine_details::wasm_f32, details::f32_sub_from_imm_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_sub_from_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_sub_from_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_f32_sub_from_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_sub_from_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_sub_from_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_sub_from_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // f32 2local and unop helpers
        template <typename OpWrapper, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_stacktop_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.f32_stack_top_begin_pos,
                                                                    CompileOption.f32_stack_top_end_pos,
                                                                    OpWrapper,
                                                                    Type...>(curr.f32_stack_top_curr_pos);
        }

        template <typename OpWrapper, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_stacktop_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.f64_stack_top_begin_pos,
                                                                    CompileOption.f64_stack_top_end_pos,
                                                                    OpWrapper,
                                                                    Type...>(curr.f64_stack_top_curr_pos);
        }

        // f32 add/sub/mul/div/min/max (2localget)
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_add_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_add_2localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_add_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_add_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_add_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_add_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_add_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_add_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // For the remaining float heavy ops, reuse the same wrappers but keep separate entry points for clarity.
        // f32_sub_2localget
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_sub_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_sub_2localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_sub_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_sub_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_sub_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_sub_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_sub_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_sub_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // f32_mul_2localget
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_mul_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_mul_2localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_mul_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_mul_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // f32_div_2localget
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_div_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_div_2localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_div_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_div_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_div_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_div_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_div_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_div_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // f32_min_2localget
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_min_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_min_2localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_min_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_min_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_min_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_min_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_min_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_min_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // f32_max_2localget
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_max_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_max_2localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_max_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_max_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_max_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_max_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_max_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_max_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // f32 unary
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_abs_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_abs_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_abs_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_abs_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_abs_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_abs_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_abs_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_abs_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_neg_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_neg_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_neg_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_neg_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_neg_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_neg_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_neg_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_neg_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_sqrt_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_sqrt_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_sqrt_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_sqrt_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_sqrt_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f32_sqrt_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_sqrt_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_sqrt_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // f32 mul-add/sub (3localget)
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_mul_add_3localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_mul_add_3localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_add_3localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_add_3localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_mul_add_3localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_mul_add_3localget<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_add_3localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_add_3localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_mul_sub_3localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_mul_sub_3localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_sub_3localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_sub_3localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_mul_sub_3localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_mul_sub_3localget<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_sub_3localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_sub_3localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // f32 2mul add/sub
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_mul_add_2mul_4localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_mul_add_2mul_4localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_add_2mul_4localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_add_2mul_4localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_f32_mul_add_2mul_4localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_mul_add_2mul_4localget<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_add_2mul_4localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_add_2mul_4localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_mul_sub_2mul_4localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_mul_sub_2mul_4localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_sub_2mul_4localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_sub_2mul_4localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_f32_mul_sub_2mul_4localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_mul_sub_2mul_4localget<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_sub_2mul_4localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_sub_2mul_4localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_2mul_add_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f32_stacktop_fptr<details::f32_2mul_add_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_2mul_add_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_2mul_add_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_2mul_add_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_2mul_add<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_2mul_add_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_2mul_add_fptr<CompileOption, TypeInTuple...>(curr); }

        // f64 (symmetrical)
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_add_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_float_localget_fptr_impl<conbine_details::wasm_f64, details::f64_add_imm_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_add_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_add_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_add_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f64_add_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_add_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_add_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_mul_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_float_localget_fptr_impl<conbine_details::wasm_f64, details::f64_mul_imm_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_mul_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f64_mul_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_min_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_float_localget_fptr_impl<conbine_details::wasm_f64, details::f64_min_imm_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_min_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_min_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_min_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f64_min_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_min_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_min_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_max_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_float_localget_fptr_impl<conbine_details::wasm_f64, details::f64_max_imm_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_max_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_max_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_max_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f64_max_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_max_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_max_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // f64 add/sub/mul/div/min/max (2localget)
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_add_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f64_stacktop_fptr<details::f64_add_2localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_add_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_add_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_add_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f64_add_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_add_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_add_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_sub_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f64_stacktop_fptr<details::f64_sub_2localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_sub_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_sub_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_sub_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f64_sub_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_sub_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_sub_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_mul_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f64_stacktop_fptr<details::f64_mul_2localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_mul_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f64_mul_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_div_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f64_stacktop_fptr<details::f64_div_2localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_div_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_div_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_div_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f64_div_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_div_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_div_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_min_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f64_stacktop_fptr<details::f64_min_2localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_min_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_min_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_min_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f64_min_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_min_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_min_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_max_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f64_stacktop_fptr<details::f64_max_2localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_max_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_max_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_max_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f64_max_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_max_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_max_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // f64 unary
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_abs_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f64_stacktop_fptr<details::f64_abs_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_abs_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_abs_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_abs_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f64_abs_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_abs_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_abs_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_neg_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f64_stacktop_fptr<details::f64_neg_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_neg_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_neg_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_neg_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f64_neg_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_neg_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_neg_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_sqrt_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f64_stacktop_fptr<details::f64_sqrt_localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_sqrt_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_sqrt_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_sqrt_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::f64_sqrt_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_sqrt_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_sqrt_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // f64 mul-add/sub (3localget)
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_mul_add_3localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f64_stacktop_fptr<details::f64_mul_add_3localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_add_3localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_add_3localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_mul_add_3localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_mul_add_3localget<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_add_3localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_add_3localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_mul_sub_3localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f64_stacktop_fptr<details::f64_mul_sub_3localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_sub_3localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_sub_3localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_mul_sub_3localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_mul_sub_3localget<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_sub_3localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_sub_3localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // f64 2mul add/sub + alias
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_mul_add_2mul_4localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f64_stacktop_fptr<details::f64_mul_add_2mul_4localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_add_2mul_4localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_add_2mul_4localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_f64_mul_add_2mul_4localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_mul_add_2mul_4localget<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_add_2mul_4localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_add_2mul_4localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_mul_sub_2mul_4localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f64_stacktop_fptr<details::f64_mul_sub_2mul_4localget_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_sub_2mul_4localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_sub_2mul_4localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_f64_mul_sub_2mul_4localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_mul_sub_2mul_4localget<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_sub_2mul_4localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_sub_2mul_4localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_2mul_add_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_f64_stacktop_fptr<details::f64_2mul_add_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_2mul_add_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_2mul_add_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_2mul_add_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_2mul_add<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_2mul_add_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_2mul_add_fptr<CompileOption, TypeInTuple...>(curr); }

        // update_local: MAC patterns (set/tee)
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_mac_local_set_acc_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_mac_local_settee_acc<CompileOption, false, 0uz, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mac_local_set_acc_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mac_local_set_acc_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_mac_local_set_acc_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_mac_local_set_acc<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mac_local_set_acc_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mac_local_set_acc_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_mac_local_tee_acc_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.f32_stack_top_begin_pos,
                                                                    CompileOption.f32_stack_top_end_pos,
                                                                    details::f32_mac_local_tee_acc_op,
                                                                    Type...>(curr.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mac_local_tee_acc_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mac_local_tee_acc_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_mac_local_tee_acc_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_mac_local_tee_acc<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mac_local_tee_acc_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mac_local_tee_acc_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_mac_local_set_acc_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_mac_local_set_acc<CompileOption, 0uz, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mac_local_set_acc_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mac_local_set_acc_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_mac_local_set_acc_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_mac_local_set_acc<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mac_local_set_acc_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mac_local_set_acc_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_mac_local_set_acc_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_mac_local_set_acc<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_mac_local_set_acc_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_mac_local_set_acc_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_mac_local_set_acc_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_mac_local_set_acc<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_mac_local_set_acc_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_mac_local_set_acc_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_mac_local_set_acc_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_mac_local_set_acc<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_mac_local_set_acc_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_mac_local_set_acc_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_mac_local_set_acc_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_mac_local_set_acc<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_mac_local_set_acc_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_mac_local_set_acc_fptr<CompileOption, TypeInTuple...>(curr); }

        // select_fuse
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_select_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_select_local_settee<CompileOption, false, 0uz, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_select_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_select_local_set_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_select_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_select_local_set<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_select_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_select_local_set_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_select_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.f32_stack_top_begin_pos,
                                                                    CompileOption.f32_stack_top_end_pos,
                                                                    details::f32_select_local_tee_op,
                                                                    Type...>(curr.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_select_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_select_local_tee_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_select_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_select_local_tee<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_select_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_select_local_tee_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_select_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_select_local_set<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_select_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_select_local_set_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_select_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_select_local_set<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_select_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_select_local_set_fptr<CompileOption, TypeInTuple...>(curr); }

        // br_if fusions
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_rem_u_eqz_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i32_rem_u_eqz_2localget<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_rem_u_eqz_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_rem_u_eqz_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_rem_u_eqz_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i32_rem_u_eqz_2localget<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_rem_u_eqz_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_rem_u_eqz_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

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

        // bit_mix (i32 stacktop)
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_xorshift_mix_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_xorshift_mix_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_xorshift_mix_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_xorshift_mix_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_xorshift_mix_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_xorshift_mix<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_xorshift_mix_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_xorshift_mix_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_rot_xor_add_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_rot_xor_add_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rot_xor_add_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rot_xor_add_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_rot_xor_add_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_rot_xor_add<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rot_xor_add_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rot_xor_add_fptr<CompileOption, TypeInTuple...>(curr); }

        // br_if f32 (operand stack)
        template <typename OpWrapper, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_f32_fptr_impl(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.f32_stack_top_begin_pos,
                                                                    CompileOption.f32_stack_top_end_pos,
                                                                    OpWrapper,
                                                                    Type...>(curr.f32_stack_top_curr_pos);
        }

        template <typename OpWrapper, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_f64_fptr_impl(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.f64_stack_top_begin_pos,
                                                                    CompileOption.f64_stack_top_end_pos,
                                                                    OpWrapper,
                                                                    Type...>(curr.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_f32_eq_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_br_if_f32_fptr_impl<details::br_if_f32_eq_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f32_eq_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f32_eq_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_f32_eq_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_f32_eq_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f32_eq_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f32_eq_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_f32_lt_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_br_if_f32_fptr_impl<details::br_if_f32_lt_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f32_lt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f32_lt_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_f32_lt_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_f32_lt_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f32_lt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f32_lt_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_f32_le_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_br_if_f32_fptr_impl<details::br_if_f32_le_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f32_le_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f32_le_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_f32_le_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_f32_le_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f32_le_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f32_le_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_f32_ge_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_br_if_f32_fptr_impl<details::br_if_f32_ge_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f32_ge_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f32_ge_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_f32_ge_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_f32_ge_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f32_ge_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f32_ge_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_f32_gt_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_br_if_f32_fptr_impl<details::br_if_f32_gt_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f32_gt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f32_gt_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_f32_gt_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_f32_gt_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f32_gt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f32_gt_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_f32_ne_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_br_if_f32_fptr_impl<details::br_if_f32_ne_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f32_ne_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f32_ne_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_f32_ne_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_f32_ne_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f32_ne_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f32_ne_fptr<CompileOption, TypeInTuple...>(curr); }

        // br_if f64 (eq/lt)
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_f64_eq_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_br_if_f64_fptr_impl<details::br_if_f64_eq_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f64_eq_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f64_eq_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_f64_eq_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_f64_eq_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f64_eq_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f64_eq_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_f64_lt_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_br_if_f64_fptr_impl<details::br_if_f64_lt_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f64_lt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f64_lt_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_f64_lt_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_f64_lt_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f64_lt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f64_lt_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_f64_lt_eqz_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_br_if_f64_fptr_impl<details::br_if_f64_lt_eqz_op, CompileOption, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f64_lt_eqz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f64_lt_eqz_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_f64_lt_eqz_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_f64_lt_eqz_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_f64_lt_eqz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_f64_lt_eqz_fptr<CompileOption, TypeInTuple...>(curr); }

    }  // namespace translate

    namespace details::memop
    {
        /// @brief Internal fused memory load (`f32`) via `local.get` address + `offset` immediate (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f32_load_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f32 = details::wasm_f32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const local_off{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], local_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 4uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 4uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            wasm_f32 const out{details::load_f32_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            push_value<CompileOption, wasm_f32, curr_f32_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load (`f32`) via `local.get` + immediate add + `offset` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: `local_offset_t`, `wasm_i32 imm`, `native_memory_t*`, `wasm_u32 offset`.
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f32_load_local_plus_imm(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f32 = details::wasm_f32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const local_off{details::read_imm<local_offset_t>(type...[0])};
            wasm_i32 const imm{details::read_imm<wasm_i32>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const base{load_local<wasm_i32>(type...[2u], local_off)};
            wasm_i32 const addr{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(base, imm)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 4uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 4uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            wasm_f32 const out{details::load_f32_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            push_value<CompileOption, wasm_f32, curr_f32_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load (`f64`) via `local.get` address + `offset` immediate (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f64_load_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f64 = details::wasm_f64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const local_off{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], local_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 8uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 8uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            wasm_f64 const out{details::load_f64_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            push_value<CompileOption, wasm_f64, curr_f64_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load (`f64`) via `local.get` + immediate add + `offset` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: `local_offset_t`, `wasm_i32 imm`, `native_memory_t*`, `wasm_u32 offset`.
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f64_load_local_plus_imm(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f64 = details::wasm_f64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const local_off{details::read_imm<local_offset_t>(type...[0])};
            wasm_i32 const imm{details::read_imm<wasm_i32>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const base{load_local<wasm_i32>(type...[2u], local_off)};
            wasm_i32 const addr{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(base, imm)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 8uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 8uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            wasm_f64 const out{details::load_f64_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            push_value<CompileOption, wasm_f64, curr_f64_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory store (`f32`) via `local.get` address + `offset` immediate (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f32_store_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f32 = details::wasm_f32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            local_offset_t const v_off{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], p_off)};
            wasm_f32 const v{load_local<wasm_f32>(type...[2u], v_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 4uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 4uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            details::store_f32_le(details::ptr_add_u64(memory.memory_begin, eff), v);
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory store (`f32`) via `local.get` + immediate add + `offset` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: `local_offset_t` (addr), `wasm_i32 imm`, `local_offset_t` (value), `native_memory_t*`, `wasm_u32 offset`.
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f32_store_local_plus_imm(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f32 = details::wasm_f32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            wasm_i32 const imm{details::read_imm<wasm_i32>(type...[0])};
            local_offset_t const v_off{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const base{load_local<wasm_i32>(type...[2u], p_off)};
            wasm_i32 const addr{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(base, imm)};
            wasm_f32 const v{load_local<wasm_f32>(type...[2u], v_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 4uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 4uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            details::store_f32_le(details::ptr_add_u64(memory.memory_begin, eff), v);
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory store immediate (`f32`) via `local.get` address + `offset` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f32_store_imm_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f32 = details::wasm_f32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            wasm_f32 const imm{details::read_imm<wasm_f32>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], p_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 4uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 4uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            details::store_f32_le(details::ptr_add_u64(memory.memory_begin, eff), imm);
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory store (`f64`) via `local.get` address + `offset` immediate (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f64_store_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f64 = details::wasm_f64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            local_offset_t const v_off{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], p_off)};
            wasm_f64 const v{load_local<wasm_f64>(type...[2u], v_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 8uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 8uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            details::store_f64_le(details::ptr_add_u64(memory.memory_begin, eff), v);
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory store (`f64`) via `local.get` + immediate add + `offset` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: `local_offset_t` (addr), `wasm_i32 imm`, `local_offset_t` (value), `native_memory_t*`, `wasm_u32 offset`.
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f64_store_local_plus_imm(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f64 = details::wasm_f64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            wasm_i32 const imm{details::read_imm<wasm_i32>(type...[0])};
            local_offset_t const v_off{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const base{load_local<wasm_i32>(type...[2u], p_off)};
            wasm_i32 const addr{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(base, imm)};
            wasm_f64 const v{load_local<wasm_f64>(type...[2u], v_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 8uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 8uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            details::store_f64_le(details::ptr_add_u64(memory.memory_begin, eff), v);
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory store immediate (`f64`) via `local.get` address + `offset` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f64_store_imm_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f64 = details::wasm_f64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            wasm_f64 const imm{details::read_imm<wasm_f64>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], p_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 8uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 8uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            details::store_f64_le(details::ptr_add_u64(memory.memory_begin, eff), imm);
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load (`f32`) with `local.set` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f32_load_localget_set_local(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f32 = details::wasm_f32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            local_offset_t const dst_off{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], p_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 4uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 4uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            wasm_f32 const out{details::load_f32_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            store_local(type...[2u], dst_off, out);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load (`f32`) with `local.tee` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f32_load_localget_tee_local(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f32 = details::wasm_f32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            local_offset_t const dst_off{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], p_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 4uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 4uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            wasm_f32 const out{details::load_f32_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            store_local(type...[2u], dst_off, out);
            push_value<CompileOption, wasm_f32, curr_f32_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load (`f64`) with `local.set` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f64_load_localget_set_local(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f64 = details::wasm_f64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            local_offset_t const dst_off{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], p_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 8uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 8uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            wasm_f64 const out{details::load_f64_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            store_local(type...[2u], dst_off, out);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load (`f64`) with `local.tee` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f64_load_localget_tee_local(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f64 = details::wasm_f64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            local_offset_t const dst_off{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], p_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 8uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 8uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            wasm_f64 const out{details::load_f64_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            store_local(type...[2u], dst_off, out);
            push_value<CompileOption, wasm_f64, curr_f64_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        // -------------------------------------------------
        // compound_mem: u16_copy_scaled_index (net 0)
        // Sequence:
        //   local.get dst; local.get idx; i32.const sh; i32.shl;
        //   i32.load16_u offset=src_off; i32.store16 offset=dst_off
        //
        // Layout: [op][dst_local_off][idx_local_off][sh:i32][memory*][src_off:u32][dst_off:u32][next]
        // -------------------------------------------------

        /// @brief Internal fused u16 copy with scaled index (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void u16_copy_scaled_index(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const dst_local_off{details::read_imm<local_offset_t>(type...[0])};
            local_offset_t const idx_local_off{details::read_imm<local_offset_t>(type...[0])};
            wasm_i32 const sh{details::read_imm<wasm_i32>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const src_static_off{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const dst_static_off{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const dst_addr{load_local<wasm_i32>(type...[2u], dst_local_off)};
            wasm_i32 const idx{load_local<wasm_i32>(type...[2u], idx_local_off)};

            ::std::uint_least32_t const idx_u{::std::bit_cast<::std::uint_least32_t>(idx)};
            ::std::uint_least32_t const sh_u{static_cast<::std::uint_least32_t>(::std::bit_cast<::std::uint_least32_t>(sh) & 31u)};
            wasm_i32 const src_addr{::std::bit_cast<wasm_i32>(static_cast<::std::uint_least32_t>(idx_u << sh_u))};

            auto const src_eff65{details::wasm32_effective_offset(src_addr, src_static_off)};
            auto const dst_eff65{details::wasm32_effective_offset(dst_addr, dst_static_off)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);

            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, src_eff65, 2uz) || details::should_trap_oob_unlocked(memory, dst_eff65, 2uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    // Prefer reporting the first failing access (src first).
                    if(details::should_trap_oob_unlocked(memory, src_eff65, 2uz))
                    {
                        details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(src_static_off), src_eff65, memory_length, 2uz);
                    }
                    else
                    {
                        details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(dst_static_off), dst_eff65, memory_length, 2uz);
                    }
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(src_static_off), src_eff65, 2uz);
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(dst_static_off), dst_eff65, 2uz);
            }

            ::std::size_t const src_eff{static_cast<::std::size_t>(src_eff65.offset)};
            ::std::size_t const dst_eff{static_cast<::std::size_t>(dst_eff65.offset)};

            ::std::uint_least16_t tmp;  // no init
            ::std::memcpy(::std::addressof(tmp), details::ptr_add_u64(memory.memory_begin, src_eff), sizeof(tmp));
            tmp = ::fast_io::little_endian(tmp);
            details::store_u16_le(details::ptr_add_u64(memory.memory_begin, dst_eff), tmp);

            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memcpy (`f32`): `local.get` dst/src + load + store (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f32_memcpy_localget_localget(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f32 = details::wasm_f32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const dst_off{details::read_imm<local_offset_t>(type...[0])};
            local_offset_t const src_off{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const src_static_off{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const dst_static_off{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const dst_addr{load_local<wasm_i32>(type...[2u], dst_off)};
            wasm_i32 const src_addr{load_local<wasm_i32>(type...[2u], src_off)};

            auto const src_eff65{details::wasm32_effective_offset(src_addr, src_static_off)};
            auto const dst_eff65{details::wasm32_effective_offset(dst_addr, dst_static_off)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);

            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, src_eff65, 4uz) || details::should_trap_oob_unlocked(memory, dst_eff65, 4uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    if(details::should_trap_oob_unlocked(memory, src_eff65, 4uz))
                    {
                        details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(src_static_off), src_eff65, memory_length, 4uz);
                    }
                    else
                    {
                        details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(dst_static_off), dst_eff65, memory_length, 4uz);
                    }
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(src_static_off), src_eff65, 4uz);
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(dst_static_off), dst_eff65, 4uz);
            }

            ::std::size_t const src_eff{static_cast<::std::size_t>(src_eff65.offset)};
            ::std::size_t const dst_eff{static_cast<::std::size_t>(dst_eff65.offset)};

            wasm_f32 const tmp{details::load_f32_le(details::ptr_add_u64(memory.memory_begin, src_eff))};
            details::store_f32_le(details::ptr_add_u64(memory.memory_begin, dst_eff), tmp);

            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memcpy (`f64`): `local.get` dst/src + load + store (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void f64_memcpy_localget_localget(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_f64 = details::wasm_f64;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const dst_off{details::read_imm<local_offset_t>(type...[0])};
            local_offset_t const src_off{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const src_static_off{details::read_imm<wasm_u32>(type...[0])};
            wasm_u32 const dst_static_off{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const dst_addr{load_local<wasm_i32>(type...[2u], dst_off)};
            wasm_i32 const src_addr{load_local<wasm_i32>(type...[2u], src_off)};

            auto const src_eff65{details::wasm32_effective_offset(src_addr, src_static_off)};
            auto const dst_eff65{details::wasm32_effective_offset(dst_addr, dst_static_off)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);

            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, src_eff65, 8uz) || details::should_trap_oob_unlocked(memory, dst_eff65, 8uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    if(details::should_trap_oob_unlocked(memory, src_eff65, 8uz))
                    {
                        details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(src_static_off), src_eff65, memory_length, 8uz);
                    }
                    else
                    {
                        details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(dst_static_off), dst_eff65, memory_length, 8uz);
                    }
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(src_static_off), src_eff65, 8uz);
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(dst_static_off), dst_eff65, 8uz);
            }

            ::std::size_t const src_eff{static_cast<::std::size_t>(src_eff65.offset)};
            ::std::size_t const dst_eff{static_cast<::std::size_t>(dst_eff65.offset)};

            wasm_f64 const tmp{details::load_f64_le(details::ptr_add_u64(memory.memory_begin, src_eff))};
            details::store_f64_le(details::ptr_add_u64(memory.memory_begin, dst_eff), tmp);

            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

    }  // namespace details::memop

    /// @brief Fused memory op with `local.get` address + `offset` immediate (`f32`) (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `native_memory_t*`, `wasm_u32 offset`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_load_localget_off(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f32 = details::wasm_f32;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], local_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        wasm_f32 const out{details::load_f32_le(details::ptr_add_u64(memory.memory_begin, eff))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused memory op with `local.get` + immediate add + `offset` (`f32`) (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32 imm`, `native_memory_t*`, `wasm_u32 offset`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_load_local_plus_imm(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f32 = details::wasm_f32;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        wasm_i32 const imm{details::read_imm<wasm_i32>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const base{details::memop::load_local<wasm_i32>(typeref...[2u], local_off)};
        wasm_i32 const addr{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(base, imm)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        wasm_f32 const out{details::load_f32_le(details::ptr_add_u64(memory.memory_begin, eff))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused memory op with `local.get` address + `offset` immediate (`f64`) (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `native_memory_t*`, `wasm_u32 offset`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_load_localget_off(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f64 = details::wasm_f64;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], local_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        wasm_f64 const out{details::load_f64_le(details::ptr_add_u64(memory.memory_begin, eff))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused memory op with `local.get` + immediate add + `offset` (`f64`) (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32 imm`, `native_memory_t*`, `wasm_u32 offset`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_load_local_plus_imm(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f64 = details::wasm_f64;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const local_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        wasm_i32 const imm{details::read_imm<wasm_i32>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const base{details::memop::load_local<wasm_i32>(typeref...[2u], local_off)};
        wasm_i32 const addr{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(base, imm)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        wasm_f64 const out{details::load_f64_le(details::ptr_add_u64(memory.memory_begin, eff))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_store_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_store_localget_off(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f32 = details::wasm_f32;
        using wasm_u32 = details::wasm_u32;
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        auto const v_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        wasm_f32 const v{details::memop::load_local<wasm_f32>(typeref...[2u], v_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_f32_le(details::ptr_add_u64(memory.memory_begin, eff), v);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_store_local_plus_imm` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_store_local_plus_imm(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f32 = details::wasm_f32;
        using wasm_u32 = details::wasm_u32;
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        wasm_i32 const imm{details::read_imm<wasm_i32>(typeref...[0])};
        auto const v_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const base{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        wasm_i32 const addr{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(base, imm)};
        wasm_f32 const v{details::memop::load_local<wasm_f32>(typeref...[2u], v_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_f32_le(details::ptr_add_u64(memory.memory_begin, eff), v);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_store_imm_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_store_imm_localget_off(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f32 = details::wasm_f32;
        using wasm_u32 = details::wasm_u32;
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        wasm_f32 const imm{details::read_imm<wasm_f32>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_f32_le(details::ptr_add_u64(memory.memory_begin, eff), imm);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_store_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_store_localget_off(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f64 = details::wasm_f64;
        using wasm_u32 = details::wasm_u32;
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        auto const v_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        wasm_f64 const v{details::memop::load_local<wasm_f64>(typeref...[2u], v_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_f64_le(details::ptr_add_u64(memory.memory_begin, eff), v);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_store_local_plus_imm` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_store_local_plus_imm(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f64 = details::wasm_f64;
        using wasm_u32 = details::wasm_u32;
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        wasm_i32 const imm{details::read_imm<wasm_i32>(typeref...[0])};
        auto const v_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const base{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        wasm_i32 const addr{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(base, imm)};
        wasm_f64 const v{details::memop::load_local<wasm_f64>(typeref...[2u], v_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_f64_le(details::ptr_add_u64(memory.memory_begin, eff), v);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_store_imm_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_store_imm_localget_off(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f64 = details::wasm_f64;
        using wasm_u32 = details::wasm_u32;
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        wasm_f64 const imm{details::read_imm<wasm_f64>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_f64_le(details::ptr_add_u64(memory.memory_begin, eff), imm);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f32_load_localget_set_local` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_load_localget_set_local(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f32 = details::wasm_f32;
        using wasm_u32 = details::wasm_u32;
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        auto const dst_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        wasm_f32 const out{details::load_f32_le(details::ptr_add_u64(memory.memory_begin, eff))};
        details::memop::store_local(typeref...[2u], dst_off, out);
    }

    /// @brief Fused memory load + `local.tee` (`f32`) (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (addr), `local_offset_t` (dst), `native_memory_t*`, `wasm_u32 offset`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_load_localget_tee_local(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f32 = details::wasm_f32;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        auto const dst_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        wasm_f32 const out{details::load_f32_le(details::ptr_add_u64(memory.memory_begin, eff))};
        details::memop::store_local(typeref...[2u], dst_off, out);

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_f64_load_localget_set_local` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_load_localget_set_local(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f64 = details::wasm_f64;
        using wasm_u32 = details::wasm_u32;
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        auto const dst_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        wasm_f64 const out{details::load_f64_le(details::ptr_add_u64(memory.memory_begin, eff))};
        details::memop::store_local(typeref...[2u], dst_off, out);
    }

    /// @brief Fused memory load + `local.tee` (`f64`) (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (addr), `local_offset_t` (dst), `native_memory_t*`, `wasm_u32 offset`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_load_localget_tee_local(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f64 = details::wasm_f64;
        using wasm_u32 = details::wasm_u32;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        auto const dst_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        wasm_f64 const out{details::load_f64_le(details::ptr_add_u64(memory.memory_begin, eff))};
        details::memop::store_local(typeref...[2u], dst_off, out);

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_u16_copy_scaled_index` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_u16_copy_scaled_index(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_u32 = details::wasm_u32;
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

        auto const dst_local_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        auto const idx_local_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        wasm_i32 const sh{details::read_imm<wasm_i32>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const src_static_off{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const dst_static_off{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const dst_addr{details::memop::load_local<wasm_i32>(typeref...[2u], dst_local_off)};
        wasm_i32 const idx{details::memop::load_local<wasm_i32>(typeref...[2u], idx_local_off)};

        ::std::uint_least32_t const idx_u{::std::bit_cast<::std::uint_least32_t>(idx)};
        ::std::uint_least32_t const sh_u{static_cast<::std::uint_least32_t>(::std::bit_cast<::std::uint_least32_t>(sh) & 31u)};
        wasm_i32 const src_addr{::std::bit_cast<wasm_i32>(static_cast<::std::uint_least32_t>(idx_u << sh_u))};

        auto const src_eff65{details::wasm32_effective_offset(src_addr, src_static_off)};
        auto const dst_eff65{details::wasm32_effective_offset(dst_addr, dst_static_off)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(src_static_off), src_eff65, 2uz);
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(dst_static_off), dst_eff65, 2uz);

        ::std::size_t const src_eff{static_cast<::std::size_t>(src_eff65.offset)};
        ::std::size_t const dst_eff{static_cast<::std::size_t>(dst_eff65.offset)};

        ::std::uint_least16_t tmp;  // no init
        ::std::memcpy(::std::addressof(tmp), details::ptr_add_u64(memory.memory_begin, src_eff), sizeof(tmp));
        tmp = ::fast_io::little_endian(tmp);
        details::store_u16_le(details::ptr_add_u64(memory.memory_begin, dst_eff), tmp);
    }

    /// @brief Fused `local.get` + `f32.memcpy.localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_memcpy_localget_localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f32 = details::wasm_f32;
        using wasm_u32 = details::wasm_u32;
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

        auto const dst_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        auto const src_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const src_static_off{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const dst_static_off{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const dst_addr{details::memop::load_local<wasm_i32>(typeref...[2u], dst_off)};
        wasm_i32 const src_addr{details::memop::load_local<wasm_i32>(typeref...[2u], src_off)};

        auto const src_eff65{details::wasm32_effective_offset(src_addr, src_static_off)};
        auto const dst_eff65{details::wasm32_effective_offset(dst_addr, dst_static_off)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(src_static_off), src_eff65, 4uz);
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(dst_static_off), dst_eff65, 4uz);

        ::std::size_t const src_eff{static_cast<::std::size_t>(src_eff65.offset)};
        ::std::size_t const dst_eff{static_cast<::std::size_t>(dst_eff65.offset)};

        wasm_f32 const tmp{details::load_f32_le(details::ptr_add_u64(memory.memory_begin, src_eff))};
        details::store_f32_le(details::ptr_add_u64(memory.memory_begin, dst_eff), tmp);
    }

    /// @brief Fused `local.get` + `f64.memcpy.localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_memcpy_localget_localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_f64 = details::wasm_f64;
        using wasm_u32 = details::wasm_u32;
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

        auto const dst_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        auto const src_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const src_static_off{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_u32 const dst_static_off{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const dst_addr{details::memop::load_local<wasm_i32>(typeref...[2u], dst_off)};
        wasm_i32 const src_addr{details::memop::load_local<wasm_i32>(typeref...[2u], src_off)};

        auto const src_eff65{details::wasm32_effective_offset(src_addr, src_static_off)};
        auto const dst_eff65{details::wasm32_effective_offset(dst_addr, dst_static_off)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(src_static_off), src_eff65, 8uz);
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(dst_static_off), dst_eff65, 8uz);

        ::std::size_t const src_eff{static_cast<::std::size_t>(src_eff65.offset)};
        ::std::size_t const dst_eff{static_cast<::std::size_t>(dst_eff65.offset)};

        wasm_f64 const tmp{details::load_f64_le(details::ptr_add_u64(memory.memory_begin, src_eff))};
        details::store_f64_le(details::ptr_add_u64(memory.memory_begin, dst_eff), tmp);
    }

    namespace translate
    {
        namespace details
        {

            struct f32_load_localget_off_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::f32_load_localget_off<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct f32_load_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::f32_load_localget_off<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct f32_load_local_plus_imm_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::f32_load_local_plus_imm<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct f32_load_local_plus_imm_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::f32_load_local_plus_imm<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct f64_load_localget_off_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::f64_load_localget_off<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct f64_load_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::f64_load_localget_off<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct f64_load_local_plus_imm_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::f64_load_local_plus_imm<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct f64_load_local_plus_imm_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::f64_load_local_plus_imm<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct f32_store_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::f32_store_localget_off<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct f32_store_local_plus_imm_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::f32_store_local_plus_imm<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct f32_store_imm_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::f32_store_imm_localget_off<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct f64_store_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::f64_store_localget_off<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct f64_store_local_plus_imm_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::f64_store_local_plus_imm<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct f64_store_imm_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::f64_store_imm_localget_off<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct f32_load_localget_set_local_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::f32_load_localget_set_local<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct f32_load_localget_tee_local_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::f32_load_localget_tee_local<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct f32_load_localget_tee_local_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::f32_load_localget_tee_local<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct f64_load_localget_set_local_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::f64_load_localget_set_local<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct f64_load_localget_tee_local_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::f64_load_localget_tee_local<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct f64_load_localget_tee_local_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::f64_load_localget_tee_local<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct f32_memcpy_localget_localget_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::f32_memcpy_localget_localget<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct f64_memcpy_localget_localget_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::f64_memcpy_localget_localget<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct u16_copy_scaled_index_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::u16_copy_scaled_index<BoundsCheckFn, CompileOption, Type...>;
                }
            };

        }  // namespace details

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_load_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                                   details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.f32_stack_top_begin_pos,
                                                       CompileOption.f32_stack_top_end_pos,
                                                       details::f32_load_localget_off_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.f32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_load_local_plus_imm_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                     details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.f32_stack_top_begin_pos,
                                                       CompileOption.f32_stack_top_end_pos,
                                                       details::f32_load_local_plus_imm_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.f32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_load_local_plus_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                  details::op_details::native_memory_t const& memory,
                                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_load_local_plus_imm_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_load_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                                   details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.f64_stack_top_begin_pos,
                                                       CompileOption.f64_stack_top_end_pos,
                                                       details::f64_load_localget_off_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.f64_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_load_local_plus_imm_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                     details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.f64_stack_top_begin_pos,
                                                       CompileOption.f64_stack_top_end_pos,
                                                       details::f64_load_local_plus_imm_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.f64_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_load_local_plus_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                  details::op_details::native_memory_t const& memory,
                                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_load_local_plus_imm_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_store_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const&,
                                                                                                    details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::f32_store_localget_off_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_store_local_plus_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&, details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::f32_store_local_plus_imm_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_store_local_plus_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                   details::op_details::native_memory_t const& memory,
                                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_store_local_plus_imm_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_store_imm_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const&, details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::f32_store_imm_localget_off_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_store_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const&,
                                                                                                    details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::f64_store_localget_off_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_store_local_plus_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&, details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::f64_store_local_plus_imm_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_store_local_plus_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                   details::op_details::native_memory_t const& memory,
                                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_store_local_plus_imm_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_store_imm_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const&, details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::f64_store_imm_localget_off_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_load_localget_set_local_fptr(uwvm_interpreter_stacktop_currpos_t const&,
                                                         details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::f32_load_localget_set_local_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_load_localget_tee_local_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                         details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.f32_stack_top_begin_pos,
                                                       CompileOption.f32_stack_top_end_pos,
                                                       details::f32_load_localget_tee_local_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.f32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_load_localget_set_local_fptr(uwvm_interpreter_stacktop_currpos_t const&,
                                                         details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::f64_load_localget_set_local_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_load_localget_tee_local_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                         details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.f64_stack_top_begin_pos,
                                                       CompileOption.f64_stack_top_end_pos,
                                                       details::f64_load_localget_tee_local_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.f64_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_memcpy_localget_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&,
                                                          details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::f32_memcpy_localget_localget_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_memcpy_localget_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&,
                                                          details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::f64_memcpy_localget_localget_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_u16_copy_scaled_index_fptr(uwvm_interpreter_stacktop_currpos_t const&,
                                                                                                   details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::u16_copy_scaled_index_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_u16_copy_scaled_index_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                details::op_details::native_memory_t const& memory,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_u16_copy_scaled_index_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_load_local_plus_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_load_local_plus_imm<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_load_local_plus_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_load_local_plus_imm<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_store_local_plus_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_store_local_plus_imm<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_store_local_plus_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_store_local_plus_imm<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_load_local_plus_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_load_local_plus_imm_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_load_local_plus_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_load_local_plus_imm_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_store_local_plus_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_store_local_plus_imm_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_store_local_plus_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_store_local_plus_imm_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

    }  // namespace translate
#endif
}  // namespace uwvm2::runtime::compiler::uwvm_int::optable

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
