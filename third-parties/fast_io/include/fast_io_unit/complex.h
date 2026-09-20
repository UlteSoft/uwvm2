#pragma once

namespace fast_io
{

namespace manipulators
{
/// @brief Formats a complex value as `(real,imag)` with shortest unprefixed hexadecimal components.
/// @details Each component uses a period radix point and a mandatory binary `p`/`P` exponent. `uppercase` controls
///          hexadecimal digits, exponent markers, and special-value spelling; the three punctuation characters
///          surrounding and separating the components remain `(`, `,`, and `)`.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto hexfloat(::std::complex<scalar_type> t) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, false>,
								  ::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}};
		}
		else
#endif
			return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, false>,
								  ::std::complex<double>>{
				{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}};
	}
	else
	{
		return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, false>,
							  ::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))}};
	}
}

/// @brief Formats a complex value as `(real,imag)` with shortest `0x`-prefixed hexadecimal components.
/// @details Each component receives its own `0x`/`0X` prefix and binary exponent. Apart from those prefixes, the
///          representation and case behavior are identical to complex `hexfloat`.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto hexfloat0x(::std::complex<scalar_type> t) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, false, true>,
								  ::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}};
		}
		else
#endif
			return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, false, true>,
								  ::std::complex<double>>{
				{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}};
	}
	else
	{
		return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, false, true>,
							  ::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))}};
	}
}

/// @brief Constructs the legacy unprefixed hexadecimal precision carrier for a complex value.
/// @details `n` is stored with significant-digit and nearest-to-even flags, but the library currently provides no
///          reserve-print customization for `scalar_manip_precision_t<..., std::complex<...>>`. The returned object is
///          therefore not printable/concatenable and has no emitted `(real,imag)` result; do not use this overload as a
///          working precision formatter.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto hexfloat(::std::complex<scalar_type> t, ::std::size_t n) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_precision_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, false>,
											::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}, n};
		}
		else
#endif
			return scalar_manip_precision_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, false>,
											::std::complex<double>>{
				{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}, n};
	}
	else
	{
		return scalar_manip_precision_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, false>,
										::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))},
			n};
	}
}

/// @brief Constructs the legacy prefixed hexadecimal precision carrier for a complex value.
/// @details Although `n` and per-component `0x` intent are recorded, no complex `scalar_manip_precision_t` print CPO is
///          defined. The returned carrier cannot currently be passed to print/concat successfully and emits no defined
///          representation.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto hexfloat0x(::std::complex<scalar_type> t, ::std::size_t n) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_precision_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, false, true>,
											::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}, n};
		}
		else
#endif
			return scalar_manip_precision_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, false, true>,
											::std::complex<double>>{
				{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}, n};
	}
	else
	{
		return scalar_manip_precision_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, false, true>,
										::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))},
			n};
	}
}

/// @brief Formats a complex value with shortest unprefixed hexadecimal components and comma radix points.
/// @details The result is `(real.imag)`: comma is the radix point inside each component, so a period separates the
///          real and imaginary components to keep the representation unambiguous. Binary exponents remain mandatory.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_hexfloat(::std::complex<scalar_type> t) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, true>,
								  ::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}};
		}
		else
#endif
			return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, true>,
								  ::std::complex<double>>{
				{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}};
	}
	else
	{
		return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, true>,
							  ::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))}};
	}
}

/// @brief Formats comma-radix complex hexadecimal output with a prefix on each component.
/// @details The result uses `(real.imag)` punctuation, `0x`/`0X` before both components, comma radix points, and a
///          binary exponent on each component.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_hexfloat0x(::std::complex<scalar_type> t) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, true, true>,
								  ::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}};
		}
		else
#endif
			return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, true, true>,
								  ::std::complex<double>>{
				{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}};
	}
	else
	{
		return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, true, true>,
							  ::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))}};
	}
}

