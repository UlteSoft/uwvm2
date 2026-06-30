/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
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
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/object/impl.h>
# include "define.h"
# include "register_ring.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
# if !(__cpp_pack_indexing >= 202311L)
#  error "UWVM requires at least C++26 standard compiler. See https://en.cppreference.com/w/cpp/feature_test#cpp_pack_indexing"
# endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    namespace variable_details
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_v128 = ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128;
        using wasm_funcref = ::uwvm2::object::global::wasm_funcref_t;
        using wasm_externref = ::uwvm2::object::global::wasm_externref_t;

        using local_offset_t = ::std::size_t;
        using global_storage_t = ::uwvm2::object::global::wasm_global_storage_t;

        template <typename T>
        UWVM_ALWAYS_INLINE inline constexpr T read_imm(::std::byte const*& ip) noexcept
        {
            T v;  // no init
            ::std::memcpy(::std::addressof(v), ip, sizeof(v));
            ip += sizeof(v);
            return v;
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval bool stacktop_enabled_for() noexcept
        {
            if constexpr(::std::same_as<OperandT, wasm_i32>) { return CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_i64>) { return CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f32>) { return CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f64>) { return CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_v128>)
            {
                return CompileOption.v128_stack_top_begin_pos != CompileOption.v128_stack_top_end_pos;
            }
            else
            {
                return false;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval ::std::size_t range_begin() noexcept
        {
            if constexpr(::std::same_as<OperandT, wasm_i32>) { return CompileOption.i32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_i64>) { return CompileOption.i64_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f32>) { return CompileOption.f32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f64>) { return CompileOption.f64_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_v128>) { return CompileOption.v128_stack_top_begin_pos; }
            else
            {
                return SIZE_MAX;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval ::std::size_t range_end() noexcept
        {
            if constexpr(::std::same_as<OperandT, wasm_i32>) { return CompileOption.i32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_i64>) { return CompileOption.i64_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f32>) { return CompileOption.f32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f64>) { return CompileOption.f64_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_v128>) { return CompileOption.v128_stack_top_end_pos; }
            else
            {
                return SIZE_MAX;
            }
        }

        template <typename GlobalT>
        UWVM_ALWAYS_INLINE inline constexpr GlobalT load_global(global_storage_t* global_p) noexcept
        {
            GlobalT v;  // no init
            if constexpr(::std::same_as<GlobalT, wasm_i32>) { v = global_p->storage.i32; }
            else if constexpr(::std::same_as<GlobalT, wasm_i64>) { v = global_p->storage.i64; }
            else if constexpr(::std::same_as<GlobalT, wasm_f32>) { v = global_p->storage.f32; }
            else if constexpr(::std::same_as<GlobalT, wasm_f64>) { v = global_p->storage.f64; }
            else if constexpr(::std::same_as<GlobalT, wasm_v128>) { v = global_p->storage.v128; }
            else if constexpr(::std::same_as<GlobalT, wasm_funcref> || ::std::same_as<GlobalT, wasm_externref>) { v.ref = global_p->storage.ref; }
            else { static_assert(::std::same_as<GlobalT, wasm_i32>); }
            return v;
        }

        template <typename GlobalT>
        UWVM_ALWAYS_INLINE inline constexpr void store_global(global_storage_t* global_p, GlobalT const& v) noexcept
        {
            if constexpr(::std::same_as<GlobalT, wasm_i32>) { global_p->storage.i32 = v; }
            else if constexpr(::std::same_as<GlobalT, wasm_i64>) { global_p->storage.i64 = v; }
            else if constexpr(::std::same_as<GlobalT, wasm_f32>) { global_p->storage.f32 = v; }
            else if constexpr(::std::same_as<GlobalT, wasm_f64>) { global_p->storage.f64 = v; }
            else if constexpr(::std::same_as<GlobalT, wasm_v128>) { global_p->storage.v128 = v; }
            else if constexpr(::std::same_as<GlobalT, wasm_funcref> || ::std::same_as<GlobalT, wasm_externref>) { global_p->storage.ref = v.ref; }
            else { static_assert(::std::same_as<GlobalT, wasm_i32>); }
        }
    }  // namespace variable_details

    // ========================
    // local.get / local.set / local.tee
    // ========================

    /// @brief `local.get` opcode (tail-call): pushes a local value onto the operand stack.
    /// @details
    /// - Stack-top optimization: supported for the local value type.
    /// - `type[0]` layout: `[opfunc_ptr][local_offset:local_offset_t][next_opfunc_ptr]`.
    /// @note `local_offset` is a byte offset from `type...[2u]` (local base) and is provided by the translator.
    template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_local_get_typed(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        variable_details::local_offset_t const off{variable_details::read_imm<variable_details::local_offset_t>(type...[0])};

        LocalT v;  // no init
        ::std::memcpy(::std::addressof(v), type...[2u] + off, sizeof(v));

        if constexpr(variable_details::stacktop_enabled_for<CompileOption, LocalT>())
        {
            constexpr ::std::size_t range_begin{variable_details::range_begin<CompileOption, LocalT>()};
            constexpr ::std::size_t range_end{variable_details::range_end<CompileOption, LocalT>()};
            static_assert(sizeof...(Type) >= range_end);
            static_assert(range_begin <= curr_stack_top && curr_stack_top < range_end);

            constexpr ::std::size_t new_pos{details::ring_prev_pos(curr_stack_top, range_begin, range_end)};
            details::set_curr_val_to_stacktop_cache<CompileOption, LocalT, new_pos>(v, type...);
        }
        else
        {
            ::std::memcpy(type...[1u], ::std::addressof(v), sizeof(v));
            type...[1u] += sizeof(v);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `local.set` opcode (tail-call): pops a value and writes it to a local.
    /// @details
    /// - Stack-top optimization: supported for the local value type.
    /// - `type[0]` layout: `[opfunc_ptr][local_offset:local_offset_t][next_opfunc_ptr]`.
    template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_local_set_typed(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        variable_details::local_offset_t const off{variable_details::read_imm<variable_details::local_offset_t>(type...[0])};

        LocalT v;  // no init
        if constexpr(variable_details::stacktop_enabled_for<CompileOption, LocalT>())
        {
            constexpr ::std::size_t range_begin{variable_details::range_begin<CompileOption, LocalT>()};
            constexpr ::std::size_t range_end{variable_details::range_end<CompileOption, LocalT>()};
            static_assert(range_begin <= curr_stack_top && curr_stack_top < range_end);
            v = get_curr_val_from_operand_stack_top<CompileOption, LocalT, curr_stack_top>(type...);
        }
        else
        {
            v = get_curr_val_from_operand_stack_cache<LocalT>(type...);
        }

        ::std::memcpy(type...[2u] + off, ::std::addressof(v), sizeof(v));

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `local.tee` opcode (tail-call): writes the current top value to a local without popping it.
    /// @details
    /// - Stack-top optimization: supported for the local value type.
    /// - `type[0]` layout: `[opfunc_ptr][local_offset:local_offset_t][next_opfunc_ptr]`.
    template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_local_tee_typed(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        variable_details::local_offset_t const off{variable_details::read_imm<variable_details::local_offset_t>(type...[0])};

        LocalT v;  // no init
        if constexpr(variable_details::stacktop_enabled_for<CompileOption, LocalT>())
        {
            constexpr ::std::size_t range_begin{variable_details::range_begin<CompileOption, LocalT>()};
            constexpr ::std::size_t range_end{variable_details::range_end<CompileOption, LocalT>()};
            static_assert(range_begin <= curr_stack_top && curr_stack_top < range_end);
            v = get_curr_val_from_operand_stack_top<CompileOption, LocalT, curr_stack_top>(type...);
        }
        else
        {
            ::std::memcpy(::std::addressof(v), type...[1u] - sizeof(v), sizeof(v));
        }

        ::std::memcpy(type...[2u] + off, ::std::addressof(v), sizeof(v));

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    // Direct fptr helpers for local.get
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_get_i32_ptr() noexcept
    { return uwvmint_local_get_typed<CompileOption, variable_details::wasm_i32, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_get_i64_ptr() noexcept
    { return uwvmint_local_get_typed<CompileOption, variable_details::wasm_i64, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_get_f32_ptr() noexcept
    { return uwvmint_local_get_typed<CompileOption, variable_details::wasm_f32, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_get_f64_ptr() noexcept
    { return uwvmint_local_get_typed<CompileOption, variable_details::wasm_f64, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_get_v128_ptr() noexcept
    { return uwvmint_local_get_typed<CompileOption, variable_details::wasm_v128, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_get_funcref_ptr() noexcept
    { return uwvmint_local_get_typed<CompileOption, variable_details::wasm_funcref, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_get_externref_ptr() noexcept
    { return uwvmint_local_get_typed<CompileOption, variable_details::wasm_externref, curr_stack_top, Type...>; }

    // Direct fptr helpers for local.set
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_set_i32_ptr() noexcept
    { return uwvmint_local_set_typed<CompileOption, variable_details::wasm_i32, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_set_i64_ptr() noexcept
    { return uwvmint_local_set_typed<CompileOption, variable_details::wasm_i64, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_set_f32_ptr() noexcept
    { return uwvmint_local_set_typed<CompileOption, variable_details::wasm_f32, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_set_f64_ptr() noexcept
    { return uwvmint_local_set_typed<CompileOption, variable_details::wasm_f64, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_set_v128_ptr() noexcept
    { return uwvmint_local_set_typed<CompileOption, variable_details::wasm_v128, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_set_funcref_ptr() noexcept
    { return uwvmint_local_set_typed<CompileOption, variable_details::wasm_funcref, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_set_externref_ptr() noexcept
    { return uwvmint_local_set_typed<CompileOption, variable_details::wasm_externref, curr_stack_top, Type...>; }

    // Direct fptr helpers for local.tee
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_tee_i32_ptr() noexcept
    { return uwvmint_local_tee_typed<CompileOption, variable_details::wasm_i32, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_tee_i64_ptr() noexcept
    { return uwvmint_local_tee_typed<CompileOption, variable_details::wasm_i64, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_tee_f32_ptr() noexcept
    { return uwvmint_local_tee_typed<CompileOption, variable_details::wasm_f32, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_tee_f64_ptr() noexcept
    { return uwvmint_local_tee_typed<CompileOption, variable_details::wasm_f64, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_tee_v128_ptr() noexcept
    { return uwvmint_local_tee_typed<CompileOption, variable_details::wasm_v128, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_tee_funcref_ptr() noexcept
    { return uwvmint_local_tee_typed<CompileOption, variable_details::wasm_funcref, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_local_tee_externref_ptr() noexcept
    { return uwvmint_local_tee_typed<CompileOption, variable_details::wasm_externref, curr_stack_top, Type...>; }

    // Byref (non-tail-call) variants

    template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_local_get_typed(TypeRef & ... typeref) UWVM_THROWS
    {
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

        variable_details::local_offset_t const off{variable_details::read_imm<variable_details::local_offset_t>(typeref...[0])};

        LocalT v;  // no init
        ::std::memcpy(::std::addressof(v), typeref...[2u] + off, sizeof(v));

        ::std::memcpy(typeref...[1u], ::std::addressof(v), sizeof(v));
        typeref...[1u] += sizeof(v);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_local_set_typed(TypeRef & ... typeref) UWVM_THROWS
    {
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

        variable_details::local_offset_t const off{variable_details::read_imm<variable_details::local_offset_t>(typeref...[0])};

        LocalT const v{get_curr_val_from_operand_stack_cache<LocalT>(typeref...)};
        ::std::memcpy(typeref...[2u] + off, ::std::addressof(v), sizeof(v));
    }

    template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_local_tee_typed(TypeRef & ... typeref) UWVM_THROWS
    {
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

        variable_details::local_offset_t const off{variable_details::read_imm<variable_details::local_offset_t>(typeref...[0])};

        LocalT v;  // no init
        ::std::memcpy(::std::addressof(v), typeref...[1u] - sizeof(v), sizeof(v));
        ::std::memcpy(typeref...[2u] + off, ::std::addressof(v), sizeof(v));
    }

    // ========================
    // global.get / global.set
    // ========================

    /// @brief `global.get` opcode (tail-call): pushes a global value.
    /// @details
    /// - Stack-top optimization: supported for the global value type.
    /// - `type[0]` layout: `[opfunc_ptr][global_ptr:wasm_global_storage_t*][next_opfunc_ptr]`.
    template <uwvm_interpreter_translate_option_t CompileOption, typename GlobalT, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_global_get_typed(Type... type) UWVM_THROWS
    {
        using global_storage_t = ::uwvm2::object::global::wasm_global_storage_t;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        global_storage_t* global_p{variable_details::read_imm<global_storage_t*>(type...[0])};
        GlobalT const v{variable_details::load_global<GlobalT>(global_p)};

        if constexpr(variable_details::stacktop_enabled_for<CompileOption, GlobalT>())
        {
            constexpr ::std::size_t range_begin{variable_details::range_begin<CompileOption, GlobalT>()};
            constexpr ::std::size_t range_end{variable_details::range_end<CompileOption, GlobalT>()};
            static_assert(sizeof...(Type) >= range_end);
            static_assert(range_begin <= curr_stack_top && curr_stack_top < range_end);

            constexpr ::std::size_t new_pos{details::ring_prev_pos(curr_stack_top, range_begin, range_end)};
            details::set_curr_val_to_stacktop_cache<CompileOption, GlobalT, new_pos>(v, type...);
        }
        else
        {
            ::std::memcpy(type...[1u], ::std::addressof(v), sizeof(v));
            type...[1u] += sizeof(v);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `global.set` opcode (tail-call): pops a value and writes it to a global.
    /// @details
    /// - Stack-top optimization: supported for the global value type.
    /// - `type[0]` layout: `[opfunc_ptr][global_ptr:wasm_global_storage_t*][next_opfunc_ptr]`.
    template <uwvm_interpreter_translate_option_t CompileOption, typename GlobalT, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_global_set_typed(Type... type) UWVM_THROWS
    {
        using global_storage_t = ::uwvm2::object::global::wasm_global_storage_t;

        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        global_storage_t* global_p{variable_details::read_imm<global_storage_t*>(type...[0])};

        GlobalT v;  // no init
        if constexpr(variable_details::stacktop_enabled_for<CompileOption, GlobalT>())
        {
            constexpr ::std::size_t range_begin{variable_details::range_begin<CompileOption, GlobalT>()};
            constexpr ::std::size_t range_end{variable_details::range_end<CompileOption, GlobalT>()};
            static_assert(range_begin <= curr_stack_top && curr_stack_top < range_end);
            v = get_curr_val_from_operand_stack_top<CompileOption, GlobalT, curr_stack_top>(type...);
        }
        else
        {
            v = get_curr_val_from_operand_stack_cache<GlobalT>(type...);
        }

        variable_details::store_global(global_p, v);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    // Direct fptr helpers for global.get/global.set (avoid wrapper-call stack growth in tail-call interpreter mode)
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_global_get_i32_ptr() noexcept
    { return uwvmint_global_get_typed<CompileOption, variable_details::wasm_i32, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_global_get_i64_ptr() noexcept
    { return uwvmint_global_get_typed<CompileOption, variable_details::wasm_i64, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_global_get_f32_ptr() noexcept
    { return uwvmint_global_get_typed<CompileOption, variable_details::wasm_f32, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_global_get_f64_ptr() noexcept
    { return uwvmint_global_get_typed<CompileOption, variable_details::wasm_f64, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_global_get_v128_ptr() noexcept
    { return uwvmint_global_get_typed<CompileOption, variable_details::wasm_v128, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_global_get_funcref_ptr() noexcept
    { return uwvmint_global_get_typed<CompileOption, variable_details::wasm_funcref, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_global_get_externref_ptr() noexcept
    { return uwvmint_global_get_typed<CompileOption, variable_details::wasm_externref, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_global_set_i32_ptr() noexcept
    { return uwvmint_global_set_typed<CompileOption, variable_details::wasm_i32, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_global_set_i64_ptr() noexcept
    { return uwvmint_global_set_typed<CompileOption, variable_details::wasm_i64, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_global_set_f32_ptr() noexcept
    { return uwvmint_global_set_typed<CompileOption, variable_details::wasm_f32, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_global_set_f64_ptr() noexcept
    { return uwvmint_global_set_typed<CompileOption, variable_details::wasm_f64, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_global_set_v128_ptr() noexcept
    { return uwvmint_global_set_typed<CompileOption, variable_details::wasm_v128, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_global_set_funcref_ptr() noexcept
    { return uwvmint_global_set_typed<CompileOption, variable_details::wasm_funcref, curr_stack_top, Type...>; }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    inline constexpr uwvm_interpreter_opfunc_t<Type...> uwvmint_global_set_externref_ptr() noexcept
    { return uwvmint_global_set_typed<CompileOption, variable_details::wasm_externref, curr_stack_top, Type...>; }

    // Byref variants for global.*

    template <uwvm_interpreter_translate_option_t CompileOption, typename GlobalT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_global_get_typed(TypeRef & ... typeref) UWVM_THROWS
    {
        using global_storage_t = ::uwvm2::object::global::wasm_global_storage_t;

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

        global_storage_t* global_p{variable_details::read_imm<global_storage_t*>(typeref...[0])};
        GlobalT const v{variable_details::load_global<GlobalT>(global_p)};

        ::std::memcpy(typeref...[1u], ::std::addressof(v), sizeof(v));
        typeref...[1u] += sizeof(v);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, typename GlobalT, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_global_set_typed(TypeRef & ... typeref) UWVM_THROWS
    {
        using global_storage_t = ::uwvm2::object::global::wasm_global_storage_t;

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

        global_storage_t* global_p{variable_details::read_imm<global_storage_t*>(typeref...[0])};
        GlobalT const v{get_curr_val_from_operand_stack_cache<GlobalT>(typeref...)};

        variable_details::store_global(global_p, v);
    }

    // ========================
    // translate helpers
    // ========================

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
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_by_currpos_impl_variable(::std::size_t pos) noexcept
            {
                static_assert(Curr < End);
                if(pos == Curr) { return OpWrapper::template fptr<CompileOption, Curr, Type...>(); }
                else
                {
                    if constexpr(Curr + 1uz < End)
                    {
                        return select_stacktop_fptr_by_currpos_impl_variable<CompileOption, Curr + 1uz, End, OpWrapper, Type...>(pos);
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

            struct local_get_i32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_get_i32_ptr<Opt, Pos, Type...>(); }
            };

            struct local_get_i64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_get_i64_ptr<Opt, Pos, Type...>(); }
            };

            struct local_get_f32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_get_f32_ptr<Opt, Pos, Type...>(); }
            };

            struct local_get_f64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_get_f64_ptr<Opt, Pos, Type...>(); }
            };

            struct local_set_i32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_set_i32_ptr<Opt, Pos, Type...>(); }
            };

            struct local_set_i64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_set_i64_ptr<Opt, Pos, Type...>(); }
            };

            struct local_set_f32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_set_f32_ptr<Opt, Pos, Type...>(); }
            };

            struct local_set_f64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_set_f64_ptr<Opt, Pos, Type...>(); }
            };

            struct local_tee_i32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_tee_i32_ptr<Opt, Pos, Type...>(); }
            };

            struct local_tee_i64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_tee_i64_ptr<Opt, Pos, Type...>(); }
            };

            struct local_tee_f32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_tee_f32_ptr<Opt, Pos, Type...>(); }
            };

            struct local_tee_f64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_tee_f64_ptr<Opt, Pos, Type...>(); }
            };

            struct global_get_i32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_global_get_i32_ptr<Opt, Pos, Type...>(); }
            };

            struct global_get_i64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_global_get_i64_ptr<Opt, Pos, Type...>(); }
            };

            struct global_get_f32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_global_get_f32_ptr<Opt, Pos, Type...>(); }
            };

            struct global_get_f64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_global_get_f64_ptr<Opt, Pos, Type...>(); }
            };

            struct global_set_i32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_global_set_i32_ptr<Opt, Pos, Type...>(); }
            };

            struct global_set_i64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_global_set_i64_ptr<Opt, Pos, Type...>(); }
            };

            struct global_set_f32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_global_set_f32_ptr<Opt, Pos, Type...>(); }
            };

            struct global_set_f64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_global_set_f64_ptr<Opt, Pos, Type...>(); }
            };
        }  // namespace details

        // local.* fptrs (tail-call)

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_get_i32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.i32_stack_top_begin_pos,
                                                                              CompileOption.i32_stack_top_end_pos,
                                                                              details::local_get_i32_op,
                                                                              Type...>(curr.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_local_get_i32_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_get_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_get_i32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_get_i64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.i64_stack_top_begin_pos,
                                                                              CompileOption.i64_stack_top_end_pos,
                                                                              details::local_get_i64_op,
                                                                              Type...>(curr.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_local_get_i64_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_get_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_get_i64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_get_f32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.f32_stack_top_begin_pos,
                                                                              CompileOption.f32_stack_top_end_pos,
                                                                              details::local_get_f32_op,
                                                                              Type...>(curr.f32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_local_get_f32_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_get_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_get_f32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_get_f64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.f64_stack_top_begin_pos,
                                                                              CompileOption.f64_stack_top_end_pos,
                                                                              details::local_get_f64_op,
                                                                              Type...>(curr.f64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_local_get_f64_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_get_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_get_f64_fptr<CompileOption, TypeInTuple...>(curr); }

        // Similar helpers for local.set/local.tee/global.get/global.set are intentionally symmetrical.

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_set_i32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.i32_stack_top_begin_pos,
                                                                              CompileOption.i32_stack_top_end_pos,
                                                                              details::local_set_i32_op,
                                                                              Type...>(curr.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_local_set_i32_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_set_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_set_i32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_set_i64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.i64_stack_top_begin_pos,
                                                                              CompileOption.i64_stack_top_end_pos,
                                                                              details::local_set_i64_op,
                                                                              Type...>(curr.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_local_set_i64_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_set_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_set_i64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_set_f32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.f32_stack_top_begin_pos,
                                                                              CompileOption.f32_stack_top_end_pos,
                                                                              details::local_set_f32_op,
                                                                              Type...>(curr.f32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_local_set_f32_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_set_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_set_f32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_set_f64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.f64_stack_top_begin_pos,
                                                                              CompileOption.f64_stack_top_end_pos,
                                                                              details::local_set_f64_op,
                                                                              Type...>(curr.f64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_local_set_f64_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_set_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_set_f64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_tee_i32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.i32_stack_top_begin_pos,
                                                                              CompileOption.i32_stack_top_end_pos,
                                                                              details::local_tee_i32_op,
                                                                              Type...>(curr.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_local_tee_i32_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_tee_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_tee_i32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_tee_i64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.i64_stack_top_begin_pos,
                                                                              CompileOption.i64_stack_top_end_pos,
                                                                              details::local_tee_i64_op,
                                                                              Type...>(curr.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_local_tee_i64_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_tee_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_tee_i64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_tee_f32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.f32_stack_top_begin_pos,
                                                                              CompileOption.f32_stack_top_end_pos,
                                                                              details::local_tee_f32_op,
                                                                              Type...>(curr.f32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_local_tee_f32_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_tee_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_tee_f32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_tee_f64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.f64_stack_top_begin_pos,
                                                                              CompileOption.f64_stack_top_end_pos,
                                                                              details::local_tee_f64_op,
                                                                              Type...>(curr.f64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_local_tee_f64_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_tee_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_tee_f64_fptr<CompileOption, TypeInTuple...>(curr); }

        // global.* fptrs (tail-call)

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_global_get_i32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.i32_stack_top_begin_pos,
                                                                              CompileOption.i32_stack_top_end_pos,
                                                                              details::global_get_i32_op,
                                                                              Type...>(curr.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_global_get_i32_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_get_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_get_i32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_global_get_i64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.i64_stack_top_begin_pos,
                                                                              CompileOption.i64_stack_top_end_pos,
                                                                              details::global_get_i64_op,
                                                                              Type...>(curr.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_global_get_i64_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_get_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_get_i64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_global_get_f32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.f32_stack_top_begin_pos,
                                                                              CompileOption.f32_stack_top_end_pos,
                                                                              details::global_get_f32_op,
                                                                              Type...>(curr.f32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_global_get_f32_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_get_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_get_f32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_global_get_f64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.f64_stack_top_begin_pos,
                                                                              CompileOption.f64_stack_top_end_pos,
                                                                              details::global_get_f64_op,
                                                                              Type...>(curr.f64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_global_get_f64_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_get_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_get_f64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_global_set_i32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.i32_stack_top_begin_pos,
                                                                              CompileOption.i32_stack_top_end_pos,
                                                                              details::global_set_i32_op,
                                                                              Type...>(curr.i32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_global_set_i32_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_set_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_set_i32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_global_set_i64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.i64_stack_top_begin_pos,
                                                                              CompileOption.i64_stack_top_end_pos,
                                                                              details::global_set_i64_op,
                                                                              Type...>(curr.i64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_global_set_i64_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_set_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_set_i64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_global_set_f32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.f32_stack_top_begin_pos,
                                                                              CompileOption.f32_stack_top_end_pos,
                                                                              details::global_set_f32_op,
                                                                              Type...>(curr.f32_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_global_set_f32_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_set_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_set_f32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_global_set_f64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                return details::select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                              CompileOption.f64_stack_top_begin_pos,
                                                                              CompileOption.f64_stack_top_end_pos,
                                                                              details::global_set_f64_op,
                                                                              Type...>(curr.f64_stack_top_curr_pos);
            }
            else
            {
                return uwvmint_global_set_f64_ptr<CompileOption, 0uz, Type...>();
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_set_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_set_f64_fptr<CompileOption, TypeInTuple...>(curr); }

        // Byref fptrs: no currpos selection required.

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_get_i32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_get_typed<CompileOption, variable_details::wasm_i32, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_get_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_get_i32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_get_i64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_get_typed<CompileOption, variable_details::wasm_i64, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_get_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_get_i64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_get_f32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_get_typed<CompileOption, variable_details::wasm_f32, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_get_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_get_f32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_get_f64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_get_typed<CompileOption, variable_details::wasm_f64, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_get_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_get_f64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_set_i32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_set_typed<CompileOption, variable_details::wasm_i32, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_set_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_set_i32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_set_i64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_set_typed<CompileOption, variable_details::wasm_i64, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_set_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_set_i64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_set_f32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_set_typed<CompileOption, variable_details::wasm_f32, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_set_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_set_f32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_set_f64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_set_typed<CompileOption, variable_details::wasm_f64, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_set_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_set_f64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_tee_i32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_tee_typed<CompileOption, variable_details::wasm_i32, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_tee_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_tee_i32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_tee_i64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_tee_typed<CompileOption, variable_details::wasm_i64, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_tee_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_tee_i64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_tee_f32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_tee_typed<CompileOption, variable_details::wasm_f32, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_tee_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_tee_f32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_tee_f64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_tee_typed<CompileOption, variable_details::wasm_f64, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_tee_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_tee_f64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_global_get_i32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_global_get_typed<CompileOption, variable_details::wasm_i32, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_get_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_get_i32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_global_get_i64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_global_get_typed<CompileOption, variable_details::wasm_i64, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_get_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_get_i64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_global_get_f32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_global_get_typed<CompileOption, variable_details::wasm_f32, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_get_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_get_f32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_global_get_f64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_global_get_typed<CompileOption, variable_details::wasm_f64, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_get_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_get_f64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_global_set_i32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_global_set_typed<CompileOption, variable_details::wasm_i32, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_set_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_set_i32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_global_set_i64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_global_set_typed<CompileOption, variable_details::wasm_i64, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_set_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_set_i64_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_global_set_f32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_global_set_typed<CompileOption, variable_details::wasm_f32, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_set_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_set_f32_fptr<CompileOption, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_global_set_f64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_global_set_typed<CompileOption, variable_details::wasm_f64, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_set_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_set_f64_fptr<CompileOption, TypeInTuple...>(curr); }

        namespace details
        {
            template <typename LocalT>
            struct local_get_typed_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_get_typed<Opt, LocalT, Pos, Type...>; }
            };

            template <typename LocalT>
            struct local_set_typed_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_set_typed<Opt, LocalT, Pos, Type...>; }
            };

            template <typename LocalT>
            struct local_tee_typed_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_local_tee_typed<Opt, LocalT, Pos, Type...>; }
            };

            template <typename GlobalT>
            struct global_get_typed_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_global_get_typed<Opt, GlobalT, Pos, Type...>; }
            };

            template <typename GlobalT>
            struct global_set_typed_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_global_set_typed<Opt, GlobalT, Pos, Type...>; }
            };

            template <typename ValT>
            [[nodiscard]] inline constexpr ::std::size_t typed_stacktop_currpos(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
            {
                if constexpr(::std::same_as<ValT, variable_details::wasm_i32>) { return curr.i32_stack_top_curr_pos; }
                else if constexpr(::std::same_as<ValT, variable_details::wasm_i64>) { return curr.i64_stack_top_curr_pos; }
                else if constexpr(::std::same_as<ValT, variable_details::wasm_f32>) { return curr.f32_stack_top_curr_pos; }
                else if constexpr(::std::same_as<ValT, variable_details::wasm_f64>) { return curr.f64_stack_top_curr_pos; }
                else if constexpr(::std::same_as<ValT, variable_details::wasm_v128>) { return curr.v128_stack_top_curr_pos; }
                else { return SIZE_MAX; }
            }

            template <uwvm_interpreter_translate_option_t CompileOption, typename ValT, typename OpWrapper, uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> get_typed_stacktop_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
            {
                if constexpr(variable_details::stacktop_enabled_for<CompileOption, ValT>())
                {
                    return select_stacktop_fptr_by_currpos_impl_variable<CompileOption,
                                                                         variable_details::range_begin<CompileOption, ValT>(),
                                                                         variable_details::range_end<CompileOption, ValT>(),
                                                                         OpWrapper,
                                                                         Type...>(typed_stacktop_currpos<ValT>(curr));
                }
                else
                {
                    return OpWrapper::template fptr<CompileOption, 0uz, Type...>();
                }
            }
        }  // namespace details

        template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_get_typed_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return details::get_typed_stacktop_fptr<CompileOption, LocalT, details::local_get_typed_op<LocalT>, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_get_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_get_typed_fptr<CompileOption, LocalT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_set_typed_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return details::get_typed_stacktop_fptr<CompileOption, LocalT, details::local_set_typed_op<LocalT>, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_set_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_set_typed_fptr<CompileOption, LocalT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_local_tee_typed_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return details::get_typed_stacktop_fptr<CompileOption, LocalT, details::local_tee_typed_op<LocalT>, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_tee_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_tee_typed_fptr<CompileOption, LocalT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename GlobalT, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_global_get_typed_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return details::get_typed_stacktop_fptr<CompileOption, GlobalT, details::global_get_typed_op<GlobalT>, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename GlobalT, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_get_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_get_typed_fptr<CompileOption, GlobalT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename GlobalT, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_global_set_typed_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        { return details::get_typed_stacktop_fptr<CompileOption, GlobalT, details::global_set_typed_op<GlobalT>, Type...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename GlobalT, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_set_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_set_typed_fptr<CompileOption, GlobalT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_get_typed_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_get_typed<CompileOption, LocalT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_get_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_get_typed_fptr<CompileOption, LocalT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_set_typed_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_set_typed<CompileOption, LocalT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_set_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_set_typed_fptr<CompileOption, LocalT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_local_tee_typed_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_local_tee_typed<CompileOption, LocalT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_local_tee_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_local_tee_typed_fptr<CompileOption, LocalT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename GlobalT, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_global_get_typed_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_global_get_typed<CompileOption, GlobalT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename GlobalT, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_get_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_get_typed_fptr<CompileOption, GlobalT, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, typename GlobalT, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_global_set_typed_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_global_set_typed<CompileOption, GlobalT, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, typename GlobalT, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_global_set_typed_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_global_set_typed_fptr<CompileOption, GlobalT, TypeInTuple...>(curr); }
    }  // namespace translate
}
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
