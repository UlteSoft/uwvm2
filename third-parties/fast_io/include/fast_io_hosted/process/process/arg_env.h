#pragma once
namespace fast_io
{
struct default_args_t
{
	inline explicit constexpr default_args_t() noexcept = default;
};
inline constexpr default_args_t default_args{};

struct args_with_argv0_t
{
	inline explicit constexpr args_with_argv0_t() noexcept = default;
};
inline constexpr args_with_argv0_t args_with_argv0{};

#if (defined(_WIN32) && !defined(__WINE__)) || defined(__CYGWIN__)

namespace details
{
template <::std::integral replace_char_type, typename Iter>
inline constexpr void append_win32_quoted_arg_common(
	bool is_first,
	::fast_io::containers::basic_string<replace_char_type, ::fast_io::native_global_allocator> &str,
	Iter first, Iter last)
{
	if (is_first)
	{
		bool needs_quote{};
		auto it{first};
		for (; it != last; ++it)
		{
			auto const c{*it};
			if (c == ::fast_io::char_literal_v<u8'\"', replace_char_type>) [[unlikely]]
			{
				throw_win32_error(13);
			}
			if (c <= ::fast_io::char_literal_v<u8' ', replace_char_type>)
			{
				needs_quote = true;
			}
		}

		if (first == last)
		{
			str.push_back(::fast_io::char_literal_v<u8'\"', replace_char_type>);
			str.push_back(::fast_io::char_literal_v<u8'\"', replace_char_type>);
			str.push_back(::fast_io::char_literal_v<u8' ', replace_char_type>);
			return;
		}

		if (!needs_quote)
		{
			for (it = first; it != last; ++it)
			{
				str.push_back(*it);
			}
			str.push_back(::fast_io::char_literal_v<u8' ', replace_char_type>);
			return;
		}

		str.push_back(::fast_io::char_literal_v<u8'\"', replace_char_type>);
		for (it = first; it != last; ++it)
		{
			str.push_back(*it);
		}
		str.push_back(::fast_io::char_literal_v<u8'\"', replace_char_type>);
		str.push_back(::fast_io::char_literal_v<u8' ', replace_char_type>);
	}
	else
	{
		// Reserve rough upper bound: quotes + worst-case doubling
		str.reserve(str.size() + 3 + static_cast<::std::size_t>(last - first) * 2u);
		str.push_back_unchecked(::fast_io::char_literal_v<u8'\"', replace_char_type>);

		::std::size_t backslash_count{};
		for (; first != last; ++first)
		{
			auto const c{*first};
			if (c == ::fast_io::char_literal_v<u8'\"', replace_char_type>)
			{
				// Output 2*n+1 backslashes before a quote
				for (::std::size_t i{}; i != ((backslash_count << 1u) + 1u); ++i)
				{
					str.push_back_unchecked(::fast_io::char_literal_v<u8'\\', replace_char_type>);
				}
				str.push_back_unchecked(::fast_io::char_literal_v<u8'\"', replace_char_type>);
				backslash_count = 0;
			}
			else if (c == ::fast_io::char_literal_v<u8'\\', replace_char_type>)
			{
				++backslash_count;
			}
			else
			{
				// Flush pending backslashes (not before a quote): output as-is
				for (::std::size_t i{}; i != backslash_count; ++i)
				{
					str.push_back_unchecked(::fast_io::char_literal_v<u8'\\', replace_char_type>);
				}
				backslash_count = 0;
				str.push_back_unchecked(c);
			}
		}
		// Before closing quote, double any trailing backslashes
		for (::std::size_t i{}; i != (backslash_count << 1u); ++i)
		{
			str.push_back_unchecked(::fast_io::char_literal_v<u8'\\', replace_char_type>);
		}
		str.push_back_unchecked(::fast_io::char_literal_v<u8'\"', replace_char_type>);
		str.push_back_unchecked(::fast_io::char_literal_v<u8' ', replace_char_type>);
	}
}

/// @brief Formats one already-normalized Windows process argument without reopening source ownership.
/// @details The enclosing pack owner has materialized the result of `io_print_alias` followed by `io_print_forward`.
///          Consequently `t` is the exact stable lvalue modeled by the decayed print predicate below. Re-entering the
///          public print boundary would normalize that object a second time, while accepting it by value would copy every
///          suffix once per recursive level. This helper instead borrows the unique owner and performs no lifetime
///          extension. The `code_cvt` fallback creates a new source wrapper, so only that wrapper enters the source-only
///          unforwarded bridge exactly once.
template <::std::integral replace_char_type, typename T>
inline constexpr void construct_win32_process_args_decay_single_ref(
	bool is_first,
	::fast_io::containers::basic_string<replace_char_type, ::fast_io::native_global_allocator> &str,
	T &t)
{
	using buffer_type = ::fast_io::basic_obuffer_view<replace_char_type>;
	using output_type = ::fast_io::basic_obuffer_view_ref<replace_char_type>;
	constexpr bool source_printable{
		::fast_io::operations::decay::defines::print_freestanding_okay_for_line<
			false, output_type, T>};

	if constexpr (source_printable)
	{
		replace_char_type buf[32767];
		buffer_type buffer{buf, buf + 32767};
		output_type output{__builtin_addressof(buffer)};
		// The concept names the exact dispatcher template types; execution observes their stable lvalue expressions.
		::fast_io::operations::decay::print_freestanding_decay_impl<false>(output, t);
		append_win32_quoted_arg_common<replace_char_type>(is_first, str, buffer.cbegin(), buffer.cend());
	}
	else if constexpr (requires { ::fast_io::mnp::code_cvt(t); })
	{
		using codecvt_type = decltype(::fast_io::mnp::code_cvt(::std::declval<T &>()));
		using codecvt_run = ::fast_io::operations::defines::print_freestanding_named_normalized_run_t<
			replace_char_type, codecvt_type &>;
		constexpr bool codecvt_printable{
			codecvt_run::template output_okay_for_line<false, output_type>};
		if constexpr (codecvt_printable)
		{
			replace_char_type buf[32767];
			buffer_type buffer{buf, buf + 32767};
			output_type output{__builtin_addressof(buffer)};
			auto converted{::fast_io::mnp::code_cvt(t)};
			::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(output, converted);
			append_win32_quoted_arg_common<replace_char_type>(is_first, str, buffer.cbegin(), buffer.cend());
		}
		else
		{
			static_assert(codecvt_printable,
						  "the code-converted process argument is not printable to the selected Windows character domain");
		}
	}
	else
	{
		static_assert(source_printable, "some types are not printable or codecvt printable, so we cannot construct basic_win32_process_args");
	}
}

template <bool is_first, ::std::integral replace_char_type, typename T, typename... Args>
inline constexpr void construct_win32_process_args_decay(
	::fast_io::containers::basic_string<replace_char_type, ::fast_io::native_global_allocator> &str, T t, Args... args)
{
	// This parameter pack is the sole owner of every normalized proxy. The comma fold is sequenced left-to-right and each
	// element is borrowed exactly once, so construction count is O(N) and argument ordering remains observable and stable.
	construct_win32_process_args_decay_single_ref(is_first, str, t);
	(construct_win32_process_args_decay_single_ref(false, str, args), ...);
}

/// @brief Appends one already-normalized Windows environment entry and its required NUL terminator.
/// @details Both the proof and the decayed dispatcher observe the normalized source and the local terminator as stable
///          lvalues. The terminator is an operation-local value, whereas the source remains owned exclusively by the
///          enclosing pack frame. No recursive or per-element value boundary is reopened.
template <::std::integral replace_char_type, typename T>
inline constexpr void construct_win32_process_envs_decay_single_ref(
	::fast_io::containers::basic_string<replace_char_type, ::fast_io::native_global_allocator> &str, T &t)
{
	using string_type = ::fast_io::containers::basic_string<
		replace_char_type, ::fast_io::native_global_allocator>;
	using output_type = ::fast_io::io_strlike_reference_wrapper<
		replace_char_type, string_type>;
	using terminator_type = decltype(::fast_io::mnp::chvw(
		::fast_io::char_literal_v<u8'\0', replace_char_type>));
	constexpr bool source_printable{
		::fast_io::operations::decay::defines::print_freestanding_okay_for_line<
			false, output_type, T, terminator_type>};

	if constexpr (source_printable)
	{
		output_type wrapper{__builtin_addressof(str)};
		terminator_type terminator{
			::fast_io::mnp::chvw(::fast_io::char_literal_v<u8'\0', replace_char_type>)};
		::fast_io::operations::decay::print_freestanding_decay_impl<false>(wrapper, t, terminator);
	}
	else if constexpr (requires { ::fast_io::mnp::code_cvt(t); })
	{
		using codecvt_type = decltype(::fast_io::mnp::code_cvt(::std::declval<T &>()));
		using codecvt_run = ::fast_io::operations::defines::print_freestanding_named_normalized_run_t<
			replace_char_type, codecvt_type &, terminator_type &>;
		constexpr bool codecvt_printable{
			codecvt_run::template output_okay_for_line<false, output_type>};
		if constexpr (codecvt_printable)
		{
			output_type wrapper{__builtin_addressof(str)};
			auto converted{::fast_io::mnp::code_cvt(t)};
			terminator_type terminator{
				::fast_io::mnp::chvw(::fast_io::char_literal_v<u8'\0', replace_char_type>)};
			::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(
				wrapper, converted, terminator);
		}
		else
		{
			static_assert(codecvt_printable,
						  "the code-converted environment entry is not printable to the selected Windows character domain");
		}
	}
	else
	{
		static_assert(source_printable, "some types are not printable or codecvt printable, so we cannot construct basic_win32_process_envs");
	}
}

template <::std::integral replace_char_type, typename T, typename... Args>
inline constexpr void construct_win32_process_envs_decay(
	::fast_io::containers::basic_string<replace_char_type, ::fast_io::native_global_allocator> &str, T t, Args... args)
{
	// Each entry contributes one explicit NUL; basic_string contributes the final implicit terminator, yielding the
	// Windows double-NUL environment block without a second ownership layer.
	construct_win32_process_envs_decay_single_ref(str, t);
	(construct_win32_process_envs_decay_single_ref(str, args), ...);
}

} // namespace details

template <::fast_io::win32_family family, bool is_first>
struct basic_win32_process_args 
{
	inline static constexpr bool is_nt{family == ::fast_io::win32_family::wide_nt};
	using char_type = ::std::conditional_t<is_nt, char16_t, char>;
	using replace_char_type = ::std::conditional_t<is_nt, char16_t, char8_t>;
	using storage_type = ::fast_io::containers::basic_string<replace_char_type, ::fast_io::native_global_allocator>;
	using char_type_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= char_type *;
	using char_type_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= char_type const *;

