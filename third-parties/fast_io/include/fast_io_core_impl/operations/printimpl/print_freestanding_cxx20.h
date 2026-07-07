#pragma once

namespace fast_io
{

namespace details::decay
{

template <::std::integral char_type, typename T = char_type>
inline constexpr basic_io_scatter_t<T> line_scatter_common{__builtin_addressof(char_literal_v<u8'\n', char_type>),
														   ::std::same_as<T, void> ? sizeof(char_type) : 1};

struct contiguous_scatter_result
{
	::std::size_t position{};
	::std::size_t neededscatters{};
	::std::size_t neededspace{};
	::std::size_t null{};
	bool lastisreserve{};
	bool hasscatters{};
	bool hasreserve{};
	bool hasdynamicreserve{};
};

template <::std::integral char_type, typename Arg, typename... Args>
inline constexpr contiguous_scatter_result find_continuous_scatters_n()
{
	contiguous_scatter_result ret{};
	if constexpr (::fast_io::scatter_printable<char_type, Arg>)
	{
		if constexpr (sizeof...(Args) != 0)
		{
			ret = find_continuous_scatters_n<char_type, Args...>();
		}
		++ret.position;
		ret.hasscatters = true;
		ret.neededscatters = ::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.neededscatters,
																					   static_cast<::std::size_t>(1));
	}
	else if constexpr (::fast_io::reserve_printable<char_type, Arg>)
	{
		if constexpr (sizeof...(Args) != 0)
		{
			ret = find_continuous_scatters_n<char_type, Args...>();
		}
		else
		{
			ret.lastisreserve = true;
		}
		constexpr ::std::size_t sz{print_reserve_size(::fast_io::io_reserve_type<char_type, Arg>)};
		static_assert(sz != 0);
		++ret.position;
		ret.neededscatters = ::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.neededscatters,
																					   static_cast<::std::size_t>(1));
		ret.neededspace = ::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.neededspace, sz);
		ret.hasreserve = true;
	}
	else if constexpr (::fast_io::dynamic_reserve_printable<char_type, Arg>)
	{
		if constexpr (sizeof...(Args) != 0)
		{
			ret = find_continuous_scatters_n<char_type, Args...>();
		}
		else
		{
			ret.lastisreserve = true;
		}
		++ret.position;
		ret.neededscatters = ::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.neededscatters,
																					   static_cast<::std::size_t>(1));
		ret.hasdynamicreserve = true;
	}
	else if constexpr (::fast_io::reserve_scatters_printable<char_type, Arg>)
	{
		if constexpr (sizeof...(Args) != 0)
		{
			ret = find_continuous_scatters_n<char_type, Args...>();
		}
		constexpr auto scatszres{print_reserve_scatters_size(::fast_io::io_reserve_type<char_type, Arg>)};
		static_assert(scatszres.scatters_size != 0);
		ret.hasscatters = true;
		ret.hasreserve = true;
		++ret.position;
		ret.neededspace =
			::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.neededspace, scatszres.reserve_size);
		ret.neededscatters =
			::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.neededscatters, scatszres.scatters_size);
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<Arg>, ::fast_io::io_null_t>)
	{
		if constexpr (sizeof...(Args) != 0)
		{
			ret = find_continuous_scatters_n<char_type, Args...>();
		}
		++ret.position;
		ret.null = ::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.null,
																			 static_cast<::std::size_t>(1));
	}
	else if constexpr (::fast_io::printable<char_type, Arg>)
	{
	}
	return ret;
}

struct scatter_rsv_result
{
	::std::size_t position{};
	::std::size_t neededspace{};
	::std::size_t null{};
};

template <bool findscatter, ::std::integral char_type, typename Arg, typename... Args>
inline constexpr scatter_rsv_result find_continuous_scatters_reserve_n()
{
	if constexpr (::fast_io::reserve_printable<char_type, Arg> && !findscatter)
	{
		constexpr ::std::size_t sz{print_reserve_size(::fast_io::io_reserve_type<char_type, Arg>)};
		if constexpr (sizeof...(Args) == 0)
		{
			return {1, sz, 0};
		}
		else
		{
			auto res{find_continuous_scatters_reserve_n<findscatter, char_type, Args...>()};
			return {res.position + 1, ::fast_io::details::intrinsics::add_or_overflow_die_chain(res.neededspace, sz), res.null};
		}
	}
	else if constexpr (::fast_io::scatter_printable<char_type, Arg> && findscatter)
	{
		if constexpr (sizeof...(Args) == 0)
		{
			return {1, 0, 0};
		}
		else
		{
			auto res{find_continuous_scatters_reserve_n<findscatter, char_type, Args...>()};
			return {res.position + 1, res.neededspace, res.null};
		}
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<Arg>, ::fast_io::io_null_t>)
	{
		if constexpr (sizeof...(Args) == 0)
		{
			return {1, 0, 1};
		}
		else
		{
			auto res{find_continuous_scatters_reserve_n<findscatter, char_type, Args...>()};
			return {res.position + 1, res.neededspace, ::fast_io::details::intrinsics::add_or_overflow_die_chain(res.null, static_cast<::std::size_t>(1))};
		}
	}
	else
	{
		return {0, 0, 0};
	}
}

template <typename output, ::std::size_t N>
inline constexpr bool minimum_buffer_output_stream_require_size_constant_impl =
	(N < obuffer_minimum_size_define(::fast_io::io_reserve_type<typename output::output_char_type, output>));

template <typename output, ::std::size_t N>
concept minimum_buffer_output_stream_require_size_impl =
	::fast_io::operations::decay::defines::has_obuffer_minimum_size_operations<output> &&
	minimum_buffer_output_stream_require_size_constant_impl<output, N>;

inline constexpr ::std::size_t print_stack_buffer_default_max_bytes{
	::fast_io::details::print_stack_buffer_default_max_bytes};

inline constexpr ::std::size_t print_stack_buffer_max_bytes() noexcept
{
	return ::fast_io::details::print_stack_buffer_max_bytes();
}

template <typename element_type>
inline constexpr ::std::size_t print_stack_buffer_max_element_count() noexcept
{
	constexpr ::std::size_t max_bytes{::fast_io::details::decay::print_stack_buffer_max_bytes()};
	if constexpr (max_bytes < sizeof(element_type))
	{
		return 0u;
	}
	else
	{
		return max_bytes / sizeof(element_type);
	}
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_stack_buffer_max_size() noexcept
{
	return ::fast_io::details::decay::print_stack_buffer_max_element_count<char_type>();
}

template <::std::size_t requested_size, typename element_type>
inline constexpr bool print_stack_buffer_size_within_limit =
	requested_size != 0 &&
	requested_size <= ::fast_io::details::decay::print_stack_buffer_max_element_count<element_type>();

template <::std::size_t static_stack_size, ::std::integral char_type>
inline constexpr ::std::size_t dynamic_print_reserve_static_stack_budget() noexcept
{
	/*
	A dynamic producer's static stack size is only a local materialization hint.
	A print run can contain many dynamic producers, so directly merging all hints into one
	stack array can create a large frame and defeat the stack-safety purpose of this path.
	*/
	constexpr ::std::size_t run_cap{::fast_io::details::decay::print_stack_buffer_max_size<char_type>()};
	if constexpr (static_stack_size < run_cap)
	{
		return static_stack_size;
	}
	else
	{
		return run_cap;
	}
}

template <bool line, ::std::integral char_type, typename T>
inline constexpr ::std::size_t dynamic_print_reserve_static_stack_size()
{
	using nocvreft = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, nocvreft>)
	{
		constexpr ::std::size_t static_stack_size{
			print_reserve_static_stack_size(::fast_io::io_reserve_type<char_type, nocvreft>)};
		static_assert(!line || static_stack_size != SIZE_MAX);
		return ::fast_io::details::decay::dynamic_print_reserve_static_stack_budget<static_stack_size, char_type>() +
			   static_cast<::std::size_t>(line);
	}
	else
	{
		return 0;
	}
}

template <bool line, ::std::integral char_type, typename T>
inline constexpr ::std::size_t context_print_static_buffer_size() noexcept
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::context_printable_with_static_buffer_size<char_type, value_type>)
	{
		constexpr ::std::size_t n{
			print_context_static_buffer_size(::fast_io::io_reserve_type<char_type, value_type>)};
		static_assert(n != 0);
		static_assert(n != SIZE_MAX);
		static_assert(n > static_cast<::std::size_t>(line));
		return n;
	}
	else
	{
		return 32u;
	}
}

template <bool line, ::std::integral char_type, typename T>
inline constexpr ::std::size_t context_print_static_buffer_size_v{
	::fast_io::details::decay::context_print_static_buffer_size<line, char_type, T>()};

template <::std::size_t sz>
	requires(sz != 0)
inline constexpr void scatter_rsv_update_times(::fast_io::io_scatter_t *first, ::fast_io::io_scatter_t *last) noexcept
{
	if constexpr (sz != 1)
	{
		for (; first != last; ++first)
		{
			first->len *= sz;
		}
	}
}

template <::std::integral char_type>
struct basic_reserve_scatters_define_byte_result
{
	io_scatter_t *scatters_pos_ptr;
	char_type *reserve_pos_ptr;
};

template <::std::integral char_type, typename T>
inline ::fast_io::details::decay::basic_reserve_scatters_define_byte_result<char_type>
prrsvsct_byte_common_rsvsc_impl(io_scatter_t *pscatters, char_type *buffer, T t)
{
	using basicioscattertypealiasptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= basic_io_scatter_t<char_type> *;
	using scatterioscattertypealiasptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= io_scatter_t *;
	auto result{print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>,
											  reinterpret_cast<basicioscattertypealiasptr>(pscatters), buffer, t)};
	scatterioscattertypealiasptr ptr{reinterpret_cast<scatterioscattertypealiasptr>(result.scatters_pos_ptr)};
	if constexpr (sizeof(char_type) != 1)
	{
		scatter_rsv_update_times<sizeof(char_type)>(pscatters, ptr);
	}
	return {ptr, result.reserve_pos_ptr};
}

template <::std::integral char_type, typename T>
inline auto prrsvsct_byte_common_impl(io_scatter_t *pscatters, char_type *buffer, T t)
{
	return ::fast_io::details::decay::prrsvsct_byte_common_rsvsc_impl(pscatters, buffer, t).scatters_pos_ptr;
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_scatter_full_output_threshold_max_chars() noexcept
{
	return ::fast_io::details::decay::print_stack_buffer_max_size<char_type>();
}

template <::std::integral char_type, typename outputstmtype>
inline constexpr ::std::size_t print_scatter_full_output_threshold() noexcept
{
	if constexpr (::fast_io::scatter_fallback_full_output_threshold_stream<char_type, outputstmtype>)
	{
		constexpr ::std::size_t threshold{
			scatter_fallback_full_output_threshold(
				::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<outputstmtype>>)};
		constexpr ::std::size_t max_threshold{
			::fast_io::details::decay::print_scatter_full_output_threshold_max_chars<char_type>()};
		if constexpr (threshold < max_threshold)
		{
			return threshold;
		}
		else
		{
			return max_threshold;
		}
	}
	else
	{
		return 0u;
	}
}

template <typename outputstmtype>
inline constexpr bool print_has_direct_write_bytes_operations =
	::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outputstmtype> ||
	::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outputstmtype> ||
	(sizeof(typename outputstmtype::output_char_type) == 1 &&
	 (::fast_io::operations::decay::defines::has_write_all_overflow_define<outputstmtype> ||
	  ::fast_io::operations::decay::defines::has_write_some_overflow_define<outputstmtype>));

template <typename outputstmtype>
inline constexpr bool print_has_direct_scatter_write_bytes_operations =
	::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outputstmtype> ||
	::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<outputstmtype> ||
	(sizeof(typename outputstmtype::output_char_type) == 1 &&
	 (::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outputstmtype> ||
	  ::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outputstmtype>));

template <typename outputstmtype>
inline constexpr bool print_has_direct_write_operations =
	::fast_io::operations::decay::defines::has_write_all_overflow_define<outputstmtype> ||
	::fast_io::operations::decay::defines::has_write_some_overflow_define<outputstmtype> ||
	(sizeof(typename outputstmtype::output_char_type) == 1 &&
	 (::fast_io::operations::decay::defines::has_write_all_bytes_overflow_define<outputstmtype> ||
	  ::fast_io::operations::decay::defines::has_write_some_bytes_overflow_define<outputstmtype>));

template <typename outputstmtype>
inline constexpr bool print_has_direct_scatter_write_operations =
	::fast_io::operations::decay::defines::has_scatter_write_all_overflow_define<outputstmtype> ||
	::fast_io::operations::decay::defines::has_scatter_write_some_overflow_define<outputstmtype> ||
	(sizeof(typename outputstmtype::output_char_type) == 1 &&
	 (::fast_io::operations::decay::defines::has_scatter_write_all_bytes_overflow_define<outputstmtype> ||
	  ::fast_io::operations::decay::defines::has_scatter_write_some_bytes_overflow_define<outputstmtype>));

template <typename char_type>
inline constexpr char_type *print_small_scatter_copy_n(char_type const *first, ::std::size_t count,
													   char_type *result) noexcept
{
	if (count <= 16u)
	{
		for (::std::size_t i{}; i != count; ++i)
		{
			result[i] = first[i];
		}
		return result + count;
	}
	return ::fast_io::details::non_overlapped_copy_n(first, count, result);
}

template <typename outputstmtype>
inline constexpr void print_scatter_write_all_bytes_maybe_coalesce(outputstmtype outstm,
																   ::fast_io::io_scatter_t const *scatters,
																   ::std::size_t n)
{
	if (n == 0)
	{
		return;
	}
	if (n == 1)
	{
		::std::size_t const len{scatters->len};
		if (len != 0)
		{
			auto first{static_cast<::std::byte const *>(scatters->base)};
			::fast_io::operations::decay::write_all_bytes_decay(outstm, first, first + len);
		}
		return;
	}
	using char_type = typename outputstmtype::output_char_type;
	constexpr ::std::size_t threshold_chars{
		::fast_io::details::decay::print_scatter_full_output_threshold<char_type, outputstmtype>()};
	if constexpr (threshold_chars != 0 &&
				  ::fast_io::details::decay::print_has_direct_write_bytes_operations<outputstmtype> &&
				  ::fast_io::details::decay::print_has_direct_scatter_write_bytes_operations<outputstmtype>)
	{
		char_type buffer[threshold_chars];
		auto curr{reinterpret_cast<::std::byte *>(buffer)};
		auto const first{curr};
		::std::size_t remaining{threshold_chars * sizeof(char_type) - 1u};
		for (auto iter{scatters}, last{scatters + n}; iter != last; ++iter)
		{
			::std::size_t const len{iter->len};
			if (remaining < len)
			{
				::fast_io::operations::decay::scatter_write_all_bytes_decay(outstm, scatters, n);
				return;
			}
			if (len != 0)
			{
				curr = ::fast_io::details::decay::print_small_scatter_copy_n(
					static_cast<::std::byte const *>(iter->base), len, curr);
				remaining -= len;
			}
		}
		if (curr != first)
		{
			::fast_io::operations::decay::write_all_bytes_decay(outstm, first, curr);
		}
		return;
	}
	::fast_io::operations::decay::scatter_write_all_bytes_decay(outstm, scatters, n);
}

template <typename outputstmtype>
inline constexpr void print_scatter_write_all_maybe_coalesce(
	outputstmtype outstm, ::fast_io::basic_io_scatter_t<typename outputstmtype::output_char_type> const *scatters,
	::std::size_t n)
{
	using char_type = typename outputstmtype::output_char_type;
	if (n == 0)
	{
		return;
	}
	if (n == 1)
	{
		auto [base, len] = *scatters;
		if (len != 0)
		{
			::fast_io::operations::decay::write_all_decay(outstm, base, base + len);
		}
		return;
	}
	constexpr ::std::size_t threshold_chars{
		::fast_io::details::decay::print_scatter_full_output_threshold<char_type, outputstmtype>()};
	if constexpr (threshold_chars != 0 &&
				  ::fast_io::details::decay::print_has_direct_write_operations<outputstmtype> &&
				  ::fast_io::details::decay::print_has_direct_scatter_write_operations<outputstmtype>)
	{
		char_type buffer[threshold_chars];
		char_type *curr{buffer};
		::std::size_t remaining{threshold_chars - 1u};
		for (auto iter{scatters}, last{scatters + n}; iter != last; ++iter)
		{
			::std::size_t const len{iter->len};
			if (remaining < len)
			{
				::fast_io::operations::decay::scatter_write_all_decay(outstm, scatters, n);
				return;
			}
			if (len != 0)
			{
				curr = ::fast_io::details::decay::print_small_scatter_copy_n(iter->base, len, curr);
				remaining -= len;
			}
		}
		if (curr != buffer)
		{
			::fast_io::operations::decay::write_all_decay(outstm, buffer, curr);
		}
		return;
	}
	::fast_io::operations::decay::scatter_write_all_decay(outstm, scatters, n);
}

template <typename outputstmtype, typename scatter_type>
inline constexpr void print_scatter_write_all_dispatch(outputstmtype outstm, scatter_type const *scatters,
													   ::std::size_t n)
{
	if constexpr (::std::same_as<scatter_type, ::fast_io::io_scatter_t>)
	{
		::fast_io::details::decay::print_scatter_write_all_bytes_maybe_coalesce(outstm, scatters, n);
	}
	else
	{
		::fast_io::details::decay::print_scatter_write_all_maybe_coalesce(outstm, scatters, n);
	}
}

template <bool needprintlf, typename outputstmtype, ::std::integral char_type, typename scatter_type>
inline constexpr void print_scatter_write_all_dispatch_with_line(outputstmtype outstm, scatter_type *scatters,
																 ::std::size_t n)
{
	if constexpr (needprintlf)
	{
		if constexpr (::std::same_as<scatter_type, ::fast_io::io_scatter_t>)
		{
			scatters[n - 1u] = ::fast_io::details::decay::line_scatter_common<char_type, void>;
		}
		else
		{
			scatters[n - 1u] = ::fast_io::details::decay::line_scatter_common<char_type>;
		}
	}
	::fast_io::details::decay::print_scatter_write_all_dispatch(outstm, scatters, n);
}

template <typename outputstmtype, ::std::integral char_type>
inline constexpr void print_single_scatter_with_line(outputstmtype outstm,
													 ::fast_io::basic_io_scatter_t<char_type> scatter_res)
{
	if constexpr (::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<outputstmtype>)
	{
		if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<2u, ::fast_io::io_scatter_t>)
		{
			::fast_io::io_scatter_t scatters[2];
			scatters[0] = {scatter_res.base, scatter_res.len * sizeof(char_type)};
			scatters[1] = {__builtin_addressof(char_literal_v<u8'\n', char_type>), sizeof(char_type)};
			::fast_io::details::decay::print_scatter_write_all_bytes_maybe_coalesce(outstm, scatters, 2u);
		}
		else
		{
			::fast_io::details::local_operator_new_array_ptr<::fast_io::io_scatter_t> scatters(2u);
			scatters.ptr[0] = {scatter_res.base, scatter_res.len * sizeof(char_type)};
			scatters.ptr[1] = {__builtin_addressof(char_literal_v<u8'\n', char_type>), sizeof(char_type)};
			::fast_io::details::decay::print_scatter_write_all_bytes_maybe_coalesce(outstm, scatters.ptr, 2u);
		}
	}
	else
	{
		using scatter_type = ::fast_io::basic_io_scatter_t<char_type>;
		if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<2u, scatter_type>)
		{
			scatter_type scatters[2];
			scatters[0] = scatter_res;
			scatters[1] = {__builtin_addressof(char_literal_v<u8'\n', char_type>), 1u};
			::fast_io::details::decay::print_scatter_write_all_maybe_coalesce(outstm, scatters, 2u);
		}
		else
		{
			::fast_io::details::local_operator_new_array_ptr<scatter_type> scatters(2u);
			scatters.ptr[0] = scatter_res;
			scatters.ptr[1] = {__builtin_addressof(char_literal_v<u8'\n', char_type>), 1u};
			::fast_io::details::decay::print_scatter_write_all_maybe_coalesce(outstm, scatters.ptr, 2u);
		}
	}
}

