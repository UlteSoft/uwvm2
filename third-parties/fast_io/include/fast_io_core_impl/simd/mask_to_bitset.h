#pragma once

namespace fast_io
{

namespace details
{

inline constexpr bool calculate_can_intrinsics_accelerate_mask_to_bitset(::std::size_t sizeofsimdvector) noexcept
{
	if (sizeofsimdvector == 16u)
	{
#if FAST_IO_HAS_ATTRIBUTE(__gnu__::__vector_size__) &&                                   \
	(((defined(__SSE2__) && (defined(__x86_64__) || defined(__i386__)) &&                \
	   !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                                \
	  FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb128)) ||                                \
	 (defined(__wasm_simd128__) && FAST_IO_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)) || \
	 (((defined(__aarch64__) || defined(__arm64__)) && defined(__ARM_NEON)) &&           \
	  (FAST_IO_HAS_BUILTIN(__builtin_neon_vpaddlq_v) ||                                  \
	   (FAST_IO_HAS_BUILTIN(__builtin_aarch64_uaddlpv16qi_uu) &&                         \
		FAST_IO_HAS_BUILTIN(__builtin_aarch64_uaddlpv8hi_uu) &&                          \
		FAST_IO_HAS_BUILTIN(__builtin_aarch64_uaddlpv4si_uu)))))
		return ::fast_io::details::calculate_can_simd_vector_run_with_cpu_instruction(sizeofsimdvector);
#endif
	}
	else if (sizeofsimdvector == 32u)
	{
#if FAST_IO_HAS_ATTRIBUTE(__gnu__::__vector_size__) && defined(__AVX2__) && \
	(defined(__x86_64__) || defined(__i386__)) &&                           \
	!(defined(__arm64ec__) || defined(_M_ARM64EC)) &&                       \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb256)
		return ::fast_io::details::calculate_can_simd_vector_run_with_cpu_instruction(sizeofsimdvector);
#endif
	}
	else if (sizeofsimdvector == 64u)
	{
#if FAST_IO_HAS_ATTRIBUTE(__gnu__::__vector_size__) && defined(__AVX512BW__) && \
	(defined(__x86_64__) || defined(__i386__)) &&                               \
	!(defined(__arm64ec__) || defined(_M_ARM64EC)) &&                           \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_cmpb512_mask)
		return ::fast_io::details::calculate_can_simd_vector_run_with_cpu_instruction(sizeofsimdvector);
#endif
	}
	return false;
}

template <::std::size_t sizeofsimdvector>
inline constexpr bool can_intrinsics_accelerate_mask_to_bitset{
	calculate_can_intrinsics_accelerate_mask_to_bitset(sizeofsimdvector)};

/*
Turn a canonical byte comparison mask into a scalar lane bitmap.  A canonical
mask has lane[i] equal to either 0 or 0xff.  The result is specified by

	bit i of result is one  <=>  lane[i] is 0xff.

This ordering is deliberately expressed in terms of simd_vector's lane
indices, rather than native register byte order.  It is therefore the same
ordering observed after load(): bit zero describes the first input byte.

Correctness of the native mappings:

* x86 pmovmskb writes the most-significant bit of byte lane i to result bit i.
  A canonical lane's most-significant bit is exactly its Boolean value.
  AVX-512 compares each byte against zero and uses the same lane-to-mask-bit
  mapping.
* AArch64 first intersects lane i with the repeating weights
  (1,2,4,...,128).  Thus the first and second groups of eight lanes contain
  respectively either 0 or 2^(i mod 8).  Three unsigned pairwise-long adds
  sum each group without overflow into the two u64 lanes.  Their values are
  therefore the low and high bitmap bytes; combining high << 8 proves the
  stated mapping.  This proof is endian-independent: `simd_vector::load()`
  copies bytes into the vector object, while vector initialization, subscripting,
  and `sums64[0/1]` all use the compiler's language-level lane order.  On
  AArch64 big-endian the backend reverses the physical D-lane extraction and
  the final combination accordingly; adding a source-level byte swap would
  therefore be incorrect.
* The fallback tests the high bit of every lane directly and shifts it to bit
  i, which is the specification itself.  It is also the constant-evaluation
  implementation.

The precondition that masks are canonical is intentional: extracting one bit
per lane is substantially cheaper than reducing arbitrary nonzero byte values,
and all fast_io SIMD comparison operators already produce canonical masks.
*/
template <::std::integral T, ::std::size_t n>
	requires(sizeof(T) == 1u && n != 0u && n <= 64u)
inline constexpr ::std::uint_least64_t
vector_mask_to_bitset_fallback_impl(::fast_io::intrinsics::simd_vector<T, n> const &vec) noexcept
{
	::std::uint_least64_t result{};
	for (::std::size_t i{}; i != n; ++i)
	{
		using unsigned_type = ::std::make_unsigned_t<T>;
		auto const lane{static_cast<unsigned_type>(vec[i])};
		constexpr unsigned shift{static_cast<unsigned>(
			(::std::numeric_limits<unsigned_type>::digits) - 1u)};
		result |= static_cast<::std::uint_least64_t>(lane >> shift) << i;
	}
	return result;
}

template <::std::integral T, ::std::size_t n>
	requires(sizeof(T) == 1u && n != 0u && n <= 64u)