	storage_type args{};

	inline constexpr basic_win32_process_args() noexcept = default;

	inline constexpr basic_win32_process_args(default_args_t, char_type const *oscstr) noexcept
	{
		args = storage_type{::fast_io::mnp::os_c_str(oscstr)};
	}

	template <typename T, typename... Args>
		requires(!::std::same_as<::std::remove_cvref_t<T>, default_args_t>)
	inline constexpr basic_win32_process_args(T &&t, Args &&...as)
	{
		details::construct_win32_process_args_decay<is_first>(args,
															  ::fast_io::io_print_forward<replace_char_type>(::fast_io::io_print_alias(t)),
															  ::fast_io::io_print_forward<replace_char_type>(::fast_io::io_print_alias(as))...);
	}

	inline constexpr char_type_may_alias_const_ptr get() const noexcept
	{
		if (args.empty())
		{
			return nullptr;
		}
		else
		{
			return reinterpret_cast<char_type_may_alias_const_ptr>(args.c_str());
		}
	}

	inline constexpr basic_win32_process_args &append(basic_win32_process_args const &others) noexcept
	{
		args.append(others.args);

		return *this;
	}
};

template <::fast_io::win32_family family>
struct basic_win32_process_envs 
{
	inline static constexpr bool is_nt{family == ::fast_io::win32_family::wide_nt};
	using char_type = ::std::conditional_t<is_nt, char16_t, char>;
	using replace_char_type = ::std::conditional_t<is_nt, char16_t, char8_t>;
	using storage_type = ::fast_io::containers::basic_string<replace_char_type, ::fast_io::native_global_allocator>;
	using char_type_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= char_type *;
	using char_type_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= char_type const *;

