#pragma once

namespace fast_io
{

namespace details::decay
{

/// @brief Tests an implementation-provided string adapter without forming it for a destination that has no such CPO.
template <bool line, ::std::integral char_type, typename string_type, typename... Args>
inline constexpr bool lc_concat_custom_destination_ok = []() constexpr {
	if constexpr (requires(string_type &string) {
		io_strlike_ref(::fast_io::io_alias, string);
	})
	{
		using output = ::std::remove_cvref_t<decltype(io_strlike_ref(
			::fast_io::io_alias, ::std::declval<string_type &>()))>;
		if constexpr (requires { typename output::output_char_type; })
		{
			// Locale binding and the final `basic_lc_all` parameter both use the result string's character domain. An
			// otherwise writable associated ref for a different domain is not a conversion protocol and must not hide the
			// generic cursor or exact range-construction alternatives below.
			return ::std::same_as<char_type, typename output::output_char_type> &&
				   ::fast_io::details::decay::lc_status_print_output_run_okay<
					   line, char_type, output, Args...>;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}();

/// @brief Builds a string through the same locale-bound output strategy used by files and decorators.
/// @details A second concat-specific reserve/scatter scanner used to duplicate print's protocol ordering, semantic
///          normalization, stack allocation, and direct-output admission rules. That scanner had already drifted far
///          enough to reference removed print internals and to classify direct printers against a dummy sink. A string
///          adapter is a real output object, so routing it through `lc_status_print_define_decay` preserves every
///          locale CPO while inheriting the maintained concat/buffer cost model.
///
///          `strlike` deliberately includes independent buffer and range-construction protocols. A user adapter may
///          expose either, both, or an unrelated `io_strlike_ref`; none of those structural expressions proves that the
///          complete locale-bound run is printable to the resulting stream. The three branches below therefore test the
///          actual rebound run against (1) the implementation-provided adapter, (2) fast_io's maintained generic cursor
///          adapter, and (3) `basic_concat_buffer` followed by range construction. The first successful proof is used.
///          This preserves output-specific direct CPOs without making a misleading adapter hide a valid buffer or
///          construct path, and it never forms `strlike_construct_define` for a buffer-only result.
template <bool line, typename string_type, typename... Args>
inline constexpr string_type lc_concat_decay_ref_impl(
	::fast_io::basic_lc_all<typename string_type::value_type> const *all, Args &...args)
{
	using char_type = typename string_type::value_type;
	if constexpr (::fast_io::details::decay::lc_concat_custom_destination_ok<
				  line, char_type, string_type, Args...>)
	{
		string_type string;
		decltype(auto) ref = io_strlike_ref(::fast_io::io_alias, string);
		::fast_io::operations::decay::lc_status_print_define_decay<line>(all, ref, args...);
		return string;
	}
	else if constexpr (
		::fast_io::buffer_strlike<char_type, string_type> &&
		::fast_io::details::decay::lc_status_print_output_run_okay<
			line, char_type,
			::fast_io::io_strlike_reference_wrapper<char_type, string_type>, Args...>)
	{
		string_type string;
		::fast_io::io_strlike_reference_wrapper<char_type, string_type> ref{
			__builtin_addressof(string)};
		::fast_io::operations::decay::lc_status_print_define_decay<line>(all, ref, args...);
		return string;
	}
	else if constexpr (::fast_io::range_constructible_strlike<char_type, string_type>)
	{
		::fast_io::details::basic_concat_buffer<char_type> buffer;
		using staging_ref = decltype(io_strlike_ref(::fast_io::io_alias, buffer));
		static_assert(
			::fast_io::details::decay::lc_status_print_output_run_okay<
				line, char_type, staging_ref, Args...>,
			"the normalized locale concat run is printable to neither the result adapter nor its staging stream");
		decltype(auto) ref = io_strlike_ref(::fast_io::io_alias, buffer);
		::fast_io::operations::decay::lc_status_print_define_decay<line>(all, ref, args...);
		return strlike_construct_define(
			::fast_io::io_strlike_type<char_type, string_type>,
			buffer.buffer_begin, buffer.buffer_curr);
	}
	else
	{
		static_assert(
			::fast_io::details::decay::lc_status_print_output_run_okay<
				line, char_type,
				::fast_io::io_strlike_reference_wrapper<char_type, string_type>, Args...>,
			"a buffer-only locale concat destination must accept the complete normalized run");
		::fast_io::fast_terminate();
	}
}

/// @brief Establishes the single owning boundary for an already-normalized locale concat run.
/// @details Public concat forwarding produces compact values, exact-reference parameters, and possibly move-only
///          owners. Materialize each such result once here, then let the implementation above borrow the named pack.
///          Calling another by-value helper with these lvalues would require a second copy (and reject an owned
///          move-only formatter); forwarding them again would instead reopen normalization after ownership was fixed.
template <bool line, typename string_type, typename... Args>
inline constexpr string_type lc_concat_decay_impl(
	::fast_io::basic_lc_all<typename string_type::value_type> const *all, Args... args)
{
	return ::fast_io::details::decay::lc_concat_decay_ref_impl<line, string_type>(
		all, args...);
}

template <typename T>
concept l10ntypes_impl = requires(T &loc) {
	requires ::std::same_as<::std::remove_cvref_t<decltype(loc.loc)>, ::fast_io::lc_locale>;
};

/// @brief Selects the character-specific locale aggregate owned by a native locale wrapper.
/// @details `lc_locale` stores pointers to complete `basic_lc_object` instances, not direct `basic_lc_all` members.
///          Going through `get_lc` documents that ownership boundary and prevents accidental field-name coupling.
template <::std::integral char_type, l10ntypes_impl T>
inline constexpr ::fast_io::basic_lc_all<char_type> const *lc_concat_all(T &loc) noexcept
{
	return __builtin_addressof(::fast_io::get_lc<char_type>(loc.loc)->all);
}

} // namespace details::decay

template <typename T, typename... Args>
inline constexpr T basic_lc_concat_decay(
	basic_lc_all<typename T::value_type> const *all, Args... args)
{
	// This compatibility function already owns its decayed arguments. Enter the reference phase directly rather than
	// copying the same normalized graph into the public owning boundary a second time.
	return ::fast_io::details::decay::lc_concat_decay_ref_impl<false, T>(all, args...);
}

template <typename T, typename... Args>
inline constexpr T basic_lc_concatln_decay(
	basic_lc_all<typename T::value_type> const *all, Args... args)
{
	return ::fast_io::details::decay::lc_concat_decay_ref_impl<true, T>(all, args...);
}

template <typename T, typename... Args>
inline constexpr T basic_lc_concat(
	basic_lc_all<typename T::value_type> const *all, Args &&...args)
{
	using char_type = typename T::value_type;
	return ::fast_io::details::decay::lc_concat_decay_impl<false, T>(
		all,
		::fast_io::io_print_forward<char_type>(
			::fast_io::io_print_alias(::std::forward<Args>(args)))...);
}

template <typename T, typename... Args>
inline constexpr T basic_lc_concatln(
	basic_lc_all<typename T::value_type> const *all, Args &&...args)
{
	using char_type = typename T::value_type;
	return ::fast_io::details::decay::lc_concat_decay_impl<true, T>(
		all,
		::fast_io::io_print_forward<char_type>(
			::fast_io::io_print_alias(::std::forward<Args>(args)))...);
}

template <::std::integral char_type, typename... Args>
inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::basic_string<char_type>
	lc_concat(basic_lc_all<char_type> const *all, Args &&...args)
{
	return ::fast_io::details::decay::lc_concat_decay_impl<false, ::std::basic_string<char_type>>(
		all,
		::fast_io::io_print_forward<char_type>(
			::fast_io::io_print_alias(::std::forward<Args>(args)))...);
}

template <::std::integral char_type, typename... Args>
inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::basic_string<char_type>
	lc_concatln(basic_lc_all<char_type> const *all, Args &&...args)
{
	return ::fast_io::details::decay::lc_concat_decay_impl<true, ::std::basic_string<char_type>>(
		all,
		::fast_io::io_print_forward<char_type>(
			::fast_io::io_print_alias(::std::forward<Args>(args)))...);
}

template <::fast_io::details::decay::l10ntypes_impl T, typename... Args>
inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::string lc_concat(T &loc, Args &&...args)
{
	return ::fast_io::lc_concat(
		::fast_io::details::decay::lc_concat_all<char>(loc), ::std::forward<Args>(args)...);
}

template <::fast_io::details::decay::l10ntypes_impl T, typename... Args>
inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::string lc_concatln(T &loc, Args &&...args)
{
	return ::fast_io::lc_concatln(
		::fast_io::details::decay::lc_concat_all<char>(loc), ::std::forward<Args>(args)...);
}

template <::fast_io::details::decay::l10ntypes_impl T, typename... Args>
inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::basic_string<wchar_t> wlc_concat(T &loc, Args &&...args)
{
	return ::fast_io::lc_concat(
		::fast_io::details::decay::lc_concat_all<wchar_t>(loc), ::std::forward<Args>(args)...);
}

template <::fast_io::details::decay::l10ntypes_impl T, typename... Args>
inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::basic_string<wchar_t> wlc_concatln(T &loc, Args &&...args)
{
	return ::fast_io::lc_concatln(
		::fast_io::details::decay::lc_concat_all<wchar_t>(loc), ::std::forward<Args>(args)...);
}

template <::fast_io::details::decay::l10ntypes_impl T, typename... Args>
inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::u8string u8lc_concat(T &loc, Args &&...args)
{
	return ::fast_io::lc_concat(
		::fast_io::details::decay::lc_concat_all<char8_t>(loc), ::std::forward<Args>(args)...);
}

template <::fast_io::details::decay::l10ntypes_impl T, typename... Args>
inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::u8string u8lc_concatln(T &loc, Args &&...args)
{
	return ::fast_io::lc_concatln(
		::fast_io::details::decay::lc_concat_all<char8_t>(loc), ::std::forward<Args>(args)...);
}

template <::fast_io::details::decay::l10ntypes_impl T, typename... Args>
inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::u16string u16lc_concat(T &loc, Args &&...args)
{
	return ::fast_io::lc_concat(
		::fast_io::details::decay::lc_concat_all<char16_t>(loc), ::std::forward<Args>(args)...);
}

template <::fast_io::details::decay::l10ntypes_impl T, typename... Args>
inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::u16string u16lc_concatln(T &loc, Args &&...args)
{
	return ::fast_io::lc_concatln(
		::fast_io::details::decay::lc_concat_all<char16_t>(loc), ::std::forward<Args>(args)...);
}

template <::fast_io::details::decay::l10ntypes_impl T, typename... Args>
inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::u32string u32lc_concat(T &loc, Args &&...args)
{
	return ::fast_io::lc_concat(
		::fast_io::details::decay::lc_concat_all<char32_t>(loc), ::std::forward<Args>(args)...);
}

template <::fast_io::details::decay::l10ntypes_impl T, typename... Args>
inline
#if __cpp_lib_constexpr_string >= 201907L
	constexpr
#endif
	::std::u32string u32lc_concatln(T &loc, Args &&...args)
{
	return ::fast_io::lc_concatln(
		::fast_io::details::decay::lc_concat_all<char32_t>(loc), ::std::forward<Args>(args)...);
}

} // namespace fast_io
