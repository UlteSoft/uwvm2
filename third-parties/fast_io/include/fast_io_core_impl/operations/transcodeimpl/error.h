#pragma once

/**
 * @file
 * @brief Defines stream-adapter errors owned by the transcode layer.
 *
 * Engine-defined data errors (invalid encoding, authentication failure,
 * padding failure, and similar conditions) propagate unchanged. This domain
 * is reserved for adapter misuse and violations of the bounded engine
 * protocol, which lets callers distinguish malformed transformed data from a
 * broken adapter/engine implementation.
 */

namespace fast_io
{

/** @brief Enumerates adapter-state and bounded-protocol failures. */
enum class transcode_stream_errc : ::std::uint_least8_t
{
	// An operation was attempted after finish, cancellation, or failure.
	invalid_state = 1,
	// A provider returned impossible cursors, status, or zero progress.
	protocol_violation,
	// A typed operation encountered a partial object representation.
	incomplete_unit
};

/** @brief Returns a stable platform-width-appropriate transcode error domain. */
inline constexpr ::std::size_t domain_define(error_type_t<transcode_stream_errc>) noexcept
{
	if constexpr (sizeof(::std::size_t) <= sizeof(::std::uint_least16_t))
	{
		// Use the compact domain constant on 16-bit size_t targets.
		return 20213u;
	}
	else if constexpr (sizeof(::std::size_t) <= sizeof(::std::uint_least32_t))
	{
		// Use the 32-bit domain constant when it fits exactly.
		return 1953657971u;
	}
	else
	{
		// Use the full 64-bit domain constant on wider hosted targets.
		return 8389754672734201972ULL;
	}
}

inline constexpr ::std::size_t transcode_stream_domain_value{
	domain_define(error_type<transcode_stream_errc>)};

/** @brief Tests whether a generic fast_io error represents a transcode code. */
inline constexpr bool equivalent_define(error_type_t<transcode_stream_errc>,
										error error_value,
										transcode_stream_errc code) noexcept
{
	return error_value.domain == transcode_stream_domain_value &&
		   error_value.code == static_cast<::std::size_t>(code);
}

/** @brief Raises a structured adapter error or terminates without exceptions. */
[[noreturn]] inline void throw_transcode_stream_error(
	transcode_stream_errc code)
{
	// Match fast_io's global no-exception policy: the same protocol failure
	// becomes a structured error with EH and a hard termination without EH.
#ifdef __cpp_exceptions
#if defined(_MSC_VER) && (!defined(_HAS_EXCEPTIONS) || _HAS_EXCEPTIONS == 0)
	// MSVC can define language EH while its runtime exception support is off.
	(void)code;
	::fast_io::fast_terminate();
#else
	// Propagate the adapter failure through fast_io's structured error type.
	throw ::fast_io::error{transcode_stream_domain_value,
						   static_cast<::std::size_t>(code)};
#endif
#else
	// Freestanding no-exception builds report protocol failures by termination.
	(void)code;
	::fast_io::fast_terminate();
#endif
}

} // namespace fast_io
