#pragma once

namespace fast_io::details::decay
{

template <::std::integral char_type, typename T>
inline constexpr ::std::size_t calculate_scatter_reserve_size_unit()
{
	using real_type = ::std::remove_cvref_t<T>;
	if constexpr (reserve_printable<char_type, real_type>)
	{
		constexpr ::std::size_t sz{print_reserve_size(io_reserve_type<char_type, real_type>)};
		return sz;
	}
	else
	{
		return 0;
	}
}

template <::std::integral char_type, typename T, typename... Args>
inline constexpr ::std::size_t calculate_scatter_reserve_size()
{
	if constexpr (sizeof...(Args) == 0)
	{
		return calculate_scatter_reserve_size_unit<char_type, T>();
	}
	else
	{
		return ::fast_io::details::intrinsics::add_or_overflow_die(
			calculate_scatter_reserve_size_unit<char_type, T>(), calculate_scatter_reserve_size<char_type, Args...>());
	}
}

template <bool line, ::std::integral char_type, typename T, typename... Args>
inline constexpr char_type *print_reserve_define_chain_impl(char_type *p, T t, Args... args)
{
	if constexpr (sizeof...(Args) == 0)
	{
		p = print_reserve_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, p, t);
		if constexpr (line)
		{
			*p = char_literal_v<u8'\n', char_type>;
			++p;
		}
		return p;
	}
	else
	{
		return print_reserve_define_chain_impl<line>(
			print_reserve_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, p, t), args...);
	}
}

template <::std::integral char_type, typename T, typename... Args>
inline constexpr ::std::size_t calculate_scatter_dynamic_reserve_size_with_scatter([[maybe_unused]] T t, Args... args)
{
	if constexpr (dynamic_reserve_printable<char_type, T>)
	{
		::std::size_t res{print_reserve_size(io_reserve_type<char_type, T>, t)};
		if constexpr (sizeof...(Args) == 0)
		{
			return res;
		}
		else
		{
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				res, calculate_scatter_dynamic_reserve_size_with_scatter<char_type>(args...));
		}
	}
	else if constexpr (scatter_printable<char_type, T>)
	{
		::std::size_t res{print_scatter_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t).len};
		if constexpr (sizeof...(Args) == 0)
		{
			return res;
		}
		else
		{
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				res, calculate_scatter_dynamic_reserve_size_with_scatter<char_type>(args...));
		}
	}
	else
	{
		if constexpr (sizeof...(Args) == 0)
		{
			return 0;
		}
		else
		{
			return calculate_scatter_dynamic_reserve_size_with_scatter<char_type>(args...);
		}
	}
}

template <bool line, ::std::integral char_type, typename T, typename... Args>
inline constexpr char_type *print_reserve_define_chain_scatter_impl(char_type *p, T t, Args... args)
{
	if constexpr (dynamic_reserve_printable<char_type, T> || reserve_printable<char_type, T>)
	{
		p = print_reserve_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, p, t);
	}
	else
	{
		auto sc{print_scatter_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t)};
		p = non_overlapped_copy_n(sc.base, sc.len, p);
	}
	if constexpr (sizeof...(Args) == 0)
	{
		if constexpr (line)
		{
			*p = char_literal_v<u8'\n', char_type>;
			++p;
		}
		return p;
	}
	else
	{
		return print_reserve_define_chain_scatter_impl<line>(p, args...);
	}
}

template <::std::integral ch_type, typename T>
inline constexpr basic_io_scatter_t<ch_type> print_scatter_define_extract_one(T t)
{
	return print_scatter_define(io_reserve_type<ch_type, ::std::remove_cvref_t<T>>, t);
}

template <::std::integral ch_type>
inline constexpr ::std::size_t calculate_scatter_total_size(basic_io_scatter_t<ch_type> const *first,
															basic_io_scatter_t<ch_type> const *last)
{
	::std::size_t total_size{};
	for (; first != last; ++first)
	{
		total_size = ::fast_io::details::intrinsics::add_or_overflow_die(total_size, first->len);
	}
	return total_size;
}