/// @brief Constructs the legacy comma-radix hexadecimal precision carrier for a complex value.
/// @details The carrier records significant precision `n`, comma radix, and nearest-to-even rounding, but no matching
///          complex precision reserve printer exists. It is not presently a printable/concatenable manipulator and
///          consequently produces no `(real.imag)` output.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_hexfloat(::std::complex<scalar_type> t, ::std::size_t n) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_precision_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, true>,
											::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}, n};
		}
		else
#endif
			return scalar_manip_precision_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, true>,
											::std::complex<double>>{
				{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}, n};
	}
	else
	{
		return scalar_manip_precision_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, true>,
										::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))},
			n};
	}
}

/// @brief Constructs the legacy prefixed comma-radix hexadecimal precision carrier for a complex value.
/// @details Prefix, radix, and significant precision flags are stored, but the library has no print customization for
///          the resulting complex precision carrier. Using it with print/concat is ill-formed; it has no current emitted
///          representation.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_hexfloat0x(::std::complex<scalar_type> t, ::std::size_t n) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_precision_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, true, true>,
											::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}, n};
		}
		else
#endif
			return scalar_manip_precision_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, true, true>,
											::std::complex<double>>{
				{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}, n};
	}
	else
	{
		return scalar_manip_precision_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, true, true>,
										::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))},
			n};
	}
}

/// @brief Formats a complex value as `(real,imag)` using shortest general decimal notation per component.
/// @details Each component independently uses fixed notation for scientific exponents in `[-4, 6)` and scientific
///          notation outside that interval. `uppercase` affects exponent markers and special values.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto general(::std::complex<scalar_type> t) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::general>,
				::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}};
		}
		else
#endif
			return scalar_manip_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::general>,
				::std::complex<double>>{{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}};
	}
	else
	{
		return scalar_manip_t<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::general>,
			::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))}};
	}
}

/// @brief Formats shortest general decimal complex output with comma radix points.
/// @details The result uses `(real.imag)` so that the period separates components while comma is reserved for each
///          component's fractional radix. General-format notation is selected independently for each component.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_general(::std::complex<scalar_type> t) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::general>,
				::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}};
		}
		else
#endif
			return scalar_manip_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::general>,
				::std::complex<double>>{{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}};
	}
	else
	{
		return scalar_manip_t<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::general>,
			::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))}};
	}
}

/// @brief Constructs the legacy general-decimal precision carrier for a complex value.
/// @details The carrier records `n` as significant precision with nearest-to-even rounding, but no complex precision
///          reserve printer consumes it. It cannot currently be printed or concatenated, so no fixed/scientific
///          component selection actually occurs.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto general(::std::complex<scalar_type> t, ::std::size_t n) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_precision_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::general>,
				::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}, n};
		}
		else
#endif
			return scalar_manip_precision_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::general>,
				::std::complex<double>>{{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}, n};
	}
	else
	{
		return scalar_manip_precision_t<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::general>,
			::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))},
			n};
	}
}

/// @brief Constructs the legacy comma-general precision carrier for a complex value.
/// @details Significant precision, comma radix, and nearest-to-even flags are stored, but the resulting complex
///          `scalar_manip_precision_t` has no reserve-print customization. Print/concat use is ill-formed and no
///          `(real.imag)` representation is currently produced.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_general(::std::complex<scalar_type> t, ::std::size_t n) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_precision_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::general>,
				::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}, n};
		}
		else
#endif
			return scalar_manip_precision_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::general>,
				::std::complex<double>>{{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}, n};
	}
	else
	{
		return scalar_manip_precision_t<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::general>,
			::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))},
			n};
	}
}

/// @brief Formats a complex value as `(real,imag)` with shortest fixed notation for both components.
/// @details Neither component contains an exponent, even when that makes the representation long. `uppercase`
///          therefore normally affects only special floating values.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto fixed(::std::complex<scalar_type> t) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::fixed>,
				::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}};
		}
		else