template <bool line, typename outputstmtype, ::std::integral char_type, typename T>
inline constexpr void print_reserve_scatters_bytes_with_scatter(outputstmtype outstm, ::fast_io::io_scatter_t *scatters,
																T t)
{
	using value_type = ::std::remove_cvref_t<T>;
	constexpr auto sz{print_reserve_scatters_size(::fast_io::io_reserve_type<char_type, value_type>)};
	constexpr ::std::size_t reserve_buffer_size{sz.reserve_size == 0 ? 1u : sz.reserve_size};
	if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<reserve_buffer_size, char_type>)
	{
		char_type buffer[reserve_buffer_size];
		::fast_io::io_scatter_t *ptr{::fast_io::details::decay::prrsvsct_byte_common_impl(scatters, buffer, t)};
		if constexpr (line)
		{
			*ptr = ::fast_io::details::decay::line_scatter_common<char_type, void>;
			++ptr;
		}
		::fast_io::details::decay::print_scatter_write_all_bytes_maybe_coalesce(
			outstm, scatters, static_cast<::std::size_t>(ptr - scatters));
	}
	else
	{
		::fast_io::details::local_operator_new_array_ptr<char_type> buffer(reserve_buffer_size);
		::fast_io::io_scatter_t *ptr{::fast_io::details::decay::prrsvsct_byte_common_impl(scatters, buffer.ptr, t)};
		if constexpr (line)
		{
			*ptr = ::fast_io::details::decay::line_scatter_common<char_type, void>;
			++ptr;
		}
		::fast_io::details::decay::print_scatter_write_all_bytes_maybe_coalesce(
			outstm, scatters, static_cast<::std::size_t>(ptr - scatters));
	}
}

template <bool line, typename outputstmtype, ::std::integral char_type, typename T>
inline constexpr void print_reserve_scatters_basic_with_scatter(
	outputstmtype outstm, ::fast_io::basic_io_scatter_t<char_type> *scatters, T t)
{
	using value_type = ::std::remove_cvref_t<T>;
	constexpr auto sz{print_reserve_scatters_size(::fast_io::io_reserve_type<char_type, value_type>)};
	constexpr ::std::size_t reserve_buffer_size{sz.reserve_size == 0 ? 1u : sz.reserve_size};
	if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<reserve_buffer_size, char_type>)
	{
		char_type buffer[reserve_buffer_size];
		auto ptr{print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, value_type>, scatters, buffer, t)
					 .scatters_pos_ptr};
		if constexpr (line)
		{
			*ptr = ::fast_io::details::decay::line_scatter_common<char_type>;
			++ptr;
		}
		::fast_io::details::decay::print_scatter_write_all_maybe_coalesce(
			outstm, scatters, static_cast<::std::size_t>(ptr - scatters));
	}
	else
	{
		::fast_io::details::local_operator_new_array_ptr<char_type> buffer(reserve_buffer_size);
		auto ptr{print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, value_type>, scatters, buffer.ptr,
											   t)
					 .scatters_pos_ptr};
		if constexpr (line)
		{
			*ptr = ::fast_io::details::decay::line_scatter_common<char_type>;
			++ptr;
		}
		::fast_io::details::decay::print_scatter_write_all_maybe_coalesce(
			outstm, scatters, static_cast<::std::size_t>(ptr - scatters));
	}
}

template <bool line, typename outputstmtype, ::std::integral char_type, typename T>
inline constexpr void print_control_single_reserve_scatters(outputstmtype outstm, T t)
{
	using value_type = ::std::remove_cvref_t<T>;
	constexpr auto sz{print_reserve_scatters_size(::fast_io::io_reserve_type<char_type, value_type>)};
	static_assert(sz.scatters_size != SIZE_MAX);
	constexpr ::std::size_t scattersnum{sz.scatters_size + static_cast<::std::size_t>(line)};
#if __cpp_if_consteval >= 202106L
	if !consteval
#else
	if (!__builtin_is_constant_evaluated())
#endif
	{
		if constexpr (::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<
						  outputstmtype>)
		{
			if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<scattersnum,
																						  ::fast_io::io_scatter_t>)
			{
				::fast_io::io_scatter_t scatters[scattersnum];
				::fast_io::details::decay::print_reserve_scatters_bytes_with_scatter<line, outputstmtype, char_type>(
					outstm, scatters, t);
			}
			else
			{
				::fast_io::details::local_operator_new_array_ptr<::fast_io::io_scatter_t> scatters(scattersnum);
				::fast_io::details::decay::print_reserve_scatters_bytes_with_scatter<line, outputstmtype, char_type>(
					outstm, scatters.ptr, t);
			}
			return;
		}
	}
	using scatter_type = ::fast_io::basic_io_scatter_t<char_type>;
	if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<scattersnum, scatter_type>)
	{
		scatter_type scatters[scattersnum];
		::fast_io::details::decay::print_reserve_scatters_basic_with_scatter<line, outputstmtype, char_type>(
			outstm, scatters, t);
	}
	else
	{
		::fast_io::details::local_operator_new_array_ptr<scatter_type> scatters(scattersnum);
		::fast_io::details::decay::print_reserve_scatters_basic_with_scatter<line, outputstmtype, char_type>(
			outstm, scatters.ptr, t);
	}
}

template <bool line, typename outputstmtype, typename context_type, typename T, ::std::integral char_type>
inline constexpr void print_context_define_to_window(outputstmtype outstm, context_type &st, T t, char_type *buffer,
													 char_type *buffered)
{
	for (;;)
	{
		auto [resit, done] = st.print_context_define(t, buffer, buffered);
		if constexpr (line)
		{
			if (done)
			{
				*resit = ::fast_io::char_literal_v<u8'\n', char_type>;
				++resit;
			}
			::fast_io::operations::decay::write_all_decay(outstm, buffer, resit);
			if (done)
			{
				return;
			}
		}
		else
		{
			::fast_io::operations::decay::write_all_decay(outstm, buffer, resit);
			if (done)
			{
				return;
			}
		}
	}
}

template <bool line = false, typename output, typename T>
	requires(::std::is_trivially_copyable_v<output> && ::std::is_trivially_copyable_v<T>)
inline constexpr void print_control_single(output outstm, T t)
{
	using char_type = typename output::output_char_type;
	using value_type = ::std::remove_cvref_t<T>;
	constexpr bool asan_activated{::fast_io::details::asan_state::current == ::fast_io::details::asan_state::activate};
	constexpr auto lfch{char_literal_v<u8'\n', char_type>};
	if constexpr (scatter_printable<char_type, value_type>)
	{
#if 0
		basic_io_scatter_t<char_type> scatter;
		if constexpr(::std::same_as<T,basic_io_scatter_t<char_type>>)
		{
			scatter=t;
		}
		else
		{
			scatter=print_scatter_define(::fast_io::io_reserve_type<char_type,value_type>,t);
		}
		if constexpr(line)
		{
			if constexpr(contiguous_output_stream<output>)
			{
				auto curr=obuffer_curr(out);
				auto end=obuffer_end(out);
				::std::ptrdiff_t sz(end-curr-1);
				::std::size_t const len{scatter.len};
				if(sz<static_cast<::std::ptrdiff_t>(len))
					fast_terminate();
				curr=non_overlapped_copy_n(scatter.base,scatter.len,curr);
				*curr=lfch;
				++curr;
				obuffer_set_curr(outstm,curr);
			}
			else if constexpr(::fast_io::operations::decay::defines::has_obuffer_basic_operations<output>)
			{
				auto curr=obuffer_curr(out);
				auto end=obuffer_end(out);
				::std::size_t const len{scatter.len};
				::std::ptrdiff_t sz(end-curr-1);
				if(static_cast<::std::ptrdiff_t>(len)<sz)[[likely]]
				{
					curr=details::non_overlapped_copy_n(scatter.base,len,curr);
					*curr=lfch;
					++curr;
					obuffer_set_curr(outstm,curr);
				}
				else
				{
					write(outstm,scatter.base,scatter.base+scatter.len);
					put(outstm,lfch);
				}
			}
			else 
			{
				write(outstm,scatter.base,scatter.base+scatter.len);
				write(outstm,__builtin_addressof(lfch),
				__builtin_addressof(lfch)+1);
			}
		}
		else
		{
			write(outstm,scatter.base,scatter.base+scatter.len);
		}
#endif
		basic_io_scatter_t<char_type> scatter_res{print_scatter_define(::fast_io::io_reserve_type<char_type, value_type>, t)};

			if constexpr (line)
			{
				::fast_io::details::decay::print_single_scatter_with_line(outstm, scatter_res);
			}
			else
			{
				::fast_io::operations::decay::write_all_decay(outstm, scatter_res.base, scatter_res.base + scatter_res.len);
		}
	}
	else if constexpr (reserve_printable<char_type, value_type>)
	{
		constexpr ::std::size_t real_size{print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>)};
		constexpr ::std::size_t size{real_size + static_cast<::std::size_t>(line)};
		static_assert(real_size < PTRDIFF_MAX);
#if 0
		if constexpr(contiguous_output_stream<output>)
		{
			auto bcurr{obuffer_curr(outstm)};
			auto bend{obuffer_end(outstm)};
			::std::size_t diff{static_cast<::std::size_t>(bend-bcurr)};
			if(diff<size)[[unlikely]]
				fast_terminate();
			auto it{print_reserve_define(::fast_io::io_reserve_type<char_type,value_type>,bcurr,t)};
			if constexpr(line)
			{
				*it=lfch;
				++it;
			}
			obuffer_set_curr(outstm,it);
		}
		else
#endif
		{
			if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<output> &&
						  !asan_activated)
			{
				char_type *bcurr{obuffer_curr(outstm)};
				char_type *bend{obuffer_end(outstm)};
				::std::ptrdiff_t const diff(bend - bcurr);
				bool smaller{static_cast<::std::ptrdiff_t>(size) < diff};
				if constexpr (minimum_buffer_output_stream_require_size_impl<output, size>)
				{
					if (!smaller) [[unlikely]]
					{
						obuffer_minimum_size_flush_prepare_define(outstm);
						bcurr = obuffer_curr(outstm);
					}
					bcurr = print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, bcurr, t);
					if constexpr (line)
					{
						*bcurr = lfch;
						++bcurr;
					}
					obuffer_set_curr(outstm, bcurr);
				}
				else
				{
					if (smaller) [[likely]]
					{
						bcurr = print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, bcurr, t);
						if constexpr (line)
						{
							*bcurr = lfch;
							++bcurr;
						}
						obuffer_set_curr(outstm, bcurr);
					}
					else [[unlikely]]
					{
						if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<size, char_type>)
						{
							char_type buffer[size];
							char_type *i{
								print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, buffer, t)};
							if constexpr (line)
							{
								*i = lfch;
								++i;
							}
							::fast_io::operations::decay::write_all_decay(outstm, buffer, i);
						}
						else
						{
							::fast_io::details::local_operator_new_array_ptr<char_type> newptr(size);
							char_type *i{
								print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, newptr.ptr, t)};
							if constexpr (line)
							{
								*i = lfch;
								++i;
							}
							::fast_io::operations::decay::write_all_decay(outstm, newptr.ptr, i);
						}
					}
				}
			}
			else
			{
				if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<size, char_type>)
				{
					char_type buffer[size];
					char_type *i{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, buffer, t)};
					if constexpr (line)
					{
						*i = lfch;
						++i;
					}
					::fast_io::operations::decay::write_all_decay(outstm, buffer, i);
				}
				else
				{
					::fast_io::details::local_operator_new_array_ptr<char_type> newptr(size);
					char_type *i{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, newptr.ptr, t)};
					if constexpr (line)
					{
						*i = lfch;
						++i;
					}
					::fast_io::operations::decay::write_all_decay(outstm, newptr.ptr, i);
				}
			}
		}
	}
	else if constexpr (dynamic_reserve_printable<char_type, value_type>)
	{
		::std::size_t size{print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>, t)};
		constexpr ::std::size_t stack_buffer_size{
			::fast_io::details::decay::dynamic_print_reserve_static_stack_size<line, char_type, value_type>()};
		if constexpr (line)
		{
			constexpr ::std::size_t mx{::std::numeric_limits<::std::ptrdiff_t>::max() - 1};
			if (size >= mx)
			{
				fast_terminate();
			}
			++size;
		}
		else
		{
			constexpr ::std::size_t mx{::std::numeric_limits<::std::ptrdiff_t>::max()};
			if (mx < size)
			{
				fast_terminate();
			}
		}
#if 0
		if constexpr(contiguous_output_stream<output>)
		{
			auto bcurr{obuffer_curr(outstm)};
			auto bend{obuffer_end(outstm)};
			auto it{print_reserve_define(::fast_io::io_reserve_type<char_type,value_type>,bcurr,t,size)};
			::std::size_t diff{static_cast<::std::size_t>(bend-bcurr)};
			if(diff<size)[[unlikely]]
				fast_terminate();
			if constexpr(line)
			{
				*it=lfch;
				++it;
			}
			obuffer_set_curr(outstm,it);
		}
		else
#endif
		{
			if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<output> &&
						  !asan_activated)
			{
				auto curr{obuffer_curr(outstm)};
				auto ed{obuffer_end(outstm)};
				::std::ptrdiff_t diff(ed - curr);
				bool smaller{static_cast<::std::ptrdiff_t>(size) < diff};
				if (smaller)
				{
					auto it{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, curr, t)};
					if constexpr (line)
					{
						*it = lfch;
						++it;
					}
					obuffer_set_curr(outstm, it);
				}
				else
#if __has_cpp_attribute(unlikely)
					[[unlikely]]
