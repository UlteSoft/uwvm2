#pragma once

namespace fast_io::details
{

template <typename flt>
struct iec559_traits;

inline constexpr ::std::uint_least32_t iec559_exponent_bits_from_max_exponent(
	::std::uint_least32_t max_exponent) noexcept
{
	auto bias{max_exponent - 1u};
	::std::uint_least32_t bits{};
	for (; bias; bias >>= 1u)
	{
		++bits;
	}
	return bits + 1u;
}

inline constexpr ::std::uint_least32_t iec559_decimal_digits(::std::uint_least32_t value) noexcept
{
	::std::uint_least32_t digits{1u};
	for (; 10u <= value; value /= 10u)
	{
		++digits;
	}
	return digits;
}

#if defined(__SIZEOF_FLOAT16__) || defined(__FLOAT16__)
template <>
struct iec559_traits<__float16>
{
	using mantissa_type = ::std::uint_least16_t;
	inline static constexpr ::std::size_t mbits{10};
	inline static constexpr ::std::size_t ebits{5};
	inline static constexpr ::std::uint_least32_t m10digits{5};
	inline static constexpr ::std::uint_least32_t m2hexdigits{3};
	inline static constexpr ::std::uint_least32_t e10digits{2};
	inline static constexpr ::std::uint_least32_t e2hexdigits{2};
	inline static constexpr ::std::uint_least32_t e10max{4};
};
#endif

#if defined(__SIZEOF_FLOAT80__) &&                                                                                \
	(!defined(__LDBL_MANT_DIG__) || !defined(__LDBL_MAX_EXP__) || !defined(__SIZEOF_LONG_DOUBLE__) ||             \
	 __LDBL_MANT_DIG__ != 64 || __LDBL_MAX_EXP__ != 16384 || __SIZEOF_LONG_DOUBLE__ != __SIZEOF_FLOAT80__)
template <>
struct iec559_traits<__float80>
{
	using mantissa_type = ::std::uint_least64_t;
	inline static constexpr ::std::size_t mbits{63};
	inline static constexpr ::std::size_t ebits{15};
	inline static constexpr ::std::uint_least32_t m10digits{21};
	inline static constexpr ::std::uint_least32_t m2hexdigits{16};
	inline static constexpr ::std::uint_least32_t e10digits{4};
	inline static constexpr ::std::uint_least32_t e2hexdigits{5};
	inline static constexpr ::std::uint_least32_t e10max{4932};
};
#endif

template <>
struct iec559_traits<float>
{
	using mantissa_type = ::std::uint_least32_t;
	inline static constexpr ::std::size_t mbits{23};
	inline static constexpr ::std::size_t ebits{8};
	inline static constexpr ::std::uint_least32_t m10digits{9};
	inline static constexpr ::std::uint_least32_t m2hexdigits{6};
	inline static constexpr ::std::uint_least32_t e10digits{2};
	inline static constexpr ::std::uint_least32_t e2hexdigits{3};
	inline static constexpr ::std::uint_least32_t e10max{38};
};

template <>
struct iec559_traits<double>
{
	using mantissa_type = ::std::uint_least64_t;
	inline static constexpr ::std::size_t mbits{52};
	inline static constexpr ::std::size_t ebits{11};
	inline static constexpr ::std::uint_least32_t m10digits{17};
	inline static constexpr ::std::uint_least32_t m2hexdigits{13};
	inline static constexpr ::std::uint_least32_t e10digits{3};
	inline static constexpr ::std::uint_least32_t e2hexdigits{4};
	inline static constexpr ::std::uint_least32_t e10max{308};
};

template <>
struct iec559_traits<long double>
{
#if defined(__SIZEOF_INT128__)
	using mantissa_type = ::std::conditional_t<(64 < ::std::numeric_limits<long double>::digits), __uint128_t,
											   ::std::uint_least64_t>;
#else
	using mantissa_type = ::std::uint_least64_t;
#endif
	inline static constexpr ::std::size_t mbits{static_cast<::std::size_t>(
		::std::numeric_limits<long double>::digits - 1)};
	inline static constexpr ::std::size_t ebits{::fast_io::details::iec559_exponent_bits_from_max_exponent(
		static_cast<::std::uint_least32_t>(::std::numeric_limits<long double>::max_exponent))};
	inline static constexpr ::std::uint_least32_t m10digits{
		::std::numeric_limits<long double>::digits == 113 ? 37u
														  : static_cast<::std::uint_least32_t>(
																::std::numeric_limits<long double>::max_digits10)};
	inline static constexpr ::std::uint_least32_t m2hexdigits{
		static_cast<::std::uint_least32_t>((mbits + 3u) / 4u)};
	inline static constexpr ::std::uint_least32_t e10digits{::fast_io::details::iec559_decimal_digits(
		static_cast<::std::uint_least32_t>(::std::numeric_limits<long double>::max_exponent10))};
	inline static constexpr ::std::uint_least32_t e2hexdigits{::fast_io::details::iec559_decimal_digits(
		static_cast<::std::uint_least32_t>(::std::numeric_limits<long double>::max_exponent))};
	inline static constexpr ::std::uint_least32_t e10max{
		static_cast<::std::uint_least32_t>(::std::numeric_limits<long double>::max_exponent10)};
};

#if defined(__SIZEOF_INT128__)

#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
template <>
struct iec559_traits<__float128>
{
	using mantissa_type = __uint128_t;
	inline static constexpr ::std::size_t mbits{112};
	inline static constexpr ::std::size_t ebits{15};
	inline static constexpr ::std::uint_least32_t m10digits{37};
	inline static constexpr ::std::uint_least32_t m2hexdigits{28};
	inline static constexpr ::std::uint_least32_t e10digits{4};
	inline static constexpr ::std::uint_least32_t e2hexdigits{5};
	inline static constexpr ::std::uint_least32_t e10max{4966};
};
#endif

#endif

#if defined(__STDCPP_FLOAT16_T__) || defined(__FLT16_MANT_DIG__)
template <>
struct iec559_traits<_Float16>
{
	using mantissa_type = ::std::uint_least16_t;
	inline static constexpr ::std::size_t mbits{10};
	inline static constexpr ::std::size_t ebits{5};
	inline static constexpr ::std::uint_least32_t m10digits{5};
	inline static constexpr ::std::uint_least32_t m2hexdigits{3};
	inline static constexpr ::std::uint_least32_t e10digits{2};
	inline static constexpr ::std::uint_least32_t e2hexdigits{2};
	inline static constexpr ::std::uint_least32_t e10max{4};
};
#endif

#ifdef __STDCPP_FLOAT32_T__
template <>
struct iec559_traits<_Float32>
{
	using mantissa_type = ::std::uint_least32_t;
	inline static constexpr ::std::size_t mbits{23};
	inline static constexpr ::std::size_t ebits{8};
	inline static constexpr ::std::uint_least32_t m10digits{9};
	inline static constexpr ::std::uint_least32_t m2hexdigits{6};
	inline static constexpr ::std::uint_least32_t e10digits{2};
	inline static constexpr ::std::uint_least32_t e2hexdigits{3};
	inline static constexpr ::std::uint_least32_t e10max{38};
};
#endif

#if defined(FAST_IO_HAS_FLOAT64_TYPE)
template <>
struct iec559_traits<_Float64>
{
	using mantissa_type = ::std::uint_least64_t;
	inline static constexpr ::std::size_t mbits{52};
	inline static constexpr ::std::size_t ebits{11};
	inline static constexpr ::std::uint_least32_t m10digits{17};
	inline static constexpr ::std::uint_least32_t m2hexdigits{13};
	inline static constexpr ::std::uint_least32_t e10digits{3};
	inline static constexpr ::std::uint_least32_t e2hexdigits{4};
	inline static constexpr ::std::uint_least32_t e10max{308};
};
#endif

#if defined(__STDCPP_FLOAT128_T__) && defined(__SIZEOF_INT128__)
template <>
struct iec559_traits<_Float128>
{
	using mantissa_type = __uint128_t;
	inline static constexpr ::std::size_t mbits{112};
	inline static constexpr ::std::size_t ebits{15};
	inline static constexpr ::std::uint_least32_t m10digits{37};
	inline static constexpr ::std::uint_least32_t m2hexdigits{28};
	inline static constexpr ::std::uint_least32_t e10digits{4};
	inline static constexpr ::std::uint_least32_t e2hexdigits{5};
	inline static constexpr ::std::uint_least32_t e10max{4966};
};
#endif

#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE)
template <>
struct iec559_traits<__bf16>
{
	using mantissa_type = ::std::uint_least16_t;
	inline static constexpr ::std::size_t mbits{7};
	inline static constexpr ::std::size_t ebits{8};
	inline static constexpr ::std::uint_least32_t m10digits{4};
	inline static constexpr ::std::uint_least32_t m2hexdigits{2};
	inline static constexpr ::std::uint_least32_t e10digits{2};
	inline static constexpr ::std::uint_least32_t e2hexdigits{3};
	inline static constexpr ::std::uint_least32_t e10max{38};
};
#endif

// A standardized C++23 bfloat16 implementation provides the literal suffix,
// making its exact language type available without relying on a vendor name.
#if defined(__STDCPP_BFLOAT16_T__)
template <>
struct iec559_traits<decltype(0.0bf16)>
{
	using mantissa_type = ::std::uint_least16_t;
	inline static constexpr ::std::size_t mbits{7};
	inline static constexpr ::std::size_t ebits{8};
	inline static constexpr ::std::uint_least32_t m10digits{4};
	// Bfloat16 has seven explicit fraction bits, so ceil(7 / 4) is exactly
	// two hexadecimal fraction digits.  Keep this representation invariant
	// accurate even though today's hexadecimal emitter derives its width from
	// mbits directly rather than consuming m2hexdigits.
	inline static constexpr ::std::uint_least32_t m2hexdigits{2};
	inline static constexpr ::std::uint_least32_t e10digits{2};
	inline static constexpr ::std::uint_least32_t e2hexdigits{3};
	inline static constexpr ::std::uint_least32_t e10max{38};
};
#endif

// GCC exposes __bf16 together with __BFLT16_MANT_DIG__ in C++20, one language
// version before the standard bf16 literal suffix.  Naming the vendor type
// directly avoids a -Wpedantic C++23-extension diagnostic while describing the
// same 7-fraction-bit, 8-exponent-bit representation.  Clang has its separately
// capability-tested __bf16 specialization above, so the frontend exclusion also
// prevents a duplicate specialization in GNU-compatibility mode.
#if defined(__GNUC__) && !defined(__clang__) && defined(__BFLT16_MANT_DIG__) && \
	!defined(__STDCPP_BFLOAT16_T__)
template <>
struct iec559_traits<__bf16>
{
	using mantissa_type = ::std::uint_least16_t;
	inline static constexpr ::std::size_t mbits{7};
	inline static constexpr ::std::size_t ebits{8};
	inline static constexpr ::std::uint_least32_t m10digits{4};
	// Bfloat16 has seven explicit fraction bits, so ceil(7 / 4) is exactly
	// two hexadecimal fraction digits.  Keep this representation invariant
	// accurate even though today's hexadecimal emitter derives its width from
	// mbits directly rather than consuming m2hexdigits.
	inline static constexpr ::std::uint_least32_t m2hexdigits{2};
	inline static constexpr ::std::uint_least32_t e10digits{2};
	inline static constexpr ::std::uint_least32_t e2hexdigits{3};
	inline static constexpr ::std::uint_least32_t e10max{38};
};
#endif

template <my_unsigned_integral T>
// Native MSVC is the only frontend selected for its vendor inlining attribute.
// Clang can define _MSC_VER in clang-cl mode, so the explicit frontend exclusion
// prevents an ABI-compatibility macro from being mistaken for compiler identity.
// The attribute-availability test keeps older MSVC releases source-compatible;
// this annotation changes only the inlining request, not the function contract.
#if defined(_MSC_VER) && !defined(__clang__)
#if __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
#endif
inline constexpr int my_countr_zero_unchecked(T x) noexcept
{
	// Precondition: x != 0. Every __builtin_ctz* operation is undefined for zero.
	// Nd is the value width of T. Select the narrowest builtin operand type that
	// covers all of T, so integral conversion cannot discard a possible low set
	// bit. For a wider (at most 128-bit) T, inspect the low unsigned-long-long
	// limb first. If that limb is zero, the precondition proves that the high limb
	// is nonzero and therefore valid input to __builtin_ctzll. The standard-library
	// fallback has the same result for every value admitted by this contract.
#if defined(__GNUC__) || defined(__clang__)
	constexpr auto Nd = ::std::numeric_limits<char>::digits * sizeof(T);
	constexpr auto Nd_ull = ::std::numeric_limits<unsigned long long>::digits;
	constexpr auto Nd_ul = ::std::numeric_limits<unsigned long>::digits;
	constexpr auto Nd_u = ::std::numeric_limits<unsigned>::digits;
	if constexpr (Nd <= Nd_u)
	{
		return __builtin_ctz(x);
	}
	else if constexpr (Nd <= Nd_ul)
	{
		return __builtin_ctzl(x);
	}
	else if constexpr (Nd <= Nd_ull)
	{
		return __builtin_ctzll(x);
	}
	else
	{
		static_assert(Nd <= (2 * Nd_ull), "Maximum supported integer size is 128-bit");
		constexpr auto max_ull = ::std::numeric_limits<unsigned long long>::max();
		unsigned long long low = x & max_ull;
		if (low != 0)
		{
			return __builtin_ctzll(low);
		}
		// Removing the low Nd_ull bits leaves at most Nd-Nd_ull bits.
		// The enclosing static_assert proves that remainder fits exactly in the
		// builtin's unsigned-long-long operand; state the narrowing explicitly so
		// -Wconversion need not infer the same range through a dependent type.
		unsigned long long high = static_cast<unsigned long long>(x >> Nd_ull);
		return __builtin_ctzll(high) + Nd_ull;
	}
#else
	return ::std::countr_zero(x);
#endif
}

template <::std::floating_point flt>
struct iec559_rep
{
	using mantissa_type = typename iec559_traits<flt>::mantissa_type;
	mantissa_type m;
	::std::int_least32_t e;
};

template <bool nan_show_type>
inline constexpr ::std::size_t print_rsv_fp_special_size_cache{nan_show_type ? 10u : 4u};

template <::std::size_t normal_size, bool nan_show_type>
inline constexpr ::std::size_t print_rsv_fp_size_with_special_cache{
	normal_size < print_rsv_fp_special_size_cache<nan_show_type> ? print_rsv_fp_special_size_cache<nan_show_type>
																 : normal_size};

template <bool showpos, ::std::integral char_type>
inline constexpr char_type *print_rsv_fp_sign_impl(char_type *iter, bool sign) noexcept;

template <::std::integral char_type, ::std::size_t n>
	requires(n != 0u)
inline constexpr char_type *copy_floating_ascii_literal(
	char8_t const (&literal)[n], char_type *iter) noexcept
{
	for (::std::size_t index{}; index + 1u != n; ++index)
	{
		*iter = ::fast_io::char_literal<char_type>(literal[index]);
		++iter;
	}
	return iter;
}

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *prsv_fp_inf_literal_impl(char_type *iter) noexcept
{
	if constexpr (!::fast_io::details::is_ascii<char_type> &&
		(::std::same_as<char_type, char> || ::std::same_as<char_type, wchar_t>))
	{
		if constexpr (uppercase)
		{
			return copy_floating_ascii_literal(u8"INF", iter);
		}
		else
		{
			return copy_floating_ascii_literal(u8"inf", iter);
		}
	}
	if constexpr (uppercase)
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("INF", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"INF", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"INF", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"INF", iter);
		}
		else
		{
			return copy_string_literal(u8"INF", iter);
		}
	}
	else
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("inf", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"inf", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"inf", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"inf", iter);
		}
		else
		{
			return copy_string_literal(u8"inf", iter);
		}
	}
}

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *prsv_fp_nan_literal_impl(char_type *iter) noexcept
{
	if constexpr (!::fast_io::details::is_ascii<char_type> &&
		(::std::same_as<char_type, char> || ::std::same_as<char_type, wchar_t>))
	{
		if constexpr (uppercase)
		{
			return copy_floating_ascii_literal(u8"NAN", iter);
		}
		else
		{
			return copy_floating_ascii_literal(u8"nan", iter);
		}
	}
	if constexpr (uppercase)
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("NAN", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"NAN", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"NAN", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"NAN", iter);
		}
		else
		{
			return copy_string_literal(u8"NAN", iter);
		}
	}
	else
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("nan", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"nan", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"nan", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"nan", iter);
		}
		else
		{
			return copy_string_literal(u8"nan", iter);
		}
	}
}

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *prsv_fp_nan_ind_literal_impl(char_type *iter) noexcept
{
	if constexpr (!::fast_io::details::is_ascii<char_type> &&
		(::std::same_as<char_type, char> || ::std::same_as<char_type, wchar_t>))
	{
		if constexpr (uppercase)
		{
			return copy_floating_ascii_literal(u8"(IND)", iter);
		}
		else
		{
			return copy_floating_ascii_literal(u8"(ind)", iter);
		}
	}
	if constexpr (uppercase)
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("(IND)", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"(IND)", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"(IND)", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"(IND)", iter);
		}
		else
		{
			return copy_string_literal(u8"(IND)", iter);
		}
	}
	else
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("(ind)", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"(ind)", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"(ind)", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"(ind)", iter);
		}
		else
		{
			return copy_string_literal(u8"(ind)", iter);
		}
	}
}

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *prsv_fp_nan_snan_literal_impl(char_type *iter) noexcept
{
	if constexpr (!::fast_io::details::is_ascii<char_type> &&
		(::std::same_as<char_type, char> || ::std::same_as<char_type, wchar_t>))
	{
		if constexpr (uppercase)
		{
			return copy_floating_ascii_literal(u8"(SNAN)", iter);
		}
		else
		{
			return copy_floating_ascii_literal(u8"(snan)", iter);
		}
	}
	if constexpr (uppercase)
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("(SNAN)", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"(SNAN)", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"(SNAN)", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"(SNAN)", iter);
		}
		else
		{
			return copy_string_literal(u8"(SNAN)", iter);
		}
	}
	else
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("(snan)", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"(snan)", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"(snan)", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"(snan)", iter);
		}
		else
		{
			return copy_string_literal(u8"(snan)", iter);
		}
	}
}