	storage_type envs{};

	inline constexpr basic_win32_process_envs() noexcept = default;

	inline constexpr basic_win32_process_envs(default_args_t, char_type const *oscstr) noexcept
	{
		auto estr{oscstr};
		for (; *estr || estr[1]; ++estr)
		{
		}
		estr += 2; // "\0\0"
		envs = storage_type{oscstr, estr};
	}

	template <typename T, typename... Args>
		requires(!::std::same_as<::std::remove_cvref_t<T>, default_args_t>)
	inline constexpr basic_win32_process_envs(T &&t, Args &&...as)
	{
		details::construct_win32_process_envs_decay(envs,
													::fast_io::io_print_forward<replace_char_type>(::fast_io::io_print_alias(t)),
													::fast_io::io_print_forward<replace_char_type>(::fast_io::io_print_alias(as))...);
	}

	inline constexpr char_type_may_alias_const_ptr get() const noexcept
	{
		if (envs.empty())
		{
			return nullptr;
		}
		else
		{
			return reinterpret_cast<char_type_may_alias_const_ptr>(envs.c_str());
		}
	}

	inline constexpr basic_win32_process_envs &append(basic_win32_process_envs const &others) noexcept
	{
		envs.append(others.envs);

		return *this;
	}
};

// Provide a Windows command line with argv0 version conversion, where argv0 is specially handled.

using win32_process_args_ntw = ::fast_io::basic_win32_process_args<::fast_io::win32_family::wide_nt, false>;
using win32_process_args_ntw_with_argv0 = ::fast_io::basic_win32_process_args<::fast_io::win32_family::wide_nt, true>;
using win32_process_envs_ntw = ::fast_io::basic_win32_process_envs<::fast_io::win32_family::wide_nt>;

using win32_process_args_9xa = ::fast_io::basic_win32_process_args<::fast_io::win32_family::ansi_9x, false>;
using win32_process_args_9xa_with_argv0 = ::fast_io::basic_win32_process_args<::fast_io::win32_family::ansi_9x, true>;
using win32_process_envs_9xa = ::fast_io::basic_win32_process_envs<::fast_io::win32_family::ansi_9x>;

using win32_process_args = ::fast_io::basic_win32_process_args<::fast_io::win32_family::native, false>;
using win32_process_args_with_argv0 = ::fast_io::basic_win32_process_args<::fast_io::win32_family::native, true>;
using win32_process_envs = ::fast_io::basic_win32_process_envs<::fast_io::win32_family::native>;

using nt_process_args = ::fast_io::basic_win32_process_args<::fast_io::win32_family::wide_nt, false>;
using nt_process_args_with_argv0 = ::fast_io::basic_win32_process_args<::fast_io::win32_family::wide_nt, true>;
using nt_process_envs = ::fast_io::basic_win32_process_envs<::fast_io::win32_family::wide_nt>;

#else

// posix
namespace details
{

template <::std::integral char_type>
struct cstr_guard 
{
	using Alloc = ::fast_io::native_typed_global_allocator<char_type>;

