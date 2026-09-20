#pragma once

namespace fast_io::details
{

template <::std::integral char_type>
inline constexpr auto execution_character_value(char8_t ch) noexcept
{
	using clean_type = ::std::remove_cvref_t<char_type>;
	using unsigned_type = ::std::make_unsigned_t<clean_type>;
	return static_cast<unsigned_type>(
		::fast_io::details::execution_character_literal<clean_type>(ch));
}

template <::std::integral char_type>
inline constexpr bool exec_charset_is_ascii() noexcept
{
	using clean_type = ::std::remove_cvref_t<char_type>;
	if constexpr (!::std::same_as<clean_type, char> &&
				  !::std::same_as<clean_type, wchar_t>)
	{
		return true;
	}
	else
	{
		constexpr char8_t controls[]{u8'\0', u8'\a', u8'\b', u8'\t',
									 u8'\n', u8'\v', u8'\f', u8'\r'};
		for (auto ch : controls)
		{
			if (::fast_io::details::execution_character_value<clean_type>(ch) !=
				static_cast<::std::make_unsigned_t<clean_type>>(ch))
			{
				return false;
			}
		}
		for (char8_t ch{u8' '}; ch != static_cast<char8_t>(0x7fu); ++ch)
		{
			if (::fast_io::details::execution_character_value<clean_type>(ch) !=
				static_cast<::std::make_unsigned_t<clean_type>>(ch))
			{
				return false;
			}
		}
		return true;
	}
}

template <::std::integral char_type>
inline constexpr bool is_ascii{exec_charset_is_ascii<char_type>()};

template <::std::integral char_type, bool swap_wide = false>
inline constexpr auto normalized_execution_character_value(char8_t ch) noexcept
{
	using clean_type = ::std::remove_cvref_t<char_type>;
	auto value{::fast_io::details::execution_character_value<clean_type>(ch)};
	if constexpr (swap_wide && ::std::same_as<clean_type, wchar_t> &&
				  sizeof(wchar_t) != 1u)
	{
		value = ::fast_io::byte_swap(value);
	}
	return value;
}

template <::std::integral char_type, bool swap_wide = false>
inline constexpr bool exec_charset_has_ebcdic_invariant_layout() noexcept
{
	using clean_type = ::std::remove_cvref_t<char_type>;
	if constexpr (!::std::same_as<clean_type, char> &&
				  !::std::same_as<clean_type, wchar_t>)
	{
		return false;
	}
	else
	{
		using unsigned_type = ::std::make_unsigned_t<clean_type>;
		if (::fast_io::details::normalized_execution_character_value<clean_type, swap_wide>(u8' ') !=
			static_cast<unsigned_type>(0x40u))
		{
			return false;
		}
		for (char8_t index{}; index != 10u; ++index)
		{
			if (::fast_io::details::normalized_execution_character_value<clean_type, swap_wide>(
					static_cast<char8_t>(u8'0' + index)) !=
				static_cast<unsigned_type>(0xf0u + index))
			{
				return false;
			}
		}
		for (char8_t index{}; index != 26u; ++index)
		{
			unsigned expected{index < 9u    ? 0xc1u + index
							  : index < 18u ? 0xd1u + (index - 9u)
											: 0xe2u + (index - 18u)};
			if (::fast_io::details::normalized_execution_character_value<clean_type, swap_wide>(
					static_cast<char8_t>(u8'A' + index)) !=
				static_cast<unsigned_type>(expected))
			{
				return false;
			}
		}
		return true;
	}
}

template <::std::integral char_type>
inline constexpr bool exec_charset_is_ebcdic() noexcept
{
	using clean_type = ::std::remove_cvref_t<char_type>;
	if constexpr (::std::same_as<clean_type, wchar_t> && sizeof(wchar_t) != 1u)
	{
		return ::fast_io::details::exec_charset_has_ebcdic_invariant_layout<clean_type>() ||
			   ::fast_io::details::exec_charset_has_ebcdic_invariant_layout<clean_type, true>();
	}
	return ::fast_io::details::exec_charset_has_ebcdic_invariant_layout<clean_type>();
}

template <::std::integral char_type>
inline constexpr bool is_ebcdic{exec_charset_is_ebcdic<char_type>()};

template <::std::integral char_type, bool swap_wide = false>
inline constexpr bool exec_charset_has_classic_ebcdic_alnum_layout() noexcept
{
	using clean_type = ::std::remove_cvref_t<char_type>;
	if constexpr (!::fast_io::details::exec_charset_has_ebcdic_invariant_layout<clean_type, swap_wide>())
	{
		return false;
	}
	else
	{
		using unsigned_type = ::std::make_unsigned_t<clean_type>;
		for (char8_t index{}; index != 26u; ++index)
		{
			unsigned expected{index < 9u    ? 0x81u + index
							  : index < 18u ? 0x91u + (index - 9u)
											: 0xa2u + (index - 18u)};
			if (::fast_io::details::normalized_execution_character_value<clean_type, swap_wide>(
					static_cast<char8_t>(u8'a' + index)) !=
				static_cast<unsigned_type>(expected))
			{
				return false;
			}
		}
		return true;
	}
}

template <::std::integral char_type>
inline constexpr bool exec_charset_is_classic_ebcdic() noexcept
{
	using clean_type = ::std::remove_cvref_t<char_type>;
	if constexpr (::std::same_as<clean_type, wchar_t> && sizeof(wchar_t) != 1u)
	{
		return ::fast_io::details::exec_charset_has_classic_ebcdic_alnum_layout<clean_type>() ||
			   ::fast_io::details::exec_charset_has_classic_ebcdic_alnum_layout<clean_type, true>();
	}
	return ::fast_io::details::exec_charset_has_classic_ebcdic_alnum_layout<clean_type>();
}

template <::std::integral char_type>
inline constexpr bool is_classic_ebcdic{
	exec_charset_is_classic_ebcdic<char_type>()};

inline consteval char execution_charset_name_fold_ascii_case(char value) noexcept
{
	switch (value)
	{
	case 'a':
		return 'A';
	case 'b':
		return 'B';
	case 'c':
		return 'C';
	case 'd':
		return 'D';
	case 'e':
		return 'E';
	case 'f':
		return 'F';
	case 'g':
		return 'G';
	case 'h':
		return 'H';
	case 'i':
		return 'I';
	case 'j':
		return 'J';
	case 'k':
		return 'K';
	case 'l':
		return 'L';
	case 'm':
		return 'M';
	case 'n':
		return 'N';
	case 'o':
		return 'O';
	case 'p':
		return 'P';
	case 'q':
		return 'Q';
	case 'r':
		return 'R';
	case 's':
		return 'S';
	case 't':
		return 'T';
	case 'u':
		return 'U';
	case 'v':
		return 'V';
	case 'w':
		return 'W';
	case 'x':
		return 'X';
	case 'y':
		return 'Y';
	case 'z':
		return 'Z';
	default:
		return value;
	}
}

template <::std::size_t text_size, ::std::size_t prefix_size>
inline consteval bool execution_charset_name_starts_with(
	char const (&text)[text_size], char const (&prefix)[prefix_size]) noexcept
{
	if constexpr (text_size < prefix_size)
	{
		return false;
	}
	else
	{
		for (::std::size_t index{}; index + 1u != prefix_size; ++index)
		{
			if (::fast_io::details::execution_charset_name_fold_ascii_case(text[index]) !=
				::fast_io::details::execution_charset_name_fold_ascii_case(prefix[index]))
			{
				return false;
			}
		}
		return true;
	}
}

template <::std::size_t lhs_size, ::std::size_t rhs_size>
inline consteval bool execution_charset_name_is(
	char const (&lhs)[lhs_size], char const (&rhs)[rhs_size]) noexcept
{
	if constexpr (lhs_size != rhs_size)
	{
		return false;
	}
	else
	{
		for (::std::size_t index{}; index != lhs_size; ++index)
		{
			if (::fast_io::details::execution_charset_name_fold_ascii_case(lhs[index]) !=
				::fast_io::details::execution_charset_name_fold_ascii_case(rhs[index]))
			{
				return false;
			}
		}
		return true;
	}
}

template <::std::size_t text_size, ::std::size_t encoding_size>
inline consteval bool execution_charset_name_is_or_has_iconv_suffix(
	char const (&text)[text_size], char const (&encoding)[encoding_size]) noexcept
{
	if constexpr (text_size == encoding_size)
	{
		return ::fast_io::details::execution_charset_name_is(text, encoding);
	}
	else if constexpr (text_size < encoding_size + 2u)
	{
		return false;
	}
	else
	{
		for (::std::size_t index{}; index + 1u != encoding_size; ++index)
		{
			if (::fast_io::details::execution_charset_name_fold_ascii_case(text[index]) !=
				::fast_io::details::execution_charset_name_fold_ascii_case(encoding[index]))
			{
				return false;
			}
		}
		constexpr ::std::size_t encoding_length{encoding_size - 1u};
		return text[encoding_length] == '/' && text[encoding_length + 1u] == '/';
	}
}

template <::std::integral char_type>
inline constexpr bool exec_charset_is_utf8() noexcept
{
	using clean_type = ::std::remove_cvref_t<char_type>;
	if constexpr (::std::same_as<clean_type, char8_t>)
	{
		return true;
	}
	else if constexpr (::std::same_as<clean_type, char>)
	{
#if defined(_MSVC_EXECUTION_CHARACTER_SET)
		return _MSVC_EXECUTION_CHARACTER_SET == 65001;
#elif defined(__GNUC_EXECUTION_CHARSET_NAME)
		return ::fast_io::details::execution_charset_name_is_or_has_iconv_suffix(
				   __GNUC_EXECUTION_CHARSET_NAME, "UTF-8") ||
			   ::fast_io::details::execution_charset_name_is_or_has_iconv_suffix(
				   __GNUC_EXECUTION_CHARSET_NAME, "UTF8");
#elif defined(__clang_literal_encoding__)
		return ::fast_io::details::execution_charset_name_is_or_has_iconv_suffix(
				   __clang_literal_encoding__, "UTF-8") ||
			   ::fast_io::details::execution_charset_name_is_or_has_iconv_suffix(
				   __clang_literal_encoding__, "UTF8");
#else
		return false;
#endif
	}
	else if constexpr (::std::same_as<clean_type, wchar_t> && sizeof(wchar_t) == 1u)
	{
#if defined(__GNUC_WIDE_EXECUTION_CHARSET_NAME)
		return ::fast_io::details::execution_charset_name_is_or_has_iconv_suffix(
				   __GNUC_WIDE_EXECUTION_CHARSET_NAME, "UTF-8") ||
			   ::fast_io::details::execution_charset_name_is_or_has_iconv_suffix(
				   __GNUC_WIDE_EXECUTION_CHARSET_NAME, "UTF8");
#elif defined(__clang_wide_literal_encoding__)
		return ::fast_io::details::execution_charset_name_is_or_has_iconv_suffix(
				   __clang_wide_literal_encoding__, "UTF-8") ||
			   ::fast_io::details::execution_charset_name_is_or_has_iconv_suffix(
				   __clang_wide_literal_encoding__, "UTF8");
#else
		return false;
#endif
	}
	else
	{
		return false;
	}
}

template <::std::integral char_type>
inline constexpr bool is_utf8_execution_charset{
	exec_charset_is_utf8<char_type>()};

template <::std::integral char_type>
inline constexpr bool exec_charset_is_unicode() noexcept
{
	using clean_type = ::std::remove_cvref_t<char_type>;
	if constexpr (!::std::same_as<clean_type, char> &&
				  !::std::same_as<clean_type, wchar_t>)
	{
		return true;
	}
	else if constexpr (::std::same_as<clean_type, char>)
	{
#if defined(_MSVC_EXECUTION_CHARACTER_SET)
		return _MSVC_EXECUTION_CHARACTER_SET == 65001 ||
			   _MSVC_EXECUTION_CHARACTER_SET == 20127;
#elif defined(__GNUC_EXECUTION_CHARSET_NAME)
		return ::fast_io::details::execution_charset_name_starts_with(
				   __GNUC_EXECUTION_CHARSET_NAME, "UTF") ||
			   ::fast_io::details::execution_charset_name_is(
				   __GNUC_EXECUTION_CHARSET_NAME, "ASCII") ||
			   ::fast_io::details::execution_charset_name_is(
				   __GNUC_EXECUTION_CHARSET_NAME, "ANSI_X3.4-1968");
#elif defined(__clang_literal_encoding__)
		return ::fast_io::details::execution_charset_name_starts_with(
			__clang_literal_encoding__, "UTF");
#elif defined(_MSC_VER)
		// Without /utf-8 or /execution-charset, MSVC uses an ACP execution
		// encoding.  Its ASCII prefix does not make its high bytes UTF-8.
		return false;
#else
		return ::fast_io::details::is_ascii<clean_type>;
#endif
	}
	else
	{
#if defined(__GNUC_WIDE_EXECUTION_CHARSET_NAME)
		return ::fast_io::details::execution_charset_name_starts_with(
				   __GNUC_WIDE_EXECUTION_CHARSET_NAME, "UTF") ||
			   ::fast_io::details::execution_charset_name_starts_with(
				   __GNUC_WIDE_EXECUTION_CHARSET_NAME, "UCS") ||
			   ::fast_io::details::execution_charset_name_is_or_has_iconv_suffix(
				   __GNUC_WIDE_EXECUTION_CHARSET_NAME, "WCHAR_T");
#elif defined(__clang_wide_literal_encoding__)
		return ::fast_io::details::execution_charset_name_starts_with(
				   __clang_wide_literal_encoding__, "UTF") ||
			   ::fast_io::details::execution_charset_name_is_or_has_iconv_suffix(
				   __clang_wide_literal_encoding__, "WCHAR_T");
#elif defined(_WIN32)
		return true;
#else
		return ::fast_io::details::is_ascii<clean_type>;
#endif
	}
}

template <::std::integral char_type>
inline constexpr bool is_unicode_execution_charset{
	exec_charset_is_unicode<char_type>()};

template <::std::integral char_type>
inline constexpr bool is_other_execution_charset{
	(::std::same_as<::std::remove_cvref_t<char_type>, char> ||
	 ::std::same_as<::std::remove_cvref_t<char_type>, wchar_t>) &&
	!::fast_io::details::is_ebcdic<char_type> &&
	!::fast_io::details::is_unicode_execution_charset<char_type>};

inline constexpr bool wexec_charset_is_utf_none_native_endian() noexcept
{
	if constexpr (!::fast_io::details::is_unicode_execution_charset<wchar_t> ||
				  sizeof(wchar_t) == 1u)
	{
		return false;
	}
	else
	{
		using unsigned_wchar_type = ::std::make_unsigned_t<wchar_t>;
		constexpr auto value{
			::fast_io::details::execution_character_value<wchar_t>(u8'A')};
		return value != static_cast<unsigned_wchar_type>(u8'A') &&
			   ::fast_io::byte_swap(value) == static_cast<unsigned_wchar_type>(u8'A');
	}
}

inline constexpr bool wide_is_none_utf_endian{wexec_charset_is_utf_none_native_endian()};

inline constexpr bool wexec_charset_is_ebcdic_none_native_endian() noexcept
{
	if constexpr (!::fast_io::details::is_ebcdic<wchar_t> || sizeof(wchar_t) == 1u)
	{
		return false;
	}
	else
	{
		return !::fast_io::details::exec_charset_has_ebcdic_invariant_layout<wchar_t>() &&
			   ::fast_io::details::exec_charset_has_ebcdic_invariant_layout<wchar_t, true>();
	}
}

inline constexpr bool wide_is_none_ebcdic_endian{
	wexec_charset_is_ebcdic_none_native_endian()};

template <bool swap_wide = false>
inline constexpr bool wexec_charset_basic_values_fit_in_byte() noexcept
{
	using unsigned_wchar_type = ::std::make_unsigned_t<wchar_t>;
	constexpr char8_t controls[]{u8'\0', u8'\a', u8'\b', u8'\t',
								 u8'\n', u8'\v', u8'\f', u8'\r'};
	for (auto ch : controls)
	{
		if (::fast_io::details::normalized_execution_character_value<
				wchar_t, swap_wide>(ch) >
			static_cast<unsigned_wchar_type>(0xffu))
		{
			return false;
		}
	}
	for (char8_t ch{u8' '}; ch != static_cast<char8_t>(0x7fu); ++ch)
	{
		if (::fast_io::details::normalized_execution_character_value<
				wchar_t, swap_wide>(ch) >
			static_cast<unsigned_wchar_type>(0xffu))
		{
			return false;
		}
	}
	return true;
}

inline constexpr bool wexec_charset_is_single_byte_none_native_endian() noexcept
{
	if constexpr (sizeof(wchar_t) == 1u ||
				  ::fast_io::details::is_unicode_execution_charset<wchar_t>)
	{
		return false;
	}
	else
	{
		return !::fast_io::details::wexec_charset_basic_values_fit_in_byte<>() &&
			   ::fast_io::details::wexec_charset_basic_values_fit_in_byte<true>();
	}
}

inline constexpr bool wide_is_none_single_byte_endian{
	wexec_charset_is_single_byte_none_native_endian()};

inline constexpr bool wide_is_none_execution_endian{
	::fast_io::details::wide_is_none_utf_endian ||
	::fast_io::details::wide_is_none_ebcdic_endian ||
	::fast_io::details::wide_is_none_single_byte_endian};

template <::std::integral char_type>
inline constexpr char_type execution_newline_literal() noexcept
{
	using clean_type = ::std::remove_cv_t<char_type>;
	using unsigned_type = ::std::make_unsigned_t<clean_type>;
	if constexpr (::fast_io::details::is_ebcdic<clean_type>)
	{
		unsigned_type value{static_cast<unsigned_type>(0x15u)};
		if constexpr (::std::same_as<clean_type, wchar_t> &&
					  ::fast_io::details::wide_is_none_ebcdic_endian)
		{
			value = ::fast_io::byte_swap(value);
		}
		return static_cast<clean_type>(value);
	}
	else if constexpr (::std::same_as<clean_type, char> ||
					   ::std::same_as<clean_type, wchar_t>)
	{
		return ::fast_io::details::execution_character_literal<clean_type>(u8'\n');
	}
	else
	{
		return static_cast<clean_type>(u8'\n');
	}
}

} // namespace fast_io::details