template <::std::integral ch_type>
inline constexpr ch_type *copy_scatter_chain_to_buffer(ch_type *iter, basic_io_scatter_t<ch_type> const *first,
													   basic_io_scatter_t<ch_type> const *last)
{
	for (; first != last; ++first)
	{
		iter = non_overlapped_copy_n(first->base, first->len, iter);
	}
	return iter;
}

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_all_scatter(T &str, Args... args)
{
	basic_io_scatter_t<ch_type> scatters[]{
		print_scatter_define(io_reserve_type<ch_type, ::std::remove_cvref_t<Args>>, args)...};
	::std::size_t total_size{calculate_scatter_total_size<ch_type>(scatters, scatters + sizeof...(Args))};
	if constexpr (line)
	{
		total_size = ::fast_io::details::intrinsics::add_or_overflow_die(total_size, static_cast<::std::size_t>(1u));
	}
	if constexpr (sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
		if (local_cap < total_size)
		{
			strlike_reserve(io_strlike_type<ch_type, T>, str, total_size);
		}
	}
	else
	{
		strlike_reserve(io_strlike_type<ch_type, T>, str, total_size);
	}
	auto ptr{copy_scatter_chain_to_buffer(strlike_begin(io_strlike_type<ch_type, T>, str), scatters,
										  scatters + sizeof...(Args))};
	if constexpr (line)
	{
		*ptr = char_literal_v<u8'\n', ch_type>;
		++ptr;
	}
	strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
}

template <bool line, ::std::integral ch_type, typename T, typename Arg>
inline constexpr T basic_general_concat_decay_impl_precise(T &str, Arg arg)
{
	::std::size_t precise_size{print_reserve_precise_size(io_reserve_type<ch_type, Arg>, arg)};
	::std::size_t precise_size_with_line{precise_size};
	if constexpr (line)
	{
		++precise_size_with_line;
	}
	constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
	if (local_cap < precise_size_with_line)
	{
		strlike_reserve(io_strlike_type<ch_type, T>, str, precise_size_with_line);
	}
	auto first{strlike_begin(io_strlike_type<ch_type, T>, str)};
	print_reserve_precise_define(io_reserve_type<ch_type, Arg>, first, precise_size, arg);
	auto ptr{first + precise_size};
	if constexpr (line)
	{
		*ptr = char_literal_v<u8'\n', ch_type>;
		++ptr;
	}
	strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
	return str;
}

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr T basic_general_concat_decay_impl(Args... args)
{
	if constexpr (sizeof...(Args) == 0)
	{
		if constexpr (line)
		{
			return strlike_construct_single_character_define(io_strlike_type<ch_type, T>,
															 char_literal_v<u8'\n', ch_type>);
		}
		else
		{
			return {};
		}
	}
	else if constexpr (((reserve_printable<ch_type, Args> || scatter_printable<ch_type, Args> ||
						 dynamic_reserve_printable<ch_type, Args>) &&
						...))
	{
		constexpr ::std::size_t sz{calculate_scatter_reserve_size<ch_type, Args...>()};
		if constexpr (line)
		{
			static_assert(sz != SIZE_MAX, "overflow\n");
		}
		constexpr ::std::size_t sz_with_line{sz + static_cast<::std::size_t>(line)};
		if constexpr ((reserve_printable<ch_type, Args> && ...))
		{
			if constexpr (sso_buffer_strlike<ch_type, T>)
			{
				constexpr ::std::size_t local_cap{strlike_sso_size(io_reserve_type<ch_type, T>)};
				constexpr bool not_enough_space{(local_cap < sz_with_line)};
				if constexpr (not_enough_space &&
							  ((sizeof...(Args) == 1) && (precise_reserve_printable<ch_type, Args> && ...)))
				{
					return basic_general_concat_decay_impl_precise<line, ch_type, T>(args...);
				}
				else
				{
					T str;
					if constexpr (not_enough_space)
					{
						strlike_reserve(io_strlike_type<ch_type, T>, str, sz_with_line);
					}
					strlike_set_curr(io_strlike_type<ch_type, T>, str,
									 print_reserve_define_chain_impl<line>(
										 strlike_begin(io_strlike_type<ch_type, T>, str), args...));
					return str;
				}
			}
			else
			{
				T str;
				strlike_reserve(io_strlike_type<ch_type, T>, str, sz_with_line);
				strlike_set_curr(
					io_strlike_type<ch_type, T>, str,
					print_reserve_define_chain_impl<line>(strlike_begin(io_strlike_type<ch_type, T>, str), args...));
				return str;
			}
		}
		else
		{
			if constexpr ((!line) && sizeof...(args) == 1 && (scatter_printable<ch_type, Args> && ...))
			{
				basic_io_scatter_t<ch_type> scatter{print_scatter_define_extract_one<ch_type>(args...)};
				return strlike_construct_define(io_strlike_type<ch_type, T>(scatter.base, scatter.base + scatter.len));
			}
			else if constexpr ((scatter_printable<ch_type, Args> && ...))
			{
				T str;
				basic_general_concat_decay_ref_impl_all_scatter<line, ch_type>(str, args...);
				return str;
			}
			else
			{
				::std::size_t total_size{::fast_io::details::intrinsics::add_or_overflow_die(
					sz_with_line, calculate_scatter_dynamic_reserve_size_with_scatter<ch_type>(args...))};
				T str;
				strlike_reserve(io_strlike_type<ch_type, T>, str, total_size);
				strlike_set_curr(io_strlike_type<ch_type, T>, str,
								 print_reserve_define_chain_scatter_impl<line>(
									 strlike_begin(io_strlike_type<ch_type, T>, str), args...));
				return str;
			}
		}
	}
	else
	{
		T str;
		auto oref{io_strlike_ref(fast_io::io_alias, str)};
		::fast_io::operations::decay::print_freestanding_decay<line>(oref, args...);
		return str;
	}
}

