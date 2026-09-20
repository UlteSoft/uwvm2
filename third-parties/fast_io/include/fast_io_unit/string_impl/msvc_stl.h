#pragma once

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC system_header
#endif

namespace fast_io::details::string_hack
{

/*
https://github.com/microsoft/STL/blob/master/stl/inc/xstring
*/

template <class _Elem, class _Traits = ::std::char_traits<_Elem>, class _Alloc = ::std::allocator<_Elem>>
struct
#if __has_cpp_attribute(__gnu__::__may_alias__)
	[[__gnu__::__may_alias__]]
#endif
	model
{
	using _Alty = ::std::_Rebind_alloc_t<_Alloc, _Elem>;
	using _Alty_traits = ::std::allocator_traits<_Alty>;
	using _Scary_val = ::std::_String_val<::std::conditional_t<
		::std::_Is_simple_alloc_v<_Alty>, ::std::_Simple_types<_Elem>,
		::std::_String_iter_types<_Elem, typename _Alty_traits::size_type, typename _Alty_traits::difference_type,
								  typename _Alty_traits::pointer, typename _Alty_traits::const_pointer
#if _MSVC_STL_UPDATE < 202306L
								  ,
								  _Elem &, _Elem const &
#endif
								  >>>;
	using compress_pair_type = ::std::_Compressed_pair<_Alty, _Scary_val>;
	compress_pair_type _Mypair;
};

/// @brief Accesses MSVC STL basic_string's compressed private value after verifying its audited layout.
template <typename elem, typename traits, typename alloc>
inline constexpr decltype(auto) hack_scary_val(::std::basic_string<elem, traits, alloc> &str) noexcept
{
	using model_t = model<elem, traits, alloc>;
	static_assert(sizeof(model_t) == sizeof(::std::basic_string<elem, traits, alloc>) &&
				  alignof(model_t) == alignof(::std::basic_string<elem, traits, alloc>),
			  "MSVC STL changed basic_string's private representation; the fast_io model must be re-audited");
	using compress_pair_type = typename model_t::compress_pair_type;
	using scary_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= typename model_t::_Scary_val *;
	return *reinterpret_cast<scary_ptr>(reinterpret_cast<::std::byte *>(__builtin_addressof(str)) +
										__builtin_offsetof(model_t, _Mypair) + __builtin_offsetof(compress_pair_type, _Myval2));
}

#if _MSVC_STL_UPDATE >= 202310L && (defined(_ANNOTATE_STRING) || defined(_ANNOTATE_STL))
#ifndef FAST_IO_MSVC_STL_INSERT_STRING_ANNOTATION
#define FAST_IO_MSVC_STL_INSERT_STRING_ANNOTATION
#endif
#elif _MSVC_STL_UPDATE >= 202302L &&                                                               \
	(!defined(_M_CEE_PURE) ||                                                                       \
	 (_MSVC_STL_UPDATE >= 202310L && defined(_ENABLE_STL_ANNOTATION_ON_UNSUPPORTED_PLATFORMS))) && \
	!defined(_DISABLE_STRING_ANNOTATION) &&                                                         \
	!(_MSVC_STL_UPDATE >= 202310L && defined(_DISABLE_STL_ANNOTATION))
#ifdef __SANITIZE_ADDRESS__
#ifndef FAST_IO_MSVC_STL_INSERT_STRING_ANNOTATION
#define FAST_IO_MSVC_STL_INSERT_STRING_ANNOTATION
#endif
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#ifndef FAST_IO_MSVC_STL_INSERT_STRING_ANNOTATION
#define FAST_IO_MSVC_STL_INSERT_STRING_ANNOTATION
#endif
#endif
#endif
#if _MSVC_STL_UPDATE < 202310L && defined(_ANNOTATE_STRING)
#ifndef FAST_IO_MSVC_STL_INSERT_STRING_ANNOTATION
#define FAST_IO_MSVC_STL_INSERT_STRING_ANNOTATION
#endif
#endif
#endif

/*
MSVC STL first enabled basic_string container annotations in update 202302
(VS 2022 17.6); 202210 still contained the implementation under `#if 0`.
Keep the same version boundary when naming the sanitizer symbols below.  The
STL's opt-in annotation macros matter even in a non-ASan translation unit, so
they must also disable raw access to spare capacity.  `_ANNOTATE_STL` and
`_DISABLE_STL_ANNOTATION` acquired their umbrella meanings in update 202310.
From that release onward an explicit `_ANNOTATE_STRING`/`_ANNOTATE_STL` is
applied after the disable macros and intentionally wins; older releases apply
`_DISABLE_STRING_ANNOTATION` last.  Mirroring that order prevents this private
adapter from exposing poisoned spare capacity under a mixed opt-in/opt-out
configuration.  The same release added the explicit unsupported-platform
override, which can re-enable compiler-driven annotations even for `_M_CEE_PURE`.
*/

inline constexpr bool msvc_stl_xstring_activate_string_annotation{
#if _MSVC_STL_UPDATE >= 202302L && defined(FAST_IO_MSVC_STL_INSERT_STRING_ANNOTATION)
	true
#endif
};

inline constexpr bool standard_string_runtime_put_area_available{
	!msvc_stl_xstring_activate_string_annotation};

#if _MSVC_STL_UPDATE >= 202302L
// These private ASan helpers first became part of xstring's active implementation in VS 2022 17.6. Keeping their
// declarations behind the same update boundary is required even when annotations are disabled: non-dependent names
// in a discarded `if constexpr` branch are still looked up while an older MSVC STL header is parsed.
// Ordinary inline is deliberate. No cross-version MSVC code-generation evidence justifies cloning this sanitizer-only
// adapter at every string commit site, and the active branch immediately enters the STL/runtime annotation machinery.
/// @brief Reports whether the active MSVC STL configuration requires string container annotations at run time.
inline _CONSTEXPR20 bool msvc_stl_xstring_get_asan_string_should_annotate() noexcept
{
#if defined(FAST_IO_MSVC_STL_INSERT_STRING_ANNOTATION)
	// Consult the runtime switch only when the audited MSVC STL configuration compiled string annotations in.
	if constexpr (::fast_io::details::string_hack::msvc_stl_xstring_activate_string_annotation)
	{
		FAST_IO_IF_CONSTEVAL
		{
			return false;
		}
		else
		{
			return ::_Asan_string_should_annotate;
		}
	}
#endif
	return false;
}

/// @brief Bridges the exact contiguous-container update selected by the matching MSVC STL release.
/// @details This remains ordinary inline for the same reason as the predicate above: annotation-enabled builds are a
///          correctness mode, and duplicating the external sanitizer call sequence has no demonstrated hot-path gain.
inline _CONSTEXPR20 void msvc_stl_sanitizer_annotate_contiguous_container(
	[[maybe_unused]] void const *_First, [[maybe_unused]] void const *_End, [[maybe_unused]] void const *_Old_last, [[maybe_unused]] void const *_New_last)
{
#if defined(FAST_IO_MSVC_STL_INSERT_STRING_ANNOTATION)
	FAST_IO_IF_CONSTEVAL
	{
		return;
	}
	else
	{
		_CSTD __sanitizer_annotate_contiguous_container(_First, _End, _Old_last, _New_last);
	}
#endif
}
/*
https://github.com/microsoft/STL/blob/a357ff1750d3f6dffd54b10d537e93e0accfcc92/stl/inc/xstring#L617
*/
template <typename elem, typename traits, typename alloc>
inline _CONSTEXPR20 void msvc_stl_xstring_Apply_annotation(typename ::std::basic_string<elem, traits, alloc>::value_type const *const _First,
														   typename ::std::basic_string<elem, traits, alloc>::size_type const _Capacity,
														   typename ::std::basic_string<elem, traits, alloc>::size_type const _Old_size,
														   typename ::std::basic_string<elem, traits, alloc>::size_type const _New_size) noexcept
{
#if _HAS_CXX20
	if (_STD is_constant_evaluated())
	{
		return;
	}
#endif // _HAS_CXX20
	using model_t = model<elem, traits, alloc>;
	using _Scary_val = typename model_t::_Scary_val;
	using size_type = typename ::std::basic_string<elem, traits, alloc>::size_type;
	constexpr size_type _Small_string_capacity = _Scary_val::_BUF_SIZE - 1;

	// Don't annotate small strings; only annotate on the heap.
	if (_Capacity <= _Small_string_capacity || !::fast_io::details::string_hack::msvc_stl_xstring_get_asan_string_should_annotate())
	{
		return;
	}

	// Note that `_Capacity`, `_Old_size`, and `_New_size` do not include the null terminator
	void const *const _End = _First + _Capacity + 1;
	void const *const _Old_last = _First + _Old_size + 1;
	void const *const _New_last = _First + _New_size + 1;

	constexpr bool _Large_string_always_asan_aligned =
		(_STD _Container_allocation_minimum_asan_alignment<::std::basic_string<elem, traits, alloc>>) >= _STD _Asan_granularity;

	// for the non-aligned buffer options, the buffer must always have size >= 9 bytes,
	// so it will always end at least one shadow memory section.

	_STD _Asan_aligned_pointers _Aligned;
	if constexpr (_Large_string_always_asan_aligned)
	{
		_Aligned = {_First, _STD _Get_asan_aligned_after(_End)};
	}
	else
	{
		_Aligned = _STD _Get_asan_aligned_first_end(_First, _End);
	}
	void const *const _Old_fixed = _Aligned._Clamp_to_end(_Old_last);
	void const *const _New_fixed = _Aligned._Clamp_to_end(_New_last);

	// --- always aligned case ---
	// old state:
	//   [_First, _Old_last) valid
	//   [_Old_last, asan_aligned_after(_End)) poison
	// new state:
	//   [_First, _New_last) valid
	//   [_New_last, asan_aligned_after(_End)) poison

	// --- sometimes non-aligned case ---
	// old state:
	//   [_Aligned._First, _Old_fixed) valid
	//   [_Old_fixed, _Aligned._End) poison
	//   [_Aligned._End, _End) valid
	// new state:
	//   [_Aligned._First, _New_fixed) valid
	//   [_New_fixed, _Aligned._End) poison
	//   [_Aligned._End, _End) valid
	::fast_io::details::string_hack::msvc_stl_sanitizer_annotate_contiguous_container(_Aligned._First, _Aligned._End, _Old_fixed, _New_fixed);
}

template <typename elem, typename traits, typename alloc>
inline _CONSTEXPR20 void msvc_stl_xstring_Modify_annotation(::std::basic_string<elem, traits, alloc> &str,
															typename ::std::basic_string<elem, traits, alloc>::size_type const _Old_size,
															typename ::std::basic_string<elem, traits, alloc>::size_type const _New_size) noexcept
{
	if (_Old_size == _New_size)
	{
		return;
	}
	decltype(auto) _My_data{::fast_io::details::string_hack::hack_scary_val(str)};
	::fast_io::details::string_hack::msvc_stl_xstring_Apply_annotation<elem, traits, alloc>(_My_data._Myptr(), _My_data._Myres, _Old_size, _New_size);
}
#endif

/// @brief Publishes a new MSVC STL string endpoint while preserving version-appropriate sanitizer annotations.
template <typename T>
inline constexpr void set_end_ptr(T &str, typename T::value_type *ptr) noexcept
{
	decltype(auto) scv{hack_scary_val(str)};
	::std::size_t newsize{static_cast<::std::size_t>(ptr - str.data())};
#if _MSVC_STL_UPDATE >= 202302L
	// Invoke the annotation transition only for releases and build modes which actually maintain poisoned spare capacity.
	if constexpr (::fast_io::details::string_hack::msvc_stl_xstring_activate_string_annotation)
	{
		::fast_io::details::string_hack::msvc_stl_xstring_Modify_annotation(str, scv._Mysize, newsize);
	}
#endif
	scv._Mysize = newsize;
}

template <typename T>
inline constexpr ::std::size_t local_capacity() noexcept
{
	using model_t = model<typename T::value_type, typename T::traits_type, typename T::allocator_type>;
	using _Scary_val = typename model_t::_Scary_val;
	return _Scary_val::_BUF_SIZE - 1;
}

} // namespace fast_io::details::string_hack