template <typename mantissa_type, ::std::size_t mbits>
inline constexpr mantissa_type fp_quiet_nan_mantissa_mask() noexcept
{
	return static_cast<mantissa_type>(static_cast<mantissa_type>(1) << (mbits - 1u));
}

template <typename mantissa_type, ::std::size_t mbits>
inline constexpr bool fp_nan_is_signaling(mantissa_type mantissa) noexcept
{
	return mantissa != 0 && (mantissa & fp_quiet_nan_mantissa_mask<mantissa_type, mbits>()) == 0;
}

#if defined(__SIZEOF_FLOAT80__) ||                                                                            \
	(defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && __LDBL_MANT_DIG__ == 64 &&                    \
	 __LDBL_MAX_EXP__ == 16384)
template <typename flt>
inline constexpr bool fp_floating_point_is_float80{
#ifdef __SIZEOF_FLOAT80__
	::std::same_as<::std::remove_cv_t<flt>, __float80> ||
#endif
	(::std::same_as<::std::remove_cv_t<flt>, long double> &&
	 ::std::numeric_limits<long double>::digits == 64 &&
	 ::std::numeric_limits<long double>::max_exponent == 16384)};

template <typename flt>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void fp_assign_float80_bits(flt &value, ::std::uint_least64_t mantissa,
											 ::std::uint_least32_t exponent, bool sign) noexcept
	requires(::fast_io::details::fp_floating_point_is_float80<flt>)
{
	static_assert(sizeof(flt) >= sizeof(::std::uint_least64_t) + sizeof(::std::uint_least16_t));
	static_assert(::std::endian::native == ::std::endian::little,
				  "fast_io float80 bit assignment currently supports only little-endian x87 storage");
	struct storage_type
	{
		unsigned char bytes[sizeof(flt)];
	};
	static_assert(sizeof(storage_type) == sizeof(flt));
	storage_type storage{};
	auto const exponent_bits{
		static_cast<::std::uint_least16_t>((sign ? 0x8000u : 0u) | (exponent & 0x7fffu))};
	for (::std::size_t index{}; index != sizeof(::std::uint_least64_t); ++index)
	{
		storage.bytes[index] = static_cast<unsigned char>(mantissa >> (index * 8u));
	}
	storage.bytes[8] = static_cast<unsigned char>(exponent_bits);
	storage.bytes[9] = static_cast<unsigned char>(exponent_bits >> 8u);
	if (__builtin_is_constant_evaluated())
	{
		value = ::fast_io::bit_cast<flt>(storage);
	}
	else
	{
#if FAST_IO_HAS_BUILTIN(__builtin_memcpy)
		__builtin_memcpy
#else
		::std::memcpy
#endif
			(__builtin_addressof(value), __builtin_addressof(storage), sizeof(flt));
	}
}
#else
template <typename flt>
inline constexpr bool fp_floating_point_is_float80{};

