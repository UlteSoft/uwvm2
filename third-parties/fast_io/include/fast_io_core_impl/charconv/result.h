#pragma once

/*
The ordinary C++20 feature-test macro for to_chars is not evidence that
<charconv> is usable in a freestanding translation: pre-C++23 libraries were
required to publish hosted feature macros from <version> even when their
headers were unavailable. Prefer either a dedicated freestanding declaration
or the meta-macro that guarantees ordinary library feature macros are truthful;
otherwise trust the implementation's hosted-mode predicate. This keeps a
hosted build ABI-identical to the standard vocabulary while allowing a
genuinely freestanding build to use fast_io's equivalent local vocabulary.
*/
#if (defined(__cpp_lib_freestanding_charconv) &&                  \
	 __cpp_lib_freestanding_charconv != 0) ||                      \
	(defined(__cpp_lib_freestanding_feature_test_macros) &&        \
	 __cpp_lib_freestanding_feature_test_macros != 0 &&            \
	 defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L)
#define FAST_IO_DETAILS_HAS_STD_CHARCONV 1
#elif defined(__GLIBCXX__)
#if defined(_GLIBCXX_HOSTED) && _GLIBCXX_HOSTED == 1
#define FAST_IO_DETAILS_HAS_STD_CHARCONV 1
#else
#define FAST_IO_DETAILS_HAS_STD_CHARCONV 0
#endif
#elif defined(_LIBCPP_VERSION)
#if !defined(_LIBCPP_FREESTANDING)
#define FAST_IO_DETAILS_HAS_STD_CHARCONV 1
#else
#define FAST_IO_DETAILS_HAS_STD_CHARCONV 0
#endif
#elif !defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 1
#define FAST_IO_DETAILS_HAS_STD_CHARCONV 1
#else
#define FAST_IO_DETAILS_HAS_STD_CHARCONV 0
#endif

#if FAST_IO_DETAILS_HAS_STD_CHARCONV
#include <charconv>
#endif

namespace fast_io
{

/*
Hosted builds deliberately alias the standard C++20 vocabulary, so every
public overload and result remains exactly type- and ABI-compatible with
std::chars_format, std::errc, and the standard result structures.  A standard
freestanding implementation is not required to provide those declarations;
the fallback therefore models only fast_io's name-based conversion contract
and never assumes the implementation-defined numeric values of std::errc or
std::chars_format.
*/
#if FAST_IO_DETAILS_HAS_STD_CHARCONV
using chars_format = ::std::chars_format;
using charconv_errc = ::std::errc;
#else
enum class chars_format : unsigned char
{
	scientific = 1u,
	fixed = 2u,
	hex = 4u,
	general = scientific | fixed
};

// chars_format is a bitmask protocol. These operators derive solely from the
// local enum and do not import or compare any standard-library representation.
inline constexpr chars_format operator~(chars_format value) noexcept
{
	return static_cast<chars_format>(
		~static_cast<unsigned char>(value));
}

inline constexpr chars_format operator&(chars_format lhs,
								 chars_format rhs) noexcept
{
	return static_cast<chars_format>(static_cast<unsigned char>(lhs) &
									 static_cast<unsigned char>(rhs));
}

inline constexpr chars_format operator|(chars_format lhs,
								 chars_format rhs) noexcept
{
	return static_cast<chars_format>(static_cast<unsigned char>(lhs) |
									 static_cast<unsigned char>(rhs));
}

inline constexpr chars_format operator^(chars_format lhs,
								 chars_format rhs) noexcept
{
	return static_cast<chars_format>(static_cast<unsigned char>(lhs) ^
									 static_cast<unsigned char>(rhs));
}

inline constexpr chars_format &operator&=(chars_format &lhs,
									  chars_format rhs) noexcept
{
	return lhs = lhs & rhs;
}

inline constexpr chars_format &operator|=(chars_format &lhs,
									  chars_format rhs) noexcept
{
	return lhs = lhs | rhs;
}

inline constexpr chars_format &operator^=(chars_format &lhs,
									  chars_format rhs) noexcept
{
	return lhs = lhs ^ rhs;
}

enum class charconv_errc : unsigned char
{
	success,
	invalid_argument,
	result_out_of_range,
	value_too_large
};
#endif

namespace details
{

template <::fast_io::details::character char_type>
struct basic_to_chars_result_impl
{
	char_type *ptr;
	::fast_io::charconv_errc ec;
};

template <::fast_io::details::character char_type>
struct basic_from_chars_result_impl
{
	char_type const *ptr;
	::fast_io::charconv_errc ec;
};

} // namespace details

#if FAST_IO_DETAILS_HAS_STD_CHARCONV
template <::fast_io::details::character char_type>
using basic_to_chars_result = ::std::conditional_t<
	::std::same_as<char_type, char>, ::std::to_chars_result,
	::fast_io::details::basic_to_chars_result_impl<char_type>>;

template <::fast_io::details::character char_type>
using basic_from_chars_result = ::std::conditional_t<
	::std::same_as<char_type, char>, ::std::from_chars_result,
	::fast_io::details::basic_from_chars_result_impl<char_type>>;
#else
template <::fast_io::details::character char_type>
using basic_to_chars_result = ::fast_io::details::basic_to_chars_result_impl<char_type>;

template <::fast_io::details::character char_type>
using basic_from_chars_result = ::fast_io::details::basic_from_chars_result_impl<char_type>;
#endif

using to_chars_result = ::fast_io::basic_to_chars_result<char>;
using from_chars_result = ::fast_io::basic_from_chars_result<char>;

} // namespace fast_io

#undef FAST_IO_DETAILS_HAS_STD_CHARCONV
