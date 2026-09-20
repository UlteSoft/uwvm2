#pragma once

namespace fast_io
{

namespace details
{

template <bool noskipws, bool line>
inline constexpr ::fast_io::manipulators::scalar_flags strlike_default_scalar_flags{.noskipws = noskipws, .line = line};

}

namespace manipulators
{

/// @brief Hidden carrier for a mutable string-like scan destination.
/// @details `reference` is obtained through `io_strlike_ref`; the enclosing scalar or whole-object wrapper selects
///          token, line, or complete-input semantics.
template <typename T>
struct basic_strlike_get
{
	using value_type = T;
	using manip_tag = manip_tag_t;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
};

/// @brief Reads one whitespace-delimited token into a mutable string-like destination.
/// @details Leading C whitespace is skipped, then code units are copied until the next C whitespace. The delimiter is
///          not part of the stored value and the terminating whitespace remains at the returned scan position.
template <typename T>
inline constexpr auto strlike_get(T &reference) noexcept
{
	return ::fast_io::manipulators::scalar_manip_t<
		::fast_io::details::strlike_default_scalar_flags<false, false>,
		::fast_io::manipulators::basic_strlike_get<decltype(io_strlike_ref(io_alias, reference))>>{
		{io_strlike_ref(io_alias, reference)}};
}

/// @brief Reads one line into a mutable string-like destination.
/// @details Input is copied until line-feed according to the scanner's line contract; ordinary spaces are preserved
///          rather than terminating the token. The line-feed is consumed but is not stored; no carriage-return
///          normalization is performed by this manipulator.
template <typename T>
inline constexpr auto strlike_line_get(T &reference) noexcept
{
	return ::fast_io::manipulators::scalar_manip_t<
		::fast_io::details::strlike_default_scalar_flags<false, true>,
		::fast_io::manipulators::basic_strlike_get<decltype(io_strlike_ref(io_alias, reference))>>{
		{io_strlike_ref(io_alias, reference)}};
}

/// @brief Reads the complete remaining input into a mutable string-like destination.
/// @details No whitespace or line delimiter has special meaning. Completion is determined by the enclosing whole-input
///          scan operation rather than by the first token boundary.
template <typename T>
inline constexpr auto strlike_whole_get(T &reference) noexcept
{
	return ::fast_io::manipulators::whole_get_t<
		::fast_io::manipulators::basic_strlike_get<decltype(io_strlike_ref(io_alias, reference))>>{
		{io_strlike_ref(io_alias, reference)}};
}

} // namespace manipulators

namespace details
{

template <bool noskipws, bool line, bool ctxread = false, ::std::integral char_type, typename T>
inline constexpr ::fast_io::parse_result<char_type const *>
scan_context_define_strlike_impl(::std::conditional_t<ctxread, bool, bool &> skip_space_done, char_type const *first,
								 char_type const *last, T ref)
{
	auto it{first};
	if constexpr (!noskipws && !line)
	{
		if (!skip_space_done)
		{
			it = ::fast_io::find_none_c_space(it, last);
			if (it == last)
			{
				return {it, ::fast_io::parse_code::partial};
			}
			skip_space_done = true;
			obuffer_set_curr(ref, obuffer_begin(ref));
		}
	}
	auto it_space{it};
	if constexpr (line)
	{
		it_space = ::fast_io::find_lf(it_space, last);
	}
	else
	{
		it_space = ::fast_io::find_c_space(it_space, last);
	}
	if constexpr (noskipws || line)
	{
		if (!skip_space_done)
		{
			obuffer_set_curr(ref, obuffer_begin(ref));
		}
		::fast_io::operations::decay::write_all_decay_dispatch(ref, it, it_space);
		skip_space_done = true;
	}
	else
	{
		::fast_io::operations::decay::write_all_decay_dispatch(ref, it, it_space);
	}
	if (it_space == last)
	{
		return {it_space, ::fast_io::parse_code::partial};
	}
	if constexpr (line)
	{
		++it_space;
	}
	return {it_space, ::fast_io::parse_code::ok};
}

template <bool ctxread = false, ::std::integral char_type, typename T>
inline constexpr ::fast_io::parse_result<char_type const *>
scan_context_define_strlike_getall_impl(::std::conditional_t<ctxread, bool, bool &> skip_space_done,
										char_type const *first, char_type const *last, T ref)
{
	if (!skip_space_done)
	{
		obuffer_set_curr(ref, obuffer_begin(ref));
		skip_space_done = true;
	}
	::fast_io::operations::decay::write_all_decay_dispatch(ref, first, last);
	return {last, ::fast_io::parse_code::partial};
}

inline constexpr ::fast_io::parse_code scan_context_eof_strlike_define_impl(bool skip_space_done) noexcept
{
	if (skip_space_done)
	{
		return ::fast_io::parse_code::ok;
	}
	else
	{
		return ::fast_io::parse_code::end_of_file;
	}
}

/// @brief Proves the target and adapter protocol consumed by the strlike scanner bodies.
/// @details `io_strlike_ref` is extensible, so merely finding members named `value_type`, `char_type`, and `ptr` is not
///          evidence that the pointed target can be completed. Same-domain writable targets use the adapter's put area
///          directly and therefore require its exact buffered plus primitive-write protocols. Construct-only and
///          cross-domain targets stage into `basic_concat_buffer`; for those branches an exact target `strlike` and a
///          pointer to that target are the complete construction proof. Keeping this predicate on every scan CPO makes
///          malformed associated refs make `context_scannable` false instead of failing in the selected function body.
template <::std::integral input_char_type, typename T>
inline consteval bool strlike_get_reference_target_impl() noexcept
{
	using reference_type = ::std::remove_cvref_t<T>;
	if constexpr (!requires(reference_type &ref) {
		typename reference_type::value_type;
		typename reference_type::char_type;
		ref.ptr;
	})
	{
		return false;
	}
	else
	{
		using target_type = typename reference_type::value_type;
		using target_char_type = typename reference_type::char_type;
		if constexpr (
			!::std::integral<target_char_type> ||
			!::std::same_as<::std::remove_cvref_t<decltype(::std::declval<reference_type &>().ptr)>, target_type *> ||
			!::fast_io::strlike<target_char_type, target_type>)
		{
			return false;
		}
		else if constexpr (
			::std::same_as<input_char_type, target_char_type> &&
			::fast_io::buffer_strlike<input_char_type, target_type>)
		{
			if constexpr (requires { typename reference_type::output_char_type; })
			{
				return ::std::same_as<input_char_type, typename reference_type::output_char_type> &&
					   ::fast_io::operations::decay::defines::has_obuffer_basic_operations<reference_type> &&
					   ::fast_io::operations::decay::defines::writable<reference_type>;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return true;
		}
	}
}

template <typename input_char_type, typename T>
concept strlike_get_reference_target =
	::std::integral<input_char_type> &&
	::fast_io::details::strlike_get_reference_target_impl<input_char_type, T>();

} // namespace details

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags, typename T>
	requires ::fast_io::details::strlike_get_reference_target<char_type, T>
inline constexpr io_type_t<
	::std::conditional_t<(::fast_io::buffer_strlike<char_type, typename ::std::remove_cvref_t<T>::value_type> &&
						  ::std::same_as<typename ::std::remove_cvref_t<T>::char_type, char_type>),
						 ::fast_io::details::str_get_all_context, ::fast_io::details::basic_concat_buffer<char_type>>>
scan_context_type(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<
												   flags, ::fast_io::manipulators::basic_strlike_get<T>>>) noexcept
{
	return {};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags, typename ctx_type, typename T>
	requires ::fast_io::details::strlike_get_reference_target<char_type, T>
inline constexpr parse_result<char_type const *> scan_context_define(
	io_reserve_type_t<char_type,
					  ::fast_io::manipulators::scalar_manip_t<flags, ::fast_io::manipulators::basic_strlike_get<T>>>,
	ctx_type &ctx, char_type const *first, char_type const *last,
	::fast_io::manipulators::scalar_manip_t<flags, ::fast_io::manipulators::basic_strlike_get<T>> ref)
{
	using value_type = ::std::remove_cvref_t<T>;
	using undefttype_char_type = typename ::std::remove_cvref_t<value_type>::char_type;
	if constexpr (::fast_io::buffer_strlike<char_type, typename ::std::remove_cvref_t<T>::value_type> &&
				  ::std::same_as<undefttype_char_type, char_type>)
	{
		return ::fast_io::details::scan_context_define_strlike_impl<flags.noskipws, flags.line>(
			ctx.copying, first, last, ref.reference.reference);
	}
	else
	{
		bool b{ctx.buffer_begin != ctx.buffer_curr};
		auto [it, ec] = ::fast_io::details::scan_context_define_strlike_impl<flags.noskipws, flags.line, true>(
			b, first, last, io_strlike_ref(io_alias, ctx));
		if (ec == ::fast_io::parse_code::ok)
		{
			using ioreftype = typename value_type::value_type;
			if constexpr (::std::same_as<undefttype_char_type, char_type>)
			{
				*ref.reference.reference.ptr = strlike_construct_define(
					io_strlike_type<undefttype_char_type, ioreftype>, ctx.buffer_begin, ctx.buffer_curr);
			}
			else
			{
				*ref.reference.reference.ptr = ::fast_io::basic_general_concat<false, undefttype_char_type, ioreftype>(
					::fast_io::manipulators::code_cvt(
						::fast_io::manipulators::strvw(ctx.buffer_begin, ctx.buffer_curr)));
			}
		}
		return {it, ec};
	}
}

template <::fast_io::manipulators::scalar_flags flags, ::std::integral char_type, typename ctx_type, typename T>
	requires ::fast_io::details::strlike_get_reference_target<char_type, T>
inline constexpr ::fast_io::parse_code scan_context_eof_define(
	io_reserve_type_t<char_type,
					  ::fast_io::manipulators::scalar_manip_t<flags, ::fast_io::manipulators::basic_strlike_get<T>>>,
	ctx_type &ctx, ::fast_io::manipulators::scalar_manip_t<flags, ::fast_io::manipulators::basic_strlike_get<T>> ref)
{
	using value_type = ::std::remove_cvref_t<T>;
	using undefttype_char_type = typename ::std::remove_cvref_t<value_type>::char_type;
	if constexpr (flags.line || flags.noskipws)
	{
		if constexpr (::fast_io::buffer_strlike<char_type, typename ::std::remove_cvref_t<T>::value_type> &&
					  ::std::same_as<undefttype_char_type, char_type>)
		{
			return ::fast_io::details::scan_context_eof_strlike_define_impl(ctx.copying);
		}
		else
		{
			if (ctx.buffer_begin == ctx.buffer_curr)
			{
				return ::fast_io::parse_code::end_of_file;
			}
			else
			{
				using ioreftype = typename value_type::value_type;
				if constexpr (::std::same_as<undefttype_char_type, char_type>)
				{
					*ref.reference.reference.ptr = strlike_construct_define(
						io_strlike_type<undefttype_char_type, ioreftype>, ctx.buffer_begin, ctx.buffer_curr);
				}
				else
				{
					*ref.reference.reference.ptr =
						::fast_io::basic_general_concat<false, undefttype_char_type, ioreftype>(
							::fast_io::manipulators::code_cvt(
								::fast_io::manipulators::strvw(ctx.buffer_begin, ctx.buffer_curr)));
				}
				return ::fast_io::parse_code::ok;
			}
		}
	}
	else
	{
		if constexpr (::fast_io::buffer_strlike<char_type, typename ::std::remove_cvref_t<T>::value_type> &&
					  ::std::same_as<undefttype_char_type, char_type>)
		{
			return ::fast_io::details::scan_context_eof_strlike_define_impl(ctx.copying);
		}
		else
		{
			if (ctx.buffer_begin != ctx.buffer_curr)
			{
				using ioreftype = typename value_type::value_type;
				if constexpr (::std::same_as<undefttype_char_type, char_type>)
				{
					*ref.reference.reference.ptr = strlike_construct_define(
						io_strlike_type<undefttype_char_type, ioreftype>, ctx.buffer_begin, ctx.buffer_curr);
				}
				else
				{
					*ref.reference.reference.ptr =
						::fast_io::basic_general_concat<false, undefttype_char_type, ioreftype>(
							::fast_io::manipulators::code_cvt(
								::fast_io::manipulators::strvw(ctx.buffer_begin, ctx.buffer_curr)));
				}
				return ::fast_io::parse_code::ok;
			}
			else
			{
				return ::fast_io::parse_code::end_of_file;
			}
		}
	}
}

template <::std::integral char_type, typename T>
	requires ::fast_io::details::strlike_get_reference_target<char_type, T>
inline constexpr io_type_t<
	::std::conditional_t<(::fast_io::buffer_strlike<char_type, typename ::std::remove_cvref_t<T>::value_type> &&
						  ::std::same_as<typename ::std::remove_cvref_t<T>::char_type, char_type>),
						 ::fast_io::details::str_get_all_context, ::fast_io::details::basic_concat_buffer<char_type>>>
scan_context_type(
	io_reserve_type_t<char_type,
					  ::fast_io::manipulators::whole_get_t<::fast_io::manipulators::basic_strlike_get<T>>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename ctx_type, typename T>
	requires ::fast_io::details::strlike_get_reference_target<char_type, T>
inline constexpr parse_result<char_type const *> scan_context_define(
	io_reserve_type_t<char_type, ::fast_io::manipulators::whole_get_t<::fast_io::manipulators::basic_strlike_get<T>>>,
	ctx_type &ctx, char_type const *first, char_type const *last,
	::fast_io::manipulators::whole_get_t<::fast_io::manipulators::basic_strlike_get<T>> ref)
{
	using value_type = ::std::remove_cvref_t<T>;
	using undefttype_char_type = typename ::std::remove_cvref_t<value_type>::char_type;
	if constexpr (::fast_io::buffer_strlike<char_type, typename ::std::remove_cvref_t<T>::value_type> &&
				  ::std::same_as<undefttype_char_type, char_type>)
	{
		return ::fast_io::details::scan_context_define_strlike_getall_impl(ctx.copying, first, last,
																		   ref.reference.reference);
	}
	else
	{
		bool b{ctx.buffer_begin != ctx.buffer_curr};
		auto [it, ec] = ::fast_io::details::scan_context_define_strlike_getall_impl<true>(
			b, first, last, io_strlike_ref(io_alias, ctx));
		if (ec == ::fast_io::parse_code::ok)
		{
			using ioreftype = typename value_type::value_type;
			if constexpr (::std::same_as<undefttype_char_type, char_type>)
			{
				*ref.reference.reference.ptr = strlike_construct_define(
					io_strlike_type<undefttype_char_type, ioreftype>, ctx.buffer_begin, ctx.buffer_curr);
			}
			else
			{
				*ref.reference.reference.ptr = ::fast_io::basic_general_concat<false, undefttype_char_type, ioreftype>(
					::fast_io::manipulators::code_cvt(
						::fast_io::manipulators::strvw(ctx.buffer_begin, ctx.buffer_curr)));
			}
		}
		return {it, ec};
	}
}

template <::std::integral char_type, typename ctx_type, typename T>
	requires ::fast_io::details::strlike_get_reference_target<char_type, T>
inline constexpr ::fast_io::parse_code scan_context_eof_define(
	io_reserve_type_t<char_type, ::fast_io::manipulators::whole_get_t<::fast_io::manipulators::basic_strlike_get<T>>>,
	ctx_type &ctx, ::fast_io::manipulators::whole_get_t<::fast_io::manipulators::basic_strlike_get<T>> ref)
{

	using value_type = ::std::remove_cvref_t<T>;
	using undefttype_char_type = typename ::std::remove_cvref_t<value_type>::char_type;
	if constexpr (::fast_io::buffer_strlike<char_type, typename ::std::remove_cvref_t<T>::value_type> &&
				  ::std::same_as<undefttype_char_type, char_type>)
	{
		if (!ctx.copying)
		{
			obuffer_set_curr(ref.reference.reference, obuffer_begin(ref.reference.reference));
		}
	}
	else
	{
		using ioreftype = typename value_type::value_type;
		if constexpr (::std::same_as<undefttype_char_type, char_type>)
		{
			*ref.reference.reference.ptr = strlike_construct_define(io_strlike_type<undefttype_char_type, ioreftype>,
																	ctx.buffer_begin, ctx.buffer_curr);
		}
		else
		{
			*ref.reference.reference.ptr = ::fast_io::basic_general_concat<false, undefttype_char_type, ioreftype>(
				::fast_io::manipulators::code_cvt(::fast_io::manipulators::strvw(ctx.buffer_begin, ctx.buffer_curr)));
		}
	}
	return ::fast_io::parse_code::ok;
}

} // namespace fast_io
