#pragma once

namespace fast_io
{

// Chrono facet descriptors are compact RVAs owned by `basic_lc_object`, not process pointers. Size-only paths read the
// descriptor count directly because they observe no storage. Every materialization path resolves the selected RVA
// through `lc_resolve_scatter`, which both re-establishes the pointer and validates the closed interval before copying.
// Keeping that translation at the locale boundary leaves the ordinary chrono fallback and character kernels unchanged.

/// @feature concept:runtime_precise_size
template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(basic_lc_all<char_type> const *__restrict all,
												  ::std::chrono::weekday wkd) noexcept
{
	unsigned value(wkd.c_encoding());
	if (7 < value)
	{
		constexpr ::std::size_t unsigned_size{print_reserve_size(io_reserve_type<char_type, unsigned>)};
		return unsigned_size;
	}
	else
	{
		if (value == 7)
		{
			value = 0;
		}
		return all->time.day[value].length;
	}
}

template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(basic_lc_all<char_type> const *__restrict all, char_type *it,
												 ::std::chrono::weekday wkd) noexcept
{
	unsigned value{wkd.c_encoding()};
	if (7u < value)
	{
		return print_reserve_define(io_reserve_type<char_type, unsigned>, it, value);
	}
	else
	{
		if (value == 7u)
		{
			value = {};
		}
		auto const scatter{
			::fast_io::details::lc_resolve_scatter(all, all->time.day[value])};
		return details::non_overlapped_copy_n(scatter.base, scatter.len, it);
	}
}

template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(basic_lc_all<char_type> const *__restrict all,
												  ::std::chrono::month m) noexcept
{
	unsigned value(m);
	--value;
	if (value < 12u)
	{
		return all->time.mon[value].length;
	}
	else
	{
		constexpr ::std::size_t unsigned_size{print_reserve_size(io_reserve_type<char_type, unsigned>)};
		return unsigned_size;
	}
}

template <::std::random_access_iterator Iter, ::std::integral char_type>
inline constexpr Iter print_reserve_define(basic_lc_all<char_type> const *__restrict all, Iter iter,
										   ::std::chrono::month m) noexcept
{
	unsigned value(m);
	unsigned value1(value);
	--value;
	if (value < 12u)
	{
		auto const scatter{
			::fast_io::details::lc_resolve_scatter(all, all->time.mon[value])};
		return details::non_overlapped_copy_n(scatter.base, scatter.len, iter);
	}
	else
	{
		return print_reserve_define(io_reserve_type<char_type, unsigned>, iter, value1);
	}
}

/*
Referenced from IBM
LC_TIME Category for the Locale Definition Source File Format
https://www.ibm.com/support/knowledgecenter/ssw_aix_71/filesreference/LC_TIME.html
*/

namespace manipulators
{

/// @brief Internal carrier requesting an abbreviated locale spelling for a chrono field.
/// @details The wrapped field is interpreted by its locale-aware print customization; valid weekdays and months map to
///          the locale's abbreviated-name tables, while an out-of-range value is emitted numerically.
template <typename T>
struct abbr_t
{
	using manip_tag = manip_tag_t;
	T reference;
};

/// @brief Internal marker requesting the locale's alternative form of a wrapped chrono field.
/// @details This carrier does not print by itself. It composes with `abbr_t` so a month uses the locale's alternative
///          abbreviated month name, with the ordinary abbreviated name as the fallback when that entry is empty.
template <typename T>
struct alt_t
{
	using manip_tag = manip_tag_t;
	T reference;
};

/// @brief Internal carrier requesting locale alternative digits for a numeric chrono field.
/// @details Month and day use their unsigned numeric value; weekday uses its ISO encoding. If the locale has no entry
///          at that value, printing falls back to the field's ordinary numeric representation.
template <typename T>
struct alt_num_t
{
	using manip_tag = manip_tag_t;
	T reference;
};

/// @brief Internal locale carrier selecting the AM or PM designator.
/// @details A false reference selects locale entry zero (AM) and a true reference selects entry one (PM); the emitted
///          text is the locale-provided designator rather than a hard-coded ASCII token.
template <typename T>
struct am_pm_t
{
	using manip_tag = manip_tag_t;
	T reference;
};

/// @brief Selects the locale AM/PM designator.
/// @details `false` produces the locale's AM entry and `true` produces its PM entry when printed through locale-aware
///          output. The returned manipulator borrows its eventual text from the locale object.
inline constexpr am_pm_t<bool> am_pm(bool is_pm) noexcept
{
	return {is_pm};
}

/// @brief Requests the locale's abbreviated name for a weekday.
/// @details Sunday encodings zero and seven both select the Sunday entry. Invalid weekday encodings are printed as an
///          unsigned number instead of indexing a locale table.
inline constexpr abbr_t<::std::chrono::weekday> abbr(::std::chrono::weekday wkd) noexcept
{
	return {wkd};
}

/// @brief Requests the locale's abbreviated name for a month.
/// @details Valid months `1` through `12` select the corresponding locale entry; an invalid month is emitted using its
///          original unsigned numeric value.
inline constexpr abbr_t<::std::chrono::month> abbr(::std::chrono::month wkd) noexcept
{
	return {wkd};
}

/// @brief Requests the locale's alternative abbreviated name for a month.
/// @details Valid months use `ab_alt_mon` when that entry is nonempty and otherwise fall back to the ordinary `abmon`
///          entry. Invalid months are emitted numerically.
inline constexpr abbr_t<alt_t<::std::chrono::month>> abbr_alt(::std::chrono::month wkd) noexcept
{
	return {{wkd}};
}

/// @brief Requests locale alternative digits for a month, day, or weekday.
/// @details Month/day index the locale table by their unsigned value and weekday by ISO weekday number. A missing or
///          out-of-range table entry falls back to ordinary numeric printing of the original chrono object.
template <typename T>
	requires(::std::same_as<T, ::std::chrono::month> || ::std::same_as<T, ::std::chrono::day> ||
			 ::std::same_as<T, ::std::chrono::weekday>)
inline constexpr alt_num_t<T> alt_num(T m) noexcept
{
	return {m};
}

/// @brief Resolves the selected AM/PM locale entry as a scatter view.
/// @details This hidden print customization returns a borrowed view into the locale object: index zero is AM and index
///          one is PM. The caller must keep the locale object alive while consuming the scatter.
template <::std::integral char_type>
inline constexpr basic_io_scatter_t<char_type> print_scatter_define(basic_lc_all<char_type> const *__restrict all,
																	am_pm_t<bool> ampm) noexcept
{
	return ::fast_io::details::lc_resolve_scatter(all, all->time.am_pm[ampm.reference]);
}

/// @brief Declares locale AM/PM output to be a repeatable borrowed-scatter source.
/// @details The marker is intentionally restricted to `am_pm_t<bool>` because its two descriptors remain stable in the
///          enclosing locale object; it does not confer that property on arbitrary locale manipulators.
template <::std::integral char_type>
inline constexpr ::std::true_type print_lc_borrowed_scatter_source(
	io_reserve_type_t<char_type, am_pm_t<bool>>) noexcept
{
	// AM/PM names are stable descriptors in the enclosing locale object, and an unchanged boolean index is repeatable.
	// The marker is intentionally limited to this exact manipulator rather than inferred for every locale scatter.
	return {};
}

/// @brief Computes the emitted size of an abbreviated locale weekday.
/// @details Valid weekday encodings use the selected locale descriptor length, with encoding seven normalized to
///          Sunday; invalid encodings reserve the ordinary unsigned-integer bound.
template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(basic_lc_all<char_type> const *__restrict all,
												  abbr_t<::std::chrono::weekday> wkd) noexcept
{
	unsigned value(wkd.reference.c_encoding());
	if (7 < value)
	{
		constexpr ::std::size_t unsigned_size{print_reserve_size(io_reserve_type<char_type, unsigned>)};
		return unsigned_size;
	}
	else
	{
		if (value == 7)
		{
			value = 0;
		}
		return all->time.abday[value].length;
	}
}

/// @brief Emits an abbreviated locale weekday into caller-provided storage.
/// @details Valid encodings copy the resolved locale entry, with encoding seven normalized to Sunday. Invalid encodings
///          are emitted numerically and the returned pointer follows the last written code unit.
template <::std::integral char_type>
inline constexpr char_type *print_reserve_define(basic_lc_all<char_type> const *__restrict all, char_type *it,
												 abbr_t<::std::chrono::weekday> wkd) noexcept
{
	unsigned value(wkd.reference.c_encoding());
	if (7u < value)
	{
		return print_reserve_define(io_reserve_type<char_type, unsigned>, it, value);
	}
	else
	{
		if (value == 7u)
		{
			value = {};
		}
		auto const scatter{
			::fast_io::details::lc_resolve_scatter(all, all->time.abday[value])};
		return details::non_overlapped_copy_n(scatter.base, scatter.len, it);
	}
}

/// @brief Computes the emitted size of an abbreviated locale month.
/// @details Months `1` through `12` use the corresponding abbreviated-month descriptor length; invalid values reserve
///          the ordinary unsigned-integer bound.
template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(basic_lc_all<char_type> const *__restrict all,
												  abbr_t<::std::chrono::month> m) noexcept
{
	unsigned value(m.reference);
	--value;
	if (value < 12u)
	{
		return all->time.abmon[value].length;
	}
	else
	{
		constexpr ::std::size_t unsigned_size{print_reserve_size(io_reserve_type<char_type, unsigned>)};
		return unsigned_size;
	}
}

/// @brief Emits an abbreviated locale month into caller-provided storage.
/// @details A valid month copies its resolved `abmon` entry; an invalid month is printed as its original unsigned value.
///          The returned iterator follows the emitted representation.
template <::std::random_access_iterator Iter, ::std::integral char_type>
inline constexpr Iter print_reserve_define(basic_lc_all<char_type> const *__restrict all, Iter iter,
										   abbr_t<::std::chrono::month> m) noexcept
{
	unsigned value(m.reference);
	unsigned value1(value);
	--value;
	if (value < 12u)
	{
		auto const scatter{
			::fast_io::details::lc_resolve_scatter(all, all->time.abmon[value])};
		return details::non_overlapped_copy_n(scatter.base, scatter.len, iter);
	}
	else
	{
		return print_reserve_define(io_reserve_type<char_type, unsigned>, iter, value1);
	}
}

/// @brief Computes the emitted size of an alternative abbreviated locale month.
/// @details A valid month uses the alternative descriptor when nonempty and the ordinary abbreviated descriptor
///          otherwise. Invalid values reserve the ordinary unsigned-integer bound.
template <::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(basic_lc_all<char_type> const *__restrict all,
												  abbr_t<alt_t<::std::chrono::month>> m) noexcept
{
	unsigned value(m.reference.reference);
	--value;
	if (value < 12u)
	{
		if (all->time.ab_alt_mon[value].length == 0)
		{
			return all->time.abmon[value].length;
		}
		else
		{
			return all->time.ab_alt_mon[value].length;
		}
	}
	else
	{
		constexpr ::std::size_t unsigned_size{print_reserve_size(io_reserve_type<char_type, unsigned>)};
		return unsigned_size;
	}
}

/// @brief Emits an alternative abbreviated locale month into caller-provided storage.
/// @details A valid month copies `ab_alt_mon`, falling back to `abmon` for an empty alternative entry; an invalid month
///          is emitted numerically. The returned iterator follows the output.
template <::std::random_access_iterator Iter, ::std::integral char_type>
inline constexpr Iter print_reserve_define(basic_lc_all<char_type> const *__restrict all, Iter iter,
										   abbr_t<alt_t<::std::chrono::month>> m) noexcept
{
	unsigned value(m.reference.reference);
	unsigned value1(value);
	--value;
	if (value < 12u)
	{
		if (all->time.ab_alt_mon[value].length == 0)
		{
			auto const scatter{
				::fast_io::details::lc_resolve_scatter(all, all->time.abmon[value])};
			return details::non_overlapped_copy_n(scatter.base, scatter.len, iter);
		}
		else
		{
			auto const scatter{
				::fast_io::details::lc_resolve_scatter(all, all->time.ab_alt_mon[value])};
			return details::non_overlapped_copy_n(scatter.base, scatter.len, iter);
		}
	}
	else
	{
		return print_reserve_define(io_reserve_type<char_type, unsigned>, iter, value1);
	}
}

/// @brief Computes the emitted size of a chrono field rendered with locale alternative digits.
/// @details The field-specific numeric index selects an alternative-digit descriptor when available; otherwise the
///          ordinary numeric formatter's bound is returned.
template <::std::integral char_type, typename T>
	requires(::std::same_as<T, ::std::chrono::month> || ::std::same_as<T, ::std::chrono::day> ||
			 ::std::same_as<T, ::std::chrono::weekday>)
inline constexpr ::std::size_t print_reserve_size(basic_lc_all<char_type> const *__restrict all,
											  alt_num_t<T> m) noexcept
{
	using namespace ::std::chrono;
	// `alt_digits` is a relative descriptor into the locale's descriptor table. Resolve that outer table once; the
	// selected entry remains a compact character RVA, so a size query can read its count without touching character
	// storage. Materialization below performs the second, character-table resolution only for the selected entry.
	auto const alt_digits{
		::fast_io::details::lc_resolve_scatter(all, all->time.alt_digits)};
	if constexpr (::std::same_as<month, T> || ::std::same_as<day, T>)
	{
		unsigned value(m.reference);
		if (value < alt_digits.len)
		{
			return alt_digits.base[value].length;
		}
		else
		{
			constexpr ::std::size_t unsigned_size{print_reserve_size(io_reserve_type<char_type, T>)};
			return unsigned_size;
		}
	}
	else
	{
		unsigned value(m.reference.iso_encoding());
		if (value < alt_digits.len)
		{
			return alt_digits.base[value].length;
		}
		else
		{
			constexpr ::std::size_t unsigned_size{print_reserve_size(io_reserve_type<char_type, T>)};
			return unsigned_size;
		}
	}
}

/// @brief Emits a chrono field using locale alternative digits when available.
/// @details The selected alternative-digit descriptor is resolved and copied; an unavailable index falls back to the
///          original month, day, or weekday formatter. The returned iterator follows the emitted text.
template <::std::integral char_type, ::std::random_access_iterator Iter, typename T>
	requires(::std::same_as<T, ::std::chrono::month> || ::std::same_as<T, ::std::chrono::day> ||
			 ::std::same_as<T, ::std::chrono::weekday>)
inline constexpr Iter print_reserve_define(basic_lc_all<char_type> const *__restrict all, Iter iter,
										   alt_num_t<T> m) noexcept
{
	using namespace ::std::chrono;
	auto const alt_digits{
		::fast_io::details::lc_resolve_scatter(all, all->time.alt_digits)};
	if constexpr (::std::same_as<month, T> || ::std::same_as<day, T>)
	{
		unsigned value(m.reference);
		if (value < alt_digits.len)
		{
			auto const scatter{
				::fast_io::details::lc_resolve_scatter(all, alt_digits.base[value])};
			return details::non_overlapped_copy_n(scatter.base, scatter.len, iter);
		}
		else
		{
			return print_reserve_define(io_reserve_type<char_type, T>, iter, m.reference);
		}
	}
	else
	{
		unsigned value(m.reference.iso_encoding());
		if (value < alt_digits.len)
		{
			auto const scatter{
				::fast_io::details::lc_resolve_scatter(all, alt_digits.base[value])};
			return details::non_overlapped_copy_n(scatter.base, scatter.len, iter);
		}
		else
		{
			return print_reserve_define(io_reserve_type<char_type, T>, iter, m.reference);
		}
	}
}

#if 0
/// @brief Disabled size customization for locale-formatted `year_month_day` output.
/// @details The unfinished declaration estimates a bound from the locale date-format program and is excluded from all
///          builds; it currently produces no user-visible manipulator behavior.
template<::std::integral char_type>
inline constexpr ::std::size_t print_reserve_size(basic_lc_all<char_type> const* __restrict all,::std::chrono::year_month_day m) noexcept
{
	constexpr ::std::size_t unitsize{::fast_io::freestanding::max(print_reserve_size(io_reserve_type<char_type,int>),print_reserve_size(io_reserve_type<char_type,unsigned>))};
	return unitsize*all->time.d_fmt.len;
}

/// @brief Disabled emitter for locale-formatted `year_month_day` output.
/// @details This unfinished customization has no implementation and is excluded from compilation, so no formatting
///          result is currently defined by it.
template<::std::integral char_type>
inline constexpr char_type* print_reserve_define(basic_lc_all<char_type> const* __restrict all,char_type* iter,::std::chrono::year_month_day m) noexcept
{

}
#endif
} // namespace manipulators
} // namespace fast_io