#endif
			return scalar_manip_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::fixed>,
				::std::complex<double>>{{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}};
	}
	else
	{
		return scalar_manip_t<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::fixed>,
			::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))}};
	}
}

/// @brief Formats both complex components in shortest comma-radix fixed notation.
/// @details The result is `(real.imag)`: each component uses comma as its radix point and the separating punctuation
///          changes to period. No exponent is emitted for either component.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_fixed(::std::complex<scalar_type> t) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::fixed>,
				::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}};
		}
		else
#endif
			return scalar_manip_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::fixed>,
				::std::complex<double>>{{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}};
	}
	else
	{
		return scalar_manip_t<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::fixed>,
			::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))}};
	}
}

/// @brief Constructs the legacy fixed-notation precision carrier for a complex value.
/// @details The carrier unusually records `n` as significant rather than fractional precision, but no complex precision
///          reserve printer is defined to materialize it. It is not presently printable/concatenable and therefore does
///          not produce fixed component text.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto fixed(::std::complex<scalar_type> t, ::std::size_t n) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_precision_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::fixed>,
				::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}, n};
		}
		else
#endif
			return scalar_manip_precision_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::fixed>,
				::std::complex<double>>{{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}, n};
	}
	else
	{
		return scalar_manip_precision_t<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::fixed>,
			::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))},
			n};
	}
}

/// @brief Constructs the legacy comma-fixed precision carrier for a complex value.
/// @details It records significant (not fractional) precision plus comma-radix intent, but there is no complex
///          `scalar_manip_precision_t` reserve printer. Using the result with print/concat is ill-formed and no field
///          separator or component spelling is emitted.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_fixed(::std::complex<scalar_type> t, ::std::size_t n) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_precision_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::fixed>,
				::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}, n};
		}
		else
#endif
			return scalar_manip_precision_t<
				::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::fixed>,
				::std::complex<double>>{{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}, n};
	}
	else
	{
		return scalar_manip_precision_t<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::fixed>,
			::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))},
			n};
	}
}

/// @brief Formats a complex value as `(real,imag)` with shortest scientific notation for both components.
/// @details Each component always contains a decimal exponent, regardless of whether fixed notation would be shorter.
///          `uppercase` selects `E` and uppercase special-value spelling.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto scientific(::std::complex<scalar_type> t) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_t<::fast_io::details::dcmfloat_mani_flags_cache<
									  uppercase, false, manipulators::floating_format::scientific>,
								  ::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}};
		}
		else
#endif
			return scalar_manip_t<::fast_io::details::dcmfloat_mani_flags_cache<
									  uppercase, false, manipulators::floating_format::scientific>,
								  ::std::complex<double>>{
				{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}};
	}
	else
	{
		return scalar_manip_t<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::scientific>,
			::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))}};
	}
}

/// @brief Formats both complex components in shortest comma-radix scientific notation.
/// @details Each component has a mandatory exponent and a comma radix point; a period separates the components, giving
///          the outer representation `(real.imag)`.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_scientific(::std::complex<scalar_type> t) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_t<::fast_io::details::dcmfloat_mani_flags_cache<
									  uppercase, true, manipulators::floating_format::scientific>,
								  ::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}};
		}
		else
#endif
			return scalar_manip_t<::fast_io::details::dcmfloat_mani_flags_cache<
									  uppercase, true, manipulators::floating_format::scientific>,
								  ::std::complex<double>>{
				{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}};
	}
	else
	{
		return scalar_manip_t<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::scientific>,
			::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))}};
	}
}

/// @brief Constructs the legacy scientific-notation precision carrier for a complex value.
/// @details The overload records `n` as significant precision rather than fractional precision. No complex precision
///          reserve printer currently exists, so the carrier is not printable/concatenable and does not actually emit
///          exponent-bearing components.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto scientific(::std::complex<scalar_type> t, ::std::size_t n) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_precision_t<::fast_io::details::dcmfloat_mani_flags_cache<
												uppercase, false, manipulators::floating_format::scientific>,
											::std::complex<__float128>>{
				{static_cast<__float128>(::std::real(t)), static_cast<__float128>(::std::imag(t))}, n};
		}
		else
