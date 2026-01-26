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
# include <algorithm>
# include <limits>
# include <memory>
# include <concepts>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/object/impl.h>
# include "define.h"
# include "storage.h"
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
        /// @brief Runtime trap bridge: handles the Wasm `unreachable` trap.
        /// @details
        /// - Stack-top optimization: not applicable.
        /// - Bytecode layout: not applicable (this helper does not read/advance the bytecode stream pointer).
        /// @note `unreachable_func` is expected to be set during interpreter initialization. If it is null (or returns unexpectedly), we terminate as a safe
        /// fallback.
        UWVM_GNU_COLD inline constexpr void unreachable() UWVM_THROWS
        {
            if(::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif

                ::fast_io::fast_terminate();
            }

            ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func();

            // Unreachable must not continue execution. If the embedding callback returns, terminate as a safety net.
            ::fast_io::fast_terminate();
        }
    }  // namespace details

    /// @brief `unreachable` opcode (tail-call): traps/terminates the VM.
    /// @details
    /// - Stack-top optimization: not applicable (no operand-stack interaction).
    /// - `type[0]` layout: `[opfunc_ptr]` (terminates/traps; no next opfunc is dispatched).
    /// @note This function cannot be forced to `[[noreturn]]` because some plugin-embedded deployments may need to perform cleanup instead of aborting.
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_unreachable(Type...) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        // curr_unreachable_opfunc ...
        // safe
        // ^^ type...[0]

        // Advance to the end of the current instruction for better diagnostics/debugging in case the trap is handled non-fatally by the embedding.
        // not necessary to: type...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>);

        details::unreachable();
    }

    /// @brief `unreachable` opcode (non-tail-call/byref): traps/terminates the VM.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching).
    /// - `type[0]` layout: `[opfunc_byref_ptr]` (no next-op dispatch here; termination happens inside this function).
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_unreachable(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(sizeof...(TypeRef) >= 1uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        // curr_unreachable_opfunc ...
        // safe
        // ^^ typeref...[0]

        // Advance to the end of the current instruction for better diagnostics/debugging in case the trap is handled non-fatally by the embedding.
        typeref...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        // curr_unreachable_opfunc ...
        // safe
        //                         ^^ typeref...[0]

        details::unreachable();
    }

    namespace translate
    {
        /// @brief Translator: returns the interpreter function pointer for `unreachable` (tail-call).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_unreachable_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            // Because there is no top-of-stack dependency, there is only a single version here.
            return uwvmint_unreachable<CompileOption, Type...>;
        }

        /// @brief Translator: infers types from a tuple and returns the `unreachable` function pointer (tail-call).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_unreachable_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_unreachable_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        /// @brief Translator: returns the interpreter function pointer for `unreachable` (non-tail-call/byref).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_unreachable_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            // Because there is no top-of-stack dependency, there is only a single version here.
            return uwvmint_unreachable<CompileOption, Type...>;
        }

        /// @brief Translator: infers types from a tuple and returns the `unreachable` function pointer (non-tail-call/byref).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_unreachable_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_unreachable_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate

    /// @brief `br` opcode (tail-call): unconditional branch to a translated instruction pointer.
    /// @details
    /// - Stack-top optimization: not applicable (no operand access; purely control-flow).
    /// - `type[0]` layout: `[opfunc_ptr][jmp_ip:byte const*]` (loads the jump target, sets `type...[0] = jmp_ip`, then tail-calls the opfunc at `jmp_ip`).
    /// @note `jmp_ip` may be unaligned for a function-pointer slot; always load the next opfunc via `memcpy`.
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_br(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        // curr_uwvmint_br jmp_ip (jmp to next_opfunc) ...
        // safe
        // ^^ type...[0]

        type...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>);

        // curr_uwvmint_br jmp_ip (jmp to next_opfunc) ...
        // safe
        //                 ^^ type...[0]

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));

        type...[0] = jmp_ip;

        // next_opfunc (*jmp_ip) ...
        // safe
        // ^^ type...[0]

        // `jmp_ip` may not be aligned for a function-pointer slot; always load via memcpy.
        ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));

        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `br` opcode (non-tail-call/byref): unconditional branch by updating `typeref...[0]`.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching).
    /// - `type[0]` layout: `[opfunc_byref_ptr][jmp_ip:byte const*][next_opfunc_byref_ptr]...`; after execution `typeref...[0]` is set to `jmp_ip`.
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_br(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(sizeof...(TypeRef) >= 1uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        // curr_uwvmint_br jmp_ip (jmp to next_opfunc) ...
        // safe
        // ^^ typeref...[0]

        typeref...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        // curr_uwvmint_br jmp_ip (jmp to next_opfunc) ...
        // safe
        //                 ^^ typeref...[0]

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), typeref...[0], sizeof(jmp_ip));

        typeref...[0] = jmp_ip;

        // next_opfunc (*jmp_ip) ...
        // safe
        // ^^ typeref...[0]

        // Function calls are initiated by higher-level functions.
    }

    namespace translate
    {
        /// @brief Translator: returns the interpreter function pointer for `br` (tail-call).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_br_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            // Because there is no top-of-stack dependency, there is only a single version here.
            return uwvmint_br<CompileOption, Type...>;
        }

        /// @brief Translator: infers types from a tuple and returns the `br` function pointer (tail-call).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_br_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        /// @brief Translator: returns the interpreter function pointer for `br` (non-tail-call/byref).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_br_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            // Because there is no top-of-stack dependency, there is only a single version here.
            return uwvmint_br<CompileOption, Type...>;
        }

        /// @brief Translator: infers types from a tuple and returns the `br` function pointer (non-tail-call/byref).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_br_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate

    /// @brief `br_if` opcode (tail-call): conditional branch based on an i32 condition.
    /// @details
    /// - Stack-top optimization: supported for the i32 condition when i32 stack-top caching is enabled; `curr_i32_stack_top` selects which cached slot is read.
    /// - `type[0]` layout: `[opfunc_ptr][jmp_ip:byte const*][next_op_false_ptr]` (if condition is non-zero, jumps to `jmp_ip`; otherwise continues at
    /// `next_op_false_ptr`).
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i32_stack_top,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_br_if(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        // curr_uwvmint_br_if jmp_ip next_op_false
        // safe
        // ^^ type...[0]

        type...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>);

        // curr_uwvmint_br_if jmp_ip next_op_false
        // safe
        //                    ^^ type...[0]

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0], sizeof(jmp_ip));

        type...[0] += sizeof(jmp_ip);

        // curr_uwvmint_br_if jmp_ip next_op_false
        // safe
        //                           ^^ type...[0]

        auto const cond{
            ::uwvm2::runtime::compiler::uwvm_int::optable::
                get_curr_val_from_operand_stack_top<CompileOption, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32, curr_i32_stack_top>(type...)};

        if(cond)
        {
            type...[0] = jmp_ip;

            // next_op_true (*jmp_ip) ...
            // safe
            // ^^ type...[0]
        }

        // next_opfunc ...
        // safe
        // ^^ type...[0]

        ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));

        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `br_if` opcode (non-tail-call/byref): conditional branch by updating `typeref...[0]`.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching; condition is popped from the operand stack).
    /// - `type[0]` layout: `[opfunc_byref_ptr][jmp_ip:byte const*][next_op_false_byref_ptr]...`; after execution `typeref...[0]` points to the chosen target.
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_br_if(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(sizeof...(TypeRef) >= 1uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        // curr_uwvmint_br_if jmp_ip next_op_false
        // safe
        // ^^ typeref...[0]

        typeref...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        // curr_uwvmint_br_if jmp_ip next_op_false
        // safe
        //                    ^^ typeref...[0]

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), typeref...[0], sizeof(jmp_ip));

        typeref...[0] += sizeof(jmp_ip);

        // curr_uwvmint_br_if jmp_ip next_op_false
        // safe
        //                           ^^ typeref...[0]

        auto const cond{
            ::uwvm2::runtime::compiler::uwvm_int::optable::get_curr_val_from_operand_stack_cache<::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>(
                typeref...)};

        if(cond)
        {
            typeref...[0] = jmp_ip;

            // next_op_true (*jmp_ip) ...
            // safe
            // ^^ typeref...[0]
        }

        // next_opfunc ...
        // safe
        // ^^ typeref...[0]

        // Function calls are initiated by higher-level functions.
    }

    namespace translate
    {
        namespace details
        {
            /// @brief Translator helper (tail-call): selects the `br_if` specialization by current i32 stack-top position.
            /// @details
            /// - Stack-top optimization: not applicable (this is translation-time selection logic).
            /// - `type[0]` layout: not applicable.
            template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t I32Curr,
                      ::std::size_t I32End,
                      ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_if_fptr_i32curr_impl(
                ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
            {
                static_assert(I32Curr < I32End);
                static_assert(CompileOption.i32_stack_top_begin_pos <= I32Curr && I32Curr < CompileOption.i32_stack_top_end_pos);

                if(curr_stacktop.i32_stack_top_curr_pos == I32Curr) { return uwvmint_br_if<CompileOption, I32Curr, Type...>; }
                else
                {
                    if constexpr(I32Curr + 1uz < I32End)
                    {
                        return get_uwvmint_br_if_fptr_i32curr_impl<CompileOption, I32Curr + 1uz, I32End, Type...>(curr_stacktop);
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
        }  // namespace details

        /// @brief Translator: returns the interpreter function pointer for `br_if` (tail-call).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_br_if_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::
                    get_uwvmint_br_if_fptr_i32curr_impl<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, Type...>(
                        curr_stacktop);
            }
            else
            {
                return uwvmint_br_if<CompileOption, 0uz, Type...>;
            }
        }

        /// @brief Translator: infers types from a tuple and returns the `br_if` function pointer (tail-call).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_br_if_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        /// @brief Translator: returns the interpreter function pointer for `br_if` (non-tail-call/byref).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_br_if_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_if<CompileOption, Type...>; }

        /// @brief Translator: infers types from a tuple and returns the `br_if` function pointer (non-tail-call/byref).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_br_if_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_if_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate

    /// @brief `br_table` opcode (tail-call): indirect branch using an i32 index and a jump table.
    /// @details
    /// - Stack-top optimization: supported for the i32 index when i32 stack-top caching is enabled; `curr_i32_stack_top` selects which cached slot is read.
    /// - `type[0]` layout: `[opfunc_ptr][max_size:size_t][table[0]:byte const*]...[table[max_size]:byte const*]`
    ///   (clamps index with `min(max_size, idx)` and branches to the selected target).
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i32_stack_top,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_br_table(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        // curr_uwvmint_br_table max_size table[0] table[1] ... table[max_size]
        // safe
        // ^^ type...[0]

        type...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>);

        // curr_uwvmint_br_table max_size table[0] table[1] ... table[max_size]
        // safe
        //                       ^^ type...[0]

        ::std::size_t max_size;  // no init
        ::std::memcpy(::std::addressof(max_size), type...[0], sizeof(max_size));

        type...[0] += sizeof(max_size);

        // curr_uwvmint_br_table max_size table[0] table[1] ... table[max_size]
        // safe
        //                                ^^ type...[0]

        auto const curr{
            ::uwvm2::runtime::compiler::uwvm_int::optable::
                get_curr_val_from_operand_stack_top<CompileOption, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32, curr_i32_stack_top>(type...)};

        ::std::size_t const curr_uz{static_cast<::std::size_t>(static_cast<::std::uint_least32_t>(curr))};
        ::std::size_t const idx{::std::min(max_size, curr_uz)};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), type...[0] + idx * sizeof(jmp_ip), sizeof(jmp_ip));

        type...[0] = jmp_ip;

        // next_opfunc (*jmp_ip) ...
        // safe
        // ^^ type...[0]

        ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));

        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `br_table` opcode (non-tail-call/byref): indirect branch by updating `typeref...[0]`.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching; index is popped from the operand stack).
    /// - `type[0]` layout: `[opfunc_byref_ptr][max_size:size_t][table[0]]...[table[max_size]][next_opfunc_byref_ptr]...`; after execution `typeref...[0]`
    /// points to the chosen target.
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_br_table(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(sizeof...(TypeRef) >= 1uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        // curr_uwvmint_br_table max_size table[0] table[1] ... table[max_size]
        // safe
        // ^^ typeref...[0]

        typeref...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        // curr_uwvmint_br_table max_size table[0] table[1] ... table[max_size]
        // safe
        //                       ^^ typeref...[0]

        ::std::size_t max_size;  // no init
        ::std::memcpy(::std::addressof(max_size), typeref...[0], sizeof(max_size));

        typeref...[0] += sizeof(max_size);

        // curr_uwvmint_br_table max_size table[0] table[1] ... table[max_size]
        // safe
        //                                ^^ typeref...[0]

        auto const curr{
            ::uwvm2::runtime::compiler::uwvm_int::optable::get_curr_val_from_operand_stack_cache<::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>(
                typeref...)};

        ::std::size_t const curr_uz{static_cast<::std::size_t>(static_cast<::std::uint_least32_t>(curr))};
        ::std::size_t const idx{::std::min(max_size, curr_uz)};

        ::std::byte const* jmp_ip;  // no init
        ::std::memcpy(::std::addressof(jmp_ip), typeref...[0] + idx * sizeof(jmp_ip), sizeof(jmp_ip));

        typeref...[0] = jmp_ip;

        // next_opfunc (*jmp_ip) ...
        // safe
        // ^^ typeref...[0]

        // Function calls are initiated by higher-level functions.
    }

    namespace translate
    {
        namespace details
        {
            /// @brief Translator helper (tail-call): selects the `br_table` specialization by current i32 stack-top position.
            /// @details
            /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
            template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t I32Curr,
                      ::std::size_t I32End,
                      ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_br_table_fptr_i32curr_impl(
                ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
            {
                static_assert(I32Curr < I32End);
                static_assert(CompileOption.i32_stack_top_begin_pos <= I32Curr && I32Curr < CompileOption.i32_stack_top_end_pos);

                if(curr_stacktop.i32_stack_top_curr_pos == I32Curr) { return uwvmint_br_table<CompileOption, I32Curr, Type...>; }
                else
                {
                    if constexpr(I32Curr + 1uz < I32End)
                    {
                        return get_uwvmint_br_table_fptr_i32curr_impl<CompileOption, I32Curr + 1uz, I32End, Type...>(curr_stacktop);
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
        }  // namespace details

        /// @brief Translator: returns the interpreter function pointer for `br_table` (tail-call).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_br_table_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            if constexpr(CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos)
            {
                return details::
                    get_uwvmint_br_table_fptr_i32curr_impl<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, Type...>(
                        curr_stacktop);
            }
            else
            {
                return uwvmint_br_table<CompileOption, 0uz, Type...>;
            }
        }

        /// @brief Translator: infers types from a tuple and returns the `br_table` function pointer (tail-call).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_br_table_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_table_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        /// @brief Translator: returns the interpreter function pointer for `br_table` (non-tail-call/byref).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_br_table_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_br_table<CompileOption, Type...>; }

        /// @brief Translator: infers types from a tuple and returns the `br_table` function pointer (non-tail-call/byref).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_br_table_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_br_table_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate

    /// @brief `return` opcode (tail-call): terminates the current tail-call dispatch chain.
    /// @details
    /// - Stack-top optimization: not applicable (no operand access here).
    /// - `type[0]` layout: `[opfunc_ptr]` (advances past the opfunc slot and returns to the outer interpreter loop).
    /// @note In tail-call mode this opcode does not pop results; before `return`, cached stack-top values must be flushed back to the operand stack via
    /// `stacktop_stack`.
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_return(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 1uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        // curr_uwvmint_return (end)
        // safe
        // ^^ type...[0]

        type...[0] += sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<Type...>);

        // curr_uwvmint_return (end)
        // safe
        //                     ^^ type...[0]

        // For tail calls, the return method does nothing.
        // Before the return instruction, all data must be pushed back onto the stack from the registers using the stacktop_stack operation.
    }

    /// @brief `return` opcode (non-tail-call/byref): signals the outer interpreter loop to exit.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching).
    /// - `type[0]` layout: `[opfunc_byref_ptr]` (this function sets `typeref...[0] = nullptr` as the sentinel for loop termination).
    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
              ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_return(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(sizeof...(TypeRef) >= 1uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        // curr_uwvmint_return (end)
        // safe
        // ^^ typeref...[0]

        typeref...[0] = nullptr;

        // The upper-level function loop in the interpreter checks whether the interpreter function is set to nullptr as the condition for exiting the loop.
    }

    namespace translate
    {
        /// @brief Translator: returns the interpreter function pointer for `return` (tail-call).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_return_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            // Because there is no top-of-stack dependency, there is only a single version here.
            return uwvmint_return<CompileOption, Type...>;
        }

        /// @brief Translator: infers types from a tuple and returns the `return` function pointer (tail-call).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_return_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_return_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        /// @brief Translator: returns the interpreter function pointer for `return` (non-tail-call/byref).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_return_fptr(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            // Because there is no top-of-stack dependency, there is only a single version here.
            return uwvmint_return<CompileOption, Type...>;
        }

        /// @brief Translator: infers types from a tuple and returns the `return` function pointer (non-tail-call/byref).
        /// @details
        /// - Stack-top optimization: not applicable; `type[0]` layout: not applicable.
        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption,
                  ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_return_fptr_from_tuple(::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_return_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