#endif
				{
					if constexpr (stack_buffer_size != 0)
					{
						if (size <= stack_buffer_size)
						{
							char_type stack_buffer[stack_buffer_size];
							auto it{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>,
														 stack_buffer, t)};
							if constexpr (line)
							{
								*it = lfch;
								++it;
							}
							::fast_io::operations::decay::write_all_decay(outstm, stack_buffer, it);
						}
						else
						{
							::fast_io::details::local_operator_new_array_ptr<char_type> newptr(size);
							auto it{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>,
														 newptr.ptr, t)};
							if constexpr (line)
							{
								*it = lfch;
								++it;
							}
							::fast_io::operations::decay::write_all_decay(outstm, newptr.ptr, it);
						}
					}
					else
					{
						::fast_io::details::local_operator_new_array_ptr<char_type> newptr(size);
						auto it{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, newptr.ptr,
													 t)};
						if constexpr (line)
						{
							*it = lfch;
							++it;
						}
						::fast_io::operations::decay::write_all_decay(outstm, newptr.ptr, it);
					}
				}
			}
			else
			{
				if constexpr (stack_buffer_size != 0)
				{
					if (size <= stack_buffer_size)
					{
						char_type buffer[stack_buffer_size];
						auto it{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, buffer, t)};
						if constexpr (line)
						{
							*it = lfch;
							++it;
						}
						::fast_io::operations::decay::write_all_decay(outstm, buffer, it);
					}
					else
					{
						::fast_io::details::local_operator_new_array_ptr<char_type> newptr(size);
						auto it{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, newptr.ptr, t)};
						if constexpr (line)
						{
							*it = lfch;
							++it;
						}
						::fast_io::operations::decay::write_all_decay(outstm, newptr.ptr, it);
					}
				}
				else
				{
					::fast_io::details::local_operator_new_array_ptr<char_type> newptr(size);
					auto it{print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, newptr.ptr, t)};
					if constexpr (line)
					{
						*it = lfch;
						++it;
					}
					::fast_io::operations::decay::write_all_decay(outstm, newptr.ptr, it);
				}
			}
		}
	}
		else if constexpr (reserve_scatters_printable<char_type, value_type>)
		{
			::fast_io::details::decay::print_control_single_reserve_scatters<line, output, char_type>(outstm, t);
		}
		else if constexpr (::fast_io::transcode_imaginary_printable<char_type, value_type>)
		{
			// todo?
		}
	else if constexpr (context_printable<char_type, value_type>)
	{
		typename ::std::remove_cvref_t<decltype(print_context_type(io_reserve_type<char_type, value_type>))>::type st;
		constexpr ::std::size_t reserved_size{
			::fast_io::details::decay::context_print_static_buffer_size_v<line, char_type, value_type>};
		constexpr ::std::ptrdiff_t reserved_size_no_line{
			static_cast<::std::ptrdiff_t>(reserved_size - static_cast<::std::size_t>(line))};
		if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<output>)
		{
			for (;;)
			{
				auto bcurr{obuffer_curr(outstm)};
				auto bed{obuffer_end(outstm)};
				if (bed <= bcurr)
#if __has_cpp_attribute(unlikely)
					[[unlikely]]
#endif
				{
					if constexpr (minimum_buffer_output_stream_require_size_impl<output, reserved_size>)
					{
						obuffer_minimum_size_flush_prepare_define(outstm);
						bcurr = obuffer_curr(outstm);
						bed = obuffer_end(outstm);
					}
					else
					{
						::fast_io::operations::decay::output_stream_buffer_flush_decay(outstm);
						bcurr = obuffer_curr(outstm);
						bed = obuffer_end(outstm);
						if (bed - bcurr < reserved_size_no_line)
#if __has_cpp_attribute(unlikely)
							[[unlikely]]
#endif
							{
								if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<
												  reserved_size, char_type>)
								{
									char_type buffer[reserved_size];
									char_type *buffered{buffer + reserved_size_no_line};
									::fast_io::details::decay::print_context_define_to_window<line>(
										outstm, st, t, buffer, buffered);
								}
								else
								{
									::fast_io::details::local_operator_new_array_ptr<char_type> newptr(reserved_size);
									char_type *buffer{newptr.ptr};
									char_type *buffered{buffer + reserved_size_no_line};
									::fast_io::details::decay::print_context_define_to_window<line>(
										outstm, st, t, buffer, buffered);
								}
								return;
							}
					}
				}
				else
				{
					auto [resit, done] = st.print_context_define(t, bcurr, bed);
					obuffer_set_curr(outstm, resit);
					if (done)
#if __has_cpp_attribute(likely)
						[[likely]]
#endif
					{
						if constexpr (line)
						{
							::fast_io::operations::decay::char_put_decay(outstm, lfch);
						}
						return;
					}
				}
			}
		}
		else
		{
			if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<reserved_size, char_type>)
			{
				char_type buffer[reserved_size];
				char_type *buffered{buffer + reserved_size_no_line};
				::fast_io::details::decay::print_context_define_to_window<line>(outstm, st, t, buffer, buffered);
			}
			else
			{
				::fast_io::details::local_operator_new_array_ptr<char_type> newptr(reserved_size);
				char_type *buffer{newptr.ptr};
				char_type *buffered{buffer + reserved_size_no_line};
				::fast_io::details::decay::print_context_define_to_window<line>(outstm, st, t, buffer, buffered);
			}
			return;
		}
	}
	else if constexpr (printable<char_type, value_type>)
	{
		print_define(::fast_io::io_reserve_type<char_type, value_type>, outstm, t);
		if constexpr (line)
		{
			::fast_io::operations::decay::char_put_decay(outstm, lfch);
		}
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<value_type>, ::fast_io::io_null_t>)
	{
	}
	else
	{
		constexpr bool no{printable<char_type, value_type>};
		static_assert(no, "type not printable");
	}
}

struct context_capture_run_result
{
	::std::size_t position{};
	::std::size_t context_buffer_size{};
	::std::size_t dynamic_buffer_size{};
	::std::size_t max_static_reserve_burst_size{};
	::std::size_t leading_static_reserve_burst_size{};
	bool has_context{};
	bool has_dynamic{};
};

template <::std::integral char_type, typename Arg, typename... Args>
inline constexpr context_capture_run_result find_context_capture_run_n()
{
	using nocvreft = ::std::remove_cvref_t<Arg>;
	if constexpr (::fast_io::reserve_printable<char_type, nocvreft>)
	{
		context_capture_run_result ret{};
		if constexpr (sizeof...(Args) != 0)
		{
			ret = ::fast_io::details::decay::find_context_capture_run_n<char_type, Args...>();
		}
		constexpr ::std::size_t sz{print_reserve_size(::fast_io::io_reserve_type<char_type, nocvreft>)};
		static_assert(sz != 0);
		++ret.position;
		ret.leading_static_reserve_burst_size =
			::fast_io::details::intrinsics::add_or_overflow_die_chain(ret.leading_static_reserve_burst_size, sz);
		if (ret.max_static_reserve_burst_size < ret.leading_static_reserve_burst_size)
		{
			ret.max_static_reserve_burst_size = ret.leading_static_reserve_burst_size;
		}
		return ret;
	}
	else if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, nocvreft>)
	{
		context_capture_run_result ret{};
		if constexpr (sizeof...(Args) != 0)
		{
			ret = ::fast_io::details::decay::find_context_capture_run_n<char_type, Args...>();
		}
		constexpr ::std::size_t static_stack_size{
			print_reserve_static_stack_size(::fast_io::io_reserve_type<char_type, nocvreft>)};
		constexpr ::std::size_t dynamic_buffer_size{
			::fast_io::details::decay::dynamic_print_reserve_static_stack_budget<static_stack_size, char_type>()};
		++ret.position;
		ret.leading_static_reserve_burst_size = 0;
		ret.has_dynamic = true;
		if (ret.dynamic_buffer_size < dynamic_buffer_size)
		{
			ret.dynamic_buffer_size = dynamic_buffer_size;
		}
		return ret;
	}
	else if constexpr (::fast_io::context_printable_with_static_buffer_size<char_type, nocvreft>)
	{
		context_capture_run_result ret{};
		if constexpr (sizeof...(Args) != 0)
		{
			ret = ::fast_io::details::decay::find_context_capture_run_n<char_type, Args...>();
		}
		constexpr ::std::size_t context_buffer_size{
			::fast_io::details::decay::context_print_static_buffer_size_v<false, char_type, nocvreft>};
		++ret.position;
		ret.leading_static_reserve_burst_size = 0;
		ret.has_context = true;
		if (ret.context_buffer_size < context_buffer_size)
		{
			ret.context_buffer_size = context_buffer_size;
		}
		return ret;
	}
	else if constexpr (::std::same_as<nocvreft, ::fast_io::io_null_t>)
	{
		context_capture_run_result ret{};
		if constexpr (sizeof...(Args) != 0)
		{
			ret = ::fast_io::details::decay::find_context_capture_run_n<char_type, Args...>();
		}
		++ret.position;
		return ret;
	}
	else
	{
		return {};
	}
}

template <typename output, ::std::integral char_type>
inline constexpr void context_capture_flush(output outstm, char_type *buffer, char_type *&curr)
{
	if (curr != buffer)
	{
		::fast_io::operations::decay::write_all_decay(outstm, buffer, curr);
		curr = buffer;
	}
}

template <bool needprintlf, ::std::size_t n, typename output, ::std::integral char_type, typename T,
		  typename... Args>
inline constexpr void print_context_capture_run(output outstm, char_type *buffer, char_type *curr, char_type *end,
												T t, Args... args)
{
	if constexpr (n != 0)
	{
		using value_type = ::std::remove_cvref_t<T>;
		if constexpr (::fast_io::reserve_printable<char_type, value_type>)
		{
			constexpr ::std::size_t sz{print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>)};
			static_assert(sz < PTRDIFF_MAX);
			if (static_cast<::std::size_t>(end - curr) < sz)
			{
				::fast_io::details::decay::context_capture_flush(outstm, buffer, curr);
			}
			if (sz <= static_cast<::std::size_t>(end - buffer))
			{
				curr = print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, curr, t);
			}
			else
			{
				::fast_io::details::decay::print_control_single<false>(outstm, t);
			}
		}
		else if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, value_type>)
		{
			::std::size_t const sz{print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>, t)};
			if (static_cast<::std::size_t>(end - curr) < sz)
			{
				::fast_io::details::decay::context_capture_flush(outstm, buffer, curr);
			}
			if (sz <= static_cast<::std::size_t>(end - buffer))
			{
				curr = print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, curr, t);
			}
			else
			{
				::fast_io::details::decay::print_control_single<false>(outstm, t);
			}
		}
		else if constexpr (::fast_io::context_printable_with_static_buffer_size<char_type, value_type>)
		{
			typename ::std::remove_cvref_t<decltype(print_context_type(io_reserve_type<char_type, value_type>))>::type st;
			for (;;)
			{
				if (curr == end)
				{
					::fast_io::details::decay::context_capture_flush(outstm, buffer, curr);
				}
				auto [resit, done] = st.print_context_define(t, curr, end);
				curr = resit;
				if (done)
				{
					break;
				}
			}
		}
		else if constexpr (::std::same_as<value_type, ::fast_io::io_null_t>)
		{
		}

		if constexpr (n == 1)
		{
			if constexpr (needprintlf)
			{
				if (curr == end)
				{
					::fast_io::details::decay::context_capture_flush(outstm, buffer, curr);
				}
				*curr = ::fast_io::char_literal_v<u8'\n', char_type>;
				++curr;
			}
			::fast_io::details::decay::context_capture_flush(outstm, buffer, curr);
		}
		else
		{
			::fast_io::details::decay::print_context_capture_run<needprintlf, n - 1>(outstm, buffer, curr, end,
																					 args...);
		}
	}
}

#if 0
template<bool ln,::std::integral char_type,::std::size_t n,typename Arg,typename ...Args>
inline constexpr char_type* printrsvcontiguousimpl(char_type* iter,Arg arg,Args... args)
{
	if constexpr(sizeof...(Args)!=0)
	{
		ret = find_continuous_scatters_n<char_type,Args...>();
	}
	if constexpr(::fast_io::scatter_printable<char_type,Arg>)
	{
		return {ret.position+1,ret.neededspace,ret.hasdynamicreserve};
	}
	else if constexpr(::fast_io::reserve_printable<char_type,Arg>)
	{
		constexpr
			::std::size_t sz{print_reserve_size(::fast_io::io_reserve_type<char_type,Arg>)};
		return {ret.position+1,
			::fast_io::details::add_overflow(ret.neededspace,sz),
			ret.hasdynamicreserve};
	}
	else if constexpr(::fast_io::dynamic_reserve_printable<char_type,Arg>)
	{
		return {ret.position+1,ret.neededspace,true};
	}
}
#endif

template <bool ln, typename output, typename T, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_controls_line(output outstm, T t, Args... args)
{
	if constexpr (sizeof...(Args) == 0)
	{
		print_control_single<ln>(outstm, t);
	}
	else
	{
#if (defined(__OPTIMIZE__) || defined(__OPTIMIZE_SIZE__)) && 0
		print_controls_line_multi_impl<ln, 0>(outstm, t, args...);
#else
		if constexpr (ln)
		{
			print_control_single<false>(outstm, t);
			print_controls_line<ln>(outstm, args...);
		}
		else
		{
			print_control_single<false>(outstm, t);
			(print_control<false>(outstm, args), ...);
		}
#endif
	}
}

template <::std::size_t n, ::std::integral char_type, typename T, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type *print_n_reserve(char_type *ptr, T t, Args... args)
{
	if constexpr (n == 0)
	{
		return ptr;
	}
	else
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>)
		{
			if constexpr (sizeof...(Args) == 0 || n < 2)
			{
				return ptr;
			}
			else
			{
				return print_n_reserve<n - 1>(ptr, args...);
			}
		}
		else
		{
			ptr = print_reserve_define(::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>, ptr, t);
			if constexpr (sizeof...(Args) == 0 || n < 2)
			{
				return ptr;
			}
			else
			{
				return print_n_reserve<n - 1>(ptr, args...);
			}
		}
	}
}

template <::std::size_t n, ::std::integral char_type, typename scattertype, typename T, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_n_scatters(basic_io_scatter_t<scattertype> *pscatters,
#if __has_cpp_attribute(maybe_unused)
									   [[maybe_unused]]
#endif
									   T t,
#if __has_cpp_attribute(maybe_unused)
									   [[maybe_unused]]
#endif
									   Args... args)
{
	if constexpr (n != 0)
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>)
		{
			if constexpr (1 < n)
			{
				return print_n_scatters<n - 1, char_type>(pscatters, args...);
			}
		}
		else if constexpr (::std::same_as<::std::remove_cvref_t<T>, basic_io_scatter_t<scattertype>>)
		{
			if constexpr (::std::same_as<scattertype, void>)
			{
				*pscatters = io_scatter_t{t.base, t.len * sizeof(char_type)};
			}
			else
			{
				*pscatters = t;
			}
		}
		else
		{
			basic_io_scatter_t<char_type> sct{print_scatter_define(::fast_io::io_reserve_type<char_type, T>, t)};
			if constexpr (::std::same_as<scattertype, void>)
			{
				*pscatters = io_scatter_t{sct.base, sct.len * sizeof(char_type)};
			}
			else
			{
				*pscatters = sct;
			}
		}
		if constexpr (1 < n)
		{
			++pscatters;
			return print_n_scatters<n - 1, char_type>(pscatters, args...);
		}
	}
}

template <::std::size_t n, ::std::integral char_type, typename T, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::size_t ndynamic_print_reserve_size(T t, Args... args)
{
	using nocvreft = ::std::remove_cvref_t<T>;
	if constexpr (n == 0)
	{
		return 0;
	}
	else if constexpr (n == 1)
	{
		if constexpr (::fast_io::dynamic_reserve_printable<char_type, nocvreft>)
		{
			return print_reserve_size(::fast_io::io_reserve_type<char_type, nocvreft>, t);
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if constexpr (::fast_io::dynamic_reserve_printable<char_type, nocvreft>)
		{
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				print_reserve_size(::fast_io::io_reserve_type<char_type, nocvreft>, t),
				::fast_io::details::decay::ndynamic_print_reserve_size<n - 1, char_type>(args...));
		}
		else
		{
			return ::fast_io::details::decay::ndynamic_print_reserve_size<n - 1, char_type>(args...);
		}
	}
}

template <::std::size_t n, ::std::integral char_type, typename T, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::size_t ndynamic_print_reserve_static_stack_size()
{
	using nocvreft = ::std::remove_cvref_t<T>;
	if constexpr (n == 0)
	{
		return 0;
	}
	else if constexpr (n == 1)
	{
		if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, nocvreft>)
		{
			return print_reserve_static_stack_size(::fast_io::io_reserve_type<char_type, nocvreft>);
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if constexpr (::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, nocvreft>)
		{
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				print_reserve_static_stack_size(::fast_io::io_reserve_type<char_type, nocvreft>),
				::fast_io::details::decay::ndynamic_print_reserve_static_stack_size<n - 1, char_type, Args...>());
		}
		else
		{
			return ::fast_io::details::decay::ndynamic_print_reserve_static_stack_size<n - 1, char_type, Args...>();
		}
	}
}

template <::std::size_t n, ::std::integral char_type, typename T, typename... Args>
inline constexpr bool ndynamic_print_reserve_has_static_stack_size()
{
	using nocvreft = ::std::remove_cvref_t<T>;
	if constexpr (n == 0)
	{
		return true;
	}
	else if constexpr (::fast_io::dynamic_reserve_printable<char_type, nocvreft> &&
					   !::fast_io::dynamic_reserve_with_possible_static_stack_size<char_type, nocvreft>)
	{
		return false;
	}
	else if constexpr (n == 1)
	{
		return true;
	}
	else
	{
		return ::fast_io::details::decay::ndynamic_print_reserve_has_static_stack_size<n - 1, char_type, Args...>();
	}
}

template <bool needprintlf, ::std::size_t n, ::std::integral char_type, typename scattertype, typename T,
		  typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr auto print_n_scatters_reserve(basic_io_scatter_t<scattertype> *pscatters, char_type *ptr, T t,
											   Args... args);

