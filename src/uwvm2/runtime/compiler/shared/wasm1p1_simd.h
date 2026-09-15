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
# include <bit>
# include <cmath>
# include <concepts>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
# include <type_traits>
# include <uwvm2/runtime/compiler/shared/strict_float.h>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/impl.h>
# include <uwvm2/object/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::shared
{
    namespace wasm1p1_details
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
        using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        using wasm_v128 = ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128;
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        template <typename WasmI32>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr WasmI32 from_u32_bits(::std::uint_least32_t u) noexcept
        {
            ::std::int_least32_t const i{::std::bit_cast<::std::int_least32_t>(u)};
            return static_cast<WasmI32>(i);
        }

        template <typename WasmI64>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr WasmI64 from_u64_bits(::std::uint_least64_t u) noexcept
        {
            ::std::int_least64_t const i{::std::bit_cast<::std::int_least64_t>(u)};
            return static_cast<WasmI64>(i);
        }

        template <typename WasmOutT, typename FloatT>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr WasmOutT trunc_sat_signed(FloatT x) noexcept
        {
            static_assert(::std::same_as<WasmOutT, wasm_i32> || ::std::same_as<WasmOutT, wasm_i64>);
            using int_out_t = ::std::conditional_t<::std::same_as<WasmOutT, wasm_i32>, ::std::int_least32_t, ::std::int_least64_t>;

            if(x != x) [[unlikely]] { return WasmOutT{}; }

            constexpr FloatT min_v{static_cast<FloatT>(::std::numeric_limits<int_out_t>::min())};
            constexpr FloatT max_plus_one{static_cast<FloatT>(static_cast<long double>(::std::numeric_limits<int_out_t>::max()) + 1.0L)};

            if(x <= min_v) [[unlikely]] { return static_cast<WasmOutT>((::std::numeric_limits<int_out_t>::min)()); }
            if(x >= max_plus_one) [[unlikely]] { return static_cast<WasmOutT>((::std::numeric_limits<int_out_t>::max)()); }
            return static_cast<WasmOutT>(static_cast<int_out_t>(x));
        }

        template <typename WasmOutT, typename FloatT>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr WasmOutT trunc_sat_unsigned(FloatT x) noexcept
        {
            static_assert(::std::same_as<WasmOutT, wasm_i32> || ::std::same_as<WasmOutT, wasm_i64>);
            using uint_out_t = ::std::conditional_t<::std::same_as<WasmOutT, wasm_i32>, ::std::uint_least32_t, ::std::uint_least64_t>;

            if(x != x || x <= static_cast<FloatT>(0)) [[unlikely]] { return WasmOutT{}; }

            constexpr FloatT max_plus_one{static_cast<FloatT>(static_cast<long double>((::std::numeric_limits<uint_out_t>::max)()) + 1.0L)};
            uint_out_t out{};
            if(x >= max_plus_one) [[unlikely]] { out = (::std::numeric_limits<uint_out_t>::max)(); }
            else { out = static_cast<uint_out_t>(x); }

            if constexpr(::std::same_as<WasmOutT, wasm_i32>)
            {
                return from_u32_bits<WasmOutT>(static_cast<::std::uint_least32_t>(out));
            }
            else if constexpr(::std::same_as<WasmOutT, wasm_i64>)
            {
                return from_u64_bits<WasmOutT>(static_cast<::std::uint_least64_t>(out));
            }
            else
            {
                static_assert(sizeof(WasmOutT) == 0, "unhandled trunc-sat output type");
            }
        }

        template <typename WasmOutT, bool Signed, typename FloatT>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr WasmOutT trunc_sat(FloatT x) noexcept
        {
            if constexpr(Signed) { return trunc_sat_signed<WasmOutT>(x); }
            else { return trunc_sat_unsigned<WasmOutT>(x); }
        }
    }  // namespace wasm1p1_details

    namespace wasm1p1_simd_details
    {
        // Keep unhandled-opcode assertions dependent without using a self-comparison rejected by GCC's
        // `-Wtautological-compare` under `-Werror`.
        template <auto Value>
        inline constexpr bool dependent_false_v{false};

        using wasm_i32 = wasm1p1_details::wasm_i32;
        using wasm_i64 = wasm1p1_details::wasm_i64;
        using wasm_f32 = wasm1p1_details::wasm_f32;
        using wasm_f64 = wasm1p1_details::wasm_f64;
        using wasm_u32 = wasm1p1_details::wasm_u32;
        using wasm_v128 = wasm1p1_details::wasm_v128;
        using native_memory_t = wasm1p1_details::native_memory_t;

        using u8 = ::std::uint8_t;
        using u16 = ::std::uint16_t;
        using u32 = ::std::uint32_t;
        using u64 = ::std::uint64_t;
        using s8 = ::std::int8_t;
        using s16 = ::std::int16_t;
        using s32 = ::std::int32_t;
        using s64 = ::std::int64_t;

        static_assert(sizeof(wasm_i32) == 4uz);
        static_assert(sizeof(wasm_i64) == 8uz);
        static_assert(sizeof(wasm_u32) == 4uz);
        static_assert(sizeof(wasm_f32) == 4uz);
        static_assert(sizeof(wasm_f64) == 8uz);
        static_assert(sizeof(wasm_v128) == 16uz);
        static_assert(sizeof(u8) == 1uz);
        static_assert(sizeof(u16) == 2uz);
        static_assert(sizeof(u32) == 4uz);
        static_assert(sizeof(u64) == 8uz);

        template <typename LaneT, ::std::size_t N>
        struct lane_array
        {
            LaneT lane[N];
        };

        enum class v128_unop
        {
            not_,
            f32x4_convert_i32x4_s,
            f32x4_convert_i32x4_u
        };

        enum class v128_binop
        {
            and_,
            andnot,
            or_,
            xor_,
            i32x4_add,
            i32x4_sub,
            i32x4_mul,
            i32x4_eq,
            f32x4_add,
            f32x4_sub,
            f32x4_mul,
            f32x4_eq
        };

        enum class v128_testop
        {
            any_true,
            i32x4_all_true
        };

        enum class v128_splatop
        {
            i32x4,
            f32x4
        };

        template <typename UIntT, ::std::size_t N>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr lane_array<UIntT, N> load_uint_lanes(wasm_v128 v) noexcept
        {
            static_assert(sizeof(UIntT) * N == sizeof(wasm_v128));
            lane_array<UIntT, N> out{};              // init
            ::std::byte bytes[sizeof(wasm_v128)]{};  // init
            ::std::memcpy(bytes, ::std::addressof(v), sizeof(bytes));
            for(::std::size_t i{}; i != N; ++i)
            {
                UIntT lane{};  // init
                ::std::memcpy(::std::addressof(lane), bytes + i * sizeof(UIntT), sizeof(UIntT));
                out.lane[i] = ::fast_io::little_endian(lane);
            }
            return out;
        }

        template <typename UIntT, ::std::size_t N>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 store_uint_lanes(lane_array<UIntT, N> const& lanes) noexcept
        {
            static_assert(sizeof(UIntT) * N == sizeof(wasm_v128));
            ::std::byte bytes[sizeof(wasm_v128)]{};  // init
            for(::std::size_t i{}; i != N; ++i)
            {
                auto lane{::fast_io::little_endian(lanes.lane[i])};
                ::std::memcpy(bytes + i * sizeof(UIntT), ::std::addressof(lane), sizeof(UIntT));
            }

            wasm_v128 out;  // no init
            ::std::memcpy(::std::addressof(out), bytes, sizeof(out));
            return out;
        }

        template <typename UInt, ::std::size_t N, bool Force = false>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 canonicalize_native_float_lanes(wasm_v128 v) noexcept
        {
            if constexpr(strict_float::needs_nan_canonicalization || Force)
            {
                constexpr UInt magnitude{static_cast<UInt>(sizeof(UInt) == 4 ? 0x7fffffffull : 0x7fffffffffffffffull)};
                constexpr UInt infinity{static_cast<UInt>(sizeof(UInt) == 4 ? 0x7f800000ull : 0x7ff0000000000000ull)};
                constexpr UInt nan{static_cast<UInt>(sizeof(UInt) == 4 ? 0x7fc00000ull : 0x7ff8000000000000ull)};
                auto lanes{load_uint_lanes<UInt, N>(v)};
                for(auto& lane: lanes.lane) { if((lane & magnitude) > infinity) { lane = nan; } }
                return store_uint_lanes<UInt, N>(lanes);
            }
            else { return v; }
        }

# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
        using v128_u8x16 [[__gnu__::__vector_size__(16)]] = u8;
        using v128_i8x16 [[__gnu__::__vector_size__(16)]] = s8;
        using v128_c8x16 [[__gnu__::__vector_size__(16)]] = char;
        using v128_u16x8 [[__gnu__::__vector_size__(16)]] = u16;
        using v128_i16x8 [[__gnu__::__vector_size__(16)]] = s16;
        using v128_u32x4 [[__gnu__::__vector_size__(16)]] = u32;
        using v128_i32x4 [[__gnu__::__vector_size__(16)]] = s32;
        using v128_u64x2 [[__gnu__::__vector_size__(16)]] = u64;
        using v128_i64x2 [[__gnu__::__vector_size__(16)]] = s64;
        using v128_f32x4 [[__gnu__::__vector_size__(16)]] = wasm_f32;
        using v128_f64x2 [[__gnu__::__vector_size__(16)]] = wasm_f64;
        using v64_u8x8 [[__gnu__::__vector_size__(8)]] = u8;
        using v64_i8x8 [[__gnu__::__vector_size__(8)]] = s8;
        using v64_u16x4 [[__gnu__::__vector_size__(8)]] = u16;
        using v64_i16x4 [[__gnu__::__vector_size__(8)]] = s16;
        using v64_u32x2 [[__gnu__::__vector_size__(8)]] = u32;
        using v64_i32x2 [[__gnu__::__vector_size__(8)]] = s32;

        static_assert(sizeof(v128_u8x16) == sizeof(wasm_v128));
        static_assert(sizeof(v128_i8x16) == sizeof(wasm_v128));
        static_assert(sizeof(v128_c8x16) == sizeof(wasm_v128));
        static_assert(sizeof(v128_u16x8) == sizeof(wasm_v128));
        static_assert(sizeof(v128_i16x8) == sizeof(wasm_v128));
        static_assert(sizeof(v128_u32x4) == sizeof(wasm_v128));
        static_assert(sizeof(v128_i32x4) == sizeof(wasm_v128));
        static_assert(sizeof(v128_u64x2) == sizeof(wasm_v128));
        static_assert(sizeof(v128_i64x2) == sizeof(wasm_v128));
        static_assert(sizeof(v128_f32x4) == sizeof(wasm_v128));
        static_assert(sizeof(v128_f64x2) == sizeof(wasm_v128));
        static_assert(sizeof(v64_u8x8) == 8uz);
        static_assert(sizeof(v64_i8x8) == 8uz);
        static_assert(sizeof(v64_u16x4) == 8uz);
        static_assert(sizeof(v64_i16x4) == 8uz);
        static_assert(sizeof(v64_u32x2) == 8uz);
        static_assert(sizeof(v64_i32x2) == 8uz);

        template <typename Vec>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr Vec v128_to_vec(wasm_v128 v) noexcept
        {
            static_assert(sizeof(Vec) == sizeof(wasm_v128));
            return ::std::bit_cast<Vec>(v);
        }

        template <typename Vec>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 vec_to_v128(Vec v) noexcept
        {
            static_assert(sizeof(Vec) == sizeof(wasm_v128));
            return ::std::bit_cast<wasm_v128>(v);
        }

        template <typename Vec, typename Lane>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr Vec vec_splat(Lane v) noexcept
        { return Vec{} + static_cast<Lane>(v); }

        template <typename UIntVec, typename MaskVec>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr UIntVec vec_select(UIntVec if_true, UIntVec if_false, MaskVec mask) noexcept
        {
            auto const m{::std::bit_cast<UIntVec>(mask)};
            return (if_true & m) | (if_false & ~m);
        }

        template <typename UIntVec, typename SignedVec>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr UIntVec vec_signed_abs_bits(UIntVec v) noexcept
        {
            auto const s{::std::bit_cast<SignedVec>(v)};
            return vec_select(static_cast<UIntVec>(UIntVec{} - v), v, s < SignedVec{});
        }

#  if UWVM_HAS_BUILTIN(__builtin_elementwise_abs)
        template <typename UIntVec, typename SignedVec>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 vec_abs_signed_v128(wasm_v128 v) noexcept
        {
            auto const in{::std::bit_cast<SignedVec>(v128_to_vec<UIntVec>(v))};
            auto const out{__builtin_elementwise_abs(in)};
            return vec_to_v128(::std::bit_cast<UIntVec>(out));
        }
#  endif

#  if UWVM_HAS_BUILTIN(__builtin_elementwise_add_sat)
        template <typename UIntVec, typename SignedVec>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 vec_add_sat_s_v128(wasm_v128 lhs, wasm_v128 rhs) noexcept
        {
            auto const l{::std::bit_cast<SignedVec>(v128_to_vec<UIntVec>(lhs))};
            auto const r{::std::bit_cast<SignedVec>(v128_to_vec<UIntVec>(rhs))};
            auto const out{__builtin_elementwise_add_sat(l, r)};
            return vec_to_v128(::std::bit_cast<UIntVec>(out));
        }

        template <typename UIntVec>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 vec_add_sat_u_v128(wasm_v128 lhs, wasm_v128 rhs) noexcept
        {
            auto const l{v128_to_vec<UIntVec>(lhs)};
            auto const r{v128_to_vec<UIntVec>(rhs)};
            return vec_to_v128(__builtin_elementwise_add_sat(l, r));
        }
#  endif

#  if UWVM_HAS_BUILTIN(__builtin_elementwise_sub_sat)
        template <typename UIntVec, typename SignedVec>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 vec_sub_sat_s_v128(wasm_v128 lhs, wasm_v128 rhs) noexcept
        {
            auto const l{::std::bit_cast<SignedVec>(v128_to_vec<UIntVec>(lhs))};
            auto const r{::std::bit_cast<SignedVec>(v128_to_vec<UIntVec>(rhs))};
            auto const out{__builtin_elementwise_sub_sat(l, r)};
            return vec_to_v128(::std::bit_cast<UIntVec>(out));
        }

        template <typename UIntVec>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 vec_sub_sat_u_v128(wasm_v128 lhs, wasm_v128 rhs) noexcept
        {
            auto const l{v128_to_vec<UIntVec>(lhs)};
            auto const r{v128_to_vec<UIntVec>(rhs)};
            return vec_to_v128(__builtin_elementwise_sub_sat(l, r));
        }
#  endif

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr bool vec_any_bit_set(wasm_v128 v) noexcept
        {
#  if defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz128)
            auto const bits{v128_to_vec<v128_i64x2>(v)};
            return !__builtin_ia32_ptestz128(bits, bits);
#  elif defined(__clang__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&                                 \
      UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32)
            return __builtin_neon_vmaxvq_u32(v128_to_vec<v128_u32x4>(v)) != 0u;
#  elif !defined(__clang__) && defined(__GNUC__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&           \
      UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu)
            return __builtin_aarch64_reduc_umax_scal_v4si_uu(v128_to_vec<v128_u32x4>(v)) != 0u;
#  else
            // LoongArch LSX __builtin_lsx_bnz_v is semantically valid here, but Clang lowers it to a conditional branch.
            // This helper returns a value, so the branchless 64-bit OR fallback is preferable.
            auto const bits{v128_to_vec<v128_u64x2>(v)};
            return (bits[0] | bits[1]) != 0u;
#  endif
        }

        template <typename MaskVec>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr bool vec_compare_mask_any_bit_set(MaskVec mask) noexcept
        {
#  if defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_ptestz128)
            auto const bits{::std::bit_cast<v128_i64x2>(mask)};
            return !__builtin_ia32_ptestz128(bits, bits);
#  elif defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
            return __builtin_ia32_pmovmskb128(::std::bit_cast<v128_c8x16>(mask)) != 0;
#  elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_all_true_i8x16)
            auto const bytes{::std::bit_cast<v128_u8x16>(mask)};
            return !__builtin_wasm_all_true_i8x16(::std::bit_cast<v128_i8x16>(~bytes));
#  elif defined(__wasm_simd128__) && UWVM_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)
            return __builtin_wasm_bitmask_i8x16(::std::bit_cast<v128_i8x16>(mask)) != 0;
#  elif defined(__clang__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&                                 \
      UWVM_HAS_BUILTIN(__builtin_neon_vmaxvq_u32)
            return __builtin_neon_vmaxvq_u32(::std::bit_cast<v128_u32x4>(mask)) != 0u;
#  elif !defined(__clang__) && defined(__GNUC__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&           \
      UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_umax_scal_v4si_uu)
            return __builtin_aarch64_reduc_umax_scal_v4si_uu(::std::bit_cast<v128_u32x4>(mask)) != 0u;
#  else
            // LoongArch LSX __builtin_lsx_bnz_v is a good fit for direct error branches in the parser, not for this value-returning helper.
            // Keep this path branchless unless a future LSX lowering produces a plain boolean value.
            auto const bits{::std::bit_cast<v128_u64x2>(mask)};
            return (bits[0] | bits[1]) != 0u;
#  endif
        }

        template <typename Vec>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr bool vec_all_lanes_nonzero(Vec v) noexcept
        { return !vec_compare_mask_any_bit_set(v == Vec{}); }
# endif

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr lane_array<wasm_f32, 4uz> load_f32x4_lanes(wasm_v128 v) noexcept
        {
            auto const bits{load_uint_lanes<wasm_u32, 4uz>(v)};
            lane_array<wasm_f32, 4uz> out{};  // init
            for(::std::size_t i{}; i != 4uz; ++i) { out.lane[i] = ::std::bit_cast<wasm_f32>(bits.lane[i]); }
            return out;
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 store_f32x4_lanes(lane_array<wasm_f32, 4uz> const& lanes) noexcept
        {
            lane_array<wasm_u32, 4uz> bits{};  // init
            for(::std::size_t i{}; i != 4uz; ++i) { bits.lane[i] = ::std::bit_cast<wasm_u32>(lanes.lane[i]); }
            return store_uint_lanes<wasm_u32, 4uz>(bits);
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 v128_bitwise_not(wasm_v128 v) noexcept
        {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
            auto const bits{v128_to_vec<v128_u8x16>(v)};
            return vec_to_v128(~bits);
# else
            auto bytes{load_uint_lanes<::std::uint_least8_t, 16uz>(v)};
            for(::std::size_t i{}; i != 16uz; ++i) { bytes.lane[i] = static_cast<::std::uint_least8_t>(~bytes.lane[i]); }
            return store_uint_lanes<::std::uint_least8_t, 16uz>(bytes);
# endif
        }

        template <v128_binop Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_v128_binop(wasm_v128 lhs, wasm_v128 rhs) noexcept
        {
            if constexpr(Op == v128_binop::and_ || Op == v128_binop::andnot || Op == v128_binop::or_ || Op == v128_binop::xor_)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                auto const l{v128_to_vec<v128_u8x16>(lhs)};
                auto const r{v128_to_vec<v128_u8x16>(rhs)};
                if constexpr(Op == v128_binop::and_) { return vec_to_v128(l & r); }
                else if constexpr(Op == v128_binop::andnot) { return vec_to_v128(l & ~r); }
                else if constexpr(Op == v128_binop::or_) { return vec_to_v128(l | r); }
                else if constexpr(Op == v128_binop::xor_) { return vec_to_v128(l ^ r); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled v128 bitwise binop");
                }
# else
                auto l{load_uint_lanes<::std::uint_least8_t, 16uz>(lhs)};
                auto const r{load_uint_lanes<::std::uint_least8_t, 16uz>(rhs)};
                for(::std::size_t i{}; i != 16uz; ++i)
                {
                    if constexpr(Op == v128_binop::and_) { l.lane[i] = static_cast<::std::uint_least8_t>(l.lane[i] & r.lane[i]); }
                    else if constexpr(Op == v128_binop::andnot) { l.lane[i] = static_cast<::std::uint_least8_t>(l.lane[i] & ~r.lane[i]); }
                    else if constexpr(Op == v128_binop::or_) { l.lane[i] = static_cast<::std::uint_least8_t>(l.lane[i] | r.lane[i]); }
                    else if constexpr(Op == v128_binop::xor_) { l.lane[i] = static_cast<::std::uint_least8_t>(l.lane[i] ^ r.lane[i]); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled v128 bitwise binop");
                    }
                }
                return store_uint_lanes<::std::uint_least8_t, 16uz>(l);
# endif
            }
            else if constexpr(Op == v128_binop::i32x4_add || Op == v128_binop::i32x4_sub || Op == v128_binop::i32x4_mul || Op == v128_binop::i32x4_eq)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                auto const l{v128_to_vec<v128_u32x4>(lhs)};
                auto const r{v128_to_vec<v128_u32x4>(rhs)};
                if constexpr(Op == v128_binop::i32x4_add) { return vec_to_v128(l + r); }
                else if constexpr(Op == v128_binop::i32x4_sub) { return vec_to_v128(l - r); }
                else if constexpr(Op == v128_binop::i32x4_mul) { return vec_to_v128(l * r); }
                else if constexpr(Op == v128_binop::i32x4_eq) { return vec_to_v128(::std::bit_cast<v128_u32x4>(l == r)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled v128 i32x4 binop");
                }
# else
                auto l{load_uint_lanes<wasm_u32, 4uz>(lhs)};
                auto const r{load_uint_lanes<wasm_u32, 4uz>(rhs)};
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    if constexpr(Op == v128_binop::i32x4_add) { l.lane[i] = static_cast<wasm_u32>(l.lane[i] + r.lane[i]); }
                    else if constexpr(Op == v128_binop::i32x4_sub) { l.lane[i] = static_cast<wasm_u32>(l.lane[i] - r.lane[i]); }
                    else if constexpr(Op == v128_binop::i32x4_mul) { l.lane[i] = static_cast<wasm_u32>(l.lane[i] * r.lane[i]); }
                    else if constexpr(Op == v128_binop::i32x4_eq) { l.lane[i] = l.lane[i] == r.lane[i] ? static_cast<wasm_u32>(0xFFFFFFFFu) : wasm_u32{}; }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled v128 i32x4 binop");
                    }
                }
                return store_uint_lanes<wasm_u32, 4uz>(l);
# endif
            }
            else if constexpr(Op == v128_binop::f32x4_add || Op == v128_binop::f32x4_sub || Op == v128_binop::f32x4_mul || Op == v128_binop::f32x4_eq)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && !UWVM2_STRICT_FLOAT_EXTENDED
                auto const l{v128_to_vec<v128_f32x4>(lhs)};
                auto const r{v128_to_vec<v128_f32x4>(rhs)};
                if constexpr(Op == v128_binop::f32x4_add) { return canonicalize_native_float_lanes<u32, 4uz>(vec_to_v128(l + r)); }
                else if constexpr(Op == v128_binop::f32x4_sub) { return canonicalize_native_float_lanes<u32, 4uz>(vec_to_v128(l - r)); }
                else if constexpr(Op == v128_binop::f32x4_mul) { return canonicalize_native_float_lanes<u32, 4uz>(vec_to_v128(l * r)); }
                else if constexpr(Op == v128_binop::f32x4_eq) { return vec_to_v128(::std::bit_cast<v128_u32x4>(l == r)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled v128 f32x4 binop");
                }
# else
                auto l{load_f32x4_lanes(lhs)};
                auto const r{load_f32x4_lanes(rhs)};
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    if constexpr(Op == v128_binop::f32x4_add) { l.lane[i] = strict_float::binary<strict_float::operation::add>(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == v128_binop::f32x4_sub) { l.lane[i] = strict_float::binary<strict_float::operation::sub>(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == v128_binop::f32x4_mul) { l.lane[i] = strict_float::binary<strict_float::operation::mul>(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == v128_binop::f32x4_eq)
                    {
                        lane_array<wasm_u32, 4uz> out{};  // init
                        for(::std::size_t j{}; j != 4uz; ++j) { out.lane[j] = l.lane[j] == r.lane[j] ? static_cast<wasm_u32>(0xFFFFFFFFu) : wasm_u32{}; }
                        return store_uint_lanes<wasm_u32, 4uz>(out);
                    }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled v128 f32x4 binop");
                    }
                }
                return store_f32x4_lanes(l);
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled v128 binop");
            }
        }

        template <v128_unop Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_v128_unop(wasm_v128 v) noexcept
        {
            if constexpr(Op == v128_unop::not_) { return v128_bitwise_not(v); }
            else if constexpr(Op == v128_unop::f32x4_convert_i32x4_s || Op == v128_unop::f32x4_convert_i32x4_u)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && UWVM_HAS_BUILTIN(__builtin_convertvector) && !UWVM2_STRICT_FLOAT_EXTENDED
                auto const lanes{v128_to_vec<v128_u32x4>(v)};
                if constexpr(Op == v128_unop::f32x4_convert_i32x4_s)
                {
                    return vec_to_v128(__builtin_convertvector(::std::bit_cast<v128_i32x4>(lanes), v128_f32x4));
                }
                else if constexpr(Op == v128_unop::f32x4_convert_i32x4_u) { return vec_to_v128(__builtin_convertvector(lanes, v128_f32x4)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled v128 unop");
                }
# else
                auto const lanes{load_uint_lanes<wasm_u32, 4uz>(v)};
                lane_array<wasm_f32, 4uz> out{};  // init
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    if constexpr(Op == v128_unop::f32x4_convert_i32x4_s) { out.lane[i] = strict_float::convert<wasm_f32>(::std::bit_cast<wasm_i32>(lanes.lane[i])); }
                    else if constexpr(Op == v128_unop::f32x4_convert_i32x4_u) { out.lane[i] = strict_float::convert<wasm_f32>(lanes.lane[i]); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled v128 unop");
                    }
                }
                return store_f32x4_lanes(out);
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled v128 unop");
            }
        }

        template <v128_testop Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_i32 eval_v128_testop(wasm_v128 v) noexcept
        {
            if constexpr(Op == v128_testop::any_true)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                return vec_any_bit_set(v) ? wasm_i32{1} : wasm_i32{};
# else
                auto const bytes{load_uint_lanes<::std::uint_least8_t, 16uz>(v)};
                for(::std::size_t i{}; i != 16uz; ++i)
                {
                    if(bytes.lane[i] != 0u) { return wasm_i32{1}; }
                }
                return wasm_i32{};
# endif
            }
            else if constexpr(Op == v128_testop::i32x4_all_true)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                return vec_all_lanes_nonzero(v128_to_vec<v128_u32x4>(v)) ? wasm_i32{1} : wasm_i32{};