inline constexpr ::std::uint_least64_t
vector_mask_to_bitset_impl(::fast_io::intrinsics::simd_vector<T, n> const &vec) noexcept
{
	FAST_IO_IF_NOT_CONSTEVAL
	{
#if FAST_IO_HAS_ATTRIBUTE(__gnu__::__vector_size__)
		if constexpr (sizeof(vec) == 16u)
		{
#if defined(__SSE2__) && (defined(__x86_64__) || defined(__i386__)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC)) &&                  \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
			using x86_v16qi [[__gnu__::__vector_size__(16)]] = char;
			return static_cast<::std::uint_least16_t>(
				__builtin_ia32_pmovmskb128(__builtin_bit_cast(x86_v16qi, vec.value)));
#elif defined(__wasm_simd128__) && FAST_IO_HAS_BUILTIN(__builtin_wasm_bitmask_i8x16)
			using wasm_i8x16 [[__gnu__::__vector_size__(16)]] = char;
			return static_cast<::std::uint_least16_t>(
				__builtin_wasm_bitmask_i8x16(__builtin_bit_cast(wasm_i8x16, vec.value)));
#elif (defined(__aarch64__) || defined(__arm64__)) && defined(__ARM_NEON)
			using arm_u8x16 [[__gnu__::__vector_size__(16)]] = unsigned char;
			constexpr arm_u8x16 weights{1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u,
										1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u};
			auto const weighted{__builtin_bit_cast(arm_u8x16, vec.value) & weights};
#if FAST_IO_HAS_BUILTIN(__builtin_neon_vpaddlq_v)
			using arm_i8x16 [[__gnu__::__vector_size__(16)]] = signed char;
			using arm_u16x8 [[__gnu__::__vector_size__(16)]] = unsigned short;
			using arm_u32x4 [[__gnu__::__vector_size__(16)]] = unsigned int;
			using arm_u64x2 [[__gnu__::__vector_size__(16)]] = unsigned long long;
			auto const sums16{__builtin_bit_cast(
				arm_u16x8,
				__builtin_neon_vpaddlq_v(__builtin_bit_cast(arm_i8x16, weighted), 49))};
			auto const sums32{__builtin_bit_cast(
				arm_u32x4,
				__builtin_neon_vpaddlq_v(__builtin_bit_cast(arm_i8x16, sums16), 50))};
			auto const sums64{__builtin_bit_cast(
				arm_u64x2,
				__builtin_neon_vpaddlq_v(__builtin_bit_cast(arm_i8x16, sums32), 51))};
#elif FAST_IO_HAS_BUILTIN(__builtin_aarch64_uaddlpv16qi_uu) && \
	FAST_IO_HAS_BUILTIN(__builtin_aarch64_uaddlpv8hi_uu) &&    \
	FAST_IO_HAS_BUILTIN(__builtin_aarch64_uaddlpv4si_uu)
			auto const sums16{__builtin_aarch64_uaddlpv16qi_uu(weighted)};
			auto const sums32{__builtin_aarch64_uaddlpv8hi_uu(sums16)};
			auto const sums64{__builtin_aarch64_uaddlpv4si_uu(sums32)};
#else
			return vector_mask_to_bitset_fallback_impl(vec);
#endif
			return static_cast<::std::uint_least64_t>(sums64[0]) |
				   (static_cast<::std::uint_least64_t>(sums64[1]) << 8u);
#endif
		}
		else if constexpr (sizeof(vec) == 32u)
		{
#if defined(__AVX2__) && (defined(__x86_64__) || defined(__i386__)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC)) &&                  \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb256)
			using x86_v32qi [[__gnu__::__vector_size__(32)]] = char;
			return static_cast<::std::uint_least32_t>(
				__builtin_ia32_pmovmskb256(__builtin_bit_cast(x86_v32qi, vec.value)));
#endif
		}
		else if constexpr (sizeof(vec) == 64u)
		{
#if defined(__AVX512BW__) && (defined(__x86_64__) || defined(__i386__)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC)) &&                      \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_cmpb512_mask)
			using x86_v64qi [[__gnu__::__vector_size__(64)]] = char;
			auto const bytes{__builtin_bit_cast(x86_v64qi, vec.value)};
			return static_cast<::std::uint_least64_t>(__builtin_ia32_cmpb512_mask(
				bytes, x86_v64qi{}, 0x04,
				(::std::numeric_limits<::std::uint_least64_t>::max)()));
#endif
		}
#endif
	}
	return vector_mask_to_bitset_fallback_impl(vec);
}

} // namespace details

namespace intrinsics
{

template <::std::size_t sizeofsimdvector>
inline constexpr bool can_intrinsics_accelerate_mask_to_bitset{
	::fast_io::details::can_intrinsics_accelerate_mask_to_bitset<sizeofsimdvector>};

inline constexpr ::std::size_t optimal_simd_vector_run_with_cpu_instruction_size_with_mask_to_bitset{
	::fast_io::intrinsics::can_intrinsics_accelerate_mask_to_bitset<64u>
		? 64u
		: (::fast_io::intrinsics::can_intrinsics_accelerate_mask_to_bitset<32u>
			   ? 32u
			   : (::fast_io::intrinsics::can_intrinsics_accelerate_mask_to_bitset<16u> ? 16u : 0u))};

template <::std::integral T, ::std::size_t n>
	requires(sizeof(T) == 1u && n != 0u && n <= 64u)
inline constexpr ::std::uint_least64_t
vector_mask_to_bitset(simd_vector<T, n> const &vec) noexcept
{
	return ::fast_io::details::vector_mask_to_bitset_impl(vec);
}

} // namespace intrinsics

} // namespace fast_io
