#pragma once

namespace fast_io
{

template <::std::integral ch_type>
class basic_omemory_map
{
public:
	using char_type = ch_type;
	using output_char_type = char_type;
	char_type *begin_ptr{}, *curr_ptr{}, *end_ptr{};
	inline constexpr basic_omemory_map() = default;
	inline constexpr basic_omemory_map(native_memory_map_file const &iob, ::std::size_t offset = 0)
		: begin_ptr(reinterpret_cast<char_type *>(iob.address_begin + offset)), curr_ptr(begin_ptr),
		  end_ptr(begin_ptr + iob.size() / sizeof(char_type))
	{
	}

	inline constexpr ::std::size_t written_bytes() const noexcept
	{
		return static_cast<::std::size_t>(curr_ptr - begin_ptr) * sizeof(char_type);
	}
};

template <::std::integral ch_type>
struct basic_omemory_map_ref
{
	using output_char_type = ch_type;
	basic_omemory_map<ch_type> *ptr{};
};

template <::std::integral ch_type>
inline constexpr basic_omemory_map_ref<ch_type>
output_stream_ref_define(basic_omemory_map<ch_type> &other) noexcept
{
	// The cursor is mutable stream state. A value observer would advance only a disposable copy and make
	// `written_bytes()` permanently report the original position.
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_omemory_map_ref<ch_type>
output_stream_ref_define(basic_omemory_map<ch_type> &&other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_omemory_map_ref<ch_type>
output_bytes_stream_ref_define(basic_omemory_map<ch_type> &other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_omemory_map_ref<ch_type>
output_bytes_stream_ref_define(basic_omemory_map<ch_type> &&other) noexcept
{
	return {__builtin_addressof(other)};
}

#if 0
namespace details
{

template<::std::integral char_type>
inline constexpr void omemory_map_write_impl(basic_omemory_map<char_type>& bomp,char_type const* begin,char_type const* end) noexcept
{
	::std::size_t const to_write(end-begin);
	if(static_cast<::std::size_t>(bomp.end_ptr-bomp.curr_ptr)<to_write)
		fast_terminate();
	non_overlapped_copy_n(begin,to_write,bomp.curr_ptr);
	bomp.curr_ptr+=to_write;
}
}

template<::std::integral char_type,::std::contiguous_iterator Iter>
inline constexpr void write(basic_omemory_map<char_type>& bomp,Iter begin,Iter end) noexcept
{
	details::omemory_map_write_impl(bomp,::std::to_address(begin),::std::to_address(end));
}
#endif

template <::std::integral char_type>
inline constexpr char_type *obuffer_begin(basic_omemory_map<char_type> &bomp) noexcept
{
	return bomp.begin_ptr;
}
template <::std::integral char_type>
inline constexpr char_type *obuffer_curr(basic_omemory_map<char_type> &bomp) noexcept
{
	return bomp.curr_ptr;
}
template <::std::integral char_type>
inline constexpr char_type *obuffer_end(basic_omemory_map<char_type> &bomp) noexcept
{
	return bomp.end_ptr;
}

template <::std::integral char_type>
inline constexpr void obuffer_overflow(basic_omemory_map<char_type> &, char_type) noexcept
{
	fast_terminate();
}

template <::std::integral char_type>
inline constexpr void obuffer_set_curr(basic_omemory_map<char_type> &bomp, char_type *ptr) noexcept
{
	bomp.curr_ptr = ptr;
}

template <::std::integral char_type>
inline constexpr char_type *obuffer_begin(basic_omemory_map_ref<char_type> bomp) noexcept
{
	return bomp.ptr->begin_ptr;
}

template <::std::integral char_type>
inline constexpr char_type *obuffer_curr(basic_omemory_map_ref<char_type> bomp) noexcept
{
	return bomp.ptr->curr_ptr;
}

template <::std::integral char_type>
inline constexpr char_type *obuffer_end(basic_omemory_map_ref<char_type> bomp) noexcept
{
	return bomp.ptr->end_ptr;
}

template <::std::integral char_type>
inline constexpr void obuffer_set_curr(basic_omemory_map_ref<char_type> bomp, char_type *ptr) noexcept
{
	bomp.ptr->curr_ptr = ptr;
}

template <::std::integral char_type>
inline constexpr void obuffer_overflow(basic_omemory_map_ref<char_type>, char_type) noexcept
{
	fast_terminate();
}

template <::std::integral char_type>
inline constexpr bool obuffer_overflow_never(basic_omemory_map_ref<char_type>) noexcept
{
	return true;
}

/// @brief Completes a bulk write against the remaining mapped output extent.
/// @details The observer intentionally aliases its owning map so cursor progress survives public normalization. Its
///          put-area CPOs make the fitting path cheap, while this independent completion CPO proves the cold path used
///          by reserve, scatter, context, and semantic formatters. Checking the complete range before copying preserves
///          all-or-terminate behavior and accepts an exact fit; no partially advanced cursor can escape on overflow.
///          The unqualified-code-unit constraint prevents a merely structural `volatile` cursor protocol from proving
///          an output assignment that the primitive copy layer cannot perform.
template <::std::integral char_type>
	requires ::std::same_as<char_type, ::std::remove_cv_t<char_type>>
inline constexpr void write_all_overflow_define(
	basic_omemory_map_ref<char_type> map, char_type const *first, char_type const *last) noexcept
{
	// A zero-length write is a no-op even for a null observer or the null-pointer representation of a default-constructed
	// empty map. For a nonempty source, reject zero capacity before subtracting cursors: nullptr - nullptr is undefined
	// even though the pointers compare equal. Once both cursors are nonnull, the map invariant places them in one mapped
	// allocation; checking the signed distance before conversion prevents a reversed cursor from masquerading as a very
	// large remaining capacity.
	if (first == last)
	{
		return;
	}
	if (map.ptr == nullptr) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	auto const curr_ptr{map.ptr->curr_ptr};
	auto const end_ptr{map.ptr->end_ptr};
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
	map.ptr->curr_ptr = ::fast_io::details::non_overlapped_copy_n(first, count, curr_ptr);
}

using omemory_map = basic_omemory_map<char>;

template <::std::integral ch_type>
class basic_imemory_map
{
public:
	using char_type = ch_type;
	using input_char_type = char_type;
	char_type *begin_ptr{}, *curr_ptr{}, *end_ptr{};
	inline constexpr basic_imemory_map() = default;
	inline constexpr basic_imemory_map(native_memory_map_file const &iob, ::std::size_t offset = 0)
		: begin_ptr(reinterpret_cast<char_type *>(iob.address_begin + offset)), curr_ptr(this->begin_ptr),
		  end_ptr(this->begin_ptr + iob.size() / sizeof(char_type))
	{
	}
};

template <::std::integral ch_type>
struct basic_imemory_map_ref
{
	using input_char_type = ch_type;
	basic_imemory_map<ch_type> *ptr{};
};

template <::std::integral ch_type>
inline constexpr basic_imemory_map_ref<ch_type>
input_stream_ref_define(basic_imemory_map<ch_type> &other) noexcept
{
	// Retain cursor ownership in the public map object so multi-argument and subsequent scans observe one position.
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_imemory_map_ref<ch_type>
input_stream_ref_define(basic_imemory_map<ch_type> &&other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_imemory_map_ref<ch_type>
input_bytes_stream_ref_define(basic_imemory_map<ch_type> &other) noexcept
{
	return {__builtin_addressof(other)};
}

template <::std::integral ch_type>
inline constexpr basic_imemory_map_ref<ch_type>
input_bytes_stream_ref_define(basic_imemory_map<ch_type> &&other) noexcept
{
	return {__builtin_addressof(other)};
}

#if 0
template<::std::integral char_type,::std::contiguous_iterator Iter>
inline constexpr Iter read(basic_imemory_map<char_type>& bomp,Iter begin,Iter end) noexcept
{
	::std::size_t to_read(end-begin);
	::std::size_t const remain_space(bomp.end_ptr-bomp.curr_ptr);
	if(remain_space<to_read)[[unlikely]]
		to_read=remain_space;
	non_overlapped_copy_n(bomp.curr_ptr,to_read,begin);
	bomp.curr_ptr+=to_read;
	return begin+to_read;
}
#endif

template <::std::integral char_type>
inline constexpr char_type *ibuffer_begin(basic_imemory_map<char_type> &bomp) noexcept
{
	return bomp.begin_ptr;
}
template <::std::integral char_type>
inline constexpr char_type *ibuffer_curr(basic_imemory_map<char_type> &bomp) noexcept
{
	return bomp.curr_ptr;
}
template <::std::integral char_type>
inline constexpr char_type *ibuffer_end(basic_imemory_map<char_type> &bomp) noexcept
{
	return bomp.end_ptr;
}

template <::std::integral char_type>
inline constexpr bool ibuffer_underflow(basic_imemory_map<char_type> &) noexcept
{
	return false;
}

template <::std::integral char_type>
inline constexpr void ibuffer_set_curr(basic_imemory_map<char_type> &bomp, char_type *ptr) noexcept
{
	bomp.curr_ptr = ptr;
}

template <::std::integral char_type>
inline constexpr char_type *ibuffer_begin(basic_imemory_map_ref<char_type> bomp) noexcept
{
	return bomp.ptr->begin_ptr;
}

template <::std::integral char_type>
inline constexpr char_type *ibuffer_curr(basic_imemory_map_ref<char_type> bomp) noexcept
{
	return bomp.ptr->curr_ptr;
}

template <::std::integral char_type>
inline constexpr char_type *ibuffer_end(basic_imemory_map_ref<char_type> bomp) noexcept
{
	return bomp.ptr->end_ptr;
}

template <::std::integral char_type>
inline constexpr bool ibuffer_underflow(basic_imemory_map_ref<char_type>) noexcept
{
	return false;
}

template <::std::integral char_type>
inline constexpr void ibuffer_set_curr(basic_imemory_map_ref<char_type> bomp, char_type *ptr) noexcept
{
	bomp.ptr->curr_ptr = ptr;
}

template <::std::integral char_type>
inline constexpr bool ibuffer_underflow_never(basic_imemory_map_ref<char_type>) noexcept
{
	return true;
}

using imemory_map = basic_imemory_map<char>;

} // namespace fast_io