template <::std::integral char_type, typename T, typename... Args>
inline constexpr bool print_next_is_reserve() noexcept
{
	using nocvreft = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::reserve_printable<char_type, nocvreft> ||
				  ::fast_io::dynamic_reserve_printable<char_type, nocvreft>)
	{
		return true;
	}
	else
	{
		return false;
	}
}

template <::std::integral char_type>
inline constexpr bool print_next_is_reserve() noexcept
{
	return false;
}

template <bool needprintlf, ::std::size_t n, ::std::integral char_type, typename scattertype, typename T,
		  typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr auto print_n_scatters_reserve_cont(basic_io_scatter_t<scattertype> *pscatters, char_type *base,
													char_type *ptr, T t, Args... args)
{
	if constexpr (n != 0)
	{
		using nocvreft = ::std::remove_cvref_t<T>;
		if constexpr (reserve_printable<char_type, nocvreft> || dynamic_reserve_printable<char_type, nocvreft>)
		{
			ptr = print_reserve_define(::fast_io::io_reserve_type<char_type, nocvreft>, ptr, t);
			if constexpr (::fast_io::details::decay::print_next_is_reserve<char_type, Args...>())
			{
				return ::fast_io::details::decay::print_n_scatters_reserve_cont<needprintlf, n - 1, char_type>(
					pscatters, base, ptr, args...);
			}
			else
			{
				if constexpr (n == 1 && needprintlf)
				{
					*ptr = char_literal_v<u8'\n', char_type>;
					++ptr;
				}
				::std::size_t const sz{static_cast<::std::size_t>(ptr - base)};
				if constexpr (::std::same_as<scattertype, void>)
				{
					*pscatters = io_scatter_t{base, sz * sizeof(char_type)};
				}
				else
				{
					*pscatters = basic_io_scatter_t<char_type>{base, sz};
				}
				++pscatters;
				if constexpr (1 < n)
				{
					return ::fast_io::details::decay::print_n_scatters_reserve<needprintlf, n - 1, char_type>(
						pscatters, ptr, args...);
				}
			}
		}
		else
		{
			::std::size_t const sz{static_cast<::std::size_t>(ptr - base)};
			if constexpr (::std::same_as<scattertype, void>)
			{
				*pscatters = io_scatter_t{base, sz * sizeof(char_type)};
			}
			else
			{
				*pscatters = basic_io_scatter_t<char_type>{base, sz};
			}
			++pscatters;
			if constexpr (1 < n)
			{
				return ::fast_io::details::decay::print_n_scatters_reserve<needprintlf, n - 1, char_type>(
					pscatters, ptr, t, args...);
			}
		}
	}
	return pscatters;
}

template <bool needprintlf, ::std::size_t n, ::std::integral char_type, typename scattertype, typename T,
		  typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr auto print_n_scatters_reserve(basic_io_scatter_t<scattertype> *pscatters, char_type *ptr, T t,
											   Args... args)
{
	if constexpr (n != 0)
	{
		using nocvreft = ::std::remove_cvref_t<T>;
		if constexpr (::fast_io::reserve_printable<char_type, nocvreft> ||
					  ::fast_io::dynamic_reserve_printable<char_type, nocvreft>)
		{
			auto ptred{print_reserve_define(::fast_io::io_reserve_type<char_type, nocvreft>, ptr, t)};
			if constexpr (sizeof...(Args) != 0 &&
						  ::fast_io::details::decay::print_next_is_reserve<char_type, Args...>())
			{
				return ::fast_io::details::decay::print_n_scatters_reserve_cont<needprintlf, n - 1, char_type>(
					pscatters, ptr, ptred, args...);
			}
			else
			{
				if constexpr (n == 1 && needprintlf)
				{
					*ptred = char_literal_v<u8'\n', char_type>;
					++ptred;
				}
				::std::size_t const sz{static_cast<::std::size_t>(ptred - ptr)};
				if constexpr (::std::same_as<scattertype, void>)
				{
					*pscatters = io_scatter_t{ptr, sz * sizeof(char_type)};
				}
				else
				{
					*pscatters = basic_io_scatter_t<char_type>{ptr, sz};
				}
				++pscatters;
				if constexpr (1 < n)
				{
					return ::fast_io::details::decay::print_n_scatters_reserve<needprintlf, n - 1, char_type>(
						pscatters, ptred, args...);
				}
			}
		}
		else if constexpr (::fast_io::scatter_printable<char_type, nocvreft>)
		{
			if constexpr (::std::same_as<nocvreft, basic_io_scatter_t<scattertype>>)
			{
				if constexpr (::std::same_as<scattertype, void>)
				{
					*pscatters = io_scatter_t{t.base, t.len * sizeof(char_type)};
				}
				else
				{
					*pscatters = t;
				}
			}
			else
			{
				basic_io_scatter_t<char_type> sct{print_scatter_define(::fast_io::io_reserve_type<char_type, T>, t)};
				if constexpr (::std::same_as<scattertype, void>)
				{
					*pscatters = io_scatter_t{sct.base, sct.len * sizeof(char_type)};
				}
				else
				{
					*pscatters = sct;
				}
			}
			++pscatters;
			if constexpr (n == 1 && needprintlf)
			{
				*pscatters = ::fast_io::details::decay::line_scatter_common<char_type, scattertype>;
				++pscatters;
			}
			if constexpr (1 < n)
			{
				return ::fast_io::details::decay::print_n_scatters_reserve<needprintlf, n - 1, char_type>(pscatters,
																										  ptr, args...);
			}
		}
		else if constexpr (::fast_io::reserve_scatters_printable<char_type, nocvreft>)
		{
			if constexpr (::std::same_as<scattertype, void>)
			{
				auto pit{::fast_io::details::decay::prrsvsct_byte_common_rsvsc_impl(pscatters, ptr, t)};
				if constexpr (1 < n)
				{
					return ::fast_io::details::decay::print_n_scatters_reserve<needprintlf, n - 1, char_type>(
						pit.scatters_pos_ptr, pit.reserve_pos_ptr, args...);
				}
				else if constexpr (n == 1 && needprintlf)
				{
					*pit.scatters_pos_ptr = ::fast_io::details::decay::line_scatter_common<char_type, scattertype>;
					++pit.scatters_pos_ptr;
				}
				return pit.scatters_pos_ptr;
			}
			else
			{
				auto pit{
					print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, nocvreft>, pscatters, ptr, t)};
				if constexpr (1 < n)
				{
					return ::fast_io::details::decay::print_n_scatters_reserve<needprintlf, n - 1, char_type>(
						pit.scatters_pos_ptr, pit.reserve_pos_ptr, args...);
				}
				else if constexpr (n == 1 && needprintlf)
				{
					*pit.scatters_pos_ptr = ::fast_io::details::decay::line_scatter_common<char_type, scattertype>;
					++pit.scatters_pos_ptr;
				}
				return pit.scatters_pos_ptr;
			}
		}
		else if constexpr (::std::same_as<nocvreft, ::fast_io::io_null_t>)
		{
			if constexpr (n == 1 && needprintlf)
			{
				*pscatters = ::fast_io::details::decay::line_scatter_common<char_type, scattertype>;
				++pscatters;
			}
			if constexpr (1 < n)
			{
				return ::fast_io::details::decay::print_n_scatters_reserve<needprintlf, n - 1, char_type>(pscatters,
																										  ptr, args...);
			}
		}
	}
	return pscatters;
}

template <bool needprintlf, ::std::size_t position, ::std::size_t scatterscount, ::std::integral char_type,
		  typename outputstmtype, typename T, typename... Args>
inline constexpr void print_controls_scatters(outputstmtype optstm, T t, Args... args)
{
	using scatter_type = ::std::conditional_t<
		::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<outputstmtype>,
		::fast_io::io_scatter_t, ::fast_io::basic_io_scatter_t<char_type>>;
	if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<scatterscount, scatter_type>)
	{
		scatter_type scatters[scatterscount];
		::fast_io::details::decay::print_n_scatters<position, char_type>(scatters, t, args...);
		::fast_io::details::decay::print_scatter_write_all_dispatch_with_line<needprintlf, outputstmtype, char_type>(
			optstm, scatters, scatterscount);
	}
	else
	{
		::fast_io::details::local_operator_new_array_ptr<scatter_type> scatters(scatterscount);
		::fast_io::details::decay::print_n_scatters<position, char_type>(scatters.ptr, t, args...);
		::fast_io::details::decay::print_scatter_write_all_dispatch_with_line<needprintlf, outputstmtype, char_type>(
			optstm, scatters.ptr, scatterscount);
	}
}

template <bool needprintlf, ::std::size_t position, ::std::size_t mxsize, ::std::integral char_type,
		  typename outputstmtype, typename scatter_type, typename T, typename... Args>
inline constexpr void print_controls_scatters_reserve_with_scatter(outputstmtype optstm, scatter_type *scatters, T t,
																   Args... args)
{
	constexpr ::std::size_t buffer_size{mxsize == 0 ? 1u : mxsize};
	if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<buffer_size, char_type>)
	{
		char_type buffer[buffer_size];
		auto ptr{::fast_io::details::decay::print_n_scatters_reserve<needprintlf, position, char_type>(scatters,
																									   buffer, t,
																									   args...)};
		::fast_io::details::decay::print_scatter_write_all_dispatch(
			optstm, scatters, static_cast<::std::size_t>(ptr - scatters));
	}
	else
	{
		::fast_io::details::local_operator_new_array_ptr<char_type> buffer(buffer_size);
		auto ptr{::fast_io::details::decay::print_n_scatters_reserve<needprintlf, position, char_type>(scatters,
																									   buffer.ptr, t,
																									   args...)};
		::fast_io::details::decay::print_scatter_write_all_dispatch(
			optstm, scatters, static_cast<::std::size_t>(ptr - scatters));
	}
}

template <bool needprintlf, ::std::size_t position, ::std::size_t scatterscount, ::std::size_t mxsize,
		  ::std::integral char_type, typename outputstmtype, typename T, typename... Args>
inline constexpr void print_controls_scatters_reserve(outputstmtype optstm, T t, Args... args)
{
	using scatter_type = ::std::conditional_t<
		::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<outputstmtype>,
		::fast_io::io_scatter_t, ::fast_io::basic_io_scatter_t<char_type>>;
	if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<scatterscount, scatter_type>)
	{
		scatter_type scatters[scatterscount];
		::fast_io::details::decay::print_controls_scatters_reserve_with_scatter<needprintlf, position, mxsize,
																				char_type>(optstm, scatters, t, args...);
	}
	else
	{
		::fast_io::details::local_operator_new_array_ptr<scatter_type> scatters(scatterscount);
		::fast_io::details::decay::print_controls_scatters_reserve_with_scatter<needprintlf, position, mxsize,
																				char_type>(optstm, scatters.ptr, t,
																							args...);
	}
}

template <bool needprintlf, ::std::size_t position, ::std::size_t stack_buffer_size, bool has_static_stack_size,
		  ::std::integral char_type, typename outputstmtype, typename scatter_type, typename T, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_controls_dynamic_scatters_reserve_with_scatter(outputstmtype optstm, scatter_type *scatters,
																		   ::std::size_t totalsz, T t, Args... args)
{
	if constexpr (has_static_stack_size &&
				  ::fast_io::details::decay::print_stack_buffer_size_within_limit<stack_buffer_size, char_type>)
	{
		if (totalsz <= stack_buffer_size)
		{
			char_type buffer[stack_buffer_size];
			auto ptr{::fast_io::details::decay::print_n_scatters_reserve<needprintlf, position, char_type>(scatters,
																										   buffer, t,
																										   args...)};
			::fast_io::details::decay::print_scatter_write_all_dispatch(
				optstm, scatters, static_cast<::std::size_t>(ptr - scatters));
			return;
		}
	}
	::fast_io::details::local_operator_new_array_ptr<char_type> buffer(totalsz == 0 ? 1u : totalsz);
	auto ptr{::fast_io::details::decay::print_n_scatters_reserve<needprintlf, position, char_type>(scatters, buffer.ptr,
																								   t, args...)};
	::fast_io::details::decay::print_scatter_write_all_dispatch(
		optstm, scatters, static_cast<::std::size_t>(ptr - scatters));
}

template <bool needprintlf, ::std::size_t position, ::std::size_t scatterscount, ::std::size_t stack_buffer_size,
		  bool has_static_stack_size, ::std::integral char_type, typename outputstmtype, typename T, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_controls_dynamic_scatters_reserve(outputstmtype optstm, ::std::size_t totalsz, T t,
															  Args... args)
{
	using scatter_type = ::std::conditional_t<
		::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<outputstmtype>,
		::fast_io::io_scatter_t, ::fast_io::basic_io_scatter_t<char_type>>;
	if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<scatterscount, scatter_type>)
	{
		scatter_type scatters[scatterscount];
		::fast_io::details::decay::print_controls_dynamic_scatters_reserve_with_scatter<
			needprintlf, position, stack_buffer_size, has_static_stack_size, char_type>(optstm, scatters, totalsz, t,
																					   args...);
	}
	else
	{
		::fast_io::details::local_operator_new_array_ptr<scatter_type> scatters(scatterscount);
		::fast_io::details::decay::print_controls_dynamic_scatters_reserve_with_scatter<
			needprintlf, position, stack_buffer_size, has_static_stack_size, char_type>(optstm, scatters.ptr, totalsz, t,
																					   args...);
	}
}

template <bool line, typename outputstmtype, ::std::size_t skippings, typename T, typename... Args>
inline constexpr void print_controls_impl(outputstmtype optstm, T t, Args... args);

template <::std::integral char_type>
inline constexpr bool print_controls_dynamic_scatters_reserve_fast_entry_available() noexcept
{
	return false;
}

template <::std::integral char_type, typename T, typename... Args>
inline constexpr bool print_controls_dynamic_scatters_reserve_fast_entry_available() noexcept
{
	constexpr contiguous_scatter_result res{
		::fast_io::details::decay::find_continuous_scatters_n<char_type, T, Args...>()};
	return res.position != 0 && res.hasscatters && res.hasdynamicreserve;
}

template <bool line, typename outputstmtype, typename T, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_controls_dynamic_scatters_reserve_fast_entry(outputstmtype optstm, T t, Args... args)
{
	using char_type = typename outputstmtype::output_char_type;
	constexpr contiguous_scatter_result res{
		::fast_io::details::decay::find_continuous_scatters_n<char_type, T, Args...>()};
	static_assert(res.position != 0);
	static_assert(res.hasscatters && res.hasdynamicreserve);
	if constexpr (line)
	{
		static_assert(res.neededscatters != SIZE_MAX);
	}
	static_assert(SIZE_MAX != sizeof...(Args));
	constexpr ::std::size_t n{sizeof...(Args) + static_cast<::std::size_t>(1)};
	constexpr bool needprintlf{n == res.position && line};
	constexpr ::std::size_t mxsize{
		static_cast<::std::size_t>(res.neededspace + static_cast<::std::size_t>(needprintlf))};
	constexpr ::std::size_t scatterscount{res.neededscatters +
										  static_cast<::std::size_t>(line && res.position == n)};
	constexpr bool has_static_stack_size{
		::fast_io::details::decay::ndynamic_print_reserve_has_static_stack_size<res.position, char_type, T,
																				 Args...>()};
	constexpr ::std::size_t producer_static_stack_size{
		::fast_io::details::decay::ndynamic_print_reserve_static_stack_size<res.position, char_type, T, Args...>()};
	constexpr ::std::size_t dynamic_stack_budget{
		::fast_io::details::decay::dynamic_print_reserve_static_stack_budget<producer_static_stack_size, char_type>()};
	constexpr ::std::size_t stack_buffer_size{
		::fast_io::details::intrinsics::add_or_overflow_die(mxsize, dynamic_stack_budget)};
	::std::size_t dynsz{::fast_io::details::decay::ndynamic_print_reserve_size<res.position, char_type>(t, args...)};
	::std::size_t totalsz{::fast_io::details::intrinsics::add_or_overflow_die(mxsize, dynsz)};
	::fast_io::details::decay::print_controls_dynamic_scatters_reserve<
		needprintlf, res.position, scatterscount, stack_buffer_size, has_static_stack_size, char_type>(optstm, totalsz,
																									  t, args...);
	if constexpr (res.position != n)
	{
		print_controls_impl<line, outputstmtype, res.position - 1>(optstm, args...);
	}
}