# else
                auto const lanes{load_uint_lanes<wasm_u32, 4uz>(v)};
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    if(lanes.lane[i] == 0u) { return wasm_i32{}; }
                }
                return wasm_i32{1};
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled v128 testop");
            }
        }

        template <v128_splatop Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_v128_splat_i32(wasm_i32 v) noexcept
        {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
            return vec_to_v128(vec_splat<v128_u32x4>(::std::bit_cast<u32>(v)));
# else
            lane_array<wasm_u32, 4uz> lanes{};  // init
            auto const bits{::std::bit_cast<wasm_u32>(v)};
            for(::std::size_t i{}; i != 4uz; ++i) { lanes.lane[i] = bits; }
            return store_uint_lanes<wasm_u32, 4uz>(lanes);
# endif
        }

        template <v128_splatop Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_v128_splat_f32(wasm_f32 v) noexcept
        {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
            return vec_to_v128(vec_splat<v128_f32x4>(v));
# else
            lane_array<wasm_f32, 4uz> lanes{};  // init
            for(::std::size_t i{}; i != 4uz; ++i) { lanes.lane[i] = v; }
            return store_f32x4_lanes(lanes);
# endif
        }

        template <::std::size_t Lane>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_f32 eval_f32x4_extract_lane(wasm_v128 v) noexcept
        {
            static_assert(Lane < 4uz);
            return load_f32x4_lanes(v).lane[Lane];
        }

        UWVM_ALWAYS_INLINE inline constexpr void push_v128_to_memory_stack(wasm_v128 const& out, ::std::byte*& stack_top) noexcept
        {
            ::std::memcpy(stack_top, ::std::addressof(out), sizeof(out));
            stack_top += sizeof(out);
        }

        UWVM_ALWAYS_INLINE inline constexpr void push_i32_to_memory_stack(wasm_i32 const& out, ::std::byte*& stack_top) noexcept
        {
            ::std::memcpy(stack_top, ::std::addressof(out), sizeof(out));
            stack_top += sizeof(out);
        }

        UWVM_ALWAYS_INLINE inline constexpr void push_f32_to_memory_stack(wasm_f32 const& out, ::std::byte*& stack_top) noexcept
        {
            ::std::memcpy(stack_top, ::std::addressof(out), sizeof(out));
            stack_top += sizeof(out);
        }

        template <typename T>
        UWVM_ALWAYS_INLINE inline constexpr void push_to_memory_stack(T const& out, ::std::byte*& stack_top) noexcept
        {
            ::std::memcpy(stack_top, ::std::addressof(out), sizeof(out));
            stack_top += sizeof(out);
        }

        using simd_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_simd;

        template <typename U>
        struct signed_lane;

        template <>
        struct signed_lane<u8>
        {
            using type = s8;
        };

        template <>
        struct signed_lane<u16>
        {
            using type = s16;
        };

        template <>
        struct signed_lane<u32>
        {
            using type = s32;
        };

        template <>
        struct signed_lane<u64>
        {
            using type = s64;
        };

        template <typename U>
        using signed_lane_t = typename signed_lane<U>::type;

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr signed_lane_t<U> as_signed_lane(U v) noexcept
        { return ::std::bit_cast<signed_lane_t<U>>(v); }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U from_signed_lane(signed_lane_t<U> v) noexcept
        { return ::std::bit_cast<U>(v); }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U lane_true() noexcept
        { return static_cast<U>((::std::numeric_limits<U>::max)()); }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U lane_bool(bool v) noexcept
        { return v ? lane_true<U>() : U{}; }

        template <typename FloatT>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline FloatT wasm_roundeven(FloatT x) noexcept
        {
            if constexpr(::std::same_as<FloatT, wasm_f32>)
            {
                auto const bits{::std::bit_cast<u32>(x)};
                auto const sign{bits & u32{0x80000000u}};
                auto const exp{(bits >> 23u) & u32{0xffu}};
                if(exp == u32{0xffu})
                { return (bits & u32{0x007fffffu}) ? ::std::bit_cast<wasm_f32>(bits | u32{0x00400000u}) : x; }
                if(exp < u32{126u}) { return ::std::bit_cast<wasm_f32>(sign); }
                if(exp == u32{126u})
                {
                    if((bits & u32{0x007fffffu}) == 0u) { return ::std::bit_cast<wasm_f32>(sign); }
                    return ::std::bit_cast<wasm_f32>(sign | u32{0x3f800000u});
                }
                if(exp >= u32{150u}) { return x; }

                auto const shift{u32{150u} - exp};
                auto const half{u32{1u} << (shift - 1u)};
                auto const increment{half << 1u};
                auto const frac_mask{increment - 1u};
                auto const frac{bits & frac_mask};
                auto out{bits & ~frac_mask};
                if(frac > half || (frac == half && (out & increment) != 0u)) { out += increment; }
                return ::std::bit_cast<wasm_f32>(out);
            }
            else if constexpr(::std::same_as<FloatT, wasm_f64>)
            {
                auto const bits{::std::bit_cast<u64>(x)};
                auto const sign{bits & u64{0x8000000000000000ull}};
                auto const exp{(bits >> 52u) & u64{0x7ffu}};
                if(exp == u64{0x7ffu})
                { return (bits & u64{0x000fffffffffffffull}) ? ::std::bit_cast<wasm_f64>(bits | u64{0x0008000000000000ull}) : x; }
                if(exp < u64{1022u}) { return ::std::bit_cast<wasm_f64>(sign); }
                if(exp == u64{1022u})
                {
                    if((bits & u64{0x000fffffffffffffull}) == 0u) { return ::std::bit_cast<wasm_f64>(sign); }
                    return ::std::bit_cast<wasm_f64>(sign | u64{0x3ff0000000000000ull});
                }
                if(exp >= u64{1075u}) { return x; }

                auto const shift{u64{1075u} - exp};
                auto const half{u64{1u} << (shift - 1u)};
                auto const increment{half << 1u};
                auto const frac_mask{increment - 1u};
                auto const frac{bits & frac_mask};
                auto out{bits & ~frac_mask};
                if(frac > half || (frac == half && (out & increment) != 0u)) { out += increment; }
                return ::std::bit_cast<wasm_f64>(out);
            }
            else
            {
                static_assert(::std::same_as<FloatT, wasm_f32> || ::std::same_as<FloatT, wasm_f64>, "unsupported wasm_roundeven type");
            }
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr lane_array<wasm_f64, 2uz> load_f64x2_lanes(wasm_v128 v) noexcept
        {
            auto const bits{load_uint_lanes<u64, 2uz>(v)};
            lane_array<wasm_f64, 2uz> out{};  // init
            for(::std::size_t i{}; i != 2uz; ++i) { out.lane[i] = ::std::bit_cast<wasm_f64>(bits.lane[i]); }
            return out;
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 store_f64x2_lanes(lane_array<wasm_f64, 2uz> const& lanes) noexcept
        {
            lane_array<u64, 2uz> bits{};  // init
            for(::std::size_t i{}; i != 2uz; ++i) { bits.lane[i] = ::std::bit_cast<u64>(lanes.lane[i]); }
            return store_uint_lanes<u64, 2uz>(bits);
        }

# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
#  if (defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_roundps)) ||                                                                                     \
      (defined(__clang__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&                                  \
       UWVM_HAS_BUILTIN(__builtin_neon_vrndnq_v)) ||                                                                                                           \
      (!defined(__clang__) && defined(__GNUC__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&            \
       UWVM_HAS_BUILTIN(__builtin_aarch64_roundevenv4sf)) ||                                                                                                   \
      (defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vfrintrne_s))
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 vec_roundeven_f32x4_v128(wasm_v128 v) noexcept
        {
#   if defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_roundps)
            return vec_to_v128(__builtin_ia32_roundps(v128_to_vec<v128_f32x4>(v), 8));
#   elif defined(__clang__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&                                \
       UWVM_HAS_BUILTIN(__builtin_neon_vrndnq_v)
            return vec_to_v128(::std::bit_cast<v128_f32x4>(__builtin_neon_vrndnq_v(::std::bit_cast<v128_i8x16>(v128_to_vec<v128_f32x4>(v)), 41)));
#   elif !defined(__clang__) && defined(__GNUC__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&          \
       UWVM_HAS_BUILTIN(__builtin_aarch64_roundevenv4sf)
            return vec_to_v128(__builtin_aarch64_roundevenv4sf(v128_to_vec<v128_f32x4>(v)));
#   elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vfrintrne_s)
            return vec_to_v128(::std::bit_cast<v128_f32x4>(__builtin_lsx_vfrintrne_s(v128_to_vec<v128_f32x4>(v))));
#   endif
        }
#  endif

#  if (defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_roundpd)) ||                                                                                     \
      (defined(__clang__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&                                  \
       UWVM_HAS_BUILTIN(__builtin_neon_vrndnq_v)) ||                                                                                                           \
      (!defined(__clang__) && defined(__GNUC__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&            \
       UWVM_HAS_BUILTIN(__builtin_aarch64_roundevenv2df)) ||                                                                                                   \
      (defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vfrintrne_d))
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 vec_roundeven_f64x2_v128(wasm_v128 v) noexcept
        {
#   if defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_roundpd)
            return vec_to_v128(__builtin_ia32_roundpd(v128_to_vec<v128_f64x2>(v), 8));
#   elif defined(__clang__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&                                \
       UWVM_HAS_BUILTIN(__builtin_neon_vrndnq_v)
            return vec_to_v128(::std::bit_cast<v128_f64x2>(__builtin_neon_vrndnq_v(::std::bit_cast<v128_i8x16>(v128_to_vec<v128_f64x2>(v)), 42)));
#   elif !defined(__clang__) && defined(__GNUC__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&          \
       UWVM_HAS_BUILTIN(__builtin_aarch64_roundevenv2df)
            return vec_to_v128(__builtin_aarch64_roundevenv2df(v128_to_vec<v128_f64x2>(v)));
#   elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vfrintrne_d)
            return vec_to_v128(::std::bit_cast<v128_f64x2>(__builtin_lsx_vfrintrne_d(v128_to_vec<v128_f64x2>(v))));
#   endif
        }
#  endif
# endif

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U abs_signed_bits(U v) noexcept
        {
            auto const s{as_signed_lane<U>(v)};
            return s < 0 ? static_cast<U>(U{} - v) : v;
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U neg_bits(U v) noexcept
        { return static_cast<U>(U{} - v); }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U min_signed_bits(U a, U b) noexcept
        { return as_signed_lane<U>(a) < as_signed_lane<U>(b) ? a : b; }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U max_signed_bits(U a, U b) noexcept
        { return as_signed_lane<U>(a) > as_signed_lane<U>(b) ? a : b; }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U sat_add_unsigned(U a, U b) noexcept
        {
            U const out{static_cast<U>(a + b)};
            return out < a ? lane_true<U>() : out;
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U sat_sub_unsigned(U a, U b) noexcept
        { return a < b ? U{} : static_cast<U>(a - b); }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U sat_add_signed(U a, U b) noexcept
        {
            using S = signed_lane_t<U>;
            using Wide = ::std::conditional_t<(sizeof(U) <= 2uz), ::std::int_least32_t, ::std::int_least64_t>;
            auto const sum{static_cast<Wide>(as_signed_lane<U>(a)) + static_cast<Wide>(as_signed_lane<U>(b))};
            auto const lo{static_cast<Wide>((::std::numeric_limits<S>::min)())};
            auto const hi{static_cast<Wide>((::std::numeric_limits<S>::max)())};
            auto const clamped{sum < lo ? lo : (sum > hi ? hi : sum)};
            return from_signed_lane<U>(static_cast<S>(clamped));
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U sat_sub_signed(U a, U b) noexcept
        {
            using S = signed_lane_t<U>;
            using Wide = ::std::conditional_t<(sizeof(U) <= 2uz), ::std::int_least32_t, ::std::int_least64_t>;
            auto const diff{static_cast<Wide>(as_signed_lane<U>(a)) - static_cast<Wide>(as_signed_lane<U>(b))};
            auto const lo{static_cast<Wide>((::std::numeric_limits<S>::min)())};
            auto const hi{static_cast<Wide>((::std::numeric_limits<S>::max)())};
            auto const clamped{diff < lo ? lo : (diff > hi ? hi : diff)};
            return from_signed_lane<U>(static_cast<S>(clamped));
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr u16 q15mulr_sat_s(u16 a, u16 b) noexcept
        {
            auto const lhs{static_cast<::std::int_least32_t>(as_signed_lane<u16>(a))};
            auto const rhs{static_cast<::std::int_least32_t>(as_signed_lane<u16>(b))};
            auto out{(lhs * rhs + 0x4000) >> 15};
            if(out > static_cast<::std::int_least32_t>((::std::numeric_limits<s16>::max)()))
            {
                out = static_cast<::std::int_least32_t>((::std::numeric_limits<s16>::max)());
            }
            if(out < static_cast<::std::int_least32_t>((::std::numeric_limits<s16>::min)()))
            {
                out = static_cast<::std::int_least32_t>((::std::numeric_limits<s16>::min)());
            }
            return from_signed_lane<u16>(static_cast<s16>(out));
        }

        template <typename OutU, typename InU>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr OutU narrow_signed_lane(InU v) noexcept
        {
            using OutS = signed_lane_t<OutU>;
            using Wide = signed_lane_t<InU>;
            auto const x{static_cast<Wide>(as_signed_lane<InU>(v))};
            auto const lo{static_cast<Wide>((::std::numeric_limits<OutS>::min)())};
            auto const hi{static_cast<Wide>((::std::numeric_limits<OutS>::max)())};
            auto const clamped{x < lo ? lo : (x > hi ? hi : x)};
            return from_signed_lane<OutU>(static_cast<OutS>(clamped));
        }

        template <typename OutU, typename InU>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr OutU narrow_unsigned_lane(InU v) noexcept
        {
            using InS = signed_lane_t<InU>;
            auto const x{static_cast<InS>(as_signed_lane<InU>(v))};
            if(x <= 0) { return OutU{}; }
            auto const hi{static_cast<::std::uint_least64_t>((::std::numeric_limits<OutU>::max)())};
            auto const ux{static_cast<::std::uint_least64_t>(x)};
            return static_cast<OutU>(ux > hi ? hi : ux);
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U shl_lane(U v, wasm_i32 count) noexcept
        {
            constexpr unsigned bits{static_cast<unsigned>(sizeof(U) * 8uz)};
            auto const sh{static_cast<unsigned>(::std::bit_cast<u32>(count)) & (bits - 1u)};
            return static_cast<U>(v << sh);
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U shr_u_lane(U v, wasm_i32 count) noexcept
        {
            constexpr unsigned bits{static_cast<unsigned>(sizeof(U) * 8uz)};
            auto const sh{static_cast<unsigned>(::std::bit_cast<u32>(count)) & (bits - 1u)};
            return static_cast<U>(v >> sh);
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr U shr_s_lane(U v, wasm_i32 count) noexcept
        {
            constexpr unsigned bits{static_cast<unsigned>(sizeof(U) * 8uz)};
            auto const sh{static_cast<unsigned>(::std::bit_cast<u32>(count)) & (bits - 1u)};
            if(sh == 0u) { return v; }
            U const sign{static_cast<U>(U{1} << (bits - 1u))};
            if((v & sign) == 0u) { return static_cast<U>(v >> sh); }
            return static_cast<U>((v >> sh) | static_cast<U>(lane_true<U>() << (bits - sh)));
        }

        template <typename FloatT>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline FloatT wasm_float_min(FloatT a, FloatT b) noexcept
        {
            if(::std::isnan(a) || ::std::isnan(b)) { return strict_float::canonical_nan<FloatT>(); }
            if(a == b)
            {
                if(::std::signbit(a)) { return a; }
                return b;
            }
            return a < b ? a : b;
        }

        template <typename FloatT>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline FloatT wasm_float_max(FloatT a, FloatT b) noexcept
        {
            if(::std::isnan(a) || ::std::isnan(b)) { return strict_float::canonical_nan<FloatT>(); }
            if(a == b)
            {
                if(::std::signbit(a)) { return b; }
                return a;
            }
            return a > b ? a : b;
        }

        template <typename FloatT>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline FloatT wasm_float_pmin(FloatT a, FloatT b) noexcept
        { return b < a ? b : a; }

        template <typename FloatT>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline FloatT wasm_float_pmax(FloatT a, FloatT b) noexcept
        { return a < b ? b : a; }

        struct shuffle_controls
        {
            u8 lhs[16];
            u8 rhs[16];
        };

        static_assert(sizeof(shuffle_controls) == 32uz);

        template <typename LaneT>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr shuffle_controls make_shuffle_controls(LaneT const (&lanes_imm)[16]) noexcept
        {
            shuffle_controls out{};  // init
            for(::std::size_t i{}; i != 16uz; ++i)
            {
                auto const lane{static_cast<u8>(lanes_imm[i])};
                out.lhs[i] = lane < 16u ? lane : u8{0x80u};
                out.rhs[i] = lane >= 16u ? static_cast<u8>(lane - 16u) : u8{0x80u};
            }
            return out;
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_splat_i32(wasm_i32 v) noexcept
        {
            auto const bits{::std::bit_cast<u32>(v)};
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
            if constexpr(Op == simd_code::i8x16_splat) { return vec_to_v128(vec_splat<v128_u8x16>(static_cast<u8>(bits))); }
            else if constexpr(Op == simd_code::i16x8_splat) { return vec_to_v128(vec_splat<v128_u16x8>(static_cast<u16>(bits))); }
            else if constexpr(Op == simd_code::i32x4_splat) { return vec_to_v128(vec_splat<v128_u32x4>(bits)); }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled i32 splat opcode");
            }
# else
            if constexpr(Op == simd_code::i8x16_splat)
            {
                lane_array<u8, 16uz> lanes{};  // init
                for(auto& lane: lanes.lane) { lane = static_cast<u8>(bits); }
                return store_uint_lanes<u8, 16uz>(lanes);
            }
            else if constexpr(Op == simd_code::i16x8_splat)
            {
                lane_array<u16, 8uz> lanes{};  // init
                for(auto& lane: lanes.lane) { lane = static_cast<u16>(bits); }
                return store_uint_lanes<u16, 8uz>(lanes);
            }
            else if constexpr(Op == simd_code::i32x4_splat)
            {
                lane_array<u32, 4uz> lanes{};  // init
                for(auto& lane: lanes.lane) { lane = bits; }
                return store_uint_lanes<u32, 4uz>(lanes);
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled i32 splat opcode");
            }
# endif
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_splat_i64(wasm_i64 v) noexcept
        {
            if constexpr(Op == simd_code::i64x2_splat)
            {
                auto const bits{::std::bit_cast<u64>(v)};
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                return vec_to_v128(vec_splat<v128_u64x2>(bits));
# else
                lane_array<u64, 2uz> lanes{};  // init
                for(auto& lane: lanes.lane) { lane = bits; }
                return store_uint_lanes<u64, 2uz>(lanes);
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled i64 splat opcode");
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_splat_f32(wasm_f32 v) noexcept
        {
            if constexpr(Op == simd_code::f32x4_splat)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                return vec_to_v128(vec_splat<v128_f32x4>(v));
# else
                lane_array<wasm_f32, 4uz> lanes{};  // init
                for(auto& lane: lanes.lane) { lane = v; }
                return store_f32x4_lanes(lanes);
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled f32 splat opcode");
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_splat_f64(wasm_f64 v) noexcept
        {
            if constexpr(Op == simd_code::f64x2_splat)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                return vec_to_v128(vec_splat<v128_f64x2>(v));
# else
                lane_array<wasm_f64, 2uz> lanes{};  // init
                for(auto& lane: lanes.lane) { lane = v; }
                return store_f64x2_lanes(lanes);
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled f64 splat opcode");
            }
        }

        template <typename U, ::std::size_t N, bool Signed>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_i32 extract_int_lane(wasm_v128 v, u8 lane) noexcept
        {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
            if constexpr(::std::same_as<U, u8>)
            {
                auto const raw{v128_to_vec<v128_u8x16>(v)[lane]};
                if constexpr(Signed) { return static_cast<wasm_i32>(static_cast<::std::int_least32_t>(::std::bit_cast<s8>(raw))); }
                else
                {
                    return ::std::bit_cast<wasm_i32>(static_cast<u32>(raw));
                }
            }
            else if constexpr(::std::same_as<U, u16>)
            {
                auto const raw{v128_to_vec<v128_u16x8>(v)[lane]};
                if constexpr(Signed) { return static_cast<wasm_i32>(static_cast<::std::int_least32_t>(::std::bit_cast<s16>(raw))); }
                else
                {
                    return ::std::bit_cast<wasm_i32>(static_cast<u32>(raw));
                }
            }
            else
            {
                static_assert(::std::same_as<U, u32> && N == 4uz);
                return ::std::bit_cast<wasm_i32>(v128_to_vec<v128_u32x4>(v)[lane]);
            }
# else
            auto const lanes{load_uint_lanes<U, N>(v)};
            auto const raw{lanes.lane[lane]};
            if constexpr(Signed) { return static_cast<wasm_i32>(static_cast<::std::int_least32_t>(as_signed_lane<U>(raw))); }
            else
            {
                return ::std::bit_cast<wasm_i32>(static_cast<u32>(raw));
            }
# endif
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_i32 eval_extract_lane_i32(wasm_v128 v, u8 lane) noexcept
        {
            if constexpr(Op == simd_code::i8x16_extract_lane_s) { return extract_int_lane<u8, 16uz, true>(v, lane); }
            else if constexpr(Op == simd_code::i8x16_extract_lane_u) { return extract_int_lane<u8, 16uz, false>(v, lane); }
            else if constexpr(Op == simd_code::i16x8_extract_lane_s) { return extract_int_lane<u16, 8uz, true>(v, lane); }
            else if constexpr(Op == simd_code::i16x8_extract_lane_u) { return extract_int_lane<u16, 8uz, false>(v, lane); }
            else if constexpr(Op == simd_code::i32x4_extract_lane)
            {
                auto const lanes{load_uint_lanes<u32, 4uz>(v)};
                return ::std::bit_cast<wasm_i32>(lanes.lane[lane]);
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled i32 extract-lane opcode");
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_i64 eval_extract_lane_i64(wasm_v128 v, u8 lane) noexcept
        {
            if constexpr(Op == simd_code::i64x2_extract_lane)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                return ::std::bit_cast<wasm_i64>(v128_to_vec<v128_u64x2>(v)[lane]);
# else
                auto const lanes{load_uint_lanes<u64, 2uz>(v)};
                return ::std::bit_cast<wasm_i64>(lanes.lane[lane]);
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled i64 extract-lane opcode");
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_f32 eval_extract_lane_f32(wasm_v128 v, u8 lane) noexcept
        {
            if constexpr(Op == simd_code::f32x4_extract_lane)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && !UWVM2_STRICT_FLOAT_EXTENDED
                return v128_to_vec<v128_f32x4>(v)[lane];
# else
                return load_f32x4_lanes(v).lane[lane];
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled f32 extract-lane opcode");
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_f64 eval_extract_lane_f64(wasm_v128 v, u8 lane) noexcept
        {
            if constexpr(Op == simd_code::f64x2_extract_lane)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && !UWVM2_STRICT_FLOAT_EXTENDED
                return v128_to_vec<v128_f64x2>(v)[lane];
# else
                return load_f64x2_lanes(v).lane[lane];
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled f64 extract-lane opcode");
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_replace_lane_i32(wasm_v128 v, wasm_i32 x, u8 lane) noexcept
        {
            auto const bits{::std::bit_cast<u32>(x)};
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
            if constexpr(Op == simd_code::i8x16_replace_lane)
            {
                auto out{v128_to_vec<v128_u8x16>(v)};
                out[lane] = static_cast<u8>(bits);
                return vec_to_v128(out);
            }
            else if constexpr(Op == simd_code::i16x8_replace_lane)
            {
                auto out{v128_to_vec<v128_u16x8>(v)};
                out[lane] = static_cast<u16>(bits);
                return vec_to_v128(out);
            }
            else if constexpr(Op == simd_code::i32x4_replace_lane)
            {
                auto out{v128_to_vec<v128_u32x4>(v)};
                out[lane] = bits;
                return vec_to_v128(out);
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled i32 replace-lane opcode");
            }
# else
            if constexpr(Op == simd_code::i8x16_replace_lane)
            {
                auto lanes{load_uint_lanes<u8, 16uz>(v)};
                lanes.lane[lane] = static_cast<u8>(bits);
                return store_uint_lanes<u8, 16uz>(lanes);
            }
            else if constexpr(Op == simd_code::i16x8_replace_lane)
            {
                auto lanes{load_uint_lanes<u16, 8uz>(v)};
                lanes.lane[lane] = static_cast<u16>(bits);
                return store_uint_lanes<u16, 8uz>(lanes);
            }
            else if constexpr(Op == simd_code::i32x4_replace_lane)
            {
                auto lanes{load_uint_lanes<u32, 4uz>(v)};
                lanes.lane[lane] = bits;
                return store_uint_lanes<u32, 4uz>(lanes);
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled i32 replace-lane opcode");
            }
# endif
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_replace_lane_i64(wasm_v128 v, wasm_i64 x, u8 lane) noexcept
        {
            if constexpr(Op == simd_code::i64x2_replace_lane)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                auto out{v128_to_vec<v128_u64x2>(v)};
                out[lane] = ::std::bit_cast<u64>(x);
                return vec_to_v128(out);
# else
                auto lanes{load_uint_lanes<u64, 2uz>(v)};
                lanes.lane[lane] = ::std::bit_cast<u64>(x);
                return store_uint_lanes<u64, 2uz>(lanes);
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled i64 replace-lane opcode");
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_replace_lane_f32(wasm_v128 v, wasm_f32 x, u8 lane) noexcept
        {
            if constexpr(Op == simd_code::f32x4_replace_lane)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && !UWVM2_STRICT_FLOAT_EXTENDED
                auto out{v128_to_vec<v128_f32x4>(v)};
                out[lane] = x;
                return vec_to_v128(out);
# else
                auto lanes{load_f32x4_lanes(v)};
                lanes.lane[lane] = x;
                return store_f32x4_lanes(lanes);
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled f32 replace-lane opcode");
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_replace_lane_f64(wasm_v128 v, wasm_f64 x, u8 lane) noexcept
        {
            if constexpr(Op == simd_code::f64x2_replace_lane)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && !UWVM2_STRICT_FLOAT_EXTENDED
                auto out{v128_to_vec<v128_f64x2>(v)};
                out[lane] = x;
                return vec_to_v128(out);
# else
                auto lanes{load_f64x2_lanes(v)};
                lanes.lane[lane] = x;
                return store_f64x2_lanes(lanes);
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled f64 replace-lane opcode");
            }
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_shuffle(wasm_v128 lhs, wasm_v128 rhs, shuffle_controls const& controls) noexcept
        {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && defined(__SSSE3__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb128)
            v128_u8x16 lhs_control;  // no init
            v128_u8x16 rhs_control;  // no init
            ::std::memcpy(::std::addressof(lhs_control), controls.lhs, sizeof(lhs_control));
            ::std::memcpy(::std::addressof(rhs_control), controls.rhs, sizeof(rhs_control));
            auto const l{::std::bit_cast<v128_u8x16>(__builtin_ia32_pshufb128(v128_to_vec<v128_u8x16>(lhs), lhs_control))};
            auto const r{::std::bit_cast<v128_u8x16>(__builtin_ia32_pshufb128(v128_to_vec<v128_u8x16>(rhs), rhs_control))};
            return vec_to_v128(l | r);
# elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && defined(__ARM_NEON) &&                                                \
     UWVM_HAS_BUILTIN(__builtin_aarch64_qtbl1v16qi_uuu)
            v128_u8x16 lhs_control;  // no init
            v128_u8x16 rhs_control;  // no init
            ::std::memcpy(::std::addressof(lhs_control), controls.lhs, sizeof(lhs_control));
            ::std::memcpy(::std::addressof(rhs_control), controls.rhs, sizeof(rhs_control));
            auto const l{__builtin_aarch64_qtbl1v16qi_uuu(v128_to_vec<v128_u8x16>(lhs), lhs_control)};
            auto const r{__builtin_aarch64_qtbl1v16qi_uuu(v128_to_vec<v128_u8x16>(rhs), rhs_control)};
            return vec_to_v128(l | r);
# elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vqtbl1q_v)
            v128_u8x16 lhs_control;  // no init
            v128_u8x16 rhs_control;  // no init
            ::std::memcpy(::std::addressof(lhs_control), controls.lhs, sizeof(lhs_control));
            ::std::memcpy(::std::addressof(rhs_control), controls.rhs, sizeof(rhs_control));
            auto const l{::std::bit_cast<v128_u8x16>(
                __builtin_neon_vqtbl1q_v(::std::bit_cast<v128_i8x16>(v128_to_vec<v128_u8x16>(lhs)), ::std::bit_cast<v128_i8x16>(lhs_control), 48))};
            auto const r{::std::bit_cast<v128_u8x16>(
                __builtin_neon_vqtbl1q_v(::std::bit_cast<v128_i8x16>(v128_to_vec<v128_u8x16>(rhs)), ::std::bit_cast<v128_i8x16>(rhs_control), 48))};
            return vec_to_v128(l | r);
# else
            auto const l{load_uint_lanes<u8, 16uz>(lhs)};
            auto const r{load_uint_lanes<u8, 16uz>(rhs)};
            lane_array<u8, 16uz> out{};  // init
            for(::std::size_t i{}; i != 16uz; ++i)
            {
                auto const lhs_lane{controls.lhs[i]};
                auto const rhs_lane{controls.rhs[i]};
                out.lane[i] = lhs_lane < 16u ? l.lane[lhs_lane] : (rhs_lane < 16u ? r.lane[rhs_lane] : u8{});
            }
            return store_uint_lanes<u8, 16uz>(out);
# endif
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_swizzle(wasm_v128 lhs, wasm_v128 rhs) noexcept
        {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && defined(__SSSE3__) && UWVM_HAS_BUILTIN(__builtin_ia32_pshufb128)
            auto const values{v128_to_vec<v128_u8x16>(lhs)};
            auto const indices{v128_to_vec<v128_u8x16>(rhs)};
            auto const signed_indices{::std::bit_cast<v128_i8x16>(indices)};
            auto const over15{::std::bit_cast<v128_u8x16>(signed_indices > vec_splat<v128_i8x16>(s8{15}))};
            auto const control{static_cast<v128_u8x16>(indices | over15)};
            return vec_to_v128(::std::bit_cast<v128_u8x16>(__builtin_ia32_pshufb128(values, control)));
# elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && defined(__ARM_NEON) &&                                                \
     UWVM_HAS_BUILTIN(__builtin_aarch64_qtbl1v16qi_uuu)
            return vec_to_v128(__builtin_aarch64_qtbl1v16qi_uuu(v128_to_vec<v128_u8x16>(lhs), v128_to_vec<v128_u8x16>(rhs)));
# elif UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && defined(__ARM_NEON) && UWVM_HAS_BUILTIN(__builtin_neon_vqtbl1q_v)
            auto const values{v128_to_vec<v128_u8x16>(lhs)};
            auto const indices{v128_to_vec<v128_u8x16>(rhs)};
            return vec_to_v128(
                ::std::bit_cast<v128_u8x16>(__builtin_neon_vqtbl1q_v(::std::bit_cast<v128_i8x16>(values), ::std::bit_cast<v128_i8x16>(indices), 48)));
# else
            auto const values{load_uint_lanes<u8, 16uz>(lhs)};
            auto const indices{load_uint_lanes<u8, 16uz>(rhs)};
            lane_array<u8, 16uz> out{};  // init
            for(::std::size_t i{}; i != 16uz; ++i)
            {
                auto const lane{indices.lane[i]};
                out.lane[i] = lane < 16u ? values.lane[lane] : u8{};
            }
            return store_uint_lanes<u8, 16uz>(out);
# endif
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_bitselect(wasm_v128 lhs, wasm_v128 rhs, wasm_v128 mask) noexcept
        {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
            auto const l{v128_to_vec<v128_u8x16>(lhs)};
            auto const r{v128_to_vec<v128_u8x16>(rhs)};
            auto const m{v128_to_vec<v128_u8x16>(mask)};
            return vec_to_v128((l & m) | (r & ~m));
# else
            auto const l{load_uint_lanes<u8, 16uz>(lhs)};
            auto const r{load_uint_lanes<u8, 16uz>(rhs)};
            auto const m{load_uint_lanes<u8, 16uz>(mask)};
            lane_array<u8, 16uz> out{};  // init
            for(::std::size_t i{}; i != 16uz; ++i) { out.lane[i] = static_cast<u8>((l.lane[i] & m.lane[i]) | (r.lane[i] & ~m.lane[i])); }
            return store_uint_lanes<u8, 16uz>(out);
# endif
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_unop_native(wasm_v128 v) noexcept
        {
            if constexpr(Op == simd_code::v128_not) { return v128_bitwise_not(v); }
            else if constexpr(Op == simd_code::i8x16_abs || Op == simd_code::i8x16_neg || Op == simd_code::i8x16_popcnt)
            {
                if constexpr(Op == simd_code::i8x16_abs || Op == simd_code::i8x16_neg)
                {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_abs)
                    if constexpr(Op == simd_code::i8x16_abs) { return vec_abs_signed_v128<v128_u8x16, v128_i8x16>(v); }
#  endif
                    auto const in{v128_to_vec<v128_u8x16>(v)};
                    if constexpr(Op == simd_code::i8x16_abs) { return vec_to_v128(vec_signed_abs_bits<v128_u8x16, v128_i8x16>(in)); }
                    else if constexpr(Op == simd_code::i8x16_neg) { return vec_to_v128(static_cast<v128_u8x16>(v128_u8x16{} - in)); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i8x16 unary opcode");
                    }
# endif
                }
                auto lanes{load_uint_lanes<u8, 16uz>(v)};
                for(auto& lane: lanes.lane)
                {
                    if constexpr(Op == simd_code::i8x16_abs) { lane = abs_signed_bits(lane); }
                    else if constexpr(Op == simd_code::i8x16_neg) { lane = neg_bits(lane); }
                    else if constexpr(Op == simd_code::i8x16_popcnt) { lane = static_cast<u8>(::std::popcount(static_cast<unsigned>(lane))); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i8x16 unary opcode");
                    }
                }
                return store_uint_lanes<u8, 16uz>(lanes);
            }
            else if constexpr(Op == simd_code::i16x8_abs || Op == simd_code::i16x8_neg || Op == simd_code::i16x8_extend_low_i8x16_s ||
                              Op == simd_code::i16x8_extend_high_i8x16_s || Op == simd_code::i16x8_extend_low_i8x16_u ||
                              Op == simd_code::i16x8_extend_high_i8x16_u || Op == simd_code::i16x8_extadd_pairwise_i8x16_s ||
                              Op == simd_code::i16x8_extadd_pairwise_i8x16_u)
            {
                if constexpr(Op == simd_code::i16x8_abs || Op == simd_code::i16x8_neg)
                {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_abs)
                    if constexpr(Op == simd_code::i16x8_abs) { return vec_abs_signed_v128<v128_u16x8, v128_i16x8>(v); }
#  endif
                    auto const in{v128_to_vec<v128_u16x8>(v)};
                    if constexpr(Op == simd_code::i16x8_abs) { return vec_to_v128(vec_signed_abs_bits<v128_u16x8, v128_i16x8>(in)); }
                    else if constexpr(Op == simd_code::i16x8_neg) { return vec_to_v128(static_cast<v128_u16x8>(v128_u16x8{} - in)); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i16x8 unary opcode");
                    }
# else
                    auto lanes{load_uint_lanes<u16, 8uz>(v)};
                    for(auto& lane: lanes.lane)
                    {
                        if constexpr(Op == simd_code::i16x8_abs) { lane = abs_signed_bits(lane); }
                        else if constexpr(Op == simd_code::i16x8_neg) { lane = neg_bits(lane); }
                        else
                        {
                            static_assert(dependent_false_v<Op>, "unhandled i16x8 unary opcode");
                        }
                    }
                    return store_uint_lanes<u16, 8uz>(lanes);
# endif
                }
                else if constexpr(Op == simd_code::i16x8_extend_low_i8x16_s || Op == simd_code::i16x8_extend_high_i8x16_s ||
                                  Op == simd_code::i16x8_extend_low_i8x16_u || Op == simd_code::i16x8_extend_high_i8x16_u ||
                                  Op == simd_code::i16x8_extadd_pairwise_i8x16_s || Op == simd_code::i16x8_extadd_pairwise_i8x16_u)
                {
                    auto const in{load_uint_lanes<u8, 16uz>(v)};
                    lane_array<u16, 8uz> out{};  // init
                    if constexpr(Op == simd_code::i16x8_extadd_pairwise_i8x16_s || Op == simd_code::i16x8_extadd_pairwise_i8x16_u)
                    {
                        for(::std::size_t i{}; i != 8uz; ++i)
                        {
                            if constexpr(Op == simd_code::i16x8_extadd_pairwise_i8x16_s)
                            {
                                auto const a{static_cast<::std::int_least16_t>(as_signed_lane<u8>(in.lane[i * 2uz]))};
                                auto const b{static_cast<::std::int_least16_t>(as_signed_lane<u8>(in.lane[i * 2uz + 1uz]))};
                                out.lane[i] = from_signed_lane<u16>(static_cast<s16>(a + b));
                            }
                            else if constexpr(Op == simd_code::i16x8_extadd_pairwise_i8x16_u)
                            {
                                out.lane[i] = static_cast<u16>(static_cast<u16>(in.lane[i * 2uz]) + static_cast<u16>(in.lane[i * 2uz + 1uz]));
                            }
                            else
                            {
                                static_assert(dependent_false_v<Op>, "unhandled i16x8 extadd opcode");
                            }
                        }
                    }
                    else if constexpr(Op == simd_code::i16x8_extend_low_i8x16_s || Op == simd_code::i16x8_extend_high_i8x16_s ||
                                      Op == simd_code::i16x8_extend_low_i8x16_u || Op == simd_code::i16x8_extend_high_i8x16_u)
                    {
                        constexpr ::std::size_t begin{(Op == simd_code::i16x8_extend_high_i8x16_s || Op == simd_code::i16x8_extend_high_i8x16_u) ? 8uz : 0uz};
                        for(::std::size_t i{}; i != 8uz; ++i)
                        {
                            auto const lane{in.lane[begin + i]};
                            if constexpr(Op == simd_code::i16x8_extend_low_i8x16_s || Op == simd_code::i16x8_extend_high_i8x16_s)
                            {
                                out.lane[i] = from_signed_lane<u16>(static_cast<s16>(as_signed_lane<u8>(lane)));
                            }
                            else if constexpr(Op == simd_code::i16x8_extend_low_i8x16_u || Op == simd_code::i16x8_extend_high_i8x16_u)
                            {
                                out.lane[i] = static_cast<u16>(lane);
                            }
                            else
                            {
                                static_assert(dependent_false_v<Op>, "unhandled i16x8 extend opcode");
                            }
                        }
                    }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i16x8 unary opcode");
                    }
                    return store_uint_lanes<u16, 8uz>(out);
                }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled i16x8 unary opcode");
                }
            }
            else if constexpr(Op == simd_code::i32x4_abs || Op == simd_code::i32x4_neg || Op == simd_code::i32x4_extend_low_i16x8_s ||
                              Op == simd_code::i32x4_extend_high_i16x8_s || Op == simd_code::i32x4_extend_low_i16x8_u ||
                              Op == simd_code::i32x4_extend_high_i16x8_u || Op == simd_code::i32x4_extadd_pairwise_i16x8_s ||
                              Op == simd_code::i32x4_extadd_pairwise_i16x8_u || Op == simd_code::i32x4_trunc_sat_f32x4_s ||
                              Op == simd_code::i32x4_trunc_sat_f32x4_u || Op == simd_code::i32x4_trunc_sat_f64x2_s_zero ||
                              Op == simd_code::i32x4_trunc_sat_f64x2_u_zero)
            {
                if constexpr(Op == simd_code::i32x4_abs || Op == simd_code::i32x4_neg)
                {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_abs)
                    if constexpr(Op == simd_code::i32x4_abs) { return vec_abs_signed_v128<v128_u32x4, v128_i32x4>(v); }
#  endif
                    auto const in{v128_to_vec<v128_u32x4>(v)};
                    if constexpr(Op == simd_code::i32x4_abs) { return vec_to_v128(vec_signed_abs_bits<v128_u32x4, v128_i32x4>(in)); }
                    else if constexpr(Op == simd_code::i32x4_neg) { return vec_to_v128(static_cast<v128_u32x4>(v128_u32x4{} - in)); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i32x4 unary opcode");
                    }
# else
                    auto lanes{load_uint_lanes<u32, 4uz>(v)};
                    for(auto& lane: lanes.lane)
                    {
                        if constexpr(Op == simd_code::i32x4_abs) { lane = abs_signed_bits(lane); }
                        else if constexpr(Op == simd_code::i32x4_neg) { lane = neg_bits(lane); }
                        else
                        {
                            static_assert(dependent_false_v<Op>, "unhandled i32x4 unary opcode");
                        }
                    }
                    return store_uint_lanes<u32, 4uz>(lanes);
# endif
                }
                else if constexpr(Op == simd_code::i32x4_trunc_sat_f32x4_s || Op == simd_code::i32x4_trunc_sat_f32x4_u)
                {
                    auto const in{load_f32x4_lanes(v)};
                    lane_array<u32, 4uz> out{};  // init
                    for(::std::size_t i{}; i != 4uz; ++i)
                    {
                        auto const x{wasm1p1_details::trunc_sat<wasm_i32, Op == simd_code::i32x4_trunc_sat_f32x4_s>(in.lane[i])};
                        out.lane[i] = ::std::bit_cast<u32>(x);
                    }
                    return store_uint_lanes<u32, 4uz>(out);
                }
                else if constexpr(Op == simd_code::i32x4_trunc_sat_f64x2_s_zero || Op == simd_code::i32x4_trunc_sat_f64x2_u_zero)
                {
                    auto const in{load_f64x2_lanes(v)};
                    lane_array<u32, 4uz> out{};  // init
                    for(::std::size_t i{}; i != 2uz; ++i)
                    {
                        auto const x{wasm1p1_details::trunc_sat<wasm_i32, Op == simd_code::i32x4_trunc_sat_f64x2_s_zero>(in.lane[i])};
                        out.lane[i] = ::std::bit_cast<u32>(x);
                    }
                    return store_uint_lanes<u32, 4uz>(out);
                }
                else if constexpr(Op == simd_code::i32x4_extend_low_i16x8_s || Op == simd_code::i32x4_extend_high_i16x8_s ||
                                  Op == simd_code::i32x4_extend_low_i16x8_u || Op == simd_code::i32x4_extend_high_i16x8_u ||
                                  Op == simd_code::i32x4_extadd_pairwise_i16x8_s || Op == simd_code::i32x4_extadd_pairwise_i16x8_u)
                {
                    auto const in{load_uint_lanes<u16, 8uz>(v)};
                    lane_array<u32, 4uz> out{};  // init
                    if constexpr(Op == simd_code::i32x4_extadd_pairwise_i16x8_s || Op == simd_code::i32x4_extadd_pairwise_i16x8_u)
                    {
                        for(::std::size_t i{}; i != 4uz; ++i)
                        {
                            if constexpr(Op == simd_code::i32x4_extadd_pairwise_i16x8_s)
                            {
                                auto const a{static_cast<::std::int_least32_t>(as_signed_lane<u16>(in.lane[i * 2uz]))};
                                auto const b{static_cast<::std::int_least32_t>(as_signed_lane<u16>(in.lane[i * 2uz + 1uz]))};
                                out.lane[i] = from_signed_lane<u32>(static_cast<s32>(a + b));
                            }
                            else if constexpr(Op == simd_code::i32x4_extadd_pairwise_i16x8_u)
                            {
                                out.lane[i] = static_cast<u32>(static_cast<u32>(in.lane[i * 2uz]) + static_cast<u32>(in.lane[i * 2uz + 1uz]));
                            }
                            else
                            {
                                static_assert(dependent_false_v<Op>, "unhandled i32x4 extadd opcode");
                            }
                        }
                    }
                    else if constexpr(Op == simd_code::i32x4_extend_low_i16x8_s || Op == simd_code::i32x4_extend_high_i16x8_s ||
                                      Op == simd_code::i32x4_extend_low_i16x8_u || Op == simd_code::i32x4_extend_high_i16x8_u)
                    {
                        constexpr ::std::size_t begin{(Op == simd_code::i32x4_extend_high_i16x8_s || Op == simd_code::i32x4_extend_high_i16x8_u) ? 4uz : 0uz};
                        for(::std::size_t i{}; i != 4uz; ++i)
                        {
                            auto const lane{in.lane[begin + i]};
                            if constexpr(Op == simd_code::i32x4_extend_low_i16x8_s || Op == simd_code::i32x4_extend_high_i16x8_s)
                            {
                                out.lane[i] = from_signed_lane<u32>(static_cast<s32>(as_signed_lane<u16>(lane)));
                            }
                            else if constexpr(Op == simd_code::i32x4_extend_low_i16x8_u || Op == simd_code::i32x4_extend_high_i16x8_u)
                            {
                                out.lane[i] = static_cast<u32>(lane);
                            }
                            else
                            {
                                static_assert(dependent_false_v<Op>, "unhandled i32x4 extend opcode");
                            }
                        }
                    }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i32x4 unary opcode");
                    }
                    return store_uint_lanes<u32, 4uz>(out);
                }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled i32x4 unary opcode");
                }
            }
            else if constexpr(Op == simd_code::i64x2_abs || Op == simd_code::i64x2_neg || Op == simd_code::i64x2_extend_low_i32x4_s ||
                              Op == simd_code::i64x2_extend_high_i32x4_s || Op == simd_code::i64x2_extend_low_i32x4_u ||
                              Op == simd_code::i64x2_extend_high_i32x4_u)
            {
                if constexpr(Op == simd_code::i64x2_abs || Op == simd_code::i64x2_neg)
                {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                    auto const in{v128_to_vec<v128_u64x2>(v)};
                    if constexpr(Op == simd_code::i64x2_abs) { return vec_to_v128(vec_signed_abs_bits<v128_u64x2, v128_i64x2>(in)); }
                    else if constexpr(Op == simd_code::i64x2_neg) { return vec_to_v128(static_cast<v128_u64x2>(v128_u64x2{} - in)); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i64x2 unary opcode");
                    }
# else
                    auto lanes{load_uint_lanes<u64, 2uz>(v)};
                    for(auto& lane: lanes.lane)
                    {
                        if constexpr(Op == simd_code::i64x2_abs) { lane = abs_signed_bits(lane); }
                        else if constexpr(Op == simd_code::i64x2_neg) { lane = neg_bits(lane); }
                        else
                        {
                            static_assert(dependent_false_v<Op>, "unhandled i64x2 unary opcode");
                        }
                    }
                    return store_uint_lanes<u64, 2uz>(lanes);
# endif
                }
                else if constexpr(Op == simd_code::i64x2_extend_low_i32x4_s || Op == simd_code::i64x2_extend_high_i32x4_s ||
                                  Op == simd_code::i64x2_extend_low_i32x4_u || Op == simd_code::i64x2_extend_high_i32x4_u)
                {
                    auto const in{load_uint_lanes<u32, 4uz>(v)};
                    lane_array<u64, 2uz> out{};  // init
                    constexpr ::std::size_t begin{(Op == simd_code::i64x2_extend_high_i32x4_s || Op == simd_code::i64x2_extend_high_i32x4_u) ? 2uz : 0uz};
                    for(::std::size_t i{}; i != 2uz; ++i)
                    {
                        auto const lane{in.lane[begin + i]};
                        if constexpr(Op == simd_code::i64x2_extend_low_i32x4_s || Op == simd_code::i64x2_extend_high_i32x4_s)
                        {
                            out.lane[i] = from_signed_lane<u64>(static_cast<s64>(as_signed_lane<u32>(lane)));
                        }
                        else if constexpr(Op == simd_code::i64x2_extend_low_i32x4_u || Op == simd_code::i64x2_extend_high_i32x4_u)
                        {
                            out.lane[i] = static_cast<u64>(lane);
                        }
                        else
                        {
                            static_assert(dependent_false_v<Op>, "unhandled i64x2 extend opcode");
                        }
                    }
                    return store_uint_lanes<u64, 2uz>(out);
                }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled i64x2 unary opcode");
                }
            }
            else if constexpr(Op == simd_code::f32x4_abs || Op == simd_code::f32x4_neg || Op == simd_code::f32x4_sqrt || Op == simd_code::f32x4_ceil ||
                              Op == simd_code::f32x4_floor || Op == simd_code::f32x4_trunc || Op == simd_code::f32x4_nearest ||
                              Op == simd_code::f32x4_convert_i32x4_s || Op == simd_code::f32x4_convert_i32x4_u || Op == simd_code::f32x4_demote_f64x2_zero)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && !UWVM2_STRICT_FLOAT_EXTENDED
                if constexpr(Op == simd_code::f32x4_abs) { return vec_to_v128(v128_to_vec<v128_u32x4>(v) & vec_splat<v128_u32x4>(u32{0x7fffffffu})); }
                else if constexpr(Op == simd_code::f32x4_neg) { return vec_to_v128(v128_to_vec<v128_u32x4>(v) ^ vec_splat<v128_u32x4>(u32{0x80000000u})); }
#  if UWVM_HAS_BUILTIN(__builtin_convertvector)
                else if constexpr(Op == simd_code::f32x4_convert_i32x4_s)
                {
                    return vec_to_v128(__builtin_convertvector(v128_to_vec<v128_i32x4>(v), v128_f32x4));
                }
                else if constexpr(Op == simd_code::f32x4_convert_i32x4_u)
                {
                    return vec_to_v128(__builtin_convertvector(v128_to_vec<v128_u32x4>(v), v128_f32x4));
                }
#  endif
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_sqrt)
                else if constexpr(Op == simd_code::f32x4_sqrt) { return vec_to_v128(__builtin_elementwise_sqrt(v128_to_vec<v128_f32x4>(v))); }
#  endif
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_ceil)
                else if constexpr(Op == simd_code::f32x4_ceil) { return vec_to_v128(__builtin_elementwise_ceil(v128_to_vec<v128_f32x4>(v))); }
#  endif
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_floor)
                else if constexpr(Op == simd_code::f32x4_floor) { return vec_to_v128(__builtin_elementwise_floor(v128_to_vec<v128_f32x4>(v))); }
#  endif
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_trunc)
                else if constexpr(Op == simd_code::f32x4_trunc) { return vec_to_v128(__builtin_elementwise_trunc(v128_to_vec<v128_f32x4>(v))); }
#  endif
#  if (defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_roundps)) ||                                                                                     \
      (defined(__clang__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&                                  \
       UWVM_HAS_BUILTIN(__builtin_neon_vrndnq_v)) ||                                                                                                           \
      (!defined(__clang__) && defined(__GNUC__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&            \
       UWVM_HAS_BUILTIN(__builtin_aarch64_roundevenv4sf)) ||                                                                                                   \
      (defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vfrintrne_s))
                else if constexpr(Op == simd_code::f32x4_nearest) { return vec_roundeven_f32x4_v128(v); }
#  endif
# endif
                // Keep the scalar fallback bitwise too: 68881/x87 fabs and even FP loads may
                // quiet a signaling NaN. Integer lane transport also handles big-endian hosts.
                if constexpr(Op == simd_code::f32x4_abs || Op == simd_code::f32x4_neg)
                {
                    auto bits{load_uint_lanes<u32, 4uz>(v)};
                    for(auto& lane: bits.lane)
                    {
                        if constexpr(Op == simd_code::f32x4_abs) { lane &= 0x7fffffffu; }
                        else { lane ^= 0x80000000u; }
                    }
                    return store_uint_lanes<u32, 4uz>(bits);
                }
                lane_array<wasm_f32, 4uz> out{};  // init
                if constexpr(Op == simd_code::f32x4_convert_i32x4_s || Op == simd_code::f32x4_convert_i32x4_u)
                {
                    auto const in{load_uint_lanes<u32, 4uz>(v)};
                    for(::std::size_t i{}; i != 4uz; ++i)
                    {
                        if constexpr(Op == simd_code::f32x4_convert_i32x4_s) { out.lane[i] = strict_float::convert<wasm_f32>(as_signed_lane<u32>(in.lane[i])); }
                        else if constexpr(Op == simd_code::f32x4_convert_i32x4_u) { out.lane[i] = strict_float::convert<wasm_f32>(in.lane[i]); }
                        else
                        {
                            static_assert(dependent_false_v<Op>, "unhandled f32x4 convert opcode");
                        }
                    }
                }
                else if constexpr(Op == simd_code::f32x4_demote_f64x2_zero)
                {
                    auto const in{load_f64x2_lanes(v)};
                    out.lane[0] = strict_float::convert<wasm_f32>(in.lane[0]);
                    out.lane[1] = strict_float::convert<wasm_f32>(in.lane[1]);
                }
                else if constexpr(Op == simd_code::f32x4_abs || Op == simd_code::f32x4_neg || Op == simd_code::f32x4_sqrt || Op == simd_code::f32x4_ceil ||
                                  Op == simd_code::f32x4_floor || Op == simd_code::f32x4_trunc || Op == simd_code::f32x4_nearest)
                {
                    auto const in{load_f32x4_lanes(v)};
                    for(::std::size_t i{}; i != 4uz; ++i)
                    {
                        if constexpr(Op == simd_code::f32x4_abs) { out.lane[i] = ::std::fabs(in.lane[i]); }
                        else if constexpr(Op == simd_code::f32x4_neg) { out.lane[i] = -in.lane[i]; }
                        else if constexpr(Op == simd_code::f32x4_sqrt)
                        {
                            if constexpr(strict_float::needs_extended_rounding) { out.lane[i] = strict_float::square_root(in.lane[i]); }
                            else { out.lane[i] = ::std::sqrt(in.lane[i]); }
                        }
                        else if constexpr(Op == simd_code::f32x4_ceil) { out.lane[i] = ::std::ceil(in.lane[i]); }
                        else if constexpr(Op == simd_code::f32x4_floor) { out.lane[i] = ::std::floor(in.lane[i]); }
                        else if constexpr(Op == simd_code::f32x4_trunc) { out.lane[i] = ::std::trunc(in.lane[i]); }
                        else if constexpr(Op == simd_code::f32x4_nearest) { out.lane[i] = wasm_roundeven(in.lane[i]); }
                        else
                        {
                            static_assert(dependent_false_v<Op>, "unhandled f32x4 unary opcode");
                        }
                    }
                }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled f32x4 unary opcode");
                }
                return store_f32x4_lanes(out);
            }
            else if constexpr(Op == simd_code::f64x2_abs || Op == simd_code::f64x2_neg || Op == simd_code::f64x2_sqrt || Op == simd_code::f64x2_ceil ||
                              Op == simd_code::f64x2_floor || Op == simd_code::f64x2_trunc || Op == simd_code::f64x2_nearest ||
                              Op == simd_code::f64x2_promote_low_f32x4 || Op == simd_code::f64x2_convert_low_i32x4_s ||
                              Op == simd_code::f64x2_convert_low_i32x4_u)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && !UWVM2_STRICT_FLOAT_EXTENDED
                if constexpr(Op == simd_code::f64x2_abs) { return vec_to_v128(v128_to_vec<v128_u64x2>(v) & vec_splat<v128_u64x2>(u64{0x7fffffffffffffffull})); }
                else if constexpr(Op == simd_code::f64x2_neg)
                {
                    return vec_to_v128(v128_to_vec<v128_u64x2>(v) ^ vec_splat<v128_u64x2>(u64{0x8000000000000000ull}));
                }
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_sqrt)
                else if constexpr(Op == simd_code::f64x2_sqrt) { return vec_to_v128(__builtin_elementwise_sqrt(v128_to_vec<v128_f64x2>(v))); }
#  endif
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_ceil)
                else if constexpr(Op == simd_code::f64x2_ceil) { return vec_to_v128(__builtin_elementwise_ceil(v128_to_vec<v128_f64x2>(v))); }
#  endif
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_floor)
                else if constexpr(Op == simd_code::f64x2_floor) { return vec_to_v128(__builtin_elementwise_floor(v128_to_vec<v128_f64x2>(v))); }
#  endif
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_trunc)
                else if constexpr(Op == simd_code::f64x2_trunc) { return vec_to_v128(__builtin_elementwise_trunc(v128_to_vec<v128_f64x2>(v))); }
#  endif
#  if (defined(__SSE4_1__) && UWVM_HAS_BUILTIN(__builtin_ia32_roundpd)) ||                                                                                     \
      (defined(__clang__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&                                  \
       UWVM_HAS_BUILTIN(__builtin_neon_vrndnq_v)) ||                                                                                                           \
      (!defined(__clang__) && defined(__GNUC__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&            \
       UWVM_HAS_BUILTIN(__builtin_aarch64_roundevenv2df)) ||                                                                                                   \
      (defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vfrintrne_d))
                else if constexpr(Op == simd_code::f64x2_nearest) { return vec_roundeven_f64x2_v128(v); }
#  endif
# endif
                if constexpr(Op == simd_code::f64x2_abs || Op == simd_code::f64x2_neg)
                {
                    auto bits{load_uint_lanes<u64, 2uz>(v)};
                    for(auto& lane: bits.lane)
                    {
                        if constexpr(Op == simd_code::f64x2_abs) { lane &= 0x7fffffffffffffffull; }
                        else { lane ^= 0x8000000000000000ull; }
                    }
                    return store_uint_lanes<u64, 2uz>(bits);
                }
                lane_array<wasm_f64, 2uz> out{};  // init
                if constexpr(Op == simd_code::f64x2_promote_low_f32x4)
                {
                    auto const in{load_f32x4_lanes(v)};
                    out.lane[0] = strict_float::convert<wasm_f64>(in.lane[0]);
                    out.lane[1] = strict_float::convert<wasm_f64>(in.lane[1]);
                }
                else if constexpr(Op == simd_code::f64x2_convert_low_i32x4_s || Op == simd_code::f64x2_convert_low_i32x4_u)
                {
                    auto const in{load_uint_lanes<u32, 4uz>(v)};
                    for(::std::size_t i{}; i != 2uz; ++i)
                    {
                        if constexpr(Op == simd_code::f64x2_convert_low_i32x4_s) { out.lane[i] = strict_float::convert<wasm_f64>(as_signed_lane<u32>(in.lane[i])); }
                        else if constexpr(Op == simd_code::f64x2_convert_low_i32x4_u) { out.lane[i] = strict_float::convert<wasm_f64>(in.lane[i]); }
                        else
                        {
                            static_assert(dependent_false_v<Op>, "unhandled f64x2 convert opcode");
                        }
                    }
                }
                else if constexpr(Op == simd_code::f64x2_abs || Op == simd_code::f64x2_neg || Op == simd_code::f64x2_sqrt || Op == simd_code::f64x2_ceil ||
                                  Op == simd_code::f64x2_floor || Op == simd_code::f64x2_trunc || Op == simd_code::f64x2_nearest)
                {
                    auto const in{load_f64x2_lanes(v)};
                    for(::std::size_t i{}; i != 2uz; ++i)
                    {
                        if constexpr(Op == simd_code::f64x2_abs) { out.lane[i] = ::std::fabs(in.lane[i]); }
                        else if constexpr(Op == simd_code::f64x2_neg) { out.lane[i] = -in.lane[i]; }
                        else if constexpr(Op == simd_code::f64x2_sqrt)
                        {
                            if constexpr(strict_float::needs_extended_rounding) { out.lane[i] = strict_float::square_root(in.lane[i]); }
                            else { out.lane[i] = ::std::sqrt(in.lane[i]); }
                        }
                        else if constexpr(Op == simd_code::f64x2_ceil) { out.lane[i] = ::std::ceil(in.lane[i]); }
                        else if constexpr(Op == simd_code::f64x2_floor) { out.lane[i] = ::std::floor(in.lane[i]); }
                        else if constexpr(Op == simd_code::f64x2_trunc) { out.lane[i] = ::std::trunc(in.lane[i]); }
                        else if constexpr(Op == simd_code::f64x2_nearest) { out.lane[i] = wasm_roundeven(in.lane[i]); }
                        else
                        {
                            static_assert(dependent_false_v<Op>, "unhandled f64x2 unary opcode");
                        }
                    }
                }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled f64x2 unary opcode");
                }
                return store_f64x2_lanes(out);
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled SIMD unary opcode");
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_unop(wasm_v128 v) noexcept
        {
            constexpr bool integral32{Op == simd_code::f32x4_ceil || Op == simd_code::f32x4_floor || Op == simd_code::f32x4_trunc || Op == simd_code::f32x4_nearest};
            constexpr bool integral64{Op == simd_code::f64x2_ceil || Op == simd_code::f64x2_floor || Op == simd_code::f64x2_trunc || Op == simd_code::f64x2_nearest};
            if constexpr(strict_float::uses_integer_rounding && (integral32 || integral64))
            {
                constexpr auto mode{Op == simd_code::f32x4_ceil || Op == simd_code::f64x2_ceil ? strict_float::integral_rounding::ceil :
                                    Op == simd_code::f32x4_floor || Op == simd_code::f64x2_floor ? strict_float::integral_rounding::floor :
                                    Op == simd_code::f32x4_trunc || Op == simd_code::f64x2_trunc ? strict_float::integral_rounding::trunc : strict_float::integral_rounding::nearest};
                using UInt = ::std::conditional_t<integral32, u32, u64>;
                constexpr ::std::size_t count{integral32 ? 4uz : 2uz};
                auto lanes{load_uint_lanes<UInt, count>(v)};
                for(auto& lane: lanes.lane) { lane = strict_float::round_integral_bits<mode>(lane); }
                return store_uint_lanes<UInt, count>(lanes);
            }
            auto result{eval_full_unop_native<Op>(v)};
            constexpr bool round32{Op == simd_code::f32x4_ceil || Op == simd_code::f32x4_floor || Op == simd_code::f32x4_trunc};
            constexpr bool round64{Op == simd_code::f64x2_ceil || Op == simd_code::f64x2_floor || Op == simd_code::f64x2_trunc};
# if defined(__riscv)
            if constexpr(round32) { return canonicalize_native_float_lanes<u32, 4uz, true>(result); }
            else if constexpr(round64) { return canonicalize_native_float_lanes<u64, 2uz, true>(result); }
# endif
            if constexpr(round32 || Op == simd_code::f32x4_nearest || Op == simd_code::f32x4_sqrt || Op == simd_code::f32x4_demote_f64x2_zero)
            { return canonicalize_native_float_lanes<u32, 4uz>(result); }
            else if constexpr(round64 || Op == simd_code::f64x2_nearest || Op == simd_code::f64x2_sqrt || Op == simd_code::f64x2_promote_low_f32x4)
            { return canonicalize_native_float_lanes<u64, 2uz>(result); }
            else { return result; }
        }

        template <typename U, ::std::size_t N, simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_int_compare(wasm_v128 lhs, wasm_v128 rhs) noexcept
        {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
            if constexpr(::std::same_as<U, u8> && N == 16uz)
            {
                auto const l{v128_to_vec<v128_u8x16>(lhs)};
                auto const r{v128_to_vec<v128_u8x16>(rhs)};
                auto const ls{::std::bit_cast<v128_i8x16>(l)};
                auto const rs{::std::bit_cast<v128_i8x16>(r)};
                if constexpr(Op == simd_code::i8x16_eq) { return vec_to_v128(::std::bit_cast<v128_u8x16>(l == r)); }
                else if constexpr(Op == simd_code::i8x16_ne) { return vec_to_v128(::std::bit_cast<v128_u8x16>(l != r)); }
                else if constexpr(Op == simd_code::i8x16_lt_s) { return vec_to_v128(::std::bit_cast<v128_u8x16>(ls < rs)); }
                else if constexpr(Op == simd_code::i8x16_lt_u) { return vec_to_v128(::std::bit_cast<v128_u8x16>(l < r)); }
                else if constexpr(Op == simd_code::i8x16_gt_s) { return vec_to_v128(::std::bit_cast<v128_u8x16>(ls > rs)); }
                else if constexpr(Op == simd_code::i8x16_gt_u) { return vec_to_v128(::std::bit_cast<v128_u8x16>(l > r)); }
                else if constexpr(Op == simd_code::i8x16_le_s) { return vec_to_v128(::std::bit_cast<v128_u8x16>(ls <= rs)); }
                else if constexpr(Op == simd_code::i8x16_le_u) { return vec_to_v128(::std::bit_cast<v128_u8x16>(l <= r)); }
                else if constexpr(Op == simd_code::i8x16_ge_s) { return vec_to_v128(::std::bit_cast<v128_u8x16>(ls >= rs)); }
                else if constexpr(Op == simd_code::i8x16_ge_u) { return vec_to_v128(::std::bit_cast<v128_u8x16>(l >= r)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled i8x16 compare opcode");
                }
            }
            else if constexpr(::std::same_as<U, u16> && N == 8uz)
            {
                auto const l{v128_to_vec<v128_u16x8>(lhs)};
                auto const r{v128_to_vec<v128_u16x8>(rhs)};
                auto const ls{::std::bit_cast<v128_i16x8>(l)};
                auto const rs{::std::bit_cast<v128_i16x8>(r)};
                if constexpr(Op == simd_code::i16x8_eq) { return vec_to_v128(::std::bit_cast<v128_u16x8>(l == r)); }
                else if constexpr(Op == simd_code::i16x8_ne) { return vec_to_v128(::std::bit_cast<v128_u16x8>(l != r)); }
                else if constexpr(Op == simd_code::i16x8_lt_s) { return vec_to_v128(::std::bit_cast<v128_u16x8>(ls < rs)); }
                else if constexpr(Op == simd_code::i16x8_lt_u) { return vec_to_v128(::std::bit_cast<v128_u16x8>(l < r)); }
                else if constexpr(Op == simd_code::i16x8_gt_s) { return vec_to_v128(::std::bit_cast<v128_u16x8>(ls > rs)); }
                else if constexpr(Op == simd_code::i16x8_gt_u) { return vec_to_v128(::std::bit_cast<v128_u16x8>(l > r)); }
                else if constexpr(Op == simd_code::i16x8_le_s) { return vec_to_v128(::std::bit_cast<v128_u16x8>(ls <= rs)); }
                else if constexpr(Op == simd_code::i16x8_le_u) { return vec_to_v128(::std::bit_cast<v128_u16x8>(l <= r)); }
                else if constexpr(Op == simd_code::i16x8_ge_s) { return vec_to_v128(::std::bit_cast<v128_u16x8>(ls >= rs)); }
                else if constexpr(Op == simd_code::i16x8_ge_u) { return vec_to_v128(::std::bit_cast<v128_u16x8>(l >= r)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled i16x8 compare opcode");
                }
            }
            else if constexpr(::std::same_as<U, u32> && N == 4uz)
            {
                auto const l{v128_to_vec<v128_u32x4>(lhs)};
                auto const r{v128_to_vec<v128_u32x4>(rhs)};
                auto const ls{::std::bit_cast<v128_i32x4>(l)};
                auto const rs{::std::bit_cast<v128_i32x4>(r)};
                if constexpr(Op == simd_code::i32x4_eq) { return vec_to_v128(::std::bit_cast<v128_u32x4>(l == r)); }
                else if constexpr(Op == simd_code::i32x4_ne) { return vec_to_v128(::std::bit_cast<v128_u32x4>(l != r)); }
                else if constexpr(Op == simd_code::i32x4_lt_s) { return vec_to_v128(::std::bit_cast<v128_u32x4>(ls < rs)); }
                else if constexpr(Op == simd_code::i32x4_lt_u) { return vec_to_v128(::std::bit_cast<v128_u32x4>(l < r)); }
                else if constexpr(Op == simd_code::i32x4_gt_s) { return vec_to_v128(::std::bit_cast<v128_u32x4>(ls > rs)); }
                else if constexpr(Op == simd_code::i32x4_gt_u) { return vec_to_v128(::std::bit_cast<v128_u32x4>(l > r)); }
                else if constexpr(Op == simd_code::i32x4_le_s) { return vec_to_v128(::std::bit_cast<v128_u32x4>(ls <= rs)); }
                else if constexpr(Op == simd_code::i32x4_le_u) { return vec_to_v128(::std::bit_cast<v128_u32x4>(l <= r)); }
                else if constexpr(Op == simd_code::i32x4_ge_s) { return vec_to_v128(::std::bit_cast<v128_u32x4>(ls >= rs)); }
                else if constexpr(Op == simd_code::i32x4_ge_u) { return vec_to_v128(::std::bit_cast<v128_u32x4>(l >= r)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled i32x4 compare opcode");
                }
            }
            else if constexpr(::std::same_as<U, u64> && N == 2uz)
            {
                auto const l{v128_to_vec<v128_u64x2>(lhs)};
                auto const r{v128_to_vec<v128_u64x2>(rhs)};
                auto const ls{::std::bit_cast<v128_i64x2>(l)};
                auto const rs{::std::bit_cast<v128_i64x2>(r)};
                if constexpr(Op == simd_code::i64x2_eq) { return vec_to_v128(::std::bit_cast<v128_u64x2>(l == r)); }
                else if constexpr(Op == simd_code::i64x2_ne) { return vec_to_v128(::std::bit_cast<v128_u64x2>(l != r)); }
                else if constexpr(Op == simd_code::i64x2_lt_s) { return vec_to_v128(::std::bit_cast<v128_u64x2>(ls < rs)); }
                else if constexpr(Op == simd_code::i64x2_gt_s) { return vec_to_v128(::std::bit_cast<v128_u64x2>(ls > rs)); }
                else if constexpr(Op == simd_code::i64x2_le_s) { return vec_to_v128(::std::bit_cast<v128_u64x2>(ls <= rs)); }
                else if constexpr(Op == simd_code::i64x2_ge_s) { return vec_to_v128(::std::bit_cast<v128_u64x2>(ls >= rs)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled i64x2 compare opcode");
                }
            }
# endif
            auto const l{load_uint_lanes<U, N>(lhs)};
            auto const r{load_uint_lanes<U, N>(rhs)};
            lane_array<U, N> out{};  // init
            for(::std::size_t i{}; i != N; ++i)
            {
                bool b{};
                if constexpr(Op == simd_code::i8x16_eq || Op == simd_code::i16x8_eq || Op == simd_code::i32x4_eq || Op == simd_code::i64x2_eq)
                {
                    b = l.lane[i] == r.lane[i];
                }
                else if constexpr(Op == simd_code::i8x16_ne || Op == simd_code::i16x8_ne || Op == simd_code::i32x4_ne || Op == simd_code::i64x2_ne)
                {
                    b = l.lane[i] != r.lane[i];
                }
                else if constexpr(Op == simd_code::i8x16_lt_s || Op == simd_code::i16x8_lt_s || Op == simd_code::i32x4_lt_s || Op == simd_code::i64x2_lt_s)
                {
                    b = as_signed_lane<U>(l.lane[i]) < as_signed_lane<U>(r.lane[i]);
                }
                else if constexpr(Op == simd_code::i8x16_lt_u || Op == simd_code::i16x8_lt_u || Op == simd_code::i32x4_lt_u) { b = l.lane[i] < r.lane[i]; }
                else if constexpr(Op == simd_code::i8x16_gt_s || Op == simd_code::i16x8_gt_s || Op == simd_code::i32x4_gt_s || Op == simd_code::i64x2_gt_s)
                {
                    b = as_signed_lane<U>(l.lane[i]) > as_signed_lane<U>(r.lane[i]);
                }
                else if constexpr(Op == simd_code::i8x16_gt_u || Op == simd_code::i16x8_gt_u || Op == simd_code::i32x4_gt_u) { b = l.lane[i] > r.lane[i]; }
                else if constexpr(Op == simd_code::i8x16_le_s || Op == simd_code::i16x8_le_s || Op == simd_code::i32x4_le_s || Op == simd_code::i64x2_le_s)
                {
                    b = as_signed_lane<U>(l.lane[i]) <= as_signed_lane<U>(r.lane[i]);
                }
                else if constexpr(Op == simd_code::i8x16_le_u || Op == simd_code::i16x8_le_u || Op == simd_code::i32x4_le_u) { b = l.lane[i] <= r.lane[i]; }
                else if constexpr(Op == simd_code::i8x16_ge_s || Op == simd_code::i16x8_ge_s || Op == simd_code::i32x4_ge_s || Op == simd_code::i64x2_ge_s)
                {
                    b = as_signed_lane<U>(l.lane[i]) >= as_signed_lane<U>(r.lane[i]);
                }
                else if constexpr(Op == simd_code::i8x16_ge_u || Op == simd_code::i16x8_ge_u || Op == simd_code::i32x4_ge_u) { b = l.lane[i] >= r.lane[i]; }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled integer SIMD compare opcode");
                }
                out.lane[i] = lane_bool<U>(b);
            }
            return store_uint_lanes<U, N>(out);
        }

        template <typename FloatT, typename MaskU, ::std::size_t N, simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_float_compare(wasm_v128 lhs, wasm_v128 rhs) noexcept
        {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && !UWVM2_STRICT_FLOAT_EXTENDED
            if constexpr(::std::same_as<FloatT, wasm_f32> && N == 4uz)
            {
                auto const l{v128_to_vec<v128_f32x4>(lhs)};
                auto const r{v128_to_vec<v128_f32x4>(rhs)};
                if constexpr(Op == simd_code::f32x4_eq) { return vec_to_v128(::std::bit_cast<v128_u32x4>(l == r)); }
                else if constexpr(Op == simd_code::f32x4_ne) { return vec_to_v128(::std::bit_cast<v128_u32x4>(l != r)); }
                else if constexpr(Op == simd_code::f32x4_lt) { return vec_to_v128(::std::bit_cast<v128_u32x4>(l < r)); }
                else if constexpr(Op == simd_code::f32x4_gt) { return vec_to_v128(::std::bit_cast<v128_u32x4>(l > r)); }
                else if constexpr(Op == simd_code::f32x4_le) { return vec_to_v128(::std::bit_cast<v128_u32x4>(l <= r)); }
                else if constexpr(Op == simd_code::f32x4_ge) { return vec_to_v128(::std::bit_cast<v128_u32x4>(l >= r)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled f32x4 compare opcode");
                }
            }
            else if constexpr(::std::same_as<FloatT, wasm_f64> && N == 2uz)
            {
                auto const l{v128_to_vec<v128_f64x2>(lhs)};
                auto const r{v128_to_vec<v128_f64x2>(rhs)};
                if constexpr(Op == simd_code::f64x2_eq) { return vec_to_v128(::std::bit_cast<v128_u64x2>(l == r)); }
                else if constexpr(Op == simd_code::f64x2_ne) { return vec_to_v128(::std::bit_cast<v128_u64x2>(l != r)); }
                else if constexpr(Op == simd_code::f64x2_lt) { return vec_to_v128(::std::bit_cast<v128_u64x2>(l < r)); }
                else if constexpr(Op == simd_code::f64x2_gt) { return vec_to_v128(::std::bit_cast<v128_u64x2>(l > r)); }
                else if constexpr(Op == simd_code::f64x2_le) { return vec_to_v128(::std::bit_cast<v128_u64x2>(l <= r)); }
                else if constexpr(Op == simd_code::f64x2_ge) { return vec_to_v128(::std::bit_cast<v128_u64x2>(l >= r)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled f64x2 compare opcode");
                }
            }
# endif
            auto const l{[](wasm_v128 x)
                         {
                             if constexpr(::std::same_as<FloatT, wasm_f32>) { return load_f32x4_lanes(x); }
                             else if constexpr(::std::same_as<FloatT, wasm_f64>) { return load_f64x2_lanes(x); }
                             else
                             {
                                 static_assert(!::std::same_as<FloatT, FloatT>, "unhandled SIMD float lane type");
                             }
                         }(lhs)};
            auto const r{[](wasm_v128 x)
                         {
                             if constexpr(::std::same_as<FloatT, wasm_f32>) { return load_f32x4_lanes(x); }
                             else if constexpr(::std::same_as<FloatT, wasm_f64>) { return load_f64x2_lanes(x); }
                             else
                             {
                                 static_assert(!::std::same_as<FloatT, FloatT>, "unhandled SIMD float lane type");
                             }
                         }(rhs)};
            lane_array<MaskU, N> out{};  // init
            for(::std::size_t i{}; i != N; ++i)
            {
                bool b{};
                if constexpr(Op == simd_code::f32x4_eq || Op == simd_code::f64x2_eq) { b = l.lane[i] == r.lane[i]; }
                else if constexpr(Op == simd_code::f32x4_ne || Op == simd_code::f64x2_ne) { b = l.lane[i] != r.lane[i]; }
                else if constexpr(Op == simd_code::f32x4_lt || Op == simd_code::f64x2_lt) { b = l.lane[i] < r.lane[i]; }
                else if constexpr(Op == simd_code::f32x4_gt || Op == simd_code::f64x2_gt) { b = l.lane[i] > r.lane[i]; }
                else if constexpr(Op == simd_code::f32x4_le || Op == simd_code::f64x2_le) { b = l.lane[i] <= r.lane[i]; }
                else if constexpr(Op == simd_code::f32x4_ge || Op == simd_code::f64x2_ge) { b = l.lane[i] >= r.lane[i]; }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled float SIMD compare opcode");
                }
                out.lane[i] = lane_bool<MaskU>(b);
            }
            return store_uint_lanes<MaskU, N>(out);
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_binop(wasm_v128 lhs, wasm_v128 rhs) noexcept
        {
            if constexpr(Op == simd_code::i8x16_swizzle) { return eval_swizzle(lhs, rhs); }
            else if constexpr(Op == simd_code::v128_and || Op == simd_code::v128_andnot || Op == simd_code::v128_or || Op == simd_code::v128_xor)
            {
                if constexpr(Op == simd_code::v128_and) { return eval_v128_binop<v128_binop::and_>(lhs, rhs); }
                else if constexpr(Op == simd_code::v128_andnot) { return eval_v128_binop<v128_binop::andnot>(lhs, rhs); }
                else if constexpr(Op == simd_code::v128_or) { return eval_v128_binop<v128_binop::or_>(lhs, rhs); }
                else if constexpr(Op == simd_code::v128_xor) { return eval_v128_binop<v128_binop::xor_>(lhs, rhs); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled v128 bitwise opcode");
                }
            }
            else if constexpr(Op >= simd_code::i8x16_eq && Op <= simd_code::i8x16_ge_u) { return eval_int_compare<u8, 16uz, Op>(lhs, rhs); }
            else if constexpr(Op >= simd_code::i16x8_eq && Op <= simd_code::i16x8_ge_u) { return eval_int_compare<u16, 8uz, Op>(lhs, rhs); }
            else if constexpr(Op >= simd_code::i32x4_eq && Op <= simd_code::i32x4_ge_u) { return eval_int_compare<u32, 4uz, Op>(lhs, rhs); }
            else if constexpr(Op >= simd_code::i64x2_eq && Op <= simd_code::i64x2_ge_s) { return eval_int_compare<u64, 2uz, Op>(lhs, rhs); }
            else if constexpr(Op >= simd_code::f32x4_eq && Op <= simd_code::f32x4_ge) { return eval_float_compare<wasm_f32, u32, 4uz, Op>(lhs, rhs); }
            else if constexpr(Op >= simd_code::f64x2_eq && Op <= simd_code::f64x2_ge) { return eval_float_compare<wasm_f64, u64, 2uz, Op>(lhs, rhs); }
            else if constexpr(Op == simd_code::i8x16_narrow_i16x8_s || Op == simd_code::i8x16_narrow_i16x8_u)
            {
                auto const l{load_uint_lanes<u16, 8uz>(lhs)};
                auto const r{load_uint_lanes<u16, 8uz>(rhs)};
                lane_array<u8, 16uz> out{};  // init
                for(::std::size_t i{}; i != 8uz; ++i)
                {
                    if constexpr(Op == simd_code::i8x16_narrow_i16x8_s)
                    {
                        out.lane[i] = narrow_signed_lane<u8>(l.lane[i]);
                        out.lane[i + 8uz] = narrow_signed_lane<u8>(r.lane[i]);
                    }
                    else
                    {
                        out.lane[i] = narrow_unsigned_lane<u8>(l.lane[i]);
                        out.lane[i + 8uz] = narrow_unsigned_lane<u8>(r.lane[i]);
                    }
                }
                return store_uint_lanes<u8, 16uz>(out);
            }
            else if constexpr(Op == simd_code::i16x8_narrow_i32x4_s || Op == simd_code::i16x8_narrow_i32x4_u)
            {
                auto const l{load_uint_lanes<u32, 4uz>(lhs)};
                auto const r{load_uint_lanes<u32, 4uz>(rhs)};
                lane_array<u16, 8uz> out{};  // init
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    if constexpr(Op == simd_code::i16x8_narrow_i32x4_s)
                    {
                        out.lane[i] = narrow_signed_lane<u16>(l.lane[i]);
                        out.lane[i + 4uz] = narrow_signed_lane<u16>(r.lane[i]);
                    }
                    else
                    {
                        out.lane[i] = narrow_unsigned_lane<u16>(l.lane[i]);
                        out.lane[i + 4uz] = narrow_unsigned_lane<u16>(r.lane[i]);
                    }
                }
                return store_uint_lanes<u16, 8uz>(out);
            }
            else if constexpr(Op == simd_code::i8x16_add || Op == simd_code::i8x16_add_sat_s || Op == simd_code::i8x16_add_sat_u ||
                              Op == simd_code::i8x16_sub || Op == simd_code::i8x16_sub_sat_s || Op == simd_code::i8x16_sub_sat_u ||
                              Op == simd_code::i8x16_min_s || Op == simd_code::i8x16_min_u || Op == simd_code::i8x16_max_s || Op == simd_code::i8x16_max_u ||
                              Op == simd_code::i8x16_avgr_u)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_add_sat)
                if constexpr(Op == simd_code::i8x16_add_sat_s) { return vec_add_sat_s_v128<v128_u8x16, v128_i8x16>(lhs, rhs); }
                else if constexpr(Op == simd_code::i8x16_add_sat_u) { return vec_add_sat_u_v128<v128_u8x16>(lhs, rhs); }
#  endif
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_sub_sat)
                if constexpr(Op == simd_code::i8x16_sub_sat_s) { return vec_sub_sat_s_v128<v128_u8x16, v128_i8x16>(lhs, rhs); }
                else if constexpr(Op == simd_code::i8x16_sub_sat_u) { return vec_sub_sat_u_v128<v128_u8x16>(lhs, rhs); }
#  endif
                if constexpr(Op == simd_code::i8x16_add || Op == simd_code::i8x16_sub || Op == simd_code::i8x16_min_s || Op == simd_code::i8x16_min_u ||
                             Op == simd_code::i8x16_max_s || Op == simd_code::i8x16_max_u || Op == simd_code::i8x16_avgr_u)
                {
                    auto const l{v128_to_vec<v128_u8x16>(lhs)};
                    auto const r{v128_to_vec<v128_u8x16>(rhs)};
                    auto const ls{::std::bit_cast<v128_i8x16>(l)};
                    auto const rs{::std::bit_cast<v128_i8x16>(r)};
                    if constexpr(Op == simd_code::i8x16_add) { return vec_to_v128(l + r); }
                    else if constexpr(Op == simd_code::i8x16_sub) { return vec_to_v128(l - r); }
                    else if constexpr(Op == simd_code::i8x16_min_s) { return vec_to_v128(vec_select(l, r, ls < rs)); }
                    else if constexpr(Op == simd_code::i8x16_min_u) { return vec_to_v128(vec_select(l, r, l < r)); }
                    else if constexpr(Op == simd_code::i8x16_max_s) { return vec_to_v128(vec_select(l, r, ls > rs)); }
                    else if constexpr(Op == simd_code::i8x16_max_u) { return vec_to_v128(vec_select(l, r, l > r)); }
                    else if constexpr(Op == simd_code::i8x16_avgr_u) { return vec_to_v128(static_cast<v128_u8x16>((l | r) - ((l ^ r) >> 1u))); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i8x16 binary opcode");
                    }
                }
# endif
                auto l{load_uint_lanes<u8, 16uz>(lhs)};
                auto const r{load_uint_lanes<u8, 16uz>(rhs)};
                for(::std::size_t i{}; i != 16uz; ++i)
                {
                    if constexpr(Op == simd_code::i8x16_add) { l.lane[i] = static_cast<u8>(l.lane[i] + r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_sub) { l.lane[i] = static_cast<u8>(l.lane[i] - r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_add_sat_s) { l.lane[i] = sat_add_signed(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_add_sat_u) { l.lane[i] = sat_add_unsigned(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_sub_sat_s) { l.lane[i] = sat_sub_signed(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_sub_sat_u) { l.lane[i] = sat_sub_unsigned(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_min_s) { l.lane[i] = min_signed_bits(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_min_u) { l.lane[i] = l.lane[i] < r.lane[i] ? l.lane[i] : r.lane[i]; }
                    else if constexpr(Op == simd_code::i8x16_max_s) { l.lane[i] = max_signed_bits(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i8x16_max_u) { l.lane[i] = l.lane[i] > r.lane[i] ? l.lane[i] : r.lane[i]; }
                    else if constexpr(Op == simd_code::i8x16_avgr_u)
                    {
                        l.lane[i] = static_cast<u8>((static_cast<unsigned>(l.lane[i]) + static_cast<unsigned>(r.lane[i]) + 1u) >> 1u);
                    }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i8x16 binary opcode");
                    }
                }
                return store_uint_lanes<u8, 16uz>(l);
            }
            else if constexpr(Op == simd_code::i16x8_q15mulr_sat_s || Op == simd_code::i16x8_add || Op == simd_code::i16x8_add_sat_s ||
                              Op == simd_code::i16x8_add_sat_u || Op == simd_code::i16x8_sub || Op == simd_code::i16x8_sub_sat_s ||
                              Op == simd_code::i16x8_sub_sat_u || Op == simd_code::i16x8_mul || Op == simd_code::i16x8_min_s || Op == simd_code::i16x8_min_u ||
                              Op == simd_code::i16x8_max_s || Op == simd_code::i16x8_max_u || Op == simd_code::i16x8_avgr_u)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_add_sat)
                if constexpr(Op == simd_code::i16x8_add_sat_s) { return vec_add_sat_s_v128<v128_u16x8, v128_i16x8>(lhs, rhs); }
                else if constexpr(Op == simd_code::i16x8_add_sat_u) { return vec_add_sat_u_v128<v128_u16x8>(lhs, rhs); }
#  endif
#  if UWVM_HAS_BUILTIN(__builtin_elementwise_sub_sat)
                if constexpr(Op == simd_code::i16x8_sub_sat_s) { return vec_sub_sat_s_v128<v128_u16x8, v128_i16x8>(lhs, rhs); }
                else if constexpr(Op == simd_code::i16x8_sub_sat_u) { return vec_sub_sat_u_v128<v128_u16x8>(lhs, rhs); }
#  endif
                if constexpr(Op == simd_code::i16x8_add || Op == simd_code::i16x8_sub || Op == simd_code::i16x8_mul || Op == simd_code::i16x8_min_s ||
                             Op == simd_code::i16x8_min_u || Op == simd_code::i16x8_max_s || Op == simd_code::i16x8_max_u || Op == simd_code::i16x8_avgr_u)
                {
                    auto const l{v128_to_vec<v128_u16x8>(lhs)};
                    auto const r{v128_to_vec<v128_u16x8>(rhs)};
                    auto const ls{::std::bit_cast<v128_i16x8>(l)};
                    auto const rs{::std::bit_cast<v128_i16x8>(r)};
                    if constexpr(Op == simd_code::i16x8_add) { return vec_to_v128(l + r); }
                    else if constexpr(Op == simd_code::i16x8_sub) { return vec_to_v128(l - r); }
                    else if constexpr(Op == simd_code::i16x8_mul) { return vec_to_v128(l * r); }
                    else if constexpr(Op == simd_code::i16x8_min_s) { return vec_to_v128(vec_select(l, r, ls < rs)); }
                    else if constexpr(Op == simd_code::i16x8_min_u) { return vec_to_v128(vec_select(l, r, l < r)); }
                    else if constexpr(Op == simd_code::i16x8_max_s) { return vec_to_v128(vec_select(l, r, ls > rs)); }
                    else if constexpr(Op == simd_code::i16x8_max_u) { return vec_to_v128(vec_select(l, r, l > r)); }
                    else if constexpr(Op == simd_code::i16x8_avgr_u) { return vec_to_v128(static_cast<v128_u16x8>((l | r) - ((l ^ r) >> 1u))); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i16x8 binary opcode");
                    }
                }
# endif
                auto l{load_uint_lanes<u16, 8uz>(lhs)};
                auto const r{load_uint_lanes<u16, 8uz>(rhs)};
                for(::std::size_t i{}; i != 8uz; ++i)
                {
                    if constexpr(Op == simd_code::i16x8_add) { l.lane[i] = static_cast<u16>(l.lane[i] + r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_sub) { l.lane[i] = static_cast<u16>(l.lane[i] - r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_mul) { l.lane[i] = static_cast<u16>(l.lane[i] * r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_q15mulr_sat_s) { l.lane[i] = q15mulr_sat_s(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_add_sat_s) { l.lane[i] = sat_add_signed(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_add_sat_u) { l.lane[i] = sat_add_unsigned(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_sub_sat_s) { l.lane[i] = sat_sub_signed(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_sub_sat_u) { l.lane[i] = sat_sub_unsigned(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_min_s) { l.lane[i] = min_signed_bits(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_min_u) { l.lane[i] = l.lane[i] < r.lane[i] ? l.lane[i] : r.lane[i]; }
                    else if constexpr(Op == simd_code::i16x8_max_s) { l.lane[i] = max_signed_bits(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i16x8_max_u) { l.lane[i] = l.lane[i] > r.lane[i] ? l.lane[i] : r.lane[i]; }
                    else if constexpr(Op == simd_code::i16x8_avgr_u)
                    {
                        l.lane[i] = static_cast<u16>((static_cast<u32>(l.lane[i]) + static_cast<u32>(r.lane[i]) + 1u) >> 1u);
                    }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i16x8 binary opcode");
                    }
                }
                return store_uint_lanes<u16, 8uz>(l);
            }
            else if constexpr(Op == simd_code::i16x8_extmul_low_i8x16_s || Op == simd_code::i16x8_extmul_high_i8x16_s ||
                              Op == simd_code::i16x8_extmul_low_i8x16_u || Op == simd_code::i16x8_extmul_high_i8x16_u)
            {
                auto const l{load_uint_lanes<u8, 16uz>(lhs)};
                auto const r{load_uint_lanes<u8, 16uz>(rhs)};
                lane_array<u16, 8uz> out{};  // init
                constexpr ::std::size_t begin{(Op == simd_code::i16x8_extmul_high_i8x16_s || Op == simd_code::i16x8_extmul_high_i8x16_u) ? 8uz : 0uz};
                for(::std::size_t i{}; i != 8uz; ++i)
                {
                    if constexpr(Op == simd_code::i16x8_extmul_low_i8x16_s || Op == simd_code::i16x8_extmul_high_i8x16_s)
                    {
                        out.lane[i] = from_signed_lane<u16>(static_cast<s16>(static_cast<::std::int_least16_t>(as_signed_lane<u8>(l.lane[begin + i])) *
                                                                             static_cast<::std::int_least16_t>(as_signed_lane<u8>(r.lane[begin + i]))));
                    }
                    else
                    {
                        out.lane[i] = static_cast<u16>(static_cast<u16>(l.lane[begin + i]) * static_cast<u16>(r.lane[begin + i]));
                    }
                }
                return store_uint_lanes<u16, 8uz>(out);
            }
            else if constexpr(Op == simd_code::i32x4_add || Op == simd_code::i32x4_sub || Op == simd_code::i32x4_mul || Op == simd_code::i32x4_min_s ||
                              Op == simd_code::i32x4_min_u || Op == simd_code::i32x4_max_s || Op == simd_code::i32x4_max_u)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                auto const l{v128_to_vec<v128_u32x4>(lhs)};
                auto const r{v128_to_vec<v128_u32x4>(rhs)};
                auto const ls{::std::bit_cast<v128_i32x4>(l)};
                auto const rs{::std::bit_cast<v128_i32x4>(r)};
                if constexpr(Op == simd_code::i32x4_add) { return vec_to_v128(l + r); }
                else if constexpr(Op == simd_code::i32x4_sub) { return vec_to_v128(l - r); }
                else if constexpr(Op == simd_code::i32x4_mul) { return vec_to_v128(l * r); }
                else if constexpr(Op == simd_code::i32x4_min_s) { return vec_to_v128(vec_select(l, r, ls < rs)); }
                else if constexpr(Op == simd_code::i32x4_min_u) { return vec_to_v128(vec_select(l, r, l < r)); }
                else if constexpr(Op == simd_code::i32x4_max_s) { return vec_to_v128(vec_select(l, r, ls > rs)); }
                else if constexpr(Op == simd_code::i32x4_max_u) { return vec_to_v128(vec_select(l, r, l > r)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled i32x4 binary opcode");
                }
# else
                auto l{load_uint_lanes<u32, 4uz>(lhs)};
                auto const r{load_uint_lanes<u32, 4uz>(rhs)};
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    if constexpr(Op == simd_code::i32x4_add) { l.lane[i] = static_cast<u32>(l.lane[i] + r.lane[i]); }
                    else if constexpr(Op == simd_code::i32x4_sub) { l.lane[i] = static_cast<u32>(l.lane[i] - r.lane[i]); }
                    else if constexpr(Op == simd_code::i32x4_mul) { l.lane[i] = static_cast<u32>(l.lane[i] * r.lane[i]); }
                    else if constexpr(Op == simd_code::i32x4_min_s) { l.lane[i] = min_signed_bits(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i32x4_min_u) { l.lane[i] = l.lane[i] < r.lane[i] ? l.lane[i] : r.lane[i]; }
                    else if constexpr(Op == simd_code::i32x4_max_s) { l.lane[i] = max_signed_bits(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::i32x4_max_u) { l.lane[i] = l.lane[i] > r.lane[i] ? l.lane[i] : r.lane[i]; }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i32x4 binary opcode");
                    }
                }
                return store_uint_lanes<u32, 4uz>(l);
# endif
            }
            else if constexpr(Op == simd_code::i32x4_dot_i16x8_s || Op == simd_code::i32x4_extmul_low_i16x8_s || Op == simd_code::i32x4_extmul_high_i16x8_s ||
                              Op == simd_code::i32x4_extmul_low_i16x8_u || Op == simd_code::i32x4_extmul_high_i16x8_u)
            {
                auto const l{load_uint_lanes<u16, 8uz>(lhs)};
                auto const r{load_uint_lanes<u16, 8uz>(rhs)};
                lane_array<u32, 4uz> out{};  // init
                if constexpr(Op == simd_code::i32x4_dot_i16x8_s)
                {
                    for(::std::size_t i{}; i != 4uz; ++i)
                    {
                        auto const a0{static_cast<::std::int_least64_t>(as_signed_lane<u16>(l.lane[i * 2uz]))};
                        auto const a1{static_cast<::std::int_least64_t>(as_signed_lane<u16>(l.lane[i * 2uz + 1uz]))};
                        auto const b0{static_cast<::std::int_least64_t>(as_signed_lane<u16>(r.lane[i * 2uz]))};
                        auto const b1{static_cast<::std::int_least64_t>(as_signed_lane<u16>(r.lane[i * 2uz + 1uz]))};
                        // The mathematical sum can be 2^31. Compute it without signed overflow, then apply Wasm's i32 wrap.
                        out.lane[i] = static_cast<u32>(a0 * b0 + a1 * b1);
                    }
                }
                else
                {
                    constexpr ::std::size_t begin{(Op == simd_code::i32x4_extmul_high_i16x8_s || Op == simd_code::i32x4_extmul_high_i16x8_u) ? 4uz : 0uz};
                    for(::std::size_t i{}; i != 4uz; ++i)
                    {
                        if constexpr(Op == simd_code::i32x4_extmul_low_i16x8_s || Op == simd_code::i32x4_extmul_high_i16x8_s)
                        {
                            out.lane[i] = from_signed_lane<u32>(static_cast<s32>(static_cast<::std::int_least32_t>(as_signed_lane<u16>(l.lane[begin + i])) *
                                                                                 static_cast<::std::int_least32_t>(as_signed_lane<u16>(r.lane[begin + i]))));
                        }
                        else
                        {
                            out.lane[i] = static_cast<u32>(static_cast<u32>(l.lane[begin + i]) * static_cast<u32>(r.lane[begin + i]));
                        }
                    }
                }
                return store_uint_lanes<u32, 4uz>(out);
            }
            else if constexpr(Op == simd_code::i64x2_add || Op == simd_code::i64x2_sub || Op == simd_code::i64x2_mul ||
                              Op == simd_code::i64x2_extmul_low_i32x4_s || Op == simd_code::i64x2_extmul_high_i32x4_s ||
                              Op == simd_code::i64x2_extmul_low_i32x4_u || Op == simd_code::i64x2_extmul_high_i32x4_u)
            {
                if constexpr(Op == simd_code::i64x2_add || Op == simd_code::i64x2_sub || Op == simd_code::i64x2_mul)
                {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                    auto const l{v128_to_vec<v128_u64x2>(lhs)};
                    auto const r{v128_to_vec<v128_u64x2>(rhs)};
                    if constexpr(Op == simd_code::i64x2_add) { return vec_to_v128(l + r); }
                    else if constexpr(Op == simd_code::i64x2_sub) { return vec_to_v128(l - r); }
                    else if constexpr(Op == simd_code::i64x2_mul) { return vec_to_v128(l * r); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i64x2 binary opcode");
                    }
# else
                    auto l{load_uint_lanes<u64, 2uz>(lhs)};
                    auto const r{load_uint_lanes<u64, 2uz>(rhs)};
                    for(::std::size_t i{}; i != 2uz; ++i)
                    {
                        if constexpr(Op == simd_code::i64x2_add) { l.lane[i] = static_cast<u64>(l.lane[i] + r.lane[i]); }
                        else if constexpr(Op == simd_code::i64x2_sub) { l.lane[i] = static_cast<u64>(l.lane[i] - r.lane[i]); }
                        else if constexpr(Op == simd_code::i64x2_mul) { l.lane[i] = static_cast<u64>(l.lane[i] * r.lane[i]); }
                        else
                        {
                            static_assert(dependent_false_v<Op>, "unhandled i64x2 binary opcode");
                        }
                    }
                    return store_uint_lanes<u64, 2uz>(l);
# endif
                }
                else if constexpr(Op == simd_code::i64x2_extmul_low_i32x4_s || Op == simd_code::i64x2_extmul_high_i32x4_s ||
                                  Op == simd_code::i64x2_extmul_low_i32x4_u || Op == simd_code::i64x2_extmul_high_i32x4_u)
                {
                    auto const l{load_uint_lanes<u32, 4uz>(lhs)};
                    auto const r{load_uint_lanes<u32, 4uz>(rhs)};
                    lane_array<u64, 2uz> out{};  // init
                    constexpr ::std::size_t begin{(Op == simd_code::i64x2_extmul_high_i32x4_s || Op == simd_code::i64x2_extmul_high_i32x4_u) ? 2uz : 0uz};
                    for(::std::size_t i{}; i != 2uz; ++i)
                    {
                        if constexpr(Op == simd_code::i64x2_extmul_low_i32x4_s || Op == simd_code::i64x2_extmul_high_i32x4_s)
                        {
                            out.lane[i] = from_signed_lane<u64>(static_cast<s64>(static_cast<::std::int_least64_t>(as_signed_lane<u32>(l.lane[begin + i])) *
                                                                                 static_cast<::std::int_least64_t>(as_signed_lane<u32>(r.lane[begin + i]))));
                        }
                        else if constexpr(Op == simd_code::i64x2_extmul_low_i32x4_u || Op == simd_code::i64x2_extmul_high_i32x4_u)
                        {
                            out.lane[i] = static_cast<u64>(static_cast<u64>(l.lane[begin + i]) * static_cast<u64>(r.lane[begin + i]));
                        }
                        else
                        {
                            static_assert(dependent_false_v<Op>, "unhandled i64x2 extmul opcode");
                        }
                    }
                    return store_uint_lanes<u64, 2uz>(out);
                }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled i64x2 binary opcode");
                }
            }
            else if constexpr(Op == simd_code::f32x4_add || Op == simd_code::f32x4_sub || Op == simd_code::f32x4_mul || Op == simd_code::f32x4_div ||
                              Op == simd_code::f32x4_min || Op == simd_code::f32x4_max || Op == simd_code::f32x4_pmin || Op == simd_code::f32x4_pmax)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && !UWVM2_STRICT_FLOAT_EXTENDED
                if constexpr(Op == simd_code::f32x4_add || Op == simd_code::f32x4_sub || Op == simd_code::f32x4_mul || Op == simd_code::f32x4_div ||
                             Op == simd_code::f32x4_pmin || Op == simd_code::f32x4_pmax)
                {
                    auto const l{v128_to_vec<v128_f32x4>(lhs)};
                    auto const r{v128_to_vec<v128_f32x4>(rhs)};
                    if constexpr(Op == simd_code::f32x4_add) { return canonicalize_native_float_lanes<u32, 4uz>(vec_to_v128(l + r)); }
                    else if constexpr(Op == simd_code::f32x4_sub) { return canonicalize_native_float_lanes<u32, 4uz>(vec_to_v128(l - r)); }
                    else if constexpr(Op == simd_code::f32x4_mul) { return canonicalize_native_float_lanes<u32, 4uz>(vec_to_v128(l * r)); }
                    else if constexpr(Op == simd_code::f32x4_div) { return canonicalize_native_float_lanes<u32, 4uz>(vec_to_v128(l / r)); }
                    else if constexpr(Op == simd_code::f32x4_pmin || Op == simd_code::f32x4_pmax)
                    {
                        auto const lb{::std::bit_cast<v128_u32x4>(l)};
                        auto const rb{::std::bit_cast<v128_u32x4>(r)};
                        if constexpr(Op == simd_code::f32x4_pmin) { return vec_to_v128(vec_select(rb, lb, r < l)); }
                        else if constexpr(Op == simd_code::f32x4_pmax) { return vec_to_v128(vec_select(rb, lb, l < r)); }
                        else
                        {
                            static_assert(dependent_false_v<Op>, "unhandled f32x4 pseudo-minmax opcode");
                        }
                    }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled f32x4 vector binary opcode");
                    }
                }
# endif
                if constexpr(Op == simd_code::f32x4_pmin || Op == simd_code::f32x4_pmax)
                {
                    auto l{load_uint_lanes<u32, 4uz>(lhs)};
                    auto const r{load_uint_lanes<u32, 4uz>(rhs)};
                    for(::std::size_t i{}; i != 4uz; ++i)
                    {
                        auto const a{::std::bit_cast<wasm_f32>(l.lane[i])};
                        auto const b{::std::bit_cast<wasm_f32>(r.lane[i])};
                        bool const take_rhs{Op == simd_code::f32x4_pmin ? b < a : a < b};
                        if(take_rhs) { l.lane[i] = r.lane[i]; }
                    }
                    return store_uint_lanes<u32, 4uz>(l);
                }
                auto l{load_f32x4_lanes(lhs)};
                auto const r{load_f32x4_lanes(rhs)};
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    if constexpr(Op == simd_code::f32x4_add) { l.lane[i] = strict_float::binary<strict_float::operation::add>(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f32x4_sub) { l.lane[i] = strict_float::binary<strict_float::operation::sub>(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f32x4_mul) { l.lane[i] = strict_float::binary<strict_float::operation::mul>(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f32x4_div) { l.lane[i] = strict_float::binary<strict_float::operation::div>(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f32x4_min) { l.lane[i] = wasm_float_min(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f32x4_max) { l.lane[i] = wasm_float_max(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f32x4_pmin) { l.lane[i] = wasm_float_pmin(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f32x4_pmax) { l.lane[i] = wasm_float_pmax(l.lane[i], r.lane[i]); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled f32x4 binary opcode");
                    }
                }
                return store_f32x4_lanes(l);
            }
            else if constexpr(Op == simd_code::f64x2_add || Op == simd_code::f64x2_sub || Op == simd_code::f64x2_mul || Op == simd_code::f64x2_div ||
                              Op == simd_code::f64x2_min || Op == simd_code::f64x2_max || Op == simd_code::f64x2_pmin || Op == simd_code::f64x2_pmax)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && !UWVM2_STRICT_FLOAT_EXTENDED
                if constexpr(Op == simd_code::f64x2_add || Op == simd_code::f64x2_sub || Op == simd_code::f64x2_mul || Op == simd_code::f64x2_div ||
                             Op == simd_code::f64x2_pmin || Op == simd_code::f64x2_pmax)
                {
                    auto const l{v128_to_vec<v128_f64x2>(lhs)};
                    auto const r{v128_to_vec<v128_f64x2>(rhs)};
                    if constexpr(Op == simd_code::f64x2_add) { return canonicalize_native_float_lanes<u64, 2uz>(vec_to_v128(l + r)); }
                    else if constexpr(Op == simd_code::f64x2_sub) { return canonicalize_native_float_lanes<u64, 2uz>(vec_to_v128(l - r)); }
                    else if constexpr(Op == simd_code::f64x2_mul) { return canonicalize_native_float_lanes<u64, 2uz>(vec_to_v128(l * r)); }
                    else if constexpr(Op == simd_code::f64x2_div) { return canonicalize_native_float_lanes<u64, 2uz>(vec_to_v128(l / r)); }
                    else if constexpr(Op == simd_code::f64x2_pmin || Op == simd_code::f64x2_pmax)
                    {
                        auto const lb{::std::bit_cast<v128_u64x2>(l)};
                        auto const rb{::std::bit_cast<v128_u64x2>(r)};
                        if constexpr(Op == simd_code::f64x2_pmin) { return vec_to_v128(vec_select(rb, lb, r < l)); }
                        else if constexpr(Op == simd_code::f64x2_pmax) { return vec_to_v128(vec_select(rb, lb, l < r)); }
                        else
                        {
                            static_assert(dependent_false_v<Op>, "unhandled f64x2 pseudo-minmax opcode");
                        }
                    }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled f64x2 vector binary opcode");
                    }
                }
# endif
                if constexpr(Op == simd_code::f64x2_pmin || Op == simd_code::f64x2_pmax)
                {
                    auto l{load_uint_lanes<u64, 2uz>(lhs)};
                    auto const r{load_uint_lanes<u64, 2uz>(rhs)};
                    for(::std::size_t i{}; i != 2uz; ++i)
                    {
                        auto const a{::std::bit_cast<wasm_f64>(l.lane[i])};
                        auto const b{::std::bit_cast<wasm_f64>(r.lane[i])};
                        bool const take_rhs{Op == simd_code::f64x2_pmin ? b < a : a < b};
                        if(take_rhs) { l.lane[i] = r.lane[i]; }
                    }
                    return store_uint_lanes<u64, 2uz>(l);
                }
                auto l{load_f64x2_lanes(lhs)};
                auto const r{load_f64x2_lanes(rhs)};
                for(::std::size_t i{}; i != 2uz; ++i)
                {
                    if constexpr(Op == simd_code::f64x2_add) { l.lane[i] = strict_float::binary<strict_float::operation::add>(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f64x2_sub) { l.lane[i] = strict_float::binary<strict_float::operation::sub>(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f64x2_mul) { l.lane[i] = strict_float::binary<strict_float::operation::mul>(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f64x2_div) { l.lane[i] = strict_float::binary<strict_float::operation::div>(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f64x2_min) { l.lane[i] = wasm_float_min(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f64x2_max) { l.lane[i] = wasm_float_max(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f64x2_pmin) { l.lane[i] = wasm_float_pmin(l.lane[i], r.lane[i]); }
                    else if constexpr(Op == simd_code::f64x2_pmax) { l.lane[i] = wasm_float_pmax(l.lane[i], r.lane[i]); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled f64x2 binary opcode");
                    }
                }
                return store_f64x2_lanes(l);
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled SIMD binary opcode");
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_v128 eval_full_shift(wasm_v128 lhs, wasm_i32 rhs) noexcept
        {
            if constexpr(Op == simd_code::i8x16_shl || Op == simd_code::i8x16_shr_s || Op == simd_code::i8x16_shr_u)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                constexpr unsigned bits{8u};
                auto const sh{static_cast<unsigned>(::std::bit_cast<u32>(rhs)) & (bits - 1u)};
                auto const lanes{v128_to_vec<v128_u8x16>(lhs)};
                if constexpr(Op == simd_code::i8x16_shl) { return vec_to_v128(static_cast<v128_u8x16>(lanes << sh)); }
                else if constexpr(Op == simd_code::i8x16_shr_u) { return vec_to_v128(static_cast<v128_u8x16>(lanes >> sh)); }
                else if constexpr(Op == simd_code::i8x16_shr_s) { return vec_to_v128(::std::bit_cast<v128_u8x16>(::std::bit_cast<v128_i8x16>(lanes) >> sh)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled i8x16 shift opcode");
                }
# else
                auto lanes{load_uint_lanes<u8, 16uz>(lhs)};
                for(auto& lane: lanes.lane)
                {
                    if constexpr(Op == simd_code::i8x16_shl) { lane = shl_lane(lane, rhs); }
                    else if constexpr(Op == simd_code::i8x16_shr_s) { lane = shr_s_lane(lane, rhs); }
                    else if constexpr(Op == simd_code::i8x16_shr_u) { lane = shr_u_lane(lane, rhs); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i8x16 shift opcode");
                    }
                }
                return store_uint_lanes<u8, 16uz>(lanes);
# endif
            }
            else if constexpr(Op == simd_code::i16x8_shl || Op == simd_code::i16x8_shr_s || Op == simd_code::i16x8_shr_u)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                constexpr unsigned bits{16u};
                auto const sh{static_cast<unsigned>(::std::bit_cast<u32>(rhs)) & (bits - 1u)};
                auto const lanes{v128_to_vec<v128_u16x8>(lhs)};
                if constexpr(Op == simd_code::i16x8_shl) { return vec_to_v128(static_cast<v128_u16x8>(lanes << sh)); }
                else if constexpr(Op == simd_code::i16x8_shr_u) { return vec_to_v128(static_cast<v128_u16x8>(lanes >> sh)); }
                else if constexpr(Op == simd_code::i16x8_shr_s) { return vec_to_v128(::std::bit_cast<v128_u16x8>(::std::bit_cast<v128_i16x8>(lanes) >> sh)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled i16x8 shift opcode");
                }
# else
                auto lanes{load_uint_lanes<u16, 8uz>(lhs)};
                for(auto& lane: lanes.lane)
                {
                    if constexpr(Op == simd_code::i16x8_shl) { lane = shl_lane(lane, rhs); }
                    else if constexpr(Op == simd_code::i16x8_shr_s) { lane = shr_s_lane(lane, rhs); }
                    else if constexpr(Op == simd_code::i16x8_shr_u) { lane = shr_u_lane(lane, rhs); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i16x8 shift opcode");
                    }
                }
                return store_uint_lanes<u16, 8uz>(lanes);
# endif
            }
            else if constexpr(Op == simd_code::i32x4_shl || Op == simd_code::i32x4_shr_s || Op == simd_code::i32x4_shr_u)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                constexpr unsigned bits{32u};
                auto const sh{static_cast<unsigned>(::std::bit_cast<u32>(rhs)) & (bits - 1u)};
                auto const lanes{v128_to_vec<v128_u32x4>(lhs)};
                if constexpr(Op == simd_code::i32x4_shl) { return vec_to_v128(static_cast<v128_u32x4>(lanes << sh)); }
                else if constexpr(Op == simd_code::i32x4_shr_u) { return vec_to_v128(static_cast<v128_u32x4>(lanes >> sh)); }
                else if constexpr(Op == simd_code::i32x4_shr_s) { return vec_to_v128(::std::bit_cast<v128_u32x4>(::std::bit_cast<v128_i32x4>(lanes) >> sh)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled i32x4 shift opcode");
                }
# else
                auto lanes{load_uint_lanes<u32, 4uz>(lhs)};
                for(auto& lane: lanes.lane)
                {
                    if constexpr(Op == simd_code::i32x4_shl) { lane = shl_lane(lane, rhs); }
                    else if constexpr(Op == simd_code::i32x4_shr_s) { lane = shr_s_lane(lane, rhs); }
                    else if constexpr(Op == simd_code::i32x4_shr_u) { lane = shr_u_lane(lane, rhs); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i32x4 shift opcode");
                    }
                }
                return store_uint_lanes<u32, 4uz>(lanes);
# endif
            }
            else if constexpr(Op == simd_code::i64x2_shl || Op == simd_code::i64x2_shr_s || Op == simd_code::i64x2_shr_u)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                constexpr unsigned bits{64u};
                auto const sh{static_cast<unsigned>(::std::bit_cast<u32>(rhs)) & (bits - 1u)};
                auto const lanes{v128_to_vec<v128_u64x2>(lhs)};
                if constexpr(Op == simd_code::i64x2_shl) { return vec_to_v128(static_cast<v128_u64x2>(lanes << sh)); }
                else if constexpr(Op == simd_code::i64x2_shr_u) { return vec_to_v128(static_cast<v128_u64x2>(lanes >> sh)); }
                else if constexpr(Op == simd_code::i64x2_shr_s) { return vec_to_v128(::std::bit_cast<v128_u64x2>(::std::bit_cast<v128_i64x2>(lanes) >> sh)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled i64x2 shift opcode");
                }
# else
                auto lanes{load_uint_lanes<u64, 2uz>(lhs)};
                for(auto& lane: lanes.lane)
                {
                    if constexpr(Op == simd_code::i64x2_shl) { lane = shl_lane(lane, rhs); }
                    else if constexpr(Op == simd_code::i64x2_shr_s) { lane = shr_s_lane(lane, rhs); }
                    else if constexpr(Op == simd_code::i64x2_shr_u) { lane = shr_u_lane(lane, rhs); }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled i64x2 shift opcode");
                    }
                }
                return store_uint_lanes<u64, 2uz>(lanes);
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled SIMD shift opcode");
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr wasm_i32 eval_full_test(wasm_v128 v) noexcept
        {
            if constexpr(Op == simd_code::v128_any_true) { return eval_v128_testop<v128_testop::any_true>(v); }
            else if constexpr(Op == simd_code::i8x16_all_true)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                return vec_all_lanes_nonzero(v128_to_vec<v128_u8x16>(v)) ? wasm_i32{1} : wasm_i32{};
# else
                auto const lanes{load_uint_lanes<u8, 16uz>(v)};
                for(auto lane: lanes.lane)
                {
                    if(lane == 0u) { return wasm_i32{}; }
                }
                return wasm_i32{1};
# endif
            }
            else if constexpr(Op == simd_code::i16x8_all_true)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                return vec_all_lanes_nonzero(v128_to_vec<v128_u16x8>(v)) ? wasm_i32{1} : wasm_i32{};
# else
                auto const lanes{load_uint_lanes<u16, 8uz>(v)};
                for(auto lane: lanes.lane)
                {
                    if(lane == 0u) { return wasm_i32{}; }
                }
                return wasm_i32{1};
# endif
            }
            else if constexpr(Op == simd_code::i32x4_all_true) { return eval_v128_testop<v128_testop::i32x4_all_true>(v); }
            else if constexpr(Op == simd_code::i64x2_all_true)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                return vec_all_lanes_nonzero(v128_to_vec<v128_u64x2>(v)) ? wasm_i32{1} : wasm_i32{};
# else
                auto const lanes{load_uint_lanes<u64, 2uz>(v)};
                for(auto lane: lanes.lane)
                {
                    if(lane == 0u) { return wasm_i32{}; }
                }
                return wasm_i32{1};
# endif
            }
            else if constexpr(Op == simd_code::i8x16_bitmask)
            {
# if defined(__SSE2__) && UWVM_HAS_BUILTIN(__builtin_ia32_pmovmskb128) && UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                return ::std::bit_cast<wasm_i32>(static_cast<u32>(__builtin_ia32_pmovmskb128(::std::bit_cast<v128_c8x16>(v128_to_vec<v128_u8x16>(v)))));
# elif defined(__loongarch_sx) && UWVM_HAS_BUILTIN(__builtin_lsx_vmskltz_b) && UWVM_HAS_BUILTIN(__builtin_lsx_vpickve2gr_hu) &&                                \
     UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                // LSX vmskltz.b packs the byte sign bits into the low halfword, matching Wasm lane order on little-endian targets.
                auto const mask{__builtin_lsx_vmskltz_b(v128_to_vec<v128_i8x16>(v))};
                return ::std::bit_cast<wasm_i32>(static_cast<u32>(__builtin_lsx_vpickve2gr_hu(::std::bit_cast<v128_u16x8>(mask), 0)));
# elif defined(__clang__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&                                  \
     UWVM_HAS_BUILTIN(__builtin_neon_vaddv_u8) && UWVM_HAS_BUILTIN(__builtin_shufflevector) && UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) &&             \
     defined(__LITTLE_ENDIAN__)
                constexpr v128_u8x16 weights{1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u};
                auto const sign_bytes{::std::bit_cast<v128_u8x16>(v128_to_vec<v128_i8x16>(v) >> 7u)};
                auto const weighted{sign_bytes & weights};
                auto const low{__builtin_shufflevector(weighted, weighted, 0, 1, 2, 3, 4, 5, 6, 7)};
                auto const high{__builtin_shufflevector(weighted, weighted, 8, 9, 10, 11, 12, 13, 14, 15)};
                return ::std::bit_cast<wasm_i32>(static_cast<u32>(__builtin_neon_vaddv_u8(low)) | (static_cast<u32>(__builtin_neon_vaddv_u8(high)) << 8u));
# elif !defined(__clang__) && defined(__GNUC__) && (defined(__aarch64__) || defined(_M_ARM64)) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) &&            \
     UWVM_HAS_BUILTIN(__builtin_aarch64_reduc_plus_scal_v8qi_uu) && UWVM_HAS_BUILTIN(__builtin_shufflevector) &&                                               \
     UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                constexpr v128_u8x16 weights{1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u};
                auto const sign_bytes{::std::bit_cast<v128_u8x16>(v128_to_vec<v128_i8x16>(v) >> 7u)};
                auto const weighted{sign_bytes & weights};
                auto const low{__builtin_shufflevector(weighted, weighted, 0, 1, 2, 3, 4, 5, 6, 7)};
                auto const high{__builtin_shufflevector(weighted, weighted, 8, 9, 10, 11, 12, 13, 14, 15)};
                return ::std::bit_cast<wasm_i32>(static_cast<u32>(__builtin_aarch64_reduc_plus_scal_v8qi_uu(low)) |
                                                 (static_cast<u32>(__builtin_aarch64_reduc_plus_scal_v8qi_uu(high)) << 8u));
# else
#  if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                auto const lanes{static_cast<v128_u8x16>(v128_to_vec<v128_u8x16>(v) >> 7u)};
#  else
                auto const lanes{load_uint_lanes<u8, 16uz>(v)};
#  endif
                u32 out{};
                for(::std::size_t i{}; i != 16uz; ++i)
                {
#  if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                    out |= static_cast<u32>(lanes[i] & 1u) << i;
#  else
                    out |= static_cast<u32>((lanes.lane[i] >> 7u) & 1u) << i;
#  endif
                }
                return ::std::bit_cast<wasm_i32>(out);
# endif
            }
            else if constexpr(Op == simd_code::i16x8_bitmask)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                auto const lanes{static_cast<v128_u16x8>(v128_to_vec<v128_u16x8>(v) >> 15u)};
# else
                auto const lanes{load_uint_lanes<u16, 8uz>(v)};
# endif
                u32 out{};
                for(::std::size_t i{}; i != 8uz; ++i)
                {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                    out |= static_cast<u32>(lanes[i] & 1u) << i;
# else
                    out |= static_cast<u32>((lanes.lane[i] >> 15u) & 1u) << i;
# endif
                }
                return ::std::bit_cast<wasm_i32>(out);
            }
            else if constexpr(Op == simd_code::i32x4_bitmask)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                auto const lanes{static_cast<v128_u32x4>(v128_to_vec<v128_u32x4>(v) >> 31u)};
# else
                auto const lanes{load_uint_lanes<u32, 4uz>(v)};
# endif
                u32 out{};
                for(::std::size_t i{}; i != 4uz; ++i)
                {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                    out |= static_cast<u32>(lanes[i] & 1u) << i;
# else
                    out |= static_cast<u32>((lanes.lane[i] >> 31u) & 1u) << i;
# endif
                }
                return ::std::bit_cast<wasm_i32>(out);
            }
            else if constexpr(Op == simd_code::i64x2_bitmask)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                auto const lanes{static_cast<v128_u64x2>(v128_to_vec<v128_u64x2>(v) >> 63u)};
# else
                auto const lanes{load_uint_lanes<u64, 2uz>(v)};
# endif
                u32 out{};
                for(::std::size_t i{}; i != 2uz; ++i)
                {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                    out |= static_cast<u32>(lanes[i] & 1u) << i;
# else
                    out |= static_cast<u32>((lanes.lane[i] >> 63u) & 1u) << i;
# endif
                }
                return ::std::bit_cast<wasm_i32>(out);
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled SIMD test opcode");
            }
        }

        template <typename U>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline U read_memory_lane(::std::byte const* p) noexcept
        {
            U v;  // no init
            ::std::memcpy(::std::addressof(v), p, sizeof(v));
            return ::fast_io::little_endian(v);
        }

        template <typename U>
        UWVM_ALWAYS_INLINE inline void write_memory_lane(::std::byte* p, U v) noexcept
        {
            auto le{::fast_io::little_endian(v)};
            ::std::memcpy(p, ::std::addressof(le), sizeof(le));
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::size_t simd_memory_access_size() noexcept
        {
            if constexpr(Op == simd_code::v128_load || Op == simd_code::v128_store) { return 16uz; }
            else if constexpr(Op == simd_code::v128_load8x8_s || Op == simd_code::v128_load8x8_u || Op == simd_code::v128_load16x4_s ||
                              Op == simd_code::v128_load16x4_u || Op == simd_code::v128_load32x2_s || Op == simd_code::v128_load32x2_u ||
                              Op == simd_code::v128_load64_splat || Op == simd_code::v128_load64_zero || Op == simd_code::v128_load64_lane ||
                              Op == simd_code::v128_store64_lane)
            {
                return 8uz;
            }
            else if constexpr(Op == simd_code::v128_load32_splat || Op == simd_code::v128_load32_zero || Op == simd_code::v128_load32_lane ||
                              Op == simd_code::v128_store32_lane)
            {
                return 4uz;
            }
            else if constexpr(Op == simd_code::v128_load16_splat || Op == simd_code::v128_load16_lane || Op == simd_code::v128_store16_lane) { return 2uz; }
            else if constexpr(Op == simd_code::v128_load8_splat || Op == simd_code::v128_load8_lane || Op == simd_code::v128_store8_lane) { return 1uz; }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled SIMD memory opcode");
            }
        }

        template <simd_code Op>
        [[nodiscard]] UWVM_ALWAYS_INLINE inline wasm_v128 eval_memory_load(::std::byte const* p, wasm_v128 old, u8 lane) noexcept
        {
            if constexpr(Op == simd_code::v128_load)
            {
                wasm_v128 out;  // no init
                ::std::memcpy(::std::addressof(out), p, sizeof(out));
                return out;
            }
            else if constexpr(Op == simd_code::v128_load8x8_s || Op == simd_code::v128_load8x8_u)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && UWVM_HAS_BUILTIN(__builtin_convertvector)
                v64_u8x8 in;  // no init
                ::std::memcpy(::std::addressof(in), p, sizeof(in));
                if constexpr(Op == simd_code::v128_load8x8_s)
                {
                    return vec_to_v128(::std::bit_cast<v128_u16x8>(__builtin_convertvector(::std::bit_cast<v64_i8x8>(in), v128_i16x8)));
                }
                else if constexpr(Op == simd_code::v128_load8x8_u) { return vec_to_v128(__builtin_convertvector(in, v128_u16x8)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled v128 load8x8 opcode");
                }
# else
                lane_array<u16, 8uz> out{};  // init
                for(::std::size_t i{}; i != 8uz; ++i)
                {
                    auto const x{read_memory_lane<u8>(p + i)};
                    if constexpr(Op == simd_code::v128_load8x8_s) { out.lane[i] = from_signed_lane<u16>(static_cast<s16>(as_signed_lane<u8>(x))); }
                    else if constexpr(Op == simd_code::v128_load8x8_u) { out.lane[i] = x; }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled v128 load8x8 opcode");
                    }
                }
                return store_uint_lanes<u16, 8uz>(out);
# endif
            }
            else if constexpr(Op == simd_code::v128_load16x4_s || Op == simd_code::v128_load16x4_u)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && UWVM_HAS_BUILTIN(__builtin_convertvector)
                v64_u16x4 in;  // no init
                ::std::memcpy(::std::addressof(in), p, sizeof(in));
                if constexpr(Op == simd_code::v128_load16x4_s)
                {
                    return vec_to_v128(::std::bit_cast<v128_u32x4>(__builtin_convertvector(::std::bit_cast<v64_i16x4>(in), v128_i32x4)));
                }
                else if constexpr(Op == simd_code::v128_load16x4_u) { return vec_to_v128(__builtin_convertvector(in, v128_u32x4)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled v128 load16x4 opcode");
                }
# else
                lane_array<u32, 4uz> out{};  // init
                for(::std::size_t i{}; i != 4uz; ++i)
                {
                    auto const x{read_memory_lane<u16>(p + i * 2uz)};
                    if constexpr(Op == simd_code::v128_load16x4_s) { out.lane[i] = from_signed_lane<u32>(static_cast<s32>(as_signed_lane<u16>(x))); }
                    else if constexpr(Op == simd_code::v128_load16x4_u) { out.lane[i] = x; }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled v128 load16x4 opcode");
                    }
                }
                return store_uint_lanes<u32, 4uz>(out);
# endif
            }
            else if constexpr(Op == simd_code::v128_load32x2_s || Op == simd_code::v128_load32x2_u)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__) && UWVM_HAS_BUILTIN(__builtin_convertvector)
                v64_u32x2 in;  // no init
                ::std::memcpy(::std::addressof(in), p, sizeof(in));
                if constexpr(Op == simd_code::v128_load32x2_s)
                {
                    return vec_to_v128(::std::bit_cast<v128_u64x2>(__builtin_convertvector(::std::bit_cast<v64_i32x2>(in), v128_i64x2)));
                }
                else if constexpr(Op == simd_code::v128_load32x2_u) { return vec_to_v128(__builtin_convertvector(in, v128_u64x2)); }
                else
                {
                    static_assert(dependent_false_v<Op>, "unhandled v128 load32x2 opcode");
                }
# else
                lane_array<u64, 2uz> out{};  // init
                for(::std::size_t i{}; i != 2uz; ++i)
                {
                    auto const x{read_memory_lane<u32>(p + i * 4uz)};
                    if constexpr(Op == simd_code::v128_load32x2_s) { out.lane[i] = from_signed_lane<u64>(static_cast<s64>(as_signed_lane<u32>(x))); }
                    else if constexpr(Op == simd_code::v128_load32x2_u) { out.lane[i] = x; }
                    else
                    {
                        static_assert(dependent_false_v<Op>, "unhandled v128 load32x2 opcode");
                    }
                }
                return store_uint_lanes<u64, 2uz>(out);
# endif
            }
            else if constexpr(Op == simd_code::v128_load8_splat)
            {
                auto const x{read_memory_lane<u8>(p)};
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                return vec_to_v128(vec_splat<v128_u8x16>(x));
# else
                lane_array<u8, 16uz> out{};  // init
                for(auto& v: out.lane) { v = x; }
                return store_uint_lanes<u8, 16uz>(out);
# endif
            }
            else if constexpr(Op == simd_code::v128_load16_splat)
            {
                auto const x{read_memory_lane<u16>(p)};
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                return vec_to_v128(vec_splat<v128_u16x8>(x));
# else
                lane_array<u16, 8uz> out{};  // init
                for(auto& v: out.lane) { v = x; }
                return store_uint_lanes<u16, 8uz>(out);
# endif
            }
            else if constexpr(Op == simd_code::v128_load32_splat)
            {
                auto const x{read_memory_lane<u32>(p)};
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                return vec_to_v128(vec_splat<v128_u32x4>(x));
# else
                lane_array<u32, 4uz> out{};  // init
                for(auto& v: out.lane) { v = x; }
                return store_uint_lanes<u32, 4uz>(out);
# endif
            }
            else if constexpr(Op == simd_code::v128_load64_splat)
            {
                auto const x{read_memory_lane<u64>(p)};
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                return vec_to_v128(vec_splat<v128_u64x2>(x));
# else
                lane_array<u64, 2uz> out{};  // init
                for(auto& v: out.lane) { v = x; }
                return store_uint_lanes<u64, 2uz>(out);
# endif
            }
            else if constexpr(Op == simd_code::v128_load32_zero)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                v128_u32x4 out{};  // init
                out[0] = read_memory_lane<u32>(p);
                return vec_to_v128(out);
# else
                lane_array<u32, 4uz> out{};  // init
                out.lane[0] = read_memory_lane<u32>(p);
                return store_uint_lanes<u32, 4uz>(out);
# endif
            }
            else if constexpr(Op == simd_code::v128_load64_zero)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                v128_u64x2 out{};  // init
                out[0] = read_memory_lane<u64>(p);
                return vec_to_v128(out);
# else
                lane_array<u64, 2uz> out{};  // init
                out.lane[0] = read_memory_lane<u64>(p);
                return store_uint_lanes<u64, 2uz>(out);
# endif
            }
            else if constexpr(Op == simd_code::v128_load8_lane)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                auto out{v128_to_vec<v128_u8x16>(old)};
                out[lane] = read_memory_lane<u8>(p);
                return vec_to_v128(out);
# else
                auto out{load_uint_lanes<u8, 16uz>(old)};
                out.lane[lane] = read_memory_lane<u8>(p);
                return store_uint_lanes<u8, 16uz>(out);
# endif
            }
            else if constexpr(Op == simd_code::v128_load16_lane)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                auto out{v128_to_vec<v128_u16x8>(old)};
                out[lane] = read_memory_lane<u16>(p);
                return vec_to_v128(out);
# else
                auto out{load_uint_lanes<u16, 8uz>(old)};
                out.lane[lane] = read_memory_lane<u16>(p);
                return store_uint_lanes<u16, 8uz>(out);
# endif
            }
            else if constexpr(Op == simd_code::v128_load32_lane)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                auto out{v128_to_vec<v128_u32x4>(old)};
                out[lane] = read_memory_lane<u32>(p);
                return vec_to_v128(out);
# else
                auto out{load_uint_lanes<u32, 4uz>(old)};
                out.lane[lane] = read_memory_lane<u32>(p);
                return store_uint_lanes<u32, 4uz>(out);
# endif
            }
            else if constexpr(Op == simd_code::v128_load64_lane)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                auto out{v128_to_vec<v128_u64x2>(old)};
                out[lane] = read_memory_lane<u64>(p);
                return vec_to_v128(out);
# else
                auto out{load_uint_lanes<u64, 2uz>(old)};
                out.lane[lane] = read_memory_lane<u64>(p);
                return store_uint_lanes<u64, 2uz>(out);
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled SIMD memory load opcode");
            }
        }

        template <simd_code Op>
        UWVM_ALWAYS_INLINE inline void eval_memory_store(::std::byte* p, wasm_v128 value, u8 lane) noexcept
        {
            if constexpr(Op == simd_code::v128_store) { ::std::memcpy(p, ::std::addressof(value), sizeof(value)); }
            else if constexpr(Op == simd_code::v128_store8_lane)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                write_memory_lane(p, v128_to_vec<v128_u8x16>(value)[lane]);
# else
                auto const lanes{load_uint_lanes<u8, 16uz>(value)};
                write_memory_lane(p, lanes.lane[lane]);
# endif
            }
            else if constexpr(Op == simd_code::v128_store16_lane)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                write_memory_lane(p, v128_to_vec<v128_u16x8>(value)[lane]);
# else
                auto const lanes{load_uint_lanes<u16, 8uz>(value)};
                write_memory_lane(p, lanes.lane[lane]);
# endif
            }
            else if constexpr(Op == simd_code::v128_store32_lane)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                write_memory_lane(p, v128_to_vec<v128_u32x4>(value)[lane]);
# else
                auto const lanes{load_uint_lanes<u32, 4uz>(value)};
                write_memory_lane(p, lanes.lane[lane]);
# endif
            }
            else if constexpr(Op == simd_code::v128_store64_lane)
            {
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__vector_size__) && defined(__LITTLE_ENDIAN__)
                write_memory_lane(p, v128_to_vec<v128_u64x2>(value)[lane]);
# else
                auto const lanes{load_uint_lanes<u64, 2uz>(value)};
                write_memory_lane(p, lanes.lane[lane]);
# endif
            }
            else
            {
                static_assert(dependent_false_v<Op>, "unhandled SIMD memory store opcode");
            }
        }
    }  // namespace wasm1p1_simd_details

    using simd_code = wasm1p1_simd_details::simd_code;

    // One canonical opcode taxonomy is shared by LLVM validation, the fallback scanner, and typed SIMD lowering.
    // The 236 entries below are mechanically aligned with uwvm-int's complete SIMD dispatch; Op remains a template
    // argument so the LLVM path can bind a distinct typed C++ bridge for every instruction without a runtime opcode switch.
    enum class wasm1p1_simd_instruction_kind : ::std::uint_least8_t
    {
        constant,
        shuffle,
        splat,
        extract_lane,
        replace_lane,
        unary,
        binary,
        ternary,
        test,
        shift,
        memory_load,
        memory_store
    };

    enum class wasm1p1_simd_scalar_kind : ::std::uint_least8_t
    {
        none,
        i32,
        i64,
        f32,
        f64
    };

    template <typename Visitor>
    [[nodiscard]] inline constexpr bool visit_wasm1p1_simd_instruction(simd_code opcode, Visitor&& visitor)
    {
        switch(opcode)
        {
            case simd_code::v128_load:
                return visitor.template operator()<simd_code::v128_load,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   4u>();
            case simd_code::v128_load8x8_s:
                return visitor.template operator()<simd_code::v128_load8x8_s,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   3u>();
            case simd_code::v128_load8x8_u:
                return visitor.template operator()<simd_code::v128_load8x8_u,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   3u>();
            case simd_code::v128_load16x4_s:
                return visitor.template operator()<simd_code::v128_load16x4_s,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   3u>();
            case simd_code::v128_load16x4_u:
                return visitor.template operator()<simd_code::v128_load16x4_u,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   3u>();
            case simd_code::v128_load32x2_s:
                return visitor.template operator()<simd_code::v128_load32x2_s,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   3u>();
            case simd_code::v128_load32x2_u:
                return visitor.template operator()<simd_code::v128_load32x2_u,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   3u>();
            case simd_code::v128_load8_splat:
                return visitor.template operator()<simd_code::v128_load8_splat,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::v128_load16_splat:
                return visitor.template operator()<simd_code::v128_load16_splat,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   1u>();
            case simd_code::v128_load32_splat:
                return visitor.template operator()<simd_code::v128_load32_splat,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   2u>();
            case simd_code::v128_load64_splat:
                return visitor.template operator()<simd_code::v128_load64_splat,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   3u>();
            case simd_code::v128_store:
                return visitor.template operator()<simd_code::v128_store,
                                                   wasm1p1_simd_instruction_kind::memory_store,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   4u>();
            case simd_code::v128_load8_lane:
                return visitor.template operator()<simd_code::v128_load8_lane,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   16uz,
                                                   0u>();
            case simd_code::v128_load16_lane:
                return visitor.template operator()<simd_code::v128_load16_lane,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   8uz,
                                                   1u>();
            case simd_code::v128_load32_lane:
                return visitor.template operator()<simd_code::v128_load32_lane,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   4uz,
                                                   2u>();
            case simd_code::v128_load64_lane:
                return visitor.template operator()<simd_code::v128_load64_lane,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   2uz,
                                                   3u>();
            case simd_code::v128_store8_lane:
                return visitor.template operator()<simd_code::v128_store8_lane,
                                                   wasm1p1_simd_instruction_kind::memory_store,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   16uz,
                                                   0u>();
            case simd_code::v128_store16_lane:
                return visitor.template operator()<simd_code::v128_store16_lane,
                                                   wasm1p1_simd_instruction_kind::memory_store,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   8uz,
                                                   1u>();
            case simd_code::v128_store32_lane:
                return visitor.template operator()<simd_code::v128_store32_lane,
                                                   wasm1p1_simd_instruction_kind::memory_store,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   4uz,
                                                   2u>();
            case simd_code::v128_store64_lane:
                return visitor.template operator()<simd_code::v128_store64_lane,
                                                   wasm1p1_simd_instruction_kind::memory_store,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   2uz,
                                                   3u>();
            case simd_code::v128_load32_zero:
                return visitor.template operator()<simd_code::v128_load32_zero,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   2u>();
            case simd_code::v128_load64_zero:
                return visitor.template operator()<simd_code::v128_load64_zero,
                                                   wasm1p1_simd_instruction_kind::memory_load,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   3u>();
            case simd_code::v128_const:
                return visitor.template operator()<simd_code::v128_const,
                                                   wasm1p1_simd_instruction_kind::constant,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_shuffle:
                return visitor.template operator()<simd_code::i8x16_shuffle,
                                                   wasm1p1_simd_instruction_kind::shuffle,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   16uz,
                                                   0u>();
            case simd_code::i8x16_swizzle:
                return visitor.template operator()<simd_code::i8x16_swizzle,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_splat:
                return visitor.template operator()<simd_code::i8x16_splat,
                                                   wasm1p1_simd_instruction_kind::splat,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_splat:
                return visitor.template operator()<simd_code::i16x8_splat,
                                                   wasm1p1_simd_instruction_kind::splat,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_splat:
                return visitor.template operator()<simd_code::i32x4_splat,
                                                   wasm1p1_simd_instruction_kind::splat,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_splat:
                return visitor.template operator()<simd_code::i64x2_splat,
                                                   wasm1p1_simd_instruction_kind::splat,
                                                   wasm1p1_simd_scalar_kind::i64,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_splat:
                return visitor.template operator()<simd_code::f32x4_splat,
                                                   wasm1p1_simd_instruction_kind::splat,
                                                   wasm1p1_simd_scalar_kind::f32,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_splat:
                return visitor.template operator()<simd_code::f64x2_splat,
                                                   wasm1p1_simd_instruction_kind::splat,
                                                   wasm1p1_simd_scalar_kind::f64,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_extract_lane_s:
                return visitor.template operator()<simd_code::i8x16_extract_lane_s,
                                                   wasm1p1_simd_instruction_kind::extract_lane,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   16uz,
                                                   0u>();
            case simd_code::i8x16_extract_lane_u:
                return visitor.template operator()<simd_code::i8x16_extract_lane_u,
                                                   wasm1p1_simd_instruction_kind::extract_lane,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   16uz,
                                                   0u>();
            case simd_code::i8x16_replace_lane:
                return visitor.template operator()<simd_code::i8x16_replace_lane,
                                                   wasm1p1_simd_instruction_kind::replace_lane,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   16uz,
                                                   0u>();
            case simd_code::i16x8_extract_lane_s:
                return visitor.template operator()<simd_code::i16x8_extract_lane_s,
                                                   wasm1p1_simd_instruction_kind::extract_lane,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   8uz,
                                                   0u>();
            case simd_code::i16x8_extract_lane_u:
                return visitor.template operator()<simd_code::i16x8_extract_lane_u,
                                                   wasm1p1_simd_instruction_kind::extract_lane,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   8uz,
                                                   0u>();
            case simd_code::i16x8_replace_lane:
                return visitor.template operator()<simd_code::i16x8_replace_lane,
                                                   wasm1p1_simd_instruction_kind::replace_lane,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   8uz,
                                                   0u>();
            case simd_code::i32x4_extract_lane:
                return visitor.template operator()<simd_code::i32x4_extract_lane,
                                                   wasm1p1_simd_instruction_kind::extract_lane,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   4uz,
                                                   0u>();
            case simd_code::i32x4_replace_lane:
                return visitor.template operator()<simd_code::i32x4_replace_lane,
                                                   wasm1p1_simd_instruction_kind::replace_lane,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   4uz,
                                                   0u>();
            case simd_code::i64x2_extract_lane:
                return visitor.template operator()<simd_code::i64x2_extract_lane,
                                                   wasm1p1_simd_instruction_kind::extract_lane,
                                                   wasm1p1_simd_scalar_kind::i64,
                                                   2uz,
                                                   0u>();
            case simd_code::i64x2_replace_lane:
                return visitor.template operator()<simd_code::i64x2_replace_lane,
                                                   wasm1p1_simd_instruction_kind::replace_lane,
                                                   wasm1p1_simd_scalar_kind::i64,
                                                   2uz,
                                                   0u>();
            case simd_code::f32x4_extract_lane:
                return visitor.template operator()<simd_code::f32x4_extract_lane,
                                                   wasm1p1_simd_instruction_kind::extract_lane,
                                                   wasm1p1_simd_scalar_kind::f32,
                                                   4uz,
                                                   0u>();
            case simd_code::f32x4_replace_lane:
                return visitor.template operator()<simd_code::f32x4_replace_lane,
                                                   wasm1p1_simd_instruction_kind::replace_lane,
                                                   wasm1p1_simd_scalar_kind::f32,
                                                   4uz,
                                                   0u>();
            case simd_code::f64x2_extract_lane:
                return visitor.template operator()<simd_code::f64x2_extract_lane,
                                                   wasm1p1_simd_instruction_kind::extract_lane,
                                                   wasm1p1_simd_scalar_kind::f64,
                                                   2uz,
                                                   0u>();
            case simd_code::f64x2_replace_lane:
                return visitor.template operator()<simd_code::f64x2_replace_lane,
                                                   wasm1p1_simd_instruction_kind::replace_lane,
                                                   wasm1p1_simd_scalar_kind::f64,
                                                   2uz,
                                                   0u>();
            case simd_code::v128_not:
                return visitor.template operator()<simd_code::v128_not,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::v128_and:
                return visitor.template operator()<simd_code::v128_and,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::v128_andnot:
                return visitor.template operator()<simd_code::v128_andnot,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::v128_or:
                return visitor.template operator()<simd_code::v128_or,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::v128_xor:
                return visitor.template operator()<simd_code::v128_xor,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::v128_any_true:
                return visitor.template operator()<simd_code::v128_any_true,
                                                   wasm1p1_simd_instruction_kind::test,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::v128_bitselect:
                return visitor.template operator()<simd_code::v128_bitselect,
                                                   wasm1p1_simd_instruction_kind::ternary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_eq:
                return visitor.template operator()<simd_code::i8x16_eq,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_ne:
                return visitor.template operator()<simd_code::i8x16_ne,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_lt_s:
                return visitor.template operator()<simd_code::i8x16_lt_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_lt_u:
                return visitor.template operator()<simd_code::i8x16_lt_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_gt_s:
                return visitor.template operator()<simd_code::i8x16_gt_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_gt_u:
                return visitor.template operator()<simd_code::i8x16_gt_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_le_s:
                return visitor.template operator()<simd_code::i8x16_le_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_le_u:
                return visitor.template operator()<simd_code::i8x16_le_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_ge_s:
                return visitor.template operator()<simd_code::i8x16_ge_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_ge_u:
                return visitor.template operator()<simd_code::i8x16_ge_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_eq:
                return visitor.template operator()<simd_code::i16x8_eq,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_ne:
                return visitor.template operator()<simd_code::i16x8_ne,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_lt_s:
                return visitor.template operator()<simd_code::i16x8_lt_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_lt_u:
                return visitor.template operator()<simd_code::i16x8_lt_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_gt_s:
                return visitor.template operator()<simd_code::i16x8_gt_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_gt_u:
                return visitor.template operator()<simd_code::i16x8_gt_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_le_s:
                return visitor.template operator()<simd_code::i16x8_le_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_le_u:
                return visitor.template operator()<simd_code::i16x8_le_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_ge_s:
                return visitor.template operator()<simd_code::i16x8_ge_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_ge_u:
                return visitor.template operator()<simd_code::i16x8_ge_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_eq:
                return visitor.template operator()<simd_code::i32x4_eq,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_ne:
                return visitor.template operator()<simd_code::i32x4_ne,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_lt_s:
                return visitor.template operator()<simd_code::i32x4_lt_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_lt_u:
                return visitor.template operator()<simd_code::i32x4_lt_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_gt_s:
                return visitor.template operator()<simd_code::i32x4_gt_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_gt_u:
                return visitor.template operator()<simd_code::i32x4_gt_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_le_s:
                return visitor.template operator()<simd_code::i32x4_le_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_le_u:
                return visitor.template operator()<simd_code::i32x4_le_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_ge_s:
                return visitor.template operator()<simd_code::i32x4_ge_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_ge_u:
                return visitor.template operator()<simd_code::i32x4_ge_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_all_true:
                return visitor.template operator()<simd_code::i32x4_all_true,
                                                   wasm1p1_simd_instruction_kind::test,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_all_true:
                return visitor.template operator()<simd_code::i8x16_all_true,
                                                   wasm1p1_simd_instruction_kind::test,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_bitmask:
                return visitor.template operator()<simd_code::i8x16_bitmask,
                                                   wasm1p1_simd_instruction_kind::test,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_all_true:
                return visitor.template operator()<simd_code::i16x8_all_true,
                                                   wasm1p1_simd_instruction_kind::test,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_bitmask:
                return visitor.template operator()<simd_code::i16x8_bitmask,
                                                   wasm1p1_simd_instruction_kind::test,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_bitmask:
                return visitor.template operator()<simd_code::i32x4_bitmask,
                                                   wasm1p1_simd_instruction_kind::test,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_all_true:
                return visitor.template operator()<simd_code::i64x2_all_true,
                                                   wasm1p1_simd_instruction_kind::test,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_bitmask:
                return visitor.template operator()<simd_code::i64x2_bitmask,
                                                   wasm1p1_simd_instruction_kind::test,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_abs:
                return visitor.template operator()<simd_code::i8x16_abs,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_neg:
                return visitor.template operator()<simd_code::i8x16_neg,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_popcnt:
                return visitor.template operator()<simd_code::i8x16_popcnt,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_abs:
                return visitor.template operator()<simd_code::i16x8_abs,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_neg:
                return visitor.template operator()<simd_code::i16x8_neg,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_extend_low_i8x16_s:
                return visitor.template operator()<simd_code::i16x8_extend_low_i8x16_s,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_extend_high_i8x16_s:
                return visitor.template operator()<simd_code::i16x8_extend_high_i8x16_s,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_extend_low_i8x16_u:
                return visitor.template operator()<simd_code::i16x8_extend_low_i8x16_u,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_extend_high_i8x16_u:
                return visitor.template operator()<simd_code::i16x8_extend_high_i8x16_u,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_extadd_pairwise_i8x16_s:
                return visitor.template operator()<simd_code::i16x8_extadd_pairwise_i8x16_s,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_extadd_pairwise_i8x16_u:
                return visitor.template operator()<simd_code::i16x8_extadd_pairwise_i8x16_u,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_abs:
                return visitor.template operator()<simd_code::i32x4_abs,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_neg:
                return visitor.template operator()<simd_code::i32x4_neg,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_extend_low_i16x8_s:
                return visitor.template operator()<simd_code::i32x4_extend_low_i16x8_s,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_extend_high_i16x8_s:
                return visitor.template operator()<simd_code::i32x4_extend_high_i16x8_s,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_extend_low_i16x8_u:
                return visitor.template operator()<simd_code::i32x4_extend_low_i16x8_u,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_extend_high_i16x8_u:
                return visitor.template operator()<simd_code::i32x4_extend_high_i16x8_u,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_extadd_pairwise_i16x8_s:
                return visitor.template operator()<simd_code::i32x4_extadd_pairwise_i16x8_s,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_extadd_pairwise_i16x8_u:
                return visitor.template operator()<simd_code::i32x4_extadd_pairwise_i16x8_u,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_abs:
                return visitor.template operator()<simd_code::i64x2_abs,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_neg:
                return visitor.template operator()<simd_code::i64x2_neg,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_extend_low_i32x4_s:
                return visitor.template operator()<simd_code::i64x2_extend_low_i32x4_s,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_extend_high_i32x4_s:
                return visitor.template operator()<simd_code::i64x2_extend_high_i32x4_s,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_extend_low_i32x4_u:
                return visitor.template operator()<simd_code::i64x2_extend_low_i32x4_u,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_extend_high_i32x4_u:
                return visitor.template operator()<simd_code::i64x2_extend_high_i32x4_u,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_narrow_i16x8_s:
                return visitor.template operator()<simd_code::i8x16_narrow_i16x8_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_narrow_i16x8_u:
                return visitor.template operator()<simd_code::i8x16_narrow_i16x8_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_add:
                return visitor.template operator()<simd_code::i8x16_add,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_add_sat_s:
                return visitor.template operator()<simd_code::i8x16_add_sat_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_add_sat_u:
                return visitor.template operator()<simd_code::i8x16_add_sat_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_sub:
                return visitor.template operator()<simd_code::i8x16_sub,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_sub_sat_s:
                return visitor.template operator()<simd_code::i8x16_sub_sat_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_sub_sat_u:
                return visitor.template operator()<simd_code::i8x16_sub_sat_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_min_s:
                return visitor.template operator()<simd_code::i8x16_min_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_min_u:
                return visitor.template operator()<simd_code::i8x16_min_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_max_s:
                return visitor.template operator()<simd_code::i8x16_max_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_max_u:
                return visitor.template operator()<simd_code::i8x16_max_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_avgr_u:
                return visitor.template operator()<simd_code::i8x16_avgr_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_q15mulr_sat_s:
                return visitor.template operator()<simd_code::i16x8_q15mulr_sat_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_narrow_i32x4_s:
                return visitor.template operator()<simd_code::i16x8_narrow_i32x4_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_narrow_i32x4_u:
                return visitor.template operator()<simd_code::i16x8_narrow_i32x4_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_add:
                return visitor.template operator()<simd_code::i16x8_add,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_add_sat_s:
                return visitor.template operator()<simd_code::i16x8_add_sat_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_add_sat_u:
                return visitor.template operator()<simd_code::i16x8_add_sat_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_sub:
                return visitor.template operator()<simd_code::i16x8_sub,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_sub_sat_s:
                return visitor.template operator()<simd_code::i16x8_sub_sat_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_sub_sat_u:
                return visitor.template operator()<simd_code::i16x8_sub_sat_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_mul:
                return visitor.template operator()<simd_code::i16x8_mul,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_min_s:
                return visitor.template operator()<simd_code::i16x8_min_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_min_u:
                return visitor.template operator()<simd_code::i16x8_min_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_max_s:
                return visitor.template operator()<simd_code::i16x8_max_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_max_u:
                return visitor.template operator()<simd_code::i16x8_max_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_avgr_u:
                return visitor.template operator()<simd_code::i16x8_avgr_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_extmul_low_i8x16_s:
                return visitor.template operator()<simd_code::i16x8_extmul_low_i8x16_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_extmul_high_i8x16_s:
                return visitor.template operator()<simd_code::i16x8_extmul_high_i8x16_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_extmul_low_i8x16_u:
                return visitor.template operator()<simd_code::i16x8_extmul_low_i8x16_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_extmul_high_i8x16_u:
                return visitor.template operator()<simd_code::i16x8_extmul_high_i8x16_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_add:
                return visitor.template operator()<simd_code::i32x4_add,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_sub:
                return visitor.template operator()<simd_code::i32x4_sub,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_mul:
                return visitor.template operator()<simd_code::i32x4_mul,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_min_s:
                return visitor.template operator()<simd_code::i32x4_min_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_min_u:
                return visitor.template operator()<simd_code::i32x4_min_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_max_s:
                return visitor.template operator()<simd_code::i32x4_max_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_max_u:
                return visitor.template operator()<simd_code::i32x4_max_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_dot_i16x8_s:
                return visitor.template operator()<simd_code::i32x4_dot_i16x8_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_extmul_low_i16x8_s:
                return visitor.template operator()<simd_code::i32x4_extmul_low_i16x8_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_extmul_high_i16x8_s:
                return visitor.template operator()<simd_code::i32x4_extmul_high_i16x8_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_extmul_low_i16x8_u:
                return visitor.template operator()<simd_code::i32x4_extmul_low_i16x8_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_extmul_high_i16x8_u:
                return visitor.template operator()<simd_code::i32x4_extmul_high_i16x8_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_add:
                return visitor.template operator()<simd_code::i64x2_add,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_sub:
                return visitor.template operator()<simd_code::i64x2_sub,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_mul:
                return visitor.template operator()<simd_code::i64x2_mul,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_extmul_low_i32x4_s:
                return visitor.template operator()<simd_code::i64x2_extmul_low_i32x4_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_extmul_high_i32x4_s:
                return visitor.template operator()<simd_code::i64x2_extmul_high_i32x4_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_extmul_low_i32x4_u:
                return visitor.template operator()<simd_code::i64x2_extmul_low_i32x4_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_extmul_high_i32x4_u:
                return visitor.template operator()<simd_code::i64x2_extmul_high_i32x4_u,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_ceil:
                return visitor.template operator()<simd_code::f32x4_ceil,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_floor:
                return visitor.template operator()<simd_code::f32x4_floor,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_trunc:
                return visitor.template operator()<simd_code::f32x4_trunc,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_nearest:
                return visitor.template operator()<simd_code::f32x4_nearest,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_ceil:
                return visitor.template operator()<simd_code::f64x2_ceil,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_floor:
                return visitor.template operator()<simd_code::f64x2_floor,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_trunc:
                return visitor.template operator()<simd_code::f64x2_trunc,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_nearest:
                return visitor.template operator()<simd_code::f64x2_nearest,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_abs:
                return visitor.template operator()<simd_code::f32x4_abs,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_neg:
                return visitor.template operator()<simd_code::f32x4_neg,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_sqrt:
                return visitor.template operator()<simd_code::f32x4_sqrt,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_abs:
                return visitor.template operator()<simd_code::f64x2_abs,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_neg:
                return visitor.template operator()<simd_code::f64x2_neg,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_sqrt:
                return visitor.template operator()<simd_code::f64x2_sqrt,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_demote_f64x2_zero:
                return visitor.template operator()<simd_code::f32x4_demote_f64x2_zero,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_promote_low_f32x4:
                return visitor.template operator()<simd_code::f64x2_promote_low_f32x4,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_eq:
                return visitor.template operator()<simd_code::f32x4_eq,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_ne:
                return visitor.template operator()<simd_code::f32x4_ne,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_lt:
                return visitor.template operator()<simd_code::f32x4_lt,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_gt:
                return visitor.template operator()<simd_code::f32x4_gt,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_le:
                return visitor.template operator()<simd_code::f32x4_le,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_ge:
                return visitor.template operator()<simd_code::f32x4_ge,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_eq:
                return visitor.template operator()<simd_code::f64x2_eq,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_ne:
                return visitor.template operator()<simd_code::f64x2_ne,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_lt:
                return visitor.template operator()<simd_code::f64x2_lt,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_gt:
                return visitor.template operator()<simd_code::f64x2_gt,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_le:
                return visitor.template operator()<simd_code::f64x2_le,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_ge:
                return visitor.template operator()<simd_code::f64x2_ge,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_eq:
                return visitor.template operator()<simd_code::i64x2_eq,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_ne:
                return visitor.template operator()<simd_code::i64x2_ne,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_lt_s:
                return visitor.template operator()<simd_code::i64x2_lt_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_gt_s:
                return visitor.template operator()<simd_code::i64x2_gt_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_le_s:
                return visitor.template operator()<simd_code::i64x2_le_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_ge_s:
                return visitor.template operator()<simd_code::i64x2_ge_s,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_add:
                return visitor.template operator()<simd_code::f32x4_add,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_sub:
                return visitor.template operator()<simd_code::f32x4_sub,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_mul:
                return visitor.template operator()<simd_code::f32x4_mul,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_div:
                return visitor.template operator()<simd_code::f32x4_div,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_min:
                return visitor.template operator()<simd_code::f32x4_min,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_max:
                return visitor.template operator()<simd_code::f32x4_max,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_pmin:
                return visitor.template operator()<simd_code::f32x4_pmin,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_pmax:
                return visitor.template operator()<simd_code::f32x4_pmax,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_add:
                return visitor.template operator()<simd_code::f64x2_add,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_sub:
                return visitor.template operator()<simd_code::f64x2_sub,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_mul:
                return visitor.template operator()<simd_code::f64x2_mul,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_div:
                return visitor.template operator()<simd_code::f64x2_div,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_min:
                return visitor.template operator()<simd_code::f64x2_min,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_max:
                return visitor.template operator()<simd_code::f64x2_max,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_pmin:
                return visitor.template operator()<simd_code::f64x2_pmin,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_pmax:
                return visitor.template operator()<simd_code::f64x2_pmax,
                                                   wasm1p1_simd_instruction_kind::binary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_trunc_sat_f32x4_s:
                return visitor.template operator()<simd_code::i32x4_trunc_sat_f32x4_s,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_trunc_sat_f32x4_u:
                return visitor.template operator()<simd_code::i32x4_trunc_sat_f32x4_u,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_convert_i32x4_s:
                return visitor.template operator()<simd_code::f32x4_convert_i32x4_s,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f32x4_convert_i32x4_u:
                return visitor.template operator()<simd_code::f32x4_convert_i32x4_u,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_trunc_sat_f64x2_s_zero:
                return visitor.template operator()<simd_code::i32x4_trunc_sat_f64x2_s_zero,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_trunc_sat_f64x2_u_zero:
                return visitor.template operator()<simd_code::i32x4_trunc_sat_f64x2_u_zero,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_convert_low_i32x4_s:
                return visitor.template operator()<simd_code::f64x2_convert_low_i32x4_s,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::f64x2_convert_low_i32x4_u:
                return visitor.template operator()<simd_code::f64x2_convert_low_i32x4_u,
                                                   wasm1p1_simd_instruction_kind::unary,
                                                   wasm1p1_simd_scalar_kind::none,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_shl:
                return visitor.template operator()<simd_code::i8x16_shl,
                                                   wasm1p1_simd_instruction_kind::shift,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_shr_s:
                return visitor.template operator()<simd_code::i8x16_shr_s,
                                                   wasm1p1_simd_instruction_kind::shift,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i8x16_shr_u:
                return visitor.template operator()<simd_code::i8x16_shr_u,
                                                   wasm1p1_simd_instruction_kind::shift,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_shl:
                return visitor.template operator()<simd_code::i16x8_shl,
                                                   wasm1p1_simd_instruction_kind::shift,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_shr_s:
                return visitor.template operator()<simd_code::i16x8_shr_s,
                                                   wasm1p1_simd_instruction_kind::shift,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i16x8_shr_u:
                return visitor.template operator()<simd_code::i16x8_shr_u,
                                                   wasm1p1_simd_instruction_kind::shift,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_shl:
                return visitor.template operator()<simd_code::i32x4_shl,
                                                   wasm1p1_simd_instruction_kind::shift,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_shr_s:
                return visitor.template operator()<simd_code::i32x4_shr_s,
                                                   wasm1p1_simd_instruction_kind::shift,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i32x4_shr_u:
                return visitor.template operator()<simd_code::i32x4_shr_u,
                                                   wasm1p1_simd_instruction_kind::shift,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_shl:
                return visitor.template operator()<simd_code::i64x2_shl,
                                                   wasm1p1_simd_instruction_kind::shift,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_shr_s:
                return visitor.template operator()<simd_code::i64x2_shr_s,
                                                   wasm1p1_simd_instruction_kind::shift,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            case simd_code::i64x2_shr_u:
                return visitor.template operator()<simd_code::i64x2_shr_u,
                                                   wasm1p1_simd_instruction_kind::shift,
                                                   wasm1p1_simd_scalar_kind::i32,
                                                   0uz,
                                                   0u>();
            [[unlikely]] default:
                return false;
        }
    }
}  // namespace uwvm2::runtime::compiler::shared

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
