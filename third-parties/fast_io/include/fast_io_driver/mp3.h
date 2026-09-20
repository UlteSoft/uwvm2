#pragma once

#include <string.h>

namespace fast_io::mp3
{

/// @brief Decodes the 28 payload bits of an ID3v2 synchsafe integer.
/// @param nval The four on-disk octets assembled in big-endian numeric order.
/// @return `sum((octet[i] & 0x7f) << (7 * (3 - i)))` for `i` in `[0, 4)`.
/// @note Callers that consume an ID3 structure must separately reject octets whose high bit is set. Keeping validation
///       at the byte boundary prevents an invalid size field from being silently normalized by the masks below.
inline constexpr ::std::uint_least32_t decode_mp3_safe_int(::std::uint_least32_t nval) noexcept
{
	constexpr ::std::uint_least32_t payload_mask{0x7Fu};
	return static_cast<::std::uint_least32_t>(((nval >> 24u) & payload_mask) << 21u) |
		   static_cast<::std::uint_least32_t>(((nval >> 16u) & payload_mask) << 14u) |
		   static_cast<::std::uint_least32_t>(((nval >> 8u) & payload_mask) << 7u) |
		   static_cast<::std::uint_least32_t>(nval & payload_mask);
}

struct mp3_duration_result
{
	::std::uint_least64_t duration_in_milliseconds{}; // milliseconds
	::fast_io::parse_code code{::fast_io::parse_code::ok};
};

namespace details
{

/// @brief Converts an object-representation byte to its unsigned octet value without aliasing through `char`.
inline constexpr ::std::uint_least32_t mp3_byte_to_uint(::std::byte value) noexcept
{
	return ::std::to_integer<::std::uint_least32_t>(value);
}

/// @brief Loads exactly four ID3 octets without depending on host byte order or the width of `uint_least32_t`.
inline constexpr ::std::uint_least32_t load_mp3_big_endian_u32(::std::byte const *first) noexcept
{
	return static_cast<::std::uint_least32_t>(mp3_byte_to_uint(first[0]) << 24u) |
		   static_cast<::std::uint_least32_t>(mp3_byte_to_uint(first[1]) << 16u) |
		   static_cast<::std::uint_least32_t>(mp3_byte_to_uint(first[2]) << 8u) |
		   mp3_byte_to_uint(first[3]);
}

/// @brief Verifies the representation invariant `octet[i] < 0x80` required of every synchsafe size field.
inline constexpr bool is_valid_mp3_safe_int(::std::uint_least32_t value) noexcept
{
	constexpr ::std::uint_least32_t high_bit_mask{0x80808080u};
	return (value & high_bit_mask) == 0u;
}

/// @brief Recognizes one byte of the ID3 frame-identifier grammar `[A-Z0-9]`.
inline constexpr bool is_valid_mp3_frame_id_byte(::std::uint_least32_t value) noexcept
{
	return (static_cast<::std::uint_least32_t>('A') <= value && value <= static_cast<::std::uint_least32_t>('Z')) ||
		   (static_cast<::std::uint_least32_t>('0') <= value && value <= static_cast<::std::uint_least32_t>('9'));
}

/// @brief Recognizes a complete four-byte ID3v2.3/v2.4 frame identifier.
inline constexpr bool is_valid_mp3_frame_id(::std::byte const *first) noexcept
{
	return is_valid_mp3_frame_id_byte(mp3_byte_to_uint(first[0])) &&
		   is_valid_mp3_frame_id_byte(mp3_byte_to_uint(first[1])) &&
		   is_valid_mp3_frame_id_byte(mp3_byte_to_uint(first[2])) &&
		   is_valid_mp3_frame_id_byte(mp3_byte_to_uint(first[3]));
}

/// @brief Establishes that an entire bounded suffix is padding rather than a partial frame.
inline constexpr bool mp3_bytes_are_zero(::std::byte const *first, ::std::byte const *last) noexcept
{
	for (; first != last; ++first)
	{
		if (mp3_byte_to_uint(*first) != 0u)
		{
			return false;
		}
	}
	return true;
}

/// @brief Extends a base-10 duration while preserving the no-wrap invariant.
/// @return `ok` after consuming one ASCII digit, `invalid` for a non-digit, or `overflow` before multiplication would
///         exceed the result type. The caller therefore never observes a partially wrapped duration.
inline constexpr ::fast_io::parse_code append_mp3_duration_digit(
	::std::uint_least64_t &value, ::std::uint_least32_t code_unit) noexcept
{
	constexpr ::std::uint_least32_t zero{static_cast<::std::uint_least32_t>('0')};
	constexpr ::std::uint_least32_t nine{static_cast<::std::uint_least32_t>('9')};
	if (code_unit < zero || nine < code_unit)
	{
		return ::fast_io::parse_code::invalid;
	}
	auto const digit{static_cast<::std::uint_least64_t>(code_unit - zero)};
	constexpr auto maximum{(::std::numeric_limits<::std::uint_least64_t>::max)()};
	if (value > static_cast<::std::uint_least64_t>((maximum - digit) / 10u))
	{
		return ::fast_io::parse_code::overflow;
	}
	value = static_cast<::std::uint_least64_t>(value * 10u + digit);
	return ::fast_io::parse_code::ok;
}

/// @brief Parses the common single-byte representation of TLEN's nonnegative decimal integer.
/// @note A terminator is accepted only when every remaining byte is also zero; data outside `[first, last)` is never read.
inline constexpr mp3_duration_result parse_mp3_tlen_single_byte(
	::std::byte const *first, ::std::byte const *last) noexcept
{
	::std::uint_least64_t value{};
	bool has_digit{};
	for (; first != last; ++first)
	{
		auto const code_unit{mp3_byte_to_uint(*first)};
		if (code_unit == 0u)
		{
			if (!mp3_bytes_are_zero(first, last))
			{
				return {0u, ::fast_io::parse_code::invalid};
			}
			break;
		}
		auto const code{append_mp3_duration_digit(value, code_unit)};
		if (code != ::fast_io::parse_code::ok)
		{
			return {0u, code};
		}
		has_digit = true;
	}
	return has_digit ? mp3_duration_result{value, ::fast_io::parse_code::ok}
					 : mp3_duration_result{0u, ::fast_io::parse_code::invalid};
}

/// @brief Parses TLEN decimal digits represented as bounded UTF-16 code units in the selected byte order.
/// @note TLEN admits only ASCII digits, so surrogate handling is deliberately unnecessary and every other code unit is invalid.
inline constexpr mp3_duration_result parse_mp3_tlen_utf16(
	::std::byte const *first, ::std::byte const *last, bool little_endian) noexcept
{
	auto const size{static_cast<::std::size_t>(last - first)};
	if ((size & 1u) != 0u)
	{
		return {0u, ::fast_io::parse_code::invalid};
	}
	::std::uint_least64_t value{};
	bool has_digit{};
	for (; first != last; first += 2)
	{
		auto const first_octet{mp3_byte_to_uint(first[0])};
		auto const second_octet{mp3_byte_to_uint(first[1])};
		auto const code_unit{little_endian
								 ? static_cast<::std::uint_least32_t>(first_octet | (second_octet << 8u))
								 : static_cast<::std::uint_least32_t>((first_octet << 8u) | second_octet)};
		if (code_unit == 0u)
		{
			if (!mp3_bytes_are_zero(first, last))
			{
				return {0u, ::fast_io::parse_code::invalid};
			}
			break;
		}
		auto const code{append_mp3_duration_digit(value, code_unit)};
		if (code != ::fast_io::parse_code::ok)
		{
			return {0u, code};
		}
		has_digit = true;
	}
	return has_digit ? mp3_duration_result{value, ::fast_io::parse_code::ok}
					 : mp3_duration_result{0u, ::fast_io::parse_code::invalid};
}

/// @brief Parses the payload of an uncompressed, unencrypted TLEN text frame within its declared frame boundary.
inline constexpr mp3_duration_result parse_mp3_tlen(
	::std::byte const *first, ::std::byte const *last, ::std::uint_least32_t major_version) noexcept
{
	if (first == last)
	{
		return {0u, ::fast_io::parse_code::invalid};
	}
	auto const encoding{mp3_byte_to_uint(*first++)};
	if (encoding == 0u || (major_version == 4u && encoding == 3u))
	{
		// ISO-8859-1 is common to both versions; UTF-8 (selector 3) was introduced by ID3v2.4.
		return parse_mp3_tlen_single_byte(first, last);
	}
	if (major_version == 4u && encoding == 2u)
	{
		// ID3v2.4 encoding 2 is UTF-16BE without a byte-order mark.
		return parse_mp3_tlen_utf16(first, last, false);
	}
	if (encoding != 1u || static_cast<::std::size_t>(last - first) < 2u)
	{
		return {0u, ::fast_io::parse_code::invalid};
	}
	auto const bom0{mp3_byte_to_uint(first[0])};
	auto const bom1{mp3_byte_to_uint(first[1])};
	first += 2;
	if (bom0 == 0xFFu && bom1 == 0xFEu)
	{
		return parse_mp3_tlen_utf16(first, last, true);
	}
	if (bom0 == 0xFEu && bom1 == 0xFFu)
	{
		return parse_mp3_tlen_utf16(first, last, false);
	}
	return {0u, ::fast_io::parse_code::invalid};
}

} // namespace details

/// @brief Finds an ID3v2.3/v2.4 TLEN frame and returns its bounded decimal duration.
/// @pre `first` and `last` delimit one valid contiguous byte range with `first <= last`.
/// @details The parser maintains two nested bounds: `tag_end` is derived from the tag header, and `frame_end` is derived
///          from a validated frame size. No field or payload parser can advance beyond either enclosing boundary.
inline mp3_duration_result compute_mp3_duration(void const *first, void const *last) noexcept
{
	::std::byte const *firstbyte{reinterpret_cast<::std::byte const *>(first)};
	::std::byte const *lastbyte{reinterpret_cast<::std::byte const *>(last)};
	::std::byte const *tag_header{firstbyte};
	constexpr ::std::size_t mp3headersize{10u};
	if (static_cast<::std::size_t>(lastbyte - firstbyte) < mp3headersize)
	{
		return {0u, ::fast_io::parse_code::end_of_file};
	}
	if (::memcmp(firstbyte, u8"ID3", 3u) != 0)
	{
		return {0u, ::fast_io::parse_code::end_of_file};
	}

	auto const major_version{details::mp3_byte_to_uint(firstbyte[3])};
	if (major_version != 3u && major_version != 4u)
	{
		// ID3v2.2 uses six-byte frame headers and three-byte frame identifiers; this parser intentionally handles v2.3/2.4.
		return {0u, ::fast_io::parse_code::invalid};
	}
	if (details::mp3_byte_to_uint(firstbyte[4]) == 0xFFu)
	{
		// The ID3 version tuple reserves 0xff; accepting it would make the frame grammar version-ambiguous.
		return {0u, ::fast_io::parse_code::invalid};
	}
	auto const header_flags{details::mp3_byte_to_uint(firstbyte[5])};
	auto const reserved_header_flag_mask{major_version == 3u ? 0x1Fu : 0x0Fu};
	if ((header_flags & reserved_header_flag_mask) != 0u)
	{
		return {0u, ::fast_io::parse_code::invalid};
	}
	if ((header_flags & 0x80u) != 0u)
	{
		// The encoded byte stream no longer preserves raw frame offsets after tag-level unsynchronisation. Until a bounded
		// de-unsynchronisation stage is inserted, rejecting the tag is the only way to preserve the cursor invariant.
		return {0u, ::fast_io::parse_code::invalid};
	}
	auto const encoded_tag_size{details::load_mp3_big_endian_u32(firstbyte + 6)};
	if (!details::is_valid_mp3_safe_int(encoded_tag_size))
	{
		return {0u, ::fast_io::parse_code::invalid};
	}
	auto const tag_size{static_cast<::std::size_t>(decode_mp3_safe_int(encoded_tag_size))};
	firstbyte += mp3headersize;
	if (static_cast<::std::size_t>(lastbyte - firstbyte) < tag_size)
	{
		return {0u, ::fast_io::parse_code::end_of_file};
	}
	::std::byte const *tag_end{firstbyte + tag_size};
	bool const footer_present{(header_flags & 0x10u) != 0u};
	if (footer_present)
	{
		// In ID3v2.4 the size field excludes both the ten-byte header and the optional ten-byte footer. Consequently the
		// footer starts exactly at `tag_end`, and all seven copied metadata bytes must agree with the leading header.
		if (static_cast<::std::size_t>(lastbyte - tag_end) < mp3headersize)
		{
			return {0u, ::fast_io::parse_code::end_of_file};
		}
		if (::memcmp(tag_end, u8"3DI", 3u) != 0 || ::memcmp(tag_end + 3, tag_header + 3, 7u) != 0)
		{
			return {0u, ::fast_io::parse_code::invalid};
		}
	}

	::std::byte const *frame_area_end{tag_end};
	bool v23_padding_is_declared{};
	if ((header_flags & 0x40u) != 0u)
	{
		constexpr ::std::size_t extended_size_field_size{4u};
		if (static_cast<::std::size_t>(tag_end - firstbyte) < extended_size_field_size)
		{
			return {0u, ::fast_io::parse_code::invalid};
		}
		auto const encoded_extended_size{details::load_mp3_big_endian_u32(firstbyte)};
		::std::size_t extended_size{};
		if (major_version == 3u)
		{
			// ID3v2.3 excludes the four-byte size field itself. Its payload is exactly the two flag bytes plus the
			// four-byte padding size, followed by four CRC bytes iff the sole defined extended flag is set.
			auto const payload_size{static_cast<::std::size_t>(encoded_extended_size)};
			if (payload_size < 6u || payload_size > static_cast<::std::size_t>(tag_end - firstbyte) - 4u)
			{
				return {0u, ::fast_io::parse_code::invalid};
			}
			extended_size = payload_size + 4u;
			auto const extended_flags0{details::mp3_byte_to_uint(firstbyte[4])};
			auto const extended_flags1{details::mp3_byte_to_uint(firstbyte[5])};
			if ((extended_flags0 & 0x7Fu) != 0u || extended_flags1 != 0u)
			{
				return {0u, ::fast_io::parse_code::invalid};
			}
			bool const crc_present{(extended_flags0 & 0x80u) != 0u};
			auto const required_payload_size{crc_present ? 10u : 6u};
			if (payload_size != required_payload_size)
			{
				// The CRC flag and extended-header size jointly determine whether the four CRC octets exist. Treating
				// either field independently would shift the first frame by four bytes on a contradictory header.
				return {0u, ::fast_io::parse_code::invalid};
			}
			auto const padding_size{static_cast<::std::size_t>(details::load_mp3_big_endian_u32(firstbyte + 6))};
			::std::byte const *extended_end{firstbyte + extended_size};
			if (padding_size > static_cast<::std::size_t>(tag_end - extended_end))
			{
				return {0u, ::fast_io::parse_code::invalid};
			}
			frame_area_end = tag_end - padding_size;
			if (!details::mp3_bytes_are_zero(frame_area_end, tag_end))
			{
				return {0u, ::fast_io::parse_code::invalid};
			}
			v23_padding_is_declared = true;
		}
		else
		{
			// ID3v2.4 stores the complete extended-header size, including this synchsafe size field.
			if (!details::is_valid_mp3_safe_int(encoded_extended_size))
			{
				return {0u, ::fast_io::parse_code::invalid};
			}
			extended_size = static_cast<::std::size_t>(decode_mp3_safe_int(encoded_extended_size));
			if (extended_size < 6u || static_cast<::std::size_t>(tag_end - firstbyte) < extended_size)
			{
				return {0u, ::fast_io::parse_code::invalid};
			}

			::std::byte const *extended_end{firstbyte + extended_size};
			if (details::mp3_byte_to_uint(firstbyte[4]) != 1u)
			{
				// ID3v2.4.0 defines exactly one extended-flag byte; accepting any other count makes the attached-data
				// cursor version-dependent and prevents a proof that it reaches `extended_end` exactly.
				return {0u, ::fast_io::parse_code::invalid};
			}
			auto const extended_flags{details::mp3_byte_to_uint(firstbyte[5])};
			if ((extended_flags & 0x8Fu) != 0u)
			{
				return {0u, ::fast_io::parse_code::invalid};
			}
			::std::byte const *extended_cursor{firstbyte + 6u};
			auto consume_flag_data = [&extended_cursor, extended_end](::std::size_t expected_size) noexcept {
				if (extended_cursor == extended_end ||
					static_cast<::std::size_t>(extended_end - extended_cursor) - 1u < expected_size ||
					details::mp3_byte_to_uint(*extended_cursor) != expected_size)
				{
					return static_cast<::std::byte const *>(nullptr);
				}
				++extended_cursor;
				auto const data{extended_cursor};
				extended_cursor += expected_size;
				return data;
			};
			if ((extended_flags & 0x40u) != 0u && consume_flag_data(0u) == nullptr)
			{
				return {0u, ::fast_io::parse_code::invalid};
			}
			if ((extended_flags & 0x20u) != 0u)
			{
				auto const crc_data{consume_flag_data(5u)};
				if (crc_data == nullptr || details::mp3_byte_to_uint(crc_data[0]) > 0x0Fu)
				{
					return {0u, ::fast_io::parse_code::invalid};
				}
				for (::std::size_t index{1u}; index != 5u; ++index)
				{
					if (details::mp3_byte_to_uint(crc_data[index]) >= 0x80u)
					{
						return {0u, ::fast_io::parse_code::invalid};
					}
				}
			}
			if ((extended_flags & 0x10u) != 0u && consume_flag_data(1u) == nullptr)
			{
				return {0u, ::fast_io::parse_code::invalid};
			}
			if (extended_cursor != extended_end)
			{
				// Every set flag owns one length-prefixed field, and unset/unknown flags own no data. Equality is the
				// formal completeness condition that rejects both truncated fields and unexplained trailing octets.
				return {0u, ::fast_io::parse_code::invalid};
			}
		}
		firstbyte += extended_size;
	}

	mp3_duration_result duration_result{};
	bool duration_found{};
	bool frame_found{};
	constexpr ::std::size_t mp3frameheadersize{10u};
	while (mp3frameheadersize <= static_cast<::std::size_t>(frame_area_end - firstbyte))
	{
		// Loop invariant: `[firstbyte, frame_area_end)` is the unconsumed frame region, so every fixed-offset
		// access below is dominated by the ten-byte frame-header check above.
		if (details::mp3_bytes_are_zero(firstbyte, firstbyte + 4))
		{
			if (!details::mp3_bytes_are_zero(firstbyte, frame_area_end) || v23_padding_is_declared || footer_present)
			{
				return {0u, ::fast_io::parse_code::invalid};
			}
			firstbyte = frame_area_end;
			break;
		}
		if (!details::is_valid_mp3_frame_id(firstbyte))
		{
			return {0u, ::fast_io::parse_code::invalid};
		}

		auto const encoded_frame_size{details::load_mp3_big_endian_u32(firstbyte + 4)};
		auto const status_flags{details::mp3_byte_to_uint(firstbyte[8])};
		auto const format_flags{details::mp3_byte_to_uint(firstbyte[9])};
		auto const reserved_status_flag_mask{major_version == 3u ? 0x1Fu : 0x8Fu};
		auto const reserved_format_flag_mask{major_version == 3u ? 0x1Fu : 0xB0u};
		if ((status_flags & reserved_status_flag_mask) != 0u ||
			(format_flags & reserved_format_flag_mask) != 0u)
		{
			// Reserved frame bits have no defined payload or lifecycle semantics in the selected major version.
			return {0u, ::fast_io::parse_code::invalid};
		}
		::std::size_t frame_size{};
		if (major_version == 4u)
		{
			if (!details::is_valid_mp3_safe_int(encoded_frame_size))
			{
				return {0u, ::fast_io::parse_code::invalid};
			}
			frame_size = static_cast<::std::size_t>(decode_mp3_safe_int(encoded_frame_size));
		}
		else
		{
			frame_size = static_cast<::std::size_t>(encoded_frame_size);
		}
		if (frame_size == 0u || static_cast<::std::size_t>(frame_area_end - firstbyte) - mp3frameheadersize < frame_size)
		{
			return {0u, ::fast_io::parse_code::invalid};
		}

		::std::byte const *payload{firstbyte + mp3frameheadersize};
		::std::byte const *frame_end{payload + frame_size};
		frame_found = true;
		if (::memcmp(firstbyte, u8"TLEN", 4u) == 0)
		{
			if (duration_found)
			{
				// Text information frame identifiers are singleton keys; a second TLEN is structurally ambiguous.
				return {0u, ::fast_io::parse_code::invalid};
			}
			auto const payload_transform_mask{major_version == 3u ? 0xE0u : 0x4Fu};
			if ((format_flags & payload_transform_mask) != 0u)
			{
				// Grouping, compression, encryption, per-frame unsynchronisation, or a data-length prefix changes the payload
				// layout. Reporting invalid is safer than interpreting transform metadata as a decimal duration.
				return {0u, ::fast_io::parse_code::invalid};
			}
			auto const parsed_duration{details::parse_mp3_tlen(payload, frame_end, major_version)};
			if (parsed_duration.code != ::fast_io::parse_code::ok)
			{
				return parsed_duration;
			}
			duration_result = parsed_duration;
			duration_found = true;
		}
		firstbyte = frame_end;
	}

	// Fewer than ten residual bytes are legal only as zero-filled padding; nonzero residue is a truncated frame header.
	if (firstbyte != frame_area_end)
	{
		if (!details::mp3_bytes_are_zero(firstbyte, frame_area_end) || v23_padding_is_declared || footer_present)
		{
			return {0u, ::fast_io::parse_code::invalid};
		}
	}
	if (!frame_found)
	{
		// Both ID3v2.3 and ID3v2.4 require at least one nonempty frame; an empty or padding-only body is not a tag.
		return {0u, ::fast_io::parse_code::invalid};
	}
	return duration_result;
}

} // namespace fast_io::mp3