template <typename flt>
inline constexpr void fp_assign_float80_bits(flt &, ::std::uint_least64_t, ::std::uint_least32_t, bool) noexcept = delete;
#endif

/*
PowerPC's historical IBM extended format is not an IEC 60559 binary field.
Its object contains two binary64 numbers (the leading component first) and its
mathematical value is their exact sum.  `numeric_limits` consequently reports
106 guaranteed significant bits, but bit-casting the 16-byte object to a
105-fraction-bit integer would concatenate unrelated sign/exponent fields.

Keep the representation predicate independent of compiler branding.  GCC and
Clang both expose the ABI through the same observable language properties,
whereas `__LONG_DOUBLE_IBM128__` is not consistently defined by older
frontends.  The size conjunct distinguishes IBM double-double from a target
that aliases long double to binary64 while the precision/range conjunctions
distinguish it from IEEE binary128.
*/
template <typename flt>
inline constexpr bool fp_floating_point_is_ibm_double_double{
	::std::same_as<::std::remove_cv_t<flt>, long double> &&
	sizeof(long double) == 2u * sizeof(double) &&
	::std::numeric_limits<long double>::digits == 106 &&
	::std::numeric_limits<long double>::max_exponent == 1024};

#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
struct ibm_double_double_storage
{
	double high;
	double low;
};

struct ibm_double_double_dyadic
{
	__uint128_t significand{};
	::std::int_least32_t exponent{};
	bool negative{};
	bool success{};
};

struct binary64_signed_dyadic
{
	::std::uint_least64_t significand{};
	::std::int_least32_t exponent{};
	bool negative{};
};

[[nodiscard]] inline constexpr unsigned
ibm_double_double_u64_trailing_zeroes(::std::uint_least64_t value) noexcept
{
	unsigned count{};
	if (value)
	{
		for (; (value & 1u) == 0u; value >>= 1u)
		{
			++count;
		}
	}
	return count;
}

