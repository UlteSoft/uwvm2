#pragma once

namespace fast_io
{

template <typename... Args>
[[deprecated("use ::fast_io::concat_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::string
	concat(Args &&...args)
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char, ::std::string>(
		::std::forward<Args>(args)...);
}

#if (!defined(_LIBCPP_VERSION)) || _LIBCPP_HAS_WIDE_CHARACTERS
template <typename... Args>
[[deprecated("use ::fast_io::wconcat_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::basic_string<wchar_t>
	wconcat(Args &&...args)
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, wchar_t, ::std::basic_string<wchar_t>>(
		::std::forward<Args>(args)...);
}
#endif

template <typename... Args>
[[deprecated("use ::fast_io::u8concat_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::u8string
	u8concat(Args &&...args)
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char8_t, ::std::u8string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[deprecated("use ::fast_io::u16concat_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::u16string
	u16concat(Args &&...args)
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char16_t, ::std::u16string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[deprecated("use ::fast_io::u32concat_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::u32string
	u32concat(Args &&...args)
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<false, char32_t, ::std::u32string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[deprecated("use ::fast_io::concatln_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::basic_string<char>
	concatln(Args &&...args)
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char, ::std::basic_string<char>>(
		::std::forward<Args>(args)...);
}

#if (!defined(_LIBCPP_VERSION)) || _LIBCPP_HAS_WIDE_CHARACTERS

template <typename... Args>
[[deprecated("use ::fast_io::wconcatln_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::basic_string<wchar_t>
	wconcatln(Args &&...args)
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, wchar_t, ::std::basic_string<wchar_t>>(
		::std::forward<Args>(args)...);
}
#endif

template <typename... Args>
[[deprecated("use ::fast_io::u8concatln_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::u8string
	u8concatln(Args &&...args)
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char8_t, ::std::u8string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[deprecated("use ::fast_io::u16concatln_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::u16string
	u16concatln(Args &&...args)
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char16_t, ::std::u16string>(
		::std::forward<Args>(args)...);
}

template <typename... Args>
[[deprecated("use ::fast_io::u32concatln_std instead"), nodiscard]] inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::u32string
	u32concatln(Args &&...args)
{
	return ::fast_io::basic_general_concat_compiler_constant_checked_entry<true, char32_t, ::std::u32string>(
		::std::forward<Args>(args)...);
}

} // namespace fast_io