	char_type *cstr{};

	inline constexpr cstr_guard() noexcept = default;

	inline constexpr cstr_guard(cstr_guard const &others) noexcept
	{
		if (others.cstr == nullptr)
		{
			cstr = nullptr;
			return;
		}

		::std::size_t str_size{::fast_io::cstr_len(others.cstr)};
		cstr = Alloc::allocate(str_size + 1);
		auto const lase_ptr{::fast_io::freestanding::non_overlapped_copy_n(others.cstr, str_size, cstr)};
		*lase_ptr = ::fast_io::char_literal_v<u8'\0', char_type>;
	}

	inline constexpr cstr_guard &operator=(cstr_guard const &others) noexcept
	{
		if (__builtin_addressof(others) == this) [[unlikely]]
		{
			return *this;
		}

		Alloc::deallocate(cstr);

		if (others.cstr == nullptr)
		{
			cstr = nullptr;
			return *this;
		}

		::std::size_t str_size{::fast_io::cstr_len(others.cstr)};
		cstr = Alloc::allocate(str_size + 1);
		auto const lase_ptr{::fast_io::freestanding::non_overlapped_copy_n(others.cstr, str_size, cstr)};
		*lase_ptr = ::fast_io::char_literal_v<u8'\0', char_type>;

		return *this;
	}