template <bool line, typename outputstmtype, ::std::size_t skippings = 0, typename T, typename... Args>
inline constexpr void print_controls_impl(outputstmtype optstm, T t, Args... args)
{
	using char_type = typename outputstmtype::output_char_type;
	using scatter_type = ::std::conditional_t<
		::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<outputstmtype>,
		io_scatter_t, basic_io_scatter_t<char_type>>;
	constexpr contiguous_scatter_result res{
		::fast_io::details::decay::find_continuous_scatters_n<char_type, T, Args...>()};
	constexpr context_capture_run_result context_capture_res{
		::fast_io::details::decay::find_context_capture_run_n<char_type, T, Args...>()};
	if constexpr (skippings != 0)
	{
		print_controls_impl<line, outputstmtype, skippings - 1>(optstm, args...);
	}
	else if constexpr (sizeof...(Args) == 0)
	{
		print_control_single<line>(optstm, t);
	}
	else if constexpr (context_capture_res.has_context)
	{
		static_assert(SIZE_MAX != sizeof...(Args));
		constexpr ::std::size_t n{sizeof...(Args) + static_cast<::std::size_t>(1)};
		constexpr bool needprintlf{n == context_capture_res.position && line};
		/*
		Context and dynamic producer sizes are reusable streaming windows.
		Only consecutive static reserve producers contribute a burst size, so
		the final capture buffer uses max semantics instead of summing producer
		windows across the run.
		*/
		constexpr ::std::size_t context_dynamic_size{
			context_capture_res.context_buffer_size < context_capture_res.dynamic_buffer_size
				? context_capture_res.dynamic_buffer_size
				: context_capture_res.context_buffer_size};
		constexpr ::std::size_t reserve_context_dynamic_size{
			context_dynamic_size < context_capture_res.max_static_reserve_burst_size
				? context_capture_res.max_static_reserve_burst_size
				: context_dynamic_size};
		constexpr ::std::size_t buffer_size{reserve_context_dynamic_size < 32u ? 32u
																			   : reserve_context_dynamic_size};
		if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<buffer_size, char_type>)
		{
			char_type buffer[buffer_size];
			::fast_io::details::decay::print_context_capture_run<needprintlf, context_capture_res.position>(
				optstm, buffer, buffer, buffer + buffer_size, t, args...);
		}
		else
		{
			::fast_io::details::local_operator_new_array_ptr<char_type> newptr(buffer_size);
			char_type *buffer{newptr.ptr};
			::fast_io::details::decay::print_context_capture_run<needprintlf, context_capture_res.position>(
				optstm, buffer, buffer, buffer + buffer_size, t, args...);
		}
		if constexpr (context_capture_res.position != n)
		{
			print_controls_impl<line, outputstmtype, context_capture_res.position - 1>(optstm, args...);
		}
	}
	else if constexpr (res.position == 0)
	{
		print_control_single<false>(optstm, t);
		print_controls_impl<line>(optstm, args...);
	}
	else
	{
		if constexpr (line)
		{
			static_assert(res.neededscatters != SIZE_MAX);
		}
		static_assert(SIZE_MAX != sizeof...(Args));
		constexpr ::std::size_t n{sizeof...(Args) + static_cast<::std::size_t>(1)};
		constexpr bool needprintlf{n == res.position && line};
		if constexpr (res.hasscatters && !res.hasreserve && !res.hasdynamicreserve)
		{
			constexpr ::std::size_t scatterscount{res.neededscatters + static_cast<::std::size_t>(needprintlf)};
			::fast_io::details::decay::print_controls_scatters<needprintlf, res.position, scatterscount, char_type>(
				optstm, t, args...);
			if constexpr (res.position != n)
			{
				print_controls_impl<line, outputstmtype, res.position - 1>(optstm, args...);
			}
		}
		else
		{
			constexpr ::std::size_t mxsize{
				static_cast<::std::size_t>(res.neededspace + static_cast<::std::size_t>(needprintlf))};
			if constexpr (!res.hasscatters)
			{
				static_assert(!needprintlf || res.neededspace != SIZE_MAX);
				if constexpr (res.hasdynamicreserve)
				{
					constexpr bool has_static_stack_size{
						::fast_io::details::decay::ndynamic_print_reserve_has_static_stack_size<res.position,
																								 char_type, T,
																								 Args...>()};
					constexpr ::std::size_t producer_static_stack_size{
						::fast_io::details::decay::ndynamic_print_reserve_static_stack_size<res.position, char_type,
																								   T, Args...>()};
					constexpr ::std::size_t dynamic_stack_budget{
						::fast_io::details::decay::dynamic_print_reserve_static_stack_budget<producer_static_stack_size,
																							char_type>()};
					constexpr ::std::size_t stack_buffer_size{
						::fast_io::details::intrinsics::add_or_overflow_die(mxsize, dynamic_stack_budget)};
					::std::size_t dynsz{
						::fast_io::details::decay::ndynamic_print_reserve_size<res.position, char_type>(t, args...)};
					::std::size_t totalsz{::fast_io::details::intrinsics::add_or_overflow_die(mxsize, dynsz)};
					if constexpr (has_static_stack_size &&
								  ::fast_io::details::decay::print_stack_buffer_size_within_limit<stack_buffer_size,
																								 char_type>)
					{
						if (totalsz <= stack_buffer_size)
						{
							char_type buffer[stack_buffer_size];
							char_type *ptred{
								::fast_io::details::decay::print_n_reserve<res.position, char_type>(buffer, t,
																									args...)};
							if constexpr (needprintlf)
							{
								*ptred = ::fast_io::char_literal_v<u8'\n', char_type>;
								++ptred;
							}
							::fast_io::operations::decay::write_all_decay(optstm, buffer, ptred);
						}
						else
						{
							::fast_io::details::local_operator_new_array_ptr<char_type> newptr(totalsz);
							char_type *buffer{newptr.ptr};
							char_type *ptred{
								::fast_io::details::decay::print_n_reserve<res.position, char_type>(buffer, t,
																									args...)};
							if constexpr (needprintlf)
							{
								*ptred = ::fast_io::char_literal_v<u8'\n', char_type>;
								++ptred;
							}
							::fast_io::operations::decay::write_all_decay(optstm, buffer, ptred);
						}
					}
					else
					{
						::fast_io::details::local_operator_new_array_ptr<char_type> newptr(totalsz);
						char_type *buffer{newptr.ptr};
						char_type *ptred{
							::fast_io::details::decay::print_n_reserve<res.position, char_type>(buffer, t, args...)};
						if constexpr (needprintlf)
						{
							*ptred = ::fast_io::char_literal_v<u8'\n', char_type>;
							++ptred;
						}
						::fast_io::operations::decay::write_all_decay(optstm, buffer, ptred);
					}
				}
				else if constexpr (res.hasreserve)
				{
					if constexpr (res.neededspace == 0)
					{
						if constexpr (needprintlf)
						{
							::fast_io::operations::decay::char_put_decay(optstm,
																		 ::fast_io::char_literal_v<u8'\n', char_type>);
						}
					}
					else
					{
						if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<mxsize,
																									   char_type>)
						{
							char_type buffer[mxsize];
							char_type *ptred{
								::fast_io::details::decay::print_n_reserve<res.position, char_type>(buffer, t, args...)};
							if constexpr (needprintlf)
							{
								*ptred = ::fast_io::char_literal_v<u8'\n', char_type>;
								++ptred;
							}
							::fast_io::operations::decay::write_all_decay(optstm, buffer, ptred);
						}
						else
						{
							::fast_io::details::local_operator_new_array_ptr<char_type> newptr(mxsize);
							char_type *buffer{newptr.ptr};
							char_type *ptred{
								::fast_io::details::decay::print_n_reserve<res.position, char_type>(buffer, t, args...)};
							if constexpr (needprintlf)
							{
								*ptred = ::fast_io::char_literal_v<u8'\n', char_type>;
								++ptred;
							}
							::fast_io::operations::decay::write_all_decay(optstm, buffer, ptred);
							}
						}
					}
				}
			else if constexpr (res.hasreserve && !res.hasdynamicreserve)
			{
				constexpr ::std::size_t scatterscount{res.neededscatters +
													  static_cast<::std::size_t>(line && res.position == n)};
				::fast_io::details::decay::print_controls_scatters_reserve<needprintlf, res.position, scatterscount,
																			mxsize, char_type>(optstm, t, args...);
			}
			else if constexpr (res.hasdynamicreserve)
			{
				constexpr ::std::size_t scatterscount{res.neededscatters +
													  static_cast<::std::size_t>(line && res.position == n)};
				constexpr bool has_static_stack_size{
					::fast_io::details::decay::ndynamic_print_reserve_has_static_stack_size<res.position, char_type, T,
																							 Args...>()};
				constexpr ::std::size_t producer_static_stack_size{
					::fast_io::details::decay::ndynamic_print_reserve_static_stack_size<res.position, char_type, T,
																					   Args...>()};
				constexpr ::std::size_t dynamic_stack_budget{
					::fast_io::details::decay::dynamic_print_reserve_static_stack_budget<producer_static_stack_size,
																						char_type>()};
				constexpr ::std::size_t stack_buffer_size{
					::fast_io::details::intrinsics::add_or_overflow_die(mxsize, dynamic_stack_budget)};
				::std::size_t dynsz{
					::fast_io::details::decay::ndynamic_print_reserve_size<res.position, char_type>(t, args...)};
				::std::size_t totalsz{::fast_io::details::intrinsics::add_or_overflow_die(mxsize, dynsz)};
				::fast_io::details::decay::print_controls_dynamic_scatters_reserve<
					needprintlf, res.position, scatterscount, stack_buffer_size, has_static_stack_size, char_type>(
					optstm, totalsz, t, args...);
			}
			if constexpr (res.position != n)
			{
				print_controls_impl<line, outputstmtype, res.position - 1>(optstm, args...);
			}
		}
	}
}

template <bool line, typename outputstmtype, ::std::size_t skippings = 0, typename T, typename... Args>
inline constexpr void print_controls_buffer_impl(outputstmtype optstm, T t, Args... args)
{
	if constexpr (skippings != 0)
	{
		::fast_io::details::decay::print_controls_buffer_impl<line, outputstmtype, skippings - 1>(optstm, args...);
	}
	else if constexpr (sizeof...(Args) == 0)
	{
		print_control_single<line>(optstm, t);
	}
	else
	{
		using char_type = typename outputstmtype::output_char_type;
		static_assert(SIZE_MAX != sizeof...(Args));
		constexpr ::std::size_t n{sizeof...(Args) + static_cast<::std::size_t>(1)};
		constexpr auto scatters_result{
			::fast_io::details::decay::find_continuous_scatters_reserve_n<true, char_type, T, Args...>()};
		using scatter_type = ::std::conditional_t<
			::fast_io::operations::decay::defines::has_any_of_write_or_seek_pwrite_bytes_operations<outputstmtype>,
			io_scatter_t, basic_io_scatter_t<char_type>>;
			if constexpr (scatters_result.position != 0)
			{
				if constexpr (line)
				{
					static_assert(scatters_result.position != SIZE_MAX);
				}
				constexpr bool needprintlf{n == scatters_result.position && line};
				constexpr ::std::size_t scatterscount{scatters_result.position - scatters_result.null +
													  static_cast<::std::size_t>(needprintlf)};
				::fast_io::details::decay::print_controls_scatters<needprintlf, scatters_result.position,
																	scatterscount, char_type>(optstm, t, args...);
				if constexpr (scatters_result.position != n)
				{
					::fast_io::details::decay::print_controls_buffer_impl<line, outputstmtype,
																		  scatters_result.position - 1>(optstm, args...);
				}
			}
		else
		{
			constexpr auto rsvresult{
				::fast_io::details::decay::find_continuous_scatters_reserve_n<false, char_type, T, Args...>()};
			if constexpr (1 < rsvresult.position)
			{
				constexpr bool needprintlf{n == rsvresult.position && line};
				constexpr ::std::size_t buffersize{rsvresult.neededspace + static_cast<::std::size_t>(needprintlf)};
				char_type *bcurr{obuffer_curr(optstm)};
				char_type *bend{obuffer_end(optstm)};
				::std::ptrdiff_t const diff(bend - bcurr);
				bool smaller{static_cast<::std::ptrdiff_t>(buffersize) < diff};
				if constexpr (minimum_buffer_output_stream_require_size_impl<outputstmtype, buffersize>)
				{
					if (!smaller) [[unlikely]]
					{
						obuffer_minimum_size_flush_prepare_define(optstm);
						bcurr = obuffer_curr(optstm);
					}
					bcurr =
						::fast_io::details::decay::print_n_reserve<rsvresult.position, char_type>(bcurr, t, args...);
					if constexpr (needprintlf)
					{
						*bcurr = ::fast_io::char_literal_v<u8'\n', char_type>;
						++bcurr;
					}
					obuffer_set_curr(optstm, bcurr);
					}
					else
					{
						if (smaller) [[likely]]
						{
							bcurr =
								::fast_io::details::decay::print_n_reserve<rsvresult.position, char_type>(bcurr, t,
																										args...);
							if constexpr (needprintlf)
							{
								*bcurr = ::fast_io::char_literal_v<u8'\n', char_type>;
								++bcurr;
							}
							obuffer_set_curr(optstm, bcurr);
						}
						else [[unlikely]]
						{
							if constexpr (::fast_io::details::decay::print_stack_buffer_size_within_limit<buffersize,
																										   char_type>)
							{
								char_type buffer[buffersize];
								char_type *ptr{::fast_io::details::decay::print_n_reserve<rsvresult.position,
																						  char_type>(buffer, t,
																									 args...)};
								if constexpr (needprintlf)
								{
									*ptr = ::fast_io::char_literal_v<u8'\n', char_type>;
									++ptr;
								}
								::fast_io::operations::decay::write_all_decay(optstm, buffer, ptr);
							}
							else
							{
								::fast_io::details::local_operator_new_array_ptr<char_type> newptr(buffersize);
								char_type *buffer{newptr.ptr};
								char_type *ptr{::fast_io::details::decay::print_n_reserve<rsvresult.position,
																						  char_type>(buffer, t,
																									 args...)};
								if constexpr (needprintlf)
								{
									*ptr = ::fast_io::char_literal_v<u8'\n', char_type>;
									++ptr;
								}
								::fast_io::operations::decay::write_all_decay(optstm, buffer, ptr);
							}
						}
					}
				if constexpr (rsvresult.position != n)
				{
					::fast_io::details::decay::print_controls_buffer_impl<line, outputstmtype, rsvresult.position - 1>(
						optstm, args...);
				}
			}
			else
			{
				::fast_io::details::decay::print_control_single<line && sizeof...(args) == 0>(optstm, t);
				if constexpr (sizeof...(args) != 0)
				{
					::fast_io::details::decay::print_controls_buffer_impl<line>(optstm, args...);
				}
			}
		}
	}
}

template <typename T>
inline constexpr bool print_semantic_condition_v = false;

template <typename T1, typename T2>
inline constexpr bool
	print_semantic_condition_v<::fast_io::manipulators::condition<T1, T2>> = true;

template <typename T>
inline constexpr bool print_semantic_width_v = false;

template <::fast_io::manipulators::scalar_placement placement, typename T>
inline constexpr bool print_semantic_width_v<::fast_io::manipulators::width_t<placement, T>> = true;

template <::fast_io::manipulators::scalar_placement placement, typename T, ::std::integral ch_type>
inline constexpr bool
	print_semantic_width_v<::fast_io::manipulators::width_ch_t<placement, T, ch_type>> = true;

template <typename T>
inline constexpr bool print_semantic_width_v<::fast_io::manipulators::width_runtime_t<T>> = true;

template <typename T, ::std::integral ch_type>
inline constexpr bool print_semantic_width_v<::fast_io::manipulators::width_runtime_ch_t<T, ch_type>> = true;

template <typename T>
inline constexpr bool print_semantic_node_no_parameter_v =
	::fast_io::details::print_pack<T> ||
	::fast_io::details::decay::print_semantic_condition_v<::std::remove_cvref_t<T>> ||
	::fast_io::details::decay::print_semantic_width_v<::std::remove_cvref_t<T>>;

template <typename T>
inline constexpr bool print_semantic_parameter_object_v = false;

template <typename T>
inline constexpr bool print_semantic_parameter_object_v<::fast_io::parameter<T>> = true;

template <typename T>
inline constexpr bool print_semantic_parameter_v = false;

template <typename T>
inline constexpr bool print_semantic_parameter_v<::fast_io::parameter<T>> =
	::fast_io::details::decay::print_semantic_node_no_parameter_v<::std::remove_cvref_t<T>>;

template <typename T>
concept print_semantic_node =
	::fast_io::details::decay::print_semantic_node_no_parameter_v<::std::remove_cvref_t<T>> ||
	::fast_io::details::decay::print_semantic_parameter_v<::std::remove_cvref_t<T>>;

template <typename T>
struct print_semantic_width_traits
{};

template <::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_width_traits<::fast_io::manipulators::width_t<placement, T>>
{
	inline static constexpr bool runtime_placement = false;
	inline static constexpr ::fast_io::manipulators::scalar_placement static_placement = placement;
	inline static constexpr bool has_fill_char = false;
};

template <::fast_io::manipulators::scalar_placement placement, typename T, ::std::integral ch_type>
struct print_semantic_width_traits<::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
{
	using fill_char_type = ch_type;
	inline static constexpr bool runtime_placement = false;
	inline static constexpr ::fast_io::manipulators::scalar_placement static_placement = placement;
	inline static constexpr bool has_fill_char = true;
};

template <typename T>
struct print_semantic_width_traits<::fast_io::manipulators::width_runtime_t<T>>
{
	inline static constexpr bool runtime_placement = true;
	inline static constexpr bool has_fill_char = false;
};

template <typename T, ::std::integral ch_type>
struct print_semantic_width_traits<::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
{
	using fill_char_type = ch_type;
	inline static constexpr bool runtime_placement = true;
	inline static constexpr bool has_fill_char = true;
};

template <typename T>
inline constexpr decltype(auto) print_semantic_node_ref(T &&t) noexcept
{
	if constexpr (::fast_io::details::decay::print_semantic_parameter_object_v<::std::remove_cvref_t<T>>)
	{
		return ::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t).reference);
	}
	else
	{
		return ::std::forward<T>(t);
	}
}

