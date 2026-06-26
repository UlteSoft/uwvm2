/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-17
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
# include <concepts>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
# include <type_traits>
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
# include "numeric.h"
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
# ifdef UWVM_ENABLE_UWVM_INT_INSTRUCTION_REORDER

    /**
     * @brief Register-ring-aware instruction recompiler opfuncs.
     *
     * @details
     * This file is intentionally separate from `conbine*.h`. Combine opfuncs fuse adjacent Wasm
     * opcodes. Instruction reorder opfuncs are emitted after the translator has proven that a
     * local, side-effect-free expression window may be regrouped to better fill the register ring.
     *
     * The first family covers LLVM-style shallow local traffic by preloading a consecutive
     * same-typed `local.get` burst into the register ring as one recompiled dispatch:
     *
     * @code{.wat}
     * local.get a
     * local.get b
     * local.get c
     * ;; any following consumer keeps the original Wasm stack order
     * @endcode
     *
     * This is the basic stack-caching layer: it does not regroup consumers, it only rebuilds the
     * producer window so the following interpreter opfuncs see more live operands in the ring.
     *
     * The second family covers LLVM-style shallow integer local reductions:
     *
     * @code{.wat}
     * local.get a
     * local.get b
     * i32.add ;; or i32.mul/i32.and/i32.or/i32.xor, with i64 equivalents
     * local.get c
     * i32.add
     * @endcode
     *
     * The translator rewrites that window into one direct-threaded dispatch that reads all locals,
     * folds a no-trap associative integer operation, and pushes one typed result through the normal
     * stack-top cache interface.
     *
     * The third and fourth families are update-aware. Same-op local reductions may write the result
     * directly to `local.set` or selected `local.tee`, avoiding a temporary operand-stack result.
     * One-step `local.get + const + int.binop + local.set/tee` updates use a dedicated compile-time
     * binop opfunc when the destination local differs from the source local; same-local updates stay
     * with ordinary update-local combination. Longer mixed local/constant integer folds use a compact
     * encoded expression program; they are gated by a higher step threshold because they trade
     * dispatch reduction for a runtime step decoder. `local.tee` followed by `br_if` is deliberately
     * left to branch fusion.
     */
    namespace instruction_reorder_details
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using wasm_u64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;

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

        template <numeric_details::int_binop Op>
        inline consteval bool is_supported_integer_reduce_op() noexcept
        {
            return Op == numeric_details::int_binop::add || Op == numeric_details::int_binop::mul || Op == numeric_details::int_binop::and_ ||
                   Op == numeric_details::int_binop::or_ || Op == numeric_details::int_binop::xor_;
        }

        enum class int_expr_operand_kind : ::std::uint8_t
        {
            local,
            imm
        };

        enum class int_expr_binop : ::std::uint8_t
        {
            add,
            sub,
            mul,
            and_,
            or_,
            xor_,
            shl,
            shr_s,
            shr_u,
            rotl,
            rotr
        };

        template <typename IntT, typename UIntT>
        UWVM_ALWAYS_INLINE inline constexpr IntT eval_int_expr_binop(int_expr_binop op, IntT lhs, IntT rhs) UWVM_THROWS
        {
            switch(op)
            {
                case int_expr_binop::add: return numeric_details::eval_int_binop<numeric_details::int_binop::add, IntT, UIntT>(lhs, rhs);
                case int_expr_binop::sub: return numeric_details::eval_int_binop<numeric_details::int_binop::sub, IntT, UIntT>(lhs, rhs);
                case int_expr_binop::mul: return numeric_details::eval_int_binop<numeric_details::int_binop::mul, IntT, UIntT>(lhs, rhs);
                case int_expr_binop::and_: return numeric_details::eval_int_binop<numeric_details::int_binop::and_, IntT, UIntT>(lhs, rhs);
                case int_expr_binop::or_: return numeric_details::eval_int_binop<numeric_details::int_binop::or_, IntT, UIntT>(lhs, rhs);
                case int_expr_binop::xor_: return numeric_details::eval_int_binop<numeric_details::int_binop::xor_, IntT, UIntT>(lhs, rhs);
                case int_expr_binop::shl: return numeric_details::eval_int_binop<numeric_details::int_binop::shl, IntT, UIntT>(lhs, rhs);
                case int_expr_binop::shr_s: return numeric_details::eval_int_binop<numeric_details::int_binop::shr_s, IntT, UIntT>(lhs, rhs);
                case int_expr_binop::shr_u: return numeric_details::eval_int_binop<numeric_details::int_binop::shr_u, IntT, UIntT>(lhs, rhs);
                case int_expr_binop::rotl: return numeric_details::eval_int_binop<numeric_details::int_binop::rotl, IntT, UIntT>(lhs, rhs);
                case int_expr_binop::rotr:
                    return numeric_details::eval_int_binop<numeric_details::int_binop::rotr, IntT, UIntT>(lhs, rhs);
                [[unlikely]] default:
                {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                    ::fast_io::fast_terminate();
                }
            }
        }

        template <int_expr_binop Op, typename IntT, typename UIntT>
        UWVM_ALWAYS_INLINE inline constexpr IntT eval_int_expr_binop_constexpr(IntT lhs, IntT rhs) UWVM_THROWS
        {
            if constexpr(Op == int_expr_binop::add) { return numeric_details::eval_int_binop<numeric_details::int_binop::add, IntT, UIntT>(lhs, rhs); }
            else if constexpr(Op == int_expr_binop::sub) { return numeric_details::eval_int_binop<numeric_details::int_binop::sub, IntT, UIntT>(lhs, rhs); }
            else if constexpr(Op == int_expr_binop::mul) { return numeric_details::eval_int_binop<numeric_details::int_binop::mul, IntT, UIntT>(lhs, rhs); }
            else if constexpr(Op == int_expr_binop::and_) { return numeric_details::eval_int_binop<numeric_details::int_binop::and_, IntT, UIntT>(lhs, rhs); }
            else if constexpr(Op == int_expr_binop::or_) { return numeric_details::eval_int_binop<numeric_details::int_binop::or_, IntT, UIntT>(lhs, rhs); }
            else if constexpr(Op == int_expr_binop::xor_) { return numeric_details::eval_int_binop<numeric_details::int_binop::xor_, IntT, UIntT>(lhs, rhs); }
            else if constexpr(Op == int_expr_binop::shl) { return numeric_details::eval_int_binop<numeric_details::int_binop::shl, IntT, UIntT>(lhs, rhs); }
            else if constexpr(Op == int_expr_binop::shr_s) { return numeric_details::eval_int_binop<numeric_details::int_binop::shr_s, IntT, UIntT>(lhs, rhs); }
            else if constexpr(Op == int_expr_binop::shr_u) { return numeric_details::eval_int_binop<numeric_details::int_binop::shr_u, IntT, UIntT>(lhs, rhs); }
            else if constexpr(Op == int_expr_binop::rotl) { return numeric_details::eval_int_binop<numeric_details::int_binop::rotl, IntT, UIntT>(lhs, rhs); }
            else if constexpr(Op == int_expr_binop::rotr) { return numeric_details::eval_int_binop<numeric_details::int_binop::rotr, IntT, UIntT>(lhs, rhs); }
            else
            {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                ::fast_io::fast_terminate();
                return {};
            }
        }

        template <typename IntT>
        UWVM_ALWAYS_INLINE inline constexpr IntT read_int_expr_operand(::std::byte const*& ip, ::std::byte* local_base) noexcept
        {
            auto const kind{static_cast<int_expr_operand_kind>(read_imm<::std::uint8_t>(ip))};
            switch(kind)
            {
                case int_expr_operand_kind::local:
                {
                    auto const off{read_imm<local_offset_t>(ip)};
                    return load_local<IntT>(local_base, off);
                }
                case int_expr_operand_kind::imm:
                    return read_imm<IntT>(ip);
                [[unlikely]] default:
                {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
                    ::fast_io::fast_terminate();
                }
            }
        }

        template <typename IntT, typename UIntT, ::std::size_t StepIndex, ::std::size_t StepCount>
        UWVM_ALWAYS_INLINE inline constexpr IntT eval_int_expr_steps(::std::byte const*& ip, ::std::byte* local_base, IntT acc) UWVM_THROWS
        {
            static_assert(StepIndex < StepCount);
            auto const op{static_cast<int_expr_binop>(read_imm<::std::uint8_t>(ip))};
            auto const rhs{read_int_expr_operand<IntT>(ip, local_base)};
            auto const next_acc{eval_int_expr_binop<IntT, UIntT>(op, acc, rhs)};
            if constexpr(StepIndex + 1uz < StepCount) { return eval_int_expr_steps<IntT, UIntT, StepIndex + 1uz, StepCount>(ip, local_base, next_acc); }
            else
            {
                return next_acc;
            }
        }

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

        template <uwvm_interpreter_translate_option_t CompileOption,
                  typename T,
                  ::std::size_t curr_stack_top,
                  ::std::size_t LocalIndex,
                  ::std::size_t LocalCount,
                  uwvm_int_stack_top_type... TypeRef>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void preload_localget_to_stacktop(::std::byte const*& ip, ::std::byte* local_base, TypeRef&... typeref) noexcept
        {
            static_assert(stacktop_enabled_for<CompileOption, T>());
            static_assert(LocalIndex < LocalCount);
            constexpr ::std::size_t begin{range_begin<CompileOption, T>()};
            constexpr ::std::size_t end{range_end<CompileOption, T>()};
            static_assert(begin <= curr_stack_top && curr_stack_top < end);
            static_assert(sizeof...(TypeRef) >= end);

            auto const off{read_imm<local_offset_t>(ip)};
            auto const v{load_local<T>(local_base, off)};
            constexpr ::std::size_t new_pos{details::ring_prev_pos(curr_stack_top, begin, end)};
            details::set_curr_val_to_stacktop_cache<CompileOption, T, new_pos>(v, typeref...);

            if constexpr(LocalIndex + 1uz < LocalCount)
            {
                preload_localget_to_stacktop<CompileOption, T, new_pos, LocalIndex + 1uz, LocalCount>(ip, local_base, typeref...);
            }
        }

        template <typename T, ::std::size_t LocalCount, uwvm_int_stack_top_type... TypeRef>
        UWVM_ALWAYS_INLINE inline constexpr void
            preload_localget_to_operand_memory(::std::byte const*& ip, ::std::byte* local_base, TypeRef&... typeref) noexcept
        {
            for(::std::size_t i{}; i != LocalCount; ++i)
            {
                auto const off{read_imm<local_offset_t>(ip)};
                auto const v{load_local<T>(local_base, off)};
                ::std::memcpy(typeref...[1u], ::std::addressof(v), sizeof(v));
                typeref...[1u] += sizeof(v);
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
    }  // namespace instruction_reorder_details

    /// @brief Recompiled same-typed `local.get` burst that preloads operands into the stack-top register ring.
    template <uwvm_interpreter_translate_option_t CompileOption,
              typename LocalT,
              ::std::size_t LocalCount,
              ::std::size_t curr_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_reorder_preload_nlocalget(Type... type) UWVM_THROWS
    {
        static_assert(LocalCount >= 2uz);
        static_assert(LocalCount <= 8uz);
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        [[maybe_unused]] auto const local_count{instruction_reorder_details::read_imm<::std::uint8_t>(type...[0])};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(local_count != LocalCount) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

        if constexpr(instruction_reorder_details::stacktop_enabled_for<CompileOption, LocalT>())
        {
            instruction_reorder_details::preload_localget_to_stacktop<CompileOption, LocalT, curr_stack_top, 0uz, LocalCount>(type...[0], type...[2u], type...);
        }
        else
        {
            instruction_reorder_details::preload_localget_to_operand_memory<LocalT, LocalCount>(type...[0], type...[2u], type...);
        }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Byref variant of @ref uwvmint_reorder_preload_nlocalget.
    template <uwvm_interpreter_translate_option_t CompileOption, typename LocalT, ::std::size_t LocalCount, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_reorder_preload_nlocalget(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(LocalCount >= 2uz);
        static_assert(LocalCount <= 8uz);
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

        [[maybe_unused]] auto const local_count{instruction_reorder_details::read_imm<::std::uint8_t>(typeref...[0])};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(local_count != LocalCount) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

        instruction_reorder_details::preload_localget_to_operand_memory<LocalT, LocalCount>(typeref...[0], typeref...[2u], typeref...);
    }

    /// @brief Recompiled integer local reduction generated by the optional instruction-reorder pass.
    template <uwvm_interpreter_translate_option_t CompileOption,
              typename IntT,
              typename UIntT,
              numeric_details::int_binop Op,
              ::std::size_t LocalCount,
              ::std::size_t curr_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_reorder_int_reduce_nlocalget(Type... type) UWVM_THROWS
    {
        static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
        static_assert(LocalCount >= 3uz);
        static_assert(LocalCount <= 8uz);
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        [[maybe_unused]] auto const local_count{instruction_reorder_details::read_imm<::std::uint8_t>(type...[0])};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(local_count != LocalCount) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

        instruction_reorder_details::local_offset_t offs[LocalCount]{};  // init
        for(::std::size_t i{}; i != LocalCount; ++i)
        {
            offs[i] = instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(type...[0]);
        }

        IntT acc{instruction_reorder_details::load_local<IntT>(type...[2u], offs[LocalCount - 1uz])};
        for(::std::size_t i{LocalCount - 1uz}; i-- != 0uz;)
        {
            acc = numeric_details::eval_int_binop<Op, IntT, UIntT>(instruction_reorder_details::load_local<IntT>(type...[2u], offs[i]), acc);
        }

        instruction_reorder_details::push_operand<CompileOption, IntT, curr_stack_top>(acc, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Byref variant of @ref uwvmint_reorder_int_reduce_nlocalget.
    template <uwvm_interpreter_translate_option_t CompileOption,
              typename IntT,
              typename UIntT,
              numeric_details::int_binop Op,
              ::std::size_t LocalCount,
              uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_reorder_int_reduce_nlocalget(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
        static_assert(LocalCount >= 3uz);
        static_assert(LocalCount <= 8uz);
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

        [[maybe_unused]] auto const local_count{instruction_reorder_details::read_imm<::std::uint8_t>(typeref...[0])};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(local_count != LocalCount) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

        instruction_reorder_details::local_offset_t offs[LocalCount]{};  // init
        for(::std::size_t i{}; i != LocalCount; ++i)
        {
            offs[i] = instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(typeref...[0]);
        }

        IntT acc{instruction_reorder_details::load_local<IntT>(typeref...[2u], offs[LocalCount - 1uz])};
        for(::std::size_t i{LocalCount - 1uz}; i-- != 0uz;)
        {
            acc = numeric_details::eval_int_binop<Op, IntT, UIntT>(instruction_reorder_details::load_local<IntT>(typeref...[2u], offs[i]), acc);
        }

        instruction_reorder_details::push_operand_byref<CompileOption>(acc, typeref...);
    }

    /// @brief Recompiled integer local reduction followed by `local.set` or `local.tee`.
    template <uwvm_interpreter_translate_option_t CompileOption,
              typename IntT,
              typename UIntT,
              numeric_details::int_binop Op,
              ::std::size_t LocalCount,
              bool KeepResult,
              ::std::size_t curr_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_reorder_int_reduce_nlocalget_local_update(Type... type) UWVM_THROWS
    {
        static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
        static_assert(LocalCount >= 3uz);
        static_assert(LocalCount <= 8uz);
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        [[maybe_unused]] auto const local_count{instruction_reorder_details::read_imm<::std::uint8_t>(type...[0])};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(local_count != LocalCount) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

        auto const dst_off{instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(type...[0])};

        instruction_reorder_details::local_offset_t offs[LocalCount]{};  // init
        for(::std::size_t i{}; i != LocalCount; ++i)
        {
            offs[i] = instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(type...[0]);
        }

        IntT acc{instruction_reorder_details::load_local<IntT>(type...[2u], offs[LocalCount - 1uz])};
        for(::std::size_t i{LocalCount - 1uz}; i-- != 0uz;)
        {
            acc = numeric_details::eval_int_binop<Op, IntT, UIntT>(instruction_reorder_details::load_local<IntT>(type...[2u], offs[i]), acc);
        }

        instruction_reorder_details::store_local<IntT>(type...[2u], dst_off, acc);
        if constexpr(KeepResult) { instruction_reorder_details::push_operand<CompileOption, IntT, curr_stack_top>(acc, type...); }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Byref variant of @ref uwvmint_reorder_int_reduce_nlocalget_local_update.
    template <uwvm_interpreter_translate_option_t CompileOption,
              typename IntT,
              typename UIntT,
              numeric_details::int_binop Op,
              ::std::size_t LocalCount,
              bool KeepResult,
              uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_reorder_int_reduce_nlocalget_local_update(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
        static_assert(LocalCount >= 3uz);
        static_assert(LocalCount <= 8uz);
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

        [[maybe_unused]] auto const local_count{instruction_reorder_details::read_imm<::std::uint8_t>(typeref...[0])};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(local_count != LocalCount) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

        auto const dst_off{instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(typeref...[0])};

        instruction_reorder_details::local_offset_t offs[LocalCount]{};  // init
        for(::std::size_t i{}; i != LocalCount; ++i)
        {
            offs[i] = instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(typeref...[0]);
        }

        IntT acc{instruction_reorder_details::load_local<IntT>(typeref...[2u], offs[LocalCount - 1uz])};
        for(::std::size_t i{LocalCount - 1uz}; i-- != 0uz;)
        {
            acc = numeric_details::eval_int_binop<Op, IntT, UIntT>(instruction_reorder_details::load_local<IntT>(typeref...[2u], offs[i]), acc);
        }

        instruction_reorder_details::store_local<IntT>(typeref...[2u], dst_off, acc);
        if constexpr(KeepResult) { instruction_reorder_details::push_operand_byref<CompileOption>(acc, typeref...); }
    }

    /// @brief Recompiled mixed local/constant integer left-fold expression.
    template <uwvm_interpreter_translate_option_t CompileOption,
              typename IntT,
              typename UIntT,
              ::std::size_t StepCount,
              ::std::size_t curr_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_reorder_int_expr_fold(Type... type) UWVM_THROWS
    {
        static_assert(StepCount >= 3uz);
        static_assert(StepCount <= 8uz);
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        [[maybe_unused]] auto const step_count{instruction_reorder_details::read_imm<::std::uint8_t>(type...[0])};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(step_count != StepCount) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

        auto const first_off{instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(type...[0])};
        auto const first{instruction_reorder_details::load_local<IntT>(type...[2u], first_off)};
        auto const out{instruction_reorder_details::eval_int_expr_steps<IntT, UIntT, 0uz, StepCount>(type...[0], type...[2u], first)};

        instruction_reorder_details::push_operand<CompileOption, IntT, curr_stack_top>(out, type...);

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Byref variant of @ref uwvmint_reorder_int_expr_fold.
    template <uwvm_interpreter_translate_option_t CompileOption, typename IntT, typename UIntT, ::std::size_t StepCount, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_reorder_int_expr_fold(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(StepCount >= 3uz);
        static_assert(StepCount <= 8uz);
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

        [[maybe_unused]] auto const step_count{instruction_reorder_details::read_imm<::std::uint8_t>(typeref...[0])};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(step_count != StepCount) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

        auto const first_off{instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(typeref...[0])};
        auto const first{instruction_reorder_details::load_local<IntT>(typeref...[2u], first_off)};
        auto const out{instruction_reorder_details::eval_int_expr_steps<IntT, UIntT, 0uz, StepCount>(typeref...[0], typeref...[2u], first)};

        instruction_reorder_details::push_operand_byref<CompileOption>(out, typeref...);
    }

    /// @brief Recompiled mixed local/constant integer expression followed by `local.set` or `local.tee`.
    template <uwvm_interpreter_translate_option_t CompileOption,
              typename IntT,
              typename UIntT,
              ::std::size_t StepCount,
              bool KeepResult,
              ::std::size_t curr_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_reorder_int_expr_local_update(Type... type) UWVM_THROWS
    {
        static_assert(StepCount >= 3uz);
        static_assert(StepCount <= 8uz);
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        [[maybe_unused]] auto const step_count{instruction_reorder_details::read_imm<::std::uint8_t>(type...[0])};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(step_count != StepCount) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

        auto const first_off{instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(type...[0])};
        auto const dst_off{instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(type...[0])};
        auto const first{instruction_reorder_details::load_local<IntT>(type...[2u], first_off)};
        auto const out{instruction_reorder_details::eval_int_expr_steps<IntT, UIntT, 0uz, StepCount>(type...[0], type...[2u], first)};
        instruction_reorder_details::store_local<IntT>(type...[2u], dst_off, out);

        if constexpr(KeepResult) { instruction_reorder_details::push_operand<CompileOption, IntT, curr_stack_top>(out, type...); }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Byref variant of @ref uwvmint_reorder_int_expr_local_update.
    template <uwvm_interpreter_translate_option_t CompileOption,
              typename IntT,
              typename UIntT,
              ::std::size_t StepCount,
              bool KeepResult,
              uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_reorder_int_expr_local_update(TypeRef & ... typeref) UWVM_THROWS
    {
        static_assert(StepCount >= 3uz);
        static_assert(StepCount <= 8uz);
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

        [[maybe_unused]] auto const step_count{instruction_reorder_details::read_imm<::std::uint8_t>(typeref...[0])};

#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(step_count != StepCount) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif

        auto const first_off{instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(typeref...[0])};
        auto const dst_off{instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(typeref...[0])};
        auto const first{instruction_reorder_details::load_local<IntT>(typeref...[2u], first_off)};
        auto const out{instruction_reorder_details::eval_int_expr_steps<IntT, UIntT, 0uz, StepCount>(typeref...[0], typeref...[2u], first)};
        instruction_reorder_details::store_local<IntT>(typeref...[2u], dst_off, out);

        if constexpr(KeepResult) { instruction_reorder_details::push_operand_byref<CompileOption>(out, typeref...); }
    }

    /// @brief Recompiled one-step `local.get; int.const; int.binop; local.set/local.tee` update.
    template <uwvm_interpreter_translate_option_t CompileOption,
              typename IntT,
              typename UIntT,
              instruction_reorder_details::int_expr_binop Op,
              bool KeepResult,
              ::std::size_t curr_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_reorder_int_const_binop_local_update(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[1u]>, ::std::byte*>);
        static_assert(::std::same_as<::std::remove_cvref_t<Type...[2u]>, ::std::byte*>);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);

        auto const src_off{instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(type...[0])};
        IntT const imm{instruction_reorder_details::read_imm<IntT>(type...[0])};
        auto const dst_off{instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(type...[0])};

        IntT const src{instruction_reorder_details::load_local<IntT>(type...[2u], src_off)};
        IntT const out{instruction_reorder_details::eval_int_expr_binop_constexpr<Op, IntT, UIntT>(src, imm)};
        instruction_reorder_details::store_local<IntT>(type...[2u], dst_off, out);

        if constexpr(KeepResult) { instruction_reorder_details::push_operand<CompileOption, IntT, curr_stack_top>(out, type...); }

        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief Byref variant of @ref uwvmint_reorder_int_const_binop_local_update.
    template <uwvm_interpreter_translate_option_t CompileOption,
              typename IntT,
              typename UIntT,
              instruction_reorder_details::int_expr_binop Op,
              bool KeepResult,
              uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_reorder_int_const_binop_local_update(TypeRef & ... typeref) UWVM_THROWS
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

        auto const src_off{instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(typeref...[0])};
        IntT const imm{instruction_reorder_details::read_imm<IntT>(typeref...[0])};
        auto const dst_off{instruction_reorder_details::read_imm<instruction_reorder_details::local_offset_t>(typeref...[0])};

        IntT const src{instruction_reorder_details::load_local<IntT>(typeref...[2u], src_off)};
        IntT const out{instruction_reorder_details::eval_int_expr_binop_constexpr<Op, IntT, UIntT>(src, imm)};
        instruction_reorder_details::store_local<IntT>(typeref...[2u], dst_off, out);

        if constexpr(KeepResult) { instruction_reorder_details::push_operand_byref<CompileOption>(out, typeref...); }
    }

    namespace translate
    {
        namespace reorder_details
        {
            template <typename LocalT, ::std::size_t LocalCount>
            struct preload_nlocalget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_reorder_preload_nlocalget<Opt, LocalT, LocalCount, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_reorder_preload_nlocalget<Opt, LocalT, LocalCount, Type...>; }
            };

            template <numeric_details::int_binop Op, ::std::size_t LocalCount>
            struct i32_reduce_nlocalget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    return uwvmint_reorder_int_reduce_nlocalget<Opt,
                                                                instruction_reorder_details::wasm_i32,
                                                                instruction_reorder_details::wasm_u32,
                                                                Op,
                                                                LocalCount,
                                                                Pos,
                                                                Type...>;
                }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                {
                    return uwvmint_reorder_int_reduce_nlocalget<Opt,
                                                                instruction_reorder_details::wasm_i32,
                                                                instruction_reorder_details::wasm_u32,
                                                                Op,
                                                                LocalCount,
                                                                Type...>;
                }
            };

            template <numeric_details::int_binop Op, ::std::size_t LocalCount>
            struct i64_reduce_nlocalget_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    return uwvmint_reorder_int_reduce_nlocalget<Opt,
                                                                instruction_reorder_details::wasm_i64,
                                                                instruction_reorder_details::wasm_u64,
                                                                Op,
                                                                LocalCount,
                                                                Pos,
                                                                Type...>;
                }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                {
                    return uwvmint_reorder_int_reduce_nlocalget<Opt,
                                                                instruction_reorder_details::wasm_i64,
                                                                instruction_reorder_details::wasm_u64,
                                                                Op,
                                                                LocalCount,
                                                                Type...>;
                }
            };

            template <numeric_details::int_binop Op, ::std::size_t LocalCount, bool KeepResult>
            struct i32_reduce_nlocalget_local_update_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    return uwvmint_reorder_int_reduce_nlocalget_local_update<Opt,
                                                                             instruction_reorder_details::wasm_i32,
                                                                             instruction_reorder_details::wasm_u32,
                                                                             Op,
                                                                             LocalCount,
                                                                             KeepResult,
                                                                             Pos,
                                                                             Type...>;
                }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                {
                    return uwvmint_reorder_int_reduce_nlocalget_local_update<Opt,
                                                                             instruction_reorder_details::wasm_i32,
                                                                             instruction_reorder_details::wasm_u32,
                                                                             Op,
                                                                             LocalCount,
                                                                             KeepResult,
                                                                             Type...>;
                }
            };

            template <numeric_details::int_binop Op, ::std::size_t LocalCount, bool KeepResult>
            struct i64_reduce_nlocalget_local_update_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    return uwvmint_reorder_int_reduce_nlocalget_local_update<Opt,
                                                                             instruction_reorder_details::wasm_i64,
                                                                             instruction_reorder_details::wasm_u64,
                                                                             Op,
                                                                             LocalCount,
                                                                             KeepResult,
                                                                             Pos,
                                                                             Type...>;
                }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                {
                    return uwvmint_reorder_int_reduce_nlocalget_local_update<Opt,
                                                                             instruction_reorder_details::wasm_i64,
                                                                             instruction_reorder_details::wasm_u64,
                                                                             Op,
                                                                             LocalCount,
                                                                             KeepResult,
                                                                             Type...>;
                }
            };

            template <::std::size_t StepCount>
            struct i32_expr_fold_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    return uwvmint_reorder_int_expr_fold<Opt,
                                                         instruction_reorder_details::wasm_i32,
                                                         instruction_reorder_details::wasm_u32,
                                                         StepCount,
                                                         Pos,
                                                         Type...>;
                }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_reorder_int_expr_fold<Opt, instruction_reorder_details::wasm_i32, instruction_reorder_details::wasm_u32, StepCount, Type...>; }
            };

            template <::std::size_t StepCount>
            struct i64_expr_fold_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    return uwvmint_reorder_int_expr_fold<Opt,
                                                         instruction_reorder_details::wasm_i64,
                                                         instruction_reorder_details::wasm_u64,
                                                         StepCount,
                                                         Pos,
                                                         Type...>;
                }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_reorder_int_expr_fold<Opt, instruction_reorder_details::wasm_i64, instruction_reorder_details::wasm_u64, StepCount, Type...>; }
            };

            template <::std::size_t StepCount, bool KeepResult>
            struct i32_expr_local_update_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    return uwvmint_reorder_int_expr_local_update<Opt,
                                                                 instruction_reorder_details::wasm_i32,
                                                                 instruction_reorder_details::wasm_u32,
                                                                 StepCount,
                                                                 KeepResult,
                                                                 Pos,
                                                                 Type...>;
                }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                {
                    return uwvmint_reorder_int_expr_local_update<Opt,
                                                                 instruction_reorder_details::wasm_i32,
                                                                 instruction_reorder_details::wasm_u32,
                                                                 StepCount,
                                                                 KeepResult,
                                                                 Type...>;
                }
            };

            template <::std::size_t StepCount, bool KeepResult>
            struct i64_expr_local_update_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    return uwvmint_reorder_int_expr_local_update<Opt,
                                                                 instruction_reorder_details::wasm_i64,
                                                                 instruction_reorder_details::wasm_u64,
                                                                 StepCount,
                                                                 KeepResult,
                                                                 Pos,
                                                                 Type...>;
                }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                {
                    return uwvmint_reorder_int_expr_local_update<Opt,
                                                                 instruction_reorder_details::wasm_i64,
                                                                 instruction_reorder_details::wasm_u64,
                                                                 StepCount,
                                                                 KeepResult,
                                                                 Type...>;
                }
            };

            template <instruction_reorder_details::int_expr_binop Op, bool KeepResult>
            struct i32_const_binop_local_update_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    return uwvmint_reorder_int_const_binop_local_update<Opt,
                                                                        instruction_reorder_details::wasm_i32,
                                                                        instruction_reorder_details::wasm_u32,
                                                                        Op,
                                                                        KeepResult,
                                                                        Pos,
                                                                        Type...>;
                }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                {
                    return uwvmint_reorder_int_const_binop_local_update<Opt,
                                                                        instruction_reorder_details::wasm_i32,
                                                                        instruction_reorder_details::wasm_u32,
                                                                        Op,
                                                                        KeepResult,
                                                                        Type...>;
                }
            };

            template <instruction_reorder_details::int_expr_binop Op, bool KeepResult>
            struct i64_const_binop_local_update_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                {
                    return uwvmint_reorder_int_const_binop_local_update<Opt,
                                                                        instruction_reorder_details::wasm_i64,
                                                                        instruction_reorder_details::wasm_u64,
                                                                        Op,
                                                                        KeepResult,
                                                                        Pos,
                                                                        Type...>;
                }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                {
                    return uwvmint_reorder_int_const_binop_local_update<Opt,
                                                                        instruction_reorder_details::wasm_i64,
                                                                        instruction_reorder_details::wasm_u64,
                                                                        Op,
                                                                        KeepResult,
                                                                        Type...>;
                }
            };

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t Curr,
                      ::std::size_t End,
                      typename OpWrapper,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_by_currpos_reorder_impl(::std::size_t pos) noexcept
            {
                static_assert(Curr < End);
                if(pos == Curr) { return OpWrapper::template fptr<CompileOption, Curr, Type...>(); }
                else
                {
                    if constexpr(Curr + 1uz < End)
                    {
                        return select_stacktop_fptr_by_currpos_reorder_impl<CompileOption, Curr + 1uz, End, OpWrapper, Type...>(pos);
                    }
                    else
                    {
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#  endif
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
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_or_default_reorder(::std::size_t pos) noexcept
            {
                if constexpr(BeginPos != EndPos)
                {
                    return select_stacktop_fptr_by_currpos_reorder_impl<CompileOption, BeginPos, EndPos, OpWrapper, Type...>(pos);
                }
                else
                {
                    return OpWrapper::template fptr<CompileOption, 0uz, Type...>();
                }
            }
        }  // namespace reorder_details

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_i32_preload_nlocalget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(LocalCount >= 2uz && LocalCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<
                CompileOption,
                CompileOption.i32_stack_top_begin_pos,
                CompileOption.i32_stack_top_end_pos,
                reorder_details::preload_nlocalget_op<instruction_reorder_details::wasm_i32, LocalCount>,
                Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i32_preload_nlocalget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_preload_nlocalget_fptr<CompileOption, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_i32_preload_nlocalget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(LocalCount >= 2uz && LocalCount <= 8uz);
            return reorder_details::preload_nlocalget_op<instruction_reorder_details::wasm_i32, LocalCount>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i32_preload_nlocalget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_preload_nlocalget_fptr<CompileOption, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_i64_preload_nlocalget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(LocalCount >= 2uz && LocalCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<
                CompileOption,
                CompileOption.i64_stack_top_begin_pos,
                CompileOption.i64_stack_top_end_pos,
                reorder_details::preload_nlocalget_op<instruction_reorder_details::wasm_i64, LocalCount>,
                Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i64_preload_nlocalget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_preload_nlocalget_fptr<CompileOption, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_i64_preload_nlocalget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(LocalCount >= 2uz && LocalCount <= 8uz);
            return reorder_details::preload_nlocalget_op<instruction_reorder_details::wasm_i64, LocalCount>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i64_preload_nlocalget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_preload_nlocalget_fptr<CompileOption, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_f32_preload_nlocalget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(LocalCount >= 2uz && LocalCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<
                CompileOption,
                CompileOption.f32_stack_top_begin_pos,
                CompileOption.f32_stack_top_end_pos,
                reorder_details::preload_nlocalget_op<instruction_reorder_details::wasm_f32, LocalCount>,
                Type...>(curr.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_f32_preload_nlocalget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_f32_preload_nlocalget_fptr<CompileOption, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_f32_preload_nlocalget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(LocalCount >= 2uz && LocalCount <= 8uz);
            return reorder_details::preload_nlocalget_op<instruction_reorder_details::wasm_f32, LocalCount>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_f32_preload_nlocalget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_f32_preload_nlocalget_fptr<CompileOption, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_f64_preload_nlocalget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(LocalCount >= 2uz && LocalCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<
                CompileOption,
                CompileOption.f64_stack_top_begin_pos,
                CompileOption.f64_stack_top_end_pos,
                reorder_details::preload_nlocalget_op<instruction_reorder_details::wasm_f64, LocalCount>,
                Type...>(curr.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_f64_preload_nlocalget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_f64_preload_nlocalget_fptr<CompileOption, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_f64_preload_nlocalget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(LocalCount >= 2uz && LocalCount <= 8uz);
            return reorder_details::preload_nlocalget_op<instruction_reorder_details::wasm_f64, LocalCount>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t LocalCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_f64_preload_nlocalget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                        ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_f64_preload_nlocalget_fptr<CompileOption, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_i32_reduce_nlocalget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
            static_assert(LocalCount >= 3uz && LocalCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<CompileOption,
                                                                            CompileOption.i32_stack_top_begin_pos,
                                                                            CompileOption.i32_stack_top_end_pos,
                                                                            reorder_details::i32_reduce_nlocalget_op<Op, LocalCount>,
                                                                            Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t LocalCount,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i32_reduce_nlocalget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_reduce_nlocalget_fptr<CompileOption, Op, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_i32_reduce_nlocalget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
            static_assert(LocalCount >= 3uz && LocalCount <= 8uz);
            return reorder_details::i32_reduce_nlocalget_op<Op, LocalCount>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t LocalCount,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i32_reduce_nlocalget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_reduce_nlocalget_fptr<CompileOption, Op, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_i64_reduce_nlocalget_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
            static_assert(LocalCount >= 3uz && LocalCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<CompileOption,
                                                                            CompileOption.i64_stack_top_begin_pos,
                                                                            CompileOption.i64_stack_top_end_pos,
                                                                            reorder_details::i64_reduce_nlocalget_op<Op, LocalCount>,
                                                                            Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t LocalCount,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i64_reduce_nlocalget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_reduce_nlocalget_fptr<CompileOption, Op, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_i64_reduce_nlocalget_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
            static_assert(LocalCount >= 3uz && LocalCount <= 8uz);
            return reorder_details::i64_reduce_nlocalget_op<Op, LocalCount>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t LocalCount,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i64_reduce_nlocalget_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_reduce_nlocalget_fptr<CompileOption, Op, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_i32_reduce_nlocalget_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
            static_assert(LocalCount >= 3uz && LocalCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<CompileOption,
                                                                            CompileOption.i32_stack_top_begin_pos,
                                                                            CompileOption.i32_stack_top_end_pos,
                                                                            reorder_details::i32_reduce_nlocalget_local_update_op<Op, LocalCount, false>,
                                                                            Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t LocalCount,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_reorder_i32_reduce_nlocalget_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_reduce_nlocalget_local_set_fptr<CompileOption, Op, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_i32_reduce_nlocalget_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
            static_assert(LocalCount >= 3uz && LocalCount <= 8uz);
            return reorder_details::i32_reduce_nlocalget_local_update_op<Op, LocalCount, false>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t LocalCount,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_reorder_i32_reduce_nlocalget_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_reduce_nlocalget_local_set_fptr<CompileOption, Op, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_i32_reduce_nlocalget_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
            static_assert(LocalCount >= 3uz && LocalCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<CompileOption,
                                                                            CompileOption.i32_stack_top_begin_pos,
                                                                            CompileOption.i32_stack_top_end_pos,
                                                                            reorder_details::i32_reduce_nlocalget_local_update_op<Op, LocalCount, true>,
                                                                            Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t LocalCount,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_reorder_i32_reduce_nlocalget_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_reduce_nlocalget_local_tee_fptr<CompileOption, Op, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_i32_reduce_nlocalget_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
            static_assert(LocalCount >= 3uz && LocalCount <= 8uz);
            return reorder_details::i32_reduce_nlocalget_local_update_op<Op, LocalCount, true>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t LocalCount,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_reorder_i32_reduce_nlocalget_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_reduce_nlocalget_local_tee_fptr<CompileOption, Op, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_i64_reduce_nlocalget_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
            static_assert(LocalCount >= 3uz && LocalCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<CompileOption,
                                                                            CompileOption.i64_stack_top_begin_pos,
                                                                            CompileOption.i64_stack_top_end_pos,
                                                                            reorder_details::i64_reduce_nlocalget_local_update_op<Op, LocalCount, false>,
                                                                            Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t LocalCount,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_reorder_i64_reduce_nlocalget_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_reduce_nlocalget_local_set_fptr<CompileOption, Op, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_i64_reduce_nlocalget_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
            static_assert(LocalCount >= 3uz && LocalCount <= 8uz);
            return reorder_details::i64_reduce_nlocalget_local_update_op<Op, LocalCount, false>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t LocalCount,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_reorder_i64_reduce_nlocalget_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_reduce_nlocalget_local_set_fptr<CompileOption, Op, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_i64_reduce_nlocalget_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
            static_assert(LocalCount >= 3uz && LocalCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<CompileOption,
                                                                            CompileOption.i64_stack_top_begin_pos,
                                                                            CompileOption.i64_stack_top_end_pos,
                                                                            reorder_details::i64_reduce_nlocalget_local_update_op<Op, LocalCount, true>,
                                                                            Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t LocalCount,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_reorder_i64_reduce_nlocalget_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_reduce_nlocalget_local_tee_fptr<CompileOption, Op, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t LocalCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_i64_reduce_nlocalget_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(instruction_reorder_details::is_supported_integer_reduce_op<Op>());
            static_assert(LocalCount >= 3uz && LocalCount <= 8uz);
            return reorder_details::i64_reduce_nlocalget_local_update_op<Op, LocalCount, true>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  numeric_details::int_binop Op,
                  ::std::size_t LocalCount,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto
            get_uwvmint_reorder_i64_reduce_nlocalget_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_reduce_nlocalget_local_tee_fptr<CompileOption, Op, LocalCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_reorder_i32_expr_fold_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(StepCount >= 3uz && StepCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<CompileOption,
                                                                            CompileOption.i32_stack_top_begin_pos,
                                                                            CompileOption.i32_stack_top_end_pos,
                                                                            reorder_details::i32_expr_fold_op<StepCount>,
                                                                            Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i32_expr_fold_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_expr_fold_fptr<CompileOption, StepCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_reorder_i32_expr_fold_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(StepCount >= 3uz && StepCount <= 8uz);
            return reorder_details::i32_expr_fold_op<StepCount>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i32_expr_fold_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_expr_fold_fptr<CompileOption, StepCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_reorder_i64_expr_fold_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(StepCount >= 3uz && StepCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<CompileOption,
                                                                            CompileOption.i64_stack_top_begin_pos,
                                                                            CompileOption.i64_stack_top_end_pos,
                                                                            reorder_details::i64_expr_fold_op<StepCount>,
                                                                            Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i64_expr_fold_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_expr_fold_fptr<CompileOption, StepCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_reorder_i64_expr_fold_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(StepCount >= 3uz && StepCount <= 8uz);
            return reorder_details::i64_expr_fold_op<StepCount>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i64_expr_fold_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_expr_fold_fptr<CompileOption, StepCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_i32_expr_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(StepCount >= 3uz && StepCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<CompileOption,
                                                                            CompileOption.i32_stack_top_begin_pos,
                                                                            CompileOption.i32_stack_top_end_pos,
                                                                            reorder_details::i32_expr_local_update_op<StepCount, false>,
                                                                            Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i32_expr_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_expr_local_set_fptr<CompileOption, StepCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_i32_expr_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(StepCount >= 3uz && StepCount <= 8uz);
            return reorder_details::i32_expr_local_update_op<StepCount, false>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i32_expr_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_expr_local_set_fptr<CompileOption, StepCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_i32_expr_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(StepCount >= 3uz && StepCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<CompileOption,
                                                                            CompileOption.i32_stack_top_begin_pos,
                                                                            CompileOption.i32_stack_top_end_pos,
                                                                            reorder_details::i32_expr_local_update_op<StepCount, true>,
                                                                            Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i32_expr_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_expr_local_tee_fptr<CompileOption, StepCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_i32_expr_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(StepCount >= 3uz && StepCount <= 8uz);
            return reorder_details::i32_expr_local_update_op<StepCount, true>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i32_expr_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_expr_local_tee_fptr<CompileOption, StepCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_i64_expr_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(StepCount >= 3uz && StepCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<CompileOption,
                                                                            CompileOption.i64_stack_top_begin_pos,
                                                                            CompileOption.i64_stack_top_end_pos,
                                                                            reorder_details::i64_expr_local_update_op<StepCount, false>,
                                                                            Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i64_expr_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_expr_local_set_fptr<CompileOption, StepCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_i64_expr_local_set_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(StepCount >= 3uz && StepCount <= 8uz);
            return reorder_details::i64_expr_local_update_op<StepCount, false>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i64_expr_local_set_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_expr_local_set_fptr<CompileOption, StepCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_i64_expr_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            static_assert(StepCount >= 3uz && StepCount <= 8uz);
            return reorder_details::select_stacktop_fptr_or_default_reorder<CompileOption,
                                                                            CompileOption.i64_stack_top_begin_pos,
                                                                            CompileOption.i64_stack_top_end_pos,
                                                                            reorder_details::i64_expr_local_update_op<StepCount, true>,
                                                                            Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i64_expr_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_expr_local_tee_fptr<CompileOption, StepCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_i64_expr_local_tee_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        {
            static_assert(StepCount >= 3uz && StepCount <= 8uz);
            return reorder_details::i64_expr_local_update_op<StepCount, true>::template fptr_byref<CompileOption, Type...>();
        }

        template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t StepCount, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i64_expr_local_tee_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_expr_local_tee_fptr<CompileOption, StepCount, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  instruction_reorder_details::int_expr_binop Op,
                  bool KeepResult,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_i32_const_binop_local_update_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return reorder_details::select_stacktop_fptr_or_default_reorder<CompileOption,
                                                                            CompileOption.i32_stack_top_begin_pos,
                                                                            CompileOption.i32_stack_top_end_pos,
                                                                            reorder_details::i32_const_binop_local_update_op<Op, KeepResult>,
                                                                            Type...>(curr.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  instruction_reorder_details::int_expr_binop Op,
                  bool KeepResult,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i32_const_binop_local_update_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_const_binop_local_update_fptr<CompileOption, Op, KeepResult, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  instruction_reorder_details::int_expr_binop Op,
                  bool KeepResult,
                  uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_i32_const_binop_local_update_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return reorder_details::i32_const_binop_local_update_op<Op, KeepResult>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  instruction_reorder_details::int_expr_binop Op,
                  bool KeepResult,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i32_const_binop_local_update_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i32_const_binop_local_update_fptr<CompileOption, Op, KeepResult, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  instruction_reorder_details::int_expr_binop Op,
                  bool KeepResult,
                  uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_reorder_i64_const_binop_local_update_fptr(uwvm_interpreter_stacktop_currpos_t const& curr) noexcept
        {
            return reorder_details::select_stacktop_fptr_or_default_reorder<CompileOption,
                                                                            CompileOption.i64_stack_top_begin_pos,
                                                                            CompileOption.i64_stack_top_end_pos,
                                                                            reorder_details::i64_const_binop_local_update_op<Op, KeepResult>,
                                                                            Type...>(curr.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  instruction_reorder_details::int_expr_binop Op,
                  bool KeepResult,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i64_const_binop_local_update_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_const_binop_local_update_fptr<CompileOption, Op, KeepResult, TypeInTuple...>(curr); }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  instruction_reorder_details::int_expr_binop Op,
                  bool KeepResult,
                  uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...>
            get_uwvmint_reorder_i64_const_binop_local_update_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return reorder_details::i64_const_binop_local_update_op<Op, KeepResult>::template fptr_byref<CompileOption, Type...>(); }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  instruction_reorder_details::int_expr_binop Op,
                  bool KeepResult,
                  uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_reorder_i64_const_binop_local_update_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr,
                                                                                               ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_reorder_i64_const_binop_local_update_fptr<CompileOption, Op, KeepResult, TypeInTuple...>(curr); }
    }  // namespace translate

# endif
}  // namespace uwvm2::runtime::compiler::uwvm_int::optable
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
