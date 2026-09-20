#pragma once

/*
Referenced from
https://github.com/sunfishcode/wasm-reference-manual/blob/master/WebAssembly.md#primitive-encoding-types
*/

namespace fast_io
{

namespace manipulators
{

/// @brief Serializes a floating value in WebAssembly's little-endian IEC 60559 binary form.
/// @details The output is the raw 32/64/etc. representation selected by `T`, not a textual floating spelling.
template <::std::floating_point T>
inline constexpr auto wasm_float_put(T t)
{
	return ::fast_io::manipulators::iec559_le_put(t);
}

/// @brief Deserializes a WebAssembly little-endian IEC 60559 floating value into `t`.
/// @details Exactly the destination representation width is consumed as binary bytes.
template <::std::floating_point T>
inline constexpr auto wasm_float_get(T &t)
{
	return ::fast_io::manipulators::iec559_le_get(t);
}

/// @brief Serializes an integer using WebAssembly signed/unsigned variable-integer encoding.
/// @details This delegates to signed or unsigned LEB128 according to `T` and emits binary continuation bytes.
template <::fast_io::details::my_integral T>
inline constexpr auto wasm_varint_put(T t)
{
	return ::fast_io::manipulators::leb128_put(t);
}

/// @brief Deserializes a WebAssembly variable integer into `t`.
/// @details Destination signedness selects signed versus unsigned LEB128 decoding.
template <::fast_io::details::my_integral T>
inline constexpr auto wasm_varint_get(T &t)
{
	return ::fast_io::manipulators::leb128_get(t);
}

/// @brief Serializes an integer as WebAssembly's fixed 32-bit little-endian field.
/// @details Exactly four binary bytes are emitted; out-of-range narrowing follows `le_put<32>` validation.
template <::fast_io::details::my_integral T>
inline constexpr auto wasm_uint32_put(T t)
{
	return ::fast_io::manipulators::le_put<32>(t);
}

/// @brief Deserializes a WebAssembly fixed 32-bit little-endian field into `t`.
/// @details Exactly four bytes are consumed and no textual integer grammar is involved.
template <::fast_io::details::my_integral T>
inline constexpr auto wasm_uint32_get(T &t)
{
	return ::fast_io::manipulators::le_get<32>(t);
}

/// @brief Serializes an integer as WebAssembly's fixed 64-bit little-endian field.
/// @details Exactly eight binary bytes are emitted; range handling follows `le_put<64>`.
template <::fast_io::details::my_integral T>
inline constexpr auto wasm_uint64_put(T t)
{
	return ::fast_io::manipulators::le_put<64>(t);
}

/// @brief Deserializes a WebAssembly fixed 64-bit little-endian field into `t`.
/// @details Exactly eight bytes are consumed as binary data.
template <::fast_io::details::my_integral T>
inline constexpr auto wasm_uint64_get(T &t)
{
	return ::fast_io::manipulators::le_get<64>(t);
}

} // namespace manipulators

} // namespace fast_io
