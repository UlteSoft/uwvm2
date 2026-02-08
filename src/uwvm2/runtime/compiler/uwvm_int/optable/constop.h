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
    /// @brief `i32.const` opcode (tail-call): pushes an i32 immediate.
    /// @details
    /// - Stack-top optimization: supported when `CompileOption.i32_stack_top_begin_pos != i32_stack_top_end_pos`; the value is written into the i32 stack-top
    /// ring
    ///   (via `ring_prev_pos`) instead of the operand stack.
    /// - `type[0]` layout: `[opfunc_ptr][imm:i32][next_opfunc_ptr]` (loads `imm` and tail-calls the next opfunc).
    /// @note All loads from the bytecode stream use `memcpy` to avoid alignment assumptions.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_const(Type... type) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        wasm_i32 imm;  // no init
        ::std::memcpy(::std::addressof(imm), type...[0], sizeof(imm));
        type...[0] += sizeof(imm);

        if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
        {
            constexpr ::std::size_t range_begin{CompileOption.i32_stack_top_begin_pos};
            constexpr ::std::size_t range_end{CompileOption.i32_stack_top_end_pos};
            static_assert(sizeof...(Type) >= range_end);
            static_assert(range_begin <= curr_i32_stack_top && curr_i32_stack_top < range_end);

            constexpr ::std::size_t new_pos{details::ring_prev_pos(curr_i32_stack_top, range_begin, range_end)};
            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, new_pos>(imm, type...);
        }
        else
        {
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);

            ::std::memcpy(type...[1u], ::std::addressof(imm), sizeof(imm));
            type...[1u] += sizeof(imm);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));

        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `i64.const` opcode (tail-call): pushes an i64 immediate.
    /// @details
    /// - Stack-top optimization: supported when `CompileOption.i64_stack_top_begin_pos != i64_stack_top_end_pos`; writes into the i64 stack-top ring instead of
    /// the stack.
    /// - `type[0]` layout: `[opfunc_ptr][imm:i64][next_opfunc_ptr]`.
    /// @note The stack-top write uses `ring_prev_pos(curr_i64_stack_top, begin, end)` to model a push into the ring cache.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_i64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_const(Type... type) UWVM_THROWS
    {
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        wasm_i64 imm;  // no init
        ::std::memcpy(::std::addressof(imm), type...[0], sizeof(imm));
        type...[0] += sizeof(imm);

        if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
        {
            constexpr ::std::size_t range_begin{CompileOption.i64_stack_top_begin_pos};
            constexpr ::std::size_t range_end{CompileOption.i64_stack_top_end_pos};
            static_assert(sizeof...(Type) >= range_end);
            static_assert(range_begin <= curr_i64_stack_top && curr_i64_stack_top < range_end);

            constexpr ::std::size_t new_pos{details::ring_prev_pos(curr_i64_stack_top, range_begin, range_end)};
            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_pos>(imm, type...);
        }
        else
        {
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);

            ::std::memcpy(type...[1u], ::std::addressof(imm), sizeof(imm));
            type...[1u] += sizeof(imm);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));

        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `f32.const` opcode (tail-call): pushes an f32 immediate.
    /// @details
    /// - Stack-top optimization: supported when `CompileOption.f32_stack_top_begin_pos != f32_stack_top_end_pos`; writes into the f32 stack-top ring.
    /// - `type[0]` layout: `[opfunc_ptr][imm:f32][next_opfunc_ptr]`.
    /// @note When stack-top caching is disabled, the immediate is appended to the operand stack (`type...[1u]`).
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f32_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_const(Type... type) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        wasm_f32 imm;  // no init
        ::std::memcpy(::std::addressof(imm), type...[0], sizeof(imm));
        type...[0] += sizeof(imm);

        if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
        {
            constexpr ::std::size_t range_begin{CompileOption.f32_stack_top_begin_pos};
            constexpr ::std::size_t range_end{CompileOption.f32_stack_top_end_pos};
            static_assert(sizeof...(Type) >= range_end);
            static_assert(range_begin <= curr_f32_stack_top && curr_f32_stack_top < range_end);

            constexpr ::std::size_t new_pos{details::ring_prev_pos(curr_f32_stack_top, range_begin, range_end)};
            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, new_pos>(imm, type...);
        }
        else
        {
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);

            ::std::memcpy(type...[1u], ::std::addressof(imm), sizeof(imm));
            type...[1u] += sizeof(imm);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));

        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `f64.const` opcode (tail-call): pushes an f64 immediate.
    /// @details
    /// - Stack-top optimization: supported when `CompileOption.f64_stack_top_begin_pos != f64_stack_top_end_pos`; writes into the f64 stack-top ring.
    /// - `type[0]` layout: `[opfunc_ptr][imm:f64][next_opfunc_ptr]`.
    /// @note Keep the operand-stack path (`memcpy` + bump `type...[1u]`) in sync with the stack-top path to preserve semantics.
    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_f64_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_const(Type... type) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        wasm_f64 imm;  // no init
        ::std::memcpy(::std::addressof(imm), type...[0], sizeof(imm));
        type...[0] += sizeof(imm);

        if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
        {
            constexpr ::std::size_t range_begin{CompileOption.f64_stack_top_begin_pos};
            constexpr ::std::size_t range_end{CompileOption.f64_stack_top_end_pos};
            static_assert(sizeof...(Type) >= range_end);
            static_assert(range_begin <= curr_f64_stack_top && curr_f64_stack_top < range_end);

            constexpr ::std::size_t new_pos{details::ring_prev_pos(curr_f64_stack_top, range_begin, range_end)};
            details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, new_pos>(imm, type...);
        }
        else
        {
            static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);

            ::std::memcpy(type...[1u], ::std::addressof(imm), sizeof(imm));
            type...[1u] += sizeof(imm);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));

        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    // Non-tailcall (byref) variants: stacktop caching is disabled, operate purely on the operand stack.
    /// @brief `i32.const` opcode (non-tail-call/byref): pushes an i32 immediate onto the operand stack.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching and enforces all stack-top ranges to be `SIZE_MAX`).
    /// - `type[0]` layout: `[opfunc_byref_ptr][imm:i32][next_opfunc_byref_ptr]...`; after execution `typeref...[0]` points to the next opfunc slot.
    /// @note The upper-level dispatcher is responsible for loading and invoking the next opfunc.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_const(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i32 imm;  // no init
        ::std::memcpy(::std::addressof(imm), typeref...[0], sizeof(imm));
        typeref...[0] += sizeof(imm);

        ::std::memcpy(typeref...[1u], ::std::addressof(imm), sizeof(imm));
        typeref...[1u] += sizeof(imm);
    }

    /// @brief `i64.const` opcode (non-tail-call/byref): pushes an i64 immediate onto the operand stack.
    /// @details
    /// - Stack-top optimization: not supported.
    /// - `type[0]` layout: `[opfunc_byref_ptr][imm:i64][next_opfunc_byref_ptr]...`.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_const(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i64 imm;  // no init
        ::std::memcpy(::std::addressof(imm), typeref...[0], sizeof(imm));
        typeref...[0] += sizeof(imm);

        ::std::memcpy(typeref...[1u], ::std::addressof(imm), sizeof(imm));
        typeref...[1u] += sizeof(imm);
    }

    /// @brief `f32.const` opcode (non-tail-call/byref): pushes an f32 immediate onto the operand stack.
    /// @details
    /// - Stack-top optimization: not supported.
    /// - `type[0]` layout: `[opfunc_byref_ptr][imm:f32][next_opfunc_byref_ptr]...`.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_const(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f32 imm;  // no init
        ::std::memcpy(::std::addressof(imm), typeref...[0], sizeof(imm));
        typeref...[0] += sizeof(imm);

        ::std::memcpy(typeref...[1u], ::std::addressof(imm), sizeof(imm));
        typeref...[1u] += sizeof(imm);
    }

    /// @brief `f64.const` opcode (non-tail-call/byref): pushes an f64 immediate onto the operand stack.
    /// @details
    /// - Stack-top optimization: not supported.
    /// - `type[0]` layout: `[opfunc_byref_ptr][imm:f64][next_opfunc_byref_ptr]...`.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_const(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f64 imm;  // no init
        ::std::memcpy(::std::addressof(imm), typeref...[0], sizeof(imm));
        typeref...[0] += sizeof(imm);

        ::std::memcpy(typeref...[1u], ::std::addressof(imm), sizeof(imm));
        typeref...[1u] += sizeof(imm);
    }

    namespace translate
    {
        namespace details
        {
            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Curr, ::std::size_t End, uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...>
                get_uwvmint_i32_const_fptr_i32curr_impl(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
            {
                static_assert(Curr < End);
                static_assert(CompileOption.i32_stack_top_begin_pos <= Curr && Curr < CompileOption.i32_stack_top_end_pos);

                if(curr_stacktop.i32_stack_top_curr_pos == Curr) { return uwvmint_i32_const<CompileOption, Curr, Type...>; }
                else
                {
                    if constexpr(Curr + 1uz < End) { return get_uwvmint_i32_const_fptr_i32curr_impl<CompileOption, Curr + 1uz, End, Type...>(curr_stacktop); }
                    else
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Curr, ::std::size_t End, uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...>
                get_uwvmint_i64_const_fptr_i64curr_impl(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
            {
                static_assert(Curr < End);
                static_assert(CompileOption.i64_stack_top_begin_pos <= Curr && Curr < CompileOption.i64_stack_top_end_pos);

                if(curr_stacktop.i64_stack_top_curr_pos == Curr) { return uwvmint_i64_const<CompileOption, Curr, Type...>; }
                else
                {
                    if constexpr(Curr + 1uz < End) { return get_uwvmint_i64_const_fptr_i64curr_impl<CompileOption, Curr + 1uz, End, Type...>(curr_stacktop); }
                    else
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Curr, ::std::size_t End, uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...>
                get_uwvmint_f32_const_fptr_f32curr_impl(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
            {
                static_assert(Curr < End);
                static_assert(CompileOption.f32_stack_top_begin_pos <= Curr && Curr < CompileOption.f32_stack_top_end_pos);

                if(curr_stacktop.f32_stack_top_curr_pos == Curr) { return uwvmint_f32_const<CompileOption, Curr, Type...>; }
                else
                {
                    if constexpr(Curr + 1uz < End) { return get_uwvmint_f32_const_fptr_f32curr_impl<CompileOption, Curr + 1uz, End, Type...>(curr_stacktop); }
                    else
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t Curr, ::std::size_t End, uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...>
                get_uwvmint_f64_const_fptr_f64curr_impl(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
            {
                static_assert(Curr < End);
                static_assert(CompileOption.f64_stack_top_begin_pos <= Curr && Curr < CompileOption.f64_stack_top_end_pos);

                if(curr_stacktop.f64_stack_top_curr_pos == Curr) { return uwvmint_f64_const<CompileOption, Curr, Type...>; }
                else
                {
                    if constexpr(Curr + 1uz < End) { return get_uwvmint_f64_const_fptr_f64curr_impl<CompileOption, Curr + 1uz, End, Type...>(curr_stacktop); }
                    else
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::fast_io::fast_terminate();
                    }
                }
            }
        }  // namespace details

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_const_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::
                    get_uwvmint_i32_const_fptr_i32curr_impl<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, Type...>(
                        curr_stacktop);
            }
            else
            {
                return uwvmint_i32_const<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_const_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_const_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_const_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos)
            {
                return details::
                    get_uwvmint_i64_const_fptr_i64curr_impl<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, Type...>(
                        curr_stacktop);
            }
            else
            {
                return uwvmint_i64_const<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_const_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_const_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_const_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos)
            {
                return details::
                    get_uwvmint_f32_const_fptr_f32curr_impl<CompileOption, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos, Type...>(
                        curr_stacktop);
            }
            else
            {
                return uwvmint_f32_const<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_const_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_const_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_const_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos)
            {
                return details::
                    get_uwvmint_f64_const_fptr_f64curr_impl<CompileOption, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos, Type...>(
                        curr_stacktop);
            }
            else
            {
                return uwvmint_f64_const<CompileOption, 0uz, Type...>;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_const_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_const_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_const_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_const<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_const_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_const_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_const_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_const<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_const_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_const_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_const_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_const<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_const_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_const_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_const_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_const<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_const_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_const_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
