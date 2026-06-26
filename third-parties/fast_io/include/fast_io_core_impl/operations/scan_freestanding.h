#pragma once

namespace fast_io
{

namespace details
{
#if 0
template<typename input,typename T,typename P>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr bool scan_single_status_impl(input in,T& state_machine,P arg)
{
	for(;state_machine.code==parse_code::partial;)
	{
		if(!ibuffer_underflow(in))
		{
			if(!state_machine.test_eof(arg))
				return false;
			if(state_machine.code==parse_code{})[[likely]]
				return true;
			break;
		}
		auto curr{ibuffer_curr(in)};
		auto end{ibuffer_end(in)};
		state_machine(curr,end,arg);
		ibuffer_set_curr(in, state_machine.iter);
		if(state_machine.code==parse_code::ok)[[likely]]
			return true;
	}
	throw_parse_code(state_machine.code);
}
#endif

template <typename input, typename P>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr bool scan_context_status_impl(input in, P arg)
{
	using char_type = typename input::input_char_type;
	for (typename ::std::remove_cvref_t<decltype(scan_context_type(io_reserve_type<char_type, P>))>::type state;;)
	{
		auto curr{ibuffer_curr(in)};
		auto end{ibuffer_end(in)};
		auto [it, ec] = scan_context_define(io_reserve_type<char_type, P>, state, curr, end, arg);
		if constexpr (::std::same_as<decltype(curr), decltype(it)>)
		{
			ibuffer_set_curr(in, it);
		}
		else
		{
			ibuffer_set_curr(in, it - curr + curr);
		}
		if (ec == parse_code::ok)
		{
			return true;
		}
		else if (ec != parse_code::partial)
		{
			throw_parse_code(ec);
		}
		if (!ibuffer_underflow(in)) [[unlikely]]
		{
			ec = scan_context_eof_define(io_reserve_type<char_type, P>, state, arg);
			if (ec == parse_code::ok)
			{
				return true;
			}
			else if (ec == parse_code::end_of_file)
			{
				break;
			}
			throw_parse_code(ec);
		}
	}
	return false;
}

template <bool>
inline constexpr bool type_not_scannable = false;

template <typename input, typename T>
[[nodiscard]] inline constexpr bool scan_single_impl(input in, T arg)
{
	using char_type = typename input::input_char_type;
#if 0
	if constexpr(contiguous_input_stream<input>)
	{
		if constexpr(precise_reserve_scannable<char_type,T>)
		{
			constexpr ::std::size_t n{scan_precise_reserve_size(io_reserve_type<char_type,T>)};
			auto curr_ptr{ibuffer_curr(in)};
			char_type const* curr{curr_ptr};
			char_type const* end{ibuffer_end(in)};
			::std::size_t const diff{static_cast<::std::size_t>(end-curr)};
			if(diff<n)[[unlikely]]
			{
				return false;
			}
			if constexpr(precise_reserve_scannable_no_error<char_type,T>)
			{
				scan_precise_reserve_define(io_reserve_type<char_type,T>,curr,arg);
			}
			else
			{
				auto ret{scan_precise_reserve_define(io_reserve_type<char_type,T>,curr,arg)};
				if(ret!=parse_code::ok)
				{
					if(ret==parse_code::end_of_file)
					{
						return false;
					}
					throw_parse_code(ret);
				}
			}
			ibuffer_set_curr(in,curr_ptr+n);
			return true;
		}
		else if constexpr(contiguous_scannable<char_type,T>)
		{
			auto curr{ibuffer_curr(in)};
			auto end{ibuffer_end(in)};
			auto [it,ec] = scan_contiguous_define(io_reserve_type<char_type,T>,curr,end,arg);
			if constexpr(::std::same_as<decltype(curr),decltype(it)>)
			{
				ibuffer_set_curr(in,it);
			}
			else
			{
				ibuffer_set_curr(in,it-curr+curr);
			}
			if(ec!=parse_code::ok)
			{
				if(ec==parse_code::end_of_file)
					return false;
				throw_parse_code(ec);
			}
			return true;
		}
		else if constexpr(context_scannable<char_type,T>)
		{
			typename ::std::remove_cvref_t<decltype(scan_context_type(io_reserve_type<char_type,T>))>::type state;
			auto curr{ibuffer_curr(in)};
			auto end{ibuffer_end(in)};
			auto [it,ec]=scan_context_define(io_reserve_type<char_type,T>,state,curr,end,arg);
			if constexpr(::std::same_as<decltype(curr),decltype(it)>)
			{
				ibuffer_set_curr(in,it);
			}
			else
			{
				ibuffer_set_curr(in,it-curr+curr);
			}
			if(ec==parse_code::ok)
				return true;
			else if(ec!=parse_code::partial)
				throw_parse_code(ec);
			ec=scan_context_eof_define(io_reserve_type<char_type,T>,state,arg);
			if(ec==parse_code::ok)
				return true;
			else if(ec!=parse_code::end_of_file)
				throw_parse_code(ec);
			return false;
		}
		else
		{
			constexpr bool not_scannable{context_scannable<char_type,T>};
			static_assert(not_scannable,"type not scannable. need context_scannable");
			return false;
		}
	}
	else
#endif
	{
		if constexpr (precise_reserve_scannable<char_type, T>)
		{
			constexpr ::std::size_t n{scan_precise_reserve_size(io_reserve_type<char_type, T>)};
			char_type buffer[n];
			if constexpr (::fast_io::details::asan_state::current == ::fast_io::details::asan_state::activate)
			{
				char_type const *p{buffer};
				::fast_io::operations::decay::read_all_decay(in, buffer, buffer + n);
				if constexpr (precise_reserve_scannable_no_error<char_type, T>)
				{
					scan_precise_reserve_define(io_reserve_type<char_type, T>, p, arg);
				}
				else
				{
					auto ret{scan_precise_reserve_define(io_reserve_type<char_type, T>, p, arg)};
					if (ret != parse_code::ok)
					{
						if (ret == parse_code::end_of_file)
						{
							return false;
						}
						throw_parse_code(ret);
					}
				}
			}
			else
			{
				auto curr_ptr{ibuffer_curr(in)};
				char_type const *curr{curr_ptr};
				char_type const *end{ibuffer_end(in)};
				char_type const *p{curr};
				::std::size_t const diff{static_cast<::std::size_t>(end - curr)};
				bool inbuffer{diff < n};
				if (inbuffer) [[unlikely]]
				{
					::fast_io::operations::decay::read_all_decay(in, buffer, buffer + n);
					p = buffer;
				}
				if constexpr (precise_reserve_scannable_no_error<char_type, T>)
				{
					scan_precise_reserve_define(io_reserve_type<char_type, T>, p, arg);
				}
				else
				{
					auto ret{scan_precise_reserve_define(io_reserve_type<char_type, T>, p, arg)};
					if (ret != parse_code::ok)
					{
						if (ret == parse_code::end_of_file)
						{
							return false;
						}
						throw_parse_code(ret);
					}
				}
				if (!inbuffer) [[likely]]
				{
					ibuffer_set_curr(in, curr_ptr + n);
				}
			}
			return true;
		}
		else if constexpr (contiguous_scannable<char_type, T> && context_scannable<char_type, T>)
		{
			auto curr{ibuffer_curr(in)};
			auto end{ibuffer_end(in)};
			auto [it, ec] = scan_contiguous_define(io_reserve_type<char_type, T>, curr, end, arg);
			if (it == end)
			{
				return scan_context_status_impl(in, arg);
			}
			else
			{
				if constexpr (::std::same_as<decltype(curr), decltype(it)>)
				{
					ibuffer_set_curr(in, it);
				}
				else
				{
					ibuffer_set_curr(in, it - curr + curr);
				}
				if (ec != parse_code::ok)
				{
					throw_parse_code(ec);
				}
			}
			return true;
		}
		else if constexpr (context_scannable<char_type, T>)
		{
			for (typename ::std::remove_cvref_t<decltype(scan_context_type(io_reserve_type<char_type, T>))>::type state;;)
			{
				auto curr{ibuffer_curr(in)};
				auto end{ibuffer_end(in)};
				auto [it, ec] = scan_context_define(io_reserve_type<char_type, T>, state, curr, end, arg);
				if constexpr (::std::same_as<decltype(curr), decltype(it)>)
				{
					ibuffer_set_curr(in, it);
				}
				else
				{
					ibuffer_set_curr(in, it - curr + curr);
				}
				if (ec == parse_code::ok)
				{
					return true;
				}
				else if (ec != parse_code::partial)
				{
					throw_parse_code(ec);
				}
				if (!ibuffer_underflow(in)) [[unlikely]]
				{
					ec = scan_context_eof_define(io_reserve_type<char_type, T>, state, arg);
					if (ec == parse_code::ok)
					{
						return true;
					}
					else if (ec == parse_code::end_of_file)
					{
						break;
					}
					throw_parse_code(ec);
				}
			}
			return false;
		}
		else
		{
			constexpr bool not_scannable{context_scannable<char_type,T>};
			static_assert(not_scannable,"type not scannable. need context_scannable");
			return false;
		}
	}
}

namespace decay
{

struct precise_scan_run_result
{
	::std::size_t position{};
	::std::size_t neededspace{};
};

template <::std::integral char_type, typename T>
inline constexpr bool batch_precise_scannable_no_error{
	::fast_io::precise_reserve_scannable_no_error<char_type, T>};

template <::std::integral char_type, typename Arg, typename... Args>
inline constexpr precise_scan_run_result find_continuous_precise_scan_n() noexcept
{
	if constexpr (::fast_io::details::decay::batch_precise_scannable_no_error<char_type, Arg>)
	{
		constexpr ::std::size_t sz{scan_precise_reserve_size(::fast_io::io_reserve_type<char_type, Arg>)};
		if constexpr (sizeof...(Args) == 0)
		{
			return {1u, sz};
		}
		else
		{
			constexpr precise_scan_run_result res{
				::fast_io::details::decay::find_continuous_precise_scan_n<char_type, Args...>()};
			return {res.position + 1u,
					::fast_io::details::intrinsics::add_or_overflow_die(res.neededspace, sz)};
		}
	}
	else
	{
		return {};
	}
}

template <::std::size_t n, ::std::integral char_type>
inline constexpr char_type const *scan_n_precise_reserve_no_error(char_type const *p) noexcept
{
	return p;
}

template <::std::size_t n, ::std::integral char_type, typename T, typename... Args>
inline constexpr char_type const *scan_n_precise_reserve_no_error(char_type const *p, T t, Args... args)
{
	if constexpr (n == 0)
	{
		return p;
	}
	else
	{
		constexpr ::std::size_t sz{scan_precise_reserve_size(::fast_io::io_reserve_type<char_type, T>)};
		scan_precise_reserve_define(::fast_io::io_reserve_type<char_type, T>, p, t);
		if constexpr (n == 1)
		{
			return p + sz;
		}
		else
		{
			return ::fast_io::details::decay::scan_n_precise_reserve_no_error<n - 1, char_type>(p + sz, args...);
		}
	}
}

struct precise_scan_may_error_result
{
	bool handled{};
	bool success{};
};

template <typename input, typename T>
[[nodiscard]] inline constexpr precise_scan_may_error_result scan_precise_reserve_may_error_in_buffer_impl(input in, T t)
{
	using char_type = typename input::input_char_type;
	constexpr ::std::size_t n{scan_precise_reserve_size(::fast_io::io_reserve_type<char_type, T>)};
	auto curr_ptr{ibuffer_curr(in)};
	char_type const *curr{curr_ptr};
	char_type const *end{ibuffer_end(in)};
	::std::size_t const diff{static_cast<::std::size_t>(end - curr)};
	if (diff < n) [[unlikely]]
	{
		return {};
	}
	auto ret{scan_precise_reserve_define(::fast_io::io_reserve_type<char_type, T>, curr, t)};
	if (ret != ::fast_io::parse_code::ok)
	{
		if (ret == ::fast_io::parse_code::end_of_file)
		{
			return {true, false};
		}
		throw_parse_code(ret);
	}
	ibuffer_set_curr(in, curr_ptr + n);
	return {true, true};
}

template <typename input, ::std::size_t skippings = 0>
[[nodiscard]] inline constexpr bool scan_controls_impl(input) noexcept
{
	return true;
}

template <typename input, ::std::size_t skippings = 0, typename T, typename... Args>
[[nodiscard]] inline constexpr bool scan_controls_impl(input in, T t, Args... args)
{
	if constexpr (skippings != 0)
	{
		if constexpr (sizeof...(Args) == 0)
		{
			return true;
		}
		else
		{
			return ::fast_io::details::decay::scan_controls_impl<input, skippings - 1>(in, args...);
		}
	}
	else if constexpr (::fast_io::details::asan_state::current == ::fast_io::details::asan_state::activate)
	{
		if (!::fast_io::details::scan_single_impl(in, t))
		{
			return false;
		}
		if constexpr (sizeof...(Args) == 0)
		{
			return true;
		}
		else
		{
			return ::fast_io::details::decay::scan_controls_impl(in, args...);
		}
	}
	else
	{
		using char_type = typename input::input_char_type;
		constexpr precise_scan_run_result res{
			::fast_io::details::decay::find_continuous_precise_scan_n<char_type, T, Args...>()};
		if constexpr (res.position > 1)
		{
			auto curr_ptr{ibuffer_curr(in)};
			char_type const *curr{curr_ptr};
			char_type const *end{ibuffer_end(in)};
			::std::size_t const diff{static_cast<::std::size_t>(end - curr)};
			if (diff >= res.neededspace) [[likely]]
			{
				::fast_io::details::decay::scan_n_precise_reserve_no_error<res.position, char_type>(curr, t,
																									args...);
				ibuffer_set_curr(in, curr_ptr + res.neededspace);
				if constexpr (res.position == sizeof...(Args) + 1u)
				{
					return true;
				}
				else
				{
					return ::fast_io::details::decay::scan_controls_impl<input, res.position - 1u>(in, args...);
				}
			}
		}
		else if constexpr (::fast_io::precise_reserve_scannable<char_type, T> &&
						   !::fast_io::precise_reserve_scannable_no_error<char_type, T>)
		{
			auto may_error_result{
				::fast_io::details::decay::scan_precise_reserve_may_error_in_buffer_impl(in, t)};
			if (may_error_result.handled)
			{
				if (!may_error_result.success)
				{
					return false;
				}
				if constexpr (sizeof...(Args) == 0)
				{
					return true;
				}
				else
				{
					return ::fast_io::details::decay::scan_controls_impl(in, args...);
				}
			}
		}

		if (!::fast_io::details::scan_single_impl(in, t))
		{
			return false;
		}
		if constexpr (sizeof...(Args) == 0)
		{
			return true;
		}
		else
		{
			return ::fast_io::details::decay::scan_controls_impl(in, args...);
		}
	}
}

} // namespace decay
} // namespace details

namespace operations::decay
{

template <typename input, typename... Args>
[[nodiscard]] inline constexpr decltype(auto) scan_freestanding_decay(input instm, Args... args)
{
	if constexpr (::fast_io::operations::decay::defines::has_status_scan_define<input>)
	{
		return status_scan_define(instm, args...);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_input_or_io_stream_mutex_ref_define<input>)
	{
		::fast_io::operations::decay::stream_ref_decay_lock_guard lg{
			::fast_io::operations::decay::input_stream_mutex_ref_decay(instm)};
		return scan_freestanding_decay(::fast_io::operations::decay::input_stream_unlocked_ref_decay(instm), args...);
	}
	else if constexpr (::fast_io::operations::decay::defines::has_ibuffer_basic_operations<input>)
	{
		return ::fast_io::details::decay::scan_controls_impl(instm, args...);
	}
	else if constexpr (::fast_io::operations::defines::available_add_ibuf<input>)
	{
		static_assert(::fast_io::operations::decay::defines::has_status_scan_define<input>, 
			"If you want to scan this type of file, please add ::fast_io::basic_ibuf.");
		return false;
	}
	else
	{
		static_assert(::fast_io::operations::decay::defines::has_status_scan_define<input>, "type not scannable.");
		return false;
	}
}

} // namespace operations::decay

} // namespace fast_io
