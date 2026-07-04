#pragma once

#include "punning.h"
#include "hexfloat.h"
#include "decfloat.h"
#include "roundtrip.h"

namespace fast_io
{

namespace details
{

template <typename T>
concept print_floating_has_iec559_traits = requires {
	typename ::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>::mantissa_type;
};

template <typename T>
inline constexpr bool print_floating_decimal_direct_supported{
	::std::same_as<::std::remove_cvref_t<T>, float> || ::std::same_as<::std::remove_cvref_t<T>, double>
#ifdef __STDCPP_FLOAT32_T__
	|| ::std::same_as<::std::remove_cvref_t<T>, _Float32>
#endif
#ifdef __STDCPP_FLOAT64_T__
	|| ::std::same_as<::std::remove_cvref_t<T>, _Float64>
#endif
};

template <typename T, bool = ::fast_io::details::print_floating_has_iec559_traits<T>>
struct print_floating_decimal_via_float_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct print_floating_decimal_via_float_impl<T, true>
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	inline static constexpr bool value{
		!::fast_io::details::print_floating_decimal_direct_supported<no_cvref_t> &&
		trait::mbits <= ::fast_io::details::iec559_traits<float>::mbits &&
		trait::ebits <= ::fast_io::details::iec559_traits<float>::ebits};
};

template <typename T>
inline constexpr bool print_floating_decimal_via_float{
	::fast_io::details::print_floating_decimal_via_float_impl<T>::value};

} // namespace details

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(flags.base == 10)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating ||
				  manipulators::floating_format::hexfloat == flags.floating);
	using trait = ::fast_io::details::iec559_traits<flt>;
	if constexpr (flags.floating == manipulators::floating_format::hexfloat)
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
					  || ::std::same_as<::std::remove_cvref_t<flt>, __float128>
#endif
		)
		{
#ifdef __SIZEOF_FLOAT80__
			if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> && sizeof(flt) == sizeof(__float80) &&
						  sizeof(flt) > sizeof(double))
			{
				return details::print_rsv_fp_size_with_special_cache<
					details::print_rsvhexfloat_size_cache<flags.showbase,
														  typename details::iec559_traits<__float80>::mantissa_type>,
					flags.nan_show_type>;
			}
			else
#endif
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
				if constexpr (sizeof(flt) > sizeof(double))
			{
				return details::print_rsv_fp_size_with_special_cache<
					details::print_rsvhexfloat_size_cache<flags.showbase, __uint128_t>, flags.nan_show_type>;
			}
			else
#endif
				return details::print_rsv_fp_size_with_special_cache<
					details::print_rsvhexfloat_size_cache<flags.showbase,
														  typename details::iec559_traits<double>::mantissa_type>,
					flags.nan_show_type>;
		}
		else
		{
			return details::print_rsv_fp_size_with_special_cache<
				details::print_rsvhexfloat_size_cache<flags.showbase, typename trait::mantissa_type>,
				flags.nan_show_type>;
		}
	}
	else
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
					  sizeof(flt) == sizeof(double)) // this is the case on xxx-windows-msvc
		{
			return details::print_rsv_fp_size_with_special_cache<details::print_rsv_cache<double, flags.floating>,
																 flags.nan_show_type>;
		}
		else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
		{
			return details::print_rsv_fp_size_with_special_cache<details::print_rsv_cache<float, flags.floating>,
																 flags.nan_show_type>;
		}
		else
		{
			static_assert(::fast_io::details::print_floating_decimal_direct_supported<flt>,
						  "currently only support iec559 float32 and float64 decimal output; narrower IEC559 "
						  "formats are printed through float");
			return details::print_rsv_fp_size_with_special_cache<
				details::print_rsv_cache<::std::remove_cvref_t<flt>, flags.floating>, flags.nan_show_type>;
		}
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(flags.base == 10)
inline constexpr char_type *print_reserve_define(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>,
												 char_type *iter, manipulators::scalar_manip_t<flags, flt> f) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating ||
				  manipulators::floating_format::hexfloat == flags.floating);
	if constexpr (flags.floating == manipulators::floating_format::hexfloat)
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
					  || ::std::same_as<::std::remove_cvref_t<flt>, __float128>
#endif
		)
		{
#ifdef __SIZEOF_FLOAT80__
			if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> && sizeof(flt) == sizeof(__float80) &&
						  sizeof(flt) > sizeof(double))
			{
				return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
															  flags.uppercase, flags.uppercase_e, flags.comma,
															  flags.nan_show_sign, flags.nan_show_type>(
					iter, static_cast<__float80>(f.reference));
			}
			else
#endif
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
				if constexpr (sizeof(flt) > sizeof(double))
			{
				return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
															  flags.uppercase, flags.uppercase_e, flags.comma,
															  flags.nan_show_sign, flags.nan_show_type>(
					iter, static_cast<__float128>(f.reference));
			}
			else