#endif
			return scalar_manip_precision_t<::fast_io::details::dcmfloat_mani_flags_cache<
												uppercase, false, manipulators::floating_format::scientific>,
											::std::complex<double>>{
				{static_cast<double>(::std::real(t)), static_cast<double>(::std::imag(t))}, n};
	}
	else
	{
		return scalar_manip_precision_t<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::scientific>,
			::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))},
			n};
	}
}

/// @brief Constructs the legacy comma-scientific precision carrier for a complex value.
/// @details Significant precision and comma-scientific intent are encoded, but no reserve-print customization accepts
///          the resulting complex precision carrier. Print/concat use is currently ill-formed; for some widened scalar
///          branches even carrier construction is not viable because the implementation attempts a scalar cast of the
///          complex object.
template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_scientific(::std::complex<scalar_type> t, ::std::size_t n) noexcept
{
	if constexpr (::std::same_as<::std::remove_cvref_t<scalar_type>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
				  || ::std::same_as<::std::remove_cvref_t<scalar_type>, __float128>
#endif
	)
	{
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
		if constexpr (sizeof(scalar_type) > sizeof(double))
		{
			return scalar_manip_precision_t<::fast_io::details::dcmfloat_mani_flags_cache<
												uppercase, true, manipulators::floating_format::scientific>,
											::std::complex<__float128>>{static_cast<__float128>(t), n};
		}
		else
#endif
			return scalar_manip_precision_t<::fast_io::details::dcmfloat_mani_flags_cache<
												uppercase, true, manipulators::floating_format::scientific>,
											::std::complex<double>>{static_cast<double>(t), n};
	}
	else
	{
		return scalar_manip_precision_t<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::scientific>,
			::std::complex<::std::remove_cvref_t<scalar_type>>>{
			{static_cast<::std::remove_cvref_t<scalar_type>>(::std::real(t)),
			 static_cast<::std::remove_cvref_t<scalar_type>>(::std::imag(t))},
			n};
	}
}

} // namespace manipulators

template <typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr auto print_alias_define(io_alias_t, ::std::complex<scalar_type> t) noexcept
{
	return ::fast_io::mnp::general<false>(t);
}

namespace details
{
template <::fast_io::manipulators::scalar_flags flags, typename T, ::std::integral char_type>
inline constexpr char_type *print_reserve_complex_impl(char_type *iter, T real, T imag) noexcept
{
	*iter = char_literal_v<u8'(', char_type>;
	++iter;
	iter = print_reserve_define(io_reserve_type<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T>>, iter,
								::fast_io::manipulators::scalar_manip_t<flags, T>{real});
	*iter = char_literal_v<(flags.comma ? u8'.' : u8','), char_type>;
	++iter;
	iter = print_reserve_define(io_reserve_type<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T>>, iter,
								::fast_io::manipulators::scalar_manip_t<flags, T>{imag});
	*iter = char_literal_v<u8')', char_type>;
	++iter;
	return iter;
}
} // namespace details

/// @feature concept:runtime_precise_size
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(flags.base == 10)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, ::std::complex<flt>>>) noexcept
{
	constexpr ::std::size_t v{print_reserve_size(io_reserve_type<char_type, manipulators::scalar_manip_t<flags, flt>>)};
	constexpr ::std::size_t res{(v * 2u) + 3};
	return res;
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(flags.base == 10)
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, ::std::complex<flt>>>,
					 char_type *iter, manipulators::scalar_manip_t<flags, ::std::complex<flt>> f) noexcept
{
	return ::fast_io::details::print_reserve_complex_impl<flags>(iter, ::std::real(f.reference),
																 ::std::imag(f.reference));
}

} // namespace fast_io