template <typename T>
inline constexpr bool print_semantic_pack_argument_v =
	::fast_io::details::print_pack<T> ||
	(::fast_io::details::decay::print_semantic_parameter_object_v<::std::remove_cvref_t<T>> &&
	 ::fast_io::details::print_pack<
		 decltype(::fast_io::details::decay::print_semantic_node_ref(::std::declval<T>()))>);

template <::std::integral char_type, typename T>
using print_semantic_input_alias_t = decltype(::fast_io::io_print_alias(::std::declval<T>()));

template <::std::integral char_type, typename T>
using print_semantic_input_forward_t =
	decltype(::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(::std::declval<T>())));

template <::std::integral char_type, typename T>
inline constexpr bool print_semantic_input_argument_v =
	::fast_io::details::decay::print_semantic_node_no_parameter_v<
		::std::remove_cvref_t<::fast_io::details::decay::print_semantic_input_alias_t<char_type, T>>> ||
	::fast_io::details::decay::print_semantic_node<
		::fast_io::details::decay::print_semantic_input_forward_t<char_type, T>>;

template <::std::integral char_type, typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) print_semantic_input_forward(T &&t) noexcept
{
	if constexpr (::fast_io::details::decay::print_semantic_node_no_parameter_v<
					  ::std::remove_cvref_t<
						  decltype(::fast_io::io_print_alias(::std::forward<T>(t)))>>)
	{
		return ::fast_io::io_print_alias(::std::forward<T>(t));
	}
	else
	{
		return ::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(::std::forward<T>(t)));
	}
}

template <typename T, typename continuation, ::std::size_t... I>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) print_semantic_pack_apply_impl(T &&t, continuation &&cont,
															   ::std::index_sequence<I...>)
{
	auto &&pack_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
	return cont(::fast_io::containers::get<I>(::std::forward<decltype(pack_ref)>(pack_ref).storage)...);
}

template <typename T, typename continuation>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) print_semantic_pack_apply(T &&t, continuation &&cont)
{
	using pack_type =
		::std::remove_cvref_t<decltype(::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t)))>;
	return ::fast_io::details::decay::print_semantic_pack_apply_impl(
		::std::forward<T>(t), ::std::forward<continuation>(cont), ::std::make_index_sequence<pack_type::size>{});
}

template <typename continuation, typename T>
struct print_semantic_value_prefix_continuation
{
	::std::remove_reference_t<continuation> *contptr;
	::std::remove_reference_t<T> *valueptr;

	template <typename... TailArgs>
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr decltype(auto) operator()(TailArgs &&...tail_args) const
	{
		return (*contptr)(::std::forward<T>(*valueptr), ::std::forward<TailArgs>(tail_args)...);
	}
};

template <typename continuation, typename T>
struct print_semantic_lvalue_prefix_continuation
{
	::std::remove_reference_t<continuation> *contptr;
	T *valueptr;

	template <typename... TailArgs>
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr decltype(auto) operator()(TailArgs &&...tail_args) const
	{
		return (*contptr)(*valueptr, ::std::forward<TailArgs>(tail_args)...);
	}
};

template <bool already_forwarded, ::std::integral char_type, typename continuation>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) print_semantic_pack_expand(continuation &&cont)
{
	return cont();
}

template <bool already_forwarded, ::std::integral char_type, typename continuation, typename T, typename... Args>
inline constexpr decltype(auto) print_semantic_pack_expand(continuation &&cont, T &&t, Args &&...args);

template <bool already_forwarded, ::std::integral char_type, typename continuation, typename... ExpandedPackArgs>
struct print_semantic_pack_expand_tail_continuation
{
	::std::remove_reference_t<continuation> *contptr;
	::fast_io::containers::tuple<::std::remove_reference_t<ExpandedPackArgs> *...> expandedptrs;

	template <::std::size_t... I, typename... TailArgs>
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr decltype(auto) operator_impl(::std::index_sequence<I...>, TailArgs &&...tail_args) const
	{
		return (*contptr)(::std::forward<ExpandedPackArgs>(*::fast_io::containers::get<I>(expandedptrs))...,
						  ::std::forward<TailArgs>(tail_args)...);
	}

	template <typename... TailArgs>
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr decltype(auto) operator()(TailArgs &&...tail_args) const
	{
		return operator_impl(::std::make_index_sequence<sizeof...(ExpandedPackArgs)>{},
							 ::std::forward<TailArgs>(tail_args)...);
	}
};

template <bool already_forwarded, ::std::integral char_type, typename continuation, typename... Args>
struct print_semantic_pack_expand_middle_continuation
{
	::std::remove_reference_t<continuation> *contptr;
	::fast_io::containers::tuple<::std::remove_reference_t<Args> *...> const *argptrs;

	template <typename tail_continuation, ::std::size_t... I>
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr decltype(auto) operator_impl(tail_continuation &&tail_cont, ::std::index_sequence<I...>) const
	{
		return ::fast_io::details::decay::print_semantic_pack_expand<already_forwarded, char_type>(
			::std::forward<tail_continuation>(tail_cont),
			::std::forward<Args>(*::fast_io::containers::get<I>(*argptrs))...);
	}

	template <typename... ExpandedPackArgs>
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr decltype(auto) operator()(ExpandedPackArgs &&...expanded_pack_args) const
	{
		return operator_impl(
			::fast_io::details::decay::print_semantic_pack_expand_tail_continuation<
				already_forwarded, char_type, continuation, ExpandedPackArgs...>{
				contptr, {__builtin_addressof(expanded_pack_args)...}},
			::std::make_index_sequence<sizeof...(Args)>{});
	}
};

template <bool already_forwarded, ::std::integral char_type, typename continuation, typename... Args>
struct print_semantic_pack_expand_initial_continuation
{
	::std::remove_reference_t<continuation> *contptr;
	::fast_io::containers::tuple<::std::remove_reference_t<Args> *...> argptrs;

	template <typename... PackArgs>
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr decltype(auto) operator()(PackArgs &&...pack_args) const
	{
		return ::fast_io::details::decay::print_semantic_pack_expand<false, char_type>(
			::fast_io::details::decay::print_semantic_pack_expand_middle_continuation<
				already_forwarded, char_type, continuation, Args...>{contptr, __builtin_addressof(argptrs)},
			::std::forward<PackArgs>(pack_args)...);
	}
};

template <bool already_forwarded, ::std::integral char_type, typename continuation, typename T, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr decltype(auto) print_semantic_pack_expand(continuation &&cont, T &&t, Args &&...args)
{
	if constexpr (::fast_io::details::print_pack<T> ||
				  (::fast_io::details::decay::print_semantic_parameter_object_v<::std::remove_cvref_t<T>> &&
				   ::fast_io::details::print_pack<
					   decltype(::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t)))>))
			{
				return ::fast_io::details::decay::print_semantic_pack_apply(
					::std::forward<T>(t),
					::fast_io::details::decay::print_semantic_pack_expand_initial_continuation<
						already_forwarded, char_type, continuation, Args...>{__builtin_addressof(cont),
																			 {__builtin_addressof(args)...}});
				}
			else if constexpr (already_forwarded)
			{
				return ::fast_io::details::decay::print_semantic_pack_expand<already_forwarded, char_type>(
					::fast_io::details::decay::print_semantic_value_prefix_continuation<continuation, T>{
						__builtin_addressof(cont), __builtin_addressof(t)},
					::std::forward<Args>(args)...);
			}
		else
		{
				auto forwarded{::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(::std::forward<T>(t)))};
				return ::fast_io::details::decay::print_semantic_pack_expand<already_forwarded, char_type>(
					::fast_io::details::decay::print_semantic_lvalue_prefix_continuation<
						continuation, decltype(forwarded)>{__builtin_addressof(cont), __builtin_addressof(forwarded)},
					::std::forward<Args>(args)...);
				}
		}

template <bool had_null, bool has_value, typename continuation>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
inline constexpr decltype(auto) print_semantic_filter_nulls(continuation &&cont)
{
	if constexpr (!has_value && had_null)
	{
		return cont(::fast_io::io_null);
	}
	else
	{
		return cont();
	}
}

template <bool had_null, bool has_value, typename continuation, typename T, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
inline constexpr decltype(auto) print_semantic_filter_nulls(continuation &&cont, T &&t, Args &&...args)
{
	if constexpr (::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>)
	{
		return ::fast_io::details::decay::print_semantic_filter_nulls<true, has_value>(
			::std::forward<continuation>(cont), ::std::forward<Args>(args)...);
	}
	else
	{
		return ::fast_io::details::decay::print_semantic_filter_nulls<had_null, true>(
			::fast_io::details::decay::print_semantic_value_prefix_continuation<continuation, T>{
				__builtin_addressof(cont), __builtin_addressof(t)},
			::std::forward<Args>(args)...);
	}
}

template <::std::integral char_type, typename T>
struct print_freestanding_decay_param_okay_single;

template <::std::integral char_type, typename T>
using print_semantic_forwarded_arg_t =
	decltype(::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(::std::declval<T>())));

template <::std::integral char_type, typename T>
struct print_semantic_static_precise_size_impl
{
	inline static constexpr bool available = ::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t>;
	inline static constexpr ::std::size_t size = available ? 0u : SIZE_MAX;
};

template <::std::integral char_type, typename T>
struct print_semantic_static_precise_size
	: ::fast_io::details::decay::print_semantic_static_precise_size_impl<char_type, ::std::remove_cvref_t<T>>
{};

template <::std::integral char_type, typename T>
	requires(!::fast_io::details::decay::print_semantic_parameter_object_v<T> &&
			 ::fast_io::static_precise_reserve_printable<char_type, T>)
struct print_semantic_static_precise_size_impl<char_type, T>
{
	inline static constexpr bool available = true;
	inline static constexpr ::std::size_t size{
		print_reserve_static_precise_size(::fast_io::io_reserve_type<char_type, T>)};
};

template <::std::integral char_type, typename T>
struct print_semantic_static_precise_size_impl<char_type, ::fast_io::parameter<T>>
	: ::fast_io::details::decay::print_semantic_static_precise_size<char_type, T>
{};

template <::std::integral char_type, typename... Args>
struct print_semantic_static_precise_size_impl<char_type, ::fast_io::manipulators::pack_t<Args...>>
{
	inline static constexpr bool available =
		(::fast_io::details::decay::print_semantic_static_precise_size<
			 char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>>::available &&
		 ...);

	static consteval ::std::size_t static_size() noexcept
	{
		if constexpr (available)
		{
			::std::size_t total{};
			((total = ::fast_io::details::intrinsics::add_or_overflow_die(
				  total, ::fast_io::details::decay::print_semantic_static_precise_size<
							 char_type,
							 ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>>::size)),
			 ...);
			return total;
		}
		else
		{
			return SIZE_MAX;
		}
	}

	inline static constexpr ::std::size_t size{static_size()};
};

template <::std::integral char_type, typename T1, typename T2>
struct print_semantic_static_precise_size_impl<char_type, ::fast_io::manipulators::condition<T1, T2>>
{
	using first_size = ::fast_io::details::decay::print_semantic_static_precise_size<
		char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T1>>;
	using second_size = ::fast_io::details::decay::print_semantic_static_precise_size<
		char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T2>>;
	inline static constexpr bool available =
		first_size::available && second_size::available && (first_size::size == second_size::size);
	inline static constexpr ::std::size_t size = available ? first_size::size : SIZE_MAX;
};

template <::std::integral char_type, typename T>
inline constexpr bool print_semantic_precise_leaf_size_ok_v =
	::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t> ||
	::fast_io::static_precise_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
	::fast_io::precise_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
	::fast_io::scatter_printable<char_type, ::std::remove_cvref_t<T>> ||
	::fast_io::reserve_scatters_printable<char_type, ::std::remove_cvref_t<T>>;

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok_impl
	: ::std::bool_constant<
		  ::fast_io::details::decay::print_semantic_precise_leaf_size_ok_v<char_type, T>>
{};

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok
	: ::fast_io::details::decay::print_semantic_precise_size_ok_impl<char_type, ::std::remove_cvref_t<T>>
{};

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::parameter<T>>
	: ::fast_io::details::decay::print_semantic_precise_size_ok<char_type, T>
{};

template <::std::integral char_type, typename... Args>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::pack_t<Args...>>
	: ::std::bool_constant<
		  (::fast_io::details::decay::print_semantic_precise_size_ok<
			   char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, Args>>::value &&
		   ...)>
{};

template <::std::integral char_type, typename T1, typename T2>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::condition<T1, T2>>
	: ::std::bool_constant<
		  ::fast_io::details::decay::print_semantic_precise_size_ok<
			  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T1>>::value &&
		  ::fast_io::details::decay::print_semantic_precise_size_ok<
			  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T2>>::value>
{};

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::width_t<placement, T>>
	: ::fast_io::details::decay::print_semantic_precise_size_ok<
		  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>
{};

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T,
		  ::std::integral ch_type>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
	: ::std::bool_constant<
		  ::std::same_as<char_type, ch_type> &&
		  ::fast_io::details::decay::print_semantic_precise_size_ok<
			  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>::value>
{};

template <::std::integral char_type, typename T>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::width_runtime_t<T>>
	: ::fast_io::details::decay::print_semantic_precise_size_ok<
		  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>
{};

template <::std::integral char_type, typename T, ::std::integral ch_type>
struct print_semantic_precise_size_ok_impl<char_type, ::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
	: ::std::bool_constant<
		  ::std::same_as<char_type, ch_type> &&
		  ::fast_io::details::decay::print_semantic_precise_size_ok<
			  char_type, ::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, T>>::value>
{};

template <::std::integral char_type, typename T>
struct print_semantic_params_okay : ::std::false_type
{};

template <::std::integral char_type, typename T>
struct print_semantic_params_okay<char_type, ::fast_io::parameter<T>>
	: print_semantic_params_okay<char_type, ::std::remove_cvref_t<T>>
{};

template <::std::integral char_type, typename... Args>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::pack_t<Args...>>
	: ::std::bool_constant<
		  (print_freestanding_decay_param_okay_single<
			   char_type, print_semantic_forwarded_arg_t<char_type, Args>>::value &&
		   ...)>
{};

template <::std::integral char_type, typename T1, typename T2>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::condition<T1, T2>>
	: ::std::bool_constant<
		  print_freestanding_decay_param_okay_single<
			  char_type, print_semantic_forwarded_arg_t<char_type, T1>>::value &&
		  print_freestanding_decay_param_okay_single<
			  char_type, print_semantic_forwarded_arg_t<char_type, T2>>::value>
{};

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::width_t<placement, T>>
	: print_freestanding_decay_param_okay_single<char_type, print_semantic_forwarded_arg_t<char_type, T>>
{};

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T,
		  ::std::integral ch_type>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::width_ch_t<placement, T, ch_type>>
	: ::std::bool_constant<
		  ::std::same_as<char_type, ch_type> &&
		  print_freestanding_decay_param_okay_single<
			  char_type, print_semantic_forwarded_arg_t<char_type, T>>::value>
{};

template <::std::integral char_type, typename T>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::width_runtime_t<T>>
	: print_freestanding_decay_param_okay_single<char_type, print_semantic_forwarded_arg_t<char_type, T>>
{};

template <::std::integral char_type, typename T, ::std::integral ch_type>
struct print_semantic_params_okay<char_type, ::fast_io::manipulators::width_runtime_ch_t<T, ch_type>>
	: ::std::bool_constant<
		  ::std::same_as<char_type, ch_type> &&
		  print_freestanding_decay_param_okay_single<
			  char_type, print_semantic_forwarded_arg_t<char_type, T>>::value>
{};

template <::std::integral char_type, typename T>
struct print_freestanding_decay_param_okay_single
	: ::std::bool_constant<
		  ::fast_io::printable<char_type, T> || ::fast_io::reserve_printable<char_type, T> ||
		  ::fast_io::dynamic_reserve_printable<char_type, T> || ::fast_io::scatter_printable<char_type, T> ||
		  ::fast_io::reserve_scatters_printable<char_type, T> || ::fast_io::context_printable<char_type, T> ||
		  ::fast_io::transcode_imaginary_printable<char_type, T> ||
		  ::std::same_as<::std::remove_cvref_t<T>, ::fast_io::io_null_t> ||
		  print_semantic_params_okay<char_type, ::std::remove_cvref_t<T>>::value>
{};

} // namespace details::decay

