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
# include <array>
# include <limits>
# include <memory>
# include <type_traits>
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
# include "variable.h"
# include "call.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
#ifdef UWVM_ENABLE_UWVM_INT_COMBINE_OPS
    /**
     * @brief Fused opcode implementations for the UWVM int interpreter.
     *
     * @details
     * This header defines "combined/fused" opfuncs that execute multiple Wasm ops in one dispatch
     * (e.g. `local.get` + immediate + arithmetic/comparison, or branch fusions).
     *
     * Two calling styles are supported:
     * - **Tail-call opfuncs** (`CompileOption.is_tail_call == true`): every opfunc consumes immediates from the bytecode stream and ends with
     *   `UWVM_MUSTTAIL` to the next opfunc read from the stream. This keeps dispatch tight and avoids returning to an outer loop.
     * - **Byref opfuncs** (`CompileOption.is_tail_call == false`): opfuncs update the interpreter state by reference and return to an external dispatcher.
     *
     * Translation helpers in `namespace translate` return function pointers to the *actual* implementation entrypoints. Forwarding wrappers are intentionally
     * avoided in selectors so the generated code has no meaningless extra trampoline jump.
     *
     * @anchor uwvmint_conbine_tailcall_layout
     * Tail-call `type[0]` layout (conceptual):
     * - `[opfunc_ptr][immediates...][next_opfunc_ptr]`
     * - Each opfunc advances `type[0]` past its own pointer, decodes its immediates from the bytecode stream, and then tail-jumps to `next_opfunc_ptr`.
     *
     * @anchor uwvmint_conbine_byref_layout
     * Byref `type[0]` layout (conceptual):
     * - `[opfunc_byref_ptr][immediates...]`
     * - The opfunc updates interpreter state by reference and returns to the external dispatcher.
     *
     * @anchor uwvmint_conbine_stacktop_opt
     * Stack-top optimization model:
     * - When enabled (via `CompileOption.*_stack_top_{begin,end}_pos`), some value-producing / value-consuming fused ops may operate on the stack-top cache
     *   instead of the operand stack.
     * - Tail-call variants usually take `curr_*_stack_top` template parameters that indicate the current ring position for the relevant value type.
     */
    namespace conbine_details
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using wasm_u64 = ::std::uint_least64_t;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        using local_offset_t = ::std::size_t;

        template <typename T>
        UWVM_ALWAYS_INLINE inline constexpr T read_imm(::std::byte const*& ip) noexcept
        {
            T v;  // no init
            ::std::memcpy(::std::addressof(v), ip, sizeof(v));
            ip += sizeof(v);
            return v;
        }

        template <typename T>
        UWVM_ALWAYS_INLINE inline constexpr T load_local(::std::byte* local_base, local_offset_t off) noexcept
        {
            T v;  // no init
            ::std::memcpy(::std::addressof(v), local_base + off, sizeof(v));
            return v;
        }

        template <typename T>
        UWVM_ALWAYS_INLINE inline constexpr void store_local(::std::byte* local_base, local_offset_t off, T v) noexcept
        { ::std::memcpy(local_base + off, ::std::addressof(v), sizeof(v)); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename T>
        inline consteval bool stacktop_enabled_for() noexcept
        {
            if constexpr(::std::same_as<T, wasm_i32>) { return CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos; }
            else if constexpr(::std::same_as<T, wasm_i64>) { return CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos; }
            else if constexpr(::std::same_as<T, wasm_f32>) { return CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos; }
            else if constexpr(::std::same_as<T, wasm_f64>) { return CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos; }
            else
            {
                return false;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename T>
        inline consteval ::std::size_t range_begin() noexcept
        {
            if constexpr(::std::same_as<T, wasm_i32>) { return CompileOption.i32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<T, wasm_i64>) { return CompileOption.i64_stack_top_begin_pos; }
            else if constexpr(::std::same_as<T, wasm_f32>) { return CompileOption.f32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<T, wasm_f64>) { return CompileOption.f64_stack_top_begin_pos; }
            else
            {
                return SIZE_MAX;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename T>
        inline consteval ::std::size_t range_end() noexcept
        {
            if constexpr(::std::same_as<T, wasm_i32>) { return CompileOption.i32_stack_top_end_pos; }
            else if constexpr(::std::same_as<T, wasm_i64>) { return CompileOption.i64_stack_top_end_pos; }
            else if constexpr(::std::same_as<T, wasm_f32>) { return CompileOption.f32_stack_top_end_pos; }
            else if constexpr(::std::same_as<T, wasm_f64>) { return CompileOption.f64_stack_top_end_pos; }
            else
            {
                return SIZE_MAX;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename T, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... TypeRef>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void push_operand(T v, TypeRef&... typeref) noexcept
        {
            if constexpr(stacktop_enabled_for<CompileOption, T>())
            {
                constexpr ::std::size_t begin{range_begin<CompileOption, T>()};
                constexpr ::std::size_t end{range_end<CompileOption, T>()};
                static_assert(begin <= curr_stack_top && curr_stack_top < end);
                static_assert(sizeof...(TypeRef) >= end);

                constexpr ::std::size_t new_pos{details::ring_prev_pos(curr_stack_top, begin, end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, T, new_pos>(v, typeref...);
            }
            else
            {
                ::std::memcpy(typeref...[1u], ::std::addressof(v), sizeof(v));
                typeref...[1u] += sizeof(v);
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename T, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void push_operand_byref(T v, TypeRef&... typeref) noexcept
        {
            ::std::memcpy(typeref...[1u], ::std::addressof(v), sizeof(v));
            typeref...[1u] += sizeof(v);
        }

        template <typename T>
        UWVM_ALWAYS_INLINE inline constexpr wasm_i32 bool_to_i32(T v) noexcept
        { return static_cast<wasm_i32>(static_cast<bool>(v)); }
    }  // namespace conbine_details

    // ========================
    // arith_imm / bit_imm / shift_imm / cmp_imm : local.get + imm + op
    // ========================

    /**
     * @brief Integer binary op fusion: `local.get` + immediate + binop.
     *
     * @details
     * - Tail-call form pushes the result onto the operand stack (or stack-top cache, if enabled) and then tail-calls the next opfunc from the bytecode stream.
     * - Byref form writes the result and returns to the external dispatcher.
     * - Bytecode immediates (in order): `local_offset_t`, `wasm_i32`.
     */
    /// @brief Fused `local.get` + immediate + `i32.binop` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop_imm_localget(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const rhs{conbine_details::read_imm<wasm_i32>(type...[0])};
        wasm_i32 const lhs{conbine_details::load_local<wasm_i32>(type...[2u], local_off)};
        wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, numeric_details::wasm_u32>(lhs, rhs)};

        conbine_details::push_operand<CompileOption, wasm_i32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + immediate + `i32.binop` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop_imm_localget(TypeRef & ... typeref) UWVM_THROWS
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
        wasm_i32 const rhs{conbine_details::read_imm<wasm_i32>(typeref...[0])};
        wasm_i32 const lhs{conbine_details::load_local<wasm_i32>(typeref...[2u], local_off)};
        wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, numeric_details::wasm_u32>(lhs, rhs)};

        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /**
     * @brief Named convenience wrappers for fused opcodes (i32).
     *
     * @details
     * These names exist for readability and grouping, but translation-time selectors should prefer the underlying core templates
     * (e.g. `uwvmint_i32_binop_imm_localget`) to avoid an extra forwarding hop in the generated machine code.
     */
    /// @brief Fused `local.get` + immediate + `i32.add` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_imm_localget<CompileOption, numeric_details::int_binop::add>(typeref...); }

    /// @brief Fused `local.get` + immediate + `i32.sub` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_sub_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_imm_localget<CompileOption, numeric_details::int_binop::sub>(typeref...); }

    /// @brief Fused `local.get` + immediate + `i32.mul` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_mul_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_imm_localget<CompileOption, numeric_details::int_binop::mul>(typeref...); }

    /// @brief Fused `local.get` + immediate + `i32.and` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_and_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_imm_localget<CompileOption, numeric_details::int_binop::and_>(typeref...); }

    /// @brief Fused `local.get` + immediate + `i32.or` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_or_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_imm_localget<CompileOption, numeric_details::int_binop::or_>(typeref...); }

    /// @brief Fused `local.get` + immediate + `i32.xor` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_xor_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_imm_localget<CompileOption, numeric_details::int_binop::xor_>(typeref...); }

    /// @brief Fused `local.get` + immediate + `i32.shl` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_shl_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_imm_localget<CompileOption, numeric_details::int_binop::shl>(typeref...); }

    /// @brief Fused `local.get` + immediate + `i32.shr.u` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_shr_u_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_imm_localget<CompileOption, numeric_details::int_binop::shr_u>(typeref...); }

    /// @brief Fused `local.get` + immediate + `i32.shr.s` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_shr_s_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_imm_localget<CompileOption, numeric_details::int_binop::shr_s>(typeref...); }

    // ========================
    // arith_imm_stack / bit_imm_stack / shift_imm_stack : (stack) + imm + op
    // ========================

    /**
     * @brief Integer binary op fusion: `i32.const` + `i32.binop` on an existing stack value.
     *
     * @details
     * This fuses the common pattern:
     * `...; <push i32 lhs>; i32.const <imm>; i32.binop`
     * into a single opcode implementation that reads the immediate directly from the bytecode stream and keeps
     * the operand-stack height unchanged.
     *
     * - Stack-top optimization: supported for i32.
     * - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
     * - Immediates: `wasm_i32`.
     * - Stack effect (relative to before `i32.const`): `(i32 -- i32)` (replace top with `eval(lhs, imm)`).
     */
    template <uwvm_interpreter_translate_option_t CompileOption,
              numeric_details::int_binop Op,
              ::std::size_t curr_i32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop_imm_stack(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        wasm_i32 const rhs{conbine_details::read_imm<wasm_i32>(type...[0])};

        if constexpr(conbine_details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t begin{conbine_details::range_begin<CompileOption, wasm_i32>()};
            constexpr ::std::size_t end{conbine_details::range_end<CompileOption, wasm_i32>()};
            static_assert(begin <= curr_i32_stack_top && curr_i32_stack_top < end);

            wasm_i32 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, numeric_details::wasm_u32>(lhs, rhs)};
            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);
        }
        else
        {
            wasm_i32 const lhs{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, numeric_details::wasm_u32>(lhs, rhs)};
            ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
            type...[1u] += sizeof(out);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `i32.const` + `i32.binop` on an existing stack value (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `wasm_i32`.
    /// - Stack effect (relative to before `i32.const`): `(i32 -- i32)`.
    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop_imm_stack(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        wasm_i32 const rhs{conbine_details::read_imm<wasm_i32>(typeref...[0])};

        wasm_i32 const lhs{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, numeric_details::wasm_u32>(lhs, rhs)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /**
     * @brief Integer binary op fusion: `i64.const` + `i64.binop` on an existing stack value.
     *
     * @details
     * - Stack-top optimization: supported for i64.
     * - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
     * - Immediates: `wasm_i64`.
     * - Stack effect (relative to before `i64.const`): `(i64 -- i64)`.
     */
    template <uwvm_interpreter_translate_option_t CompileOption,
              numeric_details::int_binop Op,
              ::std::size_t curr_i64_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop_imm_stack(Type... type) UWVM_THROWS
    {
        using wasm_i64 = conbine_details::wasm_i64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        wasm_i64 const rhs{conbine_details::read_imm<wasm_i64>(type...[0])};

        if constexpr(conbine_details::stacktop_enabled_for<CompileOption, wasm_i64>())
        {
            constexpr ::std::size_t begin{conbine_details::range_begin<CompileOption, wasm_i64>()};
            constexpr ::std::size_t end{conbine_details::range_end<CompileOption, wasm_i64>()};
            static_assert(begin <= curr_i64_stack_top && curr_i64_stack_top < end);

            wasm_i64 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...)};
            wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, numeric_details::wasm_u64>(lhs, rhs)};
            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_i64_stack_top>(out, type...);
        }
        else
        {
            wasm_i64 const lhs{get_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
            wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, numeric_details::wasm_u64>(lhs, rhs)};
            ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
            type...[1u] += sizeof(out);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `i64.const` + `i64.binop` on an existing stack value (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `wasm_i64`.
    /// - Stack effect (relative to before `i64.const`): `(i64 -- i64)`.
    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop_imm_stack(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i64 = conbine_details::wasm_i64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        wasm_i64 const rhs{conbine_details::read_imm<wasm_i64>(typeref...[0])};

        wasm_i64 const lhs{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, numeric_details::wasm_u64>(lhs, rhs)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    // i32 eqz localget
    /// @brief Fused `local.get` + `i32.eqz` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_eqz_localget(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const x{conbine_details::load_local<wasm_i32>(type...[2u], local_off)};
        wasm_i32 const out{static_cast<wasm_i32>(x == wasm_i32{})};

        conbine_details::push_operand<CompileOption, wasm_i32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `i32.eqz` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_eqz_localget(TypeRef & ... typeref) UWVM_THROWS
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
        wasm_i32 const out{static_cast<wasm_i32>(x == wasm_i32{})};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    // ========================
    // cmp_imm: local.get + imm + cmp (push i32)
    // ========================

    /// @brief Fused `local.get` + immediate + `i32.cmp` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, details::int_cmp Cmp, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_cmp_imm_localget(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_u32 = conbine_details::wasm_u32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const imm{conbine_details::read_imm<wasm_i32>(type...[0])};
        wasm_i32 const x{conbine_details::load_local<wasm_i32>(type...[2u], local_off)};

        wasm_i32 const out{conbine_details::bool_to_i32(details::eval_int_cmp<Cmp, wasm_i32, wasm_u32>(x, imm))};
        conbine_details::push_operand<CompileOption, wasm_i32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + immediate + `i32.cmp` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, details::int_cmp Cmp, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_cmp_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_u32 = conbine_details::wasm_u32;

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
        wasm_i32 const imm{conbine_details::read_imm<wasm_i32>(typeref...[0])};
        wasm_i32 const x{conbine_details::load_local<wasm_i32>(typeref...[2u], local_off)};

        wasm_i32 const out{conbine_details::bool_to_i32(details::eval_int_cmp<Cmp, wasm_i32, wasm_u32>(x, imm))};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + immediate + `i32.eq` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_eq_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_cmp_imm_localget<CompileOption, details::int_cmp::eq>(typeref...); }

    /// @brief Fused `local.get` + immediate + `i32.ne` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_ne_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_cmp_imm_localget<CompileOption, details::int_cmp::ne>(typeref...); }

    /// @brief Fused `local.get` + immediate + `i32.lt.u` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_lt_u_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_cmp_imm_localget<CompileOption, details::int_cmp::lt_u>(typeref...); }

    /// @brief Fused `local.get` + immediate + `i32.ge.u` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_ge_u_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_cmp_imm_localget<CompileOption, details::int_cmp::ge_u>(typeref...); }

    // ========================
    // arith_2local / bit_2local : local.get + local.get + op
    // ========================

    /// @brief Fused `local.get` + `local.get` + `i32.binop` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop_2localget(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const lhs_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const rhs_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const lhs{conbine_details::load_local<wasm_i32>(type...[2u], lhs_off)};
        wasm_i32 const rhs{conbine_details::load_local<wasm_i32>(type...[2u], rhs_off)};
        wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, numeric_details::wasm_u32>(lhs, rhs)};

        conbine_details::push_operand<CompileOption, wasm_i32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `local.get` + `i32.binop` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop_2localget(TypeRef & ... typeref) UWVM_THROWS
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

        auto const lhs_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const rhs_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_i32 const lhs{conbine_details::load_local<wasm_i32>(typeref...[2u], lhs_off)};
        wasm_i32 const rhs{conbine_details::load_local<wasm_i32>(typeref...[2u], rhs_off)};
        wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, numeric_details::wasm_u32>(lhs, rhs)};
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + `local.get` + `i32.add` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_2localget<CompileOption, numeric_details::int_binop::add>(typeref...); }

    // ========================
    // update_local: i32_add_2localget_local_set/tee
    // ========================

    /// @brief Fused `local.get a; local.get b; i32.add; local.set dst` (tail-call).
    /// @details
    /// - Stack-top optimization: N/A (does not read/write the operand stack).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `a_off`, `b_off`, `dst_off`.

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_2localget_local_set(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const dst_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        wasm_i32 const a{conbine_details::load_local<wasm_i32>(type...[2u], a_off)};
        wasm_i32 const b{conbine_details::load_local<wasm_i32>(type...[2u], b_off)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(a, b)};
        conbine_details::store_local(type...[2u], dst_off, out);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get a; local.get b; i32.add; local.set dst` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `a_off`, `b_off`, `dst_off`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_2localget_local_set(TypeRef & ... typeref) UWVM_THROWS
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
        auto const dst_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        wasm_i32 const a{conbine_details::load_local<wasm_i32>(typeref...[2u], a_off)};
        wasm_i32 const b{conbine_details::load_local<wasm_i32>(typeref...[2u], b_off)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(a, b)};
        conbine_details::store_local(typeref...[2u], dst_off, out);
    }

    /// @brief Fused `local.get a; local.get b; i32.add; local.tee dst` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `a_off`, `b_off`, `dst_off`.

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_2localget_local_tee(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const dst_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        wasm_i32 const a{conbine_details::load_local<wasm_i32>(type...[2u], a_off)};
        wasm_i32 const b{conbine_details::load_local<wasm_i32>(type...[2u], b_off)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(a, b)};
        conbine_details::store_local(type...[2u], dst_off, out);
        conbine_details::push_operand<CompileOption, wasm_i32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get a; local.get b; i32.add; local.tee dst` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `a_off`, `b_off`, `dst_off`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_2localget_local_tee(TypeRef & ... typeref) UWVM_THROWS
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
        auto const a_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const b_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const dst_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        wasm_i32 const a{conbine_details::load_local<wasm_i32>(typeref...[2u], a_off)};
        wasm_i32 const b{conbine_details::load_local<wasm_i32>(typeref...[2u], b_off)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(a, b)};
        conbine_details::store_local(typeref...[2u], dst_off, out);
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + `local.get` + `i32.sub` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_sub_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_2localget<CompileOption, numeric_details::int_binop::sub>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `i32.mul` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_mul_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_2localget<CompileOption, numeric_details::int_binop::mul>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `i32.and` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_and_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_2localget<CompileOption, numeric_details::int_binop::and_>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `i32.or` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_or_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_2localget<CompileOption, numeric_details::int_binop::or_>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `i32.xor` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_xor_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop_2localget<CompileOption, numeric_details::int_binop::xor_>(typeref...); }

    // ========================
    // i64 localget fusions
    // ========================

    /// @brief Fused `local.get` + immediate + `i64.binop` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`, `wasm_i64`.

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop_imm_localget(Type... type) UWVM_THROWS
    {
        using wasm_i64 = conbine_details::wasm_i64;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i64 const rhs{conbine_details::read_imm<wasm_i64>(type...[0])};
        wasm_i64 const lhs{conbine_details::load_local<wasm_i64>(type...[2u], local_off)};
        wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, numeric_details::wasm_u64>(lhs, rhs)};

        conbine_details::push_operand<CompileOption, wasm_i64, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + immediate + `i64.binop` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i64`.

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i64 = conbine_details::wasm_i64;

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
        wasm_i64 const rhs{conbine_details::read_imm<wasm_i64>(typeref...[0])};
        wasm_i64 const lhs{conbine_details::load_local<wasm_i64>(typeref...[2u], local_off)};
        wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, numeric_details::wasm_u64>(lhs, rhs)};

        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + `local.get` + `i64.binop` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop_2localget(Type... type) UWVM_THROWS
    {
        using wasm_i64 = conbine_details::wasm_i64;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const lhs_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const rhs_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i64 const lhs{conbine_details::load_local<wasm_i64>(type...[2u], lhs_off)};
        wasm_i64 const rhs{conbine_details::load_local<wasm_i64>(type...[2u], rhs_off)};
        wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, numeric_details::wasm_u64>(lhs, rhs)};

        conbine_details::push_operand<CompileOption, wasm_i64, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `local.get` + `i64.binop` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop_2localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i64 = conbine_details::wasm_i64;

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
        wasm_i64 const lhs{conbine_details::load_local<wasm_i64>(typeref...[2u], lhs_off)};
        wasm_i64 const rhs{conbine_details::load_local<wasm_i64>(typeref...[2u], rhs_off)};
        wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, numeric_details::wasm_u64>(lhs, rhs)};

        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Fused `local.get` + immediate + `i64.add` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i64`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_add_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop_imm_localget<CompileOption, numeric_details::int_binop::add>(typeref...); }

    /// @brief Fused `local.get` + immediate + `i64.and` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i64`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_and_imm_localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop_imm_localget<CompileOption, numeric_details::int_binop::and_>(typeref...); }

    /// @brief Fused `local.get` + `local.get` + `i64.add` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_add_2localget(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop_2localget<CompileOption, numeric_details::int_binop::add>(typeref...); }

    // ========================
    // update_local: i32_add_imm_local_set/tee_same
    // ========================

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_add_imm_local_set_same` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_imm_local_set_same(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const imm{conbine_details::read_imm<wasm_i32>(type...[0])};
        wasm_i32 const x{conbine_details::load_local<wasm_i32>(type...[2u], local_off)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(x, imm)};
        conbine_details::store_local(type...[2u], local_off, out);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_add_imm_local_set_same` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_imm_local_set_same(TypeRef & ... typeref) UWVM_THROWS
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
        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_i32 const imm{conbine_details::read_imm<wasm_i32>(typeref...[0])};
        wasm_i32 const x{conbine_details::load_local<wasm_i32>(typeref...[2u], local_off)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(x, imm)};
        conbine_details::store_local(typeref...[2u], local_off, out);
    }

    // ========================
    // update_global: i32_add_imm_global_set_same
    // ========================

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_add_imm_global_set_same` (tail-call).
    /// @details
    /// - Stack-top optimization: N/A (does not read/write the operand stack).
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `wasm_global_storage_t*`, `wasm_i32`.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_imm_global_set_same(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using global_storage_t = ::uwvm2::object::global::wasm_global_storage_t;

        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        global_storage_t* global_p{conbine_details::read_imm<global_storage_t*>(type...[0])};
        wasm_i32 const imm{conbine_details::read_imm<wasm_i32>(type...[0])};

        wasm_i32 const v{global_p->storage.i32};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(v, imm)};
        global_p->storage.i32 = out;

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_add_imm_global_set_same` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `wasm_global_storage_t*`, `wasm_i32`.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_imm_global_set_same(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using global_storage_t = ::uwvm2::object::global::wasm_global_storage_t;

        static_assert(sizeof...(TypeRef) >= 1uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        global_storage_t* global_p{conbine_details::read_imm<global_storage_t*>(typeref...[0])};
        wasm_i32 const imm{conbine_details::read_imm<wasm_i32>(typeref...[0])};

        wasm_i32 const v{global_p->storage.i32};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(v, imm)};
        global_p->storage.i32 = out;
    }

    // ========================
    // call_fuse: call_drop / call_local_set / call_local_tee
    // ========================

    /**
     * @brief Fast-path `call` when all parameters are cached i32 values (tail-call).
     *
     * @details
     * This avoids the translator's "spill-all-to-memory + call + fill-to-cache" sequence for hot call sites where:
     * - The operand stack is *entirely* in the i32 stack-top cache (no memory segment).
     * - The callee signature is `(i32 x ParamCount) -> RetT` (RetT = `void` or `wasm_i32`).
     *
     * Bytecode layout: `[opfunc_ptr][curr_module_id][call_function][next_opfunc_ptr]`.
     */
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i32_stack_top,
              ::std::size_t ParamCount,
              typename RetT,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_stacktop_i32(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        static_assert(ParamCount != 0uz, "stacktop call fast-path expects ParamCount >= 1.");
        static_assert(::std::is_void_v<RetT> || ::std::same_as<RetT, wasm_i32>, "stacktop i32 call fast-path currently supports RetT = void or wasm_i32.");

        constexpr ::std::size_t begin{CompileOption.i32_stack_top_begin_pos};
        constexpr ::std::size_t end{CompileOption.i32_stack_top_end_pos};
        static_assert(begin != end, "stacktop i32 call fast-path requires i32 stack-top cache enabled.");
        static_assert(begin <= curr_i32_stack_top && curr_i32_stack_top < end);

        // Advance to immediates.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), type...[0], sizeof(curr_module_id));
        type...[0] += sizeof(curr_module_id);

        ::std::size_t call_function;  // no init
        ::std::memcpy(::std::addressof(call_function), type...[0], sizeof(call_function));
        type...[0] += sizeof(call_function);

        // Scratch operand stack for the call bridge. The bridge overwrites args with results at the base.
        constexpr ::std::size_t param_bytes{ParamCount * sizeof(wasm_i32)};
        constexpr ::std::size_t result_bytes{[]() consteval noexcept -> ::std::size_t
                                             {
                                                 if constexpr(::std::is_void_v<RetT>) { return 0uz; }
                                                 else
                                                 {
                                                     return sizeof(RetT);
                                                 }
                                             }()};
        constexpr ::std::size_t scratch_bytes{param_bytes >= result_bytes ? param_bytes : result_bytes};

        ::std::array<::std::byte, (scratch_bytes == 0uz ? 1uz : scratch_bytes)> scratch{};  // zero-init: keeps tools happy on debug builds
        ::std::byte* scratch_top{scratch.data() + param_bytes};

        // Write params in canonical memory order (param0 .. paramN-1).
        [&]<::std::size_t... Is>(::std::index_sequence<Is...>) constexpr UWVM_THROWS
        {
            ((
                 [&]() constexpr UWVM_THROWS
                 {
                     constexpr ::std::size_t steps{(ParamCount - 1uz) - Is};
                     constexpr ::std::size_t pos{details::ring_advance_next_pos<curr_i32_stack_top, steps, begin, end>()};
                     wasm_i32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, pos>(type...)};
                     ::std::memcpy(scratch.data() + (Is * sizeof(wasm_i32)), ::std::addressof(v), sizeof(v));
                 }()),
             ...);
        }(::std::make_index_sequence<ParamCount>{});

        details::call(curr_module_id, call_function, ::std::addressof(scratch_top));

        if constexpr(!::std::is_void_v<RetT>)
        {
            RetT out;  // no init
            ::std::memcpy(::std::addressof(out), scratch.data(), sizeof(out));

            // Pop ParamCount + push 1 => currpos advances ParamCount times then retreats once:
            // ring_prev(ring_next^ParamCount(curr)) == ring_next^(ParamCount-1)(curr).
            constexpr ::std::size_t new_pos{details::ring_advance_next_pos<curr_i32_stack_top, ParamCount - 1uz, begin, end>()};
            details::set_curr_val_to_stacktop_cache<CompileOption, RetT, new_pos>(out, type...);
        }

        // Next opfunc.
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /**
     * @brief Fast-path `call` when all parameters are cached f32 values (tail-call).
     *
     * @details
     * Same as `uwvmint_call_stacktop_i32`, but for the f32 stack-top cache ring.
     *
     * Bytecode layout: `[opfunc_ptr][curr_module_id][call_function][next_opfunc_ptr]`.
     */
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_f32_stack_top,
              ::std::size_t ParamCount,
              typename RetT,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_stacktop_f32(Type... type) UWVM_THROWS
    {
        using wasm_f32 = conbine_details::wasm_f32;
        using wasm_f64 = conbine_details::wasm_f64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        static_assert(ParamCount != 0uz, "stacktop call fast-path expects ParamCount >= 1.");
        static_assert(::std::is_void_v<RetT> || ::std::same_as<RetT, wasm_f32> || ::std::same_as<RetT, wasm_f64>,
                      "stacktop f32 call fast-path currently supports RetT = void, wasm_f32 or wasm_f64.");

        if constexpr(::std::same_as<RetT, wasm_f64>)
        {
            static_assert(CompileOption.f64_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                              CompileOption.f64_stack_top_end_pos == CompileOption.f32_stack_top_end_pos,
                          "stacktop f32->f64 call fast-path requires f32/f64 ranges to be fully merged (same begin/end).");
        }

        constexpr ::std::size_t begin{CompileOption.f32_stack_top_begin_pos};
        constexpr ::std::size_t end{CompileOption.f32_stack_top_end_pos};
        static_assert(begin != end, "stacktop f32 call fast-path requires f32 stack-top cache enabled.");
        static_assert(begin <= curr_f32_stack_top && curr_f32_stack_top < end);

        // Advance to immediates.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), type...[0], sizeof(curr_module_id));
        type...[0] += sizeof(curr_module_id);

        ::std::size_t call_function;  // no init
        ::std::memcpy(::std::addressof(call_function), type...[0], sizeof(call_function));
        type...[0] += sizeof(call_function);

        // Scratch operand stack for the call bridge. The bridge overwrites args with results at the base.
        constexpr ::std::size_t param_bytes{ParamCount * sizeof(wasm_f32)};
        constexpr ::std::size_t result_bytes{[]() consteval noexcept -> ::std::size_t
                                             {
                                                 if constexpr(::std::is_void_v<RetT>) { return 0uz; }
                                                 else
                                                 {
                                                     return sizeof(RetT);
                                                 }
                                             }()};
        constexpr ::std::size_t scratch_bytes{param_bytes >= result_bytes ? param_bytes : result_bytes};

        ::std::array<::std::byte, (scratch_bytes == 0uz ? 1uz : scratch_bytes)> scratch{};  // zero-init: keeps tools happy on debug builds
        ::std::byte* scratch_top{scratch.data() + param_bytes};

        // Write params in canonical memory order (param0 .. paramN-1).
        [&]<::std::size_t... Is>(::std::index_sequence<Is...>) constexpr UWVM_THROWS
        {
            ((
                 [&]() constexpr UWVM_THROWS
                 {
                     constexpr ::std::size_t steps{(ParamCount - 1uz) - Is};
                     constexpr ::std::size_t pos{details::ring_advance_next_pos<curr_f32_stack_top, steps, begin, end>()};
                     wasm_f32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, pos>(type...)};
                     ::std::memcpy(scratch.data() + (Is * sizeof(wasm_f32)), ::std::addressof(v), sizeof(v));
                 }()),
             ...);
        }(::std::make_index_sequence<ParamCount>{});

        details::call(curr_module_id, call_function, ::std::addressof(scratch_top));

        if constexpr(!::std::is_void_v<RetT>)
        {
            RetT out;  // no init
            ::std::memcpy(::std::addressof(out), scratch.data(), sizeof(out));

            constexpr ::std::size_t new_pos{details::ring_advance_next_pos<curr_f32_stack_top, ParamCount - 1uz, begin, end>()};
            details::set_curr_val_to_stacktop_cache<CompileOption, RetT, new_pos>(out, type...);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /**
     * @brief Fast-path `call` when all parameters are cached f64 values (tail-call).
     *
     * @details
     * Same as `uwvmint_call_stacktop_i32`, but for the f64 stack-top cache ring.
     *
     * Bytecode layout: `[opfunc_ptr][curr_module_id][call_function][next_opfunc_ptr]`.
     */
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_f64_stack_top,
              ::std::size_t ParamCount,
              typename RetT,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_stacktop_f64(Type... type) UWVM_THROWS
    {
        using wasm_f64 = conbine_details::wasm_f64;
        using wasm_f32 = conbine_details::wasm_f32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        static_assert(ParamCount != 0uz, "stacktop call fast-path expects ParamCount >= 1.");
        static_assert(::std::is_void_v<RetT> || ::std::same_as<RetT, wasm_f64> || ::std::same_as<RetT, wasm_f32>,
                      "stacktop f64 call fast-path currently supports RetT = void, wasm_f64 or wasm_f32.");

        if constexpr(::std::same_as<RetT, wasm_f32>)
        {
            static_assert(CompileOption.f32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                              CompileOption.f32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos,
                          "stacktop f64->f32 call fast-path requires f32/f64 ranges to be fully merged (same begin/end).");
        }

        constexpr ::std::size_t begin{CompileOption.f64_stack_top_begin_pos};
        constexpr ::std::size_t end{CompileOption.f64_stack_top_end_pos};
        static_assert(begin != end, "stacktop f64 call fast-path requires f64 stack-top cache enabled.");
        static_assert(begin <= curr_f64_stack_top && curr_f64_stack_top < end);

        // Advance to immediates.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), type...[0], sizeof(curr_module_id));
        type...[0] += sizeof(curr_module_id);

        ::std::size_t call_function;  // no init
        ::std::memcpy(::std::addressof(call_function), type...[0], sizeof(call_function));
        type...[0] += sizeof(call_function);

        // Scratch operand stack for the call bridge. The bridge overwrites args with results at the base.
        constexpr ::std::size_t param_bytes{ParamCount * sizeof(wasm_f64)};
        constexpr ::std::size_t result_bytes{[]() consteval noexcept -> ::std::size_t
                                             {
                                                 if constexpr(::std::is_void_v<RetT>) { return 0uz; }
                                                 else
                                                 {
                                                     return sizeof(RetT);
                                                 }
                                             }()};
        constexpr ::std::size_t scratch_bytes{param_bytes >= result_bytes ? param_bytes : result_bytes};

        ::std::array<::std::byte, (scratch_bytes == 0uz ? 1uz : scratch_bytes)> scratch{};  // zero-init: keeps tools happy on debug builds
        ::std::byte* scratch_top{scratch.data() + param_bytes};

        // Write params in canonical memory order (param0 .. paramN-1).
        [&]<::std::size_t... Is>(::std::index_sequence<Is...>) constexpr UWVM_THROWS
        {
            ((
                 [&]() constexpr UWVM_THROWS
                 {
                     constexpr ::std::size_t steps{(ParamCount - 1uz) - Is};
                     constexpr ::std::size_t pos{details::ring_advance_next_pos<curr_f64_stack_top, steps, begin, end>()};
                     wasm_f64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, pos>(type...)};
                     ::std::memcpy(scratch.data() + (Is * sizeof(wasm_f64)), ::std::addressof(v), sizeof(v));
                 }()),
             ...);
        }(::std::make_index_sequence<ParamCount>{});

        details::call(curr_module_id, call_function, ::std::addressof(scratch_top));

        if constexpr(!::std::is_void_v<RetT>)
        {
            RetT out;  // no init
            ::std::memcpy(::std::addressof(out), scratch.data(), sizeof(out));

            constexpr ::std::size_t new_pos{details::ring_advance_next_pos<curr_f64_stack_top, ParamCount - 1uz, begin, end>()};
            details::set_curr_val_to_stacktop_cache<CompileOption, RetT, new_pos>(out, type...);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `call` + `drop` (tail-call).
    /// @details
    /// - Stack-top optimization: requires a full operand-stack spill (same constraint as `uwvmint_call`).
    /// - `type[0]` layout: `[opfunc_ptr][curr_module_id][call_function][next_opfunc_ptr]`.
    /// - Immediates: `curr_module_id`, `call_function`.
    template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_drop(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), type...[0], sizeof(curr_module_id));
        type...[0] += sizeof(curr_module_id);

        ::std::size_t call_function;  // no init
        ::std::memcpy(::std::addressof(call_function), type...[0], sizeof(call_function));
        type...[0] += sizeof(call_function);

        details::call(curr_module_id, call_function, ::std::addressof(type...[1]));
        if constexpr(!::std::is_void_v<RetT>) { type...[1u] -= sizeof(RetT); }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `call` + `local.set` (tail-call).
    /// @details
    /// - Stack-top optimization: requires a full operand-stack spill (same constraint as `uwvmint_call`).
    /// - `type[0]` layout: `[opfunc_ptr][curr_module_id][call_function][local_offset][next_opfunc_ptr]`.
    /// - Immediates: `curr_module_id`, `call_function`, `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_local_set(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), type...[0], sizeof(curr_module_id));
        type...[0] += sizeof(curr_module_id);

        ::std::size_t call_function;  // no init
        ::std::memcpy(::std::addressof(call_function), type...[0], sizeof(call_function));
        type...[0] += sizeof(call_function);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        details::call(curr_module_id, call_function, ::std::addressof(type...[1]));

        if constexpr(!::std::is_void_v<RetT>)
        {
            RetT v;  // no init
            ::std::memcpy(::std::addressof(v), type...[1u] - sizeof(RetT), sizeof(RetT));
            type...[1u] -= sizeof(RetT);
            conbine_details::store_local(type...[2u], local_off, v);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `call` + `local.tee` (tail-call).
    /// @details
    /// - Stack-top optimization: requires a full operand-stack spill (same constraint as `uwvmint_call`).
    /// - `type[0]` layout: `[opfunc_ptr][curr_module_id][call_function][local_offset][next_opfunc_ptr]`.
    /// - Immediates: `curr_module_id`, `call_function`, `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_local_tee(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), type...[0], sizeof(curr_module_id));
        type...[0] += sizeof(curr_module_id);

        ::std::size_t call_function;  // no init
        ::std::memcpy(::std::addressof(call_function), type...[0], sizeof(call_function));
        type...[0] += sizeof(call_function);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        details::call(curr_module_id, call_function, ::std::addressof(type...[1]));

        if constexpr(!::std::is_void_v<RetT>)
        {
            RetT v;  // no init
            ::std::memcpy(::std::addressof(v), type...[1u] - sizeof(RetT), sizeof(RetT));
            conbine_details::store_local(type...[2u], local_off, v);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `call` + `drop` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: `[opfunc_ptr][curr_module_id][call_function][next_opfunc_ptr]`.
    /// - Immediates: `curr_module_id`, `call_function`.
    template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_drop(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), typeref...[0], sizeof(curr_module_id));
        typeref...[0] += sizeof(curr_module_id);

        ::std::size_t call_function;  // no init
        ::std::memcpy(::std::addressof(call_function), typeref...[0], sizeof(call_function));
        typeref...[0] += sizeof(call_function);

        details::call(curr_module_id, call_function, ::std::addressof(typeref...[1]));
        if constexpr(!::std::is_void_v<RetT>) { typeref...[1u] -= sizeof(RetT); }
    }

    /// @brief Fused `call` + `local.set` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: `[opfunc_ptr][curr_module_id][call_function][local_offset][next_opfunc_ptr]`.
    /// - Immediates: `curr_module_id`, `call_function`, `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_local_set(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), typeref...[0], sizeof(curr_module_id));
        typeref...[0] += sizeof(curr_module_id);

        ::std::size_t call_function;  // no init
        ::std::memcpy(::std::addressof(call_function), typeref...[0], sizeof(call_function));
        typeref...[0] += sizeof(call_function);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        details::call(curr_module_id, call_function, ::std::addressof(typeref...[1]));

        if constexpr(!::std::is_void_v<RetT>)
        {
            RetT v;  // no init
            ::std::memcpy(::std::addressof(v), typeref...[1u] - sizeof(RetT), sizeof(RetT));
            typeref...[1u] -= sizeof(RetT);
            conbine_details::store_local(typeref...[2u], local_off, v);
        }
    }

    /// @brief Fused `call` + `local.tee` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: `[opfunc_ptr][curr_module_id][call_function][local_offset][next_opfunc_ptr]`.
    /// - Immediates: `curr_module_id`, `call_function`, `local_offset_t`.
    template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_call_local_tee(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(sizeof...(TypeRef) >= 3uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        ::std::size_t curr_module_id;  // no init
        ::std::memcpy(::std::addressof(curr_module_id), typeref...[0], sizeof(curr_module_id));
        typeref...[0] += sizeof(curr_module_id);

        ::std::size_t call_function;  // no init
        ::std::memcpy(::std::addressof(call_function), typeref...[0], sizeof(call_function));
        typeref...[0] += sizeof(call_function);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        details::call(curr_module_id, call_function, ::std::addressof(typeref...[1]));

        if constexpr(!::std::is_void_v<RetT>)
        {
            RetT v;  // no init
            ::std::memcpy(::std::addressof(v), typeref...[1u] - sizeof(RetT), sizeof(RetT));
            conbine_details::store_local(typeref...[2u], local_off, v);
        }
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_add_imm_local_tee_same` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_imm_local_tee_same(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const imm{conbine_details::read_imm<wasm_i32>(type...[0])};
        wasm_i32 const x{conbine_details::load_local<wasm_i32>(type...[2u], local_off)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(x, imm)};
        conbine_details::store_local(type...[2u], local_off, out);

        conbine_details::push_operand<CompileOption, wasm_i32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_add_imm_local_tee_same` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_imm_local_tee_same(TypeRef & ... typeref) UWVM_THROWS
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
        wasm_i32 const imm{conbine_details::read_imm<wasm_i32>(typeref...[0])};
        wasm_i32 const x{conbine_details::load_local<wasm_i32>(typeref...[2u], local_off)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(x, imm)};
        conbine_details::store_local(typeref...[2u], local_off, out);
        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    // ========================
    // addr_calc: LEA-like localget fusions
    // ========================

    // base + (idx << sh)
    /// @brief Fused `local.get` + `local.get` + `i32.add.shl.imm` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_shl_imm_2localget(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const base_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const idx_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const sh{conbine_details::read_imm<wasm_i32>(type...[0])};

        wasm_i32 const base{conbine_details::load_local<wasm_i32>(type...[2u], base_off)};
        wasm_i32 const idx{conbine_details::load_local<wasm_i32>(type...[2u], idx_off)};

        wasm_i32 const scaled{numeric_details::eval_int_binop<numeric_details::int_binop::shl, wasm_i32, numeric_details::wasm_u32>(idx, sh)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(base, scaled)};

        conbine_details::push_operand<CompileOption, wasm_i32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `local.get` + `i32.add.shl.imm` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_shl_imm_2localget(TypeRef & ... typeref) UWVM_THROWS
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

        auto const base_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const idx_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_i32 const sh{conbine_details::read_imm<wasm_i32>(typeref...[0])};

        wasm_i32 const base{conbine_details::load_local<wasm_i32>(typeref...[2u], base_off)};
        wasm_i32 const idx{conbine_details::load_local<wasm_i32>(typeref...[2u], idx_off)};

        wasm_i32 const scaled{numeric_details::eval_int_binop<numeric_details::int_binop::shl, wasm_i32, numeric_details::wasm_u32>(idx, sh)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(base, scaled)};

        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    // base + (idx * k)
    /// @brief Fused `local.get` + `local.get` + `i32.add.mul.imm` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_mul_imm_2localget(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const base_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        auto const idx_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const k{conbine_details::read_imm<wasm_i32>(type...[0])};

        wasm_i32 const base{conbine_details::load_local<wasm_i32>(type...[2u], base_off)};
        wasm_i32 const idx{conbine_details::load_local<wasm_i32>(type...[2u], idx_off)};

        wasm_i32 const scaled{numeric_details::eval_int_binop<numeric_details::int_binop::mul, wasm_i32, numeric_details::wasm_u32>(idx, k)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(base, scaled)};

        conbine_details::push_operand<CompileOption, wasm_i32, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `local.get` + `local.get` + `i32.add.mul.imm` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (lhs), `local_offset_t` (rhs).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add_mul_imm_2localget(TypeRef & ... typeref) UWVM_THROWS
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

        auto const base_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        auto const idx_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_i32 const k{conbine_details::read_imm<wasm_i32>(typeref...[0])};

        wasm_i32 const base{conbine_details::load_local<wasm_i32>(typeref...[2u], base_off)};
        wasm_i32 const idx{conbine_details::load_local<wasm_i32>(typeref...[2u], idx_off)};

        wasm_i32 const scaled{numeric_details::eval_int_binop<numeric_details::int_binop::mul, wasm_i32, numeric_details::wasm_u32>(idx, k)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(base, scaled)};

        conbine_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    // ========================
    // bit_pack: i32_shl_imm_or
    // ========================

    /// @brief Fused bit-pack: `i32.const <sh>; i32.shl; i32.or` (tail-call).
    /// @details
    /// - Stack-top optimization: supported for i32.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `wasm_i32` (shift amount).
    /// @note Stack effect: `(i32 i32 -- i32)` (lo hi -> lo | (hi<<sh)).
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_shl_imm_or(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        wasm_i32 const sh{conbine_details::read_imm<wasm_i32>(type...[0])};

        if constexpr(conbine_details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t begin{conbine_details::range_begin<CompileOption, wasm_i32>()};
            constexpr ::std::size_t end{conbine_details::range_end<CompileOption, wasm_i32>()};
            static_assert(begin <= curr_i32_stack_top && curr_i32_stack_top < end);
            constexpr ::std::size_t ring_sz{end - begin};
            static_assert(ring_sz != 0uz);
            constexpr ::std::size_t next_pos{details::ring_next_pos(curr_i32_stack_top, begin, end)};

            wasm_i32 const hi{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            wasm_i32 lo{};  // init
            if constexpr(ring_sz >= 2uz) { lo = get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, next_pos>(type...); }
            else
            {
                // Ring too small to hold both operands: keep `hi` in cache, `lo` lives in operand stack memory (no pop).
                lo = peek_curr_val_from_operand_stack_cache<wasm_i32>(type...);
            }
            wasm_i32 const shifted{numeric_details::eval_int_binop<numeric_details::int_binop::shl, wasm_i32, numeric_details::wasm_u32>(hi, sh)};
            wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::or_, wasm_i32, numeric_details::wasm_u32>(lo, shifted)};
            if constexpr(ring_sz >= 2uz) { details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, next_pos>(out, type...); }
            else
            {
                set_curr_val_to_operand_stack_cache_top(out, type...);
            }
        }
        else
        {
            wasm_i32 const hi{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            wasm_i32 const lo{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            wasm_i32 const shifted{numeric_details::eval_int_binop<numeric_details::int_binop::shl, wasm_i32, numeric_details::wasm_u32>(hi, sh)};
            wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::or_, wasm_i32, numeric_details::wasm_u32>(lo, shifted)};
            ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
            type...[1u] += sizeof(out);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused bit-pack: `i32.const <sh>; i32.shl; i32.or` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `wasm_i32` (shift amount).
    /// @note Stack effect: `(i32 i32 -- i32)`.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_shl_imm_or(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        wasm_i32 const sh{conbine_details::read_imm<wasm_i32>(typeref...[0])};

        wasm_i32 const hi{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_i32 const lo{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_i32 const shifted{numeric_details::eval_int_binop<numeric_details::int_binop::shl, wasm_i32, numeric_details::wasm_u32>(hi, sh)};
        wasm_i32 const out{numeric_details::eval_int_binop<numeric_details::int_binop::or_, wasm_i32, numeric_details::wasm_u32>(lo, shifted)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    // ========================
    // branch_fuse: compare/branch fusions
    // ========================

    /**
     * @brief Fused stack-top register transform + `br` (tail-call).
     *
     * @details
     * Rotates the active stack-top cache ring(s) so each currpos becomes its range-begin slot, then performs an
     * unconditional branch to `jmp_ip`.
     *
     * This is intended to make loop/label re-entry deterministic **without spilling to operand-stack memory**.
     *
     * Bytecode layout: `[opfunc_ptr][jmp_ip: byte const*]`.
     */

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t CurrIntPos, ::std::size_t CurrFpPos, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_stacktop_transform_to_begin(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        constexpr bool i32_enabled{details::range_enabled(CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos)};
        constexpr bool i64_enabled{details::range_enabled(CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos)};
        constexpr bool f32_enabled{details::range_enabled(CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos)};
        constexpr bool f64_enabled{details::range_enabled(CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos)};
        constexpr bool v128_enabled{details::range_enabled(CompileOption.v128_stack_top_begin_pos, CompileOption.v128_stack_top_end_pos)};

        constexpr bool int_enabled{i32_enabled || i64_enabled};
        constexpr bool fp_enabled{f32_enabled || f64_enabled || v128_enabled};

        if constexpr(i32_enabled && i64_enabled)
        {
            static_assert(CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                              CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos,
                          "stacktop transform requires i32/i64 to be fully merged (same begin/end) when both are enabled.");
        }

        if constexpr(f32_enabled && f64_enabled)
        {
            static_assert(CompileOption.f32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                              CompileOption.f32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos,
                          "stacktop transform requires f32/f64 to be fully merged (same begin/end) when both are enabled.");
        }
        if constexpr(v128_enabled && f32_enabled)
        {
            static_assert(CompileOption.v128_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                              CompileOption.v128_stack_top_end_pos == CompileOption.f32_stack_top_end_pos,
                          "stacktop transform requires v128 to be fully merged with f32/f64 (same begin/end).");
        }
        if constexpr(v128_enabled && f64_enabled)
        {
            static_assert(CompileOption.v128_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                              CompileOption.v128_stack_top_end_pos == CompileOption.f64_stack_top_end_pos,
                          "stacktop transform requires v128 to be fully merged with f32/f64 (same begin/end).");
        }

        // Advance to the `jmp_ip` immediate.
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));

        if constexpr(int_enabled && fp_enabled)
        {
            constexpr ::std::size_t int_begin{i32_enabled ? CompileOption.i32_stack_top_begin_pos : CompileOption.i64_stack_top_begin_pos};
            constexpr ::std::size_t int_end{i32_enabled ? CompileOption.i32_stack_top_end_pos : CompileOption.i64_stack_top_end_pos};
            constexpr ::std::size_t fp_begin{f32_enabled ? CompileOption.f32_stack_top_begin_pos
                                                         : (f64_enabled ? CompileOption.f64_stack_top_begin_pos : CompileOption.v128_stack_top_begin_pos)};
            constexpr ::std::size_t fp_end{f32_enabled ? CompileOption.f32_stack_top_end_pos
                                                       : (f64_enabled ? CompileOption.f64_stack_top_end_pos : CompileOption.v128_stack_top_end_pos)};

            constexpr bool same_range{int_begin == fp_begin && int_end == fp_end};

            if constexpr(same_range)
            {
                static_assert(CurrIntPos == CurrFpPos, "Merged int/fp stacktop range requires CurrIntPos == CurrFpPos.");
                static_assert(int_begin <= CurrIntPos && CurrIntPos < int_end);

                constexpr ::std::size_t step{details::ring_step_count<int_begin, int_end>(CurrIntPos, int_begin)};
                details::rotate_stacktop_range_next<int_begin, int_end, step>(type...);
            }
            else
            {
                static_assert(details::uwvm_interpreter_stacktop_range_is_disjoint(int_begin, int_end, fp_begin, fp_end),
                              "stacktop transform requires int/fp ranges to be disjoint when not merged.");

                static_assert(int_begin <= CurrIntPos && CurrIntPos < int_end);
                static_assert(fp_begin <= CurrFpPos && CurrFpPos < fp_end);

                constexpr ::std::size_t int_step{details::ring_step_count<int_begin, int_end>(CurrIntPos, int_begin)};
                constexpr ::std::size_t fp_step{details::ring_step_count<fp_begin, fp_end>(CurrFpPos, fp_begin)};

                details::rotate_stacktop_range_next<int_begin, int_end, int_step>(type...);
                details::rotate_stacktop_range_next<fp_begin, fp_end, fp_step>(type...);
            }
        }
        else if constexpr(int_enabled)
        {
            constexpr ::std::size_t int_begin{i32_enabled ? CompileOption.i32_stack_top_begin_pos : CompileOption.i64_stack_top_begin_pos};
            constexpr ::std::size_t int_end{i32_enabled ? CompileOption.i32_stack_top_end_pos : CompileOption.i64_stack_top_end_pos};
            static_assert(int_begin <= CurrIntPos && CurrIntPos < int_end);
            constexpr ::std::size_t int_step{details::ring_step_count<int_begin, int_end>(CurrIntPos, int_begin)};
            details::rotate_stacktop_range_next<int_begin, int_end, int_step>(type...);
        }
        else if constexpr(fp_enabled)
        {
            constexpr ::std::size_t fp_begin{f32_enabled ? CompileOption.f32_stack_top_begin_pos
                                                         : (f64_enabled ? CompileOption.f64_stack_top_begin_pos : CompileOption.v128_stack_top_begin_pos)};
            constexpr ::std::size_t fp_end{f32_enabled ? CompileOption.f32_stack_top_end_pos
                                                       : (f64_enabled ? CompileOption.f64_stack_top_end_pos : CompileOption.v128_stack_top_end_pos)};
            static_assert(fp_begin <= CurrFpPos && CurrFpPos < fp_end);
            constexpr ::std::size_t fp_step{details::ring_step_count<fp_begin, fp_end>(CurrFpPos, fp_begin)};
            details::rotate_stacktop_range_next<fp_begin, fp_end, fp_step>(type...);
        }

        type...[0] = jmp_ip;

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Fused `br_if` using `eqz` test (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_eqz(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        wasm_i32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};

# if UWVM_HAS_CPP_ATTRIBUTE(clang::nomerge)
        [[clang::nomerge]]
# endif
        if(v == wasm_i32{})
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

    /// @brief Fused `br_if` using `eqz` test (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_eqz(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

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

        wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        if(v == wasm_i32{}) { typeref...[0] = jmp_ip; }
    }

    /// @brief Fused operand-stack compare + `br_if` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, details::int_cmp Cmp, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_cmp(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        bool take_branch{};
        if constexpr(conbine_details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t begin{conbine_details::range_begin<CompileOption, wasm_i32>()};
            constexpr ::std::size_t end{conbine_details::range_end<CompileOption, wasm_i32>()};
            static_assert(begin <= curr_i32_stack_top && curr_i32_stack_top < end);
            constexpr ::std::size_t ring_sz{end - begin};
            static_assert(ring_sz != 0uz);
            constexpr ::std::size_t next_pos{details::ring_next_pos(curr_i32_stack_top, begin, end)};

            wasm_i32 const rhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            wasm_i32 lhs{};  // init
            if constexpr(ring_sz >= 2uz) { lhs = get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, next_pos>(type...); }
            else
            {
                // Ring too small to hold both operands: keep RHS in cache, load LHS from the operand stack memory.
                lhs = get_curr_val_from_operand_stack_cache<wasm_i32>(type...);
            }
            take_branch = details::eval_int_cmp<Cmp, wasm_i32, conbine_details::wasm_u32>(lhs, rhs);
        }
        else
        {
            wasm_i32 const rhs{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            wasm_i32 const lhs{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            take_branch = details::eval_int_cmp<Cmp, wasm_i32, conbine_details::wasm_u32>(lhs, rhs);
        }

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

    /// @brief Fused operand-stack compare + `br_if` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, details::int_cmp Cmp, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_cmp(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

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

        wasm_i32 const rhs{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_i32 const lhs{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        if(details::eval_int_cmp<Cmp, wasm_i32, conbine_details::wasm_u32>(lhs, rhs)) { typeref...[0] = jmp_ip; }
    }

    // Named wrappers (i32 stack compares)
    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i32_eq` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_eq(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_i32_cmp<CompileOption, details::int_cmp::eq>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i32_ne` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_ne(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_i32_cmp<CompileOption, details::int_cmp::ne>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i32_lt_u` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_lt_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_i32_cmp<CompileOption, details::int_cmp::lt_u>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i32_gt_u` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_gt_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_i32_cmp<CompileOption, details::int_cmp::gt_u>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i32_ge_u` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_ge_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_i32_cmp<CompileOption, details::int_cmp::ge_u>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i32_le_u` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_le_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_i32_cmp<CompileOption, details::int_cmp::le_u>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i32_gt_s` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_gt_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_i32_cmp<CompileOption, details::int_cmp::gt_s>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i32_le_s` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_le_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_i32_cmp<CompileOption, details::int_cmp::le_s>(typeref...); }

    // ========================
    // br_if fused ops (i64 stack)
    // ========================

    /// @brief Fused `br_if` using `eqz` test (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i64_eqz(Type... type) UWVM_THROWS
    {
        using wasm_i64 = conbine_details::wasm_i64;

        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        wasm_i64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...)};

# if UWVM_HAS_CPP_ATTRIBUTE(clang::nomerge)
        [[clang::nomerge]]
# endif
        if(v == wasm_i64{})
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

    /// @brief Fused `br_if` using `eqz` test (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i64_eqz(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i64 = conbine_details::wasm_i64;

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

        wasm_i64 const v{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        if(v == wasm_i64{}) { typeref...[0] = jmp_ip; }
    }

    /// @brief Fused operand-stack compare + `br_if` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, details::int_cmp Cmp, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i64_cmp(Type... type) UWVM_THROWS
    {
        using wasm_i64 = conbine_details::wasm_i64;

        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        bool take_branch{};
        if constexpr(conbine_details::stacktop_enabled_for<CompileOption, wasm_i64>())
        {
            constexpr ::std::size_t begin{conbine_details::range_begin<CompileOption, wasm_i64>()};
            constexpr ::std::size_t end{conbine_details::range_end<CompileOption, wasm_i64>()};
            static_assert(begin <= curr_i64_stack_top && curr_i64_stack_top < end);
            constexpr ::std::size_t ring_sz{end - begin};
            static_assert(ring_sz != 0uz);
            constexpr ::std::size_t next_pos{details::ring_next_pos(curr_i64_stack_top, begin, end)};

            wasm_i64 const rhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...)};
            wasm_i64 lhs{};  // init
            if constexpr(ring_sz >= 2uz) { lhs = get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, next_pos>(type...); }
            else
            {
                // Ring too small to hold both operands: keep RHS in cache, load LHS from the operand stack memory.
                lhs = get_curr_val_from_operand_stack_cache<wasm_i64>(type...);
            }
            take_branch = details::eval_int_cmp<Cmp, wasm_i64, conbine_details::wasm_u64>(lhs, rhs);
        }
        else
        {
            wasm_i64 const rhs{get_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
            wasm_i64 const lhs{get_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
            take_branch = details::eval_int_cmp<Cmp, wasm_i64, conbine_details::wasm_u64>(lhs, rhs);
        }

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

    /// @brief Fused operand-stack compare + `br_if` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, details::int_cmp Cmp, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i64_cmp(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i64 = conbine_details::wasm_i64;

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

        wasm_i64 const rhs{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        wasm_i64 const lhs{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        if(details::eval_int_cmp<Cmp, wasm_i64, conbine_details::wasm_u64>(lhs, rhs)) { typeref...[0] = jmp_ip; }
    }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i64_ne` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i64_ne(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_i64_cmp<CompileOption, details::int_cmp::ne>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i64_gt_u` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i64_gt_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_i64_cmp<CompileOption, details::int_cmp::gt_u>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i64_lt_u` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i64_lt_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_i64_cmp<CompileOption, details::int_cmp::lt_u>(typeref...); }

    // br_if_i32_and_nz: (a&b)!=0
    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i32_and_nz` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_and_nz(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        bool take_branch{};
        if constexpr(conbine_details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t begin{conbine_details::range_begin<CompileOption, wasm_i32>()};
            constexpr ::std::size_t end{conbine_details::range_end<CompileOption, wasm_i32>()};
            static_assert(begin <= curr_i32_stack_top && curr_i32_stack_top < end);
            constexpr ::std::size_t ring_sz{end - begin};
            static_assert(ring_sz != 0uz);
            constexpr ::std::size_t next_pos{details::ring_next_pos(curr_i32_stack_top, begin, end)};

            wasm_i32 const rhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            wasm_i32 lhs{};  // init
            if constexpr(ring_sz >= 2uz) { lhs = get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, next_pos>(type...); }
            else
            {
                // Ring too small to hold both operands: keep RHS in cache, load LHS from the operand stack memory.
                lhs = get_curr_val_from_operand_stack_cache<wasm_i32>(type...);
            }
            take_branch = ((lhs & rhs) != wasm_i32{});
        }
        else
        {
            wasm_i32 const rhs{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            wasm_i32 const lhs{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            take_branch = ((lhs & rhs) != wasm_i32{});
        }

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

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i32_and_nz` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_and_nz(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

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

        wasm_i32 const rhs{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_i32 const lhs{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        if((lhs & rhs) != wasm_i32{}) { typeref...[0] = jmp_ip; }
    }

    // br_if_local_eqz: local x == 0
    /// @brief Fused `br_if` using `eqz` test (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_local_eqz(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        wasm_i32 const x{conbine_details::load_local<wasm_i32>(type...[2u], local_off)};

# if UWVM_HAS_CPP_ATTRIBUTE(clang::nomerge)
        [[clang::nomerge]]
# endif
        if(x == wasm_i32{})
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

    /// @brief Fused `br_if` using `eqz` test (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_local_eqz(TypeRef & ... typeref) UWVM_THROWS
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
        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), typeref...[0], sizeof(jmp_ip));
        typeref...[0] += sizeof(jmp_ip);

        wasm_i32 const x{conbine_details::load_local<wasm_i32>(typeref...[2u], local_off)};
        if(x == wasm_i32{}) { typeref...[0] = jmp_ip; }
    }

    // local.get x; i32.const imm; cmp; br_if $L
    /// @brief Fused `local.get` + immediate compare + `br_if` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`, `<imm>`, `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, details::int_cmp Cmp, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_cmp_imm_localget(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;
        using wasm_u32 = conbine_details::wasm_u32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};
        wasm_i32 const imm{conbine_details::read_imm<wasm_i32>(type...[0])};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        wasm_i32 const x{conbine_details::load_local<wasm_i32>(type...[2u], local_off)};
        bool const take_branch{details::eval_int_cmp<Cmp, wasm_i32, wasm_u32>(x, imm)};

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

    /// @brief Fused `local.get` + immediate compare + `br_if` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `<imm>`, `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, details::int_cmp Cmp, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_cmp_imm_localget(TypeRef & ... typeref) UWVM_THROWS
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

        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};
        wasm_i32 const imm{conbine_details::read_imm<wasm_i32>(typeref...[0])};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), typeref...[0], sizeof(jmp_ip));
        typeref...[0] += sizeof(jmp_ip);

        wasm_i32 const x{conbine_details::load_local<wasm_i32>(typeref...[2u], local_off)};
        if(details::eval_int_cmp<Cmp, wasm_i32, wasm_u32>(x, imm)) { typeref...[0] = jmp_ip; }
    }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i32_lt_u_imm` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_lt_u_imm(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_i32_cmp_imm_localget<CompileOption, details::int_cmp::lt_u>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i32_ge_u_imm` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_ge_u_imm(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_i32_cmp_imm_localget<CompileOption, details::int_cmp::ge_u>(typeref...); }

    /// @brief Fused conditional branch entrypoint `uwvmint_br_if_i32_eq_imm` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_i32_eq_imm(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_br_if_i32_cmp_imm_localget<CompileOption, details::int_cmp::eq>(typeref...); }

    // br_if_local_tee_nz: pop i32, store to local, branch if non-zero (net -1 stack)
    /// @brief Fused `local.tee` + non-zero test + `br_if` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`, `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_local_tee_nz(Type... type) UWVM_THROWS
    {
        using wasm_i32 = conbine_details::wasm_i32;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(type...[0])};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));
        type...[0] += sizeof(jmp_ip);

        wasm_i32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
        conbine_details::store_local(type...[2u], local_off, v);

# if UWVM_HAS_CPP_ATTRIBUTE(clang::nomerge)
        [[clang::nomerge]]
# endif
        if(v != wasm_i32{})
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

    /// @brief Fused `local.tee` + non-zero test + `br_if` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `jump_target_ip`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_br_if_local_tee_nz(TypeRef & ... typeref) UWVM_THROWS
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
        auto const local_off{conbine_details::read_imm<conbine_details::local_offset_t>(typeref...[0])};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), typeref...[0], sizeof(jmp_ip));
        typeref...[0] += sizeof(jmp_ip);

        wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        conbine_details::store_local(typeref...[2u], local_off, v);
        if(v != wasm_i32{}) { typeref...[0] = jmp_ip; }
    }

    // ========================
    // translate: fptr selectors for fused ops (tail-call / byref)
    // ========================

    /**
     * @brief Translation-time function pointer selectors for fused opcodes.
     *
     * @details
     * These helpers pick the correct specialization based on the current stack-top ring position (when stack-top caching is enabled).
     * The returned pointer is always a direct implementation entrypoint; translation intentionally does not emit pointers to forwarding wrappers.
     */
    namespace translate
    {
        namespace details
        {
            // Alias to the outer `optable::details` namespace.
            // NOTE: This file also defines `translate::details`, so using `details::...` here
            // would refer to the translation helper namespace, not the opcode implementation one.
            namespace op_details = ::uwvm2::runtime::compiler::uwvm_int::optable::details;

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t Curr,
                      ::std::size_t End,
                      typename OpWrapper,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_by_currpos_conbine_impl(::std::size_t pos) noexcept
            {
                static_assert(Curr < End);
                if(pos == Curr) { return OpWrapper::template fptr<CompileOption, Curr, Type...>(); }
                else
                {
                    if constexpr(Curr + 1uz < End)
                    {
                        return select_stacktop_fptr_by_currpos_conbine_impl<CompileOption, Curr + 1uz, End, OpWrapper, Type...>(pos);
                    }
                    else
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t BeginPos,
                      ::std::size_t EndPos,
                      typename OpWrapper,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_or_default_conbine(::std::size_t pos) noexcept
            {
                if constexpr(BeginPos != EndPos)
                {
                    return select_stacktop_fptr_by_currpos_conbine_impl<CompileOption, BeginPos, EndPos, OpWrapper, Type...>(pos);
                }
                else
                {
                    return OpWrapper::template fptr<CompileOption, 0uz, Type...>();
                }
            }

            // ========================
            // stacktop_transform + br selectors
            // ========================

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Curr, ::std::size_t End, uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_uwvmint_br_stacktop_transform_to_begin_merged_impl(::std::size_t pos) noexcept
            {
                static_assert(Curr < End);
                if(pos == Curr) { return uwvmint_br_stacktop_transform_to_begin<CompileOption, Curr, Curr, Type...>; }
                else
                {
                    if constexpr(Curr + 1uz < End)
                    {
                        return select_uwvmint_br_stacktop_transform_to_begin_merged_impl<CompileOption, Curr + 1uz, End, Type...>(pos);
                    }
                    else
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t Curr,
                      ::std::size_t End,
                      ::std::size_t FixedFpPos,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_uwvmint_br_stacktop_transform_to_begin_int_only_impl(::std::size_t pos) noexcept
            {
                static_assert(Curr < End);
                if(pos == Curr) { return uwvmint_br_stacktop_transform_to_begin<CompileOption, Curr, FixedFpPos, Type...>; }
                else
                {
                    if constexpr(Curr + 1uz < End)
                    {
                        return select_uwvmint_br_stacktop_transform_to_begin_int_only_impl<CompileOption, Curr + 1uz, End, FixedFpPos, Type...>(pos);
                    }
                    else
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Curr, ::std::size_t End, uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_uwvmint_br_stacktop_transform_to_begin_fp_only_impl(::std::size_t pos) noexcept
            {
                static_assert(Curr < End);
                if(pos == Curr) { return uwvmint_br_stacktop_transform_to_begin<CompileOption, 0uz, Curr, Type...>; }
                else
                {
                    if constexpr(Curr + 1uz < End)
                    {
                        return select_uwvmint_br_stacktop_transform_to_begin_fp_only_impl<CompileOption, Curr + 1uz, End, Type...>(pos);
                    }
                    else
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t IntPos,
                      ::std::size_t FpCurr,
                      ::std::size_t FpEnd,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_uwvmint_br_stacktop_transform_to_begin_fp_impl(::std::size_t fp_pos) noexcept
            {
                static_assert(FpCurr < FpEnd);
                if(fp_pos == FpCurr) { return uwvmint_br_stacktop_transform_to_begin<CompileOption, IntPos, FpCurr, Type...>; }
                else
                {
                    if constexpr(FpCurr + 1uz < FpEnd)
                    {
                        return select_uwvmint_br_stacktop_transform_to_begin_fp_impl<CompileOption, IntPos, FpCurr + 1uz, FpEnd, Type...>(fp_pos);
                    }
                    else
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t IntCurr,
                      ::std::size_t IntEnd,
                      ::std::size_t FpBegin,
                      ::std::size_t FpEnd,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_uwvmint_br_stacktop_transform_to_begin_int_impl(::std::size_t int_pos,
                                                                                                                       ::std::size_t fp_pos) noexcept
            {
                static_assert(IntCurr < IntEnd);
                if(int_pos == IntCurr)
                {
                    return select_uwvmint_br_stacktop_transform_to_begin_fp_impl<CompileOption, IntCurr, FpBegin, FpEnd, Type...>(fp_pos);
                }
                else
                {
                    if constexpr(IntCurr + 1uz < IntEnd)
                    {
                        return select_uwvmint_br_stacktop_transform_to_begin_int_impl<CompileOption, IntCurr + 1uz, IntEnd, FpBegin, FpEnd, Type...>(int_pos,
                                                                                                                                                     fp_pos);
                    }
                    else
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            // wrappers for i32-producing localget fusions
            struct i32_add_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::add, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::add, Type...>; }
            };

            struct i32_sub_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::sub, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::sub, Type...>; }
            };

            struct i32_mul_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::mul, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::mul, Type...>; }
            };

            struct i32_and_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::and_, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::and_, Type...>; }
            };

            struct i32_or_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::or_, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::or_, Type...>; }
            };

            struct i32_xor_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::xor_, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::xor_, Type...>; }
            };

            struct i32_shl_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::shl, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::shl, Type...>; }
            };

            struct i32_shr_u_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::shr_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::shr_u, Type...>; }
            };

            struct i32_shr_s_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::shr_s, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_imm_localget<Opt, numeric_details::int_binop::shr_s, Type...>; }
            };

            // wrappers for `i32.const`/`i64.const` + integer binop on an existing stack value
            template <numeric_details::int_binop Op>
            struct i32_binop_imm_stack_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_imm_stack<Opt, Op, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_imm_stack<Opt, Op, Type...>; }
            };

            template <numeric_details::int_binop Op>
            struct i64_binop_imm_stack_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop_imm_stack<Opt, Op, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop_imm_stack<Opt, Op, Type...>; }
            };

            struct i32_eqz_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_eqz_localget<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_eqz_localget<Opt, Type...>; }
            };

            struct i32_eq_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_cmp_imm_localget<Opt, op_details::int_cmp::eq, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_cmp_imm_localget<Opt, op_details::int_cmp::eq, Type...>; }
            };

            struct i32_ne_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_cmp_imm_localget<Opt, op_details::int_cmp::ne, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_cmp_imm_localget<Opt, op_details::int_cmp::ne, Type...>; }
            };

            struct i32_lt_u_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_cmp_imm_localget<Opt, op_details::int_cmp::lt_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_cmp_imm_localget<Opt, op_details::int_cmp::lt_u, Type...>; }
            };

            struct i32_lt_s_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_cmp_imm_localget<Opt, op_details::int_cmp::lt_s, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_cmp_imm_localget<Opt, op_details::int_cmp::lt_s, Type...>; }
            };

            struct i32_ge_u_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_cmp_imm_localget<Opt, op_details::int_cmp::ge_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_cmp_imm_localget<Opt, op_details::int_cmp::ge_u, Type...>; }
            };

            struct i32_ge_s_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_cmp_imm_localget<Opt, op_details::int_cmp::ge_s, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_cmp_imm_localget<Opt, op_details::int_cmp::ge_s, Type...>; }
            };

            struct i32_add_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::add, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::add, Type...>; }
            };

            struct i32_add_2localget_local_tee_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_add_2localget_local_tee<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_add_2localget_local_tee<Opt, Type...>; }
            };

            struct i32_sub_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::sub, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::sub, Type...>; }
            };

            struct i32_mul_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::mul, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::mul, Type...>; }
            };

            struct i32_and_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::and_, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::and_, Type...>; }
            };

            struct i32_or_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::or_, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::or_, Type...>; }
            };

            struct i32_xor_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::xor_, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::xor_, Type...>; }
            };

            struct i32_rem_u_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::rem_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::rem_u, Type...>; }
            };

            struct i32_rem_s_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::rem_s, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_2localget<Opt, numeric_details::int_binop::rem_s, Type...>; }
            };

            struct i32_add_imm_local_tee_same_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_add_imm_local_tee_same<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_add_imm_local_tee_same<Opt, Type...>; }
            };

            struct i32_add_shl_imm_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_add_shl_imm_2localget<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_add_shl_imm_2localget<Opt, Type...>; }
            };

            struct i32_add_mul_imm_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_add_mul_imm_2localget<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_add_mul_imm_2localget<Opt, Type...>; }
            };

            struct i32_shl_imm_or_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_shl_imm_or<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_shl_imm_or<Opt, Type...>; }
            };

            struct i64_add_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop_imm_localget<Opt, numeric_details::int_binop::add, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop_imm_localget<Opt, numeric_details::int_binop::add, Type...>; }
            };

            struct i64_and_imm_localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop_imm_localget<Opt, numeric_details::int_binop::and_, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop_imm_localget<Opt, numeric_details::int_binop::and_, Type...>; }
            };

            struct i64_add_2localget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop_2localget<Opt, numeric_details::int_binop::add, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop_2localget<Opt, numeric_details::int_binop::add, Type...>; }
            };

            // wrappers for br_if fused ops (i32 stack)
            struct br_if_i32_eqz_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i32_eqz<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i32_eqz<Opt, Type...>; }
            };

            struct br_if_i32_eq_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::eq, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::eq, Type...>; }
            };

            struct br_if_i32_ne_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::ne, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::ne, Type...>; }
            };

            struct br_if_i32_lt_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::lt_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::lt_u, Type...>; }
            };

            struct br_if_i32_lt_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::lt_s, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::lt_s, Type...>; }
            };

            struct br_if_i32_gt_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::gt_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::gt_u, Type...>; }
            };

            struct br_if_i32_ge_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::ge_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::ge_u, Type...>; }
            };

            struct br_if_i32_ge_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::ge_s, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::ge_s, Type...>; }
            };

            struct br_if_i32_le_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::le_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::le_u, Type...>; }
            };

            struct br_if_i32_gt_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::gt_s, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::gt_s, Type...>; }
            };

            struct br_if_i32_le_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::le_s, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i32_cmp<Opt, op_details::int_cmp::le_s, Type...>; }
            };

            struct br_if_i32_and_nz_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i32_and_nz<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i32_and_nz<Opt, Type...>; }
            };

            // wrappers for br_if fused ops (i64 stack)
            struct br_if_i64_eqz_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i64_eqz<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i64_eqz<Opt, Type...>; }
            };

            struct br_if_i64_ne_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i64_cmp<Opt, op_details::int_cmp::ne, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i64_cmp<Opt, op_details::int_cmp::ne, Type...>; }
            };

            struct br_if_i64_gt_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i64_cmp<Opt, op_details::int_cmp::gt_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i64_cmp<Opt, op_details::int_cmp::gt_u, Type...>; }
            };

            struct br_if_i64_lt_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_i64_cmp<Opt, op_details::int_cmp::lt_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_i64_cmp<Opt, op_details::int_cmp::lt_u, Type...>; }
            };

            struct br_if_local_tee_nz_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_br_if_local_tee_nz<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_br_if_local_tee_nz<Opt, Type...>; }
            };
        }  // namespace details

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_add_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_conbine_impl<CompileOption,
                                                                             CompileOption.i32_stack_top_begin_pos,
                                                                             CompileOption.i32_stack_top_end_pos,
                                                                             details::i32_add_imm_localget_op,
                                                                             Type...>(curr.i32_stack_top_curr_pos);
            }
            else
            {
                return details::i32_add_imm_localget_op::template fptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_add_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_add_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_sub_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_sub_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_sub_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_sub_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_sub_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_sub_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_sub_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_sub_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_mul_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_mul_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_mul_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_mul_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_mul_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_mul_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_mul_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_mul_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_and_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_and_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_and_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_and_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_and_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_and_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_and_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_and_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_or_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_or_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_or_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_or_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_or_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_or_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_or_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_or_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_xor_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_xor_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_xor_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_xor_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_xor_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_xor_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_xor_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_xor_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_shl_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_shl_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_shl_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_shl_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_shl_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_shl_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_shl_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_shl_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_shr_u_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_shr_u_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_shr_u_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_shr_u_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_shr_u_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_shr_u_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_shr_u_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_shr_u_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_shr_s_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_shr_s_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_shr_s_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_shr_s_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_shr_s_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_shr_s_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_shr_s_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_shr_s_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_binop_imm_stack_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_binop_imm_stack_op<Op>,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_binop_imm_stack_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_binop_imm_stack_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_binop_imm_stack_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_binop_imm_stack_op<Op>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_binop_imm_stack_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_binop_imm_stack_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_binop_imm_stack_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i64_stack_top_begin_pos,
                                                                    CompileOption.i64_stack_top_end_pos,
                                                                    details::i64_binop_imm_stack_op<Op>,
                                                                    Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_binop_imm_stack_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_binop_imm_stack_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_binop_imm_stack_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i64_binop_imm_stack_op<Op>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_binop_imm_stack_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_binop_imm_stack_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_eq_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_eq_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_eq_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_eq_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_eq_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_eq_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_eq_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_eq_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_ne_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_ne_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ne_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ne_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_ne_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_ne_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ne_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ne_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_lt_u_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_lt_u_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_lt_u_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_lt_u_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_lt_u_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_lt_u_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_lt_u_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_lt_u_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_lt_s_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_lt_s_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_lt_s_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_lt_s_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_lt_s_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_lt_s_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_lt_s_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_lt_s_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_ge_u_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_ge_u_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ge_u_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ge_u_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_ge_u_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_ge_u_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ge_u_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ge_u_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_ge_s_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_ge_s_imm_localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ge_s_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ge_s_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_ge_s_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_ge_s_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ge_s_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ge_s_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_add_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_add_2localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_add_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_add_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_add_2localget_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_add_2localget_local_set<CompileOption, 0uz, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_2localget_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_2localget_local_set_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_i32_add_2localget_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_add_2localget_local_set<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_2localget_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_2localget_local_set_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_add_2localget_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_add_2localget_local_tee_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_2localget_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_2localget_local_tee_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_i32_add_2localget_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_add_2localget_local_tee_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_2localget_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_2localget_local_tee_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_sub_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_sub_2localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_sub_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_sub_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_sub_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_sub_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_sub_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_sub_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_mul_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_mul_2localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_mul_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_mul_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_mul_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_mul_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_mul_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_mul_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_and_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_and_2localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_and_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_and_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_and_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_and_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_and_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_and_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_or_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_or_2localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_or_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_or_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_or_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_or_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_or_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_or_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_xor_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_xor_2localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_xor_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_xor_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_xor_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_xor_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_xor_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_xor_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_rem_u_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_rem_u_2localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rem_u_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rem_u_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_rem_u_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_rem_u_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rem_u_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rem_u_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_rem_s_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_rem_s_2localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rem_s_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rem_s_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_rem_s_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_rem_s_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rem_s_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rem_s_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // update_local fusions
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_add_imm_local_set_same_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_add_imm_local_set_same<CompileOption, 0uz, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_imm_local_set_same_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_imm_local_set_same_fptr<CompileOption, TypeInTuple...>(curr); }

        // update_global fusions
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_add_imm_global_set_same_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_add_imm_global_set_same<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_imm_global_set_same_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_imm_global_set_same_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_i32_add_imm_global_set_same_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_add_imm_global_set_same<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_imm_global_set_same_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_imm_global_set_same_fptr<CompileOption, TypeInTuple...>(curr); }

        // call_fuse fusions
        template <::std::size_t ParamCount, typename RetT>
        struct call_stacktop_i32_op
        {
            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Curr, uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            static consteval uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
            { return uwvmint_call_stacktop_i32<CompileOption, Curr, ParamCount, RetT, Type...>; }
        };

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t ParamCount, typename RetT, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_call_stacktop_i32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    call_stacktop_i32_op<ParamCount, RetT>,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t ParamCount, typename RetT, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_call_stacktop_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_stacktop_i32_fptr<CompileOption, ParamCount, RetT, TypeInTuple...>(curr); }

        template <::std::size_t ParamCount, typename RetT>
        struct call_stacktop_f32_op
        {
            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Curr, uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            static consteval uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
            { return uwvmint_call_stacktop_f32<CompileOption, Curr, ParamCount, RetT, Type...>; }
        };

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t ParamCount, typename RetT, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_call_stacktop_f32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.f32_stack_top_begin_pos,
                                                                    CompileOption.f32_stack_top_end_pos,
                                                                    call_stacktop_f32_op<ParamCount, RetT>,
                                                                    Type...>(curr.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t ParamCount, typename RetT, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_call_stacktop_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_stacktop_f32_fptr<CompileOption, ParamCount, RetT, TypeInTuple...>(curr); }

        template <::std::size_t ParamCount, typename RetT>
        struct call_stacktop_f64_op
        {
            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Curr, uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            static consteval uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
            { return uwvmint_call_stacktop_f64<CompileOption, Curr, ParamCount, RetT, Type...>; }
        };

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t ParamCount, typename RetT, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_call_stacktop_f64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.f64_stack_top_begin_pos,
                                                                    CompileOption.f64_stack_top_end_pos,
                                                                    call_stacktop_f64_op<ParamCount, RetT>,
                                                                    Type...>(curr.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t ParamCount, typename RetT, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_call_stacktop_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_stacktop_f64_fptr<CompileOption, ParamCount, RetT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_call_drop_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_call_drop<CompileOption, RetT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_call_drop_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_drop_fptr<CompileOption, RetT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_call_drop_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_call_drop<CompileOption, RetT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_call_drop_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_drop_fptr<CompileOption, RetT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_call_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_call_local_set<CompileOption, RetT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_call_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_local_set_fptr<CompileOption, RetT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_call_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_call_local_set<CompileOption, RetT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_call_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_local_set_fptr<CompileOption, RetT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_call_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_call_local_tee<CompileOption, RetT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_call_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_local_tee_fptr<CompileOption, RetT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_call_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_call_local_tee<CompileOption, RetT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename RetT, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_call_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_call_local_tee_fptr<CompileOption, RetT, TypeInTuple...>(curr); }

        // bit_pack fusions
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_shl_imm_or_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_shl_imm_or_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_shl_imm_or_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_shl_imm_or_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_shl_imm_or_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i32_shl_imm_or_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_shl_imm_or_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_shl_imm_or_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_i32_add_imm_local_set_same_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_add_imm_local_set_same<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_imm_local_set_same_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_imm_local_set_same_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_add_imm_local_tee_same_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_add_imm_local_tee_same_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_imm_local_tee_same_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_imm_local_tee_same_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_i32_add_imm_local_tee_same_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_add_imm_local_tee_same<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_imm_local_tee_same_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_imm_local_tee_same_fptr<CompileOption, TypeInTuple...>(curr); }

        // addr_calc fusions
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_add_shl_imm_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_add_shl_imm_2localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_shl_imm_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_shl_imm_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_i32_add_shl_imm_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_add_shl_imm_2localget<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_shl_imm_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_shl_imm_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_add_mul_imm_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::i32_add_mul_imm_2localget_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_mul_imm_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_mul_imm_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_i32_add_mul_imm_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_add_mul_imm_2localget<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_mul_imm_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_mul_imm_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // i64 fusions
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_add_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i64_stack_top_begin_pos,
                                                                    CompileOption.i64_stack_top_end_pos,
                                                                    details::i64_add_imm_localget_op,
                                                                    Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_add_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_add_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_add_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i64_add_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_add_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_add_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_and_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i64_stack_top_begin_pos,
                                                                    CompileOption.i64_stack_top_end_pos,
                                                                    details::i64_and_imm_localget_op,
                                                                    Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_and_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_and_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_and_imm_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i64_and_imm_localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_and_imm_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_and_imm_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_add_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i64_stack_top_begin_pos,
                                                                    CompileOption.i64_stack_top_end_pos,
                                                                    details::i64_add_2localget_op,
                                                                    Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_add_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_add_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_add_2localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::i64_add_2localget_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_add_2localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_add_2localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_eqz_localget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_conbine_impl<CompileOption,
                                                                             CompileOption.i32_stack_top_begin_pos,
                                                                             CompileOption.i32_stack_top_end_pos,
                                                                             details::i32_eqz_localget_op,
                                                                             Type...>(curr.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_eqz_localget<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_eqz_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_eqz_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_eqz_localget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_eqz_localget<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_eqz_localget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_eqz_localget_fptr<CompileOption, TypeInTuple...>(curr); }

        // Branch fused fptrs
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_br_stacktop_transform_to_begin_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            constexpr bool i32_enabled{CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos};
            constexpr bool i64_enabled{CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos};
            constexpr bool f32_enabled{CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos};
            constexpr bool f64_enabled{CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos};
            constexpr bool v128_enabled{CompileOption.v128_stack_top_begin_pos != CompileOption.v128_stack_top_end_pos};

            constexpr bool int_enabled{i32_enabled || i64_enabled};
            constexpr bool fp_enabled{f32_enabled || f64_enabled || v128_enabled};

            if constexpr(!int_enabled && !fp_enabled) { return uwvmint_br_stacktop_transform_to_begin<CompileOption, 0uz, 0uz, Type...>; }

            constexpr ::std::size_t int_begin{i32_enabled ? CompileOption.i32_stack_top_begin_pos : CompileOption.i64_stack_top_begin_pos};
            constexpr ::std::size_t int_end{i32_enabled ? CompileOption.i32_stack_top_end_pos : CompileOption.i64_stack_top_end_pos};
            constexpr ::std::size_t fp_begin{f32_enabled ? CompileOption.f32_stack_top_begin_pos
                                                         : (f64_enabled ? CompileOption.f64_stack_top_begin_pos : CompileOption.v128_stack_top_begin_pos)};
            constexpr ::std::size_t fp_end{f32_enabled ? CompileOption.f32_stack_top_end_pos
                                                       : (f64_enabled ? CompileOption.f64_stack_top_end_pos : CompileOption.v128_stack_top_end_pos)};

            if constexpr(int_enabled && fp_enabled)
            {
                constexpr bool same_range{int_begin == fp_begin && int_end == fp_end};
                if constexpr(same_range)
                {
                    ::std::size_t const pos{i32_enabled ? curr.i32_stack_top_curr_pos : curr.i64_stack_top_curr_pos};
                    return details::select_uwvmint_br_stacktop_transform_to_begin_merged_impl<CompileOption, int_begin, int_end, Type...>(pos);
                }
                else
                {
                    ::std::size_t const int_pos{i32_enabled ? curr.i32_stack_top_curr_pos : curr.i64_stack_top_curr_pos};
                    ::std::size_t const fp_pos{f32_enabled ? curr.f32_stack_top_curr_pos
                                                           : (f64_enabled ? curr.f64_stack_top_curr_pos : curr.v128_stack_top_curr_pos)};
                    return details::select_uwvmint_br_stacktop_transform_to_begin_int_impl<CompileOption, int_begin, int_end, fp_begin, fp_end, Type...>(
                        int_pos,
                        fp_pos);
                }
            }
            else if constexpr(int_enabled)
            {
                ::std::size_t const int_pos{i32_enabled ? curr.i32_stack_top_curr_pos : curr.i64_stack_top_curr_pos};
                return details::select_uwvmint_br_stacktop_transform_to_begin_int_only_impl<CompileOption, int_begin, int_end, 0uz, Type...>(int_pos);
            }
            else
            {
                ::std::size_t const fp_pos{f32_enabled ? curr.f32_stack_top_curr_pos
                                                       : (f64_enabled ? curr.f64_stack_top_curr_pos : curr.v128_stack_top_curr_pos)};
                return details::select_uwvmint_br_stacktop_transform_to_begin_fp_only_impl<CompileOption, fp_begin, fp_end, Type...>(fp_pos);
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_stacktop_transform_to_begin_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_stacktop_transform_to_begin_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_eqz_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_conbine_impl<CompileOption,
                                                                             CompileOption.i32_stack_top_begin_pos,
                                                                             CompileOption.i32_stack_top_end_pos,
                                                                             details::br_if_i32_eqz_op,
                                                                             Type...>(curr.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_br_if_i32_eqz<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_eqz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_eqz_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_eqz_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i32_eqz<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_eqz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_eqz_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_eq_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::br_if_i32_eq_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_eq_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_eq_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_eq_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_i32_eq_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_eq_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_eq_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_ne_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::br_if_i32_ne_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_ne_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_ne_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_ne_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_i32_ne_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_ne_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_ne_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_lt_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::br_if_i32_lt_u_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_lt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_lt_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_lt_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_i32_lt_u_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_lt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_lt_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_lt_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::br_if_i32_lt_s_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_lt_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_lt_s_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_lt_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_i32_lt_s_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_lt_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_lt_s_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_gt_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::br_if_i32_gt_u_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_gt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_gt_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_gt_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_i32_gt_u_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_gt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_gt_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_ge_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::br_if_i32_ge_u_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_ge_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_ge_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_ge_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_i32_ge_u_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_ge_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_ge_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_ge_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::br_if_i32_ge_s_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_ge_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_ge_s_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_ge_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_i32_ge_s_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_ge_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_ge_s_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_le_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::br_if_i32_le_u_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_le_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_le_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_le_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_i32_le_u_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_le_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_le_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_gt_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::br_if_i32_gt_s_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_gt_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_gt_s_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_gt_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_i32_gt_s_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_gt_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_gt_s_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_le_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::br_if_i32_le_s_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_le_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_le_s_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_le_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_i32_le_s_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_le_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_le_s_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_and_nz_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::br_if_i32_and_nz_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_and_nz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_and_nz_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_and_nz_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i32_and_nz<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_and_nz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_and_nz_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i64_eqz_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i64_stack_top_begin_pos,
                                                                    CompileOption.i64_stack_top_end_pos,
                                                                    details::br_if_i64_eqz_op,
                                                                    Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i64_eqz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i64_eqz_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i64_eqz_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i64_eqz<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i64_eqz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i64_eqz_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i64_ne_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i64_stack_top_begin_pos,
                                                                    CompileOption.i64_stack_top_end_pos,
                                                                    details::br_if_i64_ne_op,
                                                                    Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i64_ne_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i64_ne_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i64_ne_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_i64_ne_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i64_ne_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i64_ne_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i64_gt_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i64_stack_top_begin_pos,
                                                                    CompileOption.i64_stack_top_end_pos,
                                                                    details::br_if_i64_gt_u_op,
                                                                    Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i64_gt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i64_gt_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i64_gt_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_i64_gt_u_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i64_gt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i64_gt_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i64_lt_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i64_stack_top_begin_pos,
                                                                    CompileOption.i64_stack_top_end_pos,
                                                                    details::br_if_i64_lt_u_op,
                                                                    Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i64_lt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i64_lt_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i64_lt_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return details::br_if_i64_lt_u_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i64_lt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i64_lt_u_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_local_eqz_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_local_eqz<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_local_eqz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_local_eqz_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_local_eqz_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_local_eqz<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_local_eqz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_local_eqz_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_local_tee_nz_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return details::select_stacktop_fptr_or_default_conbine<CompileOption,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos,
                                                                    details::br_if_local_tee_nz_op,
                                                                    Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_local_tee_nz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_local_tee_nz_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_local_tee_nz_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_local_tee_nz<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_local_tee_nz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_local_tee_nz_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_lt_u_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i32_cmp_imm_localget<CompileOption, details::op_details::int_cmp::lt_u, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_lt_u_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_lt_u_imm_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_lt_u_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i32_cmp_imm_localget<CompileOption, details::op_details::int_cmp::lt_u, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_lt_u_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_lt_u_imm_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_lt_s_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i32_cmp_imm_localget<CompileOption, details::op_details::int_cmp::lt_s, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_lt_s_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_lt_s_imm_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_lt_s_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i32_cmp_imm_localget<CompileOption, details::op_details::int_cmp::lt_s, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_lt_s_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_lt_s_imm_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_ge_u_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i32_cmp_imm_localget<CompileOption, details::op_details::int_cmp::ge_u, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_ge_u_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_ge_u_imm_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_ge_u_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i32_cmp_imm_localget<CompileOption, details::op_details::int_cmp::ge_u, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_ge_u_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_ge_u_imm_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_ge_s_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i32_cmp_imm_localget<CompileOption, details::op_details::int_cmp::ge_s, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_ge_s_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_ge_s_imm_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_ge_s_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i32_cmp_imm_localget<CompileOption, details::op_details::int_cmp::ge_s, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_ge_s_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                             ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_ge_s_imm_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_i32_eq_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i32_cmp_imm_localget<CompileOption, details::op_details::int_cmp::eq, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_eq_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_eq_imm_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_br_if_i32_eq_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if_i32_cmp_imm_localget<CompileOption, details::op_details::int_cmp::eq, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_br_if_i32_eq_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_i32_eq_imm_fptr<CompileOption, TypeInTuple...>(curr); }

    }  // namespace translate

    // =====================================================
    // Combined memory opcodes (fusions)
    // =====================================================

    namespace details::memop
    {
        using local_offset_t = ::std::size_t;

        template <typename T>
        UWVM_ALWAYS_INLINE inline constexpr T load_local(::std::byte* local_base, local_offset_t off) noexcept
        {
            T v;  // no init
            ::std::memcpy(::std::addressof(v), local_base + off, sizeof(v));
            return v;
        }

        template <typename T>
        UWVM_ALWAYS_INLINE inline constexpr void store_local(::std::byte* local_base, local_offset_t off, T v) noexcept
        { ::std::memcpy(local_base + off, ::std::addressof(v), sizeof(v)); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval ::std::size_t range_begin() noexcept
        {
            if constexpr(::std::same_as<OperandT, details::wasm_i32>) { return CompileOption.i32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, details::wasm_i64>) { return CompileOption.i64_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, details::wasm_f32>) { return CompileOption.f32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, details::wasm_f64>) { return CompileOption.f64_stack_top_begin_pos; }
            else
            {
                return SIZE_MAX;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval ::std::size_t range_end() noexcept
        {
            if constexpr(::std::same_as<OperandT, details::wasm_i32>) { return CompileOption.i32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, details::wasm_i64>) { return CompileOption.i64_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, details::wasm_f32>) { return CompileOption.f32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, details::wasm_f64>) { return CompileOption.f64_stack_top_end_pos; }
            else
            {
                return SIZE_MAX;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... TypeRef>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void push_value(OperandT v, TypeRef&... typeref) noexcept
        {
            if constexpr(details::stacktop_enabled_for<CompileOption, OperandT>())
            {
                constexpr ::std::size_t begin{range_begin<CompileOption, OperandT>()};
                constexpr ::std::size_t end{range_end<CompileOption, OperandT>()};
                static_assert(begin <= curr_stack_top && curr_stack_top < end);
                static_assert(sizeof...(TypeRef) >= end);

                constexpr ::std::size_t new_pos{details::ring_prev_pos(curr_stack_top, begin, end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, OperandT, new_pos>(v, typeref...);
            }
            else
            {
                ::std::memcpy(typeref...[1u], ::std::addressof(v), sizeof(v));
                typeref...[1u] += sizeof(v);
            }
        }

        // -------------------------------------------------
        // local.get + load (push result)
        // Layout: [op][local_off][memory*][offset:u32][next]
        // -------------------------------------------------

        /// @brief Internal fused memory load (`i32`) via `local.get` address + `offset` immediate (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_load_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
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
            auto const out{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            push_value<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused `local.get a; local.get b; i32.load` (tail-call).
        /// @details
        /// Leaves the deeper `local.get a` value on the operand stack, and loads from `local.get b` as the effective address.
        ///
        /// Layout: `[op][off_a][off_b][memory*][offset:u32][next]`.
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_load_localget2_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const off_a{details::read_imm<local_offset_t>(type...[0])};
            local_offset_t const off_b{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const keep_addr{load_local<wasm_i32>(type...[2u], off_a)};
            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], off_b)};
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
            auto const out{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            // Push keep_addr first, then push out, so the top is the loaded value and the deeper slot is the kept address.
            push_value<CompileOption, wasm_i32, curr_i32_stack_top>(keep_addr, type...);
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                constexpr ::std::size_t begin{range_begin<CompileOption, wasm_i32>()};
                constexpr ::std::size_t end{range_end<CompileOption, wasm_i32>()};
                static_assert(begin <= curr_i32_stack_top && curr_i32_stack_top < end);
                constexpr ::std::size_t after_keep_pos{details::ring_prev_pos(curr_i32_stack_top, begin, end)};
                push_value<CompileOption, wasm_i32, after_keep_pos>(out, type...);
            }
            else
            {
                push_value<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load (`i32`) via `local.get` + immediate add + `offset` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: `local_offset_t`, `wasm_i32 imm`, `native_memory_t*`, `wasm_u32 offset`.
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_load_local_plus_imm(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
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
            auto const out{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            push_value<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load8 (`i32`) via `local.get` address + `offset` immediate (tail-call, signedness via template).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn,
                  bool Signed,
                  uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t curr_i32_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_load8_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
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
                if(details::should_trap_oob_unlocked(memory, eff65, 1uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 1uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 1uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            ::std::uint_least8_t b{details::load_u8(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            wasm_i32 out{};
            if constexpr(Signed) { out = static_cast<wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least8_t>(b))); }
            else
            {
                out = static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(b));
            }

            push_value<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load16 (`i32`) via `local.get` address + `offset` immediate (tail-call, signedness via template).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn,
                  bool Signed,
                  uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t curr_i32_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_load16_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
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
                if(details::should_trap_oob_unlocked(memory, eff65, 2uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 2uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 2uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            ::std::uint_least16_t tmp;  // no init
            ::std::memcpy(::std::addressof(tmp), details::ptr_add_u64(memory.memory_begin, eff), sizeof(tmp));
            tmp = ::fast_io::little_endian(tmp);
            details::exit_memory_operation_memory_lock(memory);

            wasm_i32 out{};
            if constexpr(Signed) { out = static_cast<wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least16_t>(tmp))); }
            else
            {
                out = static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(tmp));
            }

            push_value<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load (`i64`) via `local.get` address + `offset` immediate (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i64_load_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_i64 = details::wasm_i64;
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
            auto const out{details::load_i64_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            push_value<CompileOption, wasm_i64, curr_i64_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        // -------------------------------------------------
        // local.get + store (no stack effect)
        // Layout: [op][p_off][v_off][memory*][offset:u32][next]
        // -------------------------------------------------

        /// @brief Internal fused memory store (`i32`) via `local.get` address + `offset` immediate (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_store_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
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
            wasm_i32 const v{load_local<wasm_i32>(type...[2u], v_off)};
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
            details::store_i32_le(details::ptr_add_u64(memory.memory_begin, eff), v);
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory store (`i32`) via `local.get` + immediate add + `offset` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: `local_offset_t` (addr), `wasm_i32 imm`, `local_offset_t` (value), `native_memory_t*`, `wasm_u32 offset`.
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_store_local_plus_imm(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
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
            wasm_i32 const v{load_local<wasm_i32>(type...[2u], v_off)};
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
            details::store_i32_le(details::ptr_add_u64(memory.memory_begin, eff), v);
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory store immediate (`i32`) via `local.get` address + `offset` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_store_imm_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            wasm_i32 const imm{details::read_imm<wasm_i32>(type...[0])};
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
            details::store_i32_le(details::ptr_add_u64(memory.memory_begin, eff), imm);
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory store8 via `local.get` address + `offset` immediate (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_store8_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
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
            wasm_i32 const v{load_local<wasm_i32>(type...[2u], v_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 1uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 1uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 1uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            details::store_u8(details::ptr_add_u64(memory.memory_begin, eff), static_cast<::std::uint_least8_t>(v));
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory store8 immediate via `local.get` address + `offset` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_store8_imm_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            wasm_i32 const imm{details::read_imm<wasm_i32>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], p_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 1uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 1uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 1uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            details::store_u8(details::ptr_add_u64(memory.memory_begin, eff), static_cast<::std::uint_least8_t>(imm));
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory store16 via `local.get` address + `offset` immediate (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_store16_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
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
            wasm_i32 const v{load_local<wasm_i32>(type...[2u], v_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 2uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 2uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 2uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            details::store_u16_le(details::ptr_add_u64(memory.memory_begin, eff), static_cast<::std::uint_least16_t>(v));
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory store16 immediate via `local.get` address + `offset` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_store16_imm_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            wasm_i32 const imm{details::read_imm<wasm_i32>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};

            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], p_off)};
            auto const eff65{details::wasm32_effective_offset(addr, offset)};

            auto const& memory{*memory_p};
            details::enter_memory_operation_memory_lock(memory);
            if constexpr(BoundsCheckFn == details::bounds_check_generic)
            {
                if(details::should_trap_oob_unlocked(memory, eff65, 2uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 2uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 2uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            details::store_u16_le(details::ptr_add_u64(memory.memory_begin, eff), static_cast<::std::uint_least16_t>(imm));
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory store (`i64`) via `local.get` address + `offset` immediate (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i64_store_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_i64 = details::wasm_i64;
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
            wasm_i64 const v{load_local<wasm_i64>(type...[2u], v_off)};
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
            details::store_i64_le(details::ptr_add_u64(memory.memory_begin, eff), v);
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory store32 (`i64` source) via `local.get` address + `offset` immediate (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i64_store32_localget_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_i64 = details::wasm_i64;
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
            wasm_i64 const v{load_local<wasm_i64>(type...[2u], v_off)};
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
            details::store_u32_le(details::ptr_add_u64(memory.memory_begin, eff), static_cast<::std::uint_least32_t>(v));
            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        // -------------------------------------------------
        // local.get + load + local.set/tee
        // Layout: [op][p_off][dst_off][memory*][offset:u32][next]
        // -------------------------------------------------

        /// @brief Internal fused memory load (`i32`) with `local.set` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_load_localget_set_local(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
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
            auto const out{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            store_local(type...[2u], dst_off, out);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load8_u (`i32`) with `local.set` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_load8_u_localget_set_local(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
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
                if(details::should_trap_oob_unlocked(memory, eff65, 1uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 1uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 1uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            ::std::uint_least8_t b{details::load_u8(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            wasm_i32 const out{static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(b))};
            store_local(type...[2u], dst_off, out);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load (`i64`) with `local.set` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i64_load_localget_set_local(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_i64 = details::wasm_i64;
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
            wasm_i64 const out{details::load_i64_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            store_local(type...[2u], dst_off, out);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load (`i32`) with `local.tee` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_load_localget_tee_local(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
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
            auto const out{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            store_local(type...[2u], dst_off, out);
            push_value<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load8_u (`i32`) with `local.tee` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_load8_u_localget_tee_local(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
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
                if(details::should_trap_oob_unlocked(memory, eff65, 1uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 1uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 1uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            ::std::uint_least8_t b{details::load_u8(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            wasm_i32 const out{static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(b))};
            store_local(type...[2u], dst_off, out);
            push_value<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load8_s (`i32`) with `local.tee` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_load8_s_localget_tee_local(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
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
                if(details::should_trap_oob_unlocked(memory, eff65, 1uz)) [[unlikely]]
                {
                    type...[0] = op_begin;
                    auto const memory_length{details::load_memory_length_for_oob_unlocked(memory)};
                    details::memory_oob_terminate(0uz, static_cast<::std::uint_least64_t>(offset), eff65, memory_length, 1uz);
                }
            }
            else
            {
                BoundsCheckFn(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 1uz);
            }

            ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
            ::std::uint_least8_t b{details::load_u8(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            wasm_i32 const out{static_cast<wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least8_t>(b)))};
            store_local(type...[2u], dst_off, out);
            push_value<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memory load (`i64`) with `local.tee` (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i64_load_localget_tee_local(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_i64 = details::wasm_i64;
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
            wasm_i64 const out{details::load_i64_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            store_local(type...[2u], dst_off, out);
            push_value<CompileOption, wasm_i64, curr_i64_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        // -------------------------------------------------
        // memcpy: local.get dst/src + load + store (net 0)
        // Layout: [op][dst_off][src_off][memory*][soff:u32][doff:u32][next]
        // -------------------------------------------------

        /// @brief Internal fused memcpy (`i32`): `local.get` dst/src + load + store (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_memcpy_localget_localget(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
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
                    // Prefer reporting the first failing access (src first).
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

            wasm_i32 const tmp{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, src_eff))};
            details::store_i32_le(details::ptr_add_u64(memory.memory_begin, dst_eff), tmp);

            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused memcpy (`i64`): `local.get` dst/src + load + store (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i64_memcpy_localget_localget(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_i64 = details::wasm_i64;
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
                    // Prefer reporting the first failing access (src first).
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

            wasm_i64 const out{details::load_i64_le(details::ptr_add_u64(memory.memory_begin, src_eff))};
            details::store_i64_le(details::ptr_add_u64(memory.memory_begin, dst_eff), out);

            details::exit_memory_operation_memory_lock(memory);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        // -------------------------------------------------
        // load_arith: load + imm op (push result)
        // Layout: [op][p_off][memory*][offset:u32][imm:i32][next]
        // -------------------------------------------------

        /// @brief Internal fused load + add immediate (`i32`) (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_load_add_imm(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};
            wasm_i32 const imm{details::read_imm<wasm_i32>(type...[0])};

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
            wasm_i32 const loaded{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            ::std::uint_least32_t const lu{::std::bit_cast<::std::uint_least32_t>(loaded)};
            ::std::uint_least32_t const iu{::std::bit_cast<::std::uint_least32_t>(imm)};
            wasm_i32 const out{::std::bit_cast<wasm_i32>(static_cast<::std::uint_least32_t>(lu + iu))};

            push_value<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused `local.get a; local.get b; i32.load; i32.const imm; i32.add` (tail-call).
        /// @details
        /// Leaves the deeper `local.get a` value on the operand stack, and performs `load(b)+imm` as the top value.
        ///
        /// Layout: `[op][off_a][off_b][memory*][offset:u32][imm:i32][next]`.
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_load_add_imm_localget2_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const off_a{details::read_imm<local_offset_t>(type...[0])};
            local_offset_t const off_b{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};
            wasm_i32 const imm{details::read_imm<wasm_i32>(type...[0])};

            wasm_i32 const keep_addr{load_local<wasm_i32>(type...[2u], off_a)};
            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], off_b)};
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
            wasm_i32 const loaded{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            ::std::uint_least32_t const lu{::std::bit_cast<::std::uint_least32_t>(loaded)};
            ::std::uint_least32_t const iu{::std::bit_cast<::std::uint_least32_t>(imm)};
            wasm_i32 const out{::std::bit_cast<wasm_i32>(static_cast<::std::uint_least32_t>(lu + iu))};

            push_value<CompileOption, wasm_i32, curr_i32_stack_top>(keep_addr, type...);
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                constexpr ::std::size_t begin{range_begin<CompileOption, wasm_i32>()};
                constexpr ::std::size_t end{range_end<CompileOption, wasm_i32>()};
                static_assert(begin <= curr_i32_stack_top && curr_i32_stack_top < end);
                constexpr ::std::size_t after_keep_pos{details::ring_prev_pos(curr_i32_stack_top, begin, end)};
                push_value<CompileOption, wasm_i32, after_keep_pos>(out, type...);
            }
            else
            {
                push_value<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused load + and immediate (`i32`) (tail-call).
        /// @details
        /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
        /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
        /// - Immediates: see implementation (bytecode order).
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_load_and_imm(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const p_off{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};
            wasm_i32 const imm{details::read_imm<wasm_i32>(type...[0])};

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
            wasm_i32 const loaded{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            wasm_i32 const out{static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(loaded) & static_cast<::std::uint_least32_t>(imm))};
            push_value<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        /// @brief Internal fused `local.get a; local.get b; i32.load; i32.const imm; i32.and` (tail-call).
        /// @details
        /// Leaves the deeper `local.get a` value on the operand stack, and performs `load(b)&imm` as the top value.
        ///
        /// Layout: `[op][off_a][off_b][memory*][offset:u32][imm:i32][next]`.
        template <auto BoundsCheckFn, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void i32_load_and_imm_localget2_off(Type... type) UWVM_THROWS
        {
            using wasm_i32 = details::wasm_i32;
            using wasm_u32 = details::wasm_u32;
            using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            auto const op_begin{type...[0]};
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const off_a{details::read_imm<local_offset_t>(type...[0])};
            local_offset_t const off_b{details::read_imm<local_offset_t>(type...[0])};
            native_memory_t* memory_p{details::read_imm<native_memory_t*>(type...[0])};
            wasm_u32 const offset{details::read_imm<wasm_u32>(type...[0])};
            wasm_i32 const imm{details::read_imm<wasm_i32>(type...[0])};

            wasm_i32 const keep_addr{load_local<wasm_i32>(type...[2u], off_a)};
            wasm_i32 const addr{load_local<wasm_i32>(type...[2u], off_b)};
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
            wasm_i32 const loaded{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};
            details::exit_memory_operation_memory_lock(memory);

            wasm_i32 const out{static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(loaded) & static_cast<::std::uint_least32_t>(imm))};

            push_value<CompileOption, wasm_i32, curr_i32_stack_top>(keep_addr, type...);
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                constexpr ::std::size_t begin{range_begin<CompileOption, wasm_i32>()};
                constexpr ::std::size_t end{range_end<CompileOption, wasm_i32>()};
                static_assert(begin <= curr_i32_stack_top && curr_i32_stack_top < end);
                constexpr ::std::size_t after_keep_pos{details::ring_prev_pos(curr_i32_stack_top, begin, end)};
                push_value<CompileOption, wasm_i32, after_keep_pos>(out, type...);
            }
            else
            {
                push_value<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }
    }  // namespace details::memop

    // -------------------------------------
    // Public opcode wrappers (tail-call)
    // -------------------------------------

    /// @brief Fused memory op with `local.get` address + `offset` immediate (`i32`) (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`, `native_memory_t*`, `wasm_u32 offset`.

    /// @brief Fused memory op with `local.get` + immediate add + `offset` (`i32`) (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32 imm`, `native_memory_t*`, `wasm_u32 offset`.

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load8_u_localget_off` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load8_s_localget_off` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load16_u_localget_off` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load16_s_localget_off` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused memory op with `local.get` address + `offset` immediate (`i64`) (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`, `native_memory_t*`, `wasm_u32 offset`.

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_store_localget_off` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused memory op with `local.get` + immediate add + `offset` (`i32` store) (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32 imm`, `local_offset_t` (value), `native_memory_t*`, `wasm_u32 offset`.

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_store_imm_localget_off` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_store8_localget_off` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_store8_imm_localget_off` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_store16_localget_off` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_store16_imm_localget_off` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused combined opcode entrypoint `uwvmint_i64_store_localget_off` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused combined opcode entrypoint `uwvmint_i64_store32_localget_off` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load_localget_set_local` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load8_u_localget_set_local` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused combined opcode entrypoint `uwvmint_i64_load_localget_set_local` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused memory load + `local.tee` (`i32`) (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (addr), `local_offset_t` (dst), `native_memory_t*`, `wasm_u32 offset`.

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load8_u_localget_tee_local` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load8_s_localget_tee_local` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused memory load + `local.tee` (`i64`) (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t` (addr), `local_offset_t` (dst), `native_memory_t*`, `wasm_u32 offset`.

    /// @brief Fused `local.get` + `i32.memcpy.localget` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`.

    /// @brief Fused `local.get` + `i64.memcpy.localget` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: `local_offset_t`.

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load_add_imm` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load_and_imm` (tail-call).
    /// @details
    /// - Stack-top optimization: see @ref uwvmint_conbine_stacktop_opt.
    /// - `type[0]` layout: see @ref uwvmint_conbine_tailcall_layout.
    /// - Immediates: see implementation (bytecode order).

    // -------------------------------------
    // Public opcode wrappers (byref)
    // -------------------------------------

    /// @brief Fused memory op with `local.get` address + `offset` immediate (`i32`) (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `native_memory_t*`, `wasm_u32 offset`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_load_localget_off(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
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
        wasm_i32 const out{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused memory op with `local.get` + immediate add + `offset` (`i32`) (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32 imm`, `native_memory_t*`, `wasm_u32 offset`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_load_local_plus_imm(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
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
        wasm_i32 const out{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load8_u_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_load8_u_localget_off(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
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
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 1uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        ::std::uint_least8_t b{details::load_u8(details::ptr_add_u64(memory.memory_begin, eff))};
        wasm_i32 const out{static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(b))};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load8_s_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_load8_s_localget_off(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
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
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 1uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        ::std::uint_least8_t b{details::load_u8(details::ptr_add_u64(memory.memory_begin, eff))};
        wasm_i32 const out{static_cast<wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least8_t>(b)))};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load16_u_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_load16_u_localget_off(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
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
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 2uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        ::std::uint_least16_t tmp;  // no init
        ::std::memcpy(::std::addressof(tmp), details::ptr_add_u64(memory.memory_begin, eff), sizeof(tmp));
        tmp = ::fast_io::little_endian(tmp);

        wasm_i32 const out{static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(tmp))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load16_s_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_load16_s_localget_off(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
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
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 2uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        ::std::uint_least16_t tmp;  // no init
        ::std::memcpy(::std::addressof(tmp), details::ptr_add_u64(memory.memory_begin, eff), sizeof(tmp));
        tmp = ::fast_io::little_endian(tmp);

        wasm_i32 const out{static_cast<wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least16_t>(tmp)))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused memory op with `local.get` address + `offset` immediate (`i64`) (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `native_memory_t*`, `wasm_u32 offset`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_load_localget_off(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_i64 = details::wasm_i64;
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
        wasm_i64 const out{details::load_i64_le(details::ptr_add_u64(memory.memory_begin, eff))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_store_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_store_localget_off(TypeRef & ... typeref) UWVM_THROWS
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        auto const v_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        wasm_i32 const v{details::memop::load_local<wasm_i32>(typeref...[2u], v_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_i32_le(details::ptr_add_u64(memory.memory_begin, eff), v);
    }

    /// @brief Fused memory op with `local.get` + immediate add + `offset` (`i32` store) (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`, `wasm_i32 imm`, `local_offset_t` (value), `native_memory_t*`, `wasm_u32 offset`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_store_local_plus_imm(TypeRef & ... typeref) UWVM_THROWS
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        wasm_i32 const imm{details::read_imm<wasm_i32>(typeref...[0])};
        auto const v_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const base{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        wasm_i32 const addr{numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, numeric_details::wasm_u32>(base, imm)};
        wasm_i32 const v{details::memop::load_local<wasm_i32>(typeref...[2u], v_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_i32_le(details::ptr_add_u64(memory.memory_begin, eff), v);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_store_imm_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_store_imm_localget_off(TypeRef & ... typeref) UWVM_THROWS
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        wasm_i32 const imm{details::read_imm<wasm_i32>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_i32_le(details::ptr_add_u64(memory.memory_begin, eff), imm);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_store8_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_store8_localget_off(TypeRef & ... typeref) UWVM_THROWS
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        auto const v_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        wasm_i32 const v{details::memop::load_local<wasm_i32>(typeref...[2u], v_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 1uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_u8(details::ptr_add_u64(memory.memory_begin, eff), static_cast<::std::uint_least8_t>(v));
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_store8_imm_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_store8_imm_localget_off(TypeRef & ... typeref) UWVM_THROWS
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        wasm_i32 const imm{details::read_imm<wasm_i32>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 1uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_u8(details::ptr_add_u64(memory.memory_begin, eff), static_cast<::std::uint_least8_t>(imm));
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_store16_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_store16_localget_off(TypeRef & ... typeref) UWVM_THROWS
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        auto const v_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        wasm_i32 const v{details::memop::load_local<wasm_i32>(typeref...[2u], v_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 2uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_u16_le(details::ptr_add_u64(memory.memory_begin, eff), static_cast<::std::uint_least16_t>(v));
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_store16_imm_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_store16_imm_localget_off(TypeRef & ... typeref) UWVM_THROWS
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        wasm_i32 const imm{details::read_imm<wasm_i32>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 2uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_u16_le(details::ptr_add_u64(memory.memory_begin, eff), static_cast<::std::uint_least16_t>(imm));
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i64_store_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_store_localget_off(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_i64 = details::wasm_i64;
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
        wasm_i64 const v{details::memop::load_local<wasm_i64>(typeref...[2u], v_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 8uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_i64_le(details::ptr_add_u64(memory.memory_begin, eff), v);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i64_store32_localget_off` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_store32_localget_off(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_i64 = details::wasm_i64;
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
        wasm_i64 const v{details::memop::load_local<wasm_i64>(typeref...[2u], v_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        details::store_u32_le(details::ptr_add_u64(memory.memory_begin, eff), static_cast<::std::uint_least32_t>(v));
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load_localget_set_local` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_load_localget_set_local(TypeRef & ... typeref) UWVM_THROWS
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
        wasm_i32 const out{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};
        details::memop::store_local(typeref...[2u], dst_off, out);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load8_u_localget_set_local` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_load8_u_localget_set_local(TypeRef & ... typeref) UWVM_THROWS
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

        auto const p_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        auto const dst_off{details::read_imm<details::memop::local_offset_t>(typeref...[0])};
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 1uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        ::std::uint_least8_t b{details::load_u8(details::ptr_add_u64(memory.memory_begin, eff))};
        wasm_i32 const out{static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(b))};
        details::memop::store_local(typeref...[2u], dst_off, out);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i64_load_localget_set_local` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_load_localget_set_local(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_i64 = details::wasm_i64;
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
        wasm_i64 const out{details::load_i64_le(details::ptr_add_u64(memory.memory_begin, eff))};
        details::memop::store_local(typeref...[2u], dst_off, out);
    }

    /// @brief Fused memory load + `local.tee` (`i32`) (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (addr), `local_offset_t` (dst), `native_memory_t*`, `wasm_u32 offset`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_load_localget_tee_local(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
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
        wasm_i32 const out{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};
        details::memop::store_local(typeref...[2u], dst_off, out);

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load8_u_localget_tee_local` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_load8_u_localget_tee_local(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
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
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 1uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        ::std::uint_least8_t b{details::load_u8(details::ptr_add_u64(memory.memory_begin, eff))};
        wasm_i32 const out{static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(b))};
        details::memop::store_local(typeref...[2u], dst_off, out);

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load8_s_localget_tee_local` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_load8_s_localget_tee_local(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
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
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 1uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        ::std::uint_least8_t b{details::load_u8(details::ptr_add_u64(memory.memory_begin, eff))};
        wasm_i32 const out{static_cast<wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least8_t>(b)))};
        details::memop::store_local(typeref...[2u], dst_off, out);

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused memory load + `local.tee` (`i64`) (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t` (addr), `local_offset_t` (dst), `native_memory_t*`, `wasm_u32 offset`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_load_localget_tee_local(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_i64 = details::wasm_i64;
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
        wasm_i64 const out{details::load_i64_le(details::ptr_add_u64(memory.memory_begin, eff))};
        details::memop::store_local(typeref...[2u], dst_off, out);

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused `local.get` + `i32.memcpy.localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_memcpy_localget_localget(TypeRef & ... typeref) UWVM_THROWS
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

        wasm_i32 const tmp{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, src_eff))};
        details::store_i32_le(details::ptr_add_u64(memory.memory_begin, dst_eff), tmp);
    }

    /// @brief Fused `local.get` + `i64.memcpy.localget` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: `local_offset_t`.

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_memcpy_localget_localget(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
        using wasm_i64 = details::wasm_i64;
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

        wasm_i64 const tmp{details::load_i64_le(details::ptr_add_u64(memory.memory_begin, src_eff))};
        details::store_i64_le(details::ptr_add_u64(memory.memory_begin, dst_eff), tmp);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load_add_imm` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_load_add_imm(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
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
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_i32 const imm{details::read_imm<wasm_i32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        wasm_i32 const loaded{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};

        ::std::uint_least32_t const lu{::std::bit_cast<::std::uint_least32_t>(loaded)};
        ::std::uint_least32_t const iu{::std::bit_cast<::std::uint_least32_t>(imm)};
        wasm_i32 const out{::std::bit_cast<wasm_i32>(static_cast<::std::uint_least32_t>(lu + iu))};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Fused combined opcode entrypoint `uwvmint_i32_load_and_imm` (byref).
    /// @details
    /// - Stack-top optimization: N/A in byref mode.
    /// - `type[0]` layout: see @ref uwvmint_conbine_byref_layout.
    /// - Immediates: see implementation (bytecode order).

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_load_and_imm(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = details::wasm_i32;
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
        native_memory_t* memory_p{details::read_imm<native_memory_t*>(typeref...[0])};
        wasm_u32 const offset{details::read_imm<wasm_u32>(typeref...[0])};
        wasm_i32 const imm{details::read_imm<wasm_i32>(typeref...[0])};

        wasm_i32 const addr{details::memop::load_local<wasm_i32>(typeref...[2u], p_off)};
        auto const eff65{details::wasm32_effective_offset(addr, offset)};

        auto const& memory{*memory_p};
        [[maybe_unused]] auto guard{details::lock_memory(memory)};
        details::check_memory_bounds_unlocked(memory, 0uz, static_cast<::std::uint_least64_t>(offset), eff65, 4uz);

        ::std::size_t const eff{static_cast<::std::size_t>(eff65.offset)};
        wasm_i32 const loaded{details::load_i32_le(details::ptr_add_u64(memory.memory_begin, eff))};

        wasm_i32 const out{static_cast<wasm_i32>(static_cast<::std::uint_least32_t>(loaded) & static_cast<::std::uint_least32_t>(imm))};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    // -------------------------------------
    // translate: fused memory opcode fptrs
    // -------------------------------------

    /**
     * @brief Translation-time selectors for fused memory opcodes.
     *
     * @details
     * These selectors choose the correct implementation based on:
     * - Stack-top caching position (when enabled).
     * - Memory/bounds-check strategy (e.g. generic vs specialized bounds checking) when a memory instance is provided.
     *
     * Like the arithmetic fusion selectors above, these return direct implementation entrypoints (usually `optable::details::memop::*`) to keep dispatch clean.
     */
    namespace translate
    {
        namespace details
        {
            namespace op_details = ::uwvm2::runtime::compiler::uwvm_int::optable::details;

            struct i32_load_localget_off_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_localget_off<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct i32_load_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_localget_off<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct i32_load_localget2_off_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_localget2_off<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct i32_load_localget2_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_localget2_off<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct i32_load_local_plus_imm_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_local_plus_imm<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct i32_load_local_plus_imm_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_local_plus_imm<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct i32_store_local_plus_imm_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_store_local_plus_imm<BoundsCheckFn, CompileOption, Type...>; }
            };

            struct i32_load8_u_localget_off_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load8_localget_off<op_details::bounds_check_generic, false, CompileOption, Pos, Type...>; }
            };

            struct i32_load8_u_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load8_localget_off<BoundsCheckFn, false, CompileOption, Pos, Type...>; }
            };

            struct i32_load8_s_localget_off_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load8_localget_off<op_details::bounds_check_generic, true, CompileOption, Pos, Type...>; }
            };

            struct i32_load8_s_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load8_localget_off<BoundsCheckFn, true, CompileOption, Pos, Type...>; }
            };

            struct i32_load16_u_localget_off_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load16_localget_off<op_details::bounds_check_generic, false, CompileOption, Pos, Type...>; }
            };

            struct i32_load16_u_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load16_localget_off<BoundsCheckFn, false, CompileOption, Pos, Type...>; }
            };

            struct i32_load16_s_localget_off_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load16_localget_off<op_details::bounds_check_generic, true, CompileOption, Pos, Type...>; }
            };

            struct i32_load16_s_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load16_localget_off<BoundsCheckFn, true, CompileOption, Pos, Type...>; }
            };

            struct i64_load_localget_off_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i64_load_localget_off<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct i64_load_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i64_load_localget_off<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct i32_store_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::i32_store_localget_off<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct i32_store_imm_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::i32_store_imm_localget_off<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct i32_store8_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::i32_store8_localget_off<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct i32_store8_imm_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::i32_store8_imm_localget_off<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct i32_store16_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::i32_store16_localget_off<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct i32_store16_imm_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::i32_store16_imm_localget_off<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct i64_store_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::i64_store_localget_off<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct i64_store32_localget_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::i64_store32_localget_off<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct i32_load_localget_set_local_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::i32_load_localget_set_local<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct i32_load8_u_localget_set_local_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::i32_load8_u_localget_set_local<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct i64_load_localget_set_local_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::i64_load_localget_set_local<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct i32_load_localget_tee_local_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_localget_tee_local<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct i32_load_localget_tee_local_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_localget_tee_local<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct i32_load8_u_localget_tee_local_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load8_u_localget_tee_local<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct i32_load8_u_localget_tee_local_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load8_u_localget_tee_local<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct i32_load8_s_localget_tee_local_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load8_s_localget_tee_local<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct i32_load8_s_localget_tee_local_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load8_s_localget_tee_local<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct i64_load_localget_tee_local_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i64_load_localget_tee_local<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct i64_load_localget_tee_local_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i64_load_localget_tee_local<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct i32_memcpy_localget_localget_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::i32_memcpy_localget_localget<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct i64_memcpy_localget_localget_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    (void)Pos;
                    return op_details::memop::i64_memcpy_localget_localget<BoundsCheckFn, CompileOption, Type...>;
                }
            };

            struct i32_load_add_imm_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_add_imm<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct i32_load_add_imm_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_add_imm<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct i32_load_add_imm_localget2_off_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_add_imm_localget2_off<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct i32_load_add_imm_localget2_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_add_imm_localget2_off<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct i32_load_and_imm_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_and_imm<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct i32_load_and_imm_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_and_imm<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };

            struct i32_load_and_imm_localget2_off_op
            {
                template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_and_imm_localget2_off<op_details::bounds_check_generic, CompileOption, Pos, Type...>; }
            };

            struct i32_load_and_imm_localget2_off_op_with
            {
                template <auto BoundsCheckFn, auto Extra, uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return op_details::memop::i32_load_and_imm_localget2_off<BoundsCheckFn, CompileOption, Pos, Type...>; }
            };
        }  // namespace details

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                                   details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_load_localget_off_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_load_local_plus_imm_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                     details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_load_local_plus_imm_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load_add_imm_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                              details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_load_add_imm_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load_and_imm_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                              details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_load_and_imm_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load_localget_off_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                details::op_details::native_memory_t const& memory,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load_localget_off_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load_local_plus_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                  details::op_details::native_memory_t const& memory,
                                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load_local_plus_imm_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load_add_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                           details::op_details::native_memory_t const& memory,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load_add_imm_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load_and_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                           details::op_details::native_memory_t const& memory,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load_and_imm_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        // Forward declare to satisfy two-phase lookup in the `_from_tuple` selector.
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_store_local_plus_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&, details::op_details::native_memory_t const& memory) noexcept;

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store_local_plus_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                   details::op_details::native_memory_t const& memory,
                                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store_local_plus_imm_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_load8_u_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                      details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_load8_u_localget_off_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_load8_s_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                      details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_load8_s_localget_off_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_load16_u_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                       details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_load16_u_localget_off_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_load16_s_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                       details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i32_stack_top_begin_pos,
                                                       CompileOption.i32_stack_top_end_pos,
                                                       details::i32_load16_s_localget_off_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i32_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load8_u_localget_off_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                   details::op_details::native_memory_t const& memory,
                                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load8_u_localget_off_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load8_s_localget_off_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                   details::op_details::native_memory_t const& memory,
                                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load8_s_localget_off_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load16_u_localget_off_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                    details::op_details::native_memory_t const& memory,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load16_u_localget_off_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_load16_s_localget_off_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                    details::op_details::native_memory_t const& memory,
                                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_load16_s_localget_off_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_load_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                                   details::op_details::native_memory_t const& memory) noexcept
        {
            return details::select_mem_fptr_or_default<CompileOption,
                                                       CompileOption.i64_stack_top_begin_pos,
                                                       CompileOption.i64_stack_top_end_pos,
                                                       details::i64_load_localget_off_op_with,
                                                       0uz,
                                                       Type...>(curr_stacktop.i64_stack_top_curr_pos, memory);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_store_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const&,
                                                                                                    details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::i32_store_localget_off_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_store_local_plus_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&, details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::i32_store_local_plus_imm_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_store_imm_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const&, details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::i32_store_imm_localget_off_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_store8_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const&, details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::i32_store8_localget_off_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_store8_imm_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const&,
                                                         details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::i32_store8_imm_localget_off_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_store16_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const&, details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::i32_store16_localget_off_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_store16_imm_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const&,
                                                          details::op_details::native_memory_t const& memory) noexcept
        { return details::select_mem_fptr_or_default<CompileOption, 0uz, 0uz, details::i32_store16_imm_localget_off_op_with, 0uz, Type...>(0uz, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store_localget_off_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                 details::op_details::native_memory_t const& memory,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store_localget_off_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store_imm_localget_off_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                     details::op_details::native_memory_t const& memory,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store_imm_localget_off_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store8_localget_off_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                  details::op_details::native_memory_t const& memory,
                                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store8_localget_off_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store8_imm_localget_off_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                      details::op_details::native_memory_t const& memory,
                                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store8_imm_localget_off_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store16_localget_off_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                   details::op_details::native_memory_t const& memory,
                                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store16_localget_off_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store16_imm_localget_off_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                       details::op_details::native_memory_t const& memory,
                                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store16_imm_localget_off_fptr<CompileOption, TypeInTuple...>(curr_stacktop, memory); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_load_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_conbine_impl<CompileOption,
                                                                             CompileOption.i32_stack_top_begin_pos,
                                                                             CompileOption.i32_stack_top_end_pos,
                                                                             details::i32_load_localget_off_op,
                                                                             Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return details::i32_load_localget_off_op::template fptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_load_local_plus_imm_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_conbine_impl<CompileOption,
                                                                             CompileOption.i32_stack_top_begin_pos,
                                                                             CompileOption.i32_stack_top_end_pos,
                                                                             details::i32_load_local_plus_imm_op,
                                                                             Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return details::i32_load_local_plus_imm_op::template fptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load_add_imm_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_conbine_impl<CompileOption,
                                                                             CompileOption.i32_stack_top_begin_pos,
                                                                             CompileOption.i32_stack_top_end_pos,
                                                                             details::i32_load_add_imm_op,
                                                                             Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return details::i32_load_add_imm_op::template fptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_load_and_imm_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_conbine_impl<CompileOption,
                                                                             CompileOption.i32_stack_top_begin_pos,
                                                                             CompileOption.i32_stack_top_end_pos,
                                                                             details::i32_load_and_imm_op,
                                                                             Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return details::i32_load_and_imm_op::template fptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_load8_u_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_conbine_impl<CompileOption,
                                                                             CompileOption.i32_stack_top_begin_pos,
                                                                             CompileOption.i32_stack_top_end_pos,
                                                                             details::i32_load8_u_localget_off_op,
                                                                             Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return details::i32_load8_u_localget_off_op::template fptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i64_load_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_conbine_impl<CompileOption,
                                                                             CompileOption.i64_stack_top_begin_pos,
                                                                             CompileOption.i64_stack_top_end_pos,
                                                                             details::i64_load_localget_off_op,
                                                                             Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return details::i64_load_localget_off_op::template fptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_load_localget_off_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_load_localget_off<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_load_local_plus_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_load_local_plus_imm<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_load_add_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_load_add_imm<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_load_and_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_load_and_imm<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_store_local_plus_imm_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_store_local_plus_imm<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_store_local_plus_imm_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_store_local_plus_imm_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate
#endif
}  // namespace uwvm2::runtime::compiler::uwvm_int::optable

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
#endif