	inline constexpr cstr_guard(cstr_guard &&others) noexcept
	{
		cstr = others.cstr;
		others.cstr = nullptr;
	}

	inline constexpr cstr_guard &operator=(cstr_guard &&others) noexcept
	{
		if (__builtin_addressof(others) == this) [[unlikely]]
		{
			return *this;
		}

		Alloc::deallocate(cstr);

		cstr = others.cstr;
		others.cstr = nullptr;

		return *this;
	}

	inline constexpr ~cstr_guard()
	{
		Alloc::deallocate(cstr);
	}
};

} // namespace details

namespace freestanding
{
template <::std::integral char_type>
struct is_trivially_copyable_or_relocatable<details::cstr_guard<char_type>>
{
	inline static constexpr bool value = true;
};

template <::std::integral char_type>
struct is_zero_default_constructible<details::cstr_guard<char_type>>
{
	inline static constexpr bool value = true;
};

} // namespace freestanding

namespace details
{
/// @brief Transfers one completed argument string into the owning POSIX C-string array.
/// @details A nonempty fast_io string already owns a uniquely allocated NUL-terminated range, so ownership can move by
///          transferring its implementation pointer. The default empty string may instead reference the library's static
///          empty sentinel; allocating one explicit NUL prevents `cstr_guard` from later deallocating non-owned storage and
///          distinguishes an empty argument from the null pointer which terminates `argv`.
template <::std::integral replace_char_type>
inline constexpr void push_posix_process_arg_string(
	::fast_io::containers::vector<cstr_guard<replace_char_type>, ::fast_io::native_global_allocator> &arguments,
	::fast_io::containers::basic_string<replace_char_type, ::fast_io::native_global_allocator> &&value)
{
	cstr_guard<replace_char_type> guard;
	if (value.empty())
	{
		using allocator_type = typename cstr_guard<replace_char_type>::Alloc;
		guard.cstr = allocator_type::allocate(1u);
		*guard.cstr = ::fast_io::char_literal_v<u8'\0', replace_char_type>;
	}
	else
	{
		guard.cstr = value.imp.begin_ptr;
		value.imp = {};
	}
	arguments.push_back(::std::move(guard));
}

/// @brief Formats one normalized POSIX argument through a stable string-output reference.
/// @details `source` is owned by the enclosing decay-pack frame and has already completed alias and character forwarding.
///          The decayed proof names the exact dispatcher template types whose stable lvalue expressions execute below.
///          A codecvt fallback creates a distinct wrapper and therefore uses the source-only unforwarded bridge once; it
///          never re-aliases the original normalized proxy. The completed string is then transferred without copying its
///          character range.
template <::std::integral replace_char_type, typename T>
inline constexpr void construct_posix_process_argenvs_decay_single_ref(
	::fast_io::containers::vector<cstr_guard<replace_char_type>, ::fast_io::native_global_allocator> &arguments,
	T &source)
{
	using string_type = ::fast_io::containers::basic_string<
		replace_char_type, ::fast_io::native_global_allocator>;
	using output_type = ::fast_io::io_strlike_reference_wrapper<
		replace_char_type, string_type>;
	constexpr bool source_printable{
		::fast_io::operations::decay::defines::print_freestanding_okay_for_line<
			false, output_type, T>};

	string_type value;
	output_type output{__builtin_addressof(value)};
	if constexpr (source_printable)
	{
		::fast_io::operations::decay::print_freestanding_decay_impl<false>(output, source);
	}
	else if constexpr (requires { ::fast_io::mnp::code_cvt(source); })
	{
		using codecvt_type = decltype(::fast_io::mnp::code_cvt(::std::declval<T &>()));
		using codecvt_run = ::fast_io::operations::defines::print_freestanding_named_normalized_run_t<
			replace_char_type, codecvt_type &>;
		constexpr bool codecvt_printable{
			codecvt_run::template output_okay_for_line<false, output_type>};
		if constexpr (codecvt_printable)
		{
			auto converted{::fast_io::mnp::code_cvt(source)};
			::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(
				output, converted);
		}
		else
		{
			static_assert(codecvt_printable,
						  "the code-converted process argument is not printable to the selected POSIX character domain");
		}
	}
	else
	{
		static_assert(source_printable,
					  "some types are not printable or codecvt printable, so we cannot construct posix_process_args");
	}

	::fast_io::details::push_posix_process_arg_string(arguments, ::std::move(value));
}

/// @brief Compatibility entry for direct users of the historical raw-source helper.
/// @details Normal process construction does not call this function; it enters the normalized reference helper above.
///          Keeping this single-element raw boundary preserves existing internal callers which intentionally ask concat to
///          perform source normalization. It owns only one source and contains no recursive suffix, so it cannot recreate
///          the former quadratic pack-copy behavior.
template <::std::size_t N, ::std::integral replace_char_type, typename T>
inline constexpr void construct_posix_process_argenvs_decay_singal(
	::fast_io::containers::vector<cstr_guard<replace_char_type>, ::fast_io::native_global_allocator> &str, T t)
{
	using string_type = ::fast_io::containers::basic_string<
		replace_char_type, ::fast_io::native_global_allocator>;
	constexpr bool source_printable{
		::fast_io::basic_general_concat_checked_available<
			false, replace_char_type, string_type, T &>()};

	if constexpr (source_printable)
	{
		// Concat proves and executes its selected string destination. A public print proof against a dummy stream does
		// not describe concat's raw source normalization or its direct-versus-staging destination choice.
		auto cstr{::fast_io::basic_general_concat_checked<
			false, replace_char_type, string_type>(t)};
		::fast_io::details::push_posix_process_arg_string(str, ::std::move(cstr));
	}
	else if constexpr (requires { ::fast_io::mnp::code_cvt(t); })
	{
		auto cstr{::fast_io::basic_general_concat<false, replace_char_type, ::fast_io::containers::basic_string<replace_char_type, ::fast_io::native_global_allocator>>(::fast_io::mnp::code_cvt(t))};
		::fast_io::details::push_posix_process_arg_string(str, ::std::move(cstr));
	}
	else
	{
		static_assert(source_printable, "some types are not printable or codecvt printable, so we cannot construct posix_process_envs");
	}
}

template <::std::size_t N = 0, ::std::integral replace_char_type, typename T, typename... Args>
inline constexpr void construct_posix_process_argenvs_decay(
	::fast_io::containers::vector<cstr_guard<replace_char_type>, ::fast_io::native_global_allocator> &str, T t, Args... args)
{
	// The function parameters are the sole normalized proxy owners. A sequenced fold borrows each one exactly once and
	// removes both the suffix recursion and its N + (N - 1) + ... + 1 copy-construction surface.
	construct_posix_process_argenvs_decay_single_ref(str, t);
	(construct_posix_process_argenvs_decay_single_ref(str, args), ...);
}

namespace posix
{
#if defined(__APPLE__) || defined(__DARWIN_C_LEVEL)
// Darwin does not provide an `environ` function; here we use `_NSGetEnviron` to obtain it.
extern char ***_NSGetEnviron() noexcept __asm__("__NSGetEnviron");
#elif defined(__MSDOS__) || defined(__DJGPP__)
// djgpp only provides `char** _environ`. For consistency, a symbolic link is used here.
extern char **environ __asm__("__environ");
#elif !(defined(_WIN32) || defined(__CYGWIN__))
// Reference to the global `environ` variable
extern "C" char **environ;
#endif
} // namespace posix
} // namespace details

struct posix_process_args 
{
	using char_type = char;
	using replace_char_type = char;
	using storage_type = ::fast_io::containers::vector<details::cstr_guard<replace_char_type>, ::fast_io::native_global_allocator>;
	using char_type_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= char_type *;
	using char_type_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= char_type const *;