#endif
				return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
															  flags.uppercase, flags.uppercase_e, flags.comma,
															  flags.nan_show_sign, flags.nan_show_type>(
					iter, static_cast<double>(f.reference));
		}
		else
		{
			return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
														  flags.uppercase, flags.uppercase_e, flags.comma,
														  flags.nan_show_sign, flags.nan_show_type>(iter,
																									f.reference);
		}
	}
	else
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
					  sizeof(flt) == sizeof(double)) // this is the case on xxx-windows-msvc
		{
			return details::print_rsvflt_define_impl<flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
													 flags.floating, flags.rounding, flags.nan_show_sign,
													 flags.nan_show_type>(
				iter, static_cast<double>(f.reference));
		}
		else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
		{
			return details::print_rsvflt_define_impl<flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
													 flags.floating, flags.rounding, flags.nan_show_sign,
													 flags.nan_show_type>(
				iter, static_cast<float>(f.reference));
		}
		else
		{
			// this is the case for every other platform, including xxx-windows-gnu
			static_assert(::fast_io::details::print_floating_decimal_direct_supported<flt>,
						  "currently only support iec559 float32 and float64 decimal output; narrower IEC559 "
						  "formats are printed through float");
			return details::print_rsvflt_define_impl<flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
													 flags.floating, flags.rounding, flags.nan_show_sign,
													 flags.nan_show_type>(iter, f.reference);
		}
	}
}

#if 0
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(flags.base == 10)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating ||
				  manipulators::floating_format::hexfloat == flags.floating);
	using trait = ::fast_io::details::iec559_traits<flt>;
	if constexpr (flags.floating == manipulators::floating_format::hexfloat)
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
					  || ::std::same_as<::std::remove_cvref_t<flt>, __float128>
#endif
		)
		{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
			if constexpr (sizeof(flt) > sizeof(double))
			{
				return details::print_rsvhexfloat_size_cache<flags.showbase, __uint128_t>;
			}
			else
#endif
				return details::print_rsvhexfloat_size_cache<flags.showbase,
															 typename details::iec559_traits<double>::mantissa_type>;
		}
		else
		{
			return details::print_rsvhexfloat_size_cache<flags.showbase, typename trait::mantissa_type>;
		}
	}
	else
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
					  sizeof(flt) == sizeof(double)) // this is the case on xxx-windows-msvc
		{
			return details::print_rsv_cache<double, flags.floating>;
		}
		static_assert((::std::same_as<::std::remove_cvref_t<flt>, double> ||
					   ::std::same_as<::std::remove_cvref_t<flt>, float>
#ifdef __STDCPP_FLOAT32_T__
					   || ::std::same_as<::std::remove_cvref_t<flt>, _Float32>
#endif
#ifdef __STDCPP_FLOAT64_T__
					   || ::std::same_as<::std::remove_cvref_t<flt>, _Float64>
#endif
					   ),
					  "currently only support iec559 float32 and float64, sorry");
		return details::print_rsv_cache<::std::remove_cvref_t<flt>, flags.floating>;
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(flags.base == 10)
inline constexpr char_type *print_reserve_define(io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>,
												 char_type *iter, manipulators::scalar_manip_precision_t<flags, flt> f) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating ||
				  manipulators::floating_format::hexfloat == flags.floating);
	if constexpr (flags.floating == manipulators::floating_format::hexfloat)
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
					  || ::std::same_as<::std::remove_cvref_t<flt>, __float128>
#endif
		)
		{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
			if constexpr (sizeof(flt) > sizeof(double))
			{
				return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
															  flags.uppercase, flags.uppercase_e, flags.comma,
															  flags.nan_show_sign, flags.nan_show_type>(
					iter, static_cast<__float128>(f.reference));
			}
			else
#endif
				return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
															  flags.uppercase, flags.uppercase_e, flags.comma,
															  flags.nan_show_sign, flags.nan_show_type>(
					iter, static_cast<double>(f.reference));
		}
		else
		{
			return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
														  flags.uppercase, flags.uppercase_e, flags.comma,
														  flags.nan_show_sign, flags.nan_show_type>(iter,
																										   f.reference);
		}
	}
	else
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
					  sizeof(flt) == sizeof(double)) // this is the case on xxx-windows-msvc
		{
			return details::print_rsvflt_define_impl<flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
													 flags.floating, flags.nan_show_sign, flags.nan_show_type>(
				iter, static_cast<double>(f.reference));
		}
		else
		{
			// this is the case for every other platform, including xxx-windows-gnu
			static_assert((::std::same_as<::std::remove_cvref_t<flt>, double> ||
						   ::std::same_as<::std::remove_cvref_t<flt>, float>
#ifdef __STDCPP_FLOAT32_T__
						   || ::std::same_as<::std::remove_cvref_t<flt>, _Float32>
#endif
#ifdef __STDCPP_FLOAT64_T__
						   || ::std::same_as<::std::remove_cvref_t<flt>, _Float64>
#endif
						   ),
						  "currently only support iec559 float32 and float64, sorry");
			return details::print_rsvflt_define_impl<flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
													 flags.floating, flags.nan_show_sign, flags.nan_show_type>(iter,
																											  f.reference);
		}
	}
}
#endif
} // namespace fast_io
