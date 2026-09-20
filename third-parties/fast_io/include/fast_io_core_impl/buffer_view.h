#pragma once

namespace fast_io
{

template <::std::integral ch_type>
struct basic_ibuffer_view
{
	using char_type = ch_type;
	using input_char_type = char_type;
	char_type const *begin_ptr{};
	char_type const *curr_ptr{};
	char_type const *end_ptr{};
	inline constexpr basic_ibuffer_view() noexcept = default;
	template <::std::contiguous_iterator Iter>
		requires ::std::same_as<::std::remove_cvref_t<::std::iter_value_t<Iter>>, char_type>
	inline constexpr basic_ibuffer_view(Iter first, Iter last) noexcept
		: begin_ptr{::std::to_address(first)}, curr_ptr{begin_ptr}, end_ptr{::std::to_address(last)}
	{
	}
	template <::std::ranges::contiguous_range rg>
		requires(::std::same_as<::std::ranges::range_value_t<rg>, char_type> &&
				 !::std::is_array_v<::std::remove_cvref_t<rg>>)
	inline explicit constexpr basic_ibuffer_view(rg &r) noexcept
		: basic_ibuffer_view(::std::ranges::cbegin(r), ::std::ranges::cend(r))
	{
	}
	inline constexpr void clear() noexcept
	{
		curr_ptr = end_ptr;
	}
};

template <::std::integral char_type>
struct basic_ibuffer_view_ref
{
	using input_char_type = typename basic_ibuffer_view<char_type>::input_char_type;
	using native_handle_type = basic_ibuffer_view<char_type> *;
	native_handle_type ptr{};
};

template <::std::integral ch_type>
struct basic_padded_ibuffer_view
{
	using char_type = ch_type;
	using input_char_type = char_type;
	char_type const *begin_ptr{};
	char_type const *curr_ptr{};
	char_type const *end_ptr{};
	::std::size_t padding{};

	inline constexpr basic_padded_ibuffer_view() noexcept = default;

	template <::std::contiguous_iterator Iter>
		requires ::std::same_as<::std::remove_cvref_t<::std::iter_value_t<Iter>>, char_type>
	inline constexpr basic_padded_ibuffer_view(
		Iter first, Iter last, ::std::size_t readable_padding) noexcept
		: begin_ptr{::std::to_address(first)}, curr_ptr{begin_ptr},
		  end_ptr{::std::to_address(last)}, padding{readable_padding}
	{
	}

	template <::std::ranges::contiguous_range rg>
		requires(
			::std::same_as<::std::ranges::range_value_t<rg>, char_type> &&
			!::std::is_array_v<::std::remove_cvref_t<rg>> &&
			::fast_io::contiguous_range_with_padding<rg>)
	inline explicit constexpr basic_padded_ibuffer_view(rg &r) noexcept
		: basic_padded_ibuffer_view(
			  ::std::ranges::cbegin(r), ::std::ranges::cend(r),
			  contiguous_range_padding_size(r))
	{
	}

