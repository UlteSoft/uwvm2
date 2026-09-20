#pragma once

namespace fast_io
{

template <typename T>
class basic_io_buffer_ref
{
public:
	using io_buffer_type = T;
	using handle_type = typename io_buffer_type::handle_type;
	using traits_type = typename io_buffer_type::traits_type;
	using input_char_type = typename io_buffer_type::input_char_type;
	using output_char_type = typename io_buffer_type::output_char_type;
	using allocator_type = typename io_buffer_type::traits_type::allocator_type;
	using native_handle_type = io_buffer_type *;
	native_handle_type iobptr{};
};

/// @brief Preserves an owned input-buffer proof through the exact buffered-stream reference wrapper.
/// @details The wrapper stores only the address of its owner, and every input cursor operation reaches the same input
///          buffer member. Requiring the underlying marker prevents a similarly shaped custom wrapper or an unproved
///          allocator from acquiring provenance through pointer layout alone.
template <typename T>
	requires(::fast_io::prfch_cacheable_read_provenance<T>)
inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	io_type_t<basic_io_buffer_ref<T>>) noexcept
{
	return {};
}

/// @brief Write-direction propagation for the same exact buffered-stream reference wrapper.
/// @details Read and write are constrained independently so mode-specific aliases retain their minimum promise. The
///          reference remains non-owning; its established synchronous stream lifetime contract, not this marker, proves
///          that `iobptr` remains valid while a bounded put-area operation executes.
template <typename T>
	requires(::fast_io::prfch_cacheable_write_provenance<T>)
inline constexpr ::std::true_type prfch_cacheable_write_provenance_define(
	io_type_t<basic_io_buffer_ref<T>>) noexcept
{
	return {};
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::out) == buffer_mode::out &&
			 (iobuffertraits::mode & buffer_mode::in) == buffer_mode::in)
inline constexpr basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>>
io_stream_ref_define(basic_io_buffer<handletype, iobuffertraits> &&r) noexcept
{
	return {__builtin_addressof(r)};
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::out) == buffer_mode::out &&
			 (iobuffertraits::mode & buffer_mode::in) == buffer_mode::in)
inline constexpr basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>>
io_stream_ref_define(basic_io_buffer<handletype, iobuffertraits> &r) noexcept
{
	return {__builtin_addressof(r)};
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::out) == buffer_mode::out &&
			 (iobuffertraits::mode & buffer_mode::in) == buffer_mode::in)
inline constexpr basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>>
io_stream_ref_define(basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>> r) noexcept
{
	return r;
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::out) == buffer_mode::out &&
			 (iobuffertraits::mode & buffer_mode::in) == buffer_mode::in)
inline constexpr decltype(auto)
io_stream_deco_filter_ref_define(basic_io_buffer<handletype, iobuffertraits> &r) noexcept
{
	return io_stream_deco_filter_ref_define(r.handle);
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::out) == buffer_mode::out &&
			 (iobuffertraits::mode & buffer_mode::in) == buffer_mode::in)
inline constexpr decltype(auto)
io_stream_deco_filter_ref_define(basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>> r) noexcept
{
	return io_stream_deco_filter_ref_define(r.iobptr->handle);
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::out) == buffer_mode::out)
inline constexpr basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>>
output_stream_ref_define(basic_io_buffer<handletype, iobuffertraits> &&r) noexcept
{
	return {__builtin_addressof(r)};
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::out) == buffer_mode::out)
inline constexpr basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>>
output_stream_ref_define(basic_io_buffer<handletype, iobuffertraits> &r) noexcept
{
	return {__builtin_addressof(r)};
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::out) == buffer_mode::out)
inline constexpr basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>>
output_stream_ref_define(basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>> r) noexcept
{
	return r;
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::out) == buffer_mode::out)
inline constexpr decltype(auto)
output_stream_deco_filter_ref_define(basic_io_buffer<handletype, iobuffertraits> &r) noexcept
{
	return output_stream_deco_filter_ref_define(r.handle);
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::out) == buffer_mode::out)
inline constexpr decltype(auto)
output_stream_deco_filter_ref_define(basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>> r) noexcept
{
	return output_stream_deco_filter_ref_define(r.iobptr->handle);
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::in) == buffer_mode::in)
inline constexpr basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>>
input_stream_ref_define(basic_io_buffer<handletype, iobuffertraits> &&r) noexcept
{
	return {__builtin_addressof(r)};
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::in) == buffer_mode::in)
inline constexpr basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>>
input_stream_ref_define(basic_io_buffer<handletype, iobuffertraits> &r) noexcept
{
	return {__builtin_addressof(r)};
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::in) == buffer_mode::in)
inline constexpr basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>>
input_stream_ref_define(basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>> r) noexcept
{
	return r;
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::in) == buffer_mode::in)
inline constexpr decltype(auto)
input_stream_deco_filter_ref_define(basic_io_buffer<handletype, iobuffertraits> &r) noexcept
{
	return input_stream_deco_filter_ref_define(r.handle);
}

template <typename handletype, typename iobuffertraits>
	requires((iobuffertraits::mode & buffer_mode::in) == buffer_mode::in)
inline constexpr decltype(auto)
input_stream_deco_filter_ref_define(basic_io_buffer_ref<basic_io_buffer<handletype, iobuffertraits>> r) noexcept
{
	return input_stream_deco_filter_ref_define(r.iobptr->handle);
}

} // namespace fast_io
