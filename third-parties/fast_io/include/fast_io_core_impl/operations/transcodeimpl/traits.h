#pragma once

/**
 * @file
 * @brief Defines allocation and publication policy for stream transcoders.
 *
 * Sizes are expressed in elements of the corresponding endpoint type, never
 * in bytes. Traits are deliberately static: adapters can specialize storage
 * without adding a runtime policy object to every stream instance.
 */

namespace fast_io
{

namespace details
{

/** @brief Converts the configured byte budget to a nonzero endpoint-unit count. */
template <::fast_io::transcode_unit unit_type>
inline constexpr ::std::size_t default_transcode_buffer_size() noexcept
{
	// Preserve the library-wide byte budget while guaranteeing at least one
	// complete endpoint unit on targets with unusually wide integer types.
	constexpr ::std::size_t bytes{
#ifdef FAST_IO_BUFFER_SIZE
		// Honor the library-wide caller override when one is configured.
		FAST_IO_BUFFER_SIZE
#elif SIZE_MAX <= UINT_LEAST16_MAX
		// Keep allocations small on 16-bit address spaces.
		128u
#elif SIZE_MAX <= UINT_LEAST32_MAX
		// Use the established medium budget on 32-bit targets.
		8192u
#else
		// Favor amortized throughput on wide hosted address spaces.
		131072u
#endif
	};
	return bytes < sizeof(unit_type) ? 1u : bytes / sizeof(unit_type);
}

} // namespace details

/** @brief Defines allocation, size, and clearing policy for output adapters. */
template <::std::integral public_char, ::fast_io::transcode_unit transformed_unit,
		  typename allocator = ::fast_io::native_global_allocator>
struct basic_otranscoder_traits
{
	using allocator_type = allocator;
	using public_char_type = public_char;
	using transformed_unit_type = transformed_unit;

	static inline constexpr ::std::size_t public_buffer_size{
		::fast_io::details::default_transcode_buffer_size<public_char_type>()};
	static inline constexpr ::std::size_t transform_buffer_size{
		::fast_io::details::default_transcode_buffer_size<transformed_unit_type>()};
	static inline constexpr bool secure_clear = false;
};

/** @brief Controls when decoded input may become visible to its caller. */
enum class transcode_input_publication_mode : ::std::uint_least8_t
{
	// Low-latency streaming; authentication may still fail at terminal finish.
	streaming_unverified,
	// Reserved for adapters that retain a complete authenticated message.
	hold_until_authenticated
};

/** @brief Defines allocation, size, clearing, and publication policy for input. */
template <::std::integral public_char,
		  ::fast_io::transcode_unit source_unit,
		  ::fast_io::transcode_unit transformed_unit,
		  typename allocator = ::fast_io::native_global_allocator>
struct basic_itranscoder_traits
{
	using allocator_type = allocator;
	using public_char_type = public_char;
	using source_unit_type = source_unit;
	using transformed_unit_type = transformed_unit;

	static inline constexpr ::std::size_t public_buffer_size{
		::fast_io::details::default_transcode_buffer_size<public_char_type>()};
	static inline constexpr ::std::size_t source_buffer_size{
		::fast_io::details::default_transcode_buffer_size<source_unit_type>()};
	static inline constexpr ::std::size_t transform_buffer_size{
		::fast_io::details::default_transcode_buffer_size<transformed_unit_type>()};
	static inline constexpr bool secure_clear = false;
	static inline constexpr transcode_input_publication_mode publication_mode{
		transcode_input_publication_mode::streaming_unverified};
};

} // namespace fast_io