[[nodiscard]] inline constexpr unsigned
ibm_double_double_u128_bit_width(__uint128_t value) noexcept
{
	unsigned width{};
	for (; value; value >>= 1u)
	{
		++width;
	}
	return width;
}

[[nodiscard]] inline constexpr ::fast_io::details::binary64_signed_dyadic
ibm_double_double_decode_component(double value) noexcept
{
	auto const bits{::fast_io::bit_cast<::std::uint_least64_t>(value)};
	auto const fraction{bits & UINT64_C(0x000fffffffffffff)};
	auto const raw_exponent{static_cast<::std::uint_least32_t>((bits >> 52u) & 0x7ffu)};
	if (!fraction && !raw_exponent)
	{
		return {};
	}
	auto significand{fraction};
	auto exponent{static_cast<::std::int_least32_t>(-1074)};
	if (raw_exponent)
	{
		significand |= UINT64_C(0x0010000000000000);
		exponent = static_cast<::std::int_least32_t>(raw_exponent) - 1075;
	}
	auto const trailing{ibm_double_double_u64_trailing_zeroes(significand)};
	significand >>= trailing;
	exponent += static_cast<::std::int_least32_t>(trailing);
	return {significand, exponent, static_cast<bool>(bits >> 63u)};
}

/*
Let h=H*2^a and l=L*2^b be the two decoded components after removing
powers of two from H and L.  The IBM normalization invariant places the
least significant nonzero bit of the low component no more than 105 places
below the high component's leading bit.  Aligning at min(a,b) therefore makes
the exact signed sum fit in 107 bits (106 value bits plus a cancellation bit),
strictly inside the unsigned 128-bit magnitude carrier.  Same-sign addition
and opposite-sign subtraction are performed on magnitudes, avoiding any
implementation-defined unsigned-to-signed conversion.  Removing the sum's
final powers of two returns the unique odd dyadic carrier S*2^e, and no step
can observe the ambient floating environment.
*/
template <typename flt>
	requires(::fast_io::details::fp_floating_point_is_ibm_double_double<flt>)
[[nodiscard]] inline constexpr ::fast_io::details::ibm_double_double_dyadic
get_ibm_double_double_dyadic(flt value) noexcept
{
	static_assert(sizeof(::fast_io::details::ibm_double_double_storage) == sizeof(flt));
	auto const storage{::fast_io::bit_cast<::fast_io::details::ibm_double_double_storage>(value)};
	auto const high_bits{::fast_io::bit_cast<::std::uint_least64_t>(storage.high)};
	auto const low_bits{::fast_io::bit_cast<::std::uint_least64_t>(storage.low)};
	auto const high_raw_exponent{static_cast<::std::uint_least32_t>((high_bits >> 52u) & 0x7ffu)};
	auto const low_raw_exponent{static_cast<::std::uint_least32_t>(
		(low_bits >> 52u) & 0x7ffu)};
	if (high_raw_exponent == 0x7ffu || low_raw_exponent == 0x7ffu)
	{
		/*
		A special leading component classifies the whole IBM object as special.
		A special low component with a finite high component is noncanonical and
		has no finite dyadic sum.  Rejecting both cases prevents an exponent-all-
		ones payload from entering the alignment arithmetic as an ordinary 2^972
		component.
		*/
		return {};
	}
	auto const high{::fast_io::details::ibm_double_double_decode_component(storage.high)};
	auto const low{::fast_io::details::ibm_double_double_decode_component(storage.low)};
	if (!high.significand)
	{
		return {static_cast<__uint128_t>(low.significand), low.exponent,
			low.negative, true};
	}
	if (!low.significand)
	{
		return {static_cast<__uint128_t>(high.significand), high.exponent,
			high.negative, true};
	}
	auto const common_exponent{high.exponent < low.exponent ? high.exponent : low.exponent};
	auto const high_shift{static_cast<unsigned>(high.exponent - common_exponent)};
	auto const low_shift{static_cast<unsigned>(low.exponent - common_exponent)};
	auto const high_width{
		::fast_io::details::ibm_double_double_u128_bit_width(high.significand)};
	auto const low_width{
		::fast_io::details::ibm_double_double_u128_bit_width(low.significand)};
	if (128u < high_shift + high_width || 128u < low_shift + low_width)
	{
		/*
		Language-produced IBM values satisfy the normalization theorem above.
		For a forged wider span, shifting would discard a leading bit modulo the
		uint128 carrier.  The width-plus-shift test is the exact representability
		condition, so rejection occurs before any potentially lossy operation.
		*/
		return {};
	}
	auto const high_integer{
		static_cast<__uint128_t>(high.significand) << high_shift};
	auto const low_integer{
		static_cast<__uint128_t>(low.significand) << low_shift};
	__uint128_t magnitude{};
	bool negative{};
	if (high.negative == low.negative)
	{
		if ((~static_cast<__uint128_t>(0u)) - high_integer < low_integer)
		{
			/* A same-sign forged pair exceeds the exact uint128 carrier. */
			return {};
		}
		magnitude = high_integer + low_integer;
		negative = high.negative;
	}
	else if (low_integer < high_integer)
	{
		magnitude = high_integer - low_integer;
		negative = high.negative;
	}
	else
	{
		magnitude = low_integer - high_integer;
		negative = low.negative;
	}
	if (!magnitude)
	{
		return {0u, 0, static_cast<bool>(high_bits >> 63u), true};
	}
	if (107u < ::fast_io::details::
			ibm_double_double_u128_bit_width(magnitude))
	{
		/* A normalized IBM sum satisfies the 107-bit bound proved above. */
		return {};
	}
	unsigned trailing{};
	for (; (magnitude & 1u) == 0u; magnitude >>= 1u)
	{
		++trailing;
	}
	return {magnitude,
		static_cast<::std::int_least32_t>(common_exponent + static_cast<::std::int_least32_t>(trailing)),
		negative, true};
}

[[nodiscard]] inline constexpr double ibm_double_double_make_binary64(
	::std::uint_least64_t significand, ::std::int_least32_t exponent,
	bool negative) noexcept
{
	if (!significand)
	{
		return ::fast_io::bit_cast<double>(
			negative ? UINT64_C(0x8000000000000000) : UINT64_C(0));
	}
	auto const trailing{
		::fast_io::details::ibm_double_double_u64_trailing_zeroes(significand)};
	significand >>= trailing;
	exponent += static_cast<::std::int_least32_t>(trailing);
	unsigned width{};
	for (auto probe{significand}; probe; probe >>= 1u)
	{
		++width;
	}
	auto const top_exponent{static_cast<::std::int_least32_t>(
		exponent + static_cast<::std::int_least32_t>(width - 1u))};
	::std::uint_least64_t bits{};
	if (-1022 <= top_exponent)
	{
		auto const complete{significand << (53u - width)};
		bits = (static_cast<::std::uint_least64_t>(top_exponent + 1023) << 52u) |
			(complete & UINT64_C(0x000fffffffffffff));
	}
	else
	{
		auto const shift{static_cast<::std::int_least32_t>(exponent + 1074)};
		if (shift < 0)
		{
			return ::fast_io::bit_cast<double>(
				negative ? UINT64_C(0x8000000000000000) : UINT64_C(0));
		}
		bits = significand << static_cast<unsigned>(shift);
	}
	if (negative)
	{
		bits |= UINT64_C(0x8000000000000000);
	}
	return ::fast_io::bit_cast<double>(bits);
}

template <typename flt>
	requires(::fast_io::details::fp_floating_point_is_ibm_double_double<flt>)
inline constexpr void fp_assign_ibm_double_double_components(
	flt &value, double high, double low) noexcept
{
	::fast_io::details::ibm_double_double_storage const storage{high, low};
	value = ::fast_io::bit_cast<flt>(storage);
}

