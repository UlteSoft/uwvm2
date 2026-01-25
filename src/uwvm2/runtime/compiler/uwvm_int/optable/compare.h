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
#endif

#if !(__cpp_pack_indexing >= 202311L)
# error "UWVM requires at least C++26 standard compiler. See https://en.cppreference.com/w/cpp/feature_test#cpp_pack_indexing"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    namespace details
    {
        enum class int_cmp
        {
            eq,
            ne,
            lt_s,
            lt_u,
            gt_s,
            gt_u,
            le_s,
            le_u,
            ge_s,
            ge_u
        };

        enum class float_cmp
        {
            eq,
            ne,
            lt,
            gt,
            le,
            ge
        };

        /// @brief Converts a C++ `bool` into a Wasm i32 boolean (`0` or `1`).
        /// @details This is used by all compare operators to materialize the Wasm boolean result.
        /// @note The C++ standard specifies that when converting a bool to an int, the result must be either 0 or 1, so directly static_cast

        /// @brief Evaluates an integer comparison for the given signed/unsigned view.
        /// @details `SignedT` is used for signed compares and equality; unsigned compares reinterpret via `UnsignedT`.
        template <int_cmp Cmp, typename SignedT, typename UnsignedT>
        UWVM_ALWAYS_INLINE inline constexpr bool eval_int_cmp(SignedT lhs, SignedT rhs) noexcept
        {
            if constexpr(Cmp == int_cmp::eq) { return lhs == rhs; }
            else if constexpr(Cmp == int_cmp::ne) { return lhs != rhs; }
            else if constexpr(Cmp == int_cmp::lt_s) { return lhs < rhs; }
            else if constexpr(Cmp == int_cmp::lt_u) { return static_cast<UnsignedT>(lhs) < static_cast<UnsignedT>(rhs); }
            else if constexpr(Cmp == int_cmp::gt_s) { return lhs > rhs; }
            else if constexpr(Cmp == int_cmp::gt_u) { return static_cast<UnsignedT>(lhs) > static_cast<UnsignedT>(rhs); }
            else if constexpr(Cmp == int_cmp::le_s) { return lhs <= rhs; }
            else if constexpr(Cmp == int_cmp::le_u) { return static_cast<UnsignedT>(lhs) <= static_cast<UnsignedT>(rhs); }
            else if constexpr(Cmp == int_cmp::ge_s) { return lhs >= rhs; }
            else if constexpr(Cmp == int_cmp::ge_u) { return static_cast<UnsignedT>(lhs) >= static_cast<UnsignedT>(rhs); }
            else
            {
                return false;
            }
        }

        /// @brief Evaluates a floating-point comparison.
        /// @details This follows C++ floating semantics (including NaN behavior) for the underlying `FloatT`.
        template <float_cmp Cmp, typename FloatT>
        UWVM_ALWAYS_INLINE inline constexpr bool eval_float_cmp(FloatT lhs, FloatT rhs) noexcept
        {
            if constexpr(Cmp == float_cmp::eq) { return lhs == rhs; }
            else if constexpr(Cmp == float_cmp::ne) { return lhs != rhs; }
            else if constexpr(Cmp == float_cmp::lt) { return lhs < rhs; }
            else if constexpr(Cmp == float_cmp::gt) { return lhs > rhs; }
            else if constexpr(Cmp == float_cmp::le) { return lhs <= rhs; }
            else if constexpr(Cmp == float_cmp::ge) { return lhs >= rhs; }
            else
            {
                return false;
            }
        }

        /// @brief Compile-time check: whether stack-top caching is enabled for the given operand type.
        /// @details Returns `true` iff the corresponding `[begin,end)` range in `CompileOption` is non-empty.
        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval bool stacktop_enabled_for() noexcept
        {
            if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>)
            {
                return CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos;
            }
            else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>)
            {
                return CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos;
            }
            else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>)
            {
                return CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos;
            }
            else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>)
            {
                return CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos;
            }
            else
            {
                return false;
            }
        }
    }  // namespace details

    namespace details
    {
        /// @brief Compile-time check: whether the i32 stack-top range matches the operand stack-top range.
        /// @details Some compare ops (e.g. `i64.*`/`f32.*`/`f64.*`) produce an i32 result but consume non-i32 operands; when using the register ring, the
        /// result
        ///          is written back into the same ring slot, so the scalar (i32) range must be merged with the operand's range.
        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval bool i32_range_matches_operand_range() noexcept
        {
            if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>)
            {
                return CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                       CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos;
            }
            else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>)
            {
                return CompileOption.i32_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                       CompileOption.i32_stack_top_end_pos == CompileOption.f32_stack_top_end_pos;
            }
            else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>)
            {
                return CompileOption.i32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                       CompileOption.i32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos;
            }
            else
            {
                return true;
            }
        }
    }  // namespace details

    /// @brief i32 binary compare core (tail-call): evaluates an i32 comparison and produces a Wasm i32 boolean.
    /// @details
    /// - Stack-top optimization: supported when i32 stack-top caching is enabled; consumes two i32 values from the stack-top ring and writes the result back
    ///   (replacing the next slot / NOS), effectively reducing the virtual operand stack height by 1.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates; always loads the next opfunc pointer and tail-calls it).
    /// @note When stack-top caching is disabled, operands are popped from the operand stack and the result is pushed back via `type...[1u]`.
    template <uwvm_interpreter_translate_option_t CompileOption, details::int_cmp Cmp, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_cmp(Type... type) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t range_begin{CompileOption.i32_stack_top_begin_pos};
            constexpr ::std::size_t range_end{CompileOption.i32_stack_top_end_pos};
            static_assert(range_begin <= curr_stack_top && curr_stack_top < range_end);

            constexpr ::std::size_t next_pos{details::ring_next_pos(curr_stack_top, range_begin, range_end)};

            wasm_i32 const rhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_stack_top>(type...)};
            wasm_i32 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, next_pos>(type...)};

            wasm_i32 const out{static_cast<wasm_i32>(details::eval_int_cmp<Cmp, wasm_i32, wasm_u32>(lhs, rhs))};

            // Binary op: result replaces NOS (next_pos), stack height -1.
            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, next_pos>(out, type...);
        }
        else
        {
            wasm_i32 const rhs{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            wasm_i32 const lhs{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};

            wasm_i32 const out{static_cast<wasm_i32>(details::eval_int_cmp<Cmp, wasm_i32, wasm_u32>(lhs, rhs))};

            ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
            type...[1u] += sizeof(out);
        }

        // curr_op next_interpreter
        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `i32.eq` (tail-call): i32 equality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_eq(Type... type) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::eq, curr_stack_top>(type...); }

    /// @brief `i32.ne` (tail-call): i32 inequality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_ne(Type... type) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::ne, curr_stack_top>(type...); }

    /// @brief `i32.lt_s` (tail-call): signed less-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_lt_s(Type... type) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::lt_s, curr_stack_top>(type...); }

    /// @brief `i32.lt_u` (tail-call): unsigned less-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_lt_u(Type... type) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::lt_u, curr_stack_top>(type...); }

    /// @brief `i32.gt_s` (tail-call): signed greater-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_gt_s(Type... type) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::gt_s, curr_stack_top>(type...); }

    /// @brief `i32.gt_u` (tail-call): unsigned greater-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_gt_u(Type... type) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::gt_u, curr_stack_top>(type...); }

    /// @brief `i32.le_s` (tail-call): signed less-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_le_s(Type... type) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::le_s, curr_stack_top>(type...); }

    /// @brief `i32.le_u` (tail-call): unsigned less-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_le_u(Type... type) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::le_u, curr_stack_top>(type...); }

    /// @brief `i32.ge_s` (tail-call): signed greater-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_ge_s(Type... type) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::ge_s, curr_stack_top>(type...); }

    /// @brief `i32.ge_u` (tail-call): unsigned greater-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_ge_u(Type... type) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::ge_u, curr_stack_top>(type...); }

    /// @brief `i32.eqz` (tail-call): tests whether the current i32 operand equals zero.
    /// @details
    /// - Stack-top optimization: supported when i32 stack-top caching is enabled; reads/writes the current ring slot.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_eqz(Type... type) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t range_begin{CompileOption.i32_stack_top_begin_pos};
            constexpr ::std::size_t range_end{CompileOption.i32_stack_top_end_pos};
            static_assert(range_begin <= curr_stack_top && curr_stack_top < range_end);

            wasm_i32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_stack_top>(type...)};
            wasm_i32 const out{static_cast<wasm_i32>(v == wasm_i32{0})};
            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_stack_top>(out, type...);
        }
        else
        {
            wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            wasm_i32 const out{static_cast<wasm_i32>(v == wasm_i32{0})};

            ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
            type...[1u] += sizeof(out);
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief i64 binary compare core (tail-call): evaluates an i64 comparison and produces a Wasm i32 boolean.
    /// @details
    /// - Stack-top optimization: supported when i64 stack-top caching is enabled; requires the i32 and i64 stack-top ranges to be merged because the i32 result
    ///   is written back into the merged scalar ring slot.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption, details::int_cmp Cmp, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_cmp(Type... type) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_u64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
        {
            static_assert(details::i32_range_matches_operand_range<CompileOption, wasm_i64>(),
                          "register_ring compare requires i32 and i64 stack-top ranges to be merged");

            constexpr ::std::size_t range_begin{CompileOption.i64_stack_top_begin_pos};
            constexpr ::std::size_t range_end{CompileOption.i64_stack_top_end_pos};
            static_assert(range_begin <= curr_stack_top && curr_stack_top < range_end);

            constexpr ::std::size_t next_pos{details::ring_next_pos(curr_stack_top, range_begin, range_end)};

            wasm_i64 const rhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_stack_top>(type...)};
            wasm_i64 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, next_pos>(type...)};

            wasm_i32 const out{static_cast<wasm_i32>(details::eval_int_cmp<Cmp, wasm_i64, wasm_u64>(lhs, rhs))};

            // Binary i64->i32: result replaces NOS (next_pos) in the merged scalar ring.
            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, next_pos>(out, type...);
        }
        else
        {
            wasm_i64 const rhs{get_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
            wasm_i64 const lhs{get_curr_val_from_operand_stack_cache<wasm_i64>(type...)};

            wasm_i32 const out{static_cast<wasm_i32>(details::eval_int_cmp<Cmp, wasm_i64, wasm_u64>(lhs, rhs))};

            ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
            type...[1u] += sizeof(out);
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `i64.eq` (tail-call): i64 equality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_eq(Type... type) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::eq, curr_stack_top>(type...); }

    /// @brief `i64.ne` (tail-call): i64 inequality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_ne(Type... type) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::ne, curr_stack_top>(type...); }

    /// @brief `i64.lt_s` (tail-call): signed less-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_lt_s(Type... type) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::lt_s, curr_stack_top>(type...); }

    /// @brief `i64.lt_u` (tail-call): unsigned less-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_lt_u(Type... type) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::lt_u, curr_stack_top>(type...); }

    /// @brief `i64.gt_s` (tail-call): signed greater-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_gt_s(Type... type) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::gt_s, curr_stack_top>(type...); }

    /// @brief `i64.gt_u` (tail-call): unsigned greater-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_gt_u(Type... type) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::gt_u, curr_stack_top>(type...); }

    /// @brief `i64.le_s` (tail-call): signed less-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_le_s(Type... type) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::le_s, curr_stack_top>(type...); }

    /// @brief `i64.le_u` (tail-call): unsigned less-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_le_u(Type... type) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::le_u, curr_stack_top>(type...); }

    /// @brief `i64.ge_s` (tail-call): signed greater-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_ge_s(Type... type) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::ge_s, curr_stack_top>(type...); }

    /// @brief `i64.ge_u` (tail-call): unsigned greater-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_ge_u(Type... type) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::ge_u, curr_stack_top>(type...); }

    /// @brief `i64.eqz` (tail-call): tests whether the current i64 operand equals zero and produces an i32 boolean.
    /// @details
    /// - Stack-top optimization: supported when i64 stack-top caching is enabled; requires i32 and i64 stack-top ranges to be merged because the i32 result is
    ///   written back into the merged scalar ring slot.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_eqz(Type... type) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
        {
            static_assert(details::i32_range_matches_operand_range<CompileOption, wasm_i64>(),
                          "register_ring compare requires i32 and i64 stack-top ranges to be merged");

            constexpr ::std::size_t range_begin{CompileOption.i64_stack_top_begin_pos};
            constexpr ::std::size_t range_end{CompileOption.i64_stack_top_end_pos};
            static_assert(range_begin <= curr_stack_top && curr_stack_top < range_end);

            wasm_i64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_stack_top>(type...)};
            wasm_i32 const out{static_cast<wasm_i32>(v == wasm_i64{0})};

            // Unary i64->i32: replace in-place in merged scalar ring.
            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_stack_top>(out, type...);
        }
        else
        {
            wasm_i64 const v{get_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
            wasm_i32 const out{static_cast<wasm_i32>(v == wasm_i64{0})};

            ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
            type...[1u] += sizeof(out);
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    UWVM_UWVM_INT_STRICT_FP_BEGIN

    /// @brief f32 binary compare core (tail-call): evaluates an f32 comparison and produces a Wasm i32 boolean.
    /// @details
    /// - Stack-top optimization: supported when f32 stack-top caching is enabled; requires the i32 and f32 stack-top ranges to be merged because the i32 result
    ///   is written back into the merged scalar ring slot.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption, details::float_cmp Cmp, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_cmp(Type... type) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
        {
            static_assert(details::i32_range_matches_operand_range<CompileOption, wasm_f32>(),
                          "register_ring compare requires i32 and f32 stack-top ranges to be merged");

            constexpr ::std::size_t range_begin{CompileOption.f32_stack_top_begin_pos};
            constexpr ::std::size_t range_end{CompileOption.f32_stack_top_end_pos};
            static_assert(range_begin <= curr_stack_top && curr_stack_top < range_end);

            constexpr ::std::size_t next_pos{details::ring_next_pos(curr_stack_top, range_begin, range_end)};

            wasm_f32 const rhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, curr_stack_top>(type...)};
            wasm_f32 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, next_pos>(type...)};

            wasm_i32 const out{static_cast<wasm_i32>(details::eval_float_cmp<Cmp, wasm_f32>(lhs, rhs))};

            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, next_pos>(out, type...);
        }
        else
        {
            wasm_f32 const rhs{get_curr_val_from_operand_stack_cache<wasm_f32>(type...)};
            wasm_f32 const lhs{get_curr_val_from_operand_stack_cache<wasm_f32>(type...)};

            wasm_i32 const out{static_cast<wasm_i32>(details::eval_float_cmp<Cmp, wasm_f32>(lhs, rhs))};

            ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
            type...[1u] += sizeof(out);
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `f32.eq` (tail-call): f32 equality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_eq(Type... type) UWVM_THROWS
    { return uwvmint_f32_cmp<CompileOption, details::float_cmp::eq, curr_stack_top>(type...); }

    /// @brief `f32.ne` (tail-call): f32 inequality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_ne(Type... type) UWVM_THROWS
    { return uwvmint_f32_cmp<CompileOption, details::float_cmp::ne, curr_stack_top>(type...); }

    /// @brief `f32.lt` (tail-call): f32 less-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_lt(Type... type) UWVM_THROWS
    { return uwvmint_f32_cmp<CompileOption, details::float_cmp::lt, curr_stack_top>(type...); }

    /// @brief `f32.gt` (tail-call): f32 greater-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_gt(Type... type) UWVM_THROWS
    { return uwvmint_f32_cmp<CompileOption, details::float_cmp::gt, curr_stack_top>(type...); }

    /// @brief `f32.le` (tail-call): f32 less-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_le(Type... type) UWVM_THROWS
    { return uwvmint_f32_cmp<CompileOption, details::float_cmp::le, curr_stack_top>(type...); }

    /// @brief `f32.ge` (tail-call): f32 greater-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f32_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_ge(Type... type) UWVM_THROWS
    { return uwvmint_f32_cmp<CompileOption, details::float_cmp::ge, curr_stack_top>(type...); }

    /// @brief f64 binary compare core (tail-call): evaluates an f64 comparison and produces a Wasm i32 boolean.
    /// @details
    /// - Stack-top optimization: supported when f64 stack-top caching is enabled; requires the i32 and f64 stack-top ranges to be merged because the i32 result
    ///   is written back into the merged scalar ring slot.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption, details::float_cmp Cmp, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_cmp(Type... type) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
        {
            static_assert(details::i32_range_matches_operand_range<CompileOption, wasm_f64>(),
                          "register_ring compare requires i32 and f64 stack-top ranges to be merged");

            constexpr ::std::size_t range_begin{CompileOption.f64_stack_top_begin_pos};
            constexpr ::std::size_t range_end{CompileOption.f64_stack_top_end_pos};
            static_assert(range_begin <= curr_stack_top && curr_stack_top < range_end);

            constexpr ::std::size_t next_pos{details::ring_next_pos(curr_stack_top, range_begin, range_end)};

            wasm_f64 const rhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, curr_stack_top>(type...)};
            wasm_f64 const lhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, next_pos>(type...)};

            wasm_i32 const out{static_cast<wasm_i32>(details::eval_float_cmp<Cmp, wasm_f64>(lhs, rhs))};

            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, next_pos>(out, type...);
        }
        else
        {
            wasm_f64 const rhs{get_curr_val_from_operand_stack_cache<wasm_f64>(type...)};
            wasm_f64 const lhs{get_curr_val_from_operand_stack_cache<wasm_f64>(type...)};

            wasm_i32 const out{static_cast<wasm_i32>(details::eval_float_cmp<Cmp, wasm_f64>(lhs, rhs))};

            ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
            type...[1u] += sizeof(out);
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `f64.eq` (tail-call): f64 equality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_eq(Type... type) UWVM_THROWS
    { return uwvmint_f64_cmp<CompileOption, details::float_cmp::eq, curr_stack_top>(type...); }

    /// @brief `f64.ne` (tail-call): f64 inequality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_ne(Type... type) UWVM_THROWS
    { return uwvmint_f64_cmp<CompileOption, details::float_cmp::ne, curr_stack_top>(type...); }

    /// @brief `f64.lt` (tail-call): f64 less-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_lt(Type... type) UWVM_THROWS
    { return uwvmint_f64_cmp<CompileOption, details::float_cmp::lt, curr_stack_top>(type...); }

    /// @brief `f64.gt` (tail-call): f64 greater-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_gt(Type... type) UWVM_THROWS
    { return uwvmint_f64_cmp<CompileOption, details::float_cmp::gt, curr_stack_top>(type...); }

    /// @brief `f64.le` (tail-call): f64 less-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_le(Type... type) UWVM_THROWS
    { return uwvmint_f64_cmp<CompileOption, details::float_cmp::le, curr_stack_top>(type...); }

    /// @brief `f64.ge` (tail-call): f64 greater-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f64_cmp`.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_ge(Type... type) UWVM_THROWS
    { return uwvmint_f64_cmp<CompileOption, details::float_cmp::ge, curr_stack_top>(type...); }

    UWVM_UWVM_INT_STRICT_FP_END

    // Non-tailcall (byref) variants: stacktop caching is disabled, operate purely on the operand stack.
    /// @brief i32 binary compare core (non-tail-call/byref): evaluates an i32 comparison and pushes a Wasm i32 boolean.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching; all stack-top ranges must be `SIZE_MAX`).
    /// - `type[0]` layout: `[opfunc_byref_ptr][next_opfunc_byref_ptr]...` (no immediates; this function advances `typeref...[0]` to the next opfunc slot).
    /// @note Operands are popped from the operand stack and the result is appended back to the operand stack.
    template <uwvm_interpreter_translate_option_t CompileOption, details::int_cmp Cmp, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_cmp(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i32 const rhs{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_i32 const lhs{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_i32 const out{static_cast<wasm_i32>(details::eval_int_cmp<Cmp, wasm_i32, wasm_u32>(lhs, rhs))};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i32.eq` (non-tail-call/byref): i32 equality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_eq(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::eq>(typeref...); }

    /// @brief `i32.ne` (non-tail-call/byref): i32 inequality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_ne(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::ne>(typeref...); }

    /// @brief `i32.lt_s` (non-tail-call/byref): signed less-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_lt_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::lt_s>(typeref...); }

    /// @brief `i32.lt_u` (non-tail-call/byref): unsigned less-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_lt_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::lt_u>(typeref...); }

    /// @brief `i32.gt_s` (non-tail-call/byref): signed greater-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_gt_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::gt_s>(typeref...); }

    /// @brief `i32.gt_u` (non-tail-call/byref): unsigned greater-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_gt_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::gt_u>(typeref...); }

    /// @brief `i32.le_s` (non-tail-call/byref): signed less-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_le_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::le_s>(typeref...); }

    /// @brief `i32.le_u` (non-tail-call/byref): unsigned less-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_le_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::le_u>(typeref...); }

    /// @brief `i32.ge_s` (non-tail-call/byref): signed greater-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_ge_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::ge_s>(typeref...); }

    /// @brief `i32.ge_u` (non-tail-call/byref): unsigned greater-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_ge_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_cmp<CompileOption, details::int_cmp::ge_u>(typeref...); }

    /// @brief `i32.eqz` (non-tail-call/byref): tests whether the current i32 operand equals zero.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching).
    /// - `type[0]` layout: `[opfunc_byref_ptr][next_opfunc_byref_ptr]...` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_eqz(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_i32 const out{static_cast<wasm_i32>(v == wasm_i32{0})};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief i64 binary compare core (non-tail-call/byref): evaluates an i64 comparison and pushes a Wasm i32 boolean.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching).
    /// - `type[0]` layout: `[opfunc_byref_ptr][next_opfunc_byref_ptr]...` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption, details::int_cmp Cmp, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_cmp(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_u64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i64 const rhs{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        wasm_i64 const lhs{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        wasm_i32 const out{static_cast<wasm_i32>(details::eval_int_cmp<Cmp, wasm_i64, wasm_u64>(lhs, rhs))};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i64.eq` (non-tail-call/byref): i64 equality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_eq(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::eq>(typeref...); }

    /// @brief `i64.ne` (non-tail-call/byref): i64 inequality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_ne(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::ne>(typeref...); }

    /// @brief `i64.lt_s` (non-tail-call/byref): signed less-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_lt_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::lt_s>(typeref...); }

    /// @brief `i64.lt_u` (non-tail-call/byref): unsigned less-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_lt_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::lt_u>(typeref...); }

    /// @brief `i64.gt_s` (non-tail-call/byref): signed greater-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_gt_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::gt_s>(typeref...); }

    /// @brief `i64.gt_u` (non-tail-call/byref): unsigned greater-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_gt_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::gt_u>(typeref...); }

    /// @brief `i64.le_s` (non-tail-call/byref): signed less-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_le_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::le_s>(typeref...); }

    /// @brief `i64.le_u` (non-tail-call/byref): unsigned less-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_le_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::le_u>(typeref...); }

    /// @brief `i64.ge_s` (non-tail-call/byref): signed greater-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_ge_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::ge_s>(typeref...); }

    /// @brief `i64.ge_u` (non-tail-call/byref): unsigned greater-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_i64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_ge_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_cmp<CompileOption, details::int_cmp::ge_u>(typeref...); }

    /// @brief `i64.eqz` (non-tail-call/byref): tests whether the current i64 operand equals zero and produces an i32 boolean.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching).
    /// - `type[0]` layout: `[opfunc_byref_ptr][next_opfunc_byref_ptr]...` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_eqz(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i64 const v{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        wasm_i32 const out{static_cast<wasm_i32>(v == wasm_i64{0})};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    UWVM_UWVM_INT_STRICT_FP_BEGIN

    /// @brief f32 binary compare core (non-tail-call/byref): evaluates an f32 comparison and pushes a Wasm i32 boolean.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching).
    /// - `type[0]` layout: `[opfunc_byref_ptr][next_opfunc_byref_ptr]...` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption, details::float_cmp Cmp, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_cmp(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f32 const rhs{get_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
        wasm_f32 const lhs{get_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
        wasm_i32 const out{static_cast<wasm_i32>(details::eval_float_cmp<Cmp, wasm_f32>(lhs, rhs))};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f32.eq` (non-tail-call/byref): f32 equality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_eq(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_cmp<CompileOption, details::float_cmp::eq>(typeref...); }

    /// @brief `f32.ne` (non-tail-call/byref): f32 inequality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_ne(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_cmp<CompileOption, details::float_cmp::ne>(typeref...); }

    /// @brief `f32.lt` (non-tail-call/byref): f32 less-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_lt(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_cmp<CompileOption, details::float_cmp::lt>(typeref...); }

    /// @brief `f32.gt` (non-tail-call/byref): f32 greater-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_gt(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_cmp<CompileOption, details::float_cmp::gt>(typeref...); }

    /// @brief `f32.le` (non-tail-call/byref): f32 less-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_le(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_cmp<CompileOption, details::float_cmp::le>(typeref...); }

    /// @brief `f32.ge` (non-tail-call/byref): f32 greater-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f32_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_ge(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_cmp<CompileOption, details::float_cmp::ge>(typeref...); }

    /// @brief f64 binary compare core (non-tail-call/byref): evaluates an f64 comparison and pushes a Wasm i32 boolean.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching).
    /// - `type[0]` layout: `[opfunc_byref_ptr][next_opfunc_byref_ptr]...` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption, details::float_cmp Cmp, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_cmp(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f64 const rhs{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        wasm_f64 const lhs{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        wasm_i32 const out{static_cast<wasm_i32>(details::eval_float_cmp<Cmp, wasm_f64>(lhs, rhs))};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f64.eq` (non-tail-call/byref): f64 equality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_eq(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_cmp<CompileOption, details::float_cmp::eq>(typeref...); }

    /// @brief `f64.ne` (non-tail-call/byref): f64 inequality compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_ne(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_cmp<CompileOption, details::float_cmp::ne>(typeref...); }

    /// @brief `f64.lt` (non-tail-call/byref): f64 less-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_lt(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_cmp<CompileOption, details::float_cmp::lt>(typeref...); }

    /// @brief `f64.gt` (non-tail-call/byref): f64 greater-than compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_gt(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_cmp<CompileOption, details::float_cmp::gt>(typeref...); }

    /// @brief `f64.le` (non-tail-call/byref): f64 less-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_le(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_cmp<CompileOption, details::float_cmp::le>(typeref...); }

    /// @brief `f64.ge` (non-tail-call/byref): f64 greater-or-equal compare.
    /// @details Stack-top optimization and `type[0]` layout are the same as `uwvmint_f64_cmp` (byref).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_ge(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_cmp<CompileOption, details::float_cmp::ge>(typeref...); }

    UWVM_UWVM_INT_STRICT_FP_END

    namespace translate
    {
        namespace details
        {
            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t Curr,
                      ::std::size_t End,
                      typename OpWrapper,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_by_currpos_impl(::std::size_t pos) noexcept
            {
                static_assert(Curr < End);

                if(pos == Curr) { return OpWrapper::template fptr<CompileOption, Curr, Type...>(); }
                else
                {
                    if constexpr(Curr + 1uz < End) { return select_stacktop_fptr_by_currpos_impl<CompileOption, Curr + 1uz, End, OpWrapper, Type...>(pos); }
                    else
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            struct i32_eq_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_eq<Opt, Pos, Type...>; }
            };

            struct i32_ne_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_ne<Opt, Pos, Type...>; }
            };

            struct i32_lt_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_lt_s<Opt, Pos, Type...>; }
            };

            struct i32_lt_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_lt_u<Opt, Pos, Type...>; }
            };

            struct i32_gt_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_gt_s<Opt, Pos, Type...>; }
            };

            struct i32_gt_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_gt_u<Opt, Pos, Type...>; }
            };

            struct i32_le_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_le_s<Opt, Pos, Type...>; }
            };

            struct i32_le_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_le_u<Opt, Pos, Type...>; }
            };

            struct i32_ge_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_ge_s<Opt, Pos, Type...>; }
            };

            struct i32_ge_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_ge_u<Opt, Pos, Type...>; }
            };

            struct i32_eqz_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_eqz<Opt, Pos, Type...>; }
            };

            struct i64_eq_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_eq<Opt, Pos, Type...>; }
            };

            struct i64_ne_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_ne<Opt, Pos, Type...>; }
            };

            struct i64_lt_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_lt_s<Opt, Pos, Type...>; }
            };

            struct i64_lt_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_lt_u<Opt, Pos, Type...>; }
            };

            struct i64_gt_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_gt_s<Opt, Pos, Type...>; }
            };

            struct i64_gt_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_gt_u<Opt, Pos, Type...>; }
            };

            struct i64_le_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_le_s<Opt, Pos, Type...>; }
            };

            struct i64_le_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_le_u<Opt, Pos, Type...>; }
            };

            struct i64_ge_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_ge_s<Opt, Pos, Type...>; }
            };

            struct i64_ge_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_ge_u<Opt, Pos, Type...>; }
            };

            struct i64_eqz_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_eqz<Opt, Pos, Type...>; }
            };

            struct f32_eq_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_eq<Opt, Pos, Type...>; }
            };

            struct f32_ne_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_ne<Opt, Pos, Type...>; }
            };

            struct f32_lt_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_lt<Opt, Pos, Type...>; }
            };

            struct f32_gt_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_gt<Opt, Pos, Type...>; }
            };

            struct f32_le_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_le<Opt, Pos, Type...>; }
            };

            struct f32_ge_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_ge<Opt, Pos, Type...>; }
            };

            struct f64_eq_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_eq<Opt, Pos, Type...>; }
            };

            struct f64_ne_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_ne<Opt, Pos, Type...>; }
            };

            struct f64_lt_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_lt<Opt, Pos, Type...>; }
            };

            struct f64_gt_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_gt<Opt, Pos, Type...>; }
            };

            struct f64_le_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_le<Opt, Pos, Type...>; }
            };

            struct f64_ge_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_ge<Opt, Pos, Type...>; }
            };
        }  // namespace details

        // i32
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_eq_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_eq_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_eq<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_eq_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_eq_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_ne_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_ne_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_ne<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ne_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ne_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_lt_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_lt_s_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_lt_s<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_lt_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_lt_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_lt_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_lt_u_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_lt_u<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_lt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_lt_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_gt_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_gt_s_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_gt_s<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_gt_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_gt_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_gt_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_gt_u_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_gt_u<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_gt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_gt_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_le_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_le_s_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_le_s<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_le_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_le_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_le_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_le_u_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_le_u<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_le_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_le_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_ge_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_ge_s_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_ge_s<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ge_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ge_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_ge_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_ge_u_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_ge_u<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ge_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ge_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_eqz_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i32_stack_top_begin_pos,
                                                                     CompileOption.i32_stack_top_end_pos,
                                                                     details::i32_eqz_op,
                                                                     Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i32_eqz<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_eqz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_eqz_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        // i64
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_eq_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_eq_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_eq<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_eq_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_eq_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_ne_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_ne_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_ne<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_ne_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_ne_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_lt_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_lt_s_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_lt_s<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_lt_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_lt_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_lt_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_lt_u_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_lt_u<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_lt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_lt_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_gt_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_gt_s_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_gt_s<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_gt_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_gt_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_gt_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_gt_u_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_gt_u<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_gt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_gt_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_le_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_le_s_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_le_s<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_le_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_le_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_le_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_le_u_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_le_u<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_le_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_le_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_ge_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_ge_s_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_ge_s<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_ge_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_ge_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_ge_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_ge_u_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_ge_u<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_ge_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_ge_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_eqz_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                     CompileOption.i64_stack_top_end_pos,
                                                                     details::i64_eqz_op,
                                                                     Type...>(curr_stacktop.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_i64_eqz<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_eqz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_eqz_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        // f32
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_eq_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.f32_stack_top_begin_pos,
                                                                     CompileOption.f32_stack_top_end_pos,
                                                                     details::f32_eq_op,
                                                                     Type...>(curr_stacktop.f32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f32_eq<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_eq_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_eq_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_ne_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.f32_stack_top_begin_pos,
                                                                     CompileOption.f32_stack_top_end_pos,
                                                                     details::f32_ne_op,
                                                                     Type...>(curr_stacktop.f32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f32_ne<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_ne_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_ne_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_lt_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.f32_stack_top_begin_pos,
                                                                     CompileOption.f32_stack_top_end_pos,
                                                                     details::f32_lt_op,
                                                                     Type...>(curr_stacktop.f32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f32_lt<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_lt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_lt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_gt_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.f32_stack_top_begin_pos,
                                                                     CompileOption.f32_stack_top_end_pos,
                                                                     details::f32_gt_op,
                                                                     Type...>(curr_stacktop.f32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f32_gt<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_gt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_gt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_le_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.f32_stack_top_begin_pos,
                                                                     CompileOption.f32_stack_top_end_pos,
                                                                     details::f32_le_op,
                                                                     Type...>(curr_stacktop.f32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f32_le<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_le_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_le_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_ge_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.f32_stack_top_begin_pos,
                                                                     CompileOption.f32_stack_top_end_pos,
                                                                     details::f32_ge_op,
                                                                     Type...>(curr_stacktop.f32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f32_ge<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_ge_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_ge_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        // f64
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_eq_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.f64_stack_top_begin_pos,
                                                                     CompileOption.f64_stack_top_end_pos,
                                                                     details::f64_eq_op,
                                                                     Type...>(curr_stacktop.f64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f64_eq<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_eq_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_eq_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_ne_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.f64_stack_top_begin_pos,
                                                                     CompileOption.f64_stack_top_end_pos,
                                                                     details::f64_ne_op,
                                                                     Type...>(curr_stacktop.f64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f64_ne<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_ne_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_ne_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_lt_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.f64_stack_top_begin_pos,
                                                                     CompileOption.f64_stack_top_end_pos,
                                                                     details::f64_lt_op,
                                                                     Type...>(curr_stacktop.f64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f64_lt<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_lt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_lt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_gt_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.f64_stack_top_begin_pos,
                                                                     CompileOption.f64_stack_top_end_pos,
                                                                     details::f64_gt_op,
                                                                     Type...>(curr_stacktop.f64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f64_gt<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_gt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_gt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_le_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.f64_stack_top_begin_pos,
                                                                     CompileOption.f64_stack_top_end_pos,
                                                                     details::f64_le_op,
                                                                     Type...>(curr_stacktop.f64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f64_le<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_le_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_le_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_ge_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl<CompileOption,
                                                                     CompileOption.f64_stack_top_begin_pos,
                                                                     CompileOption.f64_stack_top_end_pos,
                                                                     details::f64_ge_op,
                                                                     Type...>(curr_stacktop.f64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_f64_ge<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_ge_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_ge_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        // Non-tailcall translate: single version per op.
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_eq_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_eq<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_eq_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_eq_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_ne_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_ne<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ne_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ne_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_lt_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_lt_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_lt_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_lt_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_lt_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_lt_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_lt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_lt_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_gt_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_gt_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_gt_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_gt_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_gt_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_gt_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_gt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_gt_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_le_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_le_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_le_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_le_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_le_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_le_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_le_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_le_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_ge_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_ge_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ge_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ge_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_ge_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_ge_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ge_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ge_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_eqz_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_eqz<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_eqz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_eqz_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_eq_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_eq<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_eq_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_eq_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_ne_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_ne<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_ne_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_ne_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_lt_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_lt_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_lt_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_lt_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_lt_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_lt_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_lt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_lt_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_gt_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_gt_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_gt_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_gt_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_gt_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_gt_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_gt_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_gt_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_le_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_le_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_le_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_le_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_le_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_le_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_le_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_le_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_ge_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_ge_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_ge_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_ge_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_ge_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_ge_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_ge_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_ge_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_eqz_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_eqz<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_eqz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_eqz_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_eq_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_eq<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_eq_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_eq_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_ne_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_ne<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_ne_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_ne_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_lt_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_lt<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_lt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_lt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_gt_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_gt<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_gt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_gt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_le_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_le<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_le_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_le_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_ge_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_ge<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_ge_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_ge_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_eq_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_eq<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_eq_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_eq_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_ne_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_ne<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_ne_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_ne_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_lt_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_lt<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_lt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_lt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_gt_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_gt<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_gt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_gt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_le_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_le<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_le_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_le_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_ge_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_ge<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_ge_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_ge_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
