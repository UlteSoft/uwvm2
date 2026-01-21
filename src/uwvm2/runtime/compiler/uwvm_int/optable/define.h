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
# include <bit>
# include <cstring>
# include <limits>
# include <memory>
# include <concepts>
# if defined(__aarch64__) || defined(__AARCH64__)
#  include <arm_neon.h>
# endif
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/impl.h>
# include <uwvm2/object/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    using wasm1_code = ::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic;
    using wasm1_code_version_type = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version;

    struct uwvm_interpreter_function_operands_t
    {
        ::uwvm2::utils::container::vector<::std::byte> operands{};
    };

    struct local_func_storage_t
    {
        ::std::size_t local_count{};
        ::std::size_t operand_stack_max{};

        uwvm_interpreter_function_operands_t op{};
    };

    struct uwvm_interpreter_full_function_symbol_t
    {
        ::std::size_t local_count{};
        ::std::size_t operand_stack_max{};

        ::uwvm2::utils::container::vector<local_func_storage_t const*> imported_func_operands_ptrs{};
        ::uwvm2::utils::container::vector<local_func_storage_t> local_funcs{};
    };

    union wasm_stack_top_i32_with_f32_u
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 i32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32 f32;
    };

    union wasm_stack_top_i32_with_i64_u
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 i32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 i64;
    };

    union wasm_stack_top_i32_i64_f32_f64_u
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 i32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 i64;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32 f32;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 f64;
    };

    namespace details
    {
        
        UWVM_ALWAYS_INLINE inline constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32
            get_f32_low_from_f64_slot(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 v) noexcept
        {
#if defined(__aarch64__) || defined(__AARCH64__)
            float64x1_t d = vdup_n_f64(v);
            float32x2_t s2 = vreinterpret_f32_f64(d);
            return vget_lane_f32(s2, 0);
#else
            auto const u64 = ::std::bit_cast<::std::uint_least64_t>(v);
            auto const u32 = static_cast<::std::uint_least32_t>(u64);
            return ::std::bit_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>(u32);
#endif
        }

        UWVM_ALWAYS_INLINE inline constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32
            get_f32_low_from_v128_slot(::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128 v) noexcept
        {
#if defined(__aarch64__) || defined(__AARCH64__)
            auto const u8 = ::std::bit_cast<uint8x16_t>(v);
            auto const s4 = vreinterpretq_f32_u8(u8);
            return vgetq_lane_f32(s4, 0);
#else
            auto const u32x4 = ::std::bit_cast<::uwvm2::utils::container::array<::std::uint_least32_t, 4uz>>(v);
            return ::std::bit_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>(u32x4[0]);
#endif
        }

        UWVM_ALWAYS_INLINE inline constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64
            get_f64_low_from_v128_slot(::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128 v) noexcept
        {
#if defined(__aarch64__) || defined(__AARCH64__)
            auto const u8 = ::std::bit_cast<uint8x16_t>(v);
            auto const d2 = vreinterpretq_f64_u8(u8);
            return vgetq_lane_f64(d2, 0);
#else
            auto const u64x2 = ::std::bit_cast<::uwvm2::utils::container::array<::std::uint_least64_t, 2uz>>(v);
            return ::std::bit_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>(u64x2[0]);
#endif
        }
    }  // namespace details

    struct uwvm_interpreter_translate_option_t
    {
        /// @brief    tail_call
        /// @details  The `is_tail_call` parameter provides a feature that allows a function to generate a tail call rather than returning directly to the
        ///           higher‑level loop interpreter.
        ///
        ///           tail_call(::std::byte const* opcurr)
        ///           {
        ///               auto const opcurr_tail{opcurr};
        ///               opcurr += sizeof(void*);
        ///               ptr_t next_func;
        ///               ::std::memcpy(::std::addressof(next_func), opcurr_tail, sizeof(void*));
        ///               if constexpr(is_tail_call) [[gnu::musttail]] return next_func(opcurr);
        ///               // else == return
        ///           }
        bool is_tail_call{};

        /// @details  `local_stack_ptr_pos` and `operand_stack_ptr_pos` can be merged; set one of them to `SIZE_MAX`, and the other will be used as the base
        ///           address. If both are set to `SIZE_MAX`, a compile‑time error will occur.

        /// @brief   Because the target platform bitness and ABI may differ, the “stack-top optimization” must explicitly distinguish and track the cached
        ///          top-of-stack ranges for each value type.
        /// @details The begin/end fields describe the register/slot range occupied by a given type in top-of-stack caching. The idea is to map the top few
        ///          operand-stack values into registers (or register pairs) to reduce memory stack loads/stores.
        ///
        ///          1) On 32-bit targets, 64-bit values (i64/f64) typically cannot be cached in a single general-purpose register the way 32-bit values can
        ///             (they may require a register pair or must fall back to memory), so they must be tracked separately from i32/f32.
        ///
        ///          2) Whether floating-point values use a separate FP register file depends on the ABI. Some ABIs (typical hard-float) place f32/f64 in
        ///             dedicated FP registers, while others (e.g. MS ABI or some soft-float/no-hardfpu cases) may co-locate float values with integers in
        ///             GPRs or otherwise share the same caching slot layout.
        ///
        ///          3) If the f32/f64 top begin/end ranges fully coincide with the corresponding i32/i64 ranges, they are sharing the same registers/slots.
        ///             In that case a union can be used for same-location reinterpretation to reuse the stack-top optimization. If the ranges do not overlap,
        ///             floats and integers are allocated in different register files/slots and must be handled separately.
        ::std::size_t i32_stack_top_begin_pos{SIZE_MAX};
        ::std::size_t i32_stack_top_end_pos{SIZE_MAX};

        ::std::size_t i64_stack_top_begin_pos{SIZE_MAX};
        ::std::size_t i64_stack_top_end_pos{SIZE_MAX};

        ::std::size_t f32_stack_top_begin_pos{SIZE_MAX};
        ::std::size_t f32_stack_top_end_pos{SIZE_MAX};

        ::std::size_t f64_stack_top_begin_pos{SIZE_MAX};
        ::std::size_t f64_stack_top_end_pos{SIZE_MAX};

        /// @brief   v128 stack-top caching range (SIMD).
        /// @details v128 is a 128-bit SIMD value. On many mainstream ABIs/ISAs, SIMD registers share the same physical register file as floating-point
        ///          registers (e.g. x86 SSE XMM, AArch64/ARM NEON V registers). In such cases, v128 can be co-located with f32/f64 in the same top-of-stack
        ///          cache slots/registers, and the overlap can be detected the same way as described above:
        ///          if `v128_stack_top_begin_pos/end_pos` fully coincide with `f32_stack_top_*` / `f64_stack_top_*`, they are sharing the same slots.
        ///
        ///          If the ranges do not overlap, it indicates that v128 is allocated in a distinct register class or uses a different caching layout, and
        ///          must be handled separately.
        ::std::size_t v128_stack_top_begin_pos{SIZE_MAX};
        ::std::size_t v128_stack_top_end_pos{SIZE_MAX};
    };

    struct uwvm_interpreter_stacktop_currpos_t
    {
        ::std::size_t i32_stack_top_curr_pos{SIZE_MAX};
        ::std::size_t i64_stack_top_curr_pos{SIZE_MAX};
        ::std::size_t f32_stack_top_curr_pos{SIZE_MAX};
        ::std::size_t f64_stack_top_curr_pos{SIZE_MAX};
        ::std::size_t v128_stack_top_curr_pos{SIZE_MAX};
    };

    struct uwvm_interpreter_stacktop_remain_size_t
    {
        ::std::size_t i32_stack_top_remain_size{};
        ::std::size_t i64_stack_top_remain_size{};
        ::std::size_t f32_stack_top_remain_size{};
        ::std::size_t f64_stack_top_remain_size{};
        ::std::size_t v128_stack_top_remain_size{};
    };

    template <typename Type>
    concept uwvm_int_stack_top_type =
        ::std::same_as<Type, ::std::byte const*> || ::std::same_as<Type, ::std::byte*> ||
        ::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32> ||
        ::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64> ||
        ::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32> ||
        ::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64> ||
        ::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128> || ::std::same_as<Type, wasm_stack_top_i32_with_f32_u> ||
        ::std::same_as<Type, wasm_stack_top_i32_with_i64_u> || ::std::same_as<Type, wasm_stack_top_i32_i64_f32_f64_u>;

    using unreachable_func_t = void (*)() noexcept;

    struct compile_option
    {
        unreachable_func_t unreachable_func_p{};
    };

    template <uwvm_int_stack_top_type... Type>
    using uwvm_interpreter_opfunc_t = UWVM_INTERPRETER_OPFUNC_MACRO void (*)(Type... type) UWVM_THROWS;

    template <uwvm_interpreter_translate_option_t CompileOption,
              uwvm_int_stack_top_type GetType,
              ::std::size_t curr_stack_top,
              uwvm_int_stack_top_type... TypeRef>
    UWVM_ALWAYS_INLINE inline constexpr GetType get_curr_val_from_operand_stack_top(TypeRef & ... typeref) noexcept
    {
        if constexpr(::std::same_as<GetType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>)
        {
            constexpr bool has_stack_top{CompileOption.i32_stack_top_begin_pos != CompileOption.i32_stack_top_end_pos};

            if constexpr(has_stack_top)
            {
                static_assert(CompileOption.i32_stack_top_begin_pos <= curr_stack_top && curr_stack_top < CompileOption.i32_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.i32_stack_top_end_pos);

                constexpr bool is_i32_i64_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                                CompileOption.i32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos};
                constexpr bool is_i32_f32_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                                CompileOption.i32_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool is_i32_f64_merge{CompileOption.i32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                                CompileOption.i32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                constexpr bool is_i32_f32_i64_f64_merge{is_i32_i64_merge && is_i32_f32_merge && is_i32_f64_merge};

                using Type = ::std::remove_cvref_t<TypeRef...[curr_stack_top]>;

                if constexpr(is_i32_f32_i64_f64_merge)
                {
                    static_assert(::std::same_as<Type, wasm_stack_top_i32_i64_f32_f64_u>);
                    return typeref...[curr_stack_top].i32;
                }
                else if constexpr(is_i32_i64_merge)
                {
                    static_assert(::std::same_as<Type, wasm_stack_top_i32_with_i64_u>);
                    return typeref...[curr_stack_top].i32;
                }
                else if constexpr(is_i32_f32_merge)
                {
                    static_assert(::std::same_as<Type, wasm_stack_top_i32_with_f32_u>);
                    return typeref...[curr_stack_top].i32;
                }
                else
                {
                    static_assert(!is_i32_f64_merge);
                    static_assert(::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>);
                    return typeref...[curr_stack_top];
                }
            }
            else
            {
                // args: [optable ptr, operand stack top, local base]

                static_assert(sizeof...(TypeRef) >= 2uz);
                using Type = ::std::remove_cvref_t<TypeRef...[1u]>;
                static_assert(::std::same_as<Type, ::std::byte*>);

                // last_val top_val (end)
                // safe
                //                  ^^ typeref...[1u]

                typeref...[1u] -= sizeof(GetType);

                // last_val top_val (end)
                // safe
                //          ^^ typeref...[1u]

                GetType ret;  // no init
                ::std::memcpy(::std::addressof(ret), typeref...[1u], sizeof(GetType));

                return ret;
            }
        }
        else if constexpr(::std::same_as<GetType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>)
        {
            constexpr bool has_stack_top{CompileOption.i64_stack_top_begin_pos != CompileOption.i64_stack_top_end_pos};

            if constexpr(has_stack_top)
            {
                static_assert(CompileOption.i64_stack_top_begin_pos <= curr_stack_top && curr_stack_top < CompileOption.i64_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.i64_stack_top_end_pos);

                constexpr bool is_i64_i32_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.i32_stack_top_begin_pos &&
                                                CompileOption.i64_stack_top_end_pos == CompileOption.i32_stack_top_end_pos};
                constexpr bool is_i64_f32_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                                CompileOption.i64_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool is_i64_f64_merge{CompileOption.i64_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                                CompileOption.i64_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                constexpr bool is_i32_f32_i64_f64_merge{is_i64_i32_merge && is_i64_f32_merge && is_i64_f64_merge};

                using Type = ::std::remove_cvref_t<TypeRef...[curr_stack_top]>;

                if constexpr(is_i32_f32_i64_f64_merge)
                {
                    static_assert(::std::same_as<Type, wasm_stack_top_i32_i64_f32_f64_u>);
                    return typeref...[curr_stack_top].i64;
                }
                else if constexpr(is_i64_i32_merge)
                {
                    static_assert(::std::same_as<Type, wasm_stack_top_i32_with_i64_u>);
                    return typeref...[curr_stack_top].i64;
                }
                else
                {
                    static_assert(!(is_i64_f32_merge || is_i64_f64_merge));
                    static_assert(::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>);
                    return typeref...[curr_stack_top];
                }
            }
            else
            {
                static_assert(sizeof...(TypeRef) >= 2uz);
                using Type = ::std::remove_cvref_t<TypeRef...[1u]>;
                static_assert(::std::same_as<Type, ::std::byte*>);

                typeref...[1u] -= sizeof(GetType);

                GetType ret;  // no init
                ::std::memcpy(::std::addressof(ret), typeref...[1u], sizeof(GetType));

                return ret;
            }
        }
        else if constexpr(::std::same_as<GetType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>)
        {
            constexpr bool has_stack_top{CompileOption.f32_stack_top_begin_pos != CompileOption.f32_stack_top_end_pos};

            if constexpr(has_stack_top)
            {
                static_assert(CompileOption.f32_stack_top_begin_pos <= curr_stack_top && curr_stack_top < CompileOption.f32_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.f32_stack_top_end_pos);

                constexpr bool is_f32_i32_merge{CompileOption.f32_stack_top_begin_pos == CompileOption.i32_stack_top_begin_pos &&
                                                CompileOption.f32_stack_top_end_pos == CompileOption.i32_stack_top_end_pos};
                constexpr bool is_f32_i64_merge{CompileOption.f32_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                                CompileOption.f32_stack_top_end_pos == CompileOption.i64_stack_top_end_pos};
                constexpr bool is_f32_f64_merge{CompileOption.f32_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                                CompileOption.f32_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};
                constexpr bool is_f32_v128_merge{CompileOption.f32_stack_top_begin_pos == CompileOption.v128_stack_top_begin_pos &&
                                                 CompileOption.f32_stack_top_end_pos == CompileOption.v128_stack_top_end_pos};
                constexpr bool is_i32_f32_i64_f64_merge{is_f32_i32_merge && is_f32_i64_merge && is_f32_f64_merge};
                constexpr bool is_f32_f64_v128_merge{is_f32_f64_merge && is_f32_v128_merge};

                using Type = ::std::remove_cvref_t<TypeRef...[curr_stack_top]>;

                if constexpr(is_f32_f64_v128_merge)
                {
                    static_assert(::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>);
                    return details::get_f32_low_from_v128_slot(typeref...[curr_stack_top]);
                }
                else if constexpr(is_i32_f32_i64_f64_merge)
                {
                    static_assert(::std::same_as<Type, wasm_stack_top_i32_i64_f32_f64_u>);
                    return typeref...[curr_stack_top].f32;
                }
                else if constexpr(is_f32_f64_merge)
                {
                    static_assert(::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>);
                    return details::get_f32_low_from_f64_slot(typeref...[curr_stack_top]);
                }
                else if constexpr(is_f32_i32_merge)
                {
                    static_assert(::std::same_as<Type, wasm_stack_top_i32_with_f32_u>);
                    return typeref...[curr_stack_top].f32;
                }
                else
                {
                    static_assert(!is_f32_i64_merge);
                    static_assert(::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>);
                    return typeref...[curr_stack_top];
                }
            }
            else
            {
                static_assert(sizeof...(TypeRef) >= 2uz);
                using Type = ::std::remove_cvref_t<TypeRef...[1u]>;
                static_assert(::std::same_as<Type, ::std::byte*>);

                typeref...[1u] -= sizeof(GetType);

                GetType ret;  // no init
                ::std::memcpy(::std::addressof(ret), typeref...[1u], sizeof(GetType));

                return ret;
            }
        }
        else if constexpr(::std::same_as<GetType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>)
        {
            constexpr bool has_stack_top{CompileOption.f64_stack_top_begin_pos != CompileOption.f64_stack_top_end_pos};

            if constexpr(has_stack_top)
            {
                static_assert(CompileOption.f64_stack_top_begin_pos <= curr_stack_top && curr_stack_top < CompileOption.f64_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.f64_stack_top_end_pos);

                constexpr bool is_f64_i32_merge{CompileOption.f64_stack_top_begin_pos == CompileOption.i32_stack_top_begin_pos &&
                                                CompileOption.f64_stack_top_end_pos == CompileOption.i32_stack_top_end_pos};
                constexpr bool is_f64_i64_merge{CompileOption.f64_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                                CompileOption.f64_stack_top_end_pos == CompileOption.i64_stack_top_end_pos};
                constexpr bool is_f64_f32_merge{CompileOption.f64_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                                CompileOption.f64_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool is_f64_v128_merge{CompileOption.f64_stack_top_begin_pos == CompileOption.v128_stack_top_begin_pos &&
                                                 CompileOption.f64_stack_top_end_pos == CompileOption.v128_stack_top_end_pos};
                constexpr bool is_i32_f32_i64_f64_merge{is_f64_i32_merge && is_f64_i64_merge && is_f64_f32_merge};
                constexpr bool is_f32_f64_v128_merge{is_f64_f32_merge && is_f64_v128_merge};

                using Type = ::std::remove_cvref_t<TypeRef...[curr_stack_top]>;

                if constexpr(is_f32_f64_v128_merge)
                {
                    static_assert(::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>);
                    return details::get_f64_low_from_v128_slot(typeref...[curr_stack_top]);
                }
                else if constexpr(is_i32_f32_i64_f64_merge)
                {
                    static_assert(::std::same_as<Type, wasm_stack_top_i32_i64_f32_f64_u>);
                    return typeref...[curr_stack_top].f64;
                }
                else if constexpr(is_f64_f32_merge)
                {
                    static_assert(::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>);
                    return typeref...[curr_stack_top];
                }
                else
                {
                    static_assert(!(is_f64_i32_merge || is_f64_i64_merge));
                    static_assert(::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>);
                    return typeref...[curr_stack_top];
                }
            }
            else
            {
                static_assert(sizeof...(TypeRef) >= 2uz);
                using Type = ::std::remove_cvref_t<TypeRef...[1u]>;
                static_assert(::std::same_as<Type, ::std::byte*>);

                typeref...[1u] -= sizeof(GetType);

                GetType ret;  // no init
                ::std::memcpy(::std::addressof(ret), typeref...[1u], sizeof(GetType));

                return ret;
            }
        }
        else if constexpr(::std::same_as<GetType, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>)
        {
            constexpr bool has_stack_top{CompileOption.v128_stack_top_begin_pos != CompileOption.v128_stack_top_end_pos};

            if constexpr(has_stack_top)
            {
                static_assert(CompileOption.v128_stack_top_begin_pos <= curr_stack_top && curr_stack_top < CompileOption.v128_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.v128_stack_top_end_pos);

                constexpr bool is_v128_i32_merge{CompileOption.v128_stack_top_begin_pos == CompileOption.i32_stack_top_begin_pos &&
                                                 CompileOption.v128_stack_top_end_pos == CompileOption.i32_stack_top_end_pos};
                constexpr bool is_v128_i64_merge{CompileOption.v128_stack_top_begin_pos == CompileOption.i64_stack_top_begin_pos &&
                                                 CompileOption.v128_stack_top_end_pos == CompileOption.i64_stack_top_end_pos};
                constexpr bool is_v128_f32_merge{CompileOption.v128_stack_top_begin_pos == CompileOption.f32_stack_top_begin_pos &&
                                                 CompileOption.v128_stack_top_end_pos == CompileOption.f32_stack_top_end_pos};
                constexpr bool is_v128_f64_merge{CompileOption.v128_stack_top_begin_pos == CompileOption.f64_stack_top_begin_pos &&
                                                 CompileOption.v128_stack_top_end_pos == CompileOption.f64_stack_top_end_pos};

                using Type = ::std::remove_cvref_t<TypeRef...[curr_stack_top]>;

                static_assert(!(is_v128_i32_merge || is_v128_i64_merge));

                if constexpr(is_v128_f32_merge || is_v128_f64_merge)
                {
                    static_assert(is_v128_f32_merge && is_v128_f64_merge);
                    static_assert(::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>);
                    return typeref...[curr_stack_top];
                }
                else
                {
                    static_assert(::std::same_as<Type, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>);
                    return typeref...[curr_stack_top];
                }
            }
            else
            {
                static_assert(sizeof...(TypeRef) >= 2uz);
                using Type = ::std::remove_cvref_t<TypeRef...[1u]>;
                static_assert(::std::same_as<Type, ::std::byte*>);

                typeref...[1u] -= sizeof(GetType);

                GetType ret;  // no init
                ::std::memcpy(::std::addressof(ret), typeref...[1u], sizeof(GetType));

                return ret;
            }
        }
        else
        {
            static_assert(::std::same_as<GetType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>);
        }
    }

    template <uwvm_int_stack_top_type GetType, uwvm_int_stack_top_type... TypeRef>
    UWVM_ALWAYS_INLINE inline constexpr GetType get_curr_val_from_operand_stack_cache(TypeRef & ... typeref) noexcept
    {
        // args: [optable ptr, operand stack top, local base]

        static_assert(sizeof...(TypeRef) >= 2uz);
        using Type = ::std::remove_cvref_t<TypeRef...[1u]>;
        static_assert(::std::same_as<Type, ::std::byte*>);

        // last_val top_val (end)
        // safe
        //                  ^^ typeref...[1u]

        typeref...[1u] -= sizeof(GetType);

        // last_val top_val (end)
        // safe
        //          ^^ typeref...[1u]

        GetType ret;  // no init
        ::std::memcpy(::std::addressof(ret), typeref...[1u], sizeof(GetType));

        return ret;
    }

    namespace details
    {
        inline consteval bool uwvm_interpreter_stacktop_range_enabled(::std::size_t begin_pos, ::std::size_t end_pos) noexcept { return begin_pos != end_pos; }

        inline consteval bool uwvm_interpreter_stacktop_range_is_same(::std::size_t a_begin_pos,
                                                                      ::std::size_t a_end_pos,
                                                                      ::std::size_t b_begin_pos,
                                                                      ::std::size_t b_end_pos) noexcept
        { return a_begin_pos == b_begin_pos && a_end_pos == b_end_pos; }

        inline consteval bool uwvm_interpreter_stacktop_range_is_disjoint(::std::size_t a_begin_pos,
                                                                          ::std::size_t a_end_pos,
                                                                          ::std::size_t b_begin_pos,
                                                                          ::std::size_t b_end_pos) noexcept
        { return a_end_pos <= b_begin_pos || b_end_pos <= a_begin_pos; }

        inline consteval ::std::size_t uwvm_interpreter_stacktop_range_size(::std::size_t begin_pos, ::std::size_t end_pos) noexcept
        { return uwvm_interpreter_stacktop_range_enabled(begin_pos, end_pos) ? (end_pos - begin_pos) : 0uz; }

        inline consteval ::std::size_t uwvm_interpreter_stacktop_next_pos(::std::size_t curr_pos, ::std::size_t begin_pos, ::std::size_t end_pos) noexcept
        { return (curr_pos + 1uz == end_pos) ? begin_pos : (curr_pos + 1uz); }

        struct uwvm_interpreter_stacktop_state_t
        {
            uwvm_interpreter_stacktop_currpos_t currpos{};
            uwvm_interpreter_stacktop_remain_size_t remain{};
        };

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_interpreter_stacktop_currpos_t CurrStackTop, uwvm_int_stack_top_type... TypeRef>
        inline consteval void check_uwvm_interpreter_stacktop_layout() noexcept
        {
            constexpr bool i32_enabled{uwvm_interpreter_stacktop_range_enabled(CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos)};
            constexpr bool i64_enabled{uwvm_interpreter_stacktop_range_enabled(CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos)};
            constexpr bool f32_enabled{uwvm_interpreter_stacktop_range_enabled(CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos)};
            constexpr bool f64_enabled{uwvm_interpreter_stacktop_range_enabled(CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos)};
            constexpr bool v128_enabled{uwvm_interpreter_stacktop_range_enabled(CompileOption.v128_stack_top_begin_pos, CompileOption.v128_stack_top_end_pos)};

            if constexpr(i32_enabled)
            {
                static_assert(CompileOption.i32_stack_top_begin_pos != SIZE_MAX && CompileOption.i32_stack_top_end_pos != SIZE_MAX);
                static_assert(CompileOption.i32_stack_top_begin_pos >= 3uz);
                static_assert(CompileOption.i32_stack_top_begin_pos < CompileOption.i32_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.i32_stack_top_end_pos);
                static_assert(CompileOption.i32_stack_top_begin_pos <= CurrStackTop.i32_stack_top_curr_pos &&
                              CurrStackTop.i32_stack_top_curr_pos < CompileOption.i32_stack_top_end_pos);
            }
            else
            {
                static_assert(CurrStackTop.i32_stack_top_curr_pos == SIZE_MAX);
            }

            if constexpr(i64_enabled)
            {
                static_assert(CompileOption.i64_stack_top_begin_pos != SIZE_MAX && CompileOption.i64_stack_top_end_pos != SIZE_MAX);
                static_assert(CompileOption.i64_stack_top_begin_pos >= 3uz);
                static_assert(CompileOption.i64_stack_top_begin_pos < CompileOption.i64_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.i64_stack_top_end_pos);
                static_assert(CompileOption.i64_stack_top_begin_pos <= CurrStackTop.i64_stack_top_curr_pos &&
                              CurrStackTop.i64_stack_top_curr_pos < CompileOption.i64_stack_top_end_pos);
            }
            else
            {
                static_assert(CurrStackTop.i64_stack_top_curr_pos == SIZE_MAX);
            }

            if constexpr(f32_enabled)
            {
                static_assert(CompileOption.f32_stack_top_begin_pos != SIZE_MAX && CompileOption.f32_stack_top_end_pos != SIZE_MAX);
                static_assert(CompileOption.f32_stack_top_begin_pos >= 3uz);
                static_assert(CompileOption.f32_stack_top_begin_pos < CompileOption.f32_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.f32_stack_top_end_pos);
                static_assert(CompileOption.f32_stack_top_begin_pos <= CurrStackTop.f32_stack_top_curr_pos &&
                              CurrStackTop.f32_stack_top_curr_pos < CompileOption.f32_stack_top_end_pos);
            }
            else
            {
                static_assert(CurrStackTop.f32_stack_top_curr_pos == SIZE_MAX);
            }

            if constexpr(f64_enabled)
            {
                static_assert(CompileOption.f64_stack_top_begin_pos != SIZE_MAX && CompileOption.f64_stack_top_end_pos != SIZE_MAX);
                static_assert(CompileOption.f64_stack_top_begin_pos >= 3uz);
                static_assert(CompileOption.f64_stack_top_begin_pos < CompileOption.f64_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.f64_stack_top_end_pos);
                static_assert(CompileOption.f64_stack_top_begin_pos <= CurrStackTop.f64_stack_top_curr_pos &&
                              CurrStackTop.f64_stack_top_curr_pos < CompileOption.f64_stack_top_end_pos);
            }
            else
            {
                static_assert(CurrStackTop.f64_stack_top_curr_pos == SIZE_MAX);
            }

            if constexpr(v128_enabled)
            {
                static_assert(CompileOption.v128_stack_top_begin_pos != SIZE_MAX && CompileOption.v128_stack_top_end_pos != SIZE_MAX);
                static_assert(CompileOption.v128_stack_top_begin_pos >= 3uz);
                static_assert(CompileOption.v128_stack_top_begin_pos < CompileOption.v128_stack_top_end_pos);
                static_assert(sizeof...(TypeRef) >= CompileOption.v128_stack_top_end_pos);
                static_assert(CompileOption.v128_stack_top_begin_pos <= CurrStackTop.v128_stack_top_curr_pos &&
                              CurrStackTop.v128_stack_top_curr_pos < CompileOption.v128_stack_top_end_pos);
            }
            else
            {
                static_assert(CurrStackTop.v128_stack_top_curr_pos == SIZE_MAX);
            }

            constexpr bool i32_i64_same{uwvm_interpreter_stacktop_range_is_same(CompileOption.i32_stack_top_begin_pos,
                                                                                CompileOption.i32_stack_top_end_pos,
                                                                                CompileOption.i64_stack_top_begin_pos,
                                                                                CompileOption.i64_stack_top_end_pos)};
            constexpr bool i32_f32_same{uwvm_interpreter_stacktop_range_is_same(CompileOption.i32_stack_top_begin_pos,
                                                                                CompileOption.i32_stack_top_end_pos,
                                                                                CompileOption.f32_stack_top_begin_pos,
                                                                                CompileOption.f32_stack_top_end_pos)};
            constexpr bool i32_f64_same{uwvm_interpreter_stacktop_range_is_same(CompileOption.i32_stack_top_begin_pos,
                                                                                CompileOption.i32_stack_top_end_pos,
                                                                                CompileOption.f64_stack_top_begin_pos,
                                                                                CompileOption.f64_stack_top_end_pos)};
            constexpr bool f32_f64_same{uwvm_interpreter_stacktop_range_is_same(CompileOption.f32_stack_top_begin_pos,
                                                                                CompileOption.f32_stack_top_end_pos,
                                                                                CompileOption.f64_stack_top_begin_pos,
                                                                                CompileOption.f64_stack_top_end_pos)};
            constexpr bool f32_v128_same{uwvm_interpreter_stacktop_range_is_same(CompileOption.f32_stack_top_begin_pos,
                                                                                 CompileOption.f32_stack_top_end_pos,
                                                                                 CompileOption.v128_stack_top_begin_pos,
                                                                                 CompileOption.v128_stack_top_end_pos)};
            constexpr bool f64_v128_same{uwvm_interpreter_stacktop_range_is_same(CompileOption.f64_stack_top_begin_pos,
                                                                                 CompileOption.f64_stack_top_end_pos,
                                                                                 CompileOption.v128_stack_top_begin_pos,
                                                                                 CompileOption.v128_stack_top_end_pos)};

            constexpr bool i32_i64_merge{(i32_enabled && i64_enabled) && i32_i64_same};
            constexpr bool i32_f32_merge{(i32_enabled && f32_enabled) && i32_f32_same};
            constexpr bool i32_f64_merge{(i32_enabled && f64_enabled) && i32_f64_same};
            constexpr bool f32_f64_merge{(f32_enabled && f64_enabled) && f32_f64_same};
            constexpr bool f32_v128_merge{(f32_enabled && v128_enabled) && f32_v128_same};
            constexpr bool f64_v128_merge{(f64_enabled && v128_enabled) && f64_v128_same};

            constexpr bool i32_i64_f32_f64_merge{(i32_enabled && i64_enabled && f32_enabled && f64_enabled) && i32_i64_merge && i32_f32_merge && i32_f64_merge};
            static_assert(!i32_f64_merge || i32_i64_f32_f64_merge);
            static_assert(!((i64_enabled && f32_enabled) && uwvm_interpreter_stacktop_range_is_same(CompileOption.i64_stack_top_begin_pos,
                                                                                                    CompileOption.i64_stack_top_end_pos,
                                                                                                    CompileOption.f32_stack_top_begin_pos,
                                                                                                    CompileOption.f32_stack_top_end_pos)) ||
                          i32_i64_f32_f64_merge);
            static_assert(!((i64_enabled && f64_enabled) && uwvm_interpreter_stacktop_range_is_same(CompileOption.i64_stack_top_begin_pos,
                                                                                                    CompileOption.i64_stack_top_end_pos,
                                                                                                    CompileOption.f64_stack_top_begin_pos,
                                                                                                    CompileOption.f64_stack_top_end_pos)) ||
                          i32_i64_f32_f64_merge);

            static_assert(!(v128_enabled && i32_enabled &&
                            uwvm_interpreter_stacktop_range_is_same(CompileOption.v128_stack_top_begin_pos,
                                                                    CompileOption.v128_stack_top_end_pos,
                                                                    CompileOption.i32_stack_top_begin_pos,
                                                                    CompileOption.i32_stack_top_end_pos)));
            static_assert(!(v128_enabled && i64_enabled &&
                            uwvm_interpreter_stacktop_range_is_same(CompileOption.v128_stack_top_begin_pos,
                                                                    CompileOption.v128_stack_top_end_pos,
                                                                    CompileOption.i64_stack_top_begin_pos,
                                                                    CompileOption.i64_stack_top_end_pos)));

            static_assert(!f32_v128_merge || (f32_f64_merge && f64_v128_merge));
            static_assert(!f64_v128_merge || (f32_f64_merge && f32_v128_merge));

            if constexpr(i32_i64_merge) { static_assert(CurrStackTop.i32_stack_top_curr_pos == CurrStackTop.i64_stack_top_curr_pos); }
            if constexpr(i32_f32_merge) { static_assert(CurrStackTop.i32_stack_top_curr_pos == CurrStackTop.f32_stack_top_curr_pos); }
            if constexpr(i32_i64_f32_f64_merge)
            {
                static_assert(CurrStackTop.i32_stack_top_curr_pos == CurrStackTop.i64_stack_top_curr_pos);
                static_assert(CurrStackTop.i32_stack_top_curr_pos == CurrStackTop.f32_stack_top_curr_pos);
                static_assert(CurrStackTop.i32_stack_top_curr_pos == CurrStackTop.f64_stack_top_curr_pos);
            }

            if constexpr(f32_f64_merge) { static_assert(CurrStackTop.f32_stack_top_curr_pos == CurrStackTop.f64_stack_top_curr_pos); }
            if constexpr(f32_v128_merge) { static_assert(CurrStackTop.f32_stack_top_curr_pos == CurrStackTop.v128_stack_top_curr_pos); }
            if constexpr(f64_v128_merge) { static_assert(CurrStackTop.f64_stack_top_curr_pos == CurrStackTop.v128_stack_top_curr_pos); }

            if constexpr(i32_enabled && i64_enabled && !i32_i64_same)
            {
                static_assert(uwvm_interpreter_stacktop_range_is_disjoint(CompileOption.i32_stack_top_begin_pos,
                                                                          CompileOption.i32_stack_top_end_pos,
                                                                          CompileOption.i64_stack_top_begin_pos,
                                                                          CompileOption.i64_stack_top_end_pos));
            }
            if constexpr(i32_enabled && f32_enabled && !i32_f32_same)
            {
                static_assert(uwvm_interpreter_stacktop_range_is_disjoint(CompileOption.i32_stack_top_begin_pos,
                                                                          CompileOption.i32_stack_top_end_pos,
                                                                          CompileOption.f32_stack_top_begin_pos,
                                                                          CompileOption.f32_stack_top_end_pos));
            }
            if constexpr(i32_enabled && f64_enabled && !i32_f64_same)
            {
                static_assert(uwvm_interpreter_stacktop_range_is_disjoint(CompileOption.i32_stack_top_begin_pos,
                                                                          CompileOption.i32_stack_top_end_pos,
                                                                          CompileOption.f64_stack_top_begin_pos,
                                                                          CompileOption.f64_stack_top_end_pos));
            }
            if constexpr(i32_enabled && v128_enabled &&
                         !uwvm_interpreter_stacktop_range_is_same(CompileOption.i32_stack_top_begin_pos,
                                                                  CompileOption.i32_stack_top_end_pos,
                                                                  CompileOption.v128_stack_top_begin_pos,
                                                                  CompileOption.v128_stack_top_end_pos))
            {
                static_assert(uwvm_interpreter_stacktop_range_is_disjoint(CompileOption.i32_stack_top_begin_pos,
                                                                          CompileOption.i32_stack_top_end_pos,
                                                                          CompileOption.v128_stack_top_begin_pos,
                                                                          CompileOption.v128_stack_top_end_pos));
            }

            if constexpr(i64_enabled && f32_enabled &&
                         !uwvm_interpreter_stacktop_range_is_same(CompileOption.i64_stack_top_begin_pos,
                                                                  CompileOption.i64_stack_top_end_pos,
                                                                  CompileOption.f32_stack_top_begin_pos,
                                                                  CompileOption.f32_stack_top_end_pos))
            {
                static_assert(uwvm_interpreter_stacktop_range_is_disjoint(CompileOption.i64_stack_top_begin_pos,
                                                                          CompileOption.i64_stack_top_end_pos,
                                                                          CompileOption.f32_stack_top_begin_pos,
                                                                          CompileOption.f32_stack_top_end_pos));
            }
            if constexpr(i64_enabled && f64_enabled &&
                         !uwvm_interpreter_stacktop_range_is_same(CompileOption.i64_stack_top_begin_pos,
                                                                  CompileOption.i64_stack_top_end_pos,
                                                                  CompileOption.f64_stack_top_begin_pos,
                                                                  CompileOption.f64_stack_top_end_pos))
            {
                static_assert(uwvm_interpreter_stacktop_range_is_disjoint(CompileOption.i64_stack_top_begin_pos,
                                                                          CompileOption.i64_stack_top_end_pos,
                                                                          CompileOption.f64_stack_top_begin_pos,
                                                                          CompileOption.f64_stack_top_end_pos));
            }
            if constexpr(i64_enabled && v128_enabled &&
                         !uwvm_interpreter_stacktop_range_is_same(CompileOption.i64_stack_top_begin_pos,
                                                                  CompileOption.i64_stack_top_end_pos,
                                                                  CompileOption.v128_stack_top_begin_pos,
                                                                  CompileOption.v128_stack_top_end_pos))
            {
                static_assert(uwvm_interpreter_stacktop_range_is_disjoint(CompileOption.i64_stack_top_begin_pos,
                                                                          CompileOption.i64_stack_top_end_pos,
                                                                          CompileOption.v128_stack_top_begin_pos,
                                                                          CompileOption.v128_stack_top_end_pos));
            }

            if constexpr(f32_enabled && f64_enabled && !f32_f64_same)
            {
                static_assert(uwvm_interpreter_stacktop_range_is_disjoint(CompileOption.f32_stack_top_begin_pos,
                                                                          CompileOption.f32_stack_top_end_pos,
                                                                          CompileOption.f64_stack_top_begin_pos,
                                                                          CompileOption.f64_stack_top_end_pos));
            }
            if constexpr(f32_enabled && v128_enabled && !f32_v128_same)
            {
                static_assert(uwvm_interpreter_stacktop_range_is_disjoint(CompileOption.f32_stack_top_begin_pos,
                                                                          CompileOption.f32_stack_top_end_pos,
                                                                          CompileOption.v128_stack_top_begin_pos,
                                                                          CompileOption.v128_stack_top_end_pos));
            }
            if constexpr(f64_enabled && v128_enabled && !f64_v128_same)
            {
                static_assert(uwvm_interpreter_stacktop_range_is_disjoint(CompileOption.f64_stack_top_begin_pos,
                                                                          CompileOption.f64_stack_top_end_pos,
                                                                          CompileOption.v128_stack_top_begin_pos,
                                                                          CompileOption.v128_stack_top_end_pos));
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_interpreter_stacktop_currpos_t CurrStackTop>
        inline consteval uwvm_interpreter_stacktop_state_t make_uwvm_interpreter_stacktop_initial_state() noexcept
        {
            return uwvm_interpreter_stacktop_state_t{
                .currpos = CurrStackTop,
                .remain = uwvm_interpreter_stacktop_remain_size_t{
                                                                  .i32_stack_top_remain_size =
                        uwvm_interpreter_stacktop_range_size(CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos),
                                                                  .i64_stack_top_remain_size =
                        uwvm_interpreter_stacktop_range_size(CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos),
                                                                  .f32_stack_top_remain_size =
                        uwvm_interpreter_stacktop_range_size(CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos),
                                                                  .f64_stack_top_remain_size =
                        uwvm_interpreter_stacktop_range_size(CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos),
                                                                  .v128_stack_top_remain_size =
                        uwvm_interpreter_stacktop_range_size(CompileOption.v128_stack_top_begin_pos, CompileOption.v128_stack_top_end_pos)}
            };
        }

        template <typename ValType>
        inline consteval bool is_uwvm_interpreter_valtype_supported() noexcept
        {
            return ::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32> ||
                   ::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64> ||
                   ::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32> ||
                   ::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64> ||
                   ::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>;
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_interpreter_stacktop_state_t State, typename ValType>
        inline consteval bool uwvm_interpreter_can_get_val_from_stacktop_cache() noexcept
        {
            if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>)
            {
                return uwvm_interpreter_stacktop_range_enabled(CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos) &&
                       State.remain.i32_stack_top_remain_size != 0uz;
            }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>)
            {
                return uwvm_interpreter_stacktop_range_enabled(CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos) &&
                       State.remain.i64_stack_top_remain_size != 0uz;
            }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>)
            {
                return uwvm_interpreter_stacktop_range_enabled(CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos) &&
                       State.remain.f32_stack_top_remain_size != 0uz;
            }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>)
            {
                return uwvm_interpreter_stacktop_range_enabled(CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos) &&
                       State.remain.f64_stack_top_remain_size != 0uz;
            }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>)
            {
                return uwvm_interpreter_stacktop_range_enabled(CompileOption.v128_stack_top_begin_pos, CompileOption.v128_stack_top_end_pos) &&
                       State.remain.v128_stack_top_remain_size != 0uz;
            }
            else
            {
                static_assert(is_uwvm_interpreter_valtype_supported<ValType>());
                return false;
            }
        }

        template <uwvm_interpreter_stacktop_state_t State, typename ValType>
        inline consteval ::std::size_t get_uwvm_interpreter_stacktop_currpos() noexcept
        {
            if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>) { return State.currpos.i32_stack_top_curr_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>) { return State.currpos.i64_stack_top_curr_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>) { return State.currpos.f32_stack_top_curr_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>) { return State.currpos.f64_stack_top_curr_pos; }
            else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>)
            {
                return State.currpos.v128_stack_top_curr_pos;
            }
            else
            {
                static_assert(is_uwvm_interpreter_valtype_supported<ValType>());
                return SIZE_MAX;
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_interpreter_stacktop_state_t State, typename ValType>
        inline consteval uwvm_interpreter_stacktop_state_t pop_uwvm_interpreter_stacktop_state() noexcept
        {
            constexpr bool can_use_stacktop{uwvm_interpreter_can_get_val_from_stacktop_cache<CompileOption, State, ValType>()};

            if constexpr(!can_use_stacktop) { return State; }
            else
            {
                constexpr bool i32_i64_merge{uwvm_interpreter_stacktop_range_is_same(CompileOption.i32_stack_top_begin_pos,
                                                                                     CompileOption.i32_stack_top_end_pos,
                                                                                     CompileOption.i64_stack_top_begin_pos,
                                                                                     CompileOption.i64_stack_top_end_pos)};
                constexpr bool i32_f32_merge{uwvm_interpreter_stacktop_range_is_same(CompileOption.i32_stack_top_begin_pos,
                                                                                     CompileOption.i32_stack_top_end_pos,
                                                                                     CompileOption.f32_stack_top_begin_pos,
                                                                                     CompileOption.f32_stack_top_end_pos)};
                constexpr bool i32_f64_merge{uwvm_interpreter_stacktop_range_is_same(CompileOption.i32_stack_top_begin_pos,
                                                                                     CompileOption.i32_stack_top_end_pos,
                                                                                     CompileOption.f64_stack_top_begin_pos,
                                                                                     CompileOption.f64_stack_top_end_pos)};
                constexpr bool f32_f64_merge{uwvm_interpreter_stacktop_range_is_same(CompileOption.f32_stack_top_begin_pos,
                                                                                     CompileOption.f32_stack_top_end_pos,
                                                                                     CompileOption.f64_stack_top_begin_pos,
                                                                                     CompileOption.f64_stack_top_end_pos)};
                constexpr bool f32_v128_merge{uwvm_interpreter_stacktop_range_is_same(CompileOption.f32_stack_top_begin_pos,
                                                                                      CompileOption.f32_stack_top_end_pos,
                                                                                      CompileOption.v128_stack_top_begin_pos,
                                                                                      CompileOption.v128_stack_top_end_pos)};
                constexpr bool f64_v128_merge{uwvm_interpreter_stacktop_range_is_same(CompileOption.f64_stack_top_begin_pos,
                                                                                      CompileOption.f64_stack_top_end_pos,
                                                                                      CompileOption.v128_stack_top_begin_pos,
                                                                                      CompileOption.v128_stack_top_end_pos)};
                constexpr bool i32_i64_f32_f64_merge{i32_i64_merge && i32_f32_merge && i32_f64_merge};
                constexpr bool f32_f64_v128_merge{f32_f64_merge && f32_v128_merge && f64_v128_merge};

                uwvm_interpreter_stacktop_state_t next_state{State};

                if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>)
                {
                    constexpr ::std::size_t new_remain{State.remain.i32_stack_top_remain_size - 1uz};
                    constexpr ::std::size_t new_pos{uwvm_interpreter_stacktop_next_pos(State.currpos.i32_stack_top_curr_pos,
                                                                                       CompileOption.i32_stack_top_begin_pos,
                                                                                       CompileOption.i32_stack_top_end_pos)};

                    next_state.remain.i32_stack_top_remain_size = new_remain;
                    next_state.currpos.i32_stack_top_curr_pos = new_pos;

                    if constexpr(i32_i64_f32_f64_merge)
                    {
                        next_state.remain.i64_stack_top_remain_size = new_remain;
                        next_state.remain.f32_stack_top_remain_size = new_remain;
                        next_state.remain.f64_stack_top_remain_size = new_remain;
                        next_state.currpos.i64_stack_top_curr_pos = new_pos;
                        next_state.currpos.f32_stack_top_curr_pos = new_pos;
                        next_state.currpos.f64_stack_top_curr_pos = new_pos;
                    }
                    else if constexpr(i32_i64_merge)
                    {
                        next_state.remain.i64_stack_top_remain_size = new_remain;
                        next_state.currpos.i64_stack_top_curr_pos = new_pos;
                    }
                    else if constexpr(i32_f32_merge)
                    {
                        next_state.remain.f32_stack_top_remain_size = new_remain;
                        next_state.currpos.f32_stack_top_curr_pos = new_pos;
                    }

                    return next_state;
                }
                else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64>)
                {
                    constexpr ::std::size_t new_remain{State.remain.i64_stack_top_remain_size - 1uz};
                    constexpr ::std::size_t new_pos{uwvm_interpreter_stacktop_next_pos(State.currpos.i64_stack_top_curr_pos,
                                                                                       CompileOption.i64_stack_top_begin_pos,
                                                                                       CompileOption.i64_stack_top_end_pos)};

                    next_state.remain.i64_stack_top_remain_size = new_remain;
                    next_state.currpos.i64_stack_top_curr_pos = new_pos;

                    if constexpr(i32_i64_f32_f64_merge)
                    {
                        next_state.remain.i32_stack_top_remain_size = new_remain;
                        next_state.remain.f32_stack_top_remain_size = new_remain;
                        next_state.remain.f64_stack_top_remain_size = new_remain;
                        next_state.currpos.i32_stack_top_curr_pos = new_pos;
                        next_state.currpos.f32_stack_top_curr_pos = new_pos;
                        next_state.currpos.f64_stack_top_curr_pos = new_pos;
                    }
                    else if constexpr(i32_i64_merge)
                    {
                        next_state.remain.i32_stack_top_remain_size = new_remain;
                        next_state.currpos.i32_stack_top_curr_pos = new_pos;
                    }

                    return next_state;
                }
                else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>)
                {
                    constexpr ::std::size_t new_remain{State.remain.f32_stack_top_remain_size - 1uz};
                    constexpr ::std::size_t new_pos{uwvm_interpreter_stacktop_next_pos(State.currpos.f32_stack_top_curr_pos,
                                                                                       CompileOption.f32_stack_top_begin_pos,
                                                                                       CompileOption.f32_stack_top_end_pos)};

                    next_state.remain.f32_stack_top_remain_size = new_remain;
                    next_state.currpos.f32_stack_top_curr_pos = new_pos;

                    if constexpr(f32_f64_v128_merge)
                    {
                        next_state.remain.f64_stack_top_remain_size = new_remain;
                        next_state.remain.v128_stack_top_remain_size = new_remain;
                        next_state.currpos.f64_stack_top_curr_pos = new_pos;
                        next_state.currpos.v128_stack_top_curr_pos = new_pos;
                    }
                    else if constexpr(i32_i64_f32_f64_merge)
                    {
                        next_state.remain.i32_stack_top_remain_size = new_remain;
                        next_state.remain.i64_stack_top_remain_size = new_remain;
                        next_state.remain.f64_stack_top_remain_size = new_remain;
                        next_state.currpos.i32_stack_top_curr_pos = new_pos;
                        next_state.currpos.i64_stack_top_curr_pos = new_pos;
                        next_state.currpos.f64_stack_top_curr_pos = new_pos;
                    }
                    else if constexpr(f32_f64_merge)
                    {
                        next_state.remain.f64_stack_top_remain_size = new_remain;
                        next_state.currpos.f64_stack_top_curr_pos = new_pos;
                    }
                    else if constexpr(i32_f32_merge)
                    {
                        next_state.remain.i32_stack_top_remain_size = new_remain;
                        next_state.currpos.i32_stack_top_curr_pos = new_pos;
                    }

                    return next_state;
                }
                else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>)
                {
                    constexpr ::std::size_t new_remain{State.remain.f64_stack_top_remain_size - 1uz};
                    constexpr ::std::size_t new_pos{uwvm_interpreter_stacktop_next_pos(State.currpos.f64_stack_top_curr_pos,
                                                                                       CompileOption.f64_stack_top_begin_pos,
                                                                                       CompileOption.f64_stack_top_end_pos)};

                    next_state.remain.f64_stack_top_remain_size = new_remain;
                    next_state.currpos.f64_stack_top_curr_pos = new_pos;

                    if constexpr(f32_f64_v128_merge)
                    {
                        next_state.remain.f32_stack_top_remain_size = new_remain;
                        next_state.remain.v128_stack_top_remain_size = new_remain;
                        next_state.currpos.f32_stack_top_curr_pos = new_pos;
                        next_state.currpos.v128_stack_top_curr_pos = new_pos;
                    }
                    else if constexpr(i32_i64_f32_f64_merge)
                    {
                        next_state.remain.i32_stack_top_remain_size = new_remain;
                        next_state.remain.i64_stack_top_remain_size = new_remain;
                        next_state.remain.f32_stack_top_remain_size = new_remain;
                        next_state.currpos.i32_stack_top_curr_pos = new_pos;
                        next_state.currpos.i64_stack_top_curr_pos = new_pos;
                        next_state.currpos.f32_stack_top_curr_pos = new_pos;
                    }
                    else if constexpr(f32_f64_merge)
                    {
                        next_state.remain.f32_stack_top_remain_size = new_remain;
                        next_state.currpos.f32_stack_top_curr_pos = new_pos;
                    }

                    return next_state;
                }
                else if constexpr(::std::same_as<ValType, ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128>)
                {
                    constexpr ::std::size_t new_remain{State.remain.v128_stack_top_remain_size - 1uz};
                    constexpr ::std::size_t new_pos{uwvm_interpreter_stacktop_next_pos(State.currpos.v128_stack_top_curr_pos,
                                                                                       CompileOption.v128_stack_top_begin_pos,
                                                                                       CompileOption.v128_stack_top_end_pos)};

                    next_state.remain.v128_stack_top_remain_size = new_remain;
                    next_state.currpos.v128_stack_top_curr_pos = new_pos;

                    if constexpr(f32_f64_v128_merge)
                    {
                        next_state.remain.f32_stack_top_remain_size = new_remain;
                        next_state.remain.f64_stack_top_remain_size = new_remain;
                        next_state.currpos.f32_stack_top_curr_pos = new_pos;
                        next_state.currpos.f64_stack_top_curr_pos = new_pos;
                    }

                    return next_state;
                }
                else
                {
                    static_assert(is_uwvm_interpreter_valtype_supported<ValType>());
                    return State;
                }
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  uwvm_interpreter_stacktop_state_t State,
                  typename ValTuple,
                  ::std::size_t Index,
                  uwvm_int_stack_top_type... TypeRef>
            requires (::fast_io::is_tuple<ValTuple>)
        UWVM_ALWAYS_INLINE inline constexpr void fill_uwvm_interpreter_vals_from_operand_stack(ValTuple& ret, TypeRef&... typeref) noexcept
        {
            if constexpr(Index < ::fast_io::tuple_size<ValTuple>::value)
            {
                using curr_val_t = typename ::fast_io::tuple_element<Index, ValTuple>::type;
                static_assert(is_uwvm_interpreter_valtype_supported<curr_val_t>());

                constexpr bool can_use_stacktop{uwvm_interpreter_can_get_val_from_stacktop_cache<CompileOption, State, curr_val_t>()};
                if constexpr(can_use_stacktop)
                {
                    constexpr ::std::size_t curr_pos{get_uwvm_interpreter_stacktop_currpos<State, curr_val_t>()};
                    ::fast_io::get<Index>(ret) = get_curr_val_from_operand_stack_top<CompileOption, curr_val_t, curr_pos>(typeref...);
                }
                else
                {
                    ::fast_io::get<Index>(ret) = get_curr_val_from_operand_stack_cache<curr_val_t>(typeref...);
                }

                fill_uwvm_interpreter_vals_from_operand_stack<CompileOption,
                                                              pop_uwvm_interpreter_stacktop_state<CompileOption, State, curr_val_t>(),
                                                              ValTuple,
                                                              Index + 1uz>(ret, typeref...);
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_interpreter_stacktop_state_t State, typename ValTuple, ::std::size_t Index>
            requires (::fast_io::is_tuple<ValTuple>)
        inline consteval uwvm_interpreter_stacktop_state_t calc_uwvm_interpreter_stacktop_state_after() noexcept
        {
            if constexpr(Index < ::fast_io::tuple_size<ValTuple>::value)
            {
                using curr_val_t = typename ::fast_io::tuple_element<Index, ValTuple>::type;
                static_assert(is_uwvm_interpreter_valtype_supported<curr_val_t>());

                return calc_uwvm_interpreter_stacktop_state_after<CompileOption,
                                                                  pop_uwvm_interpreter_stacktop_state<CompileOption, State, curr_val_t>(),
                                                                  ValTuple,
                                                                  Index + 1uz>();
            }
            else
            {
                return State;
            }
        }
    }  // namespace details

    template <uwvm_interpreter_translate_option_t CompileOption,
              uwvm_interpreter_stacktop_currpos_t CurrStackTop,
              typename ValTuple,
              uwvm_int_stack_top_type... TypeRef>
        requires (::fast_io::is_tuple<ValTuple>)
    UWVM_ALWAYS_INLINE inline constexpr ValTuple get_vals_from_operand_stack(TypeRef & ... typeref) noexcept
    {
        details::check_uwvm_interpreter_stacktop_layout<CompileOption, CurrStackTop, TypeRef...>();
        constexpr auto state{details::make_uwvm_interpreter_stacktop_initial_state<CompileOption, CurrStackTop>()};

        ValTuple ret;  // no init
        details::fill_uwvm_interpreter_vals_from_operand_stack<CompileOption, state, ValTuple, 0uz>(ret, typeref...);
        return ret;
    }

    template <uwvm_interpreter_translate_option_t CompileOption,
              uwvm_interpreter_stacktop_currpos_t CurrStackTop,
              typename ValTuple,
              uwvm_int_stack_top_type... TypeRef>
        requires (::fast_io::is_tuple<ValTuple>)
    inline consteval uwvm_interpreter_stacktop_remain_size_t get_remain_size_from_operand_stack(TypeRef & ...) noexcept
    {
        details::check_uwvm_interpreter_stacktop_layout<CompileOption, CurrStackTop, TypeRef...>();
        constexpr auto state{details::make_uwvm_interpreter_stacktop_initial_state<CompileOption, CurrStackTop>()};
        constexpr auto final_state{details::calc_uwvm_interpreter_stacktop_state_after<CompileOption, state, ValTuple, 0uz>()};
        return final_state.remain;
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
