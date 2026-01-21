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
# include <limits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/impl.h>
# include <uwvm2/object/impl.h>
# include "define.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    namespace details
    {
        inline consteval bool range_enabled(::std::size_t begin_pos, ::std::size_t end_pos) noexcept { return begin_pos != end_pos; }

        inline consteval bool in_range(::std::size_t pos, ::std::size_t begin_pos, ::std::size_t end_pos) noexcept
        { return range_enabled(begin_pos, end_pos) && begin_pos <= pos && pos < end_pos; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename ValType>
        inline consteval ::std::size_t stacktop_range_begin_pos() noexcept
        {
            if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>) { return CompileOption.i32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>) { return CompileOption.i64_stack_top_begin_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>) { return CompileOption.f32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>) { return CompileOption.f64_stack_top_begin_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>)
            {
                return CompileOption.v128_stack_top_begin_pos;
            }
            else
            {
                static_assert(is_uwvm_interpreter_valtype_supported<ValType>());
                return SIZE_MAX;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename ValType>
        inline consteval ::std::size_t stacktop_range_end_pos() noexcept
        {
            if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>) { return CompileOption.i32_stack_top_end_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>) { return CompileOption.i64_stack_top_end_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>) { return CompileOption.f32_stack_top_end_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>) { return CompileOption.f64_stack_top_end_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>)
            {
                return CompileOption.v128_stack_top_end_pos;
            }
            else
            {
                static_assert(is_uwvm_interpreter_valtype_supported<ValType>());
                return SIZE_MAX;
            }
        }

        template <typename ValType,
                  uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t WritePos,
                  ::std::size_t Remaining,
                  uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void spill_stacktop_desc_to_operand_stack(::std::byte*& write_ptr, TypeRef&... typeref) noexcept
        {
            static_assert(Remaining != 0uz);

            ValType const v{get_curr_val_from_operand_stack_top<CompileOption, ValType, WritePos>(typeref...)};
            write_ptr -= sizeof(ValType);
            ::std::memcpy(write_ptr, ::std::addressof(v), sizeof(ValType));

            if constexpr(Remaining > 1uz)
            {
                static_assert(WritePos != 0uz);
                spill_stacktop_desc_to_operand_stack<ValType, CompileOption, WritePos - 1uz, Remaining - 1uz>(write_ptr, typeref...);
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t StartPos,
                  ::std::size_t Count,
                  typename ValType,
                  ::std::size_t RangeBegin,
                  ::std::size_t RangeEnd,
                  uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void spill_stacktop_range_to_operand_stack(TypeRef&... typeref) noexcept
        {
            static_assert(Count != 0uz);
            static_assert(StartPos < RangeEnd);
            static_assert(RangeBegin <= StartPos);
            static_assert(StartPos - (Count - 1uz) >= RangeBegin);
            static_assert(Count <= (RangeEnd - RangeBegin));

            static_assert(sizeof...(TypeRef) >= 2uz);
            using stack_ptr_t = ::std::remove_cvref_t<TypeRef...[1u]>;
            static_assert(::std::same_as<stack_ptr_t, ::std::byte*>);

            typeref...[1u] += sizeof(ValType) * Count;

            ::std::byte* write_ptr{typeref...[1u]};
            spill_stacktop_desc_to_operand_stack<ValType, CompileOption, StartPos, Count>(write_ptr, typeref...);

            static_assert(::std::same_as<decltype(write_ptr), ::std::byte*>);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  ::std::size_t StartPos,
                  typename ValType,
                  typename... RestValType,
                  uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void spill_stacktop_desc_by_types_to_operand_stack(::std::byte*& write_ptr, TypeRef&... typeref) noexcept
        {
            static_assert(is_uwvm_interpreter_valtype_supported<ValType>());

            ValType const v{get_curr_val_from_operand_stack_top<CompileOption, ValType, StartPos>(typeref...)};
            write_ptr -= sizeof(ValType);
            ::std::memcpy(write_ptr, ::std::addressof(v), sizeof(ValType));

            if constexpr(sizeof...(RestValType) > 0uz)
            {
                static_assert(StartPos != 0uz);
                spill_stacktop_desc_by_types_to_operand_stack<CompileOption, StartPos - 1uz, RestValType...>(write_ptr, typeref...);
            }
        }
    }  // namespace details

    // Spill (materialize) a contiguous cached stack-top segment back into the operand stack.
    //
    // `StartPos`/`Count` are indices in the opfunc argument pack (i.e. within the stack-top cache slots).
    // The spill is processed in descending slot order: StartPos, StartPos-1, ..., StartPos-(Count-1).
    //
    // Compile-time constraints:
    // - StartPos must belong to exactly one enabled stack-top range (i32/i64/f32/f64/v128).
    // - Count must not exceed the range size and must not underflow the range begin.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StartPos, ::std::size_t Count, uwvm_int_stack_top_type... TypeRef>
    UWVM_ALWAYS_INLINE inline constexpr void spill_stacktop_to_operand_stack(TypeRef & ... typeref) noexcept
    {
        if constexpr(Count == 0uz) { return; }

        constexpr bool i32_hit{details::in_range(StartPos, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos)};
        constexpr bool i64_hit{details::in_range(StartPos, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos)};
        constexpr bool f32_hit{details::in_range(StartPos, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos)};
        constexpr bool f64_hit{details::in_range(StartPos, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos)};
        constexpr bool v128_hit{details::in_range(StartPos, CompileOption.v128_stack_top_begin_pos, CompileOption.v128_stack_top_end_pos)};

        constexpr ::std::size_t hit_count{static_cast<::std::size_t>(i32_hit) + static_cast<::std::size_t>(i64_hit) + static_cast<::std::size_t>(f32_hit) +
                                          static_cast<::std::size_t>(f64_hit) + static_cast<::std::size_t>(v128_hit)};

        static_assert(hit_count == 1uz,
                      "StartPos must belong to exactly one stack-top range; if your stack-top ranges are merged, use the typed spill API overload.");

        if constexpr(i32_hit)
        {
            details::spill_stacktop_range_to_operand_stack<CompileOption,
                                                           StartPos,
                                                           Count,
                                                           ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32,
                                                           CompileOption.i32_stack_top_begin_pos,
                                                           CompileOption.i32_stack_top_end_pos>(typeref...);
        }
        else if constexpr(i64_hit)
        {
            details::spill_stacktop_range_to_operand_stack<CompileOption,
                                                           StartPos,
                                                           Count,
                                                           ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64,
                                                           CompileOption.i64_stack_top_begin_pos,
                                                           CompileOption.i64_stack_top_end_pos>(typeref...);
        }
        else if constexpr(f32_hit)
        {
            details::spill_stacktop_range_to_operand_stack<CompileOption,
                                                           StartPos,
                                                           Count,
                                                           ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32,
                                                           CompileOption.f32_stack_top_begin_pos,
                                                           CompileOption.f32_stack_top_end_pos>(typeref...);
        }
        else if constexpr(f64_hit)
        {
            details::spill_stacktop_range_to_operand_stack<CompileOption,
                                                           StartPos,
                                                           Count,
                                                           ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64,
                                                           CompileOption.f64_stack_top_begin_pos,
                                                           CompileOption.f64_stack_top_end_pos>(typeref...);
        }
        else if constexpr(v128_hit)
        {
            details::spill_stacktop_range_to_operand_stack<CompileOption,
                                                           StartPos,
                                                           Count,
                                                           ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128,
                                                           CompileOption.v128_stack_top_begin_pos,
                                                           CompileOption.v128_stack_top_end_pos>(typeref...);
        }
    }

    // Typed spill: explicitly selects the value type to spill, which is required when stack-top ranges are merged.
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t StartPos,
              ::std::size_t Count,
              typename ValType,
              uwvm_int_stack_top_type... TypeRef>
    UWVM_ALWAYS_INLINE inline constexpr void spill_stacktop_to_operand_stack(TypeRef & ... typeref) noexcept
    {
        static_assert(details::is_uwvm_interpreter_valtype_supported<ValType>());
        if constexpr(Count == 0uz) { return; }

        constexpr ::std::size_t range_begin{details::stacktop_range_begin_pos<CompileOption, ValType>()};
        constexpr ::std::size_t range_end{details::stacktop_range_end_pos<CompileOption, ValType>()};
        static_assert(details::range_enabled(range_begin, range_end), "ValType stack-top range is disabled; nothing to spill.");

        details::spill_stacktop_range_to_operand_stack<CompileOption, StartPos, Count, ValType, range_begin, range_end>(typeref...);
    }

    // Typed spill (mixed): spill a contiguous segment with an explicit per-slot value type list.
    // The type list is in descending slot order: StartPos, StartPos-1, ..., StartPos-(N-1).
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StartPos, typename... ValType, uwvm_int_stack_top_type... TypeRef>
        requires (sizeof...(ValType) != 0uz)
    UWVM_ALWAYS_INLINE inline constexpr void spill_stacktop_to_operand_stack(TypeRef & ... typeref) noexcept
    {
        static_assert((details::is_uwvm_interpreter_valtype_supported<ValType>() && ...));
        static_assert(StartPos >= (sizeof...(ValType) - 1uz), "StartPos underflows the type list length.");

        static_assert(sizeof...(TypeRef) >= 2uz);
        using stack_ptr_t = ::std::remove_cvref_t<TypeRef...[1u]>;
        static_assert(::std::same_as<stack_ptr_t, ::std::byte*>);

        static_assert(
            (details::range_enabled(details::stacktop_range_begin_pos<CompileOption, ValType>(), details::stacktop_range_end_pos<CompileOption, ValType>()) &&
             ...),
            "All ValType stack-top ranges must be enabled.");

        constexpr ::std::size_t total_size{(sizeof(ValType) + ...)};
        typeref...[1u] += total_size;

        ::std::byte* write_ptr{typeref...[1u]};
        details::spill_stacktop_desc_by_types_to_operand_stack<CompileOption, StartPos, ValType...>(write_ptr, type...);

        static_assert(::std::same_as<decltype(write_ptr), ::std::byte*>);
    }

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t StartPos,
              ::std::size_t Count,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_stacktop_to_operand_stack(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        spill_stacktop_to_operand_stack<CompileOption, StartPos, Count, Type...>(type...);

        // curr_uwvmint_op ...
        // safe
        // ^^ type...[0]

        type...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>);

        // curr_uwvmint_op next_interpreter
        // safe
        //                 ^^ type...[0]

        ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));

        if constexpr(CompileOption.is_tail_call) { UWVM_MUSTTAIL return next_interpreter(type...); }
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
