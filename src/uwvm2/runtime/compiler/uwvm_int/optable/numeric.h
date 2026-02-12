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
# include <bit>
# include <cmath>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <concepts>
# include <limits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
// platform
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
#  include <cfenv>
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
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
    namespace numeric_details
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_u64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

        static_assert(sizeof(wasm_i32) == sizeof(wasm_u32));
        static_assert(sizeof(wasm_i64) == sizeof(wasm_u64));

        template <typename SignedT, typename UnsignedT>
        UWVM_ALWAYS_INLINE inline constexpr UnsignedT to_unsigned_bits(SignedT v) noexcept
        {
            static_assert(sizeof(SignedT) == sizeof(UnsignedT));
            return ::std::bit_cast<UnsignedT>(v);
        }

        template <typename SignedT, typename UnsignedT>
        UWVM_ALWAYS_INLINE inline constexpr SignedT from_unsigned_bits(UnsignedT v) noexcept
        {
            static_assert(sizeof(SignedT) == sizeof(UnsignedT));
            return ::std::bit_cast<SignedT>(v);
        }

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

        template <uwvm_interpreter_translate_option_t CompileOption, typename OperandT>
        inline consteval ::std::size_t range_begin() noexcept
        {
            if constexpr(::std::same_as<OperandT, wasm_i32>) { return CompileOption.i32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_i64>) { return CompileOption.i64_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f32>) { return CompileOption.f32_stack_top_begin_pos; }
            else if constexpr(::std::same_as<OperandT, wasm_f64>) { return CompileOption.f64_stack_top_begin_pos; }
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
            else
            {
                return SIZE_MAX;
            }
        }

        UWVM_NOINLINE UWVM_GNU_COLD [[noreturn]] inline void trap_integer_divide_by_zero() noexcept
        {
            if(::uwvm2::runtime::compiler::uwvm_int::optable::trap_integer_divide_by_zero_func == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            ::uwvm2::runtime::compiler::uwvm_int::optable::trap_integer_divide_by_zero_func();
            ::fast_io::fast_terminate();
        }

        UWVM_NOINLINE UWVM_GNU_COLD [[noreturn]] inline void trap_integer_overflow() noexcept
        {
            if(::uwvm2::runtime::compiler::uwvm_int::optable::trap_integer_overflow_func == nullptr) [[unlikely]]
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::fast_io::fast_terminate();
            }

            ::uwvm2::runtime::compiler::uwvm_int::optable::trap_integer_overflow_func();
            ::fast_io::fast_terminate();
        }

        // Why this wrapper exists:
        // - In the threaded interpreter, opfuncs must keep the hot path strictly leaf so they can dispatch with `br` without setting up a frame.
        // - A direct trap call would compile to `bl ...`, which clobbers `x30` (LR) on AArch64 and typically forces an entry prologue
        //   (`stp x29, x30, ...`) even though the trap is cold and `[[noreturn]]`.
        // - By tail-calling this cold, noinline wrapper, the compiler emits a plain `b ...` from the opfunc to this helper, keeping the opfunc leaf.
        //   The wrapper may build a frame and call the user-provided trap hook; that cost is paid only on the exceptional path.
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_COLD_MACRO UWVM_NOINLINE inline constexpr void trap_integer_divide_by_zero_tail(Type... /*type*/) UWVM_THROWS
        { trap_integer_divide_by_zero(); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_COLD_MACRO UWVM_NOINLINE inline constexpr void trap_integer_overflow_tail(Type... /*type*/) UWVM_THROWS
        { trap_integer_overflow(); }

        enum class int_unop
        {
            clz,
            ctz,
            popcnt
        };

        enum class int_binop
        {
            add,
            sub,
            mul,
            div_s,
            div_u,
            rem_s,
            rem_u,
            and_,
            or_,
            xor_,
            shl,
            shr_s,
            shr_u,
            rotl,
            rotr
        };

        enum class float_unop
        {
            abs,
            neg,
            ceil,
            floor,
            trunc,
            nearest,
            sqrt
        };

        enum class float_binop
        {
            add,
            sub,
            mul,
            div,
            min,
            max,
            copysign
        };

        template <int_unop Op, typename SignedT, typename UnsignedT>
        UWVM_ALWAYS_INLINE inline constexpr SignedT eval_int_unop(SignedT v) noexcept
        {
            if constexpr(Op == int_unop::clz)
            {
                UnsignedT const u{to_unsigned_bits<SignedT, UnsignedT>(v)};
                return static_cast<SignedT>(::std::countl_zero(u));
            }
            else if constexpr(Op == int_unop::ctz)
            {
                UnsignedT const u{to_unsigned_bits<SignedT, UnsignedT>(v)};
                return static_cast<SignedT>(::std::countr_zero(u));
            }
            else if constexpr(Op == int_unop::popcnt)
            {
                UnsignedT const u{to_unsigned_bits<SignedT, UnsignedT>(v)};
                return static_cast<SignedT>(::std::popcount(u));
            }
            else
            {
                return {};
            }
        }

        template <int_binop Op, typename SignedT, typename UnsignedT>
        UWVM_ALWAYS_INLINE inline constexpr SignedT eval_int_binop(SignedT lhs, SignedT rhs) UWVM_THROWS
        {
            if constexpr(Op == int_binop::add)
            {
                UnsignedT const out{static_cast<UnsignedT>(to_unsigned_bits<SignedT, UnsignedT>(lhs) + to_unsigned_bits<SignedT, UnsignedT>(rhs))};
                return from_unsigned_bits<SignedT, UnsignedT>(out);
            }
            else if constexpr(Op == int_binop::sub)
            {
                UnsignedT const out{static_cast<UnsignedT>(to_unsigned_bits<SignedT, UnsignedT>(lhs) - to_unsigned_bits<SignedT, UnsignedT>(rhs))};
                return from_unsigned_bits<SignedT, UnsignedT>(out);
            }
            else if constexpr(Op == int_binop::mul)
            {
                UnsignedT const out{static_cast<UnsignedT>(to_unsigned_bits<SignedT, UnsignedT>(lhs) * to_unsigned_bits<SignedT, UnsignedT>(rhs))};
                return from_unsigned_bits<SignedT, UnsignedT>(out);
            }
            else if constexpr(Op == int_binop::and_)
            {
                UnsignedT const out{static_cast<UnsignedT>(to_unsigned_bits<SignedT, UnsignedT>(lhs) & to_unsigned_bits<SignedT, UnsignedT>(rhs))};
                return from_unsigned_bits<SignedT, UnsignedT>(out);
            }
            else if constexpr(Op == int_binop::or_)
            {
                UnsignedT const out{static_cast<UnsignedT>(to_unsigned_bits<SignedT, UnsignedT>(lhs) | to_unsigned_bits<SignedT, UnsignedT>(rhs))};
                return from_unsigned_bits<SignedT, UnsignedT>(out);
            }
            else if constexpr(Op == int_binop::xor_)
            {
                UnsignedT const out{static_cast<UnsignedT>(to_unsigned_bits<SignedT, UnsignedT>(lhs) ^ to_unsigned_bits<SignedT, UnsignedT>(rhs))};
                return from_unsigned_bits<SignedT, UnsignedT>(out);
            }
            else if constexpr(Op == int_binop::shl)
            {
                constexpr unsigned mask{static_cast<unsigned>(sizeof(UnsignedT) * 8u - 1u)};
                unsigned const sh{static_cast<unsigned>(to_unsigned_bits<SignedT, UnsignedT>(rhs)) & mask};
                UnsignedT const out{static_cast<UnsignedT>(to_unsigned_bits<SignedT, UnsignedT>(lhs) << sh)};
                return from_unsigned_bits<SignedT, UnsignedT>(out);
            }
            else if constexpr(Op == int_binop::shr_u)
            {
                constexpr unsigned mask{static_cast<unsigned>(sizeof(UnsignedT) * 8u - 1u)};
                unsigned const sh{static_cast<unsigned>(to_unsigned_bits<SignedT, UnsignedT>(rhs)) & mask};
                UnsignedT const out{static_cast<UnsignedT>(to_unsigned_bits<SignedT, UnsignedT>(lhs) >> sh)};
                return from_unsigned_bits<SignedT, UnsignedT>(out);
            }
            else if constexpr(Op == int_binop::shr_s)
            {
                constexpr unsigned mask{static_cast<unsigned>(sizeof(UnsignedT) * 8u - 1u)};
                unsigned const sh{static_cast<unsigned>(to_unsigned_bits<SignedT, UnsignedT>(rhs)) & mask};
                return static_cast<SignedT>(lhs >> sh);
            }
            else if constexpr(Op == int_binop::rotl)
            {
                constexpr unsigned mask{static_cast<unsigned>(sizeof(UnsignedT) * 8u - 1u)};
                int const sh{static_cast<int>(static_cast<unsigned>(to_unsigned_bits<SignedT, UnsignedT>(rhs)) & mask)};
                UnsignedT const out{::std::rotl(to_unsigned_bits<SignedT, UnsignedT>(lhs), sh)};
                return from_unsigned_bits<SignedT, UnsignedT>(out);
            }
            else if constexpr(Op == int_binop::rotr)
            {
                constexpr unsigned mask{static_cast<unsigned>(sizeof(UnsignedT) * 8u - 1u)};
                int const sh{static_cast<int>(static_cast<unsigned>(to_unsigned_bits<SignedT, UnsignedT>(rhs)) & mask)};
                UnsignedT const out{::std::rotr(to_unsigned_bits<SignedT, UnsignedT>(lhs), sh)};
                return from_unsigned_bits<SignedT, UnsignedT>(out);
            }
            else if constexpr(Op == int_binop::div_s)
            {
                if(rhs == SignedT{0}) [[unlikely]] { trap_integer_divide_by_zero(); }
                if(lhs == ::std::numeric_limits<SignedT>::min() && rhs == SignedT{-1}) [[unlikely]] { trap_integer_overflow(); }
                return static_cast<SignedT>(lhs / rhs);
            }
            else if constexpr(Op == int_binop::div_u)
            {
                UnsignedT const urhs{to_unsigned_bits<SignedT, UnsignedT>(rhs)};
                if(urhs == UnsignedT{0}) [[unlikely]] { trap_integer_divide_by_zero(); }
                UnsignedT const out{static_cast<UnsignedT>(to_unsigned_bits<SignedT, UnsignedT>(lhs) / urhs)};
                return from_unsigned_bits<SignedT, UnsignedT>(out);
            }
            else if constexpr(Op == int_binop::rem_s)
            {
                if(rhs == SignedT{0}) [[unlikely]] { trap_integer_divide_by_zero(); }
                if(lhs == ::std::numeric_limits<SignedT>::min() && rhs == SignedT{-1}) { return SignedT{0}; }
                return static_cast<SignedT>(lhs % rhs);
            }
            else if constexpr(Op == int_binop::rem_u)
            {
                UnsignedT const urhs{to_unsigned_bits<SignedT, UnsignedT>(rhs)};
                if(urhs == UnsignedT{0}) [[unlikely]] { trap_integer_divide_by_zero(); }
                UnsignedT const out{static_cast<UnsignedT>(to_unsigned_bits<SignedT, UnsignedT>(lhs) % urhs)};
                return from_unsigned_bits<SignedT, UnsignedT>(out);
            }
            else
            {
                return {};
            }
        }

        template <float_unop Op, typename FloatT>
        UWVM_ALWAYS_INLINE inline constexpr FloatT eval_float_unop(FloatT v) noexcept
        {
            if constexpr(Op == float_unop::abs) { return ::std::fabs(v); }
            else if constexpr(Op == float_unop::neg) { return -v; }
            else if constexpr(Op == float_unop::ceil) { return ::std::ceil(v); }
            else if constexpr(Op == float_unop::floor) { return ::std::floor(v); }
            else if constexpr(Op == float_unop::trunc) { return ::std::trunc(v); }
            else if constexpr(Op == float_unop::nearest)
            {
                // NOTE:
                // WebAssembly `nearest` has fixed, environment-independent semantics:
                // round to nearest integer, ties to even.
                //
                // The C++ standard library does NOT provide a function with equivalent
                // fixed semantics. In particular, std::nearbyint (and std::rint) are
                // specified to round according to the *current* floating-point rounding
                // mode and therefore remain fenv-dependent in both GCC and Clang.
                //
                // For targets that provide a hardware instruction with an encoded
                // rounding mode (e.g. SSE4.1 `roundss/roundsd` with imm = 8, or AArch64
                // `frinti`), we explicitly use compiler builtins/intrinsics to force
                // round-to-nearest, ties-to-even semantics independent of the FP environment.
                //
                // The std::nearbyint fallback is used only on targets where no such
                // instruction is available. In that case, correctness relies on the
                // runtime invariant that the default rounding mode (FE_TONEAREST)
                // is in effect and not modified.

#if defined(__loongarch_asx)
                using f64x4simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
                using f32x8simd [[__gnu__::__vector_size__(32)]] [[maybe_unused]] = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

                if constexpr(::std::same_as<FloatT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>)
                {
# if UWVM_HAS_BUILTIN(__builtin_lasx_xvfrintrne_s)
                    f32x8simd tmp{v, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
                    return __builtin_lasx_xvfrintrne_s(tmp)[0u];
# else
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(::std::fegetround() != FE_TONEAREST) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif
                    return ::std::nearbyint(v);
# endif
                }
                else if constexpr(::std::same_as<FloatT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>)
                {
# if UWVM_HAS_BUILTIN(__builtin_lasx_xvfrintrne_d)
                    f64x4simd tmp{v, 0.0, 0.0, 0.0};
                    return __builtin_lasx_xvfrintrne_d(tmp)[0u];
# else
                    // Implementation for msvc is not currently being considered; revert to the default implementation.
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(::std::fegetround() != FE_TONEAREST) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif
                    return ::std::nearbyint(v);
# endif
                }
                else
                {
                    // platform
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(::std::fegetround() != FE_TONEAREST) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
                    return ::std::nearbyint(v);
                }

#elif defined(__ARM_NEON)
# if defined(__clang__)
                using float64x1_t [[maybe_unused]] = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 [[clang::neon_vector_type(1)]];
                using float64x2_t [[maybe_unused]] = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64 [[clang::neon_vector_type(2)]];
                using float32x2_t [[maybe_unused]] = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32 [[clang::neon_vector_type(2)]];
                using float32x4_t [[maybe_unused]] = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32 [[clang::neon_vector_type(4)]];
                using int8x8_t [[maybe_unused]] = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i8 [[clang::neon_vector_type(8)]];
# elif (defined(__GNUC__) && !defined(__clang__))  // gcc
                using float64x1_t [[maybe_unused]] = __Float64x1_t;
                using float64x2_t [[maybe_unused]] = __Float64x2_t;
                using float32x2_t [[maybe_unused]] = __Float32x2_t;
                using float32x4_t [[maybe_unused]] = __Float32x4_t;
# elif (defined(_MSC_VER) && !defined(__clang__))  // msvc
                using float64x1_t [[maybe_unused]] = __n64;
                using float64x2_t [[maybe_unused]] = __n128;
                using float32x2_t [[maybe_unused]] = __n64;
                using float32x4_t [[maybe_unused]] = __n128;
# endif

                if constexpr(::std::same_as<FloatT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>)
                {
# if UWVM_HAS_BUILTIN(__builtin_neon_vrndns_f32)
                    return __builtin_neon_vrndns_f32(v);
# elif UWVM_HAS_BUILTIN(__builtin_aarch64_roundevensf)
                    return __builtin_aarch64_roundevensf(v);
# else
                    // Implementation for msvc is not currently being considered; revert to the default implementation.
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(::std::fegetround() != FE_TONEAREST) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif
                    return ::std::nearbyint(v);
# endif
                }
                else if constexpr(::std::same_as<FloatT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>)
                {
# if UWVM_HAS_BUILTIN(__builtin_neon_vrndn_v)
                    return ::std::bit_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>(__builtin_neon_vrndn_v(::std::bit_cast<int8x8_t>(v), 10));
# elif UWVM_HAS_BUILTIN(__builtin_aarch64_roundevendf)
                    return ::std::bit_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>(
                        __builtin_aarch64_roundevendf(::std::bit_cast<float64x1_t>(v)));
# else
                    // Implementation for msvc is not currently being considered; revert to the default implementation.
#  if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(::std::fegetround() != FE_TONEAREST) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#  endif
                    return ::std::nearbyint(v);
# endif
                }
                else
                {
                    // platform
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(::std::fegetround() != FE_TONEAREST) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
                    return ::std::nearbyint(v);
                }

#elif defined(__SSE4_1__) && UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && (defined(__builtin_ia32_roundss) && defined(__builtin_ia32_roundsd))
                if constexpr(::std::same_as<FloatT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32>)
                {
                    using f32x4simd [[__gnu__::__vector_size__(16)]] = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
                    f32x4simd const tmp{v, 0.0f, 0.0f, 0.0f};
                    return __builtin_ia32_roundss(tmp, tmp, 8)[0];
                }
                else if constexpr(::std::same_as<FloatT, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64>)
                {
                    using f64x2simd [[__gnu__::__vector_size__(16)]] = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
                    f64x2simd const tmp{v, 0.0};
                    return __builtin_ia32_roundsd(tmp, tmp, 8)[0];
                }
                else
                {
                    // platform
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(::std::fegetround() != FE_TONEAREST) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
                    return ::std::nearbyint(v);
                }
#else

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(::std::fegetround() != FE_TONEAREST) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
                return ::std::nearbyint(v);
#endif
            }
            else if constexpr(Op == float_unop::sqrt) { return ::std::sqrt(v); }
            else
            {
                return {};
            }
        }

        template <float_binop Op, typename FloatT>
        UWVM_ALWAYS_INLINE inline constexpr FloatT eval_float_binop(FloatT lhs, FloatT rhs) noexcept
        {
            // NOTE:
            // Do NOT use std::fmin / std::fmax here.
            //
            // std::fmin follows the C math library semantics, where if exactly one
            // operand is NaN, the other operand is returned. This behavior is useful
            // for numerical algorithms, but it is NOT the same as the WebAssembly MVP
            // floating-point operator semantics.
            //
            // In WebAssembly (and IEEE 754 min/max operators), if either operand is NaN,
            // the result must be NaN. In addition, the handling of signed zero is
            // observable and required to be precise (e.g. min(+0, -0) == -0).
            //
            // Therefore, using std::fmin/std::fmax would silently introduce incorrect
            // NaN propagation and break strict WebAssembly FP semantics.
            //
            // We must implement min/max using explicit comparisons (or std::min with
            // controlled ordering) so that NaN and signed-zero behavior exactly matches
            // the WebAssembly specification.

            if constexpr(Op == float_binop::add) { return lhs + rhs; }
            else if constexpr(Op == float_binop::sub) { return lhs - rhs; }
            else if constexpr(Op == float_binop::mul) { return lhs * rhs; }
            else if constexpr(Op == float_binop::div) { return lhs / rhs; }
            else if constexpr(Op == float_binop::copysign) { return ::std::copysign(lhs, rhs); }
            else if constexpr(Op == float_binop::min)
            {
                if(::std::isnan(lhs) || ::std::isnan(rhs)) { return ::std::numeric_limits<FloatT>::quiet_NaN(); }
                if(lhs == rhs) { return ::std::signbit(lhs) ? lhs : rhs; }
                return ::std::min(lhs, rhs);
            }
            else if constexpr(Op == float_binop::max)
            {
                if(::std::isnan(lhs) || ::std::isnan(rhs)) { return ::std::numeric_limits<FloatT>::quiet_NaN(); }
                if(lhs == rhs) { return ::std::signbit(lhs) ? rhs : lhs; }
                return ::std::max(lhs, rhs);
            }
            else
            {
                return {};
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  typename OperandT,
                  int_unop Op,
                  ::std::size_t curr_stack_top,
                  uwvm_int_stack_top_type... TypeRef>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void int_unary(TypeRef&... typeref) UWVM_THROWS
        {
            static_assert(sizeof...(TypeRef) >= 2uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

            if constexpr(stacktop_enabled_for<CompileOption, OperandT>())
            {
                constexpr ::std::size_t begin{range_begin<CompileOption, OperandT>()};
                constexpr ::std::size_t end{range_end<CompileOption, OperandT>()};
                static_assert(begin <= curr_stack_top && curr_stack_top < end);

                OperandT const v{get_curr_val_from_operand_stack_top<CompileOption, OperandT, curr_stack_top>(typeref...)};

                if constexpr(::std::same_as<OperandT, wasm_i32>)
                {
                    OperandT const out{eval_int_unop<Op, wasm_i32, wasm_u32>(v)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, OperandT, curr_stack_top>(out, typeref...);
                }
                else
                {
                    OperandT const out{eval_int_unop<Op, wasm_i64, wasm_u64>(v)};
                    details::set_curr_val_to_stacktop_cache<CompileOption, OperandT, curr_stack_top>(out, typeref...);
                }
            }
            else
            {
                OperandT const v{get_curr_val_from_operand_stack_cache<OperandT>(typeref...)};

                OperandT out;  // no init
                if constexpr(::std::same_as<OperandT, wasm_i32>) { out = eval_int_unop<Op, wasm_i32, wasm_u32>(v); }
                else
                {
                    out = eval_int_unop<Op, wasm_i64, wasm_u64>(v);
                }

                ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
                typeref...[1u] += sizeof(out);
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  typename OperandT,
                  int_binop Op,
                  ::std::size_t curr_stack_top,
                  uwvm_int_stack_top_type... TypeRef>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void int_binary(TypeRef&... typeref) UWVM_THROWS
        {
            static_assert(sizeof...(TypeRef) >= 2uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

            if constexpr(stacktop_enabled_for<CompileOption, OperandT>())
            {
                constexpr ::std::size_t begin{range_begin<CompileOption, OperandT>()};
                constexpr ::std::size_t end{range_end<CompileOption, OperandT>()};
                static_assert(begin <= curr_stack_top && curr_stack_top < end);

                constexpr ::std::size_t ring_sz{end - begin};
                static_assert(ring_sz != 0uz);
                constexpr ::std::size_t next_pos{details::ring_next_pos(curr_stack_top, begin, end)};

                OperandT const rhs{get_curr_val_from_operand_stack_top<CompileOption, OperandT, curr_stack_top>(typeref...)};
                OperandT lhs;  // no init
                if constexpr(ring_sz >= 2uz) { lhs = get_curr_val_from_operand_stack_top<CompileOption, OperandT, next_pos>(typeref...); }
                else
                {
                    // Ring too small to hold both operands: keep RHS in cache, load LHS from operand stack memory (no pop).
                    lhs = peek_curr_val_from_operand_stack_cache<OperandT>(typeref...);
                }

                OperandT out;  // no init
                if constexpr(::std::same_as<OperandT, wasm_i32>) { out = eval_int_binop<Op, wasm_i32, wasm_u32>(lhs, rhs); }
                else
                {
                    out = eval_int_binop<Op, wasm_i64, wasm_u64>(lhs, rhs);
                }

                if constexpr(ring_sz >= 2uz) { details::set_curr_val_to_stacktop_cache<CompileOption, OperandT, next_pos>(out, typeref...); }
                else
                {
                    // Binary op: result replaces NOS in operand stack memory (stack height -1).
                    set_curr_val_to_operand_stack_cache_top(out, typeref...);
                }
            }
            else
            {
                OperandT const rhs{get_curr_val_from_operand_stack_cache<OperandT>(typeref...)};
                OperandT const lhs{get_curr_val_from_operand_stack_cache<OperandT>(typeref...)};

                OperandT out;  // no init
                if constexpr(::std::same_as<OperandT, wasm_i32>) { out = eval_int_binop<Op, wasm_i32, wasm_u32>(lhs, rhs); }
                else
                {
                    out = eval_int_binop<Op, wasm_i64, wasm_u64>(lhs, rhs);
                }

                ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
                typeref...[1u] += sizeof(out);
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  typename OperandT,
                  float_unop Op,
                  ::std::size_t curr_stack_top,
                  uwvm_int_stack_top_type... TypeRef>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void float_unary(TypeRef&... typeref) noexcept
        {
            static_assert(sizeof...(TypeRef) >= 2uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

            if constexpr(stacktop_enabled_for<CompileOption, OperandT>())
            {
                constexpr ::std::size_t begin{range_begin<CompileOption, OperandT>()};
                constexpr ::std::size_t end{range_end<CompileOption, OperandT>()};
                static_assert(begin <= curr_stack_top && curr_stack_top < end);

                OperandT const v{get_curr_val_from_operand_stack_top<CompileOption, OperandT, curr_stack_top>(typeref...)};
                OperandT const out{eval_float_unop<Op, OperandT>(v)};
                details::set_curr_val_to_stacktop_cache<CompileOption, OperandT, curr_stack_top>(out, typeref...);
            }
            else
            {
                OperandT const v{get_curr_val_from_operand_stack_cache<OperandT>(typeref...)};
                OperandT const out{eval_float_unop<Op, OperandT>(v)};
                ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
                typeref...[1u] += sizeof(out);
            }
        }

        template <uwvm_interpreter_translate_option_t CompileOption,
                  typename OperandT,
                  float_binop Op,
                  ::std::size_t curr_stack_top,
                  uwvm_int_stack_top_type... TypeRef>
            requires (CompileOption.is_tail_call)
        UWVM_ALWAYS_INLINE inline constexpr void float_binary(TypeRef&... typeref) noexcept
        {
            static_assert(sizeof...(TypeRef) >= 2uz);
            static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);

            if constexpr(stacktop_enabled_for<CompileOption, OperandT>())
            {
                constexpr ::std::size_t begin{range_begin<CompileOption, OperandT>()};
                constexpr ::std::size_t end{range_end<CompileOption, OperandT>()};
                static_assert(begin <= curr_stack_top && curr_stack_top < end);

                constexpr ::std::size_t ring_sz{end - begin};
                static_assert(ring_sz != 0uz);
                constexpr ::std::size_t next_pos{details::ring_next_pos(curr_stack_top, begin, end)};

                OperandT const rhs{get_curr_val_from_operand_stack_top<CompileOption, OperandT, curr_stack_top>(typeref...)};
                OperandT lhs;  // no init
                if constexpr(ring_sz >= 2uz) { lhs = get_curr_val_from_operand_stack_top<CompileOption, OperandT, next_pos>(typeref...); }
                else
                {
                    // Ring too small to hold both operands: keep RHS in cache, load LHS from operand stack memory (no pop).
                    lhs = peek_curr_val_from_operand_stack_cache<OperandT>(typeref...);
                }
                OperandT const out{eval_float_binop<Op, OperandT>(lhs, rhs)};
                if constexpr(ring_sz >= 2uz) { details::set_curr_val_to_stacktop_cache<CompileOption, OperandT, next_pos>(out, typeref...); }
                else
                {
                    // Binary op: result replaces NOS in operand stack memory (stack height -1).
                    set_curr_val_to_operand_stack_cache_top(out, typeref...);
                }
            }
            else
            {
                OperandT const rhs{get_curr_val_from_operand_stack_cache<OperandT>(typeref...)};
                OperandT const lhs{get_curr_val_from_operand_stack_cache<OperandT>(typeref...)};
                OperandT const out{eval_float_binop<Op, OperandT>(lhs, rhs)};
                ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
                typeref...[1u] += sizeof(out);
            }
        }

    }  // namespace numeric_details

    // ========================
    // i32 numeric
    // ========================

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_unop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_unop(Type... type) UWVM_THROWS
    {
        using wasm_i32 = numeric_details::wasm_i32;
        numeric_details::int_unary<CompileOption, wasm_i32, Op, curr_stack_top>(type...);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop(Type... type) UWVM_THROWS
    {
        using wasm_i32 = numeric_details::wasm_i32;
        numeric_details::int_binary<CompileOption, wasm_i32, Op, curr_stack_top>(type...);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_unop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_unop(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = numeric_details::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i32 const v{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_i32 const out{numeric_details::eval_int_unop<Op, wasm_i32, numeric_details::wasm_u32>(v)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_binop(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i32 = numeric_details::wasm_i32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i32 const rhs{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_i32 const lhs{get_curr_val_from_operand_stack_cache<wasm_i32>(typeref...)};
        wasm_i32 const out{numeric_details::eval_int_binop<Op, wasm_i32, numeric_details::wasm_u32>(lhs, rhs)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    // i32 unary
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_clz(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_unop<CompileOption, numeric_details::int_unop::clz>(typeref...); }
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_ctz(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_unop<CompileOption, numeric_details::int_unop::ctz>(typeref...); }
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_popcnt(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_unop<CompileOption, numeric_details::int_unop::popcnt>(typeref...); }

    // i32 binary
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_add(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::add>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_sub(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::sub>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_mul(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::mul>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_div_s(Type... type) UWVM_THROWS
    {
        using wasm_i32 = numeric_details::wasm_i32;

        if constexpr(numeric_details::stacktop_enabled_for<CompileOption, wasm_i32>())
        {
            constexpr ::std::size_t begin{numeric_details::range_begin<CompileOption, wasm_i32>()};
            constexpr ::std::size_t end{numeric_details::range_end<CompileOption, wasm_i32>()};
            static_assert(begin <= curr_stack_top && curr_stack_top < end);

            constexpr ::std::size_t ring_sz{end - begin};
            static_assert(ring_sz != 0uz);
            constexpr ::std::size_t next_pos{details::ring_next_pos(curr_stack_top, begin, end)};

            wasm_i32 const rhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, curr_stack_top>(type...)};
            wasm_i32 lhs;  // no init
            if constexpr(ring_sz >= 2uz) { lhs = get_curr_val_from_operand_stack_top<CompileOption, wasm_i32, next_pos>(type...); }
            else
            {
                // Ring too small to hold both operands: keep RHS in cache, load LHS from operand stack memory (no pop).
                lhs = peek_curr_val_from_operand_stack_cache<wasm_i32>(type...);
            }

            if(rhs == wasm_i32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
            if(lhs == ::std::numeric_limits<wasm_i32>::min() && rhs == wasm_i32{-1}) [[unlikely]]
            {
                UWVM_MUSTTAIL return numeric_details::trap_integer_overflow_tail<CompileOption>(type...);
            }

            wasm_i32 const out{static_cast<wasm_i32>(lhs / rhs)};
            if constexpr(ring_sz >= 2uz) { details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i32, next_pos>(out, type...); }
            else
            {
                // Binary op: result replaces NOS in operand stack memory (stack height -1).
                set_curr_val_to_operand_stack_cache_top(out, type...);
            }
        }
        else
        {
            wasm_i32 const rhs{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};
            wasm_i32 const lhs{get_curr_val_from_operand_stack_cache<wasm_i32>(type...)};

            if(rhs == wasm_i32{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
            if(lhs == ::std::numeric_limits<wasm_i32>::min() && rhs == wasm_i32{-1}) [[unlikely]]
            {
                UWVM_MUSTTAIL return numeric_details::trap_integer_overflow_tail<CompileOption>(type...);
            }

            wasm_i32 const out{static_cast<wasm_i32>(lhs / rhs)};
            ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
            type...[1u] += sizeof(out);
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_div_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::div_s>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_div_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::div_u>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_rem_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::rem_s>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_rem_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::rem_u>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_and(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::and_>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_or(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::or_>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_xor(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::xor_>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_shl(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::shl>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_shr_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::shr_s>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_shr_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::shr_u>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_rotl(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::rotl>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i32_rotr(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i32_binop<CompileOption, numeric_details::int_binop::rotr>(typeref...); }

    // ========================
    // i64 numeric
    // ========================

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_unop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_unop(Type... type) UWVM_THROWS
    {
        using wasm_i64 = numeric_details::wasm_i64;
        numeric_details::int_unary<CompileOption, wasm_i64, Op, curr_stack_top>(type...);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop(Type... type) UWVM_THROWS
    {
        using wasm_i64 = numeric_details::wasm_i64;
        numeric_details::int_binary<CompileOption, wasm_i64, Op, curr_stack_top>(type...);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_unop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_unop(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i64 = numeric_details::wasm_i64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i64 const v{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        wasm_i64 const out{numeric_details::eval_int_unop<Op, wasm_i64, numeric_details::wasm_u64>(v)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::int_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_binop(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_i64 = numeric_details::wasm_i64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_i64 const rhs{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        wasm_i64 const lhs{get_curr_val_from_operand_stack_cache<wasm_i64>(typeref...)};
        wasm_i64 const out{numeric_details::eval_int_binop<Op, wasm_i64, numeric_details::wasm_u64>(lhs, rhs)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    // i64 unary
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_clz(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_unop<CompileOption, numeric_details::int_unop::clz>(typeref...); }
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_ctz(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_unop<CompileOption, numeric_details::int_unop::ctz>(typeref...); }
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_popcnt(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_unop<CompileOption, numeric_details::int_unop::popcnt>(typeref...); }

    // i64 binary
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_add(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::add>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_sub(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::sub>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_mul(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::mul>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_div_s(Type... type) UWVM_THROWS
    {
        using wasm_i64 = numeric_details::wasm_i64;

        if constexpr(numeric_details::stacktop_enabled_for<CompileOption, wasm_i64>())
        {
            constexpr ::std::size_t begin{numeric_details::range_begin<CompileOption, wasm_i64>()};
            constexpr ::std::size_t end{numeric_details::range_end<CompileOption, wasm_i64>()};
            static_assert(begin <= curr_stack_top && curr_stack_top < end);

            constexpr ::std::size_t ring_sz{end - begin};
            static_assert(ring_sz != 0uz);
            constexpr ::std::size_t next_pos{details::ring_next_pos(curr_stack_top, begin, end)};

            wasm_i64 const rhs{get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, curr_stack_top>(type...)};
            wasm_i64 lhs;  // no init
            if constexpr(ring_sz >= 2uz) { lhs = get_curr_val_from_operand_stack_top<CompileOption, wasm_i64, next_pos>(type...); }
            else
            {
                // Ring too small to hold both operands: keep RHS in cache, load LHS from operand stack memory (no pop).
                lhs = peek_curr_val_from_operand_stack_cache<wasm_i64>(type...);
            }

            if(rhs == wasm_i64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
            if(lhs == ::std::numeric_limits<wasm_i64>::min() && rhs == wasm_i64{-1}) [[unlikely]]
            {
                UWVM_MUSTTAIL return numeric_details::trap_integer_overflow_tail<CompileOption>(type...);
            }

            wasm_i64 const out{static_cast<wasm_i64>(lhs / rhs)};
            if constexpr(ring_sz >= 2uz) { details::set_curr_val_to_stacktop_cache<CompileOption, wasm_i64, next_pos>(out, type...); }
            else
            {
                // Binary op: result replaces NOS in operand stack memory (stack height -1).
                set_curr_val_to_operand_stack_cache_top(out, type...);
            }
        }
        else
        {
            wasm_i64 const rhs{get_curr_val_from_operand_stack_cache<wasm_i64>(type...)};
            wasm_i64 const lhs{get_curr_val_from_operand_stack_cache<wasm_i64>(type...)};

            if(rhs == wasm_i64{0}) [[unlikely]] { UWVM_MUSTTAIL return numeric_details::trap_integer_divide_by_zero_tail<CompileOption>(type...); }
            if(lhs == ::std::numeric_limits<wasm_i64>::min() && rhs == wasm_i64{-1}) [[unlikely]]
            {
                UWVM_MUSTTAIL return numeric_details::trap_integer_overflow_tail<CompileOption>(type...);
            }

            wasm_i64 const out{static_cast<wasm_i64>(lhs / rhs)};
            ::std::memcpy(type...[1u], ::std::addressof(out), sizeof(out));
            type...[1u] += sizeof(out);
        }

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_div_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::div_s>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_div_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::div_u>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_rem_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::rem_s>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_rem_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::rem_u>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_and(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::and_>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_or(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::or_>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_xor(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::xor_>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_shl(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::shl>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_shr_s(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::shr_s>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_shr_u(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::shr_u>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_rotl(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::rotl>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_i64_rotr(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_i64_binop<CompileOption, numeric_details::int_binop::rotr>(typeref...); }

    // ========================
    // f32/f64 numeric (strict-fp)
    // ========================

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_unop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_unop(Type... type) UWVM_THROWS
    {
        using wasm_f32 = numeric_details::wasm_f32;
        numeric_details::float_unary<CompileOption, wasm_f32, Op, curr_stack_top>(type...);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_binop(Type... type) UWVM_THROWS
    {
        using wasm_f32 = numeric_details::wasm_f32;
        numeric_details::float_binary<CompileOption, wasm_f32, Op, curr_stack_top>(type...);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_unop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_unop(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = numeric_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f32 const v{get_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
        wasm_f32 const out{numeric_details::eval_float_unop<Op, wasm_f32>(v)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_binop(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f32 = numeric_details::wasm_f32;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f32 const rhs{get_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
        wasm_f32 const lhs{get_curr_val_from_operand_stack_cache<wasm_f32>(typeref...)};
        wasm_f32 const out{numeric_details::eval_float_binop<Op, wasm_f32>(lhs, rhs)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    // f32 unary wrappers
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_abs(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_unop<CompileOption, numeric_details::float_unop::abs>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_neg(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_unop<CompileOption, numeric_details::float_unop::neg>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_ceil(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_unop<CompileOption, numeric_details::float_unop::ceil>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_floor(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_unop<CompileOption, numeric_details::float_unop::floor>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_trunc(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_unop<CompileOption, numeric_details::float_unop::trunc>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_nearest(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_unop<CompileOption, numeric_details::float_unop::nearest>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_sqrt(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_unop<CompileOption, numeric_details::float_unop::sqrt>(typeref...); }

    // f32 binary wrappers
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_add(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop<CompileOption, numeric_details::float_binop::add>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_sub(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop<CompileOption, numeric_details::float_binop::sub>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_mul(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop<CompileOption, numeric_details::float_binop::mul>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_div(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop<CompileOption, numeric_details::float_binop::div>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_min(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop<CompileOption, numeric_details::float_binop::min>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_max(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop<CompileOption, numeric_details::float_binop::max>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f32_copysign(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f32_binop<CompileOption, numeric_details::float_binop::copysign>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_unop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_unop(Type... type) UWVM_THROWS
    {
        using wasm_f64 = numeric_details::wasm_f64;
        numeric_details::float_unary<CompileOption, wasm_f64, Op, curr_stack_top>(type...);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, ::std::size_t curr_stack_top, uwvm_int_stack_top_type... Type>
        requires (CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_binop(Type... type) UWVM_THROWS
    {
        using wasm_f64 = numeric_details::wasm_f64;
        numeric_details::float_binary<CompileOption, wasm_f64, Op, curr_stack_top>(type...);

        type...[0] += sizeof(uwvm_interpreter_opfunc_t<Type...>);
        uwvm_interpreter_opfunc_t<Type...> next_interpreter;  // no init
        ::std::memcpy(::std::addressof(next_interpreter), type...[0], sizeof(next_interpreter));
        UWVM_MUSTTAIL return next_interpreter(type...);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_unop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_unop(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = numeric_details::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f64 const v{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        wasm_f64 const out{numeric_details::eval_float_unop<Op, wasm_f64>(v)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    template <uwvm_interpreter_translate_option_t CompileOption, numeric_details::float_binop Op, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_binop(TypeRef & ... typeref) UWVM_THROWS
    {
        using wasm_f64 = numeric_details::wasm_f64;

        static_assert(sizeof...(TypeRef) >= 2uz);
        static_assert(::std::same_as<TypeRef...[0u], ::std::byte const*>);
        static_assert(::std::same_as<::std::remove_cvref_t<TypeRef...[1u]>, ::std::byte*>);
        static_assert(CompileOption.i32_stack_top_begin_pos == SIZE_MAX && CompileOption.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.i64_stack_top_begin_pos == SIZE_MAX && CompileOption.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f32_stack_top_begin_pos == SIZE_MAX && CompileOption.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.f64_stack_top_begin_pos == SIZE_MAX && CompileOption.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(CompileOption.v128_stack_top_begin_pos == SIZE_MAX && CompileOption.v128_stack_top_end_pos == SIZE_MAX);

        typeref...[0] += sizeof(uwvm_interpreter_opfunc_byref_t<TypeRef...>);

        wasm_f64 const rhs{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        wasm_f64 const lhs{get_curr_val_from_operand_stack_cache<wasm_f64>(typeref...)};
        wasm_f64 const out{numeric_details::eval_float_binop<Op, wasm_f64>(lhs, rhs)};
        ::std::memcpy(typeref...[1u], ::std::addressof(out), sizeof(out));
        typeref...[1u] += sizeof(out);
    }

    // f64 unary wrappers
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_abs(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_unop<CompileOption, numeric_details::float_unop::abs>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_neg(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_unop<CompileOption, numeric_details::float_unop::neg>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_ceil(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_unop<CompileOption, numeric_details::float_unop::ceil>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_floor(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_unop<CompileOption, numeric_details::float_unop::floor>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_trunc(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_unop<CompileOption, numeric_details::float_unop::trunc>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_nearest(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_unop<CompileOption, numeric_details::float_unop::nearest>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_sqrt(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_unop<CompileOption, numeric_details::float_unop::sqrt>(typeref...); }

    // f64 binary wrappers
    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_add(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop<CompileOption, numeric_details::float_binop::add>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_sub(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop<CompileOption, numeric_details::float_binop::sub>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_mul(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop<CompileOption, numeric_details::float_binop::mul>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_div(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop<CompileOption, numeric_details::float_binop::div>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_min(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop<CompileOption, numeric_details::float_binop::min>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_max(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop<CompileOption, numeric_details::float_binop::max>(typeref...); }

    template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeRef>
        requires (!CompileOption.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_f64_copysign(TypeRef & ... typeref) UWVM_THROWS
    { return uwvmint_f64_binop<CompileOption, numeric_details::float_binop::copysign>(typeref...); }

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
            inline constexpr uwvm_interpreter_opfunc_t<Type...> select_stacktop_fptr_by_currpos_impl_numeric(::std::size_t pos) noexcept
            {
                static_assert(Curr < End);
                if(pos == Curr) { return OpWrapper::template fptr<CompileOption, Curr, Type...>(); }
                else
                {
                    if constexpr(Curr + 1uz < End)
                    {
                        return select_stacktop_fptr_by_currpos_impl_numeric<CompileOption, Curr + 1uz, End, OpWrapper, Type...>(pos);
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

            // i32 wrappers
            struct num_i32_clz_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_unop<Opt, numeric_details::int_unop::clz, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_unop<Opt, numeric_details::int_unop::clz, Type...>; }
            };

            struct num_i32_ctz_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_unop<Opt, numeric_details::int_unop::ctz, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_unop<Opt, numeric_details::int_unop::ctz, Type...>; }
            };

            struct num_i32_popcnt_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_unop<Opt, numeric_details::int_unop::popcnt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_unop<Opt, numeric_details::int_unop::popcnt, Type...>; }
            };

            struct num_i32_add_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::add, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::add, Type...>; }
            };

            struct num_i32_sub_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::sub, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::sub, Type...>; }
            };

            struct num_i32_mul_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::mul, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::mul, Type...>; }
            };

            struct num_i32_div_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_div_s<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_div_s<Opt, Type...>; }
            };

            struct num_i32_div_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::div_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::div_u, Type...>; }
            };

            struct num_i32_rem_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::rem_s, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::rem_s, Type...>; }
            };

            struct num_i32_rem_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::rem_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::rem_u, Type...>; }
            };

            struct num_i32_and_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::and_, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::and_, Type...>; }
            };

            struct num_i32_or_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::or_, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::or_, Type...>; }
            };

            struct num_i32_xor_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::xor_, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::xor_, Type...>; }
            };

            struct num_i32_shl_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::shl, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::shl, Type...>; }
            };

            struct num_i32_shr_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::shr_s, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::shr_s, Type...>; }
            };

            struct num_i32_shr_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::shr_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::shr_u, Type...>; }
            };

            struct num_i32_rotl_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::rotl, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::rotl, Type...>; }
            };

            struct num_i32_rotr_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::rotr, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i32_binop<Opt, numeric_details::int_binop::rotr, Type...>; }
            };

            // i64 wrappers
            struct num_i64_clz_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_unop<Opt, numeric_details::int_unop::clz, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_unop<Opt, numeric_details::int_unop::clz, Type...>; }
            };

            struct num_i64_ctz_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_unop<Opt, numeric_details::int_unop::ctz, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_unop<Opt, numeric_details::int_unop::ctz, Type...>; }
            };

            struct num_i64_popcnt_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_unop<Opt, numeric_details::int_unop::popcnt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_unop<Opt, numeric_details::int_unop::popcnt, Type...>; }
            };

            struct num_i64_add_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::add, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::add, Type...>; }
            };

            struct num_i64_sub_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::sub, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::sub, Type...>; }
            };

            struct num_i64_mul_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::mul, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::mul, Type...>; }
            };

            struct num_i64_div_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_div_s<Opt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_div_s<Opt, Type...>; }
            };

            struct num_i64_div_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::div_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::div_u, Type...>; }
            };

            struct num_i64_rem_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::rem_s, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::rem_s, Type...>; }
            };

            struct num_i64_rem_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::rem_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::rem_u, Type...>; }
            };

            struct num_i64_and_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::and_, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::and_, Type...>; }
            };

            struct num_i64_or_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::or_, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::or_, Type...>; }
            };

            struct num_i64_xor_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::xor_, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::xor_, Type...>; }
            };

            struct num_i64_shl_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::shl, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::shl, Type...>; }
            };

            struct num_i64_shr_s_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::shr_s, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::shr_s, Type...>; }
            };

            struct num_i64_shr_u_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::shr_u, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::shr_u, Type...>; }
            };

            struct num_i64_rotl_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::rotl, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::rotl, Type...>; }
            };

            struct num_i64_rotr_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::rotr, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_i64_binop<Opt, numeric_details::int_binop::rotr, Type...>; }
            };

            // f32 wrappers
            struct num_f32_abs_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_unop<Opt, numeric_details::float_unop::abs, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_unop<Opt, numeric_details::float_unop::abs, Type...>; }
            };

            struct num_f32_neg_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_unop<Opt, numeric_details::float_unop::neg, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_unop<Opt, numeric_details::float_unop::neg, Type...>; }
            };

            struct num_f32_ceil_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_unop<Opt, numeric_details::float_unop::ceil, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_unop<Opt, numeric_details::float_unop::ceil, Type...>; }
            };

            struct num_f32_floor_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_unop<Opt, numeric_details::float_unop::floor, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_unop<Opt, numeric_details::float_unop::floor, Type...>; }
            };

            struct num_f32_trunc_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_unop<Opt, numeric_details::float_unop::trunc, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_unop<Opt, numeric_details::float_unop::trunc, Type...>; }
            };

            struct num_f32_nearest_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_unop<Opt, numeric_details::float_unop::nearest, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_unop<Opt, numeric_details::float_unop::nearest, Type...>; }
            };

            struct num_f32_sqrt_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_unop<Opt, numeric_details::float_unop::sqrt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_unop<Opt, numeric_details::float_unop::sqrt, Type...>; }
            };

            struct num_f32_add_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop<Opt, numeric_details::float_binop::add, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop<Opt, numeric_details::float_binop::add, Type...>; }
            };

            struct num_f32_sub_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop<Opt, numeric_details::float_binop::sub, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop<Opt, numeric_details::float_binop::sub, Type...>; }
            };

            struct num_f32_mul_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop<Opt, numeric_details::float_binop::mul, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop<Opt, numeric_details::float_binop::mul, Type...>; }
            };

            struct num_f32_div_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop<Opt, numeric_details::float_binop::div, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop<Opt, numeric_details::float_binop::div, Type...>; }
            };

            struct num_f32_min_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop<Opt, numeric_details::float_binop::min, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop<Opt, numeric_details::float_binop::min, Type...>; }
            };

            struct num_f32_max_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop<Opt, numeric_details::float_binop::max, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop<Opt, numeric_details::float_binop::max, Type...>; }
            };

            struct num_f32_copysign_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f32_binop<Opt, numeric_details::float_binop::copysign, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f32_binop<Opt, numeric_details::float_binop::copysign, Type...>; }
            };

            // f64 wrappers
            struct num_f64_abs_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_unop<Opt, numeric_details::float_unop::abs, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_unop<Opt, numeric_details::float_unop::abs, Type...>; }
            };

            struct num_f64_neg_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_unop<Opt, numeric_details::float_unop::neg, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_unop<Opt, numeric_details::float_unop::neg, Type...>; }
            };

            struct num_f64_ceil_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_unop<Opt, numeric_details::float_unop::ceil, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_unop<Opt, numeric_details::float_unop::ceil, Type...>; }
            };

            struct num_f64_floor_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_unop<Opt, numeric_details::float_unop::floor, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_unop<Opt, numeric_details::float_unop::floor, Type...>; }
            };

            struct num_f64_trunc_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_unop<Opt, numeric_details::float_unop::trunc, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_unop<Opt, numeric_details::float_unop::trunc, Type...>; }
            };

            struct num_f64_nearest_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_unop<Opt, numeric_details::float_unop::nearest, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_unop<Opt, numeric_details::float_unop::nearest, Type...>; }
            };

            struct num_f64_sqrt_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_unop<Opt, numeric_details::float_unop::sqrt, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_unop<Opt, numeric_details::float_unop::sqrt, Type...>; }
            };

            struct num_f64_add_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop<Opt, numeric_details::float_binop::add, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop<Opt, numeric_details::float_binop::add, Type...>; }
            };

            struct num_f64_sub_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop<Opt, numeric_details::float_binop::sub, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop<Opt, numeric_details::float_binop::sub, Type...>; }
            };

            struct num_f64_mul_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop<Opt, numeric_details::float_binop::mul, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop<Opt, numeric_details::float_binop::mul, Type...>; }
            };

            struct num_f64_div_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop<Opt, numeric_details::float_binop::div, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop<Opt, numeric_details::float_binop::div, Type...>; }
            };

            struct num_f64_min_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop<Opt, numeric_details::float_binop::min, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop<Opt, numeric_details::float_binop::min, Type...>; }
            };

            struct num_f64_max_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop<Opt, numeric_details::float_binop::max, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop<Opt, numeric_details::float_binop::max, Type...>; }
            };

            struct num_f64_copysign_op
            {
                template <uwvm_interpreter_translate_option_t Opt, ::std::size_t Pos, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_t<Type...> fptr() noexcept
                { return uwvmint_f64_binop<Opt, numeric_details::float_binop::copysign, Pos, Type...>; }

                template <uwvm_interpreter_translate_option_t Opt, uwvm_int_stack_top_type... Type>
                inline static constexpr uwvm_interpreter_opfunc_byref_t<Type...> fptr_byref() noexcept
                { return uwvmint_f64_binop<Opt, numeric_details::float_binop::copysign, Type...>; }
            };

            template <uwvm_interpreter_translate_option_t CompileOption,
                      ::std::size_t BeginPos,
                      ::std::size_t EndPos,
                      typename OpWrapper,
                      uwvm_int_stack_top_type... Type>
                requires (CompileOption.is_tail_call)
            inline constexpr uwvm_interpreter_opfunc_t<Type...> get_numeric_fptr(::std::size_t pos) noexcept
            {
                if constexpr(BeginPos != EndPos)
                {
                    return select_stacktop_fptr_by_currpos_impl_numeric<CompileOption, BeginPos, EndPos, OpWrapper, Type...>(pos);
                }
                else
                {
                    return OpWrapper::template fptr<CompileOption, 0uz, Type...>();
                }
            }
        }  // namespace details

        // i32
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_clz_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_clz_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_clz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_clz_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_clz_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_clz<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_clz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_clz_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_ctz_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_ctz_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ctz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ctz_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_ctz_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_ctz<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_ctz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_ctz_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_popcnt_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::get_numeric_fptr<CompileOption,
                                             CompileOption.i32_stack_top_begin_pos,
                                             CompileOption.i32_stack_top_end_pos,
                                             details::num_i32_popcnt_op,
                                             Type...>(curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_popcnt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_popcnt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_popcnt_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_popcnt<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_popcnt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_popcnt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_add_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_add_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_add_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_add<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_add_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_add_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_sub_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_sub_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_sub_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_sub_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_sub_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_sub<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_sub_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_sub_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_mul_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_mul_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_mul_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_mul_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_mul_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_mul<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_mul_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_mul_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_div_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_div_s_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_div_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_div_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_div_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_div_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_div_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_div_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_div_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_div_u_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_div_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_div_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_div_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_div_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_div_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_div_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_rem_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_rem_s_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rem_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rem_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_rem_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_rem_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rem_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rem_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_rem_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_rem_u_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rem_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rem_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_rem_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_rem_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rem_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rem_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_and_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_and_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_and_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_and_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_and_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_and<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_and_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_and_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_or_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_or_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_or_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_or_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_or_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_or<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_or_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_or_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_xor_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_xor_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_xor_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_xor_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_xor_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_xor<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_xor_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_xor_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_shl_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_shl_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_shl_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_shl_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_shl_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_shl<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_shl_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_shl_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_shr_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_shr_s_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_shr_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_shr_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_shr_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_shr_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_shr_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_shr_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_shr_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_shr_u_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_shr_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_shr_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_shr_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_shr_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_shr_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_shr_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_rotl_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_rotl_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rotl_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rotl_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_rotl_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_rotl<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rotl_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rotl_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i32_rotr_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i32_stack_top_begin_pos, CompileOption.i32_stack_top_end_pos, details::num_i32_rotr_op, Type...>(
                    curr_stacktop.i32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rotr_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rotr_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i32_rotr_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i32_rotr<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i32_rotr_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i32_rotr_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        // i64
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_clz_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_clz_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_clz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_clz_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_clz_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_clz<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_clz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_clz_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_ctz_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_ctz_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_ctz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_ctz_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_ctz_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_ctz<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_ctz_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_ctz_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_popcnt_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::get_numeric_fptr<CompileOption,
                                             CompileOption.i64_stack_top_begin_pos,
                                             CompileOption.i64_stack_top_end_pos,
                                             details::num_i64_popcnt_op,
                                             Type...>(curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_popcnt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_popcnt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_popcnt_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_popcnt<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_popcnt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                     ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_popcnt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_add_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_add_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_add_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_add_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_add_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_add<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_add_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_add_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_sub_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_sub_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_sub_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_sub_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_sub_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_sub<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_sub_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_sub_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_mul_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_mul_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_mul_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_mul_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_mul_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_mul<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_mul_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_mul_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_div_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_div_s_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_div_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_div_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_div_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_div_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_div_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_div_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_div_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_div_u_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_div_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_div_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_div_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_div_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_div_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_div_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_rem_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_rem_s_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_rem_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_rem_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_rem_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_rem_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_rem_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_rem_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_rem_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_rem_u_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_rem_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_rem_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_rem_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_rem_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_rem_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_rem_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_and_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_and_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_and_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_and_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_and_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_and<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_and_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_and_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_or_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_or_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_or_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_or_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_or_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_or<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_or_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                 ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_or_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_xor_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_xor_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_xor_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_xor_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_xor_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_xor<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_xor_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_xor_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_shl_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_shl_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_shl_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_shl_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_shl_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_shl<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_shl_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_shl_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_shr_s_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_shr_s_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_shr_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_shr_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_shr_s_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_shr_s<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_shr_s_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_shr_s_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_shr_u_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_shr_u_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_shr_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_shr_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_shr_u_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_shr_u<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_shr_u_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_shr_u_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_rotl_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_rotl_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_rotl_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_rotl_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_rotl_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_rotl<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_rotl_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_rotl_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_i64_rotr_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.i64_stack_top_begin_pos, CompileOption.i64_stack_top_end_pos, details::num_i64_rotr_op, Type...>(
                    curr_stacktop.i64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_rotr_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_rotr_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_i64_rotr_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_i64_rotr<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_i64_rotr_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_i64_rotr_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        // f32
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_abs_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos, details::num_f32_abs_op, Type...>(
                    curr_stacktop.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_abs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_abs_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_abs_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_abs<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_abs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_abs_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_neg_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos, details::num_f32_neg_op, Type...>(
                    curr_stacktop.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_neg_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_neg_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_neg_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_neg<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_neg_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_neg_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_ceil_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos, details::num_f32_ceil_op, Type...>(
                    curr_stacktop.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_ceil_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_ceil_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_ceil_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_ceil<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_ceil_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_ceil_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_floor_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos, details::num_f32_floor_op, Type...>(
                    curr_stacktop.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_floor_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_floor_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_floor_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_floor<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_floor_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_floor_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_trunc_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos, details::num_f32_trunc_op, Type...>(
                    curr_stacktop.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_trunc_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_trunc_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_trunc_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_trunc<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_trunc_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_trunc_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_nearest_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::get_numeric_fptr<CompileOption,
                                             CompileOption.f32_stack_top_begin_pos,
                                             CompileOption.f32_stack_top_end_pos,
                                             details::num_f32_nearest_op,
                                             Type...>(curr_stacktop.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_nearest_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_nearest_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_nearest_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_nearest<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_nearest_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_nearest_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_sqrt_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos, details::num_f32_sqrt_op, Type...>(
                    curr_stacktop.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_sqrt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_sqrt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_sqrt_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_sqrt<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_sqrt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_sqrt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_add_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos, details::num_f32_add_op, Type...>(
                    curr_stacktop.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_add_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_add_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_add_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_add<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_add_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_add_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_sub_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos, details::num_f32_sub_op, Type...>(
                    curr_stacktop.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_sub_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_sub_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_sub_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_sub<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_sub_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_sub_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_mul_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos, details::num_f32_mul_op, Type...>(
                    curr_stacktop.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_mul_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_mul<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_mul_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_mul_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_div_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos, details::num_f32_div_op, Type...>(
                    curr_stacktop.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_div_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_div_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_div_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_div<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_div_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_div_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_min_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos, details::num_f32_min_op, Type...>(
                    curr_stacktop.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_min_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_min_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_min_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_min<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_min_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_min_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_max_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f32_stack_top_begin_pos, CompileOption.f32_stack_top_end_pos, details::num_f32_max_op, Type...>(
                    curr_stacktop.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_max_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_max_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_max_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_max<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_max_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_max_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f32_copysign_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::get_numeric_fptr<CompileOption,
                                             CompileOption.f32_stack_top_begin_pos,
                                             CompileOption.f32_stack_top_end_pos,
                                             details::num_f32_copysign_op,
                                             Type...>(curr_stacktop.f32_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_copysign_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_copysign_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f32_copysign_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f32_copysign<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f32_copysign_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f32_copysign_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        // f64
        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_abs_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos, details::num_f64_abs_op, Type...>(
                    curr_stacktop.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_abs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_abs_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_abs_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_abs<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_abs_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_abs_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_neg_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos, details::num_f64_neg_op, Type...>(
                    curr_stacktop.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_neg_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_neg_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_neg_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_neg<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_neg_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_neg_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_ceil_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos, details::num_f64_ceil_op, Type...>(
                    curr_stacktop.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_ceil_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_ceil_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_ceil_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_ceil<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_ceil_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_ceil_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_floor_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos, details::num_f64_floor_op, Type...>(
                    curr_stacktop.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_floor_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_floor_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_floor_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_floor<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_floor_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_floor_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_trunc_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos, details::num_f64_trunc_op, Type...>(
                    curr_stacktop.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_trunc_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_trunc_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_trunc_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_trunc<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_trunc_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                    ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_trunc_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_nearest_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::get_numeric_fptr<CompileOption,
                                             CompileOption.f64_stack_top_begin_pos,
                                             CompileOption.f64_stack_top_end_pos,
                                             details::num_f64_nearest_op,
                                             Type...>(curr_stacktop.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_nearest_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_nearest_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_nearest_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_nearest<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_nearest_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                      ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_nearest_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_sqrt_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos, details::num_f64_sqrt_op, Type...>(
                    curr_stacktop.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_sqrt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_sqrt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_sqrt_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_sqrt<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_sqrt_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                   ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_sqrt_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_add_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos, details::num_f64_add_op, Type...>(
                    curr_stacktop.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_add_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_add_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_add_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_add<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_add_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_add_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_sub_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos, details::num_f64_sub_op, Type...>(
                    curr_stacktop.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_sub_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_sub_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_sub_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_sub<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_sub_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_sub_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_mul_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos, details::num_f64_mul_op, Type...>(
                    curr_stacktop.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_mul_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_mul<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_mul_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_mul_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_div_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos, details::num_f64_div_op, Type...>(
                    curr_stacktop.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_div_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_div_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_div_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_div<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_div_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_div_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_min_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos, details::num_f64_min_op, Type...>(
                    curr_stacktop.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_min_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_min_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_min_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_min<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_min_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_min_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_max_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::
                get_numeric_fptr<CompileOption, CompileOption.f64_stack_top_begin_pos, CompileOption.f64_stack_top_end_pos, details::num_f64_max_op, Type...>(
                    curr_stacktop.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_max_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_max_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_max_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_max<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_max_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                  ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_max_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_t<Type...> get_uwvmint_f64_copysign_fptr(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop) noexcept
        {
            return details::get_numeric_fptr<CompileOption,
                                             CompileOption.f64_stack_top_begin_pos,
                                             CompileOption.f64_stack_top_end_pos,
                                             details::num_f64_copysign_op,
                                             Type...>(curr_stacktop.f64_stack_top_curr_pos);
        }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_copysign_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_copysign_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... Type>
            requires (!CompileOption.is_tail_call)
        inline constexpr uwvm_interpreter_opfunc_byref_t<Type...> get_uwvmint_f64_copysign_fptr(uwvm_interpreter_stacktop_currpos_t const&) noexcept
        { return uwvmint_f64_copysign<CompileOption, Type...>; }

        template <uwvm_interpreter_translate_option_t CompileOption, uwvm_int_stack_top_type... TypeInTuple>
            requires (!CompileOption.is_tail_call)
        inline constexpr auto get_uwvmint_f64_copysign_fptr_from_tuple(uwvm_interpreter_stacktop_currpos_t const& curr_stacktop,
                                                                       ::uwvm2::utils::container::tuple<TypeInTuple...> const&) noexcept
        { return get_uwvmint_f64_copysign_fptr<CompileOption, TypeInTuple...>(curr_stacktop); }
    }  // namespace translate
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
