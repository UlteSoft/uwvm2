#pragma once

namespace fast_io
{

template <::std::integral ch_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ch_type char_literal(char8_t ch) noexcept
{
	using clean_type = ::std::remove_cv_t<ch_type>;
	if constexpr (!::fast_io::details::is_ascii<clean_type> &&
				  (::std::same_as<clean_type, char> ||
				   ::std::same_as<clean_type, wchar_t>))
	{
		return static_cast<ch_type>(
			::fast_io::details::execution_character_literal<clean_type>(ch));
	}
	using unsigned_t = ::std::make_unsigned_t<clean_type>;
	return static_cast<ch_type>(static_cast<unsigned_t>(ch));
}
template <char8_t ch, ::std::integral ch_type>
inline constexpr ch_type char_literal_v{char_literal<ch_type>(ch)};

template <::std::integral ch_type>
inline constexpr ch_type const *null_terminated_c_str() noexcept
{
	if constexpr (::std::same_as<ch_type, char>)
	{
		return "";
	}
	else if constexpr (::std::same_as<ch_type, wchar_t>)
	{
		return L"";
	}
	else if constexpr (::std::same_as<ch_type, char8_t>)
	{
		return u8"";
	}
	else if constexpr (::std::same_as<ch_type, char16_t>)
	{
		return u"";
	}
	else if constexpr (::std::same_as<ch_type, char32_t>)
	{
		return U"";
	}
	else
	{
		return __builtin_addressof(::fast_io::char_literal_v<0, ch_type>);
	}
}

template <::std::integral ch_type>
inline constexpr ch_type const *null_terminated_c_str_v{::fast_io::null_terminated_c_str<ch_type>()};

template <::std::integral ch_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ch_type arithmetic_char_literal(char8_t ch) noexcept
{
	using clean_type = ::std::remove_cv_t<ch_type>;
	if constexpr (::std::same_as<clean_type, wchar_t> &&
				  ::fast_io::details::wide_is_none_utf_endian)
	{
		using unsigned_t = ::std::make_unsigned_t<clean_type>;
		return static_cast<ch_type>(static_cast<unsigned_t>(ch));
	}
	else if constexpr (::std::same_as<clean_type, wchar_t> &&
					   ::fast_io::details::wide_is_none_ebcdic_endian)
	{
		using unsigned_t = ::std::make_unsigned_t<clean_type>;
		return static_cast<ch_type>(::fast_io::byte_swap(
			static_cast<unsigned_t>(::fast_io::char_literal<clean_type>(ch))));
	}
	else if constexpr (::std::same_as<clean_type, wchar_t> &&
					   ::fast_io::details::wide_is_none_single_byte_endian)
	{
		using unsigned_t = ::std::make_unsigned_t<clean_type>;
		return static_cast<ch_type>(::fast_io::byte_swap(
			static_cast<unsigned_t>(::fast_io::char_literal<clean_type>(ch))));
	}
	else
	{
		return static_cast<ch_type>(char_literal<clean_type>(ch));
	}
}

template <::fast_io::details::my_integral ch_type>
inline constexpr auto integral_lifting(ch_type c) noexcept
{
	return static_cast<decltype(c + c)>(c);
}

template <char8_t ch, ::std::integral ch_type>
inline constexpr ch_type arithmetic_char_literal_v{arithmetic_char_literal<ch_type>(ch)};

template <::std::integral char_type, char8_t ch = u8'0', ::fast_io::details::my_integral T>
inline constexpr char_type char_literal_add(T offs) noexcept
{
	if constexpr (ch == u8'0')
	{
		FAST_IO_ASSUME(0 <= offs && offs < 10);
	}
	/*
	A non-native wide execution charset is not required to represent a source
	character as a bare arithmetic code value.  GCC can, for example, retain
	conversion-state bytes in an IBM single-byte wide literal.  When none of the
	known continuous layouts below describes the representation, adding to the
	encoded integer mutates a state byte rather than advancing the abstract
	character.  Map the resulting ASCII semantic character through the execution
	charset in that uncommon case.  ASCII, classic EBCDIC, and the recognized
	non-native-endian layouts retain the original arithmetic fast path.
	*/
	if constexpr (!::fast_io::details::is_ascii<char_type> &&
		(::std::same_as<char_type, char> || ::std::same_as<char_type, wchar_t>) &&
		!::fast_io::details::is_classic_ebcdic<char_type> &&
		!(::std::same_as<char_type, wchar_t> &&
		  ::fast_io::details::wide_is_none_execution_endian))
	{
		return ::fast_io::char_literal<char_type>(static_cast<char8_t>(
			static_cast<::fast_io::details::my_make_unsigned_t<T>>(ch) +
			static_cast<::fast_io::details::my_make_unsigned_t<T>>(offs)));
	}
	// The future cpp standard prohibits arithmetic between different char_types, but allows the same char_type to be arithmetic, so doing an integral_lifting
	// and then calculating gives the same result as direct arithmetic

	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_result_type = ::fast_io::details::my_make_unsigned_t<
		::std::remove_cvref_t<decltype(integral_lifting(arithmetic_char_literal_v<ch, char_type>) + integral_lifting(offs))>>;

	if constexpr (::std::same_as<char_type, wchar_t> &&
				  ::fast_io::details::wide_is_none_execution_endian)
	{
		static_assert(::std::numeric_limits<::std::uint_least8_t>::digits <= ::std::numeric_limits<wchar_t>::digits);
		constexpr unsigned leftshift_offset{static_cast<unsigned>(::std::numeric_limits<unsigned_char_type>::digits -
																  ::std::numeric_limits<::std::uint_least8_t>::digits)};
		return static_cast<char_type>(static_cast<unsigned_char_type>(static_cast<unsigned_result_type>(
										  static_cast<unsigned_result_type>(
											  integral_lifting(arithmetic_char_literal_v<ch, char_type>)) +
										  static_cast<unsigned_result_type>(integral_lifting(offs))))
									  << leftshift_offset);
	}
	else
	{
		return static_cast<char_type>(static_cast<unsigned_char_type>(
			static_cast<unsigned_result_type>(
				integral_lifting(arithmetic_char_literal_v<ch, char_type>)) +
			static_cast<unsigned_result_type>(integral_lifting(offs))));
	}
}

} // namespace fast_io
