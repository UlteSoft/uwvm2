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
# include <concepts>
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
    namespace stack_details
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval bool stacktop_enabled_for() noexcept
        {
            if constexpr(::std::same_as<OperandT, wasm_i32>) { return CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_i64>) { return CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f32>) { return CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f64>) { return CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos; }
            else
            {
                return false;
            }
        }
    }  // namespace stack_details

    // ========================
    // drop (parametric)
    // ========================

    /// @brief `drop` opcode (tail-call): pops and discards the current operand.
    /// @details
    /// - Stack-top optimization: the translator folds away `drop` when stack-top caching is enabled (by decrementing the cached-remain counter), so this opcode
    ///   must only be emitted when the operand is not in the stack-top cache (`curr_stack_top == SIZE_MAX`). This avoids unnecessary template expansion and
    ///   prevents incorrect memory stack-pointer updates on cache-hit paths.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]`.
    template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_drop_typed(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(stack_details::stacktop_enabled_for<CompileOption, OperandT>())
        {
            static_assert(curr_stack_top == SIZE_MAX, "drop must not be emitted on stack-top cache hit paths");
        }

        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        type...[1u] -= sizeof(OperandT);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `drop` opcode (non-tail-call/byref): pops and discards the current operand (operand-stack only).
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching).
    /// - `type[0]` layout: `[opfunc_byref_ptr][next_opfunc_byref_ptr]...`.
    template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_drop_typed(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);
        typeref...[1u] -= sizeof(OperandT);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_drop_i32(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_drop_typed<CompileOption, stack_details::wasm_i32>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_drop_i64(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_drop_typed<CompileOption, stack_details::wasm_i64>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_drop_f32(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_drop_typed<CompileOption, stack_details::wasm_f32>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_drop_f64(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_drop_typed<CompileOption, stack_details::wasm_f64>(typeref...); }

    // ========================
    // select (parametric)
    // ========================

    /// @brief `select` opcode (tail-call): selects between two operands based on an i32 condition.
    /// @details
    /// - Stack-top optimization: supported. The condition is always i32; the selected value can be i32/i64/f32/f64 and may live in a merged or disjoint
    ///   stack-top range, so the translate step selects the correct specialized opcode.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]`.
    template <uwvm_interpreter_translate_option_t CompileOption,
              typename ValueT,
              ::std::size_t curr_i32_stack_top,
              ::std::size_t curr_value_stack_top = curr_i32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_select_typed(Type... type) UWVM_THROWS
    {
        using wasm_i32 = stack_details::wasm_i32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(stack_details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t i32_range_begin{details::stacktop_range_begin_pos<CompileOption, wasm_i32>()};
            constexpr ::std::size_t i32_range_end{details::stacktop_range_end_pos<CompileOption, wasm_i32>()};
            static_assert(i32_range_begin <= curr_i32_stack_top && curr_i32_stack_top < i32_range_end);

            wasm_i32 const cond{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};

            if constexpr(stack_details::stacktop_enabled_for<CompileOption, ValueT>())
            {
                constexpr ::std::size_t value_range_begin{details::stacktop_range_begin_pos<CompileOption, ValueT>()};
                constexpr ::std::size_t value_range_end{details::stacktop_range_end_pos<CompileOption, ValueT>()};
                constexpr bool merged_range{i32_range_begin == value_range_begin && i32_range_end == value_range_end};

                if constexpr(merged_range)
                {
                    static_assert(curr_value_stack_top == curr_i32_stack_top);

                    constexpr ::std::size_t ring_sz{i32_range_end - i32_range_begin};
                    static_assert(ring_sz != 0uz);
                    constexpr ::std::size_t v2_pos{details::ring_next_pos(curr_i32_stack_top, i32_range_begin, i32_range_end)};
                    constexpr ::std::size_t v1_pos{details::ring_next_pos(v2_pos, i32_range_begin, i32_range_end)};

                    if constexpr(ring_sz >= 3uz)
                    {
                        ValueT const v2{get_curr_val_from_operand_stack_top<CompileOption, ValueT, v2_pos>(type...)};
                        ValueT const v1{get_curr_val_from_operand_stack_top<CompileOption, ValueT, v1_pos>(type...)};
                        ValueT const out{cond != wasm_i32{0} ? v1 : v2};
                        details::set_curr_val_to_stacktop_cache<CompileOption, ValueT, v1_pos>(out, type...);
                    }
                    else if constexpr(ring_sz == 2uz)
                    {
                        ValueT const v2{get_curr_val_from_operand_stack_top<CompileOption, ValueT, v2_pos>(type...)};
                        // `v1` is in operand stack memory and is kept (becomes the result).
                        ValueT const v1{peek_curr_val_from_operand_stack_cache<ValueT>(type...)};
                        ValueT const out{cond != wasm_i32{0} ? v1 : v2};
                        set_curr_val_to_operand_stack_cache_top(out, type...);
                    }
                    else
                    {
                        static_assert(ring_sz == 1uz);
                        // `v2` and `v1` are both in operand stack memory; consume `v2`, keep `v1`.
                        ValueT const v2_mem{peek_nth_val_from_operand_stack_cache<ValueT, 0uz>(type...)};
                        ValueT const v1_mem{peek_nth_val_from_operand_stack_cache<ValueT, 1uz>(type...)};
                        ValueT const out{cond != wasm_i32{0} ? v1_mem : v2_mem};
                        set_nth_val_to_operand_stack_cache<ValueT, 1uz>(out, type...);
                        type...[1u] -= sizeof(ValueT);
                    }
                }
                else
                {
                    static_assert(value_range_begin <= curr_value_stack_top && curr_value_stack_top < value_range_end);

                    constexpr ::std::size_t ring_sz{value_range_end - value_range_begin};
                    static_assert(ring_sz != 0uz);
                    constexpr ::std::size_t v2_pos{curr_value_stack_top};
                    constexpr ::std::size_t v1_pos{details::ring_next_pos(v2_pos, value_range_begin, value_range_end)};

                    ValueT const v2{get_curr_val_from_operand_stack_top<CompileOption, ValueT, v2_pos>(type...)};
                    ValueT v1{};  // init
                    if constexpr(ring_sz >= 2uz) { v1 = get_curr_val_from_operand_stack_top<CompileOption, ValueT, v1_pos>(type...); }
                    else
                    {
                        static_assert(ring_sz == 1uz);
                        // `v1` is in operand stack memory and is kept (becomes the result).
                        v1 = peek_curr_val_from_operand_stack_cache<ValueT>(type...);
                    }
                    ValueT const out{cond != wasm_i32{0} ? v1 : v2};
                    if constexpr(ring_sz >= 2uz) { details::set_curr_val_to_stacktop_cache<CompileOption, ValueT, v1_pos>(out, type...); }
                    else
                    {
                        set_curr_val_to_operand_stack_cache_top(out, type...);
                    }
                }
            }
            else
            {
                // Condition is cached (i32 ring), but value type has no stack-top cache range.
                // This configuration can occur when ValueT is not cachable on the current ABI/ISA; in that case the two values are on the operand stack.
                ValueT const v2{get_curr_val_from_operand_stack_cache<ValueT>(type...)};
                ValueT const v1{get_curr_val_from_operand_stack_cache<ValueT>(type...)};

                ValueT const out{cond != wasm_i32{0} ? v1 : v2};
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_i32 const cond{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};

            if constexpr(stack_details::stacktop_enabled_for<CompileOption, ValueT>())
            {
                // Condition is on the operand stack (no i32 cache), but ValueT is cached in its own ring.
                constexpr ::std::size_t value_range_begin{details::stacktop_range_begin_pos<CompileOption, ValueT>()};
                constexpr ::std::size_t value_range_end{details::stacktop_range_end_pos<CompileOption, ValueT>()};
                static_assert(value_range_begin <= curr_value_stack_top && curr_value_stack_top < value_range_end);

                constexpr ::std::size_t ring_sz{value_range_end - value_range_begin};
                static_assert(ring_sz != 0uz);
                constexpr ::std::size_t v2_pos{curr_value_stack_top};
                constexpr ::std::size_t v1_pos{details::ring_next_pos(v2_pos, value_range_begin, value_range_end)};

                ValueT const v2{get_curr_val_from_operand_stack_top<CompileOption, ValueT, v2_pos>(type...)};
                ValueT v1{};  // init
                if constexpr(ring_sz >= 2uz) { v1 = get_curr_val_from_operand_stack_top<CompileOption, ValueT, v1_pos>(type...); }
                else
                {
                    static_assert(ring_sz == 1uz);
                    // Ring too small to hold both values: `v1` is the top of the operand stack memory.
                    v1 = get_curr_val_from_operand_stack_cache<ValueT>(type...);
                }
                ValueT const out{cond != wasm_i32{0} ? v1 : v2};
                details::set_curr_val_to_stacktop_cache<CompileOption, ValueT, v1_pos>(out, type...);
            }
            else
            {
                ValueT const v2{get_curr_val_from_operand_stack_cache<ValueT>(type...)};
                ValueT const v1{get_curr_val_from_operand_stack_cache<ValueT>(type...)};

                ValueT const out{cond != wasm_i32{0} ? v1 : v2};
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `select` opcode (non-tail-call/byref): selects between two operands based on an i32 condition (operand-stack only).
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching).
    /// - `type[0]` layout: `[opfunc_byref_ptr][next_opfunc_byref_ptr]...`.
    template <uwvm_interpreter_translate_option_t CompileOption, typename ValueT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_select_typed(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = stack_details::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i32 const cond{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ValueT const v2{get_curr_val_from_operand_stack_cache<ValueT>(typeref...)};
        ValueT const v1{get_curr_val_from_operand_stack_cache<ValueT>(typeref...)};

        ValueT const out{cond != wasm_i32{0} ? v1 : v2};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_select_i32(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_select_typed<CompileOption, stack_details::wasm_i32>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_select_i64(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_select_typed<CompileOption, stack_details::wasm_i64>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_select_f32(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_select_typed<CompileOption, stack_details::wasm_f32>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_select_f64(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_select_typed<CompileOption, stack_details::wasm_f64>(typeref...); }

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
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_by_currpos_impl_stack(::std::size_t pos) noexcept
            {
                static_assert(Curr < End);
                if(pos == Curr) { return OpWrapper::template fptr<CompileOption, Curr, Type...>(); }
                else
                {
                    if constexpr(Curr + 1uz < End)
                    {
                        return select_stacktop_fptr_by_currpos_impl_stack<CompileOption, Curr + 1uz, End, OpWrapper, Type...>(pos);
                    }
                    else
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t I32Curr,
                      ::std::size_t I32End,
                      ::std::size_t ValCurr,
                      ::std::size_t ValEnd,
                      typename OpWrapper,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_by_currpos_impl_stack_2d(::std::size_t i32_pos,
                                                                                                              ::std::size_t val_pos) noexcept
            {
                static_assert(I32Curr < I32End);
                static_assert(ValCurr < ValEnd);

                if(i32_pos == I32Curr)
                {
                    if(val_pos == ValCurr) { return OpWrapper::template fptr<CompileOption, I32Curr, ValCurr, Type...>(); }
                    else
                    {
                        if constexpr(ValCurr + 1uz < ValEnd)
                        {
                            return select_stacktop_fptr_by_currpos_impl_stack_2d<CompileOption, I32Curr, I32End, ValCurr + 1uz, ValEnd, OpWrapper, Type...>(
                                i32_pos,
                                val_pos);
                        }
                        else
                        {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                            ::fast_io::fast_terminate();
                        }
                    }
                }
                else
                {
                    if constexpr(I32Curr + 1uz < I32End)
                    {
                        return select_stacktop_fptr_by_currpos_impl_stack_2d<CompileOption, I32Curr + 1uz, I32End, ValCurr, ValEnd, OpWrapper, Type...>(
                            i32_pos,
                            val_pos);
                    }
                    else
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            struct drop_i32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_drop_typed<Opt, stack_details::wasm_i32, Pos, Type...>; }
            };

            struct drop_i64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_drop_typed<Opt, stack_details::wasm_i64, Pos, Type...>; }
            };

            struct drop_f32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_drop_typed<Opt, stack_details::wasm_f32, Pos, Type...>; }
            };

            struct drop_f64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_drop_typed<Opt, stack_details::wasm_f64, Pos, Type...>; }
            };

            struct select_i32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_select_typed<Opt, stack_details::wasm_i32, Pos, Pos, Type...>; }
            };

            struct select_i64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_select_typed<Opt, stack_details::wasm_i64, Pos, Pos, Type...>; }
            };

            struct select_i64_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_select_typed<Opt, stack_details::wasm_i64, I32Pos, I64Pos, Type...>; }
            };

            struct select_i64_op_value_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_select_typed<Opt, stack_details::wasm_i64, 0uz, I64Pos, Type...>; }
            };

            struct select_f32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_select_typed<Opt, stack_details::wasm_f32, Pos, Pos, Type...>; }
            };

            struct select_f32_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, ::std::size_t F32Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_select_typed<Opt, stack_details::wasm_f32, I32Pos, F32Pos, Type...>; }
            };

            struct select_f32_op_value_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F32Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_select_typed<Opt, stack_details::wasm_f32, 0uz, F32Pos, Type...>; }
            };

            struct select_f64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_select_typed<Opt, stack_details::wasm_f64, Pos, Pos, Type...>; }
            };

            struct select_f64_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, ::std::size_t F64Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_select_typed<Opt, stack_details::wasm_f64, I32Pos, F64Pos, Type...>; }
            };

            struct select_f64_op_value_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F64Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_select_typed<Opt, stack_details::wasm_f64, 0uz, F64Pos, Type...>; }
            };
        }  // namespace details

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_drop_i32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            (void)curr_stacktop;
            return uwvmint_drop_typed<CompileOption, stack_details::wasm_i32, SIZE_MAX, Type...>;
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_drop_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_drop_i32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_drop_i32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_drop_i32<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_drop_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_drop_i32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_drop_i64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            (void)curr_stacktop;
            return uwvmint_drop_typed<CompileOption, stack_details::wasm_i64, SIZE_MAX, Type...>;
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_drop_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_drop_i64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_drop_i64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_drop_i64<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_drop_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_drop_i64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_drop_f32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            (void)curr_stacktop;
            return uwvmint_drop_typed<CompileOption, stack_details::wasm_f32, SIZE_MAX, Type...>;
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_drop_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_drop_f32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_drop_f32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_drop_f32<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_drop_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_drop_f32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_drop_f64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            (void)curr_stacktop;
            return uwvmint_drop_typed<CompileOption, stack_details::wasm_f64, SIZE_MAX, Type...>;
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_drop_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_drop_f64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_drop_f64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_drop_f64<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_drop_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_drop_f64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_select_i32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_stack<CompileOption,
                                                                           CompileOption.i32_stack_top_begin_pos,
                                                                           CompileOption.i32_stack_top_end_pos,
                                                                           details::select_i32_op,
                                                                           Type...>(curr_stacktop.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_select_typed<CompileOption, stack_details::wasm_i32, 0uz, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_select_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_select_i32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_select_i32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_select_i32<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_select_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_select_i32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_select_i64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
                {
                    if constexpr(CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                 CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos)
                    {
                        return details::select_stacktop_fptr_by_currpos_impl_stack<CompileOption,
                                                                                   CompileOption.i32_stack_top_begin_pos,
                                                                                   CompileOption.i32_stack_top_end_pos,
                                                                                   details::select_i64_op,
                                                                                   Type...>(curr_stacktop.i32_stack_top_curr_pos);
                    }
                    else
                    {
                        return details::select_stacktop_fptr_by_currpos_impl_stack_2d<CompileOption,
                                                                                      CompileOption.i32_stack_top_begin_pos,
                                                                                      CompileOption.i32_stack_top_end_pos,
                                                                                      CompileOption.i64_stack_top_begin_pos,
                                                                                      CompileOption.i64_stack_top_end_pos,
                                                                                      details::select_i64_op_2d,
                                                                                      Type...>(curr_stacktop.i32_stack_top_curr_pos,
                                                                                               curr_stacktop.i64_stack_top_curr_pos);
                    }
                }
                else
                {
                    // ValueT has no stack-top cache; still dispatch on the i32 condition position.
                    return details::select_stacktop_fptr_by_currpos_impl_stack<CompileOption,
                                                                               CompileOption.i32_stack_top_begin_pos,
                                                                               CompileOption.i32_stack_top_end_pos,
                                                                               details::select_i64_op,
                                                                               Type...>(curr_stacktop.i32_stack_top_curr_pos);
                }
            }
            else
            {
                if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
                {
                    // No i32 cache, but ValueT is cached: dispatch on the ValueT ring position.
                    return details::select_stacktop_fptr_by_currpos_impl_stack<CompileOption,
                                                                               CompileOption.i64_stack_top_begin_pos,
                                                                               CompileOption.i64_stack_top_end_pos,
                                                                               details::select_i64_op_value_only,
                                                                               Type...>(curr_stacktop.i64_stack_top_curr_pos);
                }
                else
                {
                    return uwvmint_select_typed<CompileOption, stack_details::wasm_i64, 0uz, 0uz, Type...>;
                }
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_select_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_select_i64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_select_i64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_select_i64<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_select_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_select_i64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_select_f32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
                {
                    if constexpr(CompileOption.i32_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                 CompileOption.i32_stack_top_end_pos == CompileOption.f32_stack_top_end_pos)
                    {
                        return details::select_stacktop_fptr_by_currpos_impl_stack<CompileOption,
                                                                                   CompileOption.i32_stack_top_begin_pos,
                                                                                   CompileOption.i32_stack_top_end_pos,
                                                                                   details::select_f32_op,
                                                                                   Type...>(curr_stacktop.i32_stack_top_curr_pos);
                    }
                    else
                    {
                        return details::select_stacktop_fptr_by_currpos_impl_stack_2d<CompileOption,
                                                                                      CompileOption.i32_stack_top_begin_pos,
                                                                                      CompileOption.i32_stack_top_end_pos,
                                                                                      CompileOption.f32_stack_top_begin_pos,
                                                                                      CompileOption.f32_stack_top_end_pos,
                                                                                      details::select_f32_op_2d,
                                                                                      Type...>(curr_stacktop.i32_stack_top_curr_pos,
                                                                                               curr_stacktop.f32_stack_top_curr_pos);
                    }
                }
                else
                {
                    // ValueT has no stack-top cache; still dispatch on the i32 condition position.
                    return details::select_stacktop_fptr_by_currpos_impl_stack<CompileOption,
                                                                               CompileOption.i32_stack_top_begin_pos,
                                                                               CompileOption.i32_stack_top_end_pos,
                                                                               details::select_f32_op,
                                                                               Type...>(curr_stacktop.i32_stack_top_curr_pos);
                }
            }
            else
            {
                if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
                {
                    return details::select_stacktop_fptr_by_currpos_impl_stack<CompileOption,
                                                                               CompileOption.f32_stack_top_begin_pos,
                                                                               CompileOption.f32_stack_top_end_pos,
                                                                               details::select_f32_op_value_only,
                                                                               Type...>(curr_stacktop.f32_stack_top_curr_pos);
                }
                else
                {
                    return uwvmint_select_typed<CompileOption, stack_details::wasm_f32, 0uz, 0uz, Type...>;
                }
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_select_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_select_f32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_select_f32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_select_f32<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_select_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_select_f32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_select_f64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
                {
                    if constexpr(CompileOption.i32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                 CompileOption.i32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos)
                    {
                        return details::select_stacktop_fptr_by_currpos_impl_stack<CompileOption,
                                                                                   CompileOption.i32_stack_top_begin_pos,
                                                                                   CompileOption.i32_stack_top_end_pos,
                                                                                   details::select_f64_op,
                                                                                   Type...>(curr_stacktop.i32_stack_top_curr_pos);
                    }
                    else
                    {
                        return details::select_stacktop_fptr_by_currpos_impl_stack_2d<CompileOption,
                                                                                      CompileOption.i32_stack_top_begin_pos,
                                                                                      CompileOption.i32_stack_top_end_pos,
                                                                                      CompileOption.f64_stack_top_begin_pos,
                                                                                      CompileOption.f64_stack_top_end_pos,
                                                                                      details::select_f64_op_2d,
                                                                                      Type...>(curr_stacktop.i32_stack_top_curr_pos,
                                                                                               curr_stacktop.f64_stack_top_curr_pos);
                    }
                }
                else
                {
                    // ValueT has no stack-top cache; still dispatch on the i32 condition position.
                    return details::select_stacktop_fptr_by_currpos_impl_stack<CompileOption,
                                                                               CompileOption.i32_stack_top_begin_pos,
                                                                               CompileOption.i32_stack_top_end_pos,
                                                                               details::select_f64_op,
                                                                               Type...>(curr_stacktop.i32_stack_top_curr_pos);
                }
            }
            else
            {
                if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
                {
                    return details::select_stacktop_fptr_by_currpos_impl_stack<CompileOption,
                                                                               CompileOption.f64_stack_top_begin_pos,
                                                                               CompileOption.f64_stack_top_end_pos,
                                                                               details::select_f64_op_value_only,
                                                                               Type...>(curr_stacktop.f64_stack_top_curr_pos);
                }
                else
                {
                    return uwvmint_select_typed<CompileOption, stack_details::wasm_f64, 0uz, 0uz, Type...>;
                }
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_select_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_select_f64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_select_f64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_select_f64<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_select_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_select_f64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