	storage_type arg_envs{};

	inline constexpr posix_process_args() noexcept
	{
		arg_envs.emplace_back(); // nullptr
	}

	inline constexpr posix_process_args(default_args_t, char const *const *args) noexcept
	{
		for (char const *const *curr{args}; *curr; ++curr)
		{
			details::cstr_guard<replace_char_type> str;
			::std::size_t str_size{::fast_io::cstr_len(*curr)};
			str.cstr = details::cstr_guard<replace_char_type>::Alloc::allocate(str_size + 1);
			auto const lase_ptr{::fast_io::freestanding::non_overlapped_copy_n(*curr, str_size, str.cstr)};
			*lase_ptr = ::fast_io::char_literal_v<u8'\0', replace_char_type>;
			arg_envs.push_back(::std::move(str));
		}
		arg_envs.emplace_back(); // nullptr
	}

	template <typename T, typename... Args>
		requires(!::std::same_as<::std::remove_cvref_t<T>, default_args_t>)
	inline constexpr posix_process_args(T &&t, Args &&...as)
	{
		details::construct_posix_process_argenvs_decay(arg_envs,
													   ::fast_io::io_print_forward<replace_char_type>(::fast_io::io_print_alias(t)),
													   ::fast_io::io_print_forward<replace_char_type>(::fast_io::io_print_alias(as))...);
		arg_envs.emplace_back(); // nullptr
	}