template <bool line, ::std::integral ch_type, typename T, typename Arg>
inline constexpr void basic_general_concat_decay_ref_impl_precise(T &str, Arg arg)
{
	::std::size_t precise_size{print_reserve_precise_size(io_reserve_type<ch_type, Arg>, arg)};
	::std::size_t precise_size_with_line{precise_size};
	if constexpr (line)
	{
		++precise_size_with_line;
	}
	constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
	if (local_cap < precise_size_with_line)
	{
		strlike_reserve(io_strlike_type<ch_type, T>, str, precise_size_with_line);
	}
	auto first{strlike_begin(io_strlike_type<ch_type, T>, str)};
	print_reserve_precise_define(io_reserve_type<ch_type, Arg>, first, precise_size, arg);
	auto ptr{first + precise_size};
	if constexpr (line)
	{
		*ptr = char_literal_v<u8'\n', ch_type>;
		++ptr;
	}
	strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
}

template <::std::integral ch_type, typename... Args>
inline constexpr bool basic_general_concat_semantic_precise_ok =
	(false || ... || ::fast_io::details::decay::print_semantic_node<Args>) &&
	(::fast_io::details::decay::print_semantic_precise_size_ok<ch_type, ::std::remove_cvref_t<Args>>::value && ...);

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl_semantic_precise(T &str, Args... args)
{
	::std::size_t const precise_size{
		::fast_io::operations::decay::print_semantic_precise_total_size<line, ch_type>(args...)};
	if constexpr (sso_buffer_strlike<ch_type, T>)
	{
		constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
		if (local_cap < precise_size)
		{
			strlike_reserve(io_strlike_type<ch_type, T>, str, precise_size);
		}
	}
	else
	{
		strlike_reserve(io_strlike_type<ch_type, T>, str, precise_size);
	}
	auto first{strlike_begin(io_strlike_type<ch_type, T>, str)};
	auto ptr{::fast_io::operations::decay::print_semantic_emit_unchecked_run<line, ch_type>(first, args...)};
	strlike_set_curr(io_strlike_type<ch_type, T>, str, ptr);
}

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr void basic_general_concat_decay_ref_impl(T &str, Args... args)
{
	if constexpr (basic_general_concat_semantic_precise_ok<ch_type, Args...>)
	{
		basic_general_concat_decay_ref_impl_semantic_precise<line, ch_type>(str, args...);
	}
	else if constexpr (((reserve_printable<ch_type, Args> || scatter_printable<ch_type, Args> ||
						 dynamic_reserve_printable<ch_type, Args>) &&
						...))
	{
		constexpr ::std::size_t sz{calculate_scatter_reserve_size<ch_type, Args...>()};
		if constexpr (line)
		{
			static_assert(sz != SIZE_MAX, "overflow\n");
		}
		constexpr ::std::size_t sz_with_line{sz + static_cast<::std::size_t>(line)};
		if constexpr ((reserve_printable<ch_type, Args> && ...))
		{
			if constexpr (sso_buffer_strlike<ch_type, T>)
			{
				constexpr ::std::size_t local_cap{strlike_sso_size(io_strlike_type<ch_type, T>)};
				constexpr bool not_enough_space{(local_cap < sz_with_line)};
				if constexpr (not_enough_space &&
							  ((sizeof...(Args) == 1) && (precise_reserve_printable<ch_type, Args> && ...)))
				{
					basic_general_concat_decay_ref_impl_precise<line, ch_type, T>(str, args...);
				}
				else
				{
					if constexpr (not_enough_space)
					{
						strlike_reserve(io_strlike_type<ch_type, T>, str, sz_with_line);
					}
					strlike_set_curr(io_strlike_type<ch_type, T>, str,
									 print_reserve_define_chain_impl<line>(
										 strlike_begin(io_strlike_type<ch_type, T>, str), args...));
				}
			}
			else
			{
				strlike_reserve(io_strlike_type<ch_type, T>, str, sz_with_line);
				strlike_set_curr(
					io_strlike_type<ch_type, T>, str,
					print_reserve_define_chain_impl<line>(strlike_begin(io_strlike_type<ch_type, T>, str), args...));
			}
		}
		else
		{
			if constexpr ((scatter_printable<ch_type, Args> && ...))
			{
				basic_general_concat_decay_ref_impl_all_scatter<line, ch_type>(str, args...);
			}
			else
			{
				::std::size_t total_size{::fast_io::details::intrinsics::add_or_overflow_die(
					sz_with_line, calculate_scatter_dynamic_reserve_size_with_scatter<ch_type>(args...))};
				strlike_reserve(io_strlike_type<ch_type, T>, str, total_size);
				strlike_set_curr(io_strlike_type<ch_type, T>, str,
								 print_reserve_define_chain_scatter_impl<line>(
									 strlike_begin(io_strlike_type<ch_type, T>, str), args...));
			}
		}
	}
	else
	{
		auto oref{io_strlike_ref(fast_io::io_alias, str)};
		::fast_io::operations::decay::print_freestanding_decay<line>(oref, args...);
	}
}