	inline constexpr void clear() noexcept
	{
		curr_ptr = end_ptr;
	}
};

template <::std::integral char_type>
struct basic_padded_ibuffer_view_ref
{
	using input_char_type =
		typename basic_padded_ibuffer_view<char_type>::input_char_type;
	using native_handle_type = basic_padded_ibuffer_view<char_type> *;
	native_handle_type ptr{};
};

template <::std::integral char_type>
[[nodiscard]] inline constexpr ::std::size_t
contiguous_range_padding_size(
	basic_padded_ibuffer_view<char_type> const &view) noexcept
{
	return view.padding;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr ::std::size_t
contiguous_range_padding_size(
	basic_padded_ibuffer_view_ref<char_type> view) noexcept
{
	return view.ptr->padding;
}

#if 0
template<::std::integral ch_type,::std::contiguous_iterator Iter>
requires ::std::same_as<::std::iter_value_t<Iter>,ch_type>
[[nodiscard]] inline constexpr Iter read(basic_ibuffer_view<ch_type>& view,Iter first,Iter last) noexcept
{
	auto diff{last-first};
	auto view_diff{view.end_ptr-view.curr_ptr};
	if(view_diff<diff)
		diff=view_diff;
	auto it{::fast_io::details::non_overlapped_copy_n(view.curr_ptr,static_cast<::std::size_t>(view_diff),first)};
	view.curr_ptr+=diff;
	return it;
}
#endif

template <::std::integral ch_type>
inline constexpr basic_ibuffer_view_ref<ch_type> input_stream_ref_define(basic_ibuffer_view<ch_type> &other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_ibuffer_view_ref<ch_type> input_stream_ref_define(basic_ibuffer_view<ch_type> &&other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_ibuffer_view_ref<ch_type> input_bytes_stream_ref_define(basic_ibuffer_view<ch_type> &other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_ibuffer_view_ref<ch_type> input_bytes_stream_ref_define(basic_ibuffer_view<ch_type> &&other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
[[nodiscard]] inline constexpr ch_type const *ibuffer_begin(basic_ibuffer_view_ref<ch_type> view) noexcept
{
	return view.ptr->begin_ptr;
}

template <::std::integral ch_type>
[[nodiscard]] inline constexpr ch_type const *ibuffer_curr(basic_ibuffer_view_ref<ch_type> view) noexcept
{
	return view.ptr->curr_ptr;
}

template <::std::integral ch_type>
[[nodiscard]] inline constexpr ch_type const *ibuffer_end(basic_ibuffer_view_ref<ch_type> view) noexcept
{
	return view.ptr->end_ptr;
}

template <::std::integral ch_type>
inline constexpr void ibuffer_set_curr(basic_ibuffer_view_ref<ch_type> view, ch_type const *ptr) noexcept
{
	view.ptr->curr_ptr = ptr;
}

template <::std::integral ch_type>
[[nodiscard]] inline constexpr bool ibuffer_underflow(basic_ibuffer_view_ref<ch_type>) noexcept
{
	return false;
}

template <::std::integral ch_type>
inline constexpr bool ibuffer_underflow_never(basic_ibuffer_view_ref<ch_type>) noexcept
{
	return true;
}

template <::std::integral ch_type>
inline constexpr basic_padded_ibuffer_view_ref<ch_type>
input_stream_ref_define(basic_padded_ibuffer_view<ch_type> &other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_padded_ibuffer_view_ref<ch_type>
input_stream_ref_define(basic_padded_ibuffer_view<ch_type> &&other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_padded_ibuffer_view_ref<ch_type>
input_bytes_stream_ref_define(basic_padded_ibuffer_view<ch_type> &other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_padded_ibuffer_view_ref<ch_type>
input_bytes_stream_ref_define(basic_padded_ibuffer_view<ch_type> &&other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
[[nodiscard]] inline constexpr ch_type const *
ibuffer_begin(basic_padded_ibuffer_view_ref<ch_type> view) noexcept
{
	return view.ptr->begin_ptr;
}

template <::std::integral ch_type>
[[nodiscard]] inline constexpr ch_type const *
ibuffer_curr(basic_padded_ibuffer_view_ref<ch_type> view) noexcept
{
	return view.ptr->curr_ptr;
}

template <::std::integral ch_type>
[[nodiscard]] inline constexpr ch_type const *
ibuffer_end(basic_padded_ibuffer_view_ref<ch_type> view) noexcept
{
	return view.ptr->end_ptr;
}

template <::std::integral ch_type>
inline constexpr void ibuffer_set_curr(
	basic_padded_ibuffer_view_ref<ch_type> view,
	ch_type const *ptr) noexcept
{
	view.ptr->curr_ptr = ptr;
}

template <::std::integral ch_type>
[[nodiscard]] inline constexpr bool
ibuffer_underflow(basic_padded_ibuffer_view_ref<ch_type>) noexcept
{
	return false;
}

template <::std::integral ch_type>
inline constexpr bool
ibuffer_underflow_never(basic_padded_ibuffer_view_ref<ch_type>) noexcept
{
	return true;
}

template <::std::integral ch_type>
struct basic_obuffer_view
{
	using char_type = ch_type;
	using output_char_type = char_type;
	char_type *begin_ptr{}, *curr_ptr{}, *end_ptr{};
	inline constexpr basic_obuffer_view() noexcept = default;
	template <::std::contiguous_iterator Iter>
		requires ::std::same_as<::std::iter_value_t<Iter>, char_type>
	inline constexpr basic_obuffer_view(Iter first, Iter last) noexcept
		: begin_ptr{::std::to_address(first)}, curr_ptr{begin_ptr}, end_ptr{::std::to_address(last)}
	{
	}
	template <::std::ranges::contiguous_range rg>
		requires(::std::same_as<::std::ranges::range_value_t<rg>, char_type> &&
				 !::std::is_array_v<::std::remove_cvref_t<rg>>)
	inline explicit constexpr basic_obuffer_view(rg &r) noexcept
		: basic_obuffer_view(::std::ranges::begin(r), ::std::ranges::end(r))
	{
	}
	inline constexpr void clear() noexcept
	{
		curr_ptr = begin_ptr;
	}
	inline constexpr char_type const *cbegin() const noexcept
	{
		return begin_ptr;
	}
	inline constexpr char_type const *cend() const noexcept
	{
		return curr_ptr;
	}
	inline constexpr char_type const *begin() const noexcept
	{
		return begin_ptr;
	}
	inline constexpr char_type const *end() const noexcept
	{
		return curr_ptr;
	}
	inline constexpr char_type *begin() noexcept
	{
		return begin_ptr;
	}
	inline constexpr char_type *end() noexcept
	{
		return curr_ptr;
	}

	inline constexpr char_type const *data() const noexcept
	{
		return begin_ptr;
	}
	inline constexpr char_type *data() noexcept
	{
		return begin_ptr;
	}

	inline constexpr ::std::size_t size() const noexcept
	{
		return static_cast<::std::size_t>(curr_ptr - begin_ptr);
	}

	inline constexpr ::std::size_t capacity() const noexcept
	{
		return static_cast<::std::size_t>(end_ptr - begin_ptr);
	}

	inline constexpr bool empty() const noexcept
	{
		return begin_ptr == curr_ptr;
	}
};

template <::std::integral char_type>
inline constexpr basic_io_scatter_t<char_type>
print_alias_define(io_alias_t, basic_obuffer_view<char_type> const &view) noexcept
{
	return {view.cbegin(), view.size()};
}

/// @brief Marks an output-buffer view as a stable non-owning scatter source.
/// @details The alias is the view's existing `[cbegin(), cbegin()+size())` interval; producing it performs no
///          allocation, formatting, or scratch-buffer reuse. Consequently retaining the descriptor introduces no new
///          lifetime requirement beyond the external-buffer lifetime already required by `basic_obuffer_view`.
template <::std::integral char_type>
inline constexpr ::std::true_type
print_borrowed_scatter_source(io_reserve_type_t<char_type, basic_obuffer_view<char_type>>) noexcept
{
	return {};
}

template <::std::integral char_type>
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, basic_obuffer_view<char_type>>,
				   basic_obuffer_view<char_type> const &view) noexcept
{
	return view.size();
}

template <::std::integral char_type>
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, basic_obuffer_view<char_type>>,
					 char_type *iter, basic_obuffer_view<char_type> const &view) noexcept
{
	return ::fast_io::details::non_overlapped_copy_n(view.cbegin(), view.size(), iter);
}

template <::std::integral char_type>
inline constexpr ::std::size_t
print_reserve_precise_size(io_reserve_type_t<char_type, basic_obuffer_view<char_type>>,
						   basic_obuffer_view<char_type> const &view) noexcept
{
	return view.size();
}

template <::std::integral char_type>
inline constexpr char_type *
print_reserve_precise_define(io_reserve_type_t<char_type, basic_obuffer_view<char_type>>,
							 char_type *iter, ::std::size_t, basic_obuffer_view<char_type> const &view) noexcept
{
	return ::fast_io::details::non_overlapped_copy_n(view.cbegin(), view.size(), iter);
}

template <::std::integral char_type>
struct basic_obuffer_view_ref
{
	using output_char_type = typename basic_obuffer_view<char_type>::output_char_type;
	using native_handle_type = basic_obuffer_view<char_type> *;
	native_handle_type ptr{};
};

/// The fixed external buffer owns a stable writable range, so print may fold a complete scatter copy into one cursor
/// publication without invoking the generic write CPO. Growing/string-like wrappers intentionally do not advertise this
/// stronger destination proof.
template <::std::integral ch_type>
inline constexpr ::std::true_type print_direct_obuffer_copy_safe(
	::fast_io::io_reserve_type_t<ch_type, basic_obuffer_view_ref<ch_type>>) noexcept
{
	return {};
}

template <::std::integral char_type>
inline constexpr ::std::true_type print_deferred_obuffer_commit_safe(
	io_reserve_type_t<char_type, basic_obuffer_view_ref<char_type>>) noexcept
{
	// The view keeps a stable fixed allocation, and cursor publication is exactly one assignment to curr_ptr. Raw
	// in-area copies followed by one final publication therefore have the same observable cursor state as publishing
	// after every copy; capacity exhaustion remains the view's terminating overflow policy.
	return {};
}

template <::std::integral char_type>
inline constexpr ::std::true_type obuffer_address_distance_safe_define(
	io_reserve_type_t<char_type, basic_obuffer_view_ref<char_type>>) noexcept
{
	return {};
}

#if 0
template<::std::integral ch_type,::std::contiguous_iterator Iter>
requires ::std::same_as<::std::iter_value_t<Iter>,ch_type>
inline constexpr void write(basic_obuffer_view<ch_type>& view,Iter first,Iter last) noexcept
{
	auto diff{last-first};
	auto view_diff{view.end_ptr-view.curr_ptr};
	if(view_diff<diff)
		fast_terminate();
	view.curr_ptr=::fast_io::details::non_overlapped_copy_n(first,static_cast<::std::size_t>(diff),view.curr_ptr);
}
#endif

template <::std::integral ch_type>
inline constexpr basic_obuffer_view_ref<ch_type> output_stream_ref_define(basic_obuffer_view<ch_type> &other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_obuffer_view_ref<ch_type> output_stream_ref_define(basic_obuffer_view<ch_type> &&other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_obuffer_view_ref<ch_type> output_bytes_stream_ref_define(basic_obuffer_view<ch_type> &other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_obuffer_view_ref<ch_type> output_bytes_stream_ref_define(basic_obuffer_view<ch_type> &&other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
[[nodiscard]] inline constexpr ch_type *obuffer_begin(basic_obuffer_view_ref<ch_type> view) noexcept
{
	return view.ptr->begin_ptr;
}

template <::std::integral ch_type>
[[nodiscard]] inline constexpr ch_type *obuffer_curr(basic_obuffer_view_ref<ch_type> view) noexcept
{
	return view.ptr->curr_ptr;
}

template <::std::integral ch_type>
[[nodiscard]] inline constexpr ch_type *obuffer_end(basic_obuffer_view_ref<ch_type> view) noexcept
{
	return view.ptr->end_ptr;
}

template <::std::integral ch_type>
inline constexpr void obuffer_set_curr(basic_obuffer_view_ref<ch_type> view, ch_type *ptr) noexcept
{
	view.ptr->curr_ptr = ptr;
}

template <::std::integral ch_type>
inline constexpr void obuffer_overflow(basic_obuffer_view_ref<ch_type>, ch_type) noexcept
{
	fast_terminate();
}

template <::std::integral ch_type>
inline constexpr bool obuffer_overflow_never(basic_obuffer_view_ref<ch_type>) noexcept
{
	return true;
}

/// @brief Completes a bulk write for a fixed-capacity output view.
/// @details A put area is only a fast-path cursor protocol; it is not by itself proof that a buffer miss can consume
///          the remaining range. This completion CPO makes the fixed-capacity contract explicit. Exact fits are valid,
///          while an oversized write follows the view's existing non-growing overflow policy and terminates before any
///          partial copy. Consequently destination-aware print admission may rely on `writable` without treating every
///          arbitrary put-area provider as a complete output device. A cv-qualified code-unit type is deliberately not
///          admitted: its cursor CPOs remain structurally formable, but primitive output requires an assignable,
///          ordinary code-unit destination.
template <::std::integral ch_type>
	requires ::std::same_as<ch_type, ::std::remove_cv_t<ch_type>>
inline constexpr void write_all_overflow_define(
	basic_obuffer_view_ref<ch_type> view, ch_type const *first, ch_type const *last) noexcept
{
	// The default-constructed empty view is represented by null pointers. Handle an empty source before inspecting the
	// observer so that even a null reference wrapper remains a valid no-op. For a nonempty source, test the zero-capacity
	// representation before subtracting cursors: C++ does not define nullptr - nullptr, despite both pointers comparing
	// equal. The remaining subtraction is between cursors from the view's one contiguous allocation. A negative distance
	// denotes a corrupted/reversed cursor and must not become a huge size_t after conversion.
	if (first == last)
	{
		return;
	}
	if (view.ptr == nullptr) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	auto const curr_ptr{view.ptr->curr_ptr};
	auto const end_ptr{view.ptr->end_ptr};
	if (curr_ptr == end_ptr || curr_ptr == nullptr || end_ptr == nullptr) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	auto const available_difference{end_ptr - curr_ptr};
	if (available_difference < 0) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	auto const count{static_cast<::std::size_t>(last - first)};
	auto const available{static_cast<::std::size_t>(available_difference)};
	if (available < count) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	view.ptr->curr_ptr = ::fast_io::details::non_overlapped_copy_n(first, count, curr_ptr);
}

using ibuffer_view = basic_ibuffer_view<char>;
using wibuffer_view = basic_ibuffer_view<wchar_t>;
using u8ibuffer_view = basic_ibuffer_view<char8_t>;
using u16ibuffer_view = basic_ibuffer_view<char16_t>;
using u32ibuffer_view = basic_ibuffer_view<char32_t>;
using padded_ibuffer_view = basic_padded_ibuffer_view<char>;
using padded_wibuffer_view = basic_padded_ibuffer_view<wchar_t>;
using padded_u8ibuffer_view = basic_padded_ibuffer_view<char8_t>;
using padded_u16ibuffer_view = basic_padded_ibuffer_view<char16_t>;
using padded_u32ibuffer_view = basic_padded_ibuffer_view<char32_t>;
using obuffer_view = basic_obuffer_view<char>;
using wobuffer_view = basic_obuffer_view<wchar_t>;
using u8obuffer_view = basic_obuffer_view<char8_t>;
using u16obuffer_view = basic_obuffer_view<char16_t>;
using u32obuffer_view = basic_obuffer_view<char32_t>;
} // namespace fast_io

namespace fast_io::details::decay
{

template <::std::integral char_type>
inline constexpr bool print_buffered_mixed_generic_endpoint<
	::fast_io::basic_obuffer_view_ref<char_type>> = false;

} // namespace fast_io::details::decay