	inline char const *const *get_argv() const noexcept
	{
		using char_const_p_const_p_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char const *const *;

		return reinterpret_cast<char_const_p_const_p_may_alias_ptr>(arg_envs.data());
	}

	inline char const *const *get_envs() const noexcept
	{
		using char_const_p_const_p_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char const *const *;

		if (arg_envs.size() < 2u)
		{
#if defined(__APPLE__) && defined(__MACH__)
			return reinterpret_cast<char_const_p_const_p_may_alias_ptr>(*::fast_io::details::posix::_NSGetEnviron());
#else
			return reinterpret_cast<char_const_p_const_p_may_alias_ptr>(::fast_io::details::posix::environ);
#endif
		}
		else
		{
			return reinterpret_cast<char_const_p_const_p_may_alias_ptr>(arg_envs.data());
		}
	}

	inline constexpr posix_process_args &append(posix_process_args const &others) noexcept
	{
		if (others.arg_envs.size() > 1) [[likely]]
		{
			arg_envs.pop_back(); // check and rm nullptr

			arg_envs.reserve(arg_envs.size() + others.arg_envs.size()); // sz + o.sz + 1, no need to check

			auto const end{others.arg_envs.cend() - 1}; // without nullptr
			for (auto curr{others.arg_envs.cbegin()}; curr != end; ++curr)
			{
				arg_envs.emplace_back_unchecked(*curr);
			}

			arg_envs.emplace_back_unchecked(); // nullptr
		}

		return *this;
	}
};

using posix_process_envs = posix_process_args;

#endif

namespace freestanding
{
#if (defined(_WIN32) && !defined(__WINE__)) || defined(__CYGWIN__)

template <::fast_io::win32_family family, bool is_first>
struct is_trivially_copyable_or_relocatable<basic_win32_process_args<family, is_first>>
{
	inline static constexpr bool value = true;
};

template <::fast_io::win32_family family>
struct is_trivially_copyable_or_relocatable<basic_win32_process_envs<family>>
{
	inline static constexpr bool value = true;
};
#else

template <>
struct is_trivially_copyable_or_relocatable<posix_process_args>
{
	inline static constexpr bool value = true;
};
#endif
} // namespace freestanding
} // namespace fast_io