template <bool line, ::std::integral ch_type, typename T, typename... Args>
inline constexpr T basic_general_concat_phase1_decay_impl(Args... args)
{
	if constexpr (sizeof...(Args) == 0)
	{
		if constexpr (line)
		{
			if constexpr (single_character_constructible_strlike<ch_type, T>)
			{
				return strlike_construct_single_character_define(io_strlike_type<ch_type, T>,
																 char_literal_v<u8'\n', ch_type>);
			}
			else
			{
				return strlike_construct_define(io_strlike_type<ch_type, T>,
												__builtin_addressof(char_literal_v<u8'\n', ch_type>),
												__builtin_addressof(char_literal_v<u8'\n', ch_type>) + 1);
			}
		}
		else
		{
			return {};
		}
	}
	else
	{
		if constexpr (basic_general_concat_semantic_precise_ok<ch_type, Args...>)
		{
			if constexpr (buffer_strlike<ch_type, T>)
			{
				T str;
				basic_general_concat_decay_ref_impl_semantic_precise<line, ch_type>(str, args...);
				return str;
			}
			else
			{
				basic_concat_buffer<ch_type> buffer;
				basic_general_concat_decay_ref_impl_semantic_precise<line, ch_type>(buffer, args...);
				return strlike_construct_define(io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
			}
		}
		else if constexpr (buffer_strlike<ch_type, T>)
		{
			T str;
			basic_general_concat_decay_ref_impl<line, ch_type>(str, args...);
			return str;
		}
		else if constexpr ((!line) && sizeof...(args) == 1 && (scatter_printable<ch_type, Args> && ...))
		{
			basic_io_scatter_t<ch_type> scatter{print_scatter_define_extract_one<ch_type>(args...)};
			return strlike_construct_define(io_strlike_type<ch_type, T>, scatter.base, scatter.base + scatter.len);
		}
		else if constexpr ((reserve_printable<ch_type, Args> && ...))
		{
			constexpr ::std::size_t sz{calculate_scatter_reserve_size<ch_type, Args...>()};
			if constexpr (line)
			{
				static_assert(sz != SIZE_MAX, "overflow\n");
			}
			constexpr ::std::size_t sz_with_line{sz + static_cast<::std::size_t>(line)};
			ch_type buffer[sz_with_line];
			auto p{print_reserve_define_chain_impl<line>(buffer, args...)};
			return strlike_construct_define(io_strlike_type<ch_type, T>, buffer, p);
		}
		else
		{
			basic_concat_buffer<ch_type> buffer;
			basic_general_concat_decay_ref_impl<line, ch_type>(buffer, args...);
			return strlike_construct_define(io_strlike_type<ch_type, T>, buffer.buffer_begin, buffer.buffer_curr);
		}
	}
}

} // namespace fast_io::details::decay

namespace fast_io
{

template <bool line, ::std::integral char_type, typename T, typename... Args>
	requires strlike<char_type, T>
inline constexpr T basic_general_concat(Args &&...args)
{
	return ::fast_io::details::decay::basic_general_concat_phase1_decay_impl<line, char_type, T>(
		io_print_forward<char_type>(io_print_alias(args))...);
}

} // namespace fast_io