/*
Split the already-rounded p<=106 dyadic Q*2^e into a canonical IBM pair.  If
Q has more than 53 bits, H is Q rounded to nearest-even on the binary64 grid
2^(e+n-53), and L=Q-H*2^(n-53) is the exact signed residual.  The rounding
bound |L|<=2^(n-54), together with n<=106, gives at most 53 residual bits;
both H and L are therefore exact binary64 values and H+L=Q*2^e algebraically.
The outer sign multiplies both components, including a residual whose sign is
opposite H after a rounded-up split.
*/
template <typename flt>
	requires(::fast_io::details::fp_floating_point_is_ibm_double_double<flt>)
[[nodiscard]] inline constexpr bool
fp_assign_ibm_double_double_significand(
	flt &value, __uint128_t significand, ::std::int_least32_t exponent,
	bool negative) noexcept
{
	if (!significand)
	{
		::fast_io::details::fp_assign_ibm_double_double_components(
			value, ::fast_io::details::ibm_double_double_make_binary64(
				0u, 0, negative), 0.0);
		return true;
	}
	/*
	Canonicalize the incoming dyadic before checking the IBM range.  Removing
	a factor two from Q while incrementing e preserves Q*2^e exactly and makes
	`exponent` the position of its least significant nonzero bit.  Therefore
	exponent>=-1074 is precisely the binary64 low-component floor, while
	exponent+width-1<=1023 is the leading-component ceiling.
	*/
	for (; (significand & 1u) == 0u; significand >>= 1u)
	{
		++exponent;
	}
	auto const width{
		::fast_io::details::ibm_double_double_u128_bit_width(significand)};
	if (106u < width)
	{
		return false;
	}
	auto const top_exponent{static_cast<::std::int_least32_t>(
		exponent + static_cast<::std::int_least32_t>(width - 1u))};
	if (exponent < -1074 || 1023 < top_exponent)
	{
		return false;
	}
	if (top_exponent == 1023)
	{
		/*
		The last IBM binade ends below the fictitious uniform p=106 endpoint.
		An exact dyadic comparison with numeric_limits::max is consequently
		required; exponent-range checks alone would admit unencodable values.
		Both operands have at most 106 bits and the same top exponent, so their
		alignment fits uint128 without truncation.
		*/
		auto const maximum{
			::fast_io::details::get_ibm_double_double_dyadic(
				(::std::numeric_limits<flt>::max)())};
		if (!maximum.success)
		{
			return false;
		}
		auto const common_exponent{
			exponent < maximum.exponent ? exponent : maximum.exponent};
		auto const input_shift{static_cast<unsigned>(
			exponent - common_exponent)};
		auto const maximum_shift{static_cast<unsigned>(
			maximum.exponent - common_exponent)};
		if ((maximum.significand << maximum_shift) <
			(significand << input_shift))
		{
			return false;
		}
	}
	if (width <= 53u)
	{
		auto const high{
			::fast_io::details::ibm_double_double_make_binary64(
				static_cast<::std::uint_least64_t>(significand), exponent,
				negative)};
		::fast_io::details::fp_assign_ibm_double_double_components(
			value, high, 0.0);
		return true;
	}
	auto const shift{width - 53u};
	auto high_significand{static_cast<::std::uint_least64_t>(
		significand >> shift)};
	auto const remainder_mask{(static_cast<__uint128_t>(1u) << shift) - 1u};
	auto const remainder{significand & remainder_mask};
	auto const halfway{static_cast<__uint128_t>(1u) << (shift - 1u)};
	if (halfway < remainder ||
		(remainder == halfway && (high_significand & 1u) != 0u))
	{
		++high_significand;
	}
	auto const high_integer{
		static_cast<__int128_t>(static_cast<__uint128_t>(high_significand) << shift)};
	auto residual{static_cast<__int128_t>(significand) - high_integer};
	/*
	Rounding 53 retained bits can produce the 54-bit integer 2^53.  Replacing it
	by 2^52 and incrementing its binary exponent is an exact normalization, not
	a second rounding.  The pre-normalized `high_integer` above must remain in
	the residual subtraction because it is H expressed on Q's original grid.
	The preceding IBM-maximum comparison proves that the normalized component's
	top exponent cannot become 1024.
	*/
	auto high_exponent{static_cast<::std::int_least32_t>(
		exponent + static_cast<::std::int_least32_t>(shift))};
	if (high_significand == (UINT64_C(1) << 53u))
	{
		high_significand >>= 1u;
		++high_exponent;
	}
	auto const high{
		::fast_io::details::ibm_double_double_make_binary64(
			high_significand, high_exponent, negative)};
	auto const residual_negative{residual < 0};
	auto const residual_magnitude{residual_negative
		? static_cast<::std::uint_least64_t>(-residual)
		: static_cast<::std::uint_least64_t>(residual)};
	auto const low{
		::fast_io::details::ibm_double_double_make_binary64(
			residual_magnitude, exponent,
			negative != residual_negative)};
	::fast_io::details::fp_assign_ibm_double_double_components(value, high, low);
	return true;
}
#endif

template <typename flt>
inline constexpr void fp_assign_bits(flt &value, typename iec559_traits<flt>::mantissa_type bits) noexcept
{
	using mantissa_type = typename iec559_traits<flt>::mantissa_type;
	static_assert(!::fast_io::details::fp_floating_point_is_float80<flt>,
				  "use fp_assign_float80_bits for IEC 60559 extended precision");
	static_assert(!::fast_io::details::fp_floating_point_is_ibm_double_double<flt>,
				  "use the IBM double-double component assignment bridge");
	static_assert(sizeof(flt) == sizeof(mantissa_type));
	if (__builtin_is_constant_evaluated())
	{
		value = ::fast_io::bit_cast<flt>(bits);
	}
	else
	{
#if FAST_IO_HAS_BUILTIN(__builtin_memcpy)
		__builtin_memcpy
#else
		::std::memcpy
#endif
			(__builtin_addressof(value), __builtin_addressof(bits), sizeof(flt));
	}
}

template <typename flt>
inline constexpr void fp_assign_infinity(flt &value, bool sign) noexcept
{
	using trait = iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr ::std::size_t mbits{trait::mbits};
	constexpr ::std::size_t ebits{trait::ebits};
	if constexpr (::fast_io::details::fp_floating_point_is_float80<flt>)
	{
		::fast_io::details::fp_assign_float80_bits(value, ::std::uint_least64_t{1} << mbits,
												   (static_cast<::std::uint_least32_t>(1u) << ebits) - 1u, sign);
	}
	else if constexpr (
		::fast_io::details::fp_floating_point_is_ibm_double_double<flt>)
	{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
		auto high_bits{UINT64_C(0x7ff0000000000000)};
		if (sign)
		{
			high_bits |= UINT64_C(0x8000000000000000);
		}
		::fast_io::details::fp_assign_ibm_double_double_components(
			value, ::fast_io::bit_cast<double>(high_bits), 0.0);
#endif
	}
	else
	{
		constexpr mantissa_type exponent_mask{(static_cast<mantissa_type>(1) << ebits) - 1};
		mantissa_type bits{static_cast<mantissa_type>(exponent_mask << mbits)};
		if (sign)
		{
			bits |= static_cast<mantissa_type>(static_cast<mantissa_type>(1) << (mbits + ebits));
		}
		::fast_io::details::fp_assign_bits(value, bits);
	}
}