namespace operations
{

namespace decay
{

template <bool line, typename outputstmtype, typename... Args>
inline constexpr decltype(auto) print_freestanding_decay_no_pack(outputstmtype optstm, Args... args)
{
	if constexpr (::fast_io::operations::decay::defines::has_status_print_define<outputstmtype>)
	{
		return status_print_define<line>(optstm, args...);
	}
	else if constexpr (sizeof...(Args) == 0)
	{
		if constexpr (line)
		{
			using char_type = typename outputstmtype::output_char_type;
			return ::fast_io::operations::decay::char_put_decay(optstm, char_literal_v<u8'\n', char_type>);
		}
		else
		{
			return;
		}
	}
	else if constexpr (::fast_io::operations::decay::defines::has_output_or_io_stream_mutex_ref_define<outputstmtype>)
	{
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::output_stream_mutex_ref_decay(optstm)};
		return ::fast_io::operations::decay::print_freestanding_decay_no_pack<line>(
			::fast_io::operations::decay::output_stream_unlocked_ref_decay(optstm), args...);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outputstmtype>)
	{
		return ::fast_io::details::decay::print_controls_buffer_impl<line>(optstm, args...);
	}
	else
	{
		return ::fast_io::details::decay::print_controls_impl<line>(optstm, args...);
	}
}

template <bool line, bool already_forwarded, ::std::integral char_type, typename outputstmtype, typename... Args>
inline constexpr void print_semantic_emit(outputstmtype optstm, Args &&...args);

template <::std::integral char_type, typename outputstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_semantic_write_fill(outputstmtype optstm, ::std::size_t n, char_type ch)
{
	if (n == 0)
	{
		return;
	}
	char_type buffer[64];
	::fast_io::details::my_fill_n(buffer, static_cast<::std::size_t>(64u), ch);
	while (static_cast<::std::size_t>(64u) < n)
	{
		::fast_io::operations::decay::write_all_decay(optstm, buffer, buffer + 64);
		n -= static_cast<::std::size_t>(64u);
	}
	::fast_io::operations::decay::write_all_decay(optstm, buffer, buffer + n);
}

template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_semantic_precise_size_arg(T &&t);

template <::std::integral char_type, typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::size_t print_semantic_precise_size_leaf(T &&t)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (::std::same_as<value_type, ::fast_io::io_null_t>)
	{
		return 0;
	}
	else if constexpr (::fast_io::static_precise_reserve_printable<char_type, value_type>)
	{
		return print_reserve_static_precise_size(::fast_io::io_reserve_type<char_type, value_type>);
	}
	else if constexpr (::fast_io::precise_reserve_printable<char_type, value_type>)
	{
		return print_reserve_precise_size(::fast_io::io_reserve_type<char_type, value_type>, ::std::forward<T>(t));
	}
	else if constexpr (::fast_io::scatter_printable<char_type, value_type>)
	{
		return print_scatter_define(::fast_io::io_reserve_type<char_type, value_type>, ::std::forward<T>(t)).len;
	}
	else if constexpr (::fast_io::reserve_scatters_printable<char_type, value_type>)
		{
			constexpr auto sz{print_reserve_scatters_size(::fast_io::io_reserve_type<char_type, value_type>)};
			static_assert(sz.scatters_size != 0);
			constexpr ::std::size_t reserve_buffer_size{sz.reserve_size == 0 ? 1u : sz.reserve_size};
			constexpr bool stack_buffer_ok{
				::fast_io::details::decay::print_stack_buffer_size_within_limit<reserve_buffer_size, char_type> &&
				::fast_io::details::decay::print_stack_buffer_size_within_limit<sz.scatters_size,
																				::fast_io::basic_io_scatter_t<char_type>>};
			if constexpr (stack_buffer_ok)
			{
				::fast_io::basic_io_scatter_t<char_type> scatters[sz.scatters_size];
				char_type buffer[reserve_buffer_size];
				auto result{print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, value_type>, scatters,
														  buffer, ::std::forward<T>(t))};
				::std::size_t len{};
				for (auto iter{scatters}; iter != result.scatters_pos_ptr; ++iter)
				{
					len = ::fast_io::details::intrinsics::add_or_overflow_die(len, iter->len);
				}
				return len;
			}
			else
			{
				::fast_io::details::local_operator_new_array_ptr<::fast_io::basic_io_scatter_t<char_type>> scatters(
					sz.scatters_size);
				::fast_io::details::local_operator_new_array_ptr<char_type> buffer(reserve_buffer_size);
				auto result{print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, value_type>,
														  scatters.ptr, buffer.ptr, ::std::forward<T>(t))};
				::std::size_t len{};
				for (auto iter{scatters.ptr}; iter != result.scatters_pos_ptr; ++iter)
				{
					len = ::fast_io::details::intrinsics::add_or_overflow_die(len, iter->len);
				}
				return len;
			}
		}
	}

template <typename placement_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::size_t print_semantic_width_placement_code(placement_type placement) noexcept
{
	return static_cast<::std::size_t>(placement);
}

template <::std::integral char_type, typename width_traits, typename width_reference_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type print_semantic_width_fill_char(width_reference_type &&wr) noexcept
{
	if constexpr (width_traits::has_fill_char)
	{
		return static_cast<char_type>(wr.ch);
	}
	else
	{
		return ::fast_io::char_literal_v<u8' ', char_type>;
	}
}

template <typename width_traits, typename width_reference_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr auto print_semantic_width_placement(width_reference_type &&wr) noexcept
{
	if constexpr (width_traits::runtime_placement)
	{
		return wr.placement;
	}
	else
	{
		return width_traits::static_placement;
	}
}

template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_semantic_internal_shift_arg(T &&t);

template <::std::integral char_type, typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::size_t print_semantic_internal_shift(T &&t)
{
	auto &&node_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
	using node_type = ::std::remove_cvref_t<decltype(node_ref)>;
	if constexpr (::fast_io::details::decay::print_semantic_condition_v<node_type>)
	{
		if (node_ref.pred)
		{
			return ::fast_io::operations::decay::print_semantic_internal_shift_arg<char_type>(node_ref.t1);
		}
		return ::fast_io::operations::decay::print_semantic_internal_shift_arg<char_type>(node_ref.t2);
	}
	else if constexpr (::fast_io::details::print_pack<node_type> ||
					   ::fast_io::details::decay::print_semantic_width_v<node_type>)
	{
		return 0;
	}
	else
	{
		using value_type = ::std::remove_cvref_t<decltype(node_ref)>;
		if constexpr (::fast_io::printable_internal_shift<char_type, value_type>)
		{
			return print_define_internal_shift(::fast_io::io_reserve_type<char_type, value_type>,
											   ::std::forward<decltype(node_ref)>(node_ref));
		}
		else
		{
			return 0;
		}
	}
}

template <::std::integral char_type, typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::size_t print_semantic_internal_shift_arg(T &&t)
{
	decltype(auto) forwarded{
		::fast_io::details::decay::print_semantic_input_forward<char_type>(::std::forward<T>(t))};
	return ::fast_io::operations::decay::print_semantic_internal_shift<char_type>(
		::std::forward<decltype(forwarded)>(forwarded));
}

template <::std::integral char_type>
struct print_semantic_precise_size_pack_continuation
{
	::std::size_t *totalptr;

	template <typename... PackArgs>
	inline constexpr void operator()(PackArgs &&...pack_args) const
	{
		((*totalptr = ::fast_io::details::intrinsics::add_or_overflow_die(
			  *totalptr,
			  ::fast_io::operations::decay::print_semantic_precise_size_arg<char_type>(
				  ::std::forward<PackArgs>(pack_args)))),
		 ...);
	}
};

template <::std::integral char_type, typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::size_t print_semantic_precise_size(T &&t)
{
	auto &&node_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
	using node_type = ::std::remove_cvref_t<decltype(node_ref)>;
	if constexpr (::fast_io::details::print_pack<node_type>)
	{
		::std::size_t total{};
		::fast_io::details::decay::print_semantic_pack_apply(
			::std::forward<decltype(node_ref)>(node_ref),
			::fast_io::operations::decay::print_semantic_precise_size_pack_continuation<char_type>{
				__builtin_addressof(total)});
		return total;
	}
	else if constexpr (::fast_io::details::decay::print_semantic_condition_v<node_type>)
	{
		if (node_ref.pred)
		{
			return ::fast_io::operations::decay::print_semantic_precise_size_arg<char_type>(node_ref.t1);
		}
		else
		{
			return ::fast_io::operations::decay::print_semantic_precise_size_arg<char_type>(node_ref.t2);
		}
	}
	else if constexpr (::fast_io::details::decay::print_semantic_width_v<node_type>)
	{
		using width_traits = ::fast_io::details::decay::print_semantic_width_traits<node_type>;
		::std::size_t const child_len{
			::fast_io::operations::decay::print_semantic_precise_size_arg<char_type>(node_ref.reference)};
		::std::size_t const width{node_ref.width};
		auto const placement{
			::fast_io::operations::decay::print_semantic_width_placement<width_traits>(node_ref)};
		::std::size_t const placement_code{
			::fast_io::operations::decay::print_semantic_width_placement_code(placement)};
		if (width <= child_len || placement_code == 0u)
		{
			return child_len;
		}
		return width;
	}
	else
	{
		return ::fast_io::operations::decay::print_semantic_precise_size_leaf<char_type>(
			::std::forward<decltype(node_ref)>(node_ref));
	}
}

template <::std::integral char_type, typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::std::size_t print_semantic_precise_size_arg(T &&t)
{
	decltype(auto) forwarded{
		::fast_io::details::decay::print_semantic_input_forward<char_type>(::std::forward<T>(t))};
	return ::fast_io::operations::decay::print_semantic_precise_size<char_type>(
		::std::forward<decltype(forwarded)>(forwarded));
}

template <::std::integral char_type, typename T>
inline constexpr char_type *print_semantic_emit_unchecked(char_type *iter, T &&t);

template <::std::integral char_type, typename T>
inline constexpr char_type *print_semantic_emit_unchecked_arg(char_type *iter, T &&t)
{
	decltype(auto) forwarded{
		::fast_io::details::decay::print_semantic_input_forward<char_type>(::std::forward<T>(t))};
	return ::fast_io::operations::decay::print_semantic_emit_unchecked<char_type>(
		iter, ::std::forward<decltype(forwarded)>(forwarded));
}

template <::std::integral char_type>
struct print_semantic_emit_unchecked_pack_continuation
{
	char_type **iterptr;

	template <typename... PackArgs>
	inline constexpr void operator()(PackArgs &&...pack_args) const
	{
		((*iterptr = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(
			  *iterptr, ::std::forward<PackArgs>(pack_args))),
		 ...);
	}
};

template <::std::integral char_type, typename T>
inline constexpr char_type *print_semantic_emit_unchecked_leaf(char_type *iter, T &&t)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (::std::same_as<value_type, ::fast_io::io_null_t>)
	{
		return iter;
	}
	else if constexpr (::fast_io::static_precise_reserve_printable<char_type, value_type>)
	{
		return print_reserve_define(::fast_io::io_reserve_type<char_type, value_type>, iter, ::std::forward<T>(t));
	}
	else if constexpr (::fast_io::precise_reserve_printable<char_type, value_type>)
	{
		::std::size_t const precise_size{
			print_reserve_precise_size(::fast_io::io_reserve_type<char_type, value_type>, t)};
		if constexpr (requires {
						  {
							  print_reserve_precise_define(::fast_io::io_reserve_type<char_type, value_type>, iter,
														   precise_size, ::std::forward<T>(t))
						  } -> ::std::same_as<char_type *>;
					  })
		{
			return print_reserve_precise_define(::fast_io::io_reserve_type<char_type, value_type>, iter,
												precise_size, ::std::forward<T>(t));
		}
		else
		{
			print_reserve_precise_define(::fast_io::io_reserve_type<char_type, value_type>, iter,
										 precise_size, ::std::forward<T>(t));
			return iter + precise_size;
		}
	}
	else if constexpr (::fast_io::scatter_printable<char_type, value_type>)
	{
		auto scatter{print_scatter_define(::fast_io::io_reserve_type<char_type, value_type>, ::std::forward<T>(t))};
		return ::fast_io::details::non_overlapped_copy_n(scatter.base, scatter.len, iter);
	}
	else if constexpr (::fast_io::reserve_scatters_printable<char_type, value_type>)
		{
			constexpr auto sz{print_reserve_scatters_size(::fast_io::io_reserve_type<char_type, value_type>)};
			static_assert(sz.scatters_size != 0);
			constexpr ::std::size_t reserve_buffer_size{sz.reserve_size == 0 ? 1u : sz.reserve_size};
			constexpr bool stack_buffer_ok{
				::fast_io::details::decay::print_stack_buffer_size_within_limit<reserve_buffer_size, char_type> &&
				::fast_io::details::decay::print_stack_buffer_size_within_limit<sz.scatters_size,
																				::fast_io::basic_io_scatter_t<char_type>>};
			if constexpr (stack_buffer_ok)
			{
				::fast_io::basic_io_scatter_t<char_type> scatters[sz.scatters_size];
				char_type buffer[reserve_buffer_size];
				auto result{print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, value_type>, scatters,
														  buffer, ::std::forward<T>(t))};
				for (auto scatter_iter{scatters}; scatter_iter != result.scatters_pos_ptr; ++scatter_iter)
				{
					iter = ::fast_io::details::non_overlapped_copy_n(scatter_iter->base, scatter_iter->len, iter);
				}
				return iter;
			}
			else
			{
				::fast_io::details::local_operator_new_array_ptr<::fast_io::basic_io_scatter_t<char_type>> scatters(
					sz.scatters_size);
				::fast_io::details::local_operator_new_array_ptr<char_type> buffer(reserve_buffer_size);
				auto result{print_reserve_scatters_define(::fast_io::io_reserve_type<char_type, value_type>,
														  scatters.ptr, buffer.ptr, ::std::forward<T>(t))};
				for (auto scatter_iter{scatters.ptr}; scatter_iter != result.scatters_pos_ptr; ++scatter_iter)
				{
					iter = ::fast_io::details::non_overlapped_copy_n(scatter_iter->base, scatter_iter->len, iter);
				}
				return iter;
			}
		}
	}

template <::std::integral char_type, typename T>
inline constexpr char_type *print_semantic_emit_unchecked(char_type *iter, T &&t)
{
	auto &&node_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
	using node_type = ::std::remove_cvref_t<decltype(node_ref)>;
	if constexpr (::fast_io::details::print_pack<node_type>)
	{
		::fast_io::details::decay::print_semantic_pack_apply(
			::std::forward<decltype(node_ref)>(node_ref),
			::fast_io::operations::decay::print_semantic_emit_unchecked_pack_continuation<char_type>{
				__builtin_addressof(iter)});
		return iter;
	}
	else if constexpr (::fast_io::details::decay::print_semantic_condition_v<node_type>)
	{
		if (node_ref.pred)
		{
			return ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, node_ref.t1);
		}
		return ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, node_ref.t2);
	}
	else if constexpr (::fast_io::details::decay::print_semantic_width_v<node_type>)
	{
		using width_traits = ::fast_io::details::decay::print_semantic_width_traits<node_type>;
		::std::size_t const len{
			::fast_io::operations::decay::print_semantic_precise_size_arg<char_type>(node_ref.reference)};
		::std::size_t const width{node_ref.width};
		char_type const fillch{
			::fast_io::operations::decay::print_semantic_width_fill_char<char_type, width_traits>(node_ref)};
		auto const placement{
			::fast_io::operations::decay::print_semantic_width_placement<width_traits>(node_ref)};
		::std::size_t const placement_code{
			::fast_io::operations::decay::print_semantic_width_placement_code(placement)};
		if (width <= len || placement_code == 0u)
		{
			return ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, node_ref.reference);
		}
		::std::size_t const padding{width - len};
		if (placement_code == 1u)
		{
			iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, node_ref.reference);
			return ::fast_io::details::my_fill_n(iter, padding, fillch);
		}
		if (placement_code == 2u)
		{
			::std::size_t const left_padding{padding >> 1u};
			::std::size_t const right_padding{padding - left_padding};
			iter = ::fast_io::details::my_fill_n(iter, left_padding, fillch);
			iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, node_ref.reference);
			return ::fast_io::details::my_fill_n(iter, right_padding, fillch);
		}
		if (placement_code == 4u)
		{
			::std::size_t const internal_len{
				::fast_io::operations::decay::print_semantic_internal_shift_arg<char_type>(node_ref.reference)};
			if (internal_len != 0)
			{
				char_type *const first{iter};
				char_type *const last{
					::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, node_ref.reference)};
				if (len < internal_len)
				{
					return last;
				}
				char_type *const shift_pos{first + internal_len};
				::fast_io::details::my_copy(shift_pos, last, shift_pos + padding);
				::fast_io::details::my_fill_n(shift_pos, padding, fillch);
				return first + width;
			}
		}
		iter = ::fast_io::details::my_fill_n(iter, padding, fillch);
		return ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, node_ref.reference);
	}
	else
	{
		return ::fast_io::operations::decay::print_semantic_emit_unchecked_leaf<char_type>(
			iter, ::std::forward<decltype(node_ref)>(node_ref));
	}
}

