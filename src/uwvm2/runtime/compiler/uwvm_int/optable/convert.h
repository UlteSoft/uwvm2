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
# include <bit>
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
# include "storage.h"
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
        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval ::std::size_t stacktop_begin_pos() noexcept
        {
            if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>) { return CompileOption.i32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>)
            {
                return CompileOption.i64_stack_top_begin_pos;
            }
            else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>)
            {
                return CompileOption.f32_stack_top_begin_pos;
            }
            else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>)
            {
                return CompileOption.f64_stack_top_begin_pos;
            }
            else
            {
                return SIZE_MAX;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval ::std::size_t stacktop_end_pos() noexcept
        {
            if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>) { return CompileOption.i32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>) { return CompileOption.i64_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>) { return CompileOption.f32_stack_top_end_pos; }
            else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>) { return CompileOption.f64_stack_top_end_pos; }
            else
            {
                return SIZE_MAX;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, typename L, typename R>
        inline consteval bool stacktop_ranges_merged() noexcept
        {
            return stacktop_begin_pos<CompileOption, L>() == stacktop_begin_pos<CompileOption, R>() &&
                   stacktop_end_pos<CompileOption, L>() == stacktop_end_pos<CompileOption, R>();
        }

        /// @brief Compile-time check: whether stack-top caching is enabled for the given operand type.
        /// @details Returns `true` iff the corresponding `[begin,end)` range in `CompileOption` is non-empty.
        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval bool stacktop_enabled_for() noexcept
        {
            if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>)
            {
                return stacktop_begin_pos<CompileOption, OperandT>() != stacktop_end_pos<CompileOption, OperandT>();
            }
            else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>)
            {
                return stacktop_begin_pos<CompileOption, OperandT>() != stacktop_end_pos<CompileOption, OperandT>();
            }
            else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>)
            {
                return stacktop_begin_pos<CompileOption, OperandT>() != stacktop_end_pos<CompileOption, OperandT>();
            }
            else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>)
            {
                return stacktop_begin_pos<CompileOption, OperandT>() != stacktop_end_pos<CompileOption, OperandT>();
            }
            else
            {
                return true;
            }
        }

        /// @brief Compile-time check: whether i32/i64/f32/f64 stack-top ranges are fully merged.
        /// @details Required by some conversions that reuse the same stack-top slot while changing the value type.
        template <uwvm_interpreter_translate_option_t CompileOption>
        inline consteval bool scalar_ranges_all_merged() noexcept
        {
            return CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                   CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos &&
                   CompileOption.i32_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                   CompileOption.i32_stack_top_end_pos == CompileOption.f32_stack_top_end_pos &&
                   CompileOption.i32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                   CompileOption.i32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos;
        }

        /// @brief Compile-time check: whether i32 and i64 stack-top ranges are merged.
        /// @details Required by conversions that read one of {i32,i64} and write the other into the same stack-top slot.
        template <uwvm_interpreter_translate_option_t CompileOption>
        inline consteval bool i32_i64_ranges_merged() noexcept
        {
            return CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                   CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos;
        }

        /// @brief Compile-time check: whether i32 and f32 stack-top ranges are merged.
        /// @details Required by conversions that read f32 and write i32 (or vice versa) into the same stack-top slot.
        template <uwvm_interpreter_translate_option_t CompileOption>
        inline consteval bool i32_f32_ranges_merged() noexcept
        {
            return CompileOption.i32_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                   CompileOption.i32_stack_top_end_pos == CompileOption.f32_stack_top_end_pos;
        }

        /// @brief Compile-time check: whether f32 and f64 stack-top ranges are merged.
        /// @details Required by conversions that read one of {f32,f64} and write the other into the same stack-top slot.
        template <uwvm_interpreter_translate_option_t CompileOption>
        inline consteval bool f32_f64_ranges_merged() noexcept
        {
            return CompileOption.f32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                   CompileOption.f32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos;
        }

        /// @brief Trap helper used by float-to-int truncation when the conversion is invalid.
        /// @details This is the implementation for Wasm's "invalid conversion to integer" trap.
        /// @note There's no need for `noreturn`; let the calling function handle it.
        /// @note `unreachable_func` is expected to be set during interpreter initialization. If it is null (or returns unexpectedly), we terminate as a safe
        ///       fallback.
        UWVM_GNU_COLD inline void trap_invalid_conversion_to_integer() noexcept
        {
            if(::uwvm2::runtime::compiler::uwvm_int::optable::trap_invalid_conversion_to_integer_func == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif

                ::fast_io::fast_terminate();
            }

            ::uwvm2::runtime::compiler::uwvm_int::optable::trap_invalid_conversion_to_integer_func();

            // Unreachable must not continue execution. If the embedding callback returns, terminate as a safety net.
            ::fast_io::fast_terminate();
        }

        /// @brief Reinterprets a Wasm i32 value as unsigned bits.
        /// @details This preserves the original bit pattern (two's-complement) via `bit_cast`.
        template <typename WasmI32>
        UWVM_ALWAYS_INLINE inline constexpr ::std::uint_least32_t to_u32_bits(WasmI32 v) noexcept
        {
            ::std::int_least32_t const i{static_cast<::std::int_least32_t>(v)};
            return ::std::bit_cast<::std::uint_least32_t>(i);
        }

        /// @brief Reinterprets a Wasm i64 value as unsigned bits.
        /// @details This preserves the original bit pattern (two's-complement) via a value-preserving cast.
        template <typename WasmI64>
        UWVM_ALWAYS_INLINE inline constexpr ::std::uint_least64_t to_u64_bits(WasmI64 v) noexcept
        {
            ::std::int_least64_t const i{static_cast<::std::int_least64_t>(v)};
            return static_cast<::std::uint_least64_t>(i);
        }

        /// @brief Reinterprets unsigned i32 bits as a Wasm i32 value.
        /// @details This preserves the bit pattern using `bit_cast`.
        template <typename WasmI32>
        UWVM_ALWAYS_INLINE inline constexpr WasmI32 from_u32_bits(::std::uint_least32_t u) noexcept
        {
            ::std::int_least32_t const i{::std::bit_cast<::std::int_least32_t>(u)};
            return static_cast<WasmI32>(i);
        }

        template <typename WasmI64>
        UWVM_ALWAYS_INLINE inline constexpr WasmI64 from_u64_bits(::std::uint_least64_t u) noexcept
        {
            ::std::int_least64_t const i{::std::bit_cast<::std::int_least64_t>(u)};
            return static_cast<WasmI64>(i);
        }

        // Float-to-int truncation must retain strict IEEE semantics even when the whole project is built with -ffast-math.
        // (These helpers may be inlined, but correctness must not rely on inlining.)

        template <typename IntOut, typename FloatIn>
        UWVM_ALWAYS_INLINE inline IntOut trunc_float_to_int_s(FloatIn x) noexcept
        {
            constexpr FloatIn min_v{static_cast<FloatIn>(::std::numeric_limits<IntOut>::min())};
            constexpr FloatIn max_plus_one{static_cast<FloatIn>(static_cast<long double>(::std::numeric_limits<IntOut>::max()) + 1.0L)};

            if(x >= min_v && x < max_plus_one) [[likely]]
            {
                return static_cast<IntOut>(x);  // trunc toward zero
            }

            // Avoid UB even if the trap handler returns.
            trap_invalid_conversion_to_integer();
            return IntOut{};
        }

        template <typename UIntOut, typename FloatIn>
        UWVM_ALWAYS_INLINE inline UIntOut trunc_float_to_int_u(FloatIn x) noexcept
        {
            constexpr FloatIn max_plus_one{static_cast<FloatIn>(static_cast<long double>(::std::numeric_limits<UIntOut>::max()) + 1.0L)};

            if(x >= static_cast<FloatIn>(0) && x < max_plus_one) [[likely]]
            {
                return static_cast<UIntOut>(x);  // trunc toward zero
            }

            // Avoid UB even if the trap handler returns.
            trap_invalid_conversion_to_integer();
            return UIntOut{};
        }

    }  // namespace details

    // =========================
    // Tailcall (stacktop-aware)
    // =========================

    /// @brief `i32.wrap_i64` (tail-call): truncates i64 to i32 (low 32 bits).
    /// @details
    /// - Stack-top optimization: supported when i64 stack-top caching is enabled **and** the i32/i64 stack-top ranges are merged. Otherwise the opcode falls
    ///   back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i64_stack_top,
              ::std::size_t curr_i32_stack_top = curr_i64_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_wrap_i64(Type... type) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
            static_assert(range_begin <= curr_i64_stack_top && curr_i64_stack_top < range_end);

            wasm_i64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...)};
            ::std::uint_least32_t const low{static_cast<::std::uint_least32_t>(details::to_u64_bits(v))};
            wasm_i32 const out{details::from_u32_bits<wasm_i32>(low)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_i64, wasm_i32>())
                {
                    static_assert(curr_i64_stack_top == curr_i32_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_i64_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t i32_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
                    constexpr ::std::size_t i32_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
                    static_assert(i32_begin <= curr_i32_stack_top && curr_i32_stack_top < i32_end);
                    constexpr ::std::size_t new_i32_pos{details::ring_prev_pos(curr_i32_stack_top, i32_begin, i32_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, new_i32_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_i64 const v{get_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
            ::std::uint_least32_t const low{static_cast<::std::uint_least32_t>(details::to_u64_bits(v))};
            wasm_i32 const out{details::from_u32_bits<wasm_i32>(low)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                constexpr ::std::size_t i32_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
                constexpr ::std::size_t i32_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
                static_assert(i32_begin <= curr_i32_stack_top && curr_i32_stack_top < i32_end);
                constexpr ::std::size_t new_i32_pos{details::ring_prev_pos(curr_i32_stack_top, i32_begin, i32_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, new_i32_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `i64.extend_i32_s` (tail-call): sign-extends i32 to i64.
    /// @details
    /// - Stack-top optimization: supported when i32 stack-top caching is enabled **and** the i32/i64 stack-top ranges are merged. Otherwise the opcode falls
    ///   back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i32_stack_top,
              ::std::size_t curr_i64_stack_top = curr_i32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_extend_i32_s(Type... type) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
            static_assert(range_begin <= curr_i32_stack_top && curr_i32_stack_top < range_end);

            wasm_i32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            ::std::int_least64_t const out64{static_cast<::std::int_least64_t>(static_cast<::std::int_least32_t>(v))};
            wasm_i64 const out{static_cast<wasm_i64>(out64)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_i32, wasm_i64>())
                {
                    static_assert(curr_i32_stack_top == curr_i64_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_i32_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t i64_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
                    constexpr ::std::size_t i64_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
                    static_assert(i64_begin <= curr_i64_stack_top && curr_i64_stack_top < i64_end);
                    constexpr ::std::size_t new_i64_pos{details::ring_prev_pos(curr_i64_stack_top, i64_begin, i64_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_i64_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            ::std::int_least64_t const out64{static_cast<::std::int_least64_t>(static_cast<::std::int_least32_t>(v))};
            wasm_i64 const out{static_cast<wasm_i64>(out64)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                constexpr ::std::size_t i64_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
                constexpr ::std::size_t i64_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
                static_assert(i64_begin <= curr_i64_stack_top && curr_i64_stack_top < i64_end);
                constexpr ::std::size_t new_i64_pos{details::ring_prev_pos(curr_i64_stack_top, i64_begin, i64_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_i64_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `i64.extend_i32_u` (tail-call): zero-extends i32 to i64.
    /// @details
    /// - Stack-top optimization: supported when i32 stack-top caching is enabled **and** the i32/i64 stack-top ranges are merged. Otherwise the opcode falls
    ///   back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i32_stack_top,
              ::std::size_t curr_i64_stack_top = curr_i32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_extend_i32_u(Type... type) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
            static_assert(range_begin <= curr_i32_stack_top && curr_i32_stack_top < range_end);

            wasm_i32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            ::std::uint_least32_t const u32{details::to_u32_bits(v)};
            wasm_i64 const out{static_cast<wasm_i64>(static_cast<::std::uint_least64_t>(u32))};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_i32, wasm_i64>())
                {
                    static_assert(curr_i32_stack_top == curr_i64_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_i32_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t i64_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
                    constexpr ::std::size_t i64_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
                    static_assert(i64_begin <= curr_i64_stack_top && curr_i64_stack_top < i64_end);
                    constexpr ::std::size_t new_i64_pos{details::ring_prev_pos(curr_i64_stack_top, i64_begin, i64_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_i64_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            ::std::uint_least32_t const u32{details::to_u32_bits(v)};
            wasm_i64 const out{static_cast<wasm_i64>(static_cast<::std::uint_least64_t>(u32))};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                constexpr ::std::size_t i64_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
                constexpr ::std::size_t i64_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
                static_assert(i64_begin <= curr_i64_stack_top && curr_i64_stack_top < i64_end);
                constexpr ::std::size_t new_i64_pos{details::ring_prev_pos(curr_i64_stack_top, i64_begin, i64_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_i64_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `i32.reinterpret_f32` (tail-call): bitcasts f32 to i32.
    /// @details
    /// - Stack-top optimization: supported when f32 stack-top caching is enabled **and** the i32/f32 stack-top ranges are merged. Otherwise the opcode falls
    ///   back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_f32_stack_top,
              ::std::size_t curr_i32_stack_top = curr_f32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_reinterpret_f32(Type... type) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
            static_assert(range_begin <= curr_f32_stack_top && curr_f32_stack_top < range_end);

            wasm_f32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, curr_f32_stack_top>(type...)};
            ::std::uint_least32_t const bits{::std::bit_cast<::std::uint_least32_t>(v)};
            wasm_i32 const out{details::from_u32_bits<wasm_i32>(bits)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_f32, wasm_i32>())
                {
                    static_assert(curr_f32_stack_top == curr_i32_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_f32_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t i32_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
                    constexpr ::std::size_t i32_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
                    static_assert(i32_begin <= curr_i32_stack_top && curr_i32_stack_top < i32_end);
                    constexpr ::std::size_t new_i32_pos{details::ring_prev_pos(curr_i32_stack_top, i32_begin, i32_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, new_i32_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_f32 const v{get_curr_val_from_operand_stack_cache<wasm_f32>(type...)};
            ::std::uint_least32_t const bits{::std::bit_cast<::std::uint_least32_t>(v)};
            wasm_i32 const out{details::from_u32_bits<wasm_i32>(bits)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                constexpr ::std::size_t i32_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
                constexpr ::std::size_t i32_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
                static_assert(i32_begin <= curr_i32_stack_top && curr_i32_stack_top < i32_end);
                constexpr ::std::size_t new_i32_pos{details::ring_prev_pos(curr_i32_stack_top, i32_begin, i32_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, new_i32_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `f32.reinterpret_i32` (tail-call): bitcasts i32 to f32.
    /// @details
    /// - Stack-top optimization: supported when i32 stack-top caching is enabled **and** the i32/f32 stack-top ranges are merged. Otherwise the opcode falls
    ///   back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i32_stack_top,
              ::std::size_t curr_f32_stack_top = curr_i32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_reinterpret_i32(Type... type) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
            static_assert(range_begin <= curr_i32_stack_top && curr_i32_stack_top < range_end);

            wasm_i32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            ::std::uint_least32_t const bits{details::to_u32_bits(v)};
            wasm_f32 const out{::std::bit_cast<wasm_f32>(bits)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_i32, wasm_f32>())
                {
                    static_assert(curr_i32_stack_top == curr_f32_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, curr_i32_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t f32_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
                    constexpr ::std::size_t f32_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
                    static_assert(f32_begin <= curr_f32_stack_top && curr_f32_stack_top < f32_end);
                    constexpr ::std::size_t new_f32_pos{details::ring_prev_pos(curr_f32_stack_top, f32_begin, f32_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, new_f32_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            ::std::uint_least32_t const bits{details::to_u32_bits(v)};
            wasm_f32 const out{::std::bit_cast<wasm_f32>(bits)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
            {
                constexpr ::std::size_t f32_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
                constexpr ::std::size_t f32_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
                static_assert(f32_begin <= curr_f32_stack_top && curr_f32_stack_top < f32_end);
                constexpr ::std::size_t new_f32_pos{details::ring_prev_pos(curr_f32_stack_top, f32_begin, f32_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, new_f32_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    // ---- float<->float ----

    /// @brief `f32.demote_f64` (tail-call): converts f64 to f32.
    /// @details
    /// - Stack-top optimization: supported when f64 stack-top caching is enabled **and** the f32/f64 stack-top ranges are merged. Otherwise the opcode falls
    ///   back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_f64_stack_top,
              ::std::size_t curr_f32_stack_top = curr_f64_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_demote_f64(Type... type) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
            static_assert(range_begin <= curr_f64_stack_top && curr_f64_stack_top < range_end);

            wasm_f64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, curr_f64_stack_top>(type...)};
            wasm_f32 const out{static_cast<wasm_f32>(v)};
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_f64, wasm_f32>())
                {
                    static_assert(curr_f64_stack_top == curr_f32_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, curr_f64_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t f32_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
                    constexpr ::std::size_t f32_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
                    static_assert(f32_begin <= curr_f32_stack_top && curr_f32_stack_top < f32_end);
                    constexpr ::std::size_t new_f32_pos{details::ring_prev_pos(curr_f32_stack_top, f32_begin, f32_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, new_f32_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_f64 const v{get_curr_val_from_operand_stack_cache<wasm_f64>(type...)};
            wasm_f32 const out{static_cast<wasm_f32>(v)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
            {
                constexpr ::std::size_t f32_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
                constexpr ::std::size_t f32_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
                static_assert(f32_begin <= curr_f32_stack_top && curr_f32_stack_top < f32_end);
                constexpr ::std::size_t new_f32_pos{details::ring_prev_pos(curr_f32_stack_top, f32_begin, f32_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, new_f32_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `f64.promote_f32` (tail-call): converts f32 to f64.
    /// @details
    /// - Stack-top optimization: supported when f32 stack-top caching is enabled **and** the f32/f64 stack-top ranges are merged. Otherwise the opcode falls
    ///   back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_f32_stack_top,
              ::std::size_t curr_f64_stack_top = curr_f32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_promote_f32(Type... type) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
            static_assert(range_begin <= curr_f32_stack_top && curr_f32_stack_top < range_end);

            wasm_f32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, curr_f32_stack_top>(type...)};
            wasm_f64 const out{static_cast<wasm_f64>(v)};
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_f32, wasm_f64>())
                {
                    static_assert(curr_f32_stack_top == curr_f64_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, curr_f32_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t f64_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
                    constexpr ::std::size_t f64_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
                    static_assert(f64_begin <= curr_f64_stack_top && curr_f64_stack_top < f64_end);
                    constexpr ::std::size_t new_f64_pos{details::ring_prev_pos(curr_f64_stack_top, f64_begin, f64_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, new_f64_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_f32 const v{get_curr_val_from_operand_stack_cache<wasm_f32>(type...)};
            wasm_f64 const out{static_cast<wasm_f64>(v)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
            {
                constexpr ::std::size_t f64_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
                constexpr ::std::size_t f64_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
                static_assert(f64_begin <= curr_f64_stack_top && curr_f64_stack_top < f64_end);
                constexpr ::std::size_t new_f64_pos{details::ring_prev_pos(curr_f64_stack_top, f64_begin, f64_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, new_f64_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    // ---- float->int trunc (traps on invalid) ----

    /// @brief `i32.trunc_f32_s` (tail-call): truncates f32 to signed i32, trapping on invalid conversion.
    /// @details
    /// - Stack-top optimization: supported when f32 stack-top caching is enabled **and** the i32/f32 stack-top ranges are merged. Otherwise the opcode falls
    ///   back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    /// @note This opcode may terminate the VM on NaN/out-of-range inputs (Wasm invalid conversion trap).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_f32_stack_top,
              ::std::size_t curr_i32_stack_top = curr_f32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_trunc_f32_s(Type... type) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
            static_assert(range_begin <= curr_f32_stack_top && curr_f32_stack_top < range_end);

            wasm_f32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, curr_f32_stack_top>(type...)};
            ::std::int_least32_t const out32{details::trunc_float_to_int_s<::std::int_least32_t>(v)};
            wasm_i32 const out{static_cast<wasm_i32>(out32)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_f32, wasm_i32>())
                {
                    static_assert(curr_f32_stack_top == curr_i32_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_f32_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t i32_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
                    constexpr ::std::size_t i32_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
                    static_assert(i32_begin <= curr_i32_stack_top && curr_i32_stack_top < i32_end);
                    constexpr ::std::size_t new_i32_pos{details::ring_prev_pos(curr_i32_stack_top, i32_begin, i32_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, new_i32_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_f32 const v{get_curr_val_from_operand_stack_cache<wasm_f32>(type...)};
            ::std::int_least32_t const out32{details::trunc_float_to_int_s<::std::int_least32_t>(v)};
            wasm_i32 const out{static_cast<wasm_i32>(out32)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                constexpr ::std::size_t i32_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
                constexpr ::std::size_t i32_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
                static_assert(i32_begin <= curr_i32_stack_top && curr_i32_stack_top < i32_end);
                constexpr ::std::size_t new_i32_pos{details::ring_prev_pos(curr_i32_stack_top, i32_begin, i32_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, new_i32_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `i32.trunc_f32_u` (tail-call): truncates f32 to unsigned i32, trapping on invalid conversion.
    /// @details
    /// - Stack-top optimization: supported when f32 stack-top caching is enabled **and** the i32/f32 stack-top ranges are merged. Otherwise the opcode falls
    ///   back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    /// @note This opcode may terminate the VM on NaN/out-of-range inputs (Wasm invalid conversion trap).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_f32_stack_top,
              ::std::size_t curr_i32_stack_top = curr_f32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_trunc_f32_u(Type... type) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
            static_assert(range_begin <= curr_f32_stack_top && curr_f32_stack_top < range_end);

            wasm_f32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, curr_f32_stack_top>(type...)};
            ::std::uint_least32_t const u32{details::trunc_float_to_int_u<::std::uint_least32_t>(v)};
            wasm_i32 const out{details::from_u32_bits<wasm_i32>(u32)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_f32, wasm_i32>())
                {
                    static_assert(curr_f32_stack_top == curr_i32_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_f32_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t i32_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
                    constexpr ::std::size_t i32_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
                    static_assert(i32_begin <= curr_i32_stack_top && curr_i32_stack_top < i32_end);
                    constexpr ::std::size_t new_i32_pos{details::ring_prev_pos(curr_i32_stack_top, i32_begin, i32_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, new_i32_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_f32 const v{get_curr_val_from_operand_stack_cache<wasm_f32>(type...)};
            ::std::uint_least32_t const u32{details::trunc_float_to_int_u<::std::uint_least32_t>(v)};
            wasm_i32 const out{details::from_u32_bits<wasm_i32>(u32)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                constexpr ::std::size_t i32_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
                constexpr ::std::size_t i32_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
                static_assert(i32_begin <= curr_i32_stack_top && curr_i32_stack_top < i32_end);
                constexpr ::std::size_t new_i32_pos{details::ring_prev_pos(curr_i32_stack_top, i32_begin, i32_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, new_i32_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `i32.trunc_f64_s` (tail-call): truncates f64 to signed i32, trapping on invalid conversion.
    /// @details
    /// - Stack-top optimization: supported when f64 stack-top caching is enabled **and** the scalar stack-top ranges are fully merged. Otherwise the opcode
    ///   falls back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    /// @note This opcode may terminate the VM on NaN/out-of-range inputs (Wasm invalid conversion trap).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_f64_stack_top,
              ::std::size_t curr_i32_stack_top = curr_f64_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_trunc_f64_s(Type... type) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
            static_assert(range_begin <= curr_f64_stack_top && curr_f64_stack_top < range_end);

            wasm_f64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, curr_f64_stack_top>(type...)};
            ::std::int_least32_t const out32{details::trunc_float_to_int_s<::std::int_least32_t>(v)};
            wasm_i32 const out{static_cast<wasm_i32>(out32)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_f64, wasm_i32>())
                {
                    static_assert(curr_f64_stack_top == curr_i32_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_f64_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t i32_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
                    constexpr ::std::size_t i32_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
                    static_assert(i32_begin <= curr_i32_stack_top && curr_i32_stack_top < i32_end);
                    constexpr ::std::size_t new_i32_pos{details::ring_prev_pos(curr_i32_stack_top, i32_begin, i32_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, new_i32_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_f64 const v{get_curr_val_from_operand_stack_cache<wasm_f64>(type...)};
            ::std::int_least32_t const out32{details::trunc_float_to_int_s<::std::int_least32_t>(v)};
            wasm_i32 const out{static_cast<wasm_i32>(out32)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                constexpr ::std::size_t i32_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
                constexpr ::std::size_t i32_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
                static_assert(i32_begin <= curr_i32_stack_top && curr_i32_stack_top < i32_end);
                constexpr ::std::size_t new_i32_pos{details::ring_prev_pos(curr_i32_stack_top, i32_begin, i32_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, new_i32_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `i32.trunc_f64_u` (tail-call): truncates f64 to unsigned i32, trapping on invalid conversion.
    /// @details
    /// - Stack-top optimization: supported when f64 stack-top caching is enabled **and** the scalar stack-top ranges are fully merged. Otherwise the opcode
    ///   falls back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    /// @note This opcode may terminate the VM on NaN/out-of-range inputs (Wasm invalid conversion trap).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_f64_stack_top,
              ::std::size_t curr_i32_stack_top = curr_f64_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_trunc_f64_u(Type... type) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
            static_assert(range_begin <= curr_f64_stack_top && curr_f64_stack_top < range_end);

            wasm_f64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, curr_f64_stack_top>(type...)};
            ::std::uint_least32_t const u32{details::trunc_float_to_int_u<::std::uint_least32_t>(v)};
            wasm_i32 const out{details::from_u32_bits<wasm_i32>(u32)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_f64, wasm_i32>())
                {
                    static_assert(curr_f64_stack_top == curr_i32_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, curr_f64_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t i32_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
                    constexpr ::std::size_t i32_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
                    static_assert(i32_begin <= curr_i32_stack_top && curr_i32_stack_top < i32_end);
                    constexpr ::std::size_t new_i32_pos{details::ring_prev_pos(curr_i32_stack_top, i32_begin, i32_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, new_i32_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_f64 const v{get_curr_val_from_operand_stack_cache<wasm_f64>(type...)};
            ::std::uint_least32_t const u32{details::trunc_float_to_int_u<::std::uint_least32_t>(v)};
            wasm_i32 const out{details::from_u32_bits<wasm_i32>(u32)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
            {
                constexpr ::std::size_t i32_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
                constexpr ::std::size_t i32_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
                static_assert(i32_begin <= curr_i32_stack_top && curr_i32_stack_top < i32_end);
                constexpr ::std::size_t new_i32_pos{details::ring_prev_pos(curr_i32_stack_top, i32_begin, i32_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, new_i32_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    // ---- i64 trunc ----

    /// @brief `i64.trunc_f32_s` (tail-call): truncates f32 to signed i64, trapping on invalid conversion.
    /// @details
    /// - Stack-top optimization: supported when f32 stack-top caching is enabled **and** the scalar stack-top ranges are fully merged. Otherwise the opcode
    ///   falls back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    /// @note This opcode may terminate the VM on NaN/out-of-range inputs (Wasm invalid conversion trap).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_f32_stack_top,
              ::std::size_t curr_i64_stack_top = curr_f32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_trunc_f32_s(Type... type) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
            static_assert(range_begin <= curr_f32_stack_top && curr_f32_stack_top < range_end);

            wasm_f32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, curr_f32_stack_top>(type...)};
            ::std::int_least64_t const out64{details::trunc_float_to_int_s<::std::int_least64_t>(v)};
            wasm_i64 const out{static_cast<wasm_i64>(out64)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_f32, wasm_i64>())
                {
                    static_assert(curr_f32_stack_top == curr_i64_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_f32_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t i64_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
                    constexpr ::std::size_t i64_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
                    static_assert(i64_begin <= curr_i64_stack_top && curr_i64_stack_top < i64_end);
                    constexpr ::std::size_t new_i64_pos{details::ring_prev_pos(curr_i64_stack_top, i64_begin, i64_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_i64_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_f32 const v{get_curr_val_from_operand_stack_cache<wasm_f32>(type...)};
            ::std::int_least64_t const out64{details::trunc_float_to_int_s<::std::int_least64_t>(v)};
            wasm_i64 const out{static_cast<wasm_i64>(out64)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                constexpr ::std::size_t i64_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
                constexpr ::std::size_t i64_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
                static_assert(i64_begin <= curr_i64_stack_top && curr_i64_stack_top < i64_end);
                constexpr ::std::size_t new_i64_pos{details::ring_prev_pos(curr_i64_stack_top, i64_begin, i64_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_i64_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `i64.trunc_f32_u` (tail-call): truncates f32 to unsigned i64, trapping on invalid conversion.
    /// @details
    /// - Stack-top optimization: supported when f32 stack-top caching is enabled **and** the scalar stack-top ranges are fully merged. Otherwise the opcode
    ///   falls back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    /// @note This opcode may terminate the VM on NaN/out-of-range inputs (Wasm invalid conversion trap).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_f32_stack_top,
              ::std::size_t curr_i64_stack_top = curr_f32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_trunc_f32_u(Type... type) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
            static_assert(range_begin <= curr_f32_stack_top && curr_f32_stack_top < range_end);

            wasm_f32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_f32, curr_f32_stack_top>(type...)};
            ::std::uint_least64_t const u64{details::trunc_float_to_int_u<::std::uint_least64_t>(v)};
            wasm_i64 const out{details::from_u64_bits<wasm_i64>(u64)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_f32, wasm_i64>())
                {
                    static_assert(curr_f32_stack_top == curr_i64_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_f32_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t i64_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
                    constexpr ::std::size_t i64_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
                    static_assert(i64_begin <= curr_i64_stack_top && curr_i64_stack_top < i64_end);
                    constexpr ::std::size_t new_i64_pos{details::ring_prev_pos(curr_i64_stack_top, i64_begin, i64_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_i64_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_f32 const v{get_curr_val_from_operand_stack_cache<wasm_f32>(type...)};
            ::std::uint_least64_t const u64{details::trunc_float_to_int_u<::std::uint_least64_t>(v)};
            wasm_i64 const out{details::from_u64_bits<wasm_i64>(u64)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                constexpr ::std::size_t i64_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
                constexpr ::std::size_t i64_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
                static_assert(i64_begin <= curr_i64_stack_top && curr_i64_stack_top < i64_end);
                constexpr ::std::size_t new_i64_pos{details::ring_prev_pos(curr_i64_stack_top, i64_begin, i64_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_i64_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `i64.trunc_f64_s` (tail-call): truncates f64 to signed i64, trapping on invalid conversion.
    /// @details
    /// - Stack-top optimization: supported when f64 stack-top caching is enabled **and** the scalar stack-top ranges are fully merged. Otherwise the opcode
    ///   falls back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    /// @note This opcode may terminate the VM on NaN/out-of-range inputs (Wasm invalid conversion trap).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_f64_stack_top,
              ::std::size_t curr_i64_stack_top = curr_f64_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_trunc_f64_s(Type... type) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
            static_assert(range_begin <= curr_f64_stack_top && curr_f64_stack_top < range_end);

            wasm_f64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, curr_f64_stack_top>(type...)};
            ::std::int_least64_t const out64{details::trunc_float_to_int_s<::std::int_least64_t>(v)};
            wasm_i64 const out{static_cast<wasm_i64>(out64)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_f64, wasm_i64>())
                {
                    static_assert(curr_f64_stack_top == curr_i64_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_f64_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t i64_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
                    constexpr ::std::size_t i64_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
                    static_assert(i64_begin <= curr_i64_stack_top && curr_i64_stack_top < i64_end);
                    constexpr ::std::size_t new_i64_pos{details::ring_prev_pos(curr_i64_stack_top, i64_begin, i64_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_i64_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_f64 const v{get_curr_val_from_operand_stack_cache<wasm_f64>(type...)};
            ::std::int_least64_t const out64{details::trunc_float_to_int_s<::std::int_least64_t>(v)};
            wasm_i64 const out{static_cast<wasm_i64>(out64)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                constexpr ::std::size_t i64_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
                constexpr ::std::size_t i64_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
                static_assert(i64_begin <= curr_i64_stack_top && curr_i64_stack_top < i64_end);
                constexpr ::std::size_t new_i64_pos{details::ring_prev_pos(curr_i64_stack_top, i64_begin, i64_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_i64_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `i64.trunc_f64_u` (tail-call): truncates f64 to unsigned i64, trapping on invalid conversion.
    /// @details
    /// - Stack-top optimization: supported when f64 stack-top caching is enabled **and** the scalar stack-top ranges are fully merged. Otherwise the opcode
    ///   falls back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    /// @note This opcode may terminate the VM on NaN/out-of-range inputs (Wasm invalid conversion trap).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_f64_stack_top,
              ::std::size_t curr_i64_stack_top = curr_f64_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_trunc_f64_u(Type... type) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
            static_assert(range_begin <= curr_f64_stack_top && curr_f64_stack_top < range_end);

            wasm_f64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, curr_f64_stack_top>(type...)};
            ::std::uint_least64_t const u64{details::trunc_float_to_int_u<::std::uint_least64_t>(v)};
            wasm_i64 const out{details::from_u64_bits<wasm_i64>(u64)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_f64, wasm_i64>())
                {
                    static_assert(curr_f64_stack_top == curr_i64_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_f64_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t i64_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
                    constexpr ::std::size_t i64_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
                    static_assert(i64_begin <= curr_i64_stack_top && curr_i64_stack_top < i64_end);
                    constexpr ::std::size_t new_i64_pos{details::ring_prev_pos(curr_i64_stack_top, i64_begin, i64_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_i64_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_f64 const v{get_curr_val_from_operand_stack_cache<wasm_f64>(type...)};
            ::std::uint_least64_t const u64{details::trunc_float_to_int_u<::std::uint_least64_t>(v)};
            wasm_i64 const out{details::from_u64_bits<wasm_i64>(u64)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                constexpr ::std::size_t i64_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
                constexpr ::std::size_t i64_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
                static_assert(i64_begin <= curr_i64_stack_top && curr_i64_stack_top < i64_end);
                constexpr ::std::size_t new_i64_pos{details::ring_prev_pos(curr_i64_stack_top, i64_begin, i64_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_i64_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    // ---- int -> float ----

    /// @brief `f32.convert_i32_s` (tail-call): converts signed i32 to f32.
    /// @details
    /// - Stack-top optimization: supported when i32 stack-top caching is enabled **and** the i32/f32 stack-top ranges are merged. Otherwise the opcode falls
    ///   back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i32_stack_top,
              ::std::size_t curr_f32_stack_top = curr_i32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_convert_i32_s(Type... type) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
            static_assert(range_begin <= curr_i32_stack_top && curr_i32_stack_top < range_end);

            wasm_i32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            wasm_f32 const out{static_cast<wasm_f32>(static_cast<::std::int_least32_t>(v))};
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_i32, wasm_f32>())
                {
                    static_assert(curr_i32_stack_top == curr_f32_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, curr_i32_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t f32_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
                    constexpr ::std::size_t f32_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
                    static_assert(f32_begin <= curr_f32_stack_top && curr_f32_stack_top < f32_end);
                    constexpr ::std::size_t new_f32_pos{details::ring_prev_pos(curr_f32_stack_top, f32_begin, f32_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, new_f32_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            wasm_f32 const out{static_cast<wasm_f32>(static_cast<::std::int_least32_t>(v))};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
            {
                constexpr ::std::size_t f32_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
                constexpr ::std::size_t f32_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
                static_assert(f32_begin <= curr_f32_stack_top && curr_f32_stack_top < f32_end);
                constexpr ::std::size_t new_f32_pos{details::ring_prev_pos(curr_f32_stack_top, f32_begin, f32_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, new_f32_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `f32.convert_i32_u` (tail-call): converts unsigned i32 to f32.
    /// @details
    /// - Stack-top optimization: supported when i32 stack-top caching is enabled **and** the i32/f32 stack-top ranges are merged. Otherwise the opcode falls
    ///   back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i32_stack_top,
              ::std::size_t curr_f32_stack_top = curr_i32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_convert_i32_u(Type... type) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
            static_assert(range_begin <= curr_i32_stack_top && curr_i32_stack_top < range_end);

            wasm_i32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            ::std::uint_least32_t const u32{details::to_u32_bits(v)};
            wasm_f32 const out{static_cast<wasm_f32>(u32)};
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_i32, wasm_f32>())
                {
                    static_assert(curr_i32_stack_top == curr_f32_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, curr_i32_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t f32_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
                    constexpr ::std::size_t f32_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
                    static_assert(f32_begin <= curr_f32_stack_top && curr_f32_stack_top < f32_end);
                    constexpr ::std::size_t new_f32_pos{details::ring_prev_pos(curr_f32_stack_top, f32_begin, f32_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, new_f32_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            ::std::uint_least32_t const u32{details::to_u32_bits(v)};
            wasm_f32 const out{static_cast<wasm_f32>(u32)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
            {
                constexpr ::std::size_t f32_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
                constexpr ::std::size_t f32_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
                static_assert(f32_begin <= curr_f32_stack_top && curr_f32_stack_top < f32_end);
                constexpr ::std::size_t new_f32_pos{details::ring_prev_pos(curr_f32_stack_top, f32_begin, f32_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, new_f32_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `f64.convert_i32_s` (tail-call): converts signed i32 to f64.
    /// @details
    /// - Stack-top optimization: supported when i32 stack-top caching is enabled **and** the scalar stack-top ranges are fully merged. Otherwise the opcode
    ///   falls back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i32_stack_top,
              ::std::size_t curr_f64_stack_top = curr_i32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_convert_i32_s(Type... type) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
            static_assert(range_begin <= curr_i32_stack_top && curr_i32_stack_top < range_end);

            wasm_i32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            wasm_f64 const out{static_cast<wasm_f64>(static_cast<::std::int_least32_t>(v))};
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_i32, wasm_f64>())
                {
                    static_assert(curr_i32_stack_top == curr_f64_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, curr_i32_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t f64_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
                    constexpr ::std::size_t f64_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
                    static_assert(f64_begin <= curr_f64_stack_top && curr_f64_stack_top < f64_end);
                    constexpr ::std::size_t new_f64_pos{details::ring_prev_pos(curr_f64_stack_top, f64_begin, f64_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, new_f64_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            wasm_f64 const out{static_cast<wasm_f64>(static_cast<::std::int_least32_t>(v))};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
            {
                constexpr ::std::size_t f64_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
                constexpr ::std::size_t f64_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
                static_assert(f64_begin <= curr_f64_stack_top && curr_f64_stack_top < f64_end);
                constexpr ::std::size_t new_f64_pos{details::ring_prev_pos(curr_f64_stack_top, f64_begin, f64_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, new_f64_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `f64.convert_i32_u` (tail-call): converts unsigned i32 to f64.
    /// @details
    /// - Stack-top optimization: supported when i32 stack-top caching is enabled **and** the scalar stack-top ranges are fully merged. Otherwise the opcode
    ///   falls back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i32_stack_top,
              ::std::size_t curr_f64_stack_top = curr_i32_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_convert_i32_u(Type... type) UWVM_THROWS
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_i32>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_i32>()};
            static_assert(range_begin <= curr_i32_stack_top && curr_i32_stack_top < range_end);

            wasm_i32 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_i32_stack_top>(type...)};
            ::std::uint_least32_t const u32{details::to_u32_bits(v)};
            wasm_f64 const out{static_cast<wasm_f64>(u32)};
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_i32, wasm_f64>())
                {
                    static_assert(curr_i32_stack_top == curr_f64_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, curr_i32_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t f64_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
                    constexpr ::std::size_t f64_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
                    static_assert(f64_begin <= curr_f64_stack_top && curr_f64_stack_top < f64_end);
                    constexpr ::std::size_t new_f64_pos{details::ring_prev_pos(curr_f64_stack_top, f64_begin, f64_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, new_f64_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            ::std::uint_least32_t const u32{details::to_u32_bits(v)};
            wasm_f64 const out{static_cast<wasm_f64>(u32)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
            {
                constexpr ::std::size_t f64_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
                constexpr ::std::size_t f64_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
                static_assert(f64_begin <= curr_f64_stack_top && curr_f64_stack_top < f64_end);
                constexpr ::std::size_t new_f64_pos{details::ring_prev_pos(curr_f64_stack_top, f64_begin, f64_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, new_f64_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `f32.convert_i64_s` (tail-call): converts signed i64 to f32.
    /// @details
    /// - Stack-top optimization: supported when i64 stack-top caching is enabled **and** the scalar stack-top ranges are fully merged. Otherwise the opcode
    ///   falls back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i64_stack_top,
              ::std::size_t curr_f32_stack_top = curr_i64_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_convert_i64_s(Type... type) UWVM_THROWS
    {
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
            static_assert(range_begin <= curr_i64_stack_top && curr_i64_stack_top < range_end);

            wasm_i64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...)};
            wasm_f32 const out{static_cast<wasm_f32>(static_cast<::std::int_least64_t>(v))};
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_i64, wasm_f32>())
                {
                    static_assert(curr_i64_stack_top == curr_f32_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, curr_i64_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t f32_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
                    constexpr ::std::size_t f32_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
                    static_assert(f32_begin <= curr_f32_stack_top && curr_f32_stack_top < f32_end);
                    constexpr ::std::size_t new_f32_pos{details::ring_prev_pos(curr_f32_stack_top, f32_begin, f32_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, new_f32_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_i64 const v{get_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
            wasm_f32 const out{static_cast<wasm_f32>(static_cast<::std::int_least64_t>(v))};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
            {
                constexpr ::std::size_t f32_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
                constexpr ::std::size_t f32_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
                static_assert(f32_begin <= curr_f32_stack_top && curr_f32_stack_top < f32_end);
                constexpr ::std::size_t new_f32_pos{details::ring_prev_pos(curr_f32_stack_top, f32_begin, f32_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, new_f32_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `f32.convert_i64_u` (tail-call): converts unsigned i64 to f32.
    /// @details
    /// - Stack-top optimization: supported when i64 stack-top caching is enabled **and** the scalar stack-top ranges are fully merged. Otherwise the opcode
    ///   falls back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i64_stack_top,
              ::std::size_t curr_f32_stack_top = curr_i64_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_convert_i64_u(Type... type) UWVM_THROWS
    {
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
            static_assert(range_begin <= curr_i64_stack_top && curr_i64_stack_top < range_end);

            wasm_i64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...)};
            ::std::uint_least64_t const u64{details::to_u64_bits(v)};
            wasm_f32 const out{static_cast<wasm_f32>(u64)};
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_i64, wasm_f32>())
                {
                    static_assert(curr_i64_stack_top == curr_f32_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, curr_i64_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t f32_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
                    constexpr ::std::size_t f32_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
                    static_assert(f32_begin <= curr_f32_stack_top && curr_f32_stack_top < f32_end);
                    constexpr ::std::size_t new_f32_pos{details::ring_prev_pos(curr_f32_stack_top, f32_begin, f32_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, new_f32_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_i64 const v{get_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
            ::std::uint_least64_t const u64{details::to_u64_bits(v)};
            wasm_f32 const out{static_cast<wasm_f32>(u64)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f32>())
            {
                constexpr ::std::size_t f32_begin{details::stacktop_begin_pos<CompileOption, wasm_f32>()};
                constexpr ::std::size_t f32_end{details::stacktop_end_pos<CompileOption, wasm_f32>()};
                static_assert(f32_begin <= curr_f32_stack_top && curr_f32_stack_top < f32_end);
                constexpr ::std::size_t new_f32_pos{details::ring_prev_pos(curr_f32_stack_top, f32_begin, f32_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f32, new_f32_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `f64.convert_i64_s` (tail-call): converts signed i64 to f64.
    /// @details
    /// - Stack-top optimization: supported when i64 stack-top caching is enabled **and** the scalar stack-top ranges are fully merged. Otherwise the opcode
    ///   falls back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i64_stack_top,
              ::std::size_t curr_f64_stack_top = curr_i64_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_convert_i64_s(Type... type) UWVM_THROWS
    {
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
            static_assert(range_begin <= curr_i64_stack_top && curr_i64_stack_top < range_end);

            wasm_i64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...)};
            wasm_f64 const out{static_cast<wasm_f64>(static_cast<::std::int_least64_t>(v))};
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_i64, wasm_f64>())
                {
                    static_assert(curr_i64_stack_top == curr_f64_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, curr_i64_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t f64_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
                    constexpr ::std::size_t f64_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
                    static_assert(f64_begin <= curr_f64_stack_top && curr_f64_stack_top < f64_end);
                    constexpr ::std::size_t new_f64_pos{details::ring_prev_pos(curr_f64_stack_top, f64_begin, f64_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, new_f64_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_i64 const v{get_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
            wasm_f64 const out{static_cast<wasm_f64>(static_cast<::std::int_least64_t>(v))};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
            {
                constexpr ::std::size_t f64_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
                constexpr ::std::size_t f64_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
                static_assert(f64_begin <= curr_f64_stack_top && curr_f64_stack_top < f64_end);
                constexpr ::std::size_t new_f64_pos{details::ring_prev_pos(curr_f64_stack_top, f64_begin, f64_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, new_f64_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `f64.convert_i64_u` (tail-call): converts unsigned i64 to f64.
    /// @details
    /// - Stack-top optimization: supported when i64 stack-top caching is enabled **and** the scalar stack-top ranges are fully merged. Otherwise the opcode
    ///   falls back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i64_stack_top,
              ::std::size_t curr_f64_stack_top = curr_i64_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_convert_i64_u(Type... type) UWVM_THROWS
    {
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
            static_assert(range_begin <= curr_i64_stack_top && curr_i64_stack_top < range_end);

            wasm_i64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...)};
            ::std::uint_least64_t const u64{details::to_u64_bits(v)};
            wasm_f64 const out{static_cast<wasm_f64>(u64)};
            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_i64, wasm_f64>())
                {
                    static_assert(curr_i64_stack_top == curr_f64_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, curr_i64_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t f64_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
                    constexpr ::std::size_t f64_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
                    static_assert(f64_begin <= curr_f64_stack_top && curr_f64_stack_top < f64_end);
                    constexpr ::std::size_t new_f64_pos{details::ring_prev_pos(curr_f64_stack_top, f64_begin, f64_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, new_f64_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_i64 const v{get_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
            ::std::uint_least64_t const u64{details::to_u64_bits(v)};
            wasm_f64 const out{static_cast<wasm_f64>(u64)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
            {
                constexpr ::std::size_t f64_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
                constexpr ::std::size_t f64_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
                static_assert(f64_begin <= curr_f64_stack_top && curr_f64_stack_top < f64_end);
                constexpr ::std::size_t new_f64_pos{details::ring_prev_pos(curr_f64_stack_top, f64_begin, f64_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, new_f64_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `i64.reinterpret_f64` (tail-call): bitcasts f64 to i64.
    /// @details
    /// - Stack-top optimization: supported when f64 stack-top caching is enabled **and** the scalar stack-top ranges are fully merged. Otherwise the opcode
    ///   falls back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_f64_stack_top,
              ::std::size_t curr_i64_stack_top = curr_f64_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_reinterpret_f64(Type... type) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
            static_assert(range_begin <= curr_f64_stack_top && curr_f64_stack_top < range_end);

            wasm_f64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_f64, curr_f64_stack_top>(type...)};
            ::std::uint_least64_t const bits{::std::bit_cast<::std::uint_least64_t>(v)};
            wasm_i64 const out{details::from_u64_bits<wasm_i64>(bits)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_f64, wasm_i64>())
                {
                    static_assert(curr_f64_stack_top == curr_i64_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, curr_f64_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t i64_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
                    constexpr ::std::size_t i64_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
                    static_assert(i64_begin <= curr_i64_stack_top && curr_i64_stack_top < i64_end);
                    constexpr ::std::size_t new_i64_pos{details::ring_prev_pos(curr_i64_stack_top, i64_begin, i64_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_i64_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_f64 const v{get_curr_val_from_operand_stack_cache<wasm_f64>(type...)};
            ::std::uint_least64_t const bits{::std::bit_cast<::std::uint_least64_t>(v)};
            wasm_i64 const out{details::from_u64_bits<wasm_i64>(bits)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
            {
                constexpr ::std::size_t i64_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
                constexpr ::std::size_t i64_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
                static_assert(i64_begin <= curr_i64_stack_top && curr_i64_stack_top < i64_end);
                constexpr ::std::size_t new_i64_pos{details::ring_prev_pos(curr_i64_stack_top, i64_begin, i64_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, new_i64_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    /// @brief `f64.reinterpret_i64` (tail-call): bitcasts i64 to f64.
    /// @details
    /// - Stack-top optimization: supported when i64 stack-top caching is enabled **and** the scalar stack-top ranges are fully merged. Otherwise the opcode
    ///   falls back to operand-stack execution.
    /// - `type[0]` layout: `[opfunc_ptr][next_opfunc_ptr]` (no immediates).
    template <uwvm_interpreter_translate_option_t CompileOption,
              ::std::size_t curr_i64_stack_top,
              ::std::size_t curr_f64_stack_top = curr_i64_stack_top,
              uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_reinterpret_i64(Type... type) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(Type) >= 2uz);
        static_assert(::std::same_as<Type...[0u], ::std::byte const*>);

        if constexpr(details::stacktop_enabled_for<CompileOption, wasm_i64>())
        {
            constexpr ::std::size_t range_begin{details::stacktop_begin_pos<CompileOption, wasm_i64>()};
            constexpr ::std::size_t range_end{details::stacktop_end_pos<CompileOption, wasm_i64>()};
            static_assert(range_begin <= curr_i64_stack_top && curr_i64_stack_top < range_end);

            wasm_i64 const v{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_i64_stack_top>(type...)};
            ::std::uint_least64_t const bits{details::to_u64_bits(v)};
            wasm_f64 const out{::std::bit_cast<wasm_f64>(bits)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
            {
                if constexpr(details::stacktop_ranges_merged<CompileOption, wasm_i64, wasm_f64>())
                {
                    static_assert(curr_i64_stack_top == curr_f64_stack_top);
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, curr_i64_stack_top>(out, type...);
                }
                else
                {
                    constexpr ::std::size_t f64_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
                    constexpr ::std::size_t f64_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
                    static_assert(f64_begin <= curr_f64_stack_top && curr_f64_stack_top < f64_end);
                    constexpr ::std::size_t new_f64_pos{details::ring_prev_pos(curr_f64_stack_top, f64_begin, f64_end)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, new_f64_pos>(out, type...);
                }
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }
        else
        {
            wasm_i64 const v{get_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
            ::std::uint_least64_t const bits{details::to_u64_bits(v)};
            wasm_f64 const out{::std::bit_cast<wasm_f64>(bits)};

            if constexpr(details::stacktop_enabled_for<CompileOption, wasm_f64>())
            {
                constexpr ::std::size_t f64_begin{details::stacktop_begin_pos<CompileOption, wasm_f64>()};
                constexpr ::std::size_t f64_end{details::stacktop_end_pos<CompileOption, wasm_f64>()};
                static_assert(f64_begin <= curr_f64_stack_top && curr_f64_stack_top < f64_end);
                constexpr ::std::size_t new_f64_pos{details::ring_prev_pos(curr_f64_stack_top, f64_begin, f64_end)};
                details::set_curr_val_to_stacktop_cache<CompileOption, wasm_f64, new_f64_pos>(out, type...);
            }
            else
            {
                ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
                type...[1u] += sizeof(out);
            }
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    // =========================
    // Non-tailcall (byref only)
    // =========================

    /// @brief Conversion opcodes (non-tail-call/byref): operate purely on the operand stack.
    /// @details
    /// - Stack-top optimization: not supported (byref mode disables stack-top caching and enforces all stack-top ranges to be `SIZE_MAX`).
    /// - `type[0]` layout: `[opfunc_byref_ptr][next_opfunc_byref_ptr]...` (no immediates for convert opcodes); after execution `typeref...[0]` points at the
    /// next opfunc slot.
    /// @note Dispatch of the next opfunc is driven by the outer interpreter loop in byref mode.

    /// @brief `i32.wrap_i64` (non-tail-call/byref): truncates i64 to i32 (low 32 bits).
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_wrap_i64(TypeRef & ... typeref) UWVM_THROWS
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
        ::std::uint_least32_t const low{static_cast<::std::uint_least32_t>(details::to_u64_bits(v))};
        wasm_i32 const out{details::from_u32_bits<wasm_i32>(low)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i32.trunc_f32_s` (non-tail-call/byref): truncates f32 to signed i32, trapping on invalid conversion.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_trunc_f32_s(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f32 const v{get_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
        ::std::int_least32_t const out32{details::trunc_float_to_int_s<::std::int_least32_t>(v)};
        wasm_i32 const out{static_cast<wasm_i32>(out32)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i32.trunc_f32_u` (non-tail-call/byref): truncates f32 to unsigned i32, trapping on invalid conversion.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_trunc_f32_u(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f32 const v{get_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
        ::std::uint_least32_t const u32{details::trunc_float_to_int_u<::std::uint_least32_t>(v)};
        wasm_i32 const out{details::from_u32_bits<wasm_i32>(u32)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_trunc_f64_s(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f64 const v{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        ::std::int_least32_t const out32{details::trunc_float_to_int_s<::std::int_least32_t>(v)};
        wasm_i32 const out{static_cast<wasm_i32>(out32)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i32.trunc_f64_u` (non-tail-call/byref): truncates f64 to unsigned i32, trapping on invalid conversion.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_trunc_f64_u(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f64 const v{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        ::std::uint_least32_t const u32{details::trunc_float_to_int_u<::std::uint_least32_t>(v)};
        wasm_i32 const out{details::from_u32_bits<wasm_i32>(u32)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i64.extend_i32_s` (non-tail-call/byref): sign-extends i32 to i64.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_extend_i32_s(TypeRef & ... typeref) UWVM_THROWS
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

        wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::int_least64_t const out64{static_cast<::std::int_least64_t>(static_cast<::std::int_least32_t>(v))};
        wasm_i64 const out{static_cast<wasm_i64>(out64)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i64.extend_i32_u` (non-tail-call/byref): zero-extends i32 to i64.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_extend_i32_u(TypeRef & ... typeref) UWVM_THROWS
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

        wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::uint_least32_t const u32{details::to_u32_bits(v)};
        wasm_i64 const out{static_cast<wasm_i64>(static_cast<::std::uint_least64_t>(u32))};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i64.trunc_f32_s` (non-tail-call/byref): truncates f32 to signed i64, trapping on invalid conversion.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_trunc_f32_s(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f32 const v{get_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
        ::std::int_least64_t const out64{details::trunc_float_to_int_s<::std::int_least64_t>(v)};
        wasm_i64 const out{static_cast<wasm_i64>(out64)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i64.trunc_f32_u` (non-tail-call/byref): truncates f32 to unsigned i64, trapping on invalid conversion.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_trunc_f32_u(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f32 const v{get_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
        ::std::uint_least64_t const u64{details::trunc_float_to_int_u<::std::uint_least64_t>(v)};
        wasm_i64 const out{details::from_u64_bits<wasm_i64>(u64)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i64.trunc_f64_s` (non-tail-call/byref): truncates f64 to signed i64, trapping on invalid conversion.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_trunc_f64_s(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f64 const v{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        ::std::int_least64_t const out64{details::trunc_float_to_int_s<::std::int_least64_t>(v)};
        wasm_i64 const out{static_cast<wasm_i64>(out64)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i64.trunc_f64_u` (non-tail-call/byref): truncates f64 to unsigned i64, trapping on invalid conversion.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_trunc_f64_u(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f64 const v{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        ::std::uint_least64_t const u64{details::trunc_float_to_int_u<::std::uint_least64_t>(v)};
        wasm_i64 const out{details::from_u64_bits<wasm_i64>(u64)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f32.convert_i32_s` (non-tail-call/byref): converts signed i32 to f32.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_convert_i32_s(TypeRef & ... typeref) UWVM_THROWS
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

        wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_f32 const out{static_cast<wasm_f32>(static_cast<::std::int_least32_t>(v))};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f32.convert_i32_u` (non-tail-call/byref): converts unsigned i32 to f32.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_convert_i32_u(TypeRef & ... typeref) UWVM_THROWS
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

        wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::uint_least32_t const u32{details::to_u32_bits(v)};
        wasm_f32 const out{static_cast<wasm_f32>(u32)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f32.convert_i64_s` (non-tail-call/byref): converts signed i64 to f32.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_convert_i64_s(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i64 const v{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        wasm_f32 const out{static_cast<wasm_f32>(static_cast<::std::int_least64_t>(v))};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f32.convert_i64_u` (non-tail-call/byref): converts unsigned i64 to f32.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_convert_i64_u(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i64 const v{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        ::std::uint_least64_t const u64{details::to_u64_bits(v)};
        wasm_f32 const out{static_cast<wasm_f32>(u64)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f32.demote_f64` (non-tail-call/byref): converts f64 to f32.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_demote_f64(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f64 const v{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        wasm_f32 const out{static_cast<wasm_f32>(v)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f64.convert_i32_s` (non-tail-call/byref): converts signed i32 to f64.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_convert_i32_s(TypeRef & ... typeref) UWVM_THROWS
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

        wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_f64 const out{static_cast<wasm_f64>(static_cast<::std::int_least32_t>(v))};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f64.convert_i32_u` (non-tail-call/byref): converts unsigned i32 to f64.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_convert_i32_u(TypeRef & ... typeref) UWVM_THROWS
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

        wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        ::std::uint_least32_t const u32{details::to_u32_bits(v)};
        wasm_f64 const out{static_cast<wasm_f64>(u32)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f64.convert_i64_s` (non-tail-call/byref): converts signed i64 to f64.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_convert_i64_s(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i64 const v{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        wasm_f64 const out{static_cast<wasm_f64>(static_cast<::std::int_least64_t>(v))};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f64.convert_i64_u` (non-tail-call/byref): converts unsigned i64 to f64.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_convert_i64_u(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i64 const v{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        ::std::uint_least64_t const u64{details::to_u64_bits(v)};
        wasm_f64 const out{static_cast<wasm_f64>(u64)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f64.promote_f32` (non-tail-call/byref): converts f32 to f64.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_promote_f32(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f32 const v{get_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
        wasm_f64 const out{static_cast<wasm_f64>(v)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i32.reinterpret_f32` (non-tail-call/byref): bitcasts f32 to i32.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i32_reinterpret_f32(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f32 const v{get_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
        ::std::uint_least32_t const bits{::std::bit_cast<::std::uint_least32_t>(v)};
        wasm_i32 const out{details::from_u32_bits<wasm_i32>(bits)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f32.reinterpret_i32` (non-tail-call/byref): bitcasts i32 to f32.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f32_reinterpret_i32(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
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
        ::std::uint_least32_t const bits{details::to_u32_bits(v)};
        wasm_f32 const out{::std::bit_cast<wasm_f32>(bits)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `i64.reinterpret_f64` (non-tail-call/byref): bitcasts f64 to i64.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_i64_reinterpret_f64(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f64 const v{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        ::std::uint_least64_t const bits{::std::bit_cast<::std::uint_least64_t>(v)};
        wasm_i64 const out{details::from_u64_bits<wasm_i64>(bits)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief `f64.reinterpret_i64` (non-tail-call/byref): bitcasts i64 to f64.
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_MACRO inline constexpr void uwvmint_f64_reinterpret_i64(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
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
        ::std::uint_least64_t const bits{details::to_u64_bits(v)};
        wasm_f64 const out{::std::bit_cast<wasm_f64>(bits)};

        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    /// @brief Translation helpers for convert opcodes.
    /// @details
    /// - Tail-call mode: returns a specialized `uwvm_interpreter_opfunc_t<...>` based on the current stack-top cursor position so that stack-top cached
    /// operands
    ///   are accessed via the correct `curr_stack_top` template parameter.
    /// - Non-tail-call/byref mode: stack-top caching is disabled; translation typically returns the byref variant directly.
    /// - `type[0]` layout: not applicable in translation; these helpers do not manipulate the bytecode stream pointer.
    namespace translate
    {
        namespace details
        {
            /// @brief Compile-time selector for stack-top-aware opfuncs (tail-call).
            /// @details
            /// - `pos` is a runtime cursor (e.g., `curr_stacktop.i32_stack_top_curr_pos`) used to choose the matching `Curr` specialization.
            /// - `OpWrapper` must provide `template <Opt, Pos, Type...> inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr()`.
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

            template <typename OperandT>
            inline constexpr ::std::size_t stacktop_currpos(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
            {
                if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>) { return curr_stacktop.i32_stack_top_curr_pos; }
                else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>)
                {
                    return curr_stacktop.i64_stack_top_curr_pos;
                }
                else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>)
                {
                    return curr_stacktop.f32_stack_top_curr_pos;
                }
                else if constexpr(::std::same_as<OperandT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>)
                {
                    return curr_stacktop.f64_stack_top_curr_pos;
                }
                else
                {
                    return SIZE_MAX;
                }
            }

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t OutCurr,
                      ::std::size_t OutEnd,
                      ::std::size_t InCurr,
                      ::std::size_t InEnd,
                      typename OpWrapper,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_by_currpos_impl_2d(::std::size_t out_pos, ::std::size_t in_pos) noexcept
            {
                static_assert(OutCurr < OutEnd);
                static_assert(InCurr < InEnd);

                if(out_pos == OutCurr)
                {
                    if(in_pos == InCurr) { return OpWrapper::template fptr<CompileOption, OutCurr, InCurr, Type...>(); }
                    else
                    {
                        if constexpr(InCurr + 1uz < InEnd)
                        {
                            return select_stacktop_fptr_by_currpos_impl_2d<CompileOption, OutCurr, OutEnd, InCurr + 1uz, InEnd, OpWrapper, Type...>(out_pos,
                                                                                                                                                    in_pos);
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
                    if constexpr(OutCurr + 1uz < OutEnd)
                    {
                        return select_stacktop_fptr_by_currpos_impl_2d<CompileOption, OutCurr + 1uz, OutEnd, InCurr, InEnd, OpWrapper, Type...>(out_pos,
                                                                                                                                                in_pos);
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
                      typename InT,
                      typename OutT,
                      ::std::size_t InBegin,
                      ::std::size_t InEnd,
                      ::std::size_t OutBegin,
                      ::std::size_t OutEnd,
                      typename OpWrapper1D,
                      typename OpWrapper2D,
                      typename OpWrapperOutOnly,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_unary_convert_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
            {
                if constexpr(InBegin != InEnd)
                {
                    if constexpr(OutBegin != OutEnd)
                    {
                        if constexpr(::uwvm2::runtime::compiler::uwvm_int::optable::details::stacktop_ranges_merged<CompileOption, InT, OutT>())
                        {
                            return select_stacktop_fptr_by_currpos_impl<CompileOption, InBegin, InEnd, OpWrapper1D, Type...>(
                                stacktop_currpos<InT>(curr_stacktop));
                        }
                        else
                        {
                            return select_stacktop_fptr_by_currpos_impl_2d<CompileOption, OutBegin, OutEnd, InBegin, InEnd, OpWrapper2D, Type...>(
                                stacktop_currpos<OutT>(curr_stacktop),
                                stacktop_currpos<InT>(curr_stacktop));
                        }
                    }
                    else
                    {
                        return select_stacktop_fptr_by_currpos_impl<CompileOption, InBegin, InEnd, OpWrapper1D, Type...>(stacktop_currpos<InT>(curr_stacktop));
                    }
                }
                else
                {
                    if constexpr(OutBegin != OutEnd)
                    {
                        return select_stacktop_fptr_by_currpos_impl<CompileOption, OutBegin, OutEnd, OpWrapperOutOnly, Type...>(
                            stacktop_currpos<OutT>(curr_stacktop));
                    }
                    else
                    {
                        return OpWrapper1D::template fptr<CompileOption, 0uz, Type...>();
                    }
                }
            }

            struct i32_wrap_i64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_wrap_i64<Opt, Pos, Type...>; }
            };

            struct i32_trunc_f32_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_trunc_f32_s<Opt, Pos, Type...>; }
            };

            struct i32_trunc_f32_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_trunc_f32_u<Opt, Pos, Type...>; }
            };

            struct i32_trunc_f64_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_trunc_f64_s<Opt, Pos, Type...>; }
            };

            struct i32_trunc_f64_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_trunc_f64_u<Opt, Pos, Type...>; }
            };

            struct i64_extend_i32_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_extend_i32_s<Opt, Pos, Type...>; }
            };

            struct i64_extend_i32_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_extend_i32_u<Opt, Pos, Type...>; }
            };

            struct i64_trunc_f32_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_trunc_f32_s<Opt, Pos, Type...>; }
            };

            struct i64_trunc_f32_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_trunc_f32_u<Opt, Pos, Type...>; }
            };

            struct i64_trunc_f64_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_trunc_f64_s<Opt, Pos, Type...>; }
            };

            struct i64_trunc_f64_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_trunc_f64_u<Opt, Pos, Type...>; }
            };

            struct f32_convert_i32_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_convert_i32_s<Opt, Pos, Type...>; }
            };

            struct f32_convert_i32_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_convert_i32_u<Opt, Pos, Type...>; }
            };

            struct f32_convert_i64_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_convert_i64_s<Opt, Pos, Type...>; }
            };

            struct f32_convert_i64_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_convert_i64_u<Opt, Pos, Type...>; }
            };

            struct f32_demote_f64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_demote_f64<Opt, Pos, Type...>; }
            };

            struct f64_convert_i32_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_convert_i32_s<Opt, Pos, Type...>; }
            };

            struct f64_convert_i32_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_convert_i32_u<Opt, Pos, Type...>; }
            };

            struct f64_convert_i64_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_convert_i64_s<Opt, Pos, Type...>; }
            };

            struct f64_convert_i64_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_convert_i64_u<Opt, Pos, Type...>; }
            };

            struct f64_promote_f32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_promote_f32<Opt, Pos, Type...>; }
            };

            struct i32_reinterpret_f32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_reinterpret_f32<Opt, Pos, Type...>; }
            };

            struct i64_reinterpret_f64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_reinterpret_f64<Opt, Pos, Type...>; }
            };

            struct f32_reinterpret_i32_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_reinterpret_i32<Opt, Pos, Type...>; }
            };

            struct f64_reinterpret_i64_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_reinterpret_i64<Opt, Pos, Type...>; }
            };

            // Disjoint-ring + output-only wrappers
            //
            // Conversions move one value from an input type (InT) to an output type (OutT). When the corresponding
            // stack-top caches are disjoint, the translator must carry two independent ring cursors and therefore
            // selects a 2D-specialized opcode (OutPos × InPos). When the input type has no ring but the output type
            // does, we select an output-only opcode that reads from the operand stack and pushes into the output ring.

            struct i32_wrap_i64_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_wrap_i64<Opt, I64Pos, I32Pos, Type...>; }
            };

            struct i32_wrap_i64_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_wrap_i64<Opt, 0uz, I32Pos, Type...>; }
            };

            struct i32_trunc_f32_s_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, ::std::size_t F32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_trunc_f32_s<Opt, F32Pos, I32Pos, Type...>; }
            };

            struct i32_trunc_f32_s_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_trunc_f32_s<Opt, 0uz, I32Pos, Type...>; }
            };

            struct i32_trunc_f32_u_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, ::std::size_t F32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_trunc_f32_u<Opt, F32Pos, I32Pos, Type...>; }
            };

            struct i32_trunc_f32_u_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_trunc_f32_u<Opt, 0uz, I32Pos, Type...>; }
            };

            struct i32_trunc_f64_s_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, ::std::size_t F64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_trunc_f64_s<Opt, F64Pos, I32Pos, Type...>; }
            };

            struct i32_trunc_f64_s_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_trunc_f64_s<Opt, 0uz, I32Pos, Type...>; }
            };

            struct i32_trunc_f64_u_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, ::std::size_t F64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_trunc_f64_u<Opt, F64Pos, I32Pos, Type...>; }
            };

            struct i32_trunc_f64_u_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_trunc_f64_u<Opt, 0uz, I32Pos, Type...>; }
            };

            struct i64_extend_i32_s_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_extend_i32_s<Opt, I32Pos, I64Pos, Type...>; }
            };

            struct i64_extend_i32_s_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_extend_i32_s<Opt, 0uz, I64Pos, Type...>; }
            };

            struct i64_extend_i32_u_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_extend_i32_u<Opt, I32Pos, I64Pos, Type...>; }
            };

            struct i64_extend_i32_u_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_extend_i32_u<Opt, 0uz, I64Pos, Type...>; }
            };

            struct i64_trunc_f32_s_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, ::std::size_t F32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_trunc_f32_s<Opt, F32Pos, I64Pos, Type...>; }
            };

            struct i64_trunc_f32_s_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_trunc_f32_s<Opt, 0uz, I64Pos, Type...>; }
            };

            struct i64_trunc_f32_u_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, ::std::size_t F32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_trunc_f32_u<Opt, F32Pos, I64Pos, Type...>; }
            };

            struct i64_trunc_f32_u_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_trunc_f32_u<Opt, 0uz, I64Pos, Type...>; }
            };

            struct i64_trunc_f64_s_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, ::std::size_t F64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_trunc_f64_s<Opt, F64Pos, I64Pos, Type...>; }
            };

            struct i64_trunc_f64_s_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_trunc_f64_s<Opt, 0uz, I64Pos, Type...>; }
            };

            struct i64_trunc_f64_u_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, ::std::size_t F64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_trunc_f64_u<Opt, F64Pos, I64Pos, Type...>; }
            };

            struct i64_trunc_f64_u_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_trunc_f64_u<Opt, 0uz, I64Pos, Type...>; }
            };

            struct f32_convert_i32_s_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F32Pos, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_convert_i32_s<Opt, I32Pos, F32Pos, Type...>; }
            };

            struct f32_convert_i32_s_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_convert_i32_s<Opt, 0uz, F32Pos, Type...>; }
            };

            struct f32_convert_i32_u_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F32Pos, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_convert_i32_u<Opt, I32Pos, F32Pos, Type...>; }
            };

            struct f32_convert_i32_u_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_convert_i32_u<Opt, 0uz, F32Pos, Type...>; }
            };

            struct f32_convert_i64_s_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F32Pos, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_convert_i64_s<Opt, I64Pos, F32Pos, Type...>; }
            };

            struct f32_convert_i64_s_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_convert_i64_s<Opt, 0uz, F32Pos, Type...>; }
            };

            struct f32_convert_i64_u_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F32Pos, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_convert_i64_u<Opt, I64Pos, F32Pos, Type...>; }
            };

            struct f32_convert_i64_u_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_convert_i64_u<Opt, 0uz, F32Pos, Type...>; }
            };

            struct f32_demote_f64_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F32Pos, ::std::size_t F64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_demote_f64<Opt, F64Pos, F32Pos, Type...>; }
            };

            struct f32_demote_f64_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_demote_f64<Opt, 0uz, F32Pos, Type...>; }
            };

            struct f64_convert_i32_s_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F64Pos, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_convert_i32_s<Opt, I32Pos, F64Pos, Type...>; }
            };

            struct f64_convert_i32_s_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_convert_i32_s<Opt, 0uz, F64Pos, Type...>; }
            };

            struct f64_convert_i32_u_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F64Pos, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_convert_i32_u<Opt, I32Pos, F64Pos, Type...>; }
            };

            struct f64_convert_i32_u_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_convert_i32_u<Opt, 0uz, F64Pos, Type...>; }
            };

            struct f64_convert_i64_s_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F64Pos, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_convert_i64_s<Opt, I64Pos, F64Pos, Type...>; }
            };

            struct f64_convert_i64_s_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_convert_i64_s<Opt, 0uz, F64Pos, Type...>; }
            };

            struct f64_convert_i64_u_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F64Pos, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_convert_i64_u<Opt, I64Pos, F64Pos, Type...>; }
            };

            struct f64_convert_i64_u_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_convert_i64_u<Opt, 0uz, F64Pos, Type...>; }
            };

            struct f64_promote_f32_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F64Pos, ::std::size_t F32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_promote_f32<Opt, F32Pos, F64Pos, Type...>; }
            };

            struct f64_promote_f32_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_promote_f32<Opt, 0uz, F64Pos, Type...>; }
            };

            struct i32_reinterpret_f32_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, ::std::size_t F32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_reinterpret_f32<Opt, F32Pos, I32Pos, Type...>; }
            };

            struct i32_reinterpret_f32_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_reinterpret_f32<Opt, 0uz, I32Pos, Type...>; }
            };

            struct i64_reinterpret_f64_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, ::std::size_t F64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_reinterpret_f64<Opt, F64Pos, I64Pos, Type...>; }
            };

            struct i64_reinterpret_f64_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_reinterpret_f64<Opt, 0uz, I64Pos, Type...>; }
            };

            struct f32_reinterpret_i32_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F32Pos, ::std::size_t I32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_reinterpret_i32<Opt, I32Pos, F32Pos, Type...>; }
            };

            struct f32_reinterpret_i32_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F32Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_reinterpret_i32<Opt, 0uz, F32Pos, Type...>; }
            };

            struct f64_reinterpret_i64_op_2d
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F64Pos, ::std::size_t I64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_reinterpret_i64<Opt, I64Pos, F64Pos, Type...>; }
            };

            struct f64_reinterpret_i64_op_out_only
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t F64Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_reinterpret_i64<Opt, 0uz, F64Pos, Type...>; }
            };
        }  // namespace details

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_wrap_i64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
            using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_i64,
                                                      wasm_i32,
                                                      CompileOption.i64_stack_top_begin_pos,
                                                      CompileOption.i64_stack_top_end_pos,
                                                      CompileOption.i32_stack_top_begin_pos,
                                                      CompileOption.i32_stack_top_end_pos,
                                                      details::i32_wrap_i64_op,
                                                      details::i32_wrap_i64_op_2d,
                                                      details::i32_wrap_i64_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_wrap_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_wrap_i64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_trunc_f32_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_f32,
                                                      wasm_i32,
                                                      CompileOption.f32_stack_top_begin_pos,
                                                      CompileOption.f32_stack_top_end_pos,
                                                      CompileOption.i32_stack_top_begin_pos,
                                                      CompileOption.i32_stack_top_end_pos,
                                                      details::i32_trunc_f32_s_op,
                                                      details::i32_trunc_f32_s_op_2d,
                                                      details::i32_trunc_f32_s_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_trunc_f32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_trunc_f32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_trunc_f32_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_f32,
                                                      wasm_i32,
                                                      CompileOption.f32_stack_top_begin_pos,
                                                      CompileOption.f32_stack_top_end_pos,
                                                      CompileOption.i32_stack_top_begin_pos,
                                                      CompileOption.i32_stack_top_end_pos,
                                                      details::i32_trunc_f32_u_op,
                                                      details::i32_trunc_f32_u_op_2d,
                                                      details::i32_trunc_f32_u_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_trunc_f32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_trunc_f32_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_trunc_f64_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_f64,
                                                      wasm_i32,
                                                      CompileOption.f64_stack_top_begin_pos,
                                                      CompileOption.f64_stack_top_end_pos,
                                                      CompileOption.i32_stack_top_begin_pos,
                                                      CompileOption.i32_stack_top_end_pos,
                                                      details::i32_trunc_f64_s_op,
                                                      details::i32_trunc_f64_s_op_2d,
                                                      details::i32_trunc_f64_s_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_trunc_f64_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_trunc_f64_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_trunc_f64_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_f64,
                                                      wasm_i32,
                                                      CompileOption.f64_stack_top_begin_pos,
                                                      CompileOption.f64_stack_top_end_pos,
                                                      CompileOption.i32_stack_top_begin_pos,
                                                      CompileOption.i32_stack_top_end_pos,
                                                      details::i32_trunc_f64_u_op,
                                                      details::i32_trunc_f64_u_op_2d,
                                                      details::i32_trunc_f64_u_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_trunc_f64_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_trunc_f64_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_extend_i32_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
            using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_i32,
                                                      wasm_i64,
                                                      CompileOption.i32_stack_top_begin_pos,
                                                      CompileOption.i32_stack_top_end_pos,
                                                      CompileOption.i64_stack_top_begin_pos,
                                                      CompileOption.i64_stack_top_end_pos,
                                                      details::i64_extend_i32_s_op,
                                                      details::i64_extend_i32_s_op_2d,
                                                      details::i64_extend_i32_s_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_extend_i32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_extend_i32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_extend_i32_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
            using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_i32,
                                                      wasm_i64,
                                                      CompileOption.i32_stack_top_begin_pos,
                                                      CompileOption.i32_stack_top_end_pos,
                                                      CompileOption.i64_stack_top_begin_pos,
                                                      CompileOption.i64_stack_top_end_pos,
                                                      details::i64_extend_i32_u_op,
                                                      details::i64_extend_i32_u_op_2d,
                                                      details::i64_extend_i32_u_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_extend_i32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_extend_i32_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_trunc_f32_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
            using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_f32,
                                                      wasm_i64,
                                                      CompileOption.f32_stack_top_begin_pos,
                                                      CompileOption.f32_stack_top_end_pos,
                                                      CompileOption.i64_stack_top_begin_pos,
                                                      CompileOption.i64_stack_top_end_pos,
                                                      details::i64_trunc_f32_s_op,
                                                      details::i64_trunc_f32_s_op_2d,
                                                      details::i64_trunc_f32_s_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_trunc_f32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_trunc_f32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_trunc_f32_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
            using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_f32,
                                                      wasm_i64,
                                                      CompileOption.f32_stack_top_begin_pos,
                                                      CompileOption.f32_stack_top_end_pos,
                                                      CompileOption.i64_stack_top_begin_pos,
                                                      CompileOption.i64_stack_top_end_pos,
                                                      details::i64_trunc_f32_u_op,
                                                      details::i64_trunc_f32_u_op_2d,
                                                      details::i64_trunc_f32_u_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_trunc_f32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_trunc_f32_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_trunc_f64_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
            using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_f64,
                                                      wasm_i64,
                                                      CompileOption.f64_stack_top_begin_pos,
                                                      CompileOption.f64_stack_top_end_pos,
                                                      CompileOption.i64_stack_top_begin_pos,
                                                      CompileOption.i64_stack_top_end_pos,
                                                      details::i64_trunc_f64_s_op,
                                                      details::i64_trunc_f64_s_op_2d,
                                                      details::i64_trunc_f64_s_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_trunc_f64_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_trunc_f64_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_trunc_f64_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
            using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_f64,
                                                      wasm_i64,
                                                      CompileOption.f64_stack_top_begin_pos,
                                                      CompileOption.f64_stack_top_end_pos,
                                                      CompileOption.i64_stack_top_begin_pos,
                                                      CompileOption.i64_stack_top_end_pos,
                                                      details::i64_trunc_f64_u_op,
                                                      details::i64_trunc_f64_u_op_2d,
                                                      details::i64_trunc_f64_u_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_trunc_f64_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_trunc_f64_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_convert_i32_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
            using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_i32,
                                                      wasm_f32,
                                                      CompileOption.i32_stack_top_begin_pos,
                                                      CompileOption.i32_stack_top_end_pos,
                                                      CompileOption.f32_stack_top_begin_pos,
                                                      CompileOption.f32_stack_top_end_pos,
                                                      details::f32_convert_i32_s_op,
                                                      details::f32_convert_i32_s_op_2d,
                                                      details::f32_convert_i32_s_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_convert_i32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_convert_i32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_convert_i32_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
            using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_i32,
                                                      wasm_f32,
                                                      CompileOption.i32_stack_top_begin_pos,
                                                      CompileOption.i32_stack_top_end_pos,
                                                      CompileOption.f32_stack_top_begin_pos,
                                                      CompileOption.f32_stack_top_end_pos,
                                                      details::f32_convert_i32_u_op,
                                                      details::f32_convert_i32_u_op_2d,
                                                      details::f32_convert_i32_u_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_convert_i32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_convert_i32_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_convert_i64_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
            using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_i64,
                                                      wasm_f32,
                                                      CompileOption.i64_stack_top_begin_pos,
                                                      CompileOption.i64_stack_top_end_pos,
                                                      CompileOption.f32_stack_top_begin_pos,
                                                      CompileOption.f32_stack_top_end_pos,
                                                      details::f32_convert_i64_s_op,
                                                      details::f32_convert_i64_s_op_2d,
                                                      details::f32_convert_i64_s_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_convert_i64_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_convert_i64_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_convert_i64_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
            using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_i64,
                                                      wasm_f32,
                                                      CompileOption.i64_stack_top_begin_pos,
                                                      CompileOption.i64_stack_top_end_pos,
                                                      CompileOption.f32_stack_top_begin_pos,
                                                      CompileOption.f32_stack_top_end_pos,
                                                      details::f32_convert_i64_u_op,
                                                      details::f32_convert_i64_u_op_2d,
                                                      details::f32_convert_i64_u_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_convert_i64_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_convert_i64_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_demote_f64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
            using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_f64,
                                                      wasm_f32,
                                                      CompileOption.f64_stack_top_begin_pos,
                                                      CompileOption.f64_stack_top_end_pos,
                                                      CompileOption.f32_stack_top_begin_pos,
                                                      CompileOption.f32_stack_top_end_pos,
                                                      details::f32_demote_f64_op,
                                                      details::f32_demote_f64_op_2d,
                                                      details::f32_demote_f64_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_demote_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_demote_f64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_convert_i32_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
            using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_i32,
                                                      wasm_f64,
                                                      CompileOption.i32_stack_top_begin_pos,
                                                      CompileOption.i32_stack_top_end_pos,
                                                      CompileOption.f64_stack_top_begin_pos,
                                                      CompileOption.f64_stack_top_end_pos,
                                                      details::f64_convert_i32_s_op,
                                                      details::f64_convert_i32_s_op_2d,
                                                      details::f64_convert_i32_s_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_convert_i32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_convert_i32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_convert_i32_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
            using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_i32,
                                                      wasm_f64,
                                                      CompileOption.i32_stack_top_begin_pos,
                                                      CompileOption.i32_stack_top_end_pos,
                                                      CompileOption.f64_stack_top_begin_pos,
                                                      CompileOption.f64_stack_top_end_pos,
                                                      details::f64_convert_i32_u_op,
                                                      details::f64_convert_i32_u_op_2d,
                                                      details::f64_convert_i32_u_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_convert_i32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_convert_i32_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_convert_i64_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
            using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_i64,
                                                      wasm_f64,
                                                      CompileOption.i64_stack_top_begin_pos,
                                                      CompileOption.i64_stack_top_end_pos,
                                                      CompileOption.f64_stack_top_begin_pos,
                                                      CompileOption.f64_stack_top_end_pos,
                                                      details::f64_convert_i64_s_op,
                                                      details::f64_convert_i64_s_op_2d,
                                                      details::f64_convert_i64_s_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_convert_i64_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_convert_i64_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_convert_i64_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
            using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_i64,
                                                      wasm_f64,
                                                      CompileOption.i64_stack_top_begin_pos,
                                                      CompileOption.i64_stack_top_end_pos,
                                                      CompileOption.f64_stack_top_begin_pos,
                                                      CompileOption.f64_stack_top_end_pos,
                                                      details::f64_convert_i64_u_op,
                                                      details::f64_convert_i64_u_op_2d,
                                                      details::f64_convert_i64_u_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_convert_i64_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_convert_i64_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_promote_f32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
            using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_f32,
                                                      wasm_f64,
                                                      CompileOption.f32_stack_top_begin_pos,
                                                      CompileOption.f32_stack_top_end_pos,
                                                      CompileOption.f64_stack_top_begin_pos,
                                                      CompileOption.f64_stack_top_end_pos,
                                                      details::f64_promote_f32_op,
                                                      details::f64_promote_f32_op_2d,
                                                      details::f64_promote_f32_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_promote_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_promote_f32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i32_reinterpret_f32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_f32,
                                                      wasm_i32,
                                                      CompileOption.f32_stack_top_begin_pos,
                                                      CompileOption.f32_stack_top_end_pos,
                                                      CompileOption.i32_stack_top_begin_pos,
                                                      CompileOption.i32_stack_top_end_pos,
                                                      details::i32_reinterpret_f32_op,
                                                      details::i32_reinterpret_f32_op_2d,
                                                      details::i32_reinterpret_f32_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_reinterpret_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_reinterpret_f32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_i64_reinterpret_f64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
            using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_f64,
                                                      wasm_i64,
                                                      CompileOption.f64_stack_top_begin_pos,
                                                      CompileOption.f64_stack_top_end_pos,
                                                      CompileOption.i64_stack_top_begin_pos,
                                                      CompileOption.i64_stack_top_end_pos,
                                                      details::i64_reinterpret_f64_op,
                                                      details::i64_reinterpret_f64_op_2d,
                                                      details::i64_reinterpret_f64_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_reinterpret_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_reinterpret_f64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f32_reinterpret_i32_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
            using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_i32,
                                                      wasm_f32,
                                                      CompileOption.i32_stack_top_begin_pos,
                                                      CompileOption.i32_stack_top_end_pos,
                                                      CompileOption.f32_stack_top_begin_pos,
                                                      CompileOption.f32_stack_top_end_pos,
                                                      details::f32_reinterpret_i32_op,
                                                      details::f32_reinterpret_i32_op_2d,
                                                      details::f32_reinterpret_i32_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_reinterpret_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_reinterpret_i32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...>
            get_uwvmint_f64_reinterpret_i64_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
            using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
            return details::select_unary_convert_fptr<CompileOption,
                                                      wasm_i64,
                                                      wasm_f64,
                                                      CompileOption.i64_stack_top_begin_pos,
                                                      CompileOption.i64_stack_top_end_pos,
                                                      CompileOption.f64_stack_top_begin_pos,
                                                      CompileOption.f64_stack_top_end_pos,
                                                      details::f64_reinterpret_i64_op,
                                                      details::f64_reinterpret_i64_op_2d,
                                                      details::f64_reinterpret_i64_op_out_only,
                                                      Type...>(curr_stacktop);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_reinterpret_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_reinterpret_i64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_wrap_i64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_wrap_i64<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_wrap_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_wrap_i64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_trunc_f32_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_trunc_f32_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_trunc_f32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_trunc_f32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_trunc_f32_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_trunc_f32_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_trunc_f32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_trunc_f32_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_trunc_f64_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_trunc_f64_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_trunc_f64_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_trunc_f64_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_trunc_f64_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_trunc_f64_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_trunc_f64_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_trunc_f64_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_extend_i32_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_extend_i32_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_extend_i32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_extend_i32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_extend_i32_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_extend_i32_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_extend_i32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                           ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_extend_i32_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_trunc_f32_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_trunc_f32_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_trunc_f32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_trunc_f32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_trunc_f32_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_trunc_f32_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_trunc_f32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_trunc_f32_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_trunc_f64_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_trunc_f64_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_trunc_f64_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_trunc_f64_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_trunc_f64_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_trunc_f64_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_trunc_f64_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_trunc_f64_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_convert_i32_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_convert_i32_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_convert_i32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_convert_i32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_convert_i32_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_convert_i32_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_convert_i32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_convert_i32_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_convert_i64_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_convert_i64_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_convert_i64_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_convert_i64_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_convert_i64_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_convert_i64_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_convert_i64_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_convert_i64_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_demote_f64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_demote_f64<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_demote_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                         ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_demote_f64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_convert_i32_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_convert_i32_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_convert_i32_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_convert_i32_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_convert_i32_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_convert_i32_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_convert_i32_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_convert_i32_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_convert_i64_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_convert_i64_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_convert_i64_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_convert_i64_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_convert_i64_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_convert_i64_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_convert_i64_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                            ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_convert_i64_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_promote_f32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_promote_f32<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_promote_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                          ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_promote_f32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_reinterpret_f32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_reinterpret_f32<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_reinterpret_f32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_reinterpret_f32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_reinterpret_f64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_reinterpret_f64<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_reinterpret_f64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_reinterpret_f64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_reinterpret_i32_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_reinterpret_i32<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_reinterpret_i32_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_reinterpret_i32_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_reinterpret_i64_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_reinterpret_i64<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_reinterpret_i64_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                              ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_reinterpret_i64_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