template <typename flt, bool signaling = false, bool indeterminate = false>
inline constexpr void fp_assign_nan(flt &value, bool sign) noexcept
{
	using trait = iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr ::std::size_t mbits{trait::mbits};
	constexpr ::std::size_t ebits{trait::ebits};
	constexpr mantissa_type exponent_mask{(static_cast<mantissa_type>(1) << ebits) - 1};
	constexpr mantissa_type quiet_bit{fp_quiet_nan_mantissa_mask<mantissa_type, mbits>()};
	if constexpr (::fast_io::details::fp_floating_point_is_float80<flt>)
	{
		constexpr auto explicit_integer_bit{::std::uint_least64_t{1} << mbits};
		::std::uint_least64_t mantissa{explicit_integer_bit |
									   (signaling ? ::std::uint_least64_t{1}
												  : static_cast<::std::uint_least64_t>(quiet_bit))};
		::fast_io::details::fp_assign_float80_bits(value, mantissa, static_cast<::std::uint_least32_t>(exponent_mask),
												   sign || indeterminate);
	}
	else if constexpr (
		::fast_io::details::fp_floating_point_is_ibm_double_double<flt>)
	{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
		::std::uint_least64_t high_bits{
			signaling ? UINT64_C(0x7ff0000000000001)
					  : UINT64_C(0x7ff8000000000000)};
		if (sign || indeterminate)
		{
			high_bits |= UINT64_C(0x8000000000000000);
		}
		::fast_io::details::fp_assign_ibm_double_double_components(
			value, ::fast_io::bit_cast<double>(high_bits), 0.0);
#endif
	}
	else
	{
		mantissa_type mantissa{signaling ? static_cast<mantissa_type>(1) : quiet_bit};
		mantissa_type bits{static_cast<mantissa_type>((exponent_mask << mbits) | mantissa)};
		if (sign || indeterminate)
		{
			bits |= static_cast<mantissa_type>(static_cast<mantissa_type>(1) << (mbits + ebits));
		}
		::fast_io::details::fp_assign_bits(value, bits);
	}
}

template <typename flt>
inline constexpr flt fp_make_infinity(bool sign) noexcept
{
	flt value;
	::fast_io::details::fp_assign_infinity(value, sign);
	return value;
}

template <typename flt, bool signaling = false, bool indeterminate = false>
inline constexpr flt fp_make_nan(bool sign) noexcept
{
	flt value;
	::fast_io::details::fp_assign_nan<flt, signaling, indeterminate>(value, sign);
	return value;
}

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *prsv_fp_nan_impl(char_type *iter, bool isnan) noexcept
{
	if (isnan)
	{
		return prsv_fp_nan_literal_impl<uppercase>(iter);
	}
	return prsv_fp_inf_literal_impl<uppercase>(iter);
}

template <bool showpos, bool uppercase, bool nan_show_sign, bool nan_show_type, ::std::size_t mbits,
		  typename mantissa_type, ::std::integral char_type>
inline constexpr char_type *prsv_fp_nan_impl(char_type *iter, mantissa_type mantissa, bool sign) noexcept
{
	if (mantissa == 0)
	{
		iter = print_rsv_fp_sign_impl<showpos>(iter, sign);
		return prsv_fp_inf_literal_impl<uppercase>(iter);
	}

	if constexpr (nan_show_sign)
	{
		iter = print_rsv_fp_sign_impl<showpos>(iter, sign);
	}
	iter = prsv_fp_nan_literal_impl<uppercase>(iter);
	if constexpr (nan_show_type)
	{
		constexpr mantissa_type quiet_bit{fp_quiet_nan_mantissa_mask<mantissa_type, mbits>()};
		if (sign && mantissa == quiet_bit)
		{
			return prsv_fp_nan_ind_literal_impl<uppercase>(iter);
		}
		if (fp_nan_is_signaling<mantissa_type, mbits>(mantissa))
		{
			return prsv_fp_nan_snan_literal_impl<uppercase>(iter);
		}
	}
	return iter;
}

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *prsv_fp_hex_0(char_type *iter) noexcept
{
	if constexpr (!::fast_io::details::is_ascii<char_type> &&
		(::std::same_as<char_type, char> || ::std::same_as<char_type, wchar_t>))
	{
		if constexpr (uppercase)
		{
			return copy_floating_ascii_literal(u8"0P+0", iter);
		}
		else
		{
			return copy_floating_ascii_literal(u8"0p+0", iter);
		}
	}
	if constexpr (uppercase)
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("0P+0", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"0P+0", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"0P+0", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"0P+0", iter);
		}
		else
		{
			return copy_string_literal(u8"0P+0", iter);
		}
	}
	else
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("0p+0", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"0p+0", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"0p+0", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"0p+0", iter);
		}
		else
		{
			return copy_string_literal(u8"0p+0", iter);
		}
	}
}

template <bool comma = false, ::std::integral char_type>
inline constexpr char_type *prsv_fp_hex1d(char_type *iter) noexcept
{
	if constexpr (!::fast_io::details::is_ascii<char_type> &&
		(::std::same_as<char_type, char> || ::std::same_as<char_type, wchar_t>))
	{
		if constexpr (comma)
		{
			return copy_floating_ascii_literal(u8"1,", iter);
		}
		else
		{
			return copy_floating_ascii_literal(u8"1.", iter);
		}
	}
	if constexpr (comma)
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("1,", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"1,", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"1,", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"1,", iter);
		}
		else
		{
			return copy_string_literal(u8"1,", iter);
		}
	}
	else
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("1.", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"1.", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"1.", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"1.", iter);
		}
		else
		{
			return copy_string_literal(u8"1.", iter);
		}
	}
}

template <bool comma = false, ::std::integral char_type>
inline constexpr char_type *prsv_fp_hex0d(char_type *iter) noexcept
{
	if constexpr (!::fast_io::details::is_ascii<char_type> &&
		(::std::same_as<char_type, char> || ::std::same_as<char_type, wchar_t>))
	{
		if constexpr (comma)
		{
			return copy_floating_ascii_literal(u8"0,", iter);
		}
		else
		{
			return copy_floating_ascii_literal(u8"0.", iter);
		}
	}
	if constexpr (comma)
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("0,", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"0,", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"0,", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"0,", iter);
		}
		else
		{
			return copy_string_literal(u8"0,", iter);
		}
	}
	else
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("0.", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"0.", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"0.", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"0.", iter);
		}
		else
		{
			return copy_string_literal(u8"0.", iter);
		}
	}
}

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *prsv_fp_hex0p0(char_type *iter) noexcept
{
	if constexpr (!::fast_io::details::is_ascii<char_type> &&
		(::std::same_as<char_type, char> || ::std::same_as<char_type, wchar_t>))
	{
		if constexpr (uppercase)
		{
			return copy_floating_ascii_literal(u8"0P+0", iter);
		}
		else
		{
			return copy_floating_ascii_literal(u8"0p+0", iter);
		}
	}
	if constexpr (uppercase)
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("0P+0", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"0P+0", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"0P+0", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"0P+0", iter);
		}
		else
		{
			return copy_string_literal(u8"0P+0", iter);
		}
	}
	else
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("0p+0", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"0p+0", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"0p+0", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"0p+0", iter);
		}
		else
		{
			return copy_string_literal(u8"0p+0", iter);
		}
	}
}

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *prsv_fp_dece0(char_type *iter) noexcept
{
	/*
	Scientific charconv requires one digit before the radix and an exponent
	with at least two decimal digits.  Zero has exact coefficient 0 and
	exponent 0, so the unique shortest spelling in this grammar is `0e+00`
	(or its uppercase image).  Each branch below is the same five-code-unit
	abstract spelling in the destination execution character type; using
	typed literals avoids assuming that narrow `char` is ASCII.
	*/
	if constexpr (!::fast_io::details::is_ascii<char_type> &&
		(::std::same_as<char_type, char> || ::std::same_as<char_type, wchar_t>))
	{
		if constexpr (uppercase)
		{
			return copy_floating_ascii_literal(u8"0E+00", iter);
		}
		else
		{
			return copy_floating_ascii_literal(u8"0e+00", iter);
		}
	}
	else if constexpr (uppercase)
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("0E+00", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"0E+00", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"0E+00", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"0E+00", iter);
		}
		else
		{
			return copy_string_literal(u8"0E+00", iter);
		}
	}
	else
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("0e+00", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"0e+00", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"0e+00", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"0e+00", iter);
		}
		else
		{
			return copy_string_literal(u8"0e+00", iter);
		}
	}
}