template <bool line, ::std::integral char_type, typename outputstmtype, typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_semantic_emit_width_direct(outputstmtype optstm, T &&reference, ::std::size_t width,
													   char_type fillch, ::std::size_t placement_code,
													   ::std::size_t len)
{
	if constexpr (::fast_io::operations::decay::defines::has_obuffer_basic_operations<outputstmtype>)
	{
		::std::size_t required{(width <= len || placement_code == 0u) ? len : width};
		if constexpr (line)
		{
			required = ::fast_io::details::intrinsics::add_or_overflow_die(required, static_cast<::std::size_t>(1u));
		}
		char_type *const curr{obuffer_curr(optstm)};
		char_type *const end{obuffer_end(optstm)};
		if (static_cast<::std::size_t>(end - curr) >= required)
		{
			char_type *iter{curr};
			if (width <= len || placement_code == 0u)
			{
				iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference);
			}
			else
			{
				::std::size_t const padding{width - len};
				if (placement_code == 1u)
				{
					iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference);
					iter = ::fast_io::details::my_fill_n(iter, padding, fillch);
				}
				else if (placement_code == 2u)
				{
					::std::size_t const left_padding{padding >> 1u};
					::std::size_t const right_padding{padding - left_padding};
					iter = ::fast_io::details::my_fill_n(iter, left_padding, fillch);
					iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference);
					iter = ::fast_io::details::my_fill_n(iter, right_padding, fillch);
				}
				else if (placement_code == 4u)
				{
					::std::size_t const internal_len{
						::fast_io::operations::decay::print_semantic_internal_shift_arg<char_type>(reference)};
					if (internal_len == 0)
					{
						iter = ::fast_io::details::my_fill_n(iter, padding, fillch);
						iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter,
																										 reference);
					}
					else
					{
						char_type *const first{iter};
						char_type *const last{
							::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference)};
						if (len < internal_len)
						{
							iter = last;
						}
						else
						{
							char_type *const shift_pos{first + internal_len};
							::fast_io::details::my_copy(shift_pos, last, shift_pos + padding);
							::fast_io::details::my_fill_n(shift_pos, padding, fillch);
							iter = first + width;
						}
					}
				}
				else
				{
					iter = ::fast_io::details::my_fill_n(iter, padding, fillch);
					iter = ::fast_io::operations::decay::print_semantic_emit_unchecked_arg<char_type>(iter, reference);
				}
			}
			if constexpr (line)
			{
				*iter++ = ::fast_io::char_literal_v<u8'\n', char_type>;
			}
			obuffer_set_curr(optstm, iter);
			return;
		}
	}
	::std::size_t required{(width <= len || placement_code == 0u) ? len : width};
	if constexpr (line)
	{
		required = ::fast_io::details::intrinsics::add_or_overflow_die(required, static_cast<::std::size_t>(1u));
	}
	if (required <= static_cast<::std::size_t>(256u))
	{
		::fast_io::basic_dynamic_output_buffer<char_type> buffer;
		auto buffer_ref{::fast_io::operations::output_stream_ref(buffer)};
		::fast_io::operations::decay::print_semantic_emit_width_direct<line, char_type>(
			buffer_ref, reference, width, fillch, placement_code, len);
		::fast_io::operations::decay::write_all_decay(optstm, buffer.begin_ptr, buffer.curr_ptr);
		return;
	}
	if (width <= len || placement_code == 0u)
	{
		::fast_io::operations::decay::print_semantic_emit<false, false, char_type>(optstm, reference);
	}
	else
	{
		::std::size_t const padding{width - len};
		if (placement_code == 1u)
		{
			::fast_io::operations::decay::print_semantic_emit<false, false, char_type>(optstm, reference);
			::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
		}
		else if (placement_code == 2u)
		{
			::std::size_t const left_padding{padding >> 1u};
			::std::size_t const right_padding{padding - left_padding};
			::fast_io::operations::decay::print_semantic_write_fill(optstm, left_padding, fillch);
			::fast_io::operations::decay::print_semantic_emit<false, false, char_type>(optstm, reference);
			::fast_io::operations::decay::print_semantic_write_fill(optstm, right_padding, fillch);
		}
		else if (placement_code == 4u)
		{
			::std::size_t const internal_len{
				::fast_io::operations::decay::print_semantic_internal_shift_arg<char_type>(reference)};
			if (internal_len == 0)
			{
				::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
				::fast_io::operations::decay::print_semantic_emit<false, false, char_type>(optstm, reference);
			}
			else
			{
				::fast_io::basic_dynamic_output_buffer<char_type> buffer;
				auto buffer_ref{::fast_io::operations::output_stream_ref(buffer)};
				::fast_io::operations::decay::print_semantic_emit<false, false, char_type>(buffer_ref, reference);
				char_type const *const first{buffer.begin_ptr};
				char_type const *const last{buffer.curr_ptr};
				if (len < internal_len)
				{
					::fast_io::operations::decay::write_all_decay(optstm, first, last);
				}
				else
				{
					::fast_io::operations::decay::write_all_decay(optstm, first, first + internal_len);
					::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
					::fast_io::operations::decay::write_all_decay(optstm, first + internal_len, last);
				}
			}
		}
		else
		{
			::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
			::fast_io::operations::decay::print_semantic_emit<false, false, char_type>(optstm, reference);
		}
	}
	if constexpr (line)
	{
		::fast_io::operations::decay::char_put_decay(optstm, ::fast_io::char_literal_v<u8'\n', char_type>);
	}
}

template <bool line, ::std::integral char_type, typename outputstmtype>
struct print_semantic_emit_node_pack_continuation
{
	outputstmtype optstm;

	template <typename... PackArgs>
	inline constexpr void operator()(PackArgs &&...pack_args) const
	{
		::fast_io::operations::decay::print_semantic_emit<line, false, char_type>(
			optstm, ::std::forward<PackArgs>(pack_args)...);
	}
};

template <bool line, ::std::integral char_type, typename outputstmtype, typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_semantic_emit_width(outputstmtype optstm, T &&t)
{
	auto &&width_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
	using width_type = ::std::remove_cvref_t<decltype(width_ref)>;
	using width_traits = ::fast_io::details::decay::print_semantic_width_traits<width_type>;
	using width_child_type =
		::fast_io::details::decay::print_semantic_forwarded_arg_t<char_type, decltype(width_ref.reference)>;
	::std::size_t const width{width_ref.width};
	char_type const fillch{
		::fast_io::operations::decay::print_semantic_width_fill_char<char_type, width_traits>(width_ref)};
	auto const placement{::fast_io::operations::decay::print_semantic_width_placement<width_traits>(width_ref)};
	::std::size_t const placement_code{
		::fast_io::operations::decay::print_semantic_width_placement_code(placement)};
	if constexpr (::fast_io::details::decay::print_semantic_static_precise_size<char_type,
																				width_child_type>::available)
	{
		constexpr ::std::size_t len{
			::fast_io::details::decay::print_semantic_static_precise_size<char_type, width_child_type>::size};
		::fast_io::operations::decay::print_semantic_emit_width_direct<line, char_type>(
			optstm, width_ref.reference, width, fillch, placement_code, len);
		return;
	}
	else if constexpr (::fast_io::details::decay::print_semantic_precise_size_ok<char_type, width_child_type>::value)
	{
		::std::size_t const len{
			::fast_io::operations::decay::print_semantic_precise_size_arg<char_type>(width_ref.reference)};
		::fast_io::operations::decay::print_semantic_emit_width_direct<line, char_type>(
			optstm, width_ref.reference, width, fillch, placement_code, len);
		return;
	}
	::fast_io::basic_dynamic_output_buffer<char_type> buffer;
	auto buffer_ref{::fast_io::operations::output_stream_ref(buffer)};
	::fast_io::operations::decay::print_semantic_emit<false, false, char_type>(buffer_ref, width_ref.reference);
	char_type const *const first{buffer.begin_ptr};
	char_type const *const last{buffer.curr_ptr};
	::std::size_t const len{static_cast<::std::size_t>(last - first)};
	if (width <= len || placement_code == 0u)
	{
		::fast_io::operations::decay::write_all_decay(optstm, first, last);
	}
	else
	{
		::std::size_t const padding{width - len};
		if (placement_code == 1u)
		{
			::fast_io::operations::decay::write_all_decay(optstm, first, last);
			::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
		}
		else if (placement_code == 2u)
		{
			::std::size_t const left_padding{padding >> 1u};
			::std::size_t const right_padding{padding - left_padding};
			::fast_io::operations::decay::print_semantic_write_fill(optstm, left_padding, fillch);
			::fast_io::operations::decay::write_all_decay(optstm, first, last);
			::fast_io::operations::decay::print_semantic_write_fill(optstm, right_padding, fillch);
		}
		else if (placement_code == 4u)
		{
			::std::size_t const internal_len{
				::fast_io::operations::decay::print_semantic_internal_shift_arg<char_type>(width_ref.reference)};
			if (internal_len == 0)
			{
				::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
				::fast_io::operations::decay::write_all_decay(optstm, first, last);
			}
			else if (len < internal_len)
			{
				::fast_io::operations::decay::write_all_decay(optstm, first, last);
			}
			else
			{
				::fast_io::operations::decay::write_all_decay(optstm, first, first + internal_len);
				::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
				::fast_io::operations::decay::write_all_decay(optstm, first + internal_len, last);
			}
		}
		else
		{
			::fast_io::operations::decay::print_semantic_write_fill(optstm, padding, fillch);
			::fast_io::operations::decay::write_all_decay(optstm, first, last);
		}
	}
	if constexpr (line)
	{
		::fast_io::operations::decay::char_put_decay(optstm, ::fast_io::char_literal_v<u8'\n', char_type>);
	}
}

template <bool line, ::std::integral char_type, typename outputstmtype, typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_semantic_emit_node(outputstmtype optstm, T &&t)
{
	auto &&node_ref{::fast_io::details::decay::print_semantic_node_ref(::std::forward<T>(t))};
	using node_type = ::std::remove_cvref_t<decltype(node_ref)>;
	if constexpr (::fast_io::details::print_pack<node_type>)
	{
		::fast_io::details::decay::print_semantic_pack_apply(
			::std::forward<decltype(node_ref)>(node_ref),
			::fast_io::operations::decay::print_semantic_emit_node_pack_continuation<line, char_type, outputstmtype>{
				optstm});
	}
	else if constexpr (::fast_io::details::decay::print_semantic_condition_v<node_type>)
	{
		if (node_ref.pred)
		{
			::fast_io::operations::decay::print_semantic_emit<line, false, char_type>(optstm, node_ref.t1);
		}
		else
		{
			::fast_io::operations::decay::print_semantic_emit<line, false, char_type>(optstm, node_ref.t2);
		}
	}
	else if constexpr (::fast_io::details::decay::print_semantic_width_v<node_type>)
	{
		::fast_io::operations::decay::print_semantic_emit_width<line, char_type>(optstm,
																				::std::forward<T>(t));
	}
}

template <bool line, ::std::integral char_type, typename outputstmtype, typename continuation>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
inline constexpr void print_semantic_emit_flat_impl(outputstmtype, continuation &&cont)
{
	cont.template operator()<line>();
}

template <typename continuation, typename T>
struct print_semantic_emit_flat_prefix_continuation
{
	::std::remove_reference_t<continuation> *contptr;
	::std::remove_reference_t<T> *valueptr;

	template <bool prefix_line, typename... Prefix>
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr void operator()(Prefix &&...prefix) const
	{
		contptr->template operator()<prefix_line>(::std::forward<T>(*valueptr),
												  ::std::forward<Prefix>(prefix)...);
	}
};

template <bool line, ::std::integral char_type, typename outputstmtype, typename continuation, typename T,
		  typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
inline constexpr void print_semantic_emit_flat_impl(outputstmtype optstm, continuation &&cont, T &&t, Args &&...args)
{
	if constexpr (::fast_io::details::decay::print_semantic_node<T>)
	{
		cont.template operator()<false>();
		::fast_io::operations::decay::print_semantic_emit_node<false, char_type>(optstm, ::std::forward<T>(t));
		::fast_io::operations::decay::print_semantic_emit<line, true, char_type>(optstm,
																				::std::forward<Args>(args)...);
	}
	else
	{
		::fast_io::operations::decay::print_semantic_emit_flat_impl<line, char_type>(
			optstm,
			::fast_io::operations::decay::print_semantic_emit_flat_prefix_continuation<continuation, T>{
				__builtin_addressof(cont), __builtin_addressof(t)},
			::std::forward<Args>(args)...);
	}
}

template <typename outputstmtype>
struct print_semantic_emit_freestanding_continuation
{
	outputstmtype optstm;

	template <bool prefix_line, typename... Prefix>
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr void operator()(Prefix &&...prefix) const
	{
		using char_type = typename outputstmtype::output_char_type;
		if constexpr (::fast_io::details::decay::print_controls_dynamic_scatters_reserve_fast_entry_available<
						  char_type, ::std::remove_cvref_t<Prefix>...>())
		{
			::fast_io::details::decay::print_controls_dynamic_scatters_reserve_fast_entry<prefix_line>(
				optstm, ::std::forward<Prefix>(prefix)...);
		}
		else
		{
			::fast_io::operations::decay::print_freestanding_decay_no_pack<prefix_line>(
				optstm, ::std::forward<Prefix>(prefix)...);
		}
	}
};

template <bool line, ::std::integral char_type, typename outputstmtype>
struct print_semantic_emit_flat_continuation
{
	outputstmtype optstm;

	template <typename... FilteredArgs>
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr void operator()(FilteredArgs &&...filtered_args) const
	{
		::fast_io::operations::decay::print_semantic_emit_flat_impl<line, char_type>(
			optstm, ::fast_io::operations::decay::print_semantic_emit_freestanding_continuation<outputstmtype>{optstm},
			::std::forward<FilteredArgs>(filtered_args)...);
	}
};

template <typename emit_flat_type>
struct print_semantic_filter_flat_continuation
{
	::std::remove_reference_t<emit_flat_type> *emitflatptr;

	template <typename... FlatArgs>
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	inline constexpr void operator()(FlatArgs &&...flat_args) const
	{
		::fast_io::details::decay::print_semantic_filter_nulls<false, false>(
			*emitflatptr, ::std::forward<FlatArgs>(flat_args)...);
	}
};

template <bool line, bool already_forwarded, ::std::integral char_type, typename outputstmtype, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
inline constexpr void print_semantic_emit(outputstmtype optstm, Args &&...args)
{
	print_semantic_emit_flat_continuation<line, char_type, outputstmtype> emit_flat{optstm};
	if constexpr (!((::fast_io::details::decay::print_semantic_pack_argument_v<Args> ||
					 ::std::same_as<::std::remove_cvref_t<Args>, ::fast_io::io_null_t>) ||
					...))
	{
		emit_flat(::std::forward<Args>(args)...);
	}
	else
	{
		::fast_io::details::decay::print_semantic_pack_expand<already_forwarded, char_type>(
			::fast_io::operations::decay::print_semantic_filter_flat_continuation<decltype(emit_flat)>{
				__builtin_addressof(emit_flat)},
			::std::forward<Args>(args)...);
	}
}

template <bool line, typename outputstmtype, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
inline constexpr decltype(auto) print_freestanding_decay(outputstmtype optstm, Args... args)
{
	if constexpr ((::fast_io::details::decay::print_semantic_node<Args> || ...))
	{
		using char_type = typename outputstmtype::output_char_type;
		return ::fast_io::operations::decay::print_semantic_emit<line, true, char_type>(optstm, args...);
	}
	else
	{
		using char_type = typename outputstmtype::output_char_type;
		if constexpr (::fast_io::details::decay::print_controls_dynamic_scatters_reserve_fast_entry_available<
						  char_type, ::std::remove_cvref_t<Args>...>())
		{
			return ::fast_io::details::decay::print_controls_dynamic_scatters_reserve_fast_entry<line>(optstm,
																									   args...);
		}
		else
		{
			return ::fast_io::operations::decay::print_freestanding_decay_no_pack<line>(optstm, args...);
		}
	}
}

template <bool line, typename outputstmtype, typename... Args>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr decltype(auto) print_freestanding_decay_cold(outputstmtype optstm, Args... args)
{
#if !__has_cpp_attribute(__gnu__::__cold__) && __has_cpp_attribute(unlikely)
	if (true) [[unlikely]]
#endif
		return ::fast_io::operations::decay::print_freestanding_decay<line>(optstm, args...);
}

} // namespace decay

namespace decay::defines
{

template <typename char_type, typename... Args>
concept print_freestanding_params_okay =
	::std::integral<char_type> &&
	(::fast_io::details::decay::print_freestanding_decay_param_okay_single<char_type, Args>::value && ...);

template <typename output, typename... Args>
concept print_freestanding_okay =
	::fast_io::operations::decay::defines::print_freestanding_params_okay<typename output::output_char_type, Args...>;

} // namespace decay::defines

namespace defines
{

template <typename char_type, typename... Args>
concept print_freestanding_params_okay = ::fast_io::operations::decay::defines::print_freestanding_params_okay<char_type,
																											   decltype(::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(::std::declval<Args>())))...>;

template <typename output, typename... Args>
concept print_freestanding_okay = ::fast_io::operations::decay::defines::print_freestanding_okay<
	decltype(::fast_io::operations::output_stream_ref(::std::declval<output>())),
	decltype(::fast_io::io_print_forward<typename decltype(::fast_io::operations::output_stream_ref(
				 ::std::declval<output>()))::output_char_type>(::fast_io::io_print_alias(::std::declval<Args>())))...>;

} // namespace defines

template <bool line, typename output, typename... Args>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void print_freestanding(output &&outstm, Args &&...args)
{
	auto outref{::fast_io::operations::output_stream_ref(outstm)};
	using char_type = typename decltype(outref)::output_char_type;
	if constexpr ((false || ... ||
				  ::fast_io::details::decay::print_semantic_input_argument_v<char_type, Args &&>))
	{
		::fast_io::operations::decay::print_semantic_emit<line, false, char_type>(
			outref,
			::fast_io::details::decay::print_semantic_input_forward<char_type>(
				::std::forward<Args>(args))...);
	}
	else
	{
		::fast_io::operations::decay::print_freestanding_decay<line>(
			outref, io_print_forward<char_type>(io_print_alias(args))...);
	}
}

} // namespace operations

} // namespace fast_io
