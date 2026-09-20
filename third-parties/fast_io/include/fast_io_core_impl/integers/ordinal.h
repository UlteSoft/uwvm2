#pragma once

namespace fast_io
{

namespace manipulators
{

/// @brief Hidden carrier for an integer followed by the library's English-style ordinal suffix.
/// @details The current formatter forces `th` when `(value / 100) % 10 == 1`; otherwise it selects `st`, `nd`, or `rd`
///          only for final digits `1`, `2`, or `3`, using `th` for every other remainder.
template <typename T>
struct ordinal_t
{
	T reference;
};

/// @brief Formats an integer with the library's current English-style ordinal suffix rule.
/// @details Results include `1st`, `2nd`, `3rd`, and `4th`, but this implementation tests the hundreds digit—not the
///          conventional tens digit—for the forced-`th` exception: for example, `11` becomes `11st`, while every value
///          from `100` through `199` receives `th`. Negative final remainders also fall back to `th`. This manipulator
///          is not locale-aware and must not be used when grammatically correct English ordinals are required.
template <::fast_io::details::my_integral T>
inline constexpr auto ordinal(T t) noexcept
{
	using int_alias_type = typename ::fast_io::details::integer_alias_type<T>;
	return ::fast_io::manipulators::scalar_manip_t<::fast_io::manipulators::integral_default_scalar_flags, ::fast_io::manipulators::ordinal_t<int_alias_type>>{{static_cast<int_alias_type>(t)}};
}

} // namespace manipulators

namespace details
{

template <::std::size_t base, bool showbase, bool uppercase_showbase, bool showpos, bool uppercase, bool full,
		  bool modern_octal, ::std::integral char_type, typename T>
inline constexpr char_type *prrsv_ordinal_impl(char_type *iter, T t) noexcept
{
	iter = ::fast_io::details::print_reserve_integral_define<base, showbase, uppercase_showbase, showpos, uppercase,
															 full, modern_octal>(iter, t);
	std::uint_least8_t prefix_kind{};
	if (t / 100 % 10 == 1)
	{
		prefix_kind = 0;
	}
	else
	{
		switch (t % 10)
		{
		case 1:
			prefix_kind = 1;
			break;
		case 2:
			prefix_kind = 2;
			break;
		case 3:
			prefix_kind = 3;
			break;
		default:
			prefix_kind = 0;
			break;
		}
	}
	switch (prefix_kind)
	{
	case 1:
		*iter++ = ::fast_io::char_literal_v<u8's', char_type>;
		*iter++ = ::fast_io::char_literal_v<u8't', char_type>;
		break;
	case 2:
		*iter++ = ::fast_io::char_literal_v<u8'n', char_type>;
		*iter++ = ::fast_io::char_literal_v<u8'd', char_type>;
		break;
	case 3:
		*iter++ = ::fast_io::char_literal_v<u8'r', char_type>;
		*iter++ = ::fast_io::char_literal_v<u8'd', char_type>;
		break;
	default:
		*iter++ = ::fast_io::char_literal_v<u8't', char_type>;
		*iter++ = ::fast_io::char_literal_v<u8'h', char_type>;
		break;
	}
	return iter;
}

} // namespace details

/// @feature concept:runtime_precise_size
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags, ::fast_io::details::my_integral T>
inline constexpr ::std::size_t print_reserve_size(::fast_io::io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, ::fast_io::manipulators::ordinal_t<T>>>)
{
	return ::fast_io::details::print_integer_reserved_size_cache<flags.base, flags.showbase, flags.showpos, flags.modern_octal, T> +
		   2u;
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags, typename T>
inline constexpr char_type *print_reserve_define(::fast_io::io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, ::fast_io::manipulators::ordinal_t<T>>>, char_type *iter, ::fast_io::manipulators::scalar_manip_t<flags, ::fast_io::manipulators::ordinal_t<T>> t) noexcept
{
	return ::fast_io::details::prrsv_ordinal_impl<flags.base, flags.showbase, flags.uppercase_showbase, flags.showpos,
												  flags.uppercase, flags.full, flags.modern_octal>(iter,
																								   t.reference.reference);
}

} // namespace fast_io