// The extracted sign is exactly zero or one; changing its storage carrier does
// not change its logical domain.  For float and double, GCC 14--16 on Linux
// System V x86-64 LP64 otherwise materialize and later reload the tail padding
// of this returned aggregate in the scalar DA entry.  A 32-bit carrier occupies
// that existing padding without changing sizeof or alignment and removes those
// stores.  Every other floating type retains bool because no equivalent
// code-generation evidence exists for its representation or conversion path.
// Paired current/candidate runs on i9-14900HX improved GCC 14 binary64 by about
// 4--5% and GCC 15/16 binary32/binary64 by 19--45%; GCC 13, Clang 23 and the
// other ABIs did not show the same lowering defect and retain bool.  GCC 14 is
// therefore the measured lower bound, and later GNU frontends inherit the
// latest proved aggregate layout unless a complete-caller counterexample is
// found.  SSE4.1 and SSSE3 close the policy to the audited DA backend rather
// than extending a compiler-code-generation result to an unmeasured x86
// baseline ISA.
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__SSE4_1__) && defined(__SSSE3__) && defined(__GNUC__) && \
	!defined(__clang__) && 14 <= __GNUC__ && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
template <typename flt>
using punning_sign_type = ::std::conditional_t<
	::std::same_as<::std::remove_cv_t<flt>, float> ||
	::std::same_as<::std::remove_cv_t<flt>, double>,
	::std::uint_least32_t, bool>;
#else
template <typename>
using punning_sign_type = bool;
#endif

template <typename flt>
struct punning_result
{
	typename iec559_traits<flt>::mantissa_type mantissa;
	::std::uint_least32_t exponent;
	::fast_io::details::punning_sign_type<flt> sign;
};

#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
/*
The exact decimal backends consume an IEC-like `(fraction, exponent, sign)`
carrier.  IBM double-double has no such object field, but every canonical
finite value is nevertheless one dyadic S*2^e with at most 106 significant
bits.  The following adapter embeds that *value* (not its object bytes) in a
synthetic p=106 binary carrier.  Normal values use the binary64 bias solely so
existing exact-expansion algebra reconstructs

  ((2^105 + fraction) * 2^(raw-1023-105)) = S*2^e.

For magnitudes below binary64's normal threshold, raw exponent zero fixes the
synthetic quantum at 2^-1127; every actual IBM subnormal is a multiple of
2^-1074 and therefore embeds by a nonnegative left shift.  The adapter is not
a claim that IBM's adjacent-value lattice is uniform.  Shortest conversion
uses the real neighbors separately; only exact value expansion and
presentation consume these fields.
*/
template <typename flt>
	requires(::fast_io::details::fp_floating_point_is_ibm_double_double<flt>)
inline constexpr punning_result<flt> get_punned_result(flt value) noexcept
{
	auto const storage{
		::fast_io::bit_cast<::fast_io::details::ibm_double_double_storage>(value)};
	auto const high_bits{
		::fast_io::bit_cast<::std::uint_least64_t>(storage.high)};
	auto const high_fraction{high_bits & UINT64_C(0x000fffffffffffff)};
	auto const high_exponent{
		static_cast<::std::uint_least32_t>((high_bits >> 52u) & 0x7ffu)};
	auto const negative{static_cast<bool>(high_bits >> 63u)};
	if (high_exponent == 0x7ffu)
	{
		return {static_cast<__uint128_t>(high_fraction) << 53u,
			0x7ffu, negative};
	}
	auto const dyadic{
		::fast_io::details::get_ibm_double_double_dyadic(value)};
	if (!dyadic.success || !dyadic.significand)
	{
		return {0u, 0u, negative};
	}
	auto const width{
		::fast_io::details::ibm_double_double_u128_bit_width(
			dyadic.significand)};
	if (106u < width)
	{
		::fast_io::fast_terminate();
	}
	auto const top_exponent{static_cast<::std::int_least32_t>(
		dyadic.exponent + static_cast<::std::int_least32_t>(width - 1u))};
	if (-1022 <= top_exponent)
	{
		auto const complete{dyadic.significand << (106u - width)};
		return {complete - (static_cast<__uint128_t>(1u) << 105u),
			static_cast<::std::uint_least32_t>(top_exponent + 1023),
			dyadic.negative};
	}
	auto const subnormal_shift{
		static_cast<::std::int_least32_t>(dyadic.exponent + 1127)};
	if (subnormal_shift < 0)
	{
		::fast_io::fast_terminate();
	}
	return {dyadic.significand << static_cast<unsigned>(subnormal_shift),
		0u, dyadic.negative};
}
#endif

struct
#if __has_cpp_attribute(__gnu__::__packed__)
	[[__gnu__::__packed__]]
#endif
	float80_result
{
	::std::uint_least64_t mantissa;
	::std::uint_least16_t exponent;
};

#if defined(__SIZEOF_FLOAT80__) ||                                                                            \
	(defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && __LDBL_MANT_DIG__ == 64 &&                    \
	 __LDBL_MAX_EXP__ == 16384)
template <::std::size_t padding_size>
struct
#if __has_cpp_attribute(__gnu__::__packed__)
	[[__gnu__::__packed__]]
#endif
	float80_storage
{
	::std::uint_least64_t mantissa;
	::std::uint_least16_t exponent;
	unsigned char padding[padding_size];
};

template <>
struct
#if __has_cpp_attribute(__gnu__::__packed__)
	[[__gnu__::__packed__]]
#endif
	float80_storage<0>
{
	::std::uint_least64_t mantissa;
	::std::uint_least16_t exponent;
};
#endif

/// @brief Decomposes a native floating scalar through its ordinary by-value ABI.
/// @details Keeping the parameter by value lets ordinary callers retain the floating-point register class.  An upper
/// CPO extracts fields from the owning object for representation-sensitive exceptional ABIs before reaching this API.
template <typename flt>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr punning_result<flt> get_punned_result(flt f) noexcept
{
	using trait = iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr ::std::size_t mbits{trait::mbits};
	constexpr ::std::size_t ebits{trait::ebits};
	constexpr ::std::size_t total_bits{mbits + ebits};
	constexpr mantissa_type mantissa_mask{(static_cast<mantissa_type>(1) << mbits) - 1};
	constexpr mantissa_type exponent_mask{(static_cast<mantissa_type>(1) << ebits) - 1};

	// Native MSVC in C++20 mode provides `__builtin_bit_cast` with the
	// representation-preserving contract advertised by `__cpp_lib_bit_cast`, but
	// does not consistently expose it through the `__has_builtin` probe used by
	// FAST_IO_HAS_BUILTIN.  Clang-cl takes the ordinary capability branch when it
	// advertises the builtin.  The fallback has identical bits; this split avoids
	// inferring any ABI or arithmetic difference from compiler identity.
	auto unwrap =
#if FAST_IO_HAS_BUILTIN(__builtin_bit_cast)
		__builtin_bit_cast(mantissa_type, f)
#elif defined(_MSC_VER) && __cpp_lib_bit_cast >= 201806L
		__builtin_bit_cast(mantissa_type, f)
#else
		bit_cast<mantissa_type>(f)
#endif
		;
	return {static_cast<mantissa_type>(unwrap & mantissa_mask),
			static_cast<::std::uint_least32_t>((unwrap >> mbits) & exponent_mask),
			static_cast<bool>((unwrap >> total_bits) & 1u)};
}

