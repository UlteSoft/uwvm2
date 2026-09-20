#pragma once

/*
 * fast_io time-type bridge for chrono formatting (FMT level).
 *
 * Native fast_io timestamp/calendar values are classified and projected into
 * the common chrono-format state defined by `time.h`. This file is a source
 * adapter, not a clock service or output implementation; the resulting typed
 * representation continues through ordinary IO formatting and transfer.
 */

// Native fast_io calendar providers are layered on the common time grammar and
// emitter.  No standard-library chrono compatibility is provided here.
#include "time.h"

namespace fast_io::fmt::details
{

template <>
struct is_time_format_source<::fast_io::iso8601_timestamp> : ::std::true_type
{};

template <::std::int_least64_t offset_to_epoch>
struct is_time_format_source<::fast_io::basic_timestamp<offset_to_epoch>>
	: ::std::true_type
{};

[[nodiscard]] inline constexpr unsigned fast_io_time_days_in_month(
	::std::int_least64_t year, unsigned month) noexcept
{
	constexpr unsigned month_lengths[]{
		31u, 28u, 31u, 30u, 31u, 30u,
		31u, 31u, 30u, 31u, 30u, 31u};
	if (month < 1u || month > 12u)
	{
		::fast_io::fast_terminate();
	}
	return month_lengths[month - 1u] +
		   static_cast<unsigned>(month == 2u &&
								 chrono_is_gregorian_leap_year(year));
}

[[nodiscard]] inline constexpr unsigned fast_io_time_year_day(
	::fast_io::iso8601_timestamp const &value) noexcept
{
	constexpr unsigned days_before_month[]{
		0u, 31u, 59u, 90u, 120u, 151u,
		181u, 212u, 243u, 273u, 304u, 334u};
	if (value.day < 1u ||
		value.day > fast_io_time_days_in_month(value.year, value.month))
	{
		::fast_io::fast_terminate();
	}
	auto result{days_before_month[value.month - 1u] + value.day - 1u};
	if (value.month > 2u && chrono_is_gregorian_leap_year(value.year))
	{
		++result;
	}
	return result;
}

[[nodiscard]] inline constexpr unsigned fast_io_time_c_weekday(
	::fast_io::iso8601_timestamp const &value) noexcept
{
	// Gregorian weekdays repeat every 400 years.  Reducing first avoids every
	// overflow edge for the full int64_t native year domain, including Jan/Feb
	// of INT64_MIN.
	constexpr unsigned month_offsets[]{0u, 3u, 2u, 5u, 0u, 3u,
									   5u, 1u, 4u, 6u, 2u, 4u};
	if (value.month < 1u || value.month > 12u)
	{
		::fast_io::fast_terminate();
	}
	auto reduced_year{static_cast<::std::int_least64_t>(
		chrono_positive_modulo(value.year, 400u))};
	if (value.month < 3u)
	{
		reduced_year = reduced_year == 0 ? 399 : reduced_year - 1;
	}
	auto const total{reduced_year + reduced_year / 4 - reduced_year / 100 +
					 reduced_year / 400 + month_offsets[value.month - 1u] +
					 value.day};
	return chrono_positive_modulo(total, 7u);
}

template <bool has_utc_offset, bool has_time_zone_name>
[[nodiscard]] inline constexpr basic_chrono_calendar_state<
	has_utc_offset, has_time_zone_name>
make_chrono_calendar_state_from_iso(
	::fast_io::iso8601_timestamp const &value, unsigned fractional_precision,
	::std::int_least32_t utc_offset) noexcept
{
	basic_chrono_calendar_state<has_utc_offset, has_time_zone_name> state{};
	state.value = {value.year, value.month, value.day,
				   fast_io_time_c_weekday(value), fast_io_time_year_day(value),
				   value.hours, value.minutes, value.seconds};
	state.fractional_second = value.subseconds;
	state.fractional_precision = fractional_precision;
	state.utc_offset = utc_offset;
	return state;
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification>
[[nodiscard]] inline constexpr auto make_chrono_field(
	::fast_io::iso8601_timestamp const &value) noexcept
{
	auto state{make_chrono_calendar_state_from_iso<true, false>(
		value, ::std::numeric_limits<::std::uint_least64_t>::digits10,
		value.timezone)};
	return ::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, decltype(state)>{state};
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification,
		  ::std::int_least64_t offset_to_epoch>
[[nodiscard]] inline constexpr auto make_chrono_field(
	::fast_io::basic_timestamp<offset_to_epoch> value) noexcept
{
	// Normalize exactly once at the lowering boundary.  The printable owns the
	// resulting civil state, so reserve sizing and emission do not call utc().
	auto const iso{::fast_io::utc(value)};
	auto state{make_chrono_calendar_state_from_iso<true, true>(
		iso, ::std::numeric_limits<::std::uint_least64_t>::digits10, 0)};
	return ::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, decltype(state)>{state};
}

} // namespace fast_io::fmt::details
