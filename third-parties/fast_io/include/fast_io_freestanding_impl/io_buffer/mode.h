#pragma once

namespace fast_io
{

enum class buffer_mode
{
	in = 1 << 0,
	out = 1 << 1,
	tie = 1 << 2,
	io = in | out | tie,
	secure_clear = 1 << 3,
#if 0
line_buffering = (1<<4)|(1<<1),
#endif
};

inline constexpr buffer_mode operator&(buffer_mode x, buffer_mode y) noexcept
{
	using utype = typename ::std::underlying_type<buffer_mode>::type;
	return static_cast<buffer_mode>(static_cast<utype>(x) & static_cast<utype>(y));
}

inline constexpr buffer_mode operator|(buffer_mode x, buffer_mode y) noexcept
{
	using utype = typename ::std::underlying_type<buffer_mode>::type;
	return static_cast<buffer_mode>(static_cast<utype>(x) | static_cast<utype>(y));
}

inline constexpr buffer_mode operator^(buffer_mode x, buffer_mode y) noexcept
{
	using utype = typename ::std::underlying_type<buffer_mode>::type;
	return static_cast<buffer_mode>(static_cast<utype>(x) ^ static_cast<utype>(y));
}

inline constexpr buffer_mode operator~(buffer_mode x) noexcept
{
	using utype = typename ::std::underlying_type<buffer_mode>::type;
	return static_cast<buffer_mode>(~static_cast<utype>(x));
}

inline constexpr buffer_mode &operator&=(buffer_mode &x, buffer_mode y) noexcept
{
	return x = x & y;
}

inline constexpr buffer_mode &operator|=(buffer_mode &x, buffer_mode y) noexcept
{
	return x = x | y;
}

inline constexpr buffer_mode &operator^=(buffer_mode &x, buffer_mode y) noexcept
{
	return x = x ^ y;
}

template <typename char_type>
inline constexpr ::std::size_t io_default_buffer_size = details::cal_buffer_size<char_type, true>();

struct empty_buffer_pointers
{
};

template <typename T>
struct basic_io_buffer_pointers
{
	using value_type = T;
	using pointer = T *;
	pointer buffer_begin{}, buffer_curr{}, buffer_end{};
};

#if 0

template<typename T,::std::size_t N>
struct basic_io_buffer_pointers_on_stack
{
	using value_type = T;
	using pointer = T*;
	pointer buffer_curr{},buffer_end{};
	value_type buffer[N];
};

template<typename T>
struct basic_io_buffer_pointers_with_cap
{
	using value_type = T;
	using pointer = T*;
	pointer buffer_begin{},buffer_curr{},buffer_end{},buffer_cap{};
};

template<typename T>
struct basic_io_buffer_pointers_only_begin
{
	using value_type = T;
	using pointer = T*;
	pointer buffer_begin{};
};

#endif

template <typename T>
struct basic_io_buffer_pointers_no_curr
{
	using value_type = T;
	using pointer = T *;
	pointer buffer_begin{}, buffer_end{};
};

namespace details
{

template <typename char_type>
inline constexpr ::std::size_t compute_default_buffer_size() noexcept
{
	if constexpr (::std::same_as<char_type, void>)
	{
		return 0;
	}
	else if constexpr (sizeof(char_type) == 0)
	{
		return 0;
	}
	else
	{
		constexpr ::std::size_t buffersize{
#ifdef FAST_IO_BUFFER_SIZE
			// `FAST_IO_BUFFER_SIZE` is a byte budget, not an already-converted element count.  Preserve the complete
			// budget here and divide exactly once below; taking its minimum with `sizeof(char_type)` collapsed every
			// ordinary configured buffer to one element and could otherwise make a too-small budget produce zero.
			FAST_IO_BUFFER_SIZE
#elif SIZE_MAX <= UINT_LEAST16_MAX
			128
#elif SIZE_MAX <= UINT_LEAST32_MAX
			8192
#else
			131072
#endif
		};
		static_assert(sizeof(char_type) <= buffersize);
		return buffersize / sizeof(char_type);
	}
}

template <typename chtype>
inline constexpr ::std::size_t compute_default_input_output_buf_size(::fast_io::buffer_mode req,
																	 ::fast_io::buffer_mode m) noexcept
{
	if ((m & req) == req)
	{
		return ::fast_io::details::compute_default_buffer_size<chtype>();
	}
	return 0;
}

template <typename chtype>
inline constexpr bool compute_whether_possible_flag_for_buffer_mode(::fast_io::buffer_mode req,
																	::fast_io::buffer_mode m,
																	::std::size_t buffersize) noexcept
{
	if ((m & req) == req)
	{
		return buffersize != 0 && ::std::integral<chtype>;
	}
	else
	{
		return buffersize == 0 && ::std::same_as<chtype, void>;
	}
}

} // namespace details

template <typename char_type>
inline constexpr ::std::size_t default_buffer_size{::fast_io::details::compute_default_buffer_size<char_type>()};

template <buffer_mode mde, typename allocatortype, typename input_chtype, typename output_chtype,
		  ::std::size_t input_buf_size =
			  ::fast_io::details::compute_default_input_output_buf_size<input_chtype>(::fast_io::buffer_mode::in, mde),
		  ::std::size_t output_buf_size = ::fast_io::details::compute_default_input_output_buf_size<output_chtype>(
			  ::fast_io::buffer_mode::out, mde)>
	requires(::fast_io::details::compute_whether_possible_flag_for_buffer_mode<input_chtype>(::fast_io::buffer_mode::in,
																							 mde, input_buf_size) &&
			 ::fast_io::details::compute_whether_possible_flag_for_buffer_mode<output_chtype>(
				 ::fast_io::buffer_mode::out, mde, output_buf_size))
struct basic_io_buffer_traits
{
	using allocator_type = allocatortype;
	using input_char_type = input_chtype;
	using output_char_type = output_chtype;
	static inline constexpr buffer_mode mode = mde;
	static inline constexpr bool input_buffer_on_stack = false;
	static inline constexpr bool output_buffer_on_stack = false;
	static inline constexpr bool input_buffer_size_is_fixed = true;
	static inline constexpr bool output_buffer_size_is_fixed = true;
	static inline constexpr bool allocator_is_object = false;
	static inline constexpr ::std::size_t input_buffer_size = input_buf_size;
	static inline constexpr ::std::size_t output_buffer_size = output_buf_size;
};

namespace details
{

/// @brief Proves the allocator and direction required by an owned input-buffer layer.
/// @details `basic_io_buffer` obtains its input allocation through
///          `typed_generic_allocator_adapter<traits_type::allocator_type, input_char_type>`. The historical
///          `input_buffer_on_stack` policy member is not a storage-domain proof and the current owner contains pointer
///          state rather than an inline character array. Restricting this refinement to an enabled input mode plus an
///          exact allocator marker follows the allocation expression actually used by underflow. Null/unallocated
///          state is harmless; a caller must still prove a nonempty current range before issuing a hint.
template <typename traits_type>
concept prfch_cacheable_input_io_buffer_traits = requires {
	typename ::std::remove_cvref_t<traits_type>::allocator_type;
	requires((::std::remove_cvref_t<traits_type>::mode & ::fast_io::buffer_mode::in) ==
			 ::fast_io::buffer_mode::in);
	requires(::fast_io::prfch_cacheable_allocator_provenance<
			 typename ::std::remove_cvref_t<traits_type>::allocator_type>);
};

/// @brief Write-direction counterpart for an owned output-buffer layer.
/// @details Output allocation and destruction use the same allocator selected by the traits, but read and write
///          provenance remain separate so an input-only file buffer cannot enter a put-area policy and an output-only
///          file buffer cannot enter an input-consumption policy. Cursor capacity and lifetime are intentionally not
///          folded into this type-level proof; the eventual print call site must establish both at run time.
template <typename traits_type>
concept prfch_cacheable_output_io_buffer_traits = requires {
	typename ::std::remove_cvref_t<traits_type>::allocator_type;
	requires((::std::remove_cvref_t<traits_type>::mode & ::fast_io::buffer_mode::out) ==
			 ::fast_io::buffer_mode::out);
	requires(::fast_io::prfch_cacheable_allocator_provenance<
			 typename ::std::remove_cvref_t<traits_type>::allocator_type>);
};

} // namespace details

} // namespace fast_io