#if (defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && __LDBL_MANT_DIG__ == 64 &&                    \
	 __LDBL_MAX_EXP__ == 16384) &&                                                                             \
	(!defined(__BYTE_ORDER__) || __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
template <typename flt>
	requires(::std::same_as<::std::remove_cv_t<flt>, long double> &&
			 ::std::numeric_limits<long double>::digits == 64 &&
			 ::std::numeric_limits<long double>::max_exponent == 16384)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr punning_result<flt> get_punned_result(flt f) noexcept
{
	static_assert(sizeof(flt) >= sizeof(::std::uint_least64_t) + sizeof(::std::uint_least16_t));
	using storage_type = float80_storage<sizeof(flt) - sizeof(::std::uint_least64_t) - sizeof(::std::uint_least16_t)>;
	// Native MSVC's C++20 feature macro is the audited capability fallback for
	// `__builtin_bit_cast` when `__has_builtin` is unavailable.  Both selected
	// operations copy the complete object representation into the same storage;
	// the frontend branch changes neither the binary80 fields nor padding bytes.
	auto unwrap =
#if FAST_IO_HAS_BUILTIN(__builtin_bit_cast)
		__builtin_bit_cast(storage_type, f)
#elif defined(_MSC_VER) && __cpp_lib_bit_cast >= 201806L
		__builtin_bit_cast(storage_type, f)
#else
		bit_cast<storage_type>(f)
#endif
		;
	constexpr ::std::uint_least64_t explicit_integer_bit{::std::uint_least64_t{1} << 63u};
	return {unwrap.mantissa & static_cast<::std::uint_least64_t>(explicit_integer_bit - 1u),
			static_cast<::std::uint_least32_t>(unwrap.exponent & 0x7fffu),
			static_cast<bool>((unwrap.exponent >> 15u) & 1u)};
}
#endif

#if defined(__SIZEOF_FLOAT80__) && (!defined(__BYTE_ORDER__) || __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
template <>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr punning_result<__float80> get_punned_result<__float80>(__float80 f) noexcept
{
	static_assert(sizeof(__float80) >= sizeof(::std::uint_least64_t) + sizeof(::std::uint_least16_t));
	using storage_type = float80_storage<sizeof(__float80) - sizeof(::std::uint_least64_t) - sizeof(::std::uint_least16_t)>;
	// This repeats the long-double capability boundary deliberately: native MSVC
	// may omit `__has_builtin` while providing the C++20 bit-cast intrinsic.
	// Every branch preserves all bytes before the common binary80 field decode,
	// so compiler selection cannot affect sign, exponent or significand semantics.
	auto unwrap =
#if FAST_IO_HAS_BUILTIN(__builtin_bit_cast)
		__builtin_bit_cast(storage_type, f)
#elif defined(_MSC_VER) && __cpp_lib_bit_cast >= 201806L
		__builtin_bit_cast(storage_type, f)
#else
		bit_cast<storage_type>(f)
#endif
		;
	constexpr ::std::uint_least64_t explicit_integer_bit{::std::uint_least64_t{1} << 63u};
	return {unwrap.mantissa & static_cast<::std::uint_least64_t>(explicit_integer_bit - 1u),
			static_cast<::std::uint_least32_t>(unwrap.exponent & 0x7fffu),
			static_cast<bool>((unwrap.exponent >> 15u) & 1u)};
}
#endif

template <bool showpos, ::std::integral char_type>
inline constexpr char_type *print_rsv_fp_sign_impl(char_type *iter, bool sign) noexcept
{
	if constexpr (showpos)
	{
		*iter = sign ? char_literal_v<u8'-', char_type> : char_literal_v<u8'+', char_type>;
		++iter;
	}
	else
	{
		if (sign)
		{
			*iter = char_literal_v<u8'-', char_type>;
			++iter;
		}
	}
	return iter;
}

template <::std::integral char_type, my_unsigned_integral U>
#if __has_cpp_attribute(__gnu__::__hot__)
[[__gnu__::__hot__]]
#endif
inline constexpr char_type *prt_rsv_hundred_flt_impl(char_type *iter, U u) noexcept
{
	return non_overlapped_copy_n(::fast_io::details::digits_table<char_type, 10, false> + (u << 1), 2, iter);
}

template <::std::size_t mxdigits, bool indent, ::std::integral char_type, my_unsigned_integral U>
inline constexpr char_type *prt_rsv_exponent_impl(char_type *iter, U u) noexcept
{
	if constexpr (mxdigits == 0)
	{
		return iter;
	}
	else if constexpr (mxdigits == 1)
	{
		*iter = ::fast_io::char_literal_add<char_type>(u);
		++iter;
		return iter;
	}
	else
	{
		constexpr U ten{10u};
		constexpr U hundred{100u};
		constexpr U thousand{1000u};
		if constexpr (mxdigits == 2)
		{
			if constexpr (indent)
			{
				return prt_rsv_hundred_flt_impl(iter, u);
			}
			else
			{
				if (u >= ten)
				{
					return prt_rsv_hundred_flt_impl(iter, u);
				}
				else
				{
					*iter = ::fast_io::char_literal_add<char_type>(u);
					++iter;
					return iter;
				}
			}
		}
		else if constexpr (mxdigits == 3)
		{
			if constexpr (indent)
			{
				if (u >= hundred)
				{
					U div100{u / hundred};
					U mod100{u % hundred};
					*iter = ::fast_io::char_literal_add<char_type>(div100);
					++iter;
					u = mod100;
				}
				return prt_rsv_hundred_flt_impl(iter, u);
			}
			else
			{
				if (u >= hundred)
				{
					U div100{u / hundred};
					U mod100{u % hundred};
					*iter = ::fast_io::char_literal_add<char_type>(div100);
					++iter;
					return prt_rsv_hundred_flt_impl(iter, mod100);
				}
				else if (u < ten)
				{
					*iter = ::fast_io::char_literal_add<char_type>(u);
					++iter;
					return iter;
				}
				return prt_rsv_hundred_flt_impl(iter, u);
			}
		}
		else if constexpr (mxdigits == 4)
		{
			if constexpr (indent)
			{
				if (u < hundred)
				{
					return prt_rsv_hundred_flt_impl(iter, u);
				}
				::std::size_t sz(3);
				if (u >= thousand)
				{
					sz = 4;
				}
				print_reserve_integral_main_impl<10, false>(iter += sz, u, sz);
			}
			else
			{
				::std::size_t sz(1);
				if (u >= thousand)
				{
					sz = 4;
				}
				else if (u >= hundred)
				{
					sz = 3;
				}
				else if (u >= ten)
				{
					sz = 2;
				}
				print_reserve_integral_main_impl<10, false>(iter += sz, u, sz);
			}
			return iter;
		}
		else if constexpr (mxdigits == 5)
		{
			constexpr U tenthousand{10000u};
			if constexpr (indent)
			{
				if (u < hundred)
				{
					return prt_rsv_hundred_flt_impl(iter, u);
				}
				::std::size_t sz(3);
				if (u >= tenthousand)
				{
					sz = 5;
				}
				else if (u >= thousand)
				{
					sz = 4;
				}
				print_reserve_integral_main_impl<10, false>(iter += sz, u, sz);
			}
			else
			{
				::std::size_t sz(1);
				if (u >= tenthousand)
				{
					sz = 5;
				}
				else if (u >= thousand)
				{
					sz = 4;
				}
				else if (u >= hundred)
				{
					sz = 3;
				}
				else if (u >= ten)
				{
					sz = 2;
				}
				print_reserve_integral_main_impl<10, false>(iter += sz, u, sz);
			}
			return iter;
		}
		else
		{
			if constexpr (indent)
			{
				if (u < hundred)
				{
					return prt_rsv_hundred_flt_impl(iter, u);
				}
			}
			::std::size_t sz{chars_len<10, false>(u)};
			auto temp{iter + sz};
			print_reserve_integral_main_impl<10, false>(temp, u, sz);
			return temp;
		}
	}
}

} // namespace fast_io::details
