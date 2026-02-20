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
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
// import
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include "define.h"
# include "register_ring.h"
# include "numeric.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT)
    /**
     * @brief "Delay-local" fused opcode implementations.
     *
     * @details
     * This header provides opfunc variants that treat a `local.get` as a virtual operand source and
     * directly consume the local slot at the first real use site (typically a binary op).
     *
     * This is different from adjacent "combine" patterns (e.g. `local.get; local.get; i32.add`) because:
     * - No stack height change is materialized for the `local.get`.
     * - The fused opfunc performs the operation in-place on the current stack-top value and leaves
     *   the stack-top position unchanged (net effect = 0 for `local.get; binop`).
     *
     * Tail-call layout (conceptual):
     * - `[opfunc_ptr][local_offset:std::size_t][next_opfunc_ptr]`
     */

    namespace delay_local_details
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_u64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        using local_offset_t = ::std::size_t;

        template <typename T>
        UWVM_ALWAYS_INLINE inline constexpr T read_imm(::std::byte const*& ip) noexcept
        {
            static_assert(::std::is_trivially_copyable_v<T>);
            T v;  // no init
            ::std::memcpy(::std::addressof(v), ip, sizeof(v));
            ip += sizeof(v);
            return v;
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t curr_i32_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop_localget_rhs(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const off{read_imm<local_offset_t>(type...[0])};

            wasm_i32 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), type...[2u] + off, sizeof(rhs));

            // This opfunc represents: `local.get rhs; <binop>`.
            // Net stack effect is 0 relative to the pre-`local.get` state, so:
            // - do not pop/push the operand stack pointer,
            // - do not change the stack-top position.
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                static_assert(CompileOption.i32_stack_top_begin_pos <= curr_i32_stack_top && curr_i32_stack_top < CompileOption.i32_stack_top_end_pos);

                wasm_i32 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
                wasm_i32 out{};  // init
                if constexpr(Op == numeric_details::int_binop::div_s)
                {
                    if(rhs == wasm_i32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    if(lhs == ::std::numeric_limits<wasm_i32>::min() && rhs == wasm_i32{-1}) [[unlikely]]
                    {
                        UWVM_MUSTTAIL return numeric_details::trap_integer_overflow_tail<CompileOption>(type...);
                    }
                    out = static_cast<wasm_i32>(lhs / rhs);
                }
                else if constexpr(Op == numeric_details::int_binop::div_u)
                {
                    wasm_u32 const urhs{numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(rhs)};
                    if(urhs == wasm_u32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    wasm_u32 const out_u32{static_cast<wasm_u32>(numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(lhs) / urhs)};
                    out = numeric_details::from_unsigned_bits<wasm_i32, wasm_u32>(out_u32);
                }
                else if constexpr(Op == numeric_details::int_binop::rem_s)
                {
                    if(rhs == wasm_i32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    if(lhs == ::std::numeric_limits<wasm_i32>::min() && rhs == wasm_i32{-1}) { out = wasm_i32{0}; }
                    else
                    {
                        out = static_cast<wasm_i32>(lhs % rhs);
                    }
                }
                else if constexpr(Op == numeric_details::int_binop::rem_u)
                {
                    wasm_u32 const urhs{numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(rhs)};
                    if(urhs == wasm_u32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    wasm_u32 const out_u32{static_cast<wasm_u32>(numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(lhs) % urhs)};
                    out = numeric_details::from_unsigned_bits<wasm_i32, wasm_u32>(out_u32);
                }
                else
                {
                    out = numeric_details::eval_int_binop<Op, wasm_i32, wasm_u32>(lhs, rhs);
                }
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);
            }
            else
            {
                wasm_i32 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
                wasm_i32 out{};  // init
                if constexpr(Op == numeric_details::int_binop::div_s)
                {
                    if(rhs == wasm_i32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    if(lhs == ::std::numeric_limits<wasm_i32>::min() && rhs == wasm_i32{-1}) [[unlikely]]
                    {
                        UWVM_MUSTTAIL return numeric_details::trap_integer_overflow_tail<CompileOption>(type...);
                    }
                    out = static_cast<wasm_i32>(lhs / rhs);
                }
                else if constexpr(Op == numeric_details::int_binop::div_u)
                {
                    wasm_u32 const urhs{numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(rhs)};
                    if(urhs == wasm_u32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    wasm_u32 const out_u32{static_cast<wasm_u32>(numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(lhs) / urhs)};
                    out = numeric_details::from_unsigned_bits<wasm_i32, wasm_u32>(out_u32);
                }
                else if constexpr(Op == numeric_details::int_binop::rem_s)
                {
                    if(rhs == wasm_i32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    if(lhs == ::std::numeric_limits<wasm_i32>::min() && rhs == wasm_i32{-1}) { out = wasm_i32{0}; }
                    else
                    {
                        out = static_cast<wasm_i32>(lhs % rhs);
                    }
                }
                else if constexpr(Op == numeric_details::int_binop::rem_u)
                {
                    wasm_u32 const urhs{numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(rhs)};
                    if(urhs == wasm_u32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    wasm_u32 const out_u32{static_cast<wasm_u32>(numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(lhs) % urhs)};
                    out = numeric_details::from_unsigned_bits<wasm_i32, wasm_u32>(out_u32);
                }
                else
                {
                    out = numeric_details::eval_int_binop<Op, wasm_i32, wasm_u32>(lhs, rhs);
                }
                set_curr_val_to_operand_stack_cache_top(out, type...);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop_localget_rhs(TypeRef&... typeref) UWVM_THROWS
        {
            using wasm_i32 = delay_local_details::wasm_i32;
            using wasm_u32 = delay_local_details::wasm_u32;

            static_assert(sizeof...(TypeRef) >= 3uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

            // Byref mode always has stack-top caching disabled for all types.
            static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const off{read_imm<local_offset_t>(typeref...[0])};

            wasm_i32 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), typeref...[2u] + off, sizeof(rhs));

            wasm_i32 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
            wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, wasm_u32>(lhs, rhs)};
            set_curr_val_to_operand_stack_cache_top(out, typeref...);
        }

        /**
         * @brief Heavy-delay-local dynamic i32 binop.
         *
         * @details
         * This opfunc reads the binop kind from the bytecode stream to avoid instantiating one opfunc per `(Op × stacktop-pos)`
         * in delay-local heavy mode. It is intended for heavy-only ops; soft keeps specialized fastpaths.
         *
         * Tail-call bytecode layout:
         * - `[opfunc_ptr][rhs_off:local_offset_t][op:u64(low8)][next_opfunc_ptr]`
         */
        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop_localget_rhs_dyn(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const off{read_imm<local_offset_t>(type...[0])};
            ::std::uint_least64_t const op_raw{read_imm<::std::uint_least64_t>(type...[0])};
            auto const op{static_cast<numeric_details::int_binop>(static_cast<::std::uint_least8_t>(op_raw))};

            wasm_i32 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), type...[2u] + off, sizeof(rhs));

            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                static_assert(CompileOption.i32_stack_top_begin_pos <= curr_i32_stack_top && curr_i32_stack_top < CompileOption.i32_stack_top_end_pos);

                wasm_i32 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
                wasm_i32 out{};  // init
                switch(op)
                {
                    case numeric_details::int_binop::add:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::sub:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::sub, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::mul:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::mul, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::and_:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::and_, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::or_:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::or_, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::xor_:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::xor_, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::shl:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::shl, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::shr_s:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::shr_s, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::shr_u:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::shr_u, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::rotl:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::rotl, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::rotr:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::rotr, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::div_s:
                        if(rhs == wasm_i32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        if(lhs == ::std::numeric_limits<wasm_i32>::min() && rhs == wasm_i32{-1}) [[unlikely]]
                        {
                            UWVM_MUSTTAIL return numeric_details::trap_integer_overflow_tail<CompileOption>(type...);
                        }
                        out = static_cast<wasm_i32>(lhs / rhs);
                        break;
                    case numeric_details::int_binop::div_u:
                    {
                        wasm_u32 const urhs{numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(rhs)};
                        if(urhs == wasm_u32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        wasm_u32 const out_u32{static_cast<wasm_u32>(numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(lhs) / urhs)};
                        out = numeric_details::from_unsigned_bits<wasm_i32, wasm_u32>(out_u32);
                        break;
                    }
                    case numeric_details::int_binop::rem_s:
                        if(rhs == wasm_i32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        if(lhs == ::std::numeric_limits<wasm_i32>::min() && rhs == wasm_i32{-1}) { out = wasm_i32{0}; }
                        else
                        {
                            out = static_cast<wasm_i32>(lhs % rhs);
                        }
                        break;
                    case numeric_details::int_binop::rem_u:
                    {
                        wasm_u32 const urhs{numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(rhs)};
                        if(urhs == wasm_u32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        wasm_u32 const out_u32{static_cast<wasm_u32>(numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(lhs) % urhs)};
                        out = numeric_details::from_unsigned_bits<wasm_i32, wasm_u32>(out_u32);
                        break;
                    }
                    [[unlikely]] default:
                        ::fast_io::fast_terminate();
                }
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);
            }
            else
            {
                wasm_i32 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
                wasm_i32 out{};  // init
                switch(op)
                {
                    case numeric_details::int_binop::add:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::sub:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::sub, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::mul:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::mul, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::and_:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::and_, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::or_:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::or_, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::xor_:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::xor_, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::shl:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::shl, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::shr_s:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::shr_s, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::shr_u:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::shr_u, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::rotl:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::rotl, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::rotr:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::rotr, wasm_i32, wasm_u32>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::div_s:
                        if(rhs == wasm_i32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        if(lhs == ::std::numeric_limits<wasm_i32>::min() && rhs == wasm_i32{-1}) [[unlikely]]
                        {
                            UWVM_MUSTTAIL return numeric_details::trap_integer_overflow_tail<CompileOption>(type...);
                        }
                        out = static_cast<wasm_i32>(lhs / rhs);
                        break;
                    case numeric_details::int_binop::div_u:
                    {
                        wasm_u32 const urhs{numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(rhs)};
                        if(urhs == wasm_u32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        wasm_u32 const out_u32{static_cast<wasm_u32>(numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(lhs) / urhs)};
                        out = numeric_details::from_unsigned_bits<wasm_i32, wasm_u32>(out_u32);
                        break;
                    }
                    case numeric_details::int_binop::rem_s:
                        if(rhs == wasm_i32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        if(lhs == ::std::numeric_limits<wasm_i32>::min() && rhs == wasm_i32{-1}) { out = wasm_i32{0}; }
                        else
                        {
                            out = static_cast<wasm_i32>(lhs % rhs);
                        }
                        break;
                    case numeric_details::int_binop::rem_u:
                    {
                        wasm_u32 const urhs{numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(rhs)};
                        if(urhs == wasm_u32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        wasm_u32 const out_u32{static_cast<wasm_u32>(numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(lhs) % urhs)};
                        out = numeric_details::from_unsigned_bits<wasm_i32, wasm_u32>(out_u32);
                        break;
                    }
                    [[unlikely]] default:
                        ::fast_io::fast_terminate();
                }
                set_curr_val_to_operand_stack_cache_top(out, type...);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop_localget_rhs_dyn(TypeRef&... typeref) UWVM_THROWS
        {
            using wasm_i32 = delay_local_details::wasm_i32;
            using wasm_u32 = delay_local_details::wasm_u32;

            static_assert(sizeof...(TypeRef) >= 3uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

            // Byref mode always has stack-top caching disabled for all types.
            static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const off{read_imm<local_offset_t>(typeref...[0])};
            ::std::uint_least64_t const op_raw{read_imm<::std::uint_least64_t>(typeref...[0])};
            auto const op{static_cast<numeric_details::int_binop>(static_cast<::std::uint_least8_t>(op_raw))};

            wasm_i32 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), typeref...[2u] + off, sizeof(rhs));

            wasm_i32 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
            wasm_i32 out{};  // init

            switch(op)
            {
                case numeric_details::int_binop::add:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i32, wasm_u32>(lhs, rhs);
                    break;
                case numeric_details::int_binop::sub:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::sub, wasm_i32, wasm_u32>(lhs, rhs);
                    break;
                case numeric_details::int_binop::mul:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::mul, wasm_i32, wasm_u32>(lhs, rhs);
                    break;
                case numeric_details::int_binop::and_:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::and_, wasm_i32, wasm_u32>(lhs, rhs);
                    break;
                case numeric_details::int_binop::or_:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::or_, wasm_i32, wasm_u32>(lhs, rhs);
                    break;
                case numeric_details::int_binop::xor_:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::xor_, wasm_i32, wasm_u32>(lhs, rhs);
                    break;
                case numeric_details::int_binop::shl:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::shl, wasm_i32, wasm_u32>(lhs, rhs);
                    break;
                case numeric_details::int_binop::shr_s:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::shr_s, wasm_i32, wasm_u32>(lhs, rhs);
                    break;
                case numeric_details::int_binop::shr_u:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::shr_u, wasm_i32, wasm_u32>(lhs, rhs);
                    break;
                case numeric_details::int_binop::rotl:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::rotl, wasm_i32, wasm_u32>(lhs, rhs);
                    break;
                case numeric_details::int_binop::rotr:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::rotr, wasm_i32, wasm_u32>(lhs, rhs);
                    break;
                case numeric_details::int_binop::div_s:
                    if(rhs == wasm_i32{0}) [[unlikely]] { numeric_details::trap_integer_divide_by_zero(); }
                    if(lhs == ::std::numeric_limits<wasm_i32>::min() && rhs == wasm_i32{-1}) [[unlikely]] { numeric_details::trap_integer_overflow(); }
                    out = static_cast<wasm_i32>(lhs / rhs);
                    break;
                case numeric_details::int_binop::div_u:
                {
                    wasm_u32 const urhs{numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(rhs)};
                    if(urhs == wasm_u32{0}) [[unlikely]] { numeric_details::trap_integer_divide_by_zero(); }
                    wasm_u32 const out_u32{static_cast<wasm_u32>(numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(lhs) / urhs)};
                    out = numeric_details::from_unsigned_bits<wasm_i32, wasm_u32>(out_u32);
                    break;
                }
                case numeric_details::int_binop::rem_s:
                    if(rhs == wasm_i32{0}) [[unlikely]] { numeric_details::trap_integer_divide_by_zero(); }
                    if(lhs == ::std::numeric_limits<wasm_i32>::min() && rhs == wasm_i32{-1}) { out = wasm_i32{0}; }
                    else
                    {
                        out = static_cast<wasm_i32>(lhs % rhs);
                    }
                    break;
                case numeric_details::int_binop::rem_u:
                {
                    wasm_u32 const urhs{numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(rhs)};
                    if(urhs == wasm_u32{0}) [[unlikely]] { numeric_details::trap_integer_divide_by_zero(); }
                    wasm_u32 const out_u32{static_cast<wasm_u32>(numeric_details::to_unsigned_bits<wasm_i32, wasm_u32>(lhs) % urhs)};
                    out = numeric_details::from_unsigned_bits<wasm_i32, wasm_u32>(out_u32);
                    break;
                }
            }

            set_curr_val_to_operand_stack_cache_top(out, typeref...);
        }

        /**
         * @brief Delay-local + store fusions.
         *
         * @details
         * These opfuncs fuse: `local.get rhs; <binop>; local.set/tee dst`.
         *
         * Bytecode layout (tail-call):
         * - `[fused_opfunc_ptr][rhs_off:local_offset_t][orig_local_set/tee_ptr][dst_off:local_offset_t][next_ptr]`
         *
         * Bytecode layout (byref):
         * - `[fused_opfunc_byref_ptr][rhs_off:local_offset_t][orig_local_set/tee_ptr][dst_off:local_offset_t]`
         *
         * The fused opfunc skips the original `local.set`/`local.tee` opfunc pointer so the translator does not need
         * to change the emission order.
         */

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t curr_i32_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop_localget_rhs_local_set(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(type...[0])};

            // Skip the original `local.set` opfunc pointer.
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(type...[0])};

            wasm_i32 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), type...[2u] + rhs_off, sizeof(rhs));

            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                static_assert(CompileOption.i32_stack_top_begin_pos <= curr_i32_stack_top && curr_i32_stack_top < CompileOption.i32_stack_top_end_pos);

                wasm_i32 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
                wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, wasm_u32>(lhs, rhs)};
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));
            }
            else
            {
                wasm_i32 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
                wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, wasm_u32>(lhs, rhs)};
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));

                // `local.set` pops the top value. Since `local.get rhs` is not materialized, net effect is pop(1).
                type...[1u] -= sizeof(wasm_i32);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop_localget_rhs_local_set(TypeRef&... typeref) UWVM_THROWS
        {
            using wasm_i32 = delay_local_details::wasm_i32;
            using wasm_u32 = delay_local_details::wasm_u32;

            static_assert(sizeof...(TypeRef) >= 3uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

            // Byref mode always has stack-top caching disabled for all types.
            static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(typeref...[0])};

            // Skip the original `local.set` opfunc pointer.
            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(typeref...[0])};

            wasm_i32 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), typeref...[2u] + rhs_off, sizeof(rhs));

            wasm_i32 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
            wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, wasm_u32>(lhs, rhs)};
            ::std::memcpy(typeref...[2u] + dst_off, ::std::addressof(out), sizeof(out));

            typeref...[1u] -= sizeof(wasm_i32);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t curr_i32_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop_localget_rhs_local_tee(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(type...[0])};

            // Skip the original `local.tee` opfunc pointer.
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(type...[0])};

            wasm_i32 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), type...[2u] + rhs_off, sizeof(rhs));

            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                static_assert(CompileOption.i32_stack_top_begin_pos <= curr_i32_stack_top && curr_i32_stack_top < CompileOption.i32_stack_top_end_pos);

                wasm_i32 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
                wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, wasm_u32>(lhs, rhs)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_i32_stack_top>(out, type...);
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));
            }
            else
            {
                wasm_i32 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
                wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, wasm_u32>(lhs, rhs)};
                set_curr_val_to_operand_stack_cache_top(out, type...);
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop_localget_rhs_local_tee(TypeRef&... typeref) UWVM_THROWS
        {
            using wasm_i32 = delay_local_details::wasm_i32;
            using wasm_u32 = delay_local_details::wasm_u32;

            static_assert(sizeof...(TypeRef) >= 3uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

            // Byref mode always has stack-top caching disabled for all types.
            static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(typeref...[0])};

            // Skip the original `local.tee` opfunc pointer.
            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(typeref...[0])};

            wasm_i32 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), typeref...[2u] + rhs_off, sizeof(rhs));

            wasm_i32 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
            wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, wasm_u32>(lhs, rhs)};
            set_curr_val_to_operand_stack_cache_top(out, typeref...);
            ::std::memcpy(typeref...[2u] + dst_off, ::std::addressof(out), sizeof(out));
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t curr_i64_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop_localget_rhs(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const off{read_imm<local_offset_t>(type...[0])};

            wasm_i64 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), type...[2u] + off, sizeof(rhs));

            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                static_assert(CompileOption.i64_stack_top_begin_pos <= curr_i64_stack_top && curr_i64_stack_top < CompileOption.i64_stack_top_end_pos);

                wasm_i64 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...)};
                wasm_i64 out{};  // init
                if constexpr(Op == numeric_details::int_binop::div_s)
                {
                    if(rhs == wasm_i64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    if(lhs == ::std::numeric_limits<wasm_i64>::min() && rhs == wasm_i64{-1}) [[unlikely]]
                    {
                        UWVM_MUSTTAIL return numeric_details::trap_integer_overflow_tail<CompileOption>(type...);
                    }
                    out = static_cast<wasm_i64>(lhs / rhs);
                }
                else if constexpr(Op == numeric_details::int_binop::div_u)
                {
                    wasm_u64 const urhs{numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(rhs)};
                    if(urhs == wasm_u64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    wasm_u64 const out_u64{static_cast<wasm_u64>(numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(lhs) / urhs)};
                    out = numeric_details::from_unsigned_bits<wasm_i64, wasm_u64>(out_u64);
                }
                else if constexpr(Op == numeric_details::int_binop::rem_s)
                {
                    if(rhs == wasm_i64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    if(lhs == ::std::numeric_limits<wasm_i64>::min() && rhs == wasm_i64{-1}) { out = wasm_i64{0}; }
                    else
                    {
                        out = static_cast<wasm_i64>(lhs % rhs);
                    }
                }
                else if constexpr(Op == numeric_details::int_binop::rem_u)
                {
                    wasm_u64 const urhs{numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(rhs)};
                    if(urhs == wasm_u64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    wasm_u64 const out_u64{static_cast<wasm_u64>(numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(lhs) % urhs)};
                    out = numeric_details::from_unsigned_bits<wasm_i64, wasm_u64>(out_u64);
                }
                else
                {
                    out = numeric_details::eval_int_binop<Op, wasm_i64, wasm_u64>(lhs, rhs);
                }
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_i64_stack_top>(out, type...);
            }
            else
            {
                wasm_i64 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
                wasm_i64 out{};  // init
                if constexpr(Op == numeric_details::int_binop::div_s)
                {
                    if(rhs == wasm_i64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    if(lhs == ::std::numeric_limits<wasm_i64>::min() && rhs == wasm_i64{-1}) [[unlikely]]
                    {
                        UWVM_MUSTTAIL return numeric_details::trap_integer_overflow_tail<CompileOption>(type...);
                    }
                    out = static_cast<wasm_i64>(lhs / rhs);
                }
                else if constexpr(Op == numeric_details::int_binop::div_u)
                {
                    wasm_u64 const urhs{numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(rhs)};
                    if(urhs == wasm_u64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    wasm_u64 const out_u64{static_cast<wasm_u64>(numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(lhs) / urhs)};
                    out = numeric_details::from_unsigned_bits<wasm_i64, wasm_u64>(out_u64);
                }
                else if constexpr(Op == numeric_details::int_binop::rem_s)
                {
                    if(rhs == wasm_i64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    if(lhs == ::std::numeric_limits<wasm_i64>::min() && rhs == wasm_i64{-1}) { out = wasm_i64{0}; }
                    else
                    {
                        out = static_cast<wasm_i64>(lhs % rhs);
                    }
                }
                else if constexpr(Op == numeric_details::int_binop::rem_u)
                {
                    wasm_u64 const urhs{numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(rhs)};
                    if(urhs == wasm_u64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                    wasm_u64 const out_u64{static_cast<wasm_u64>(numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(lhs) % urhs)};
                    out = numeric_details::from_unsigned_bits<wasm_i64, wasm_u64>(out_u64);
                }
                else
                {
                    out = numeric_details::eval_int_binop<Op, wasm_i64, wasm_u64>(lhs, rhs);
                }
                set_curr_val_to_operand_stack_cache_top(out, type...);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop_localget_rhs(TypeRef&... typeref) UWVM_THROWS
        {
            using wasm_i64 = delay_local_details::wasm_i64;
            using wasm_u64 = delay_local_details::wasm_u64;

            static_assert(sizeof...(TypeRef) >= 3uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

            // Byref mode always has stack-top caching disabled for all types.
            static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const off{read_imm<local_offset_t>(typeref...[0])};

            wasm_i64 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), typeref...[2u] + off, sizeof(rhs));

            wasm_i64 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
            wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, wasm_u64>(lhs, rhs)};
            set_curr_val_to_operand_stack_cache_top(out, typeref...);
        }

        /**
         * @brief Heavy-delay-local dynamic i64 binop.
         *
         * Tail-call bytecode layout:
         * - `[opfunc_ptr][rhs_off:local_offset_t][op:u64(low8)][next_opfunc_ptr]`
         */
        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop_localget_rhs_dyn(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const off{read_imm<local_offset_t>(type...[0])};
            ::std::uint_least64_t const op_raw{read_imm<::std::uint_least64_t>(type...[0])};
            auto const op{static_cast<numeric_details::int_binop>(static_cast<::std::uint_least8_t>(op_raw))};

            wasm_i64 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), type...[2u] + off, sizeof(rhs));

            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                static_assert(CompileOption.i64_stack_top_begin_pos <= curr_i64_stack_top && curr_i64_stack_top < CompileOption.i64_stack_top_end_pos);

                wasm_i64 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...)};
                wasm_i64 out{};  // init
                switch(op)
                {
                    case numeric_details::int_binop::add:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::sub:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::sub, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::mul:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::mul, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::and_:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::and_, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::or_:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::or_, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::xor_:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::xor_, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::shl:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::shl, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::shr_s:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::shr_s, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::shr_u:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::shr_u, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::rotl:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::rotl, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::rotr:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::rotr, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::div_s:
                        if(rhs == wasm_i64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        if(lhs == ::std::numeric_limits<wasm_i64>::min() && rhs == wasm_i64{-1}) [[unlikely]]
                        {
                            UWVM_MUSTTAIL return numeric_details::trap_integer_overflow_tail<CompileOption>(type...);
                        }
                        out = static_cast<wasm_i64>(lhs / rhs);
                        break;
                    case numeric_details::int_binop::div_u:
                    {
                        wasm_u64 const urhs{numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(rhs)};
                        if(urhs == wasm_u64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        wasm_u64 const out_u64{static_cast<wasm_u64>(numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(lhs) / urhs)};
                        out = numeric_details::from_unsigned_bits<wasm_i64, wasm_u64>(out_u64);
                        break;
                    }
                    case numeric_details::int_binop::rem_s:
                        if(rhs == wasm_i64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        if(lhs == ::std::numeric_limits<wasm_i64>::min() && rhs == wasm_i64{-1}) { out = wasm_i64{0}; }
                        else
                        {
                            out = static_cast<wasm_i64>(lhs % rhs);
                        }
                        break;
                    case numeric_details::int_binop::rem_u:
                    {
                        wasm_u64 const urhs{numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(rhs)};
                        if(urhs == wasm_u64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        wasm_u64 const out_u64{static_cast<wasm_u64>(numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(lhs) % urhs)};
                        out = numeric_details::from_unsigned_bits<wasm_i64, wasm_u64>(out_u64);
                        break;
                    }
                    [[unlikely]] default:
                        ::fast_io::fast_terminate();
                }
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_i64_stack_top>(out, type...);
            }
            else
            {
                wasm_i64 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
                wasm_i64 out{};  // init
                switch(op)
                {
                    case numeric_details::int_binop::add:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::sub:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::sub, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::mul:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::mul, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::and_:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::and_, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::or_:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::or_, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::xor_:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::xor_, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::shl:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::shl, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::shr_s:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::shr_s, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::shr_u:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::shr_u, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::rotl:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::rotl, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::rotr:
                        out = numeric_details::eval_int_binop<numeric_details::int_binop::rotr, wasm_i64, wasm_u64>(lhs, rhs);
                        break;
                    case numeric_details::int_binop::div_s:
                        if(rhs == wasm_i64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        if(lhs == ::std::numeric_limits<wasm_i64>::min() && rhs == wasm_i64{-1}) [[unlikely]]
                        {
                            UWVM_MUSTTAIL return numeric_details::trap_integer_overflow_tail<CompileOption>(type...);
                        }
                        out = static_cast<wasm_i64>(lhs / rhs);
                        break;
                    case numeric_details::int_binop::div_u:
                    {
                        wasm_u64 const urhs{numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(rhs)};
                        if(urhs == wasm_u64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        wasm_u64 const out_u64{static_cast<wasm_u64>(numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(lhs) / urhs)};
                        out = numeric_details::from_unsigned_bits<wasm_i64, wasm_u64>(out_u64);
                        break;
                    }
                    case numeric_details::int_binop::rem_s:
                        if(rhs == wasm_i64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        if(lhs == ::std::numeric_limits<wasm_i64>::min() && rhs == wasm_i64{-1}) { out = wasm_i64{0}; }
                        else
                        {
                            out = static_cast<wasm_i64>(lhs % rhs);
                        }
                        break;
                    case numeric_details::int_binop::rem_u:
                    {
                        wasm_u64 const urhs{numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(rhs)};
                        if(urhs == wasm_u64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
                        wasm_u64 const out_u64{static_cast<wasm_u64>(numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(lhs) % urhs)};
                        out = numeric_details::from_unsigned_bits<wasm_i64, wasm_u64>(out_u64);
                        break;
                    }
                    [[unlikely]] default:
                        ::fast_io::fast_terminate();
                }
                set_curr_val_to_operand_stack_cache_top(out, type...);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop_localget_rhs_dyn(TypeRef&... typeref) UWVM_THROWS
        {
            using wasm_i64 = delay_local_details::wasm_i64;
            using wasm_u64 = delay_local_details::wasm_u64;

            static_assert(sizeof...(TypeRef) >= 3uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

            // Byref mode always has stack-top caching disabled for all types.
            static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const off{read_imm<local_offset_t>(typeref...[0])};
            ::std::uint_least64_t const op_raw{read_imm<::std::uint_least64_t>(typeref...[0])};
            auto const op{static_cast<numeric_details::int_binop>(static_cast<::std::uint_least8_t>(op_raw))};

            wasm_i64 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), typeref...[2u] + off, sizeof(rhs));

            wasm_i64 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
            wasm_i64 out{};  // init

            switch(op)
            {
                case numeric_details::int_binop::add:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::add, wasm_i64, wasm_u64>(lhs, rhs);
                    break;
                case numeric_details::int_binop::sub:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::sub, wasm_i64, wasm_u64>(lhs, rhs);
                    break;
                case numeric_details::int_binop::mul:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::mul, wasm_i64, wasm_u64>(lhs, rhs);
                    break;
                case numeric_details::int_binop::and_:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::and_, wasm_i64, wasm_u64>(lhs, rhs);
                    break;
                case numeric_details::int_binop::or_:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::or_, wasm_i64, wasm_u64>(lhs, rhs);
                    break;
                case numeric_details::int_binop::xor_:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::xor_, wasm_i64, wasm_u64>(lhs, rhs);
                    break;
                case numeric_details::int_binop::shl:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::shl, wasm_i64, wasm_u64>(lhs, rhs);
                    break;
                case numeric_details::int_binop::shr_s:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::shr_s, wasm_i64, wasm_u64>(lhs, rhs);
                    break;
                case numeric_details::int_binop::shr_u:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::shr_u, wasm_i64, wasm_u64>(lhs, rhs);
                    break;
                case numeric_details::int_binop::rotl:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::rotl, wasm_i64, wasm_u64>(lhs, rhs);
                    break;
                case numeric_details::int_binop::rotr:
                    out = numeric_details::eval_int_binop<numeric_details::int_binop::rotr, wasm_i64, wasm_u64>(lhs, rhs);
                    break;
                case numeric_details::int_binop::div_s:
                    if(rhs == wasm_i64{0}) [[unlikely]] { numeric_details::trap_integer_divide_by_zero(); }
                    if(lhs == ::std::numeric_limits<wasm_i64>::min() && rhs == wasm_i64{-1}) [[unlikely]] { numeric_details::trap_integer_overflow(); }
                    out = static_cast<wasm_i64>(lhs / rhs);
                    break;
                case numeric_details::int_binop::div_u:
                {
                    wasm_u64 const urhs{numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(rhs)};
                    if(urhs == wasm_u64{0}) [[unlikely]] { numeric_details::trap_integer_divide_by_zero(); }
                    wasm_u64 const out_u64{static_cast<wasm_u64>(numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(lhs) / urhs)};
                    out = numeric_details::from_unsigned_bits<wasm_i64, wasm_u64>(out_u64);
                    break;
                }
                case numeric_details::int_binop::rem_s:
                    if(rhs == wasm_i64{0}) [[unlikely]] { numeric_details::trap_integer_divide_by_zero(); }
                    if(lhs == ::std::numeric_limits<wasm_i64>::min() && rhs == wasm_i64{-1}) { out = wasm_i64{0}; }
                    else
                    {
                        out = static_cast<wasm_i64>(lhs % rhs);
                    }
                    break;
                case numeric_details::int_binop::rem_u:
                {
                    wasm_u64 const urhs{numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(rhs)};
                    if(urhs == wasm_u64{0}) [[unlikely]] { numeric_details::trap_integer_divide_by_zero(); }
                    wasm_u64 const out_u64{static_cast<wasm_u64>(numeric_details::to_unsigned_bits<wasm_i64, wasm_u64>(lhs) % urhs)};
                    out = numeric_details::from_unsigned_bits<wasm_i64, wasm_u64>(out_u64);
                    break;
                }
            }

            set_curr_val_to_operand_stack_cache_top(out, typeref...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t curr_i64_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop_localget_rhs_local_set(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(type...[0])};

            // Skip the original `local.set` opfunc pointer.
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(type...[0])};

            wasm_i64 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), type...[2u] + rhs_off, sizeof(rhs));

            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                static_assert(CompileOption.i64_stack_top_begin_pos <= curr_i64_stack_top && curr_i64_stack_top < CompileOption.i64_stack_top_end_pos);

                wasm_i64 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...)};
                wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, wasm_u64>(lhs, rhs)};
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));
            }
            else
            {
                wasm_i64 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
                wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, wasm_u64>(lhs, rhs)};
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));

                type...[1u] -= sizeof(wasm_i64);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop_localget_rhs_local_set(TypeRef&... typeref) UWVM_THROWS
        {
            using wasm_i64 = delay_local_details::wasm_i64;
            using wasm_u64 = delay_local_details::wasm_u64;

            static_assert(sizeof...(TypeRef) >= 3uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

            // Byref mode always has stack-top caching disabled for all types.
            static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(typeref...[0])};

            // Skip the original `local.set` opfunc pointer.
            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(typeref...[0])};

            wasm_i64 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), typeref...[2u] + rhs_off, sizeof(rhs));

            wasm_i64 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
            wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, wasm_u64>(lhs, rhs)};
            ::std::memcpy(typeref...[2u] + dst_off, ::std::addressof(out), sizeof(out));

            typeref...[1u] -= sizeof(wasm_i64);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t curr_i64_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop_localget_rhs_local_tee(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(type...[0])};

            // Skip the original `local.tee` opfunc pointer.
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(type...[0])};

            wasm_i64 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), type...[2u] + rhs_off, sizeof(rhs));

            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                static_assert(CompileOption.i64_stack_top_begin_pos <= curr_i64_stack_top && curr_i64_stack_top < CompileOption.i64_stack_top_end_pos);

                wasm_i64 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...)};
                wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, wasm_u64>(lhs, rhs)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_i64_stack_top>(out, type...);
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));
            }
            else
            {
                wasm_i64 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
                wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, wasm_u64>(lhs, rhs)};
                set_curr_val_to_operand_stack_cache_top(out, type...);
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop_localget_rhs_local_tee(TypeRef&... typeref) UWVM_THROWS
        {
            using wasm_i64 = delay_local_details::wasm_i64;
            using wasm_u64 = delay_local_details::wasm_u64;

            static_assert(sizeof...(TypeRef) >= 3uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

            // Byref mode always has stack-top caching disabled for all types.
            static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(typeref...[0])};

            // Skip the original `local.tee` opfunc pointer.
            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(typeref...[0])};

            wasm_i64 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), typeref...[2u] + rhs_off, sizeof(rhs));

            wasm_i64 const lhs{peek_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
            wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, wasm_u64>(lhs, rhs)};
            set_curr_val_to_operand_stack_cache_top(out, typeref...);
            ::std::memcpy(typeref...[2u] + dst_off, ::std::addressof(out), sizeof(out));
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::float_binop Op,
                  ::std::size_t curr_f32_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_binop_localget_rhs(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const off{read_imm<local_offset_t>(type...[0])};

            wasm_f32 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), type...[2u] + off, sizeof(rhs));

            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                static_assert(CompileOption.f32_stack_top_begin_pos <= curr_f32_stack_top && curr_f32_stack_top < CompileOption.f32_stack_top_end_pos);

                wasm_f32 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, curr_f32_stack_top>(type...)};
                wasm_f32 const out{numeric_details::eval_float_binop<Op, wasm_f32>(lhs, rhs)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, curr_f32_stack_top>(out, type...);
            }
            else
            {
                wasm_f32 const lhs{peek_curr_val_from_operand_stack_cache<wasm_f32>(type...)};
                wasm_f32 const out{numeric_details::eval_float_binop<Op, wasm_f32>(lhs, rhs)};
                set_curr_val_to_operand_stack_cache_top(out, type...);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_binop_localget_rhs(TypeRef&... typeref) UWVM_THROWS
        {
            using wasm_f32 = delay_local_details::wasm_f32;

            static_assert(sizeof...(TypeRef) >= 3uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

            // Byref mode always has stack-top caching disabled for all types.
            static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const off{read_imm<local_offset_t>(typeref...[0])};

            wasm_f32 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), typeref...[2u] + off, sizeof(rhs));

            wasm_f32 const lhs{peek_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
            wasm_f32 const out{numeric_details::eval_float_binop<Op, wasm_f32>(lhs, rhs)};
            set_curr_val_to_operand_stack_cache_top(out, typeref...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::float_binop Op,
                  ::std::size_t curr_f32_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_binop_localget_rhs_local_set(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(type...[0])};

            // Skip the original `local.set` opfunc pointer.
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(type...[0])};

            wasm_f32 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), type...[2u] + rhs_off, sizeof(rhs));

            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                static_assert(CompileOption.f32_stack_top_begin_pos <= curr_f32_stack_top && curr_f32_stack_top < CompileOption.f32_stack_top_end_pos);

                wasm_f32 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, curr_f32_stack_top>(type...)};
                wasm_f32 const out{numeric_details::eval_float_binop<Op, wasm_f32>(lhs, rhs)};
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));
            }
            else
            {
                wasm_f32 const lhs{peek_curr_val_from_operand_stack_cache<wasm_f32>(type...)};
                wasm_f32 const out{numeric_details::eval_float_binop<Op, wasm_f32>(lhs, rhs)};
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));

                type...[1u] -= sizeof(wasm_f32);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_binop_localget_rhs_local_set(TypeRef&... typeref) UWVM_THROWS
        {
            using wasm_f32 = delay_local_details::wasm_f32;

            static_assert(sizeof...(TypeRef) >= 3uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

            // Byref mode always has stack-top caching disabled for all types.
            static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(typeref...[0])};

            // Skip the original `local.set` opfunc pointer.
            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(typeref...[0])};

            wasm_f32 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), typeref...[2u] + rhs_off, sizeof(rhs));

            wasm_f32 const lhs{peek_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
            wasm_f32 const out{numeric_details::eval_float_binop<Op, wasm_f32>(lhs, rhs)};
            ::std::memcpy(typeref...[2u] + dst_off, ::std::addressof(out), sizeof(out));

            typeref...[1u] -= sizeof(wasm_f32);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::float_binop Op,
                  ::std::size_t curr_f32_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_binop_localget_rhs_local_tee(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(type...[0])};

            // Skip the original `local.tee` opfunc pointer.
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(type...[0])};

            wasm_f32 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), type...[2u] + rhs_off, sizeof(rhs));

            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                static_assert(CompileOption.f32_stack_top_begin_pos <= curr_f32_stack_top && curr_f32_stack_top < CompileOption.f32_stack_top_end_pos);

                wasm_f32 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, curr_f32_stack_top>(type...)};
                wasm_f32 const out{numeric_details::eval_float_binop<Op, wasm_f32>(lhs, rhs)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, curr_f32_stack_top>(out, type...);
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));
            }
            else
            {
                wasm_f32 const lhs{peek_curr_val_from_operand_stack_cache<wasm_f32>(type...)};
                wasm_f32 const out{numeric_details::eval_float_binop<Op, wasm_f32>(lhs, rhs)};
                set_curr_val_to_operand_stack_cache_top(out, type...);
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_binop_localget_rhs_local_tee(TypeRef&... typeref) UWVM_THROWS
        {
            using wasm_f32 = delay_local_details::wasm_f32;

            static_assert(sizeof...(TypeRef) >= 3uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

            // Byref mode always has stack-top caching disabled for all types.
            static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(typeref...[0])};

            // Skip the original `local.tee` opfunc pointer.
            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(typeref...[0])};

            wasm_f32 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), typeref...[2u] + rhs_off, sizeof(rhs));

            wasm_f32 const lhs{peek_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
            wasm_f32 const out{numeric_details::eval_float_binop<Op, wasm_f32>(lhs, rhs)};
            set_curr_val_to_operand_stack_cache_top(out, typeref...);
            ::std::memcpy(typeref...[2u] + dst_off, ::std::addressof(out), sizeof(out));
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::float_binop Op,
                  ::std::size_t curr_f64_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_binop_localget_rhs(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const off{read_imm<local_offset_t>(type...[0])};

            wasm_f64 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), type...[2u] + off, sizeof(rhs));

            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                static_assert(CompileOption.f64_stack_top_begin_pos <= curr_f64_stack_top && curr_f64_stack_top < CompileOption.f64_stack_top_end_pos);

                wasm_f64 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, curr_f64_stack_top>(type...)};
                wasm_f64 const out{numeric_details::eval_float_binop<Op, wasm_f64>(lhs, rhs)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, curr_f64_stack_top>(out, type...);
            }
            else
            {
                wasm_f64 const lhs{peek_curr_val_from_operand_stack_cache<wasm_f64>(type...)};
                wasm_f64 const out{numeric_details::eval_float_binop<Op, wasm_f64>(lhs, rhs)};
                set_curr_val_to_operand_stack_cache_top(out, type...);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_binop_localget_rhs(TypeRef&... typeref) UWVM_THROWS
        {
            using wasm_f64 = delay_local_details::wasm_f64;

            static_assert(sizeof...(TypeRef) >= 3uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

            // Byref mode always has stack-top caching disabled for all types.
            static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const off{read_imm<local_offset_t>(typeref...[0])};

            wasm_f64 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), typeref...[2u] + off, sizeof(rhs));

            wasm_f64 const lhs{peek_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
            wasm_f64 const out{numeric_details::eval_float_binop<Op, wasm_f64>(lhs, rhs)};
            set_curr_val_to_operand_stack_cache_top(out, typeref...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::float_binop Op,
                  ::std::size_t curr_f64_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_binop_localget_rhs_local_set(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(type...[0])};

            // Skip the original `local.set` opfunc pointer.
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(type...[0])};

            wasm_f64 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), type...[2u] + rhs_off, sizeof(rhs));

            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                static_assert(CompileOption.f64_stack_top_begin_pos <= curr_f64_stack_top && curr_f64_stack_top < CompileOption.f64_stack_top_end_pos);

                wasm_f64 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, curr_f64_stack_top>(type...)};
                wasm_f64 const out{numeric_details::eval_float_binop<Op, wasm_f64>(lhs, rhs)};
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));
            }
            else
            {
                wasm_f64 const lhs{peek_curr_val_from_operand_stack_cache<wasm_f64>(type...)};
                wasm_f64 const out{numeric_details::eval_float_binop<Op, wasm_f64>(lhs, rhs)};
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));

                type...[1u] -= sizeof(wasm_f64);
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_binop_localget_rhs_local_set(TypeRef&... typeref) UWVM_THROWS
        {
            using wasm_f64 = delay_local_details::wasm_f64;

            static_assert(sizeof...(TypeRef) >= 3uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

            // Byref mode always has stack-top caching disabled for all types.
            static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(typeref...[0])};

            // Skip the original `local.set` opfunc pointer.
            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(typeref...[0])};

            wasm_f64 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), typeref...[2u] + rhs_off, sizeof(rhs));

            wasm_f64 const lhs{peek_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
            wasm_f64 const out{numeric_details::eval_float_binop<Op, wasm_f64>(lhs, rhs)};
            ::std::memcpy(typeref...[2u] + dst_off, ::std::addressof(out), sizeof(out));

            typeref...[1u] -= sizeof(wasm_f64);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::float_binop Op,
                  ::std::size_t curr_f64_stack_top,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_binop_localget_rhs_local_tee(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(type...[0])};

            // Skip the original `local.tee` opfunc pointer.
            type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(type...[0])};

            wasm_f64 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), type...[2u] + rhs_off, sizeof(rhs));

            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                static_assert(CompileOption.f64_stack_top_begin_pos <= curr_f64_stack_top && curr_f64_stack_top < CompileOption.f64_stack_top_end_pos);

                wasm_f64 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, curr_f64_stack_top>(type...)};
                wasm_f64 const out{numeric_details::eval_float_binop<Op, wasm_f64>(lhs, rhs)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, curr_f64_stack_top>(out, type...);
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));
            }
            else
            {
                wasm_f64 const lhs{peek_curr_val_from_operand_stack_cache<wasm_f64>(type...)};
                wasm_f64 const out{numeric_details::eval_float_binop<Op, wasm_f64>(lhs, rhs)};
                set_curr_val_to_operand_stack_cache_top(out, type...);
                ::std::memcpy(type...[2u] + dst_off, ::std::addressof(out), sizeof(out));
            }

            uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
            ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
            UWVM_MUSTTAIL return next_interpreter(type...);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeRef>
            requires (!CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_binop_localget_rhs_local_tee(TypeRef&... typeref) UWVM_THROWS
        {
            using wasm_f64 = delay_local_details::wasm_f64;

            static_assert(sizeof...(TypeRef) >= 3uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
            static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[2u]>, ::std::byte*>);

            // Byref mode always has stack-top caching disabled for all types.
            static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const rhs_off{read_imm<local_offset_t>(typeref...[0])};

            // Skip the original `local.tee` opfunc pointer.
            typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

            local_offset_t const dst_off{read_imm<local_offset_t>(typeref...[0])};

            wasm_f64 rhs;  // no init
            ::std::memcpy(::std::addressof(rhs), typeref...[2u] + rhs_off, sizeof(rhs));

            wasm_f64 const lhs{peek_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
            wasm_f64 const out{numeric_details::eval_float_binop<Op, wasm_f64>(lhs, rhs)};
            set_curr_val_to_operand_stack_cache_top(out, typeref...);
            ::std::memcpy(typeref...[2u] + dst_off, ::std::addressof(out), sizeof(out));
        }

        namespace details
        {
            template <numeric_details::int_binop Op>
            struct i32_binop_localget_rhs_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_localget_rhs<Opt, Op, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_localget_rhs<Opt, Op, Type...>; }
            };

            struct i32_binop_localget_rhs_dyn_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_localget_rhs_dyn<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_localget_rhs_dyn<Opt, Type...>; }
            };

            template <numeric_details::int_binop Op>
            struct i32_binop_localget_rhs_local_set_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_localget_rhs_local_set<Opt, Op, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_localget_rhs_local_set<Opt, Op, Type...>; }
            };

            template <numeric_details::int_binop Op>
            struct i32_binop_localget_rhs_local_tee_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop_localget_rhs_local_tee<Opt, Op, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop_localget_rhs_local_tee<Opt, Op, Type...>; }
            };

            template <numeric_details::int_binop Op>
            struct i64_binop_localget_rhs_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop_localget_rhs<Opt, Op, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop_localget_rhs<Opt, Op, Type...>; }
            };

            struct i64_binop_localget_rhs_dyn_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop_localget_rhs_dyn<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop_localget_rhs_dyn<Opt, Type...>; }
            };

            template <numeric_details::int_binop Op>
            struct i64_binop_localget_rhs_local_set_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop_localget_rhs_local_set<Opt, Op, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop_localget_rhs_local_set<Opt, Op, Type...>; }
            };

            template <numeric_details::int_binop Op>
            struct i64_binop_localget_rhs_local_tee_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop_localget_rhs_local_tee<Opt, Op, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop_localget_rhs_local_tee<Opt, Op, Type...>; }
            };

            template <numeric_details::float_binop Op>
            struct f32_binop_localget_rhs_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop_localget_rhs<Opt, Op, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop_localget_rhs<Opt, Op, Type...>; }
            };

            template <numeric_details::float_binop Op>
            struct f32_binop_localget_rhs_local_set_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop_localget_rhs_local_set<Opt, Op, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop_localget_rhs_local_set<Opt, Op, Type...>; }
            };

            template <numeric_details::float_binop Op>
            struct f32_binop_localget_rhs_local_tee_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop_localget_rhs_local_tee<Opt, Op, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop_localget_rhs_local_tee<Opt, Op, Type...>; }
            };

            template <numeric_details::float_binop Op>
            struct f64_binop_localget_rhs_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop_localget_rhs<Opt, Op, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop_localget_rhs<Opt, Op, Type...>; }
            };

            template <numeric_details::float_binop Op>
            struct f64_binop_localget_rhs_local_set_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop_localget_rhs_local_set<Opt, Op, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop_localget_rhs_local_set<Opt, Op, Type...>; }
            };

            template <numeric_details::float_binop Op>
            struct f64_binop_localget_rhs_local_tee_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop_localget_rhs_local_tee<Opt, Op, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop_localget_rhs_local_tee<Opt, Op, Type...>; }
            };

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t Curr,
                      ::std::size_t End,
                      typename OpWrapper,
                      uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_by_currpos_delay_local_impl(::std::size_t pos) noexcept
            {
                if(pos == Curr) { return OpWrapper::template fptr<CompileOption, Curr, Type...>(); }
                if constexpr(Curr + 1uz < End)
                {
                    return select_stacktop_fptr_by_currpos_delay_local_impl<CompileOption, Curr + 1uz, End, OpWrapper, Type...>(pos);
                }
                else
                {
                    return OpWrapper::template fptr<CompileOption, Curr, Type...>();
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t BeginPos,
                      ::std::size_t EndPos,
                      typename OpWrapper,
                      uwvm_int_stack_top_type... Type>
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_or_default_delay_local(::std::size_t pos) noexcept
            {
                if constexpr(BeginPos != EndPos)
                {
                    return select_stacktop_fptr_by_currpos_delay_local_impl<CompileOption, BeginPos, EndPos, OpWrapper, Type...>(pos);
                }
                else
                {
                    return OpWrapper::template fptr<CompileOption, 0uz, Type...>();
                }
            }

        }  // namespace details
    }  // namespace delay_local_details

    namespace translate
    {
        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_binop_localget_rhs_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return delay_local_details::details::select_stacktop_fptr_or_default_delay_local<CompileOption,
                                                                                             CompileOption.i32_stack_top_begin_pos,
                                                                                             CompileOption.i32_stack_top_end_pos,
                                                                                             delay_local_details::details::i32_binop_localget_rhs_op<Op>,
                                                                                             Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_binop_localget_rhs_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_binop_localget_rhs_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return delay_local_details::details::i32_binop_localget_rhs_op<Op>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_binop_localget_rhs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_binop_localget_rhs_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_binop_localget_rhs_dyn_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return delay_local_details::details::select_stacktop_fptr_or_default_delay_local<CompileOption,
                                                                                             CompileOption.i32_stack_top_begin_pos,
                                                                                             CompileOption.i32_stack_top_end_pos,
                                                                                             delay_local_details::details::i32_binop_localget_rhs_dyn_op,
                                                                                             Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_binop_localget_rhs_dyn_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_binop_localget_rhs_dyn_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_i32_binop_localget_rhs_dyn_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return delay_local_details::details::i32_binop_localget_rhs_dyn_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_binop_localget_rhs_dyn_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_binop_localget_rhs_dyn_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_binop_localget_rhs_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return delay_local_details::details::select_stacktop_fptr_or_default_delay_local<
                CompileOption,
                CompileOption.i32_stack_top_begin_pos,
                CompileOption.i32_stack_top_end_pos,
                delay_local_details::details::i32_binop_localget_rhs_local_set_op<Op>,
                Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_binop_localget_rhs_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_binop_localget_rhs_local_set_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_i32_binop_localget_rhs_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return delay_local_details::details::i32_binop_localget_rhs_local_set_op<Op>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_binop_localget_rhs_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_binop_localget_rhs_local_set_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_binop_localget_rhs_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return delay_local_details::details::select_stacktop_fptr_or_default_delay_local<
                CompileOption,
                CompileOption.i32_stack_top_begin_pos,
                CompileOption.i32_stack_top_end_pos,
                delay_local_details::details::i32_binop_localget_rhs_local_tee_op<Op>,
                Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_binop_localget_rhs_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_binop_localget_rhs_local_tee_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_i32_binop_localget_rhs_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return delay_local_details::details::i32_binop_localget_rhs_local_tee_op<Op>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_binop_localget_rhs_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_binop_localget_rhs_local_tee_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_binop_localget_rhs_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return delay_local_details::details::select_stacktop_fptr_or_default_delay_local<CompileOption,
                                                                                             CompileOption.i64_stack_top_begin_pos,
                                                                                             CompileOption.i64_stack_top_end_pos,
                                                                                             delay_local_details::details::i64_binop_localget_rhs_op<Op>,
                                                                                             Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_binop_localget_rhs_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_binop_localget_rhs_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return delay_local_details::details::i64_binop_localget_rhs_op<Op>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_binop_localget_rhs_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i64_binop_localget_rhs_dyn_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return delay_local_details::details::select_stacktop_fptr_or_default_delay_local<CompileOption,
                                                                                             CompileOption.i64_stack_top_begin_pos,
                                                                                             CompileOption.i64_stack_top_end_pos,
                                                                                             delay_local_details::details::i64_binop_localget_rhs_dyn_op,
                                                                                             Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_binop_localget_rhs_dyn_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_binop_localget_rhs_dyn_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_i64_binop_localget_rhs_dyn_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return delay_local_details::details::i64_binop_localget_rhs_dyn_op::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_binop_localget_rhs_dyn_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_binop_localget_rhs_dyn_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i64_binop_localget_rhs_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return delay_local_details::details::select_stacktop_fptr_or_default_delay_local<
                CompileOption,
                CompileOption.i64_stack_top_begin_pos,
                CompileOption.i64_stack_top_end_pos,
                delay_local_details::details::i64_binop_localget_rhs_local_set_op<Op>,
                Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_binop_localget_rhs_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_binop_localget_rhs_local_set_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_i64_binop_localget_rhs_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return delay_local_details::details::i64_binop_localget_rhs_local_set_op<Op>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_binop_localget_rhs_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_binop_localget_rhs_local_set_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i64_binop_localget_rhs_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return delay_local_details::details::select_stacktop_fptr_or_default_delay_local<
                CompileOption,
                CompileOption.i64_stack_top_begin_pos,
                CompileOption.i64_stack_top_end_pos,
                delay_local_details::details::i64_binop_localget_rhs_local_tee_op<Op>,
                Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_binop_localget_rhs_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_binop_localget_rhs_local_tee_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_i64_binop_localget_rhs_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return delay_local_details::details::i64_binop_localget_rhs_local_tee_op<Op>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_binop_localget_rhs_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_binop_localget_rhs_local_tee_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_binop_localget_rhs_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return delay_local_details::details::select_stacktop_fptr_or_default_delay_local<CompileOption,
                                                                                             CompileOption.f32_stack_top_begin_pos,
                                                                                             CompileOption.f32_stack_top_end_pos,
                                                                                             delay_local_details::details::f32_binop_localget_rhs_op<Op>,
                                                                                             Type...>(curr.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_binop_localget_rhs_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_binop_localget_rhs_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return delay_local_details::details::f32_binop_localget_rhs_op<Op>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_binop_localget_rhs_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_binop_localget_rhs_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return delay_local_details::details::select_stacktop_fptr_or_default_delay_local<
                CompileOption,
                CompileOption.f32_stack_top_begin_pos,
                CompileOption.f32_stack_top_end_pos,
                delay_local_details::details::f32_binop_localget_rhs_local_set_op<Op>,
                Type...>(curr.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_binop_localget_rhs_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_binop_localget_rhs_local_set_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_f32_binop_localget_rhs_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return delay_local_details::details::f32_binop_localget_rhs_local_set_op<Op>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_binop_localget_rhs_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_binop_localget_rhs_local_set_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_binop_localget_rhs_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return delay_local_details::details::select_stacktop_fptr_or_default_delay_local<
                CompileOption,
                CompileOption.f32_stack_top_begin_pos,
                CompileOption.f32_stack_top_end_pos,
                delay_local_details::details::f32_binop_localget_rhs_local_tee_op<Op>,
                Type...>(curr.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_binop_localget_rhs_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_binop_localget_rhs_local_tee_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_f32_binop_localget_rhs_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return delay_local_details::details::f32_binop_localget_rhs_local_tee_op<Op>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_binop_localget_rhs_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_binop_localget_rhs_local_tee_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_binop_localget_rhs_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return delay_local_details::details::select_stacktop_fptr_or_default_delay_local<CompileOption,
                                                                                             CompileOption.f64_stack_top_begin_pos,
                                                                                             CompileOption.f64_stack_top_end_pos,
                                                                                             delay_local_details::details::f64_binop_localget_rhs_op<Op>,
                                                                                             Type...>(curr.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_binop_localget_rhs_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_binop_localget_rhs_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return delay_local_details::details::f64_binop_localget_rhs_op<Op>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_binop_localget_rhs_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_binop_localget_rhs_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return delay_local_details::details::select_stacktop_fptr_or_default_delay_local<
                CompileOption,
                CompileOption.f64_stack_top_begin_pos,
                CompileOption.f64_stack_top_end_pos,
                delay_local_details::details::f64_binop_localget_rhs_local_set_op<Op>,
                Type...>(curr.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_binop_localget_rhs_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_binop_localget_rhs_local_set_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_f64_binop_localget_rhs_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return delay_local_details::details::f64_binop_localget_rhs_local_set_op<Op>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_binop_localget_rhs_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_binop_localget_rhs_local_set_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_binop_localget_rhs_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return delay_local_details::details::select_stacktop_fptr_or_default_delay_local<
                CompileOption,
                CompileOption.f64_stack_top_begin_pos,
                CompileOption.f64_stack_top_end_pos,
                delay_local_details::details::f64_binop_localget_rhs_local_tee_op<Op>,
                Type...>(curr.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_binop_localget_rhs_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_binop_localget_rhs_local_tee_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_f64_binop_localget_rhs_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return delay_local_details::details::f64_binop_localget_rhs_local_tee_op<Op>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_binop_localget_rhs_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_binop_localget_rhs_local_tee_fptr<CompileOption, Op, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_add_localget_rhs_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_i32_binop_localget_rhs_fptr<CompileOption, numeric_details::int_binop::add, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_localget_rhs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_localget_rhs_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_add_localget_rhs_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return get_uwvmint_i32_binop_localget_rhs_fptr<CompileOption, numeric_details::int_binop::add, Type...>({}); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_localget_rhs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_localget_rhs_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_xor_localget_rhs_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return get_uwvmint_i32_binop_localget_rhs_fptr<CompileOption, numeric_details::int_binop::xor_, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_xor_localget_rhs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_xor_localget_rhs_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_xor_localget_rhs_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return get_uwvmint_i32_binop_localget_rhs_fptr<CompileOption, numeric_details::int_binop::xor_, Type...>({}); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_xor_localget_rhs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_xor_localget_rhs_fptr<CompileOption, TypeInTuple...>(curr); }

    }  // namespace translate
#endif
}  // namespace uwvm2::runtime::compiler::uwvm_int::optable
