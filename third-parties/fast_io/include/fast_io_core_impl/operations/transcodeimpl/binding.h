#pragma once

/**
 * @file
 * @brief Defines public stream-character to engine-endpoint binding.
 *
 * Equal sizeof is intentionally not a binding rule. A public character domain
 * either matches the engine unit exactly, or the engine explicitly consumes or
 * produces the object representation as std::byte.
 */

namespace fast_io
{

/** @brief Selects direct typed access or explicit byte-object bridging. */
enum class transcode_unit_binding : ::std::uint_least8_t
{
	// Public characters and engine units are the same semantic C++ type.
	exact_units,
	// The engine endpoint is std::byte and observes object representations.
	object_bytes
};

/** @brief Detects endpoints that can share one typed code-unit representation. */
template <typename engine_unit, typename public_char>
concept transcode_exact_units_bindable =
	::fast_io::transcode_unit<engine_unit> &&
	::std::integral<::std::remove_cv_t<public_char>> &&
	::std::same_as<::std::remove_cv_t<engine_unit>,
				   ::std::remove_cv_t<public_char>>;

/** @brief Detects byte endpoints that may view complete public objects as bytes. */
template <typename engine_unit, typename public_char>
concept transcode_object_bytes_bindable =
	::std::same_as<::std::remove_cv_t<engine_unit>, ::std::byte> &&
	::std::integral<::std::remove_cv_t<public_char>>;

/** @brief Accepts endpoint/public pairs supported by either binding strategy. */
template <typename engine_unit, typename public_char>
concept transcode_automatically_bindable =
	transcode_exact_units_bindable<engine_unit, public_char> ||
	transcode_object_bytes_bindable<engine_unit, public_char>;

/** @brief Chooses the compile-time binding strategy for an endpoint pair. */
template <typename engine_unit, typename public_char>
	requires transcode_automatically_bindable<engine_unit, public_char>
inline constexpr transcode_unit_binding transcode_unit_binding_for() noexcept
{
	if constexpr (transcode_exact_units_bindable<engine_unit, public_char>)
	{
		// Equal unit types can be passed directly without representation bridging.
		return transcode_unit_binding::exact_units;
	}
	else
	{
		// A byte endpoint observes complete object representations explicitly.
		return transcode_unit_binding::object_bytes;
	}
}

} // namespace fast_io
