#pragma once

/*
Floating to_chars: proof and bounded-store contract
===================================================

Exact source model
------------------

For every admitted IEC 60559 field representation, punning.h decomposes a
finite value into sign s, integer significand m, and binary exponent e:

						 x = (-1)^s m 2^e.

This identity is exact, including subnormals (whose hidden bit is absent).
NaN, infinity, and signed zero are classified from the same integer fields
before any floating arithmetic, which also preserves narrow signaling-NaN
payloads on ABIs where a native by-value conversion would quiet them.

PowerPC IBM double-double is deliberately not covered by that field argument.
Its object is two binary64 components h and l and denotes their exact sum.
The IBM bridge decodes both signed dyadics, aligns their integer magnitudes,
and obtains the unique odd carrier m*2^e without floating arithmetic.  Its
predecessor and successor are constructed from the real p=106 lattice quantum
2^max(E-105,-1074), using the preceding-binade quantum below an exact power of
two.  Thus its shortest interval is the actual ABI interval, including the
reduced-precision underflow region and asymmetric maximum, rather than the
interval of a fictitious IEEE binary128 field.

Shortest conversion constructs the rounding interval I(x): the set of reals
which the selected policy maps back to x.  Its endpoints are the adjacent
binary midpoints for a nearest policy and adjacent representable values for a
directed policy; endpoint openness is precisely the policy's tie rule.  For a
decimal candidate c = M*10^q, cached-power multiplication compares the integer
image of c with those endpoints.  The cache error is carried as an interval,
so a candidate is accepted only when its entire computed interval has the same
comparison result.  The exact fallback uses the dyadic identity above.

Existence follows because sufficiently fine decimal grids intersect the
nonempty interior of I(x).  The implementation tests digit counts in increasing
order; the first admitted (M,q) is therefore shortest.  When two candidates of
the same length exist, the documented distance/tie comparison selects the one
required by the rounding policy.  Hence parsing the emitted carrier under that
policy returns x, and no shorter carrier can do so.

Precision conversion asks for a prescribed grid rather than a shortest
interval.  Fixed precision P uses the global quantum 10^-P.  If scientific
normalization gives x=y*10^X with 1<=y<10, P fractional digits mean P+1
significant digits and quantum 10^(X-P).  General precision P likewise means
P significant digits, while hexadecimal precision P uses P radix-16 digits
after binary normalization.  Exact binary expansion gives quotient Q and
remainder R.  Comparing 2R with the divisor implements the six nearest rules;
testing R!=0 implements the four directed rules.  Carry is propagated before
presentation, so powers of ten and binade boundaries need no exceptional
rounding rule.

Presentation is injective in the selected carrier: fixed inserts the radix
according to q, scientific writes one leading digit and adjusts the exponent,
and hexadecimal writes the binary exponent with no `0x` prefix.  The
three-argument overload compares the complete fixed and scientific spelling
lengths and selects the shorter (fixed on a tie); an explicit
chars_format::general instead applies the standard `g` exponent-window layout
to the same shortest carrier.  With P significant digits, let X be the
scientific exponent *after rounding*; the standard `g` rule selects fixed iff
-4 <= X < max(P,1).  The charconv_significant precision tag records only this
final layout rule and reuses the same numeric rounding engine.

Digits and punctuation are produced with char_literal_v/charliteralofnumber,
so char, wchar_t, char8_t, char16_t, and char32_t encode the same abstract
spelling.  ASCII SIMD is gated by both one-byte width and is_ascii; EBCDIC and
wide execution characters use the scalar table.

Bounded-store theorem
---------------------

The ordinary floating reserve writer may issue a fixed-width SIMD store beyond
its *logical* returned pointer.  Such a store is safe only when the full
type/format reserve extent is available.  to_chars_floating_emit therefore has
two proved branches:

  1. capacity >= print_reserve_size(tag[,value]): the ordinary writer's
	 physical-store contract is satisfied, so it performs the fast single
	 conversion.  Runtime-precision manipulators necessarily use the
	 value-dependent form because precision is part of `value`;
  2. otherwise, print_reserve_precise_size computes the exact logical length.
	 If it does not fit, no write occurs and {last,value_too_large} is returned.
	 If it fits, print_reserve_precise_define selects exact-bounds stores whose
	 every written code unit is inside that measured slice.

These cases cover every capacity and prove both absence of overrun at an exact
boundary and the standard all-or-error result.  The second conversion in the
tight branch is intentional: the first call is a non-writing size proof.

Constant/runtime equivalence
----------------------------

For field representations, the compiler-constant proxy captures exactly the
same (s,m,e) fields, rounding policy, and presentation flags as the native
runtime manipulator.  The proof above makes the output a deterministic
function F(s,m,e,flags,precision), so both paths emit identical arguments to
F.  IBM double-double intentionally bypasses this proxy: retaining only one
synthetic p=106 carrier would lose the component-dependent neighbor lattice.
It uses the native-object adapter in both optimizer-constant and dynamic calls.
__builtin_constant_p is therefore only a profitability gate where the proxy is
representation-complete; it is never a semantic test.  During consteval, the
same constexpr integer arithmetic is used and runtime SIMD branches are
unavailable.  A runtime `chars_format` switch and a literal-format arm call the
same fixed-format instantiation, proving dispatch equivalence by substitution.
*/

namespace fast_io
{

namespace details
{

template <::fast_io::chars_format format, bool shortest_general,
		  ::fast_io::manipulators::floating_rounding rounding>
inline consteval ::fast_io::manipulators::scalar_flags
to_chars_floating_flags() noexcept
{
	auto flags{::fast_io::manipulators::floating_point_default_scalar_flags};
	flags.rounding = rounding;
	flags.showpos = false;
	flags.showbase = false;
	flags.comma = false;
	if constexpr (format == ::fast_io::chars_format::fixed)
	{
		/*
		Fixed presentation inserts the radix according to the decimal exponent
		and never emits an exponent field.  The carrier itself is unchanged.
		*/
		flags.floating = ::fast_io::manipulators::floating_format::fixed;
	}
	else if constexpr (format == ::fast_io::chars_format::scientific)
	{
		/*
		Scientific presentation normalizes to one leading decimal digit and
		moves the compensating power into an exponent.  This is an exact
		identity M*10^q=(M/10^(L-1))*10^(q+L-1).
		*/
		flags.floating = ::fast_io::manipulators::floating_format::scientific;
	}
	else if constexpr (format == ::fast_io::chars_format::hex)
	{
		/*
		Hexadecimal presentation consumes the exact m*2^e decomposition and
		charconv suppresses the `0x` prefix through showbase=false above.
		*/
		flags.floating = ::fast_io::manipulators::floating_format::hexfloat;
	}
	else
	{
		static_assert(format == ::fast_io::chars_format::general);
		/*
		The three-argument overload (`shortest_general=true`) minimizes the
		complete fixed/scientific spelling.  Explicit `chars_format::general`
		instead applies the standard `%g`-style exponent window.  Distinct
		presentation tags are necessary even though both reuse one shortest
		numeric carrier.
		*/
		flags.floating = shortest_general
			? ::fast_io::manipulators::floating_format::decimal
			: ::fast_io::manipulators::floating_format::general;
	}
	return flags;
}

template <::fast_io::chars_format format,
		  ::fast_io::manipulators::floating_rounding rounding>
inline consteval ::fast_io::manipulators::scalar_flags
to_chars_floating_precision_flags() noexcept
{
	auto flags{
		::fast_io::details::to_chars_floating_flags<
			format, false, rounding>()};
	if constexpr (format == ::fast_io::chars_format::general)
	{
		/*
		Precision P is a significant-digit grid for general format.  The
		charconv tag changes only the post-rounding layout test
		-4<=X<max(P,1); quotient, remainder, and carry are shared.
		*/
		flags.floating =
			::fast_io::manipulators::floating_format::general;
		flags.precision =
			::fast_io::manipulators::floating_precision::
				charconv_significant;
	}
	else
	{
		if constexpr (format == ::fast_io::chars_format::hex)
		{
			/*
			Hex precision counts fractional hexadecimal digits.  Its dedicated
			tag preserves a leading carry as `2p+E`, which is numerically equal
			to `1p+(E+1)` but is the spelling mandated for explicit precision.
			*/
			flags.precision =
				::fast_io::manipulators::floating_precision::
					charconv_hex_fractional;
		}
		else
		{
			/*
			Fixed and scientific precision both count displayed fractional
			decimal digits and retain trailing zeroes, so they share this precision
			category.  The format remains part of the manipulator: fixed uses the
			global 10^-P grid, whereas scientific first determines the normalized
			exponent X and therefore rounds on the 10^(X-P) grid.  Sharing the tag
			does not identify those two grids.
			*/
			flags.precision =
				::fast_io::manipulators::floating_precision::
					fractional_preserve_trailing_zero;
		}
	}
	return flags;
}

template <::fast_io::details::character char_type, typename printable>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_floating_emit(char_type *first, char_type *last,
					   printable const &value) noexcept
{
	using printable_type = ::std::remove_cvref_t<printable>;
	constexpr auto tag{::fast_io::io_reserve_type<char_type, printable_type>};
	auto const capacity{static_cast<::std::size_t>(last - first)};
	auto const reserve_size{[&]() constexpr noexcept
	{
		if constexpr (requires { print_reserve_size(tag); })
		{
			/*
			A static reserve extent is a type-level upper bound on every
			physical store performed by the ordinary writer.
			*/
			return print_reserve_size(tag);
		}
		else
		{
			/*
			Runtime precision participates in the store bound, so the
			value-dependent query is required to prove the same containment.
			*/
			return print_reserve_size(tag, value);
		}
	}()};
	if (reserve_size <= capacity) [[likely]]
	{
		/*
		The ordinary writer may overstore past its logical result but never
		past `reserve_size`.  This inequality embeds that whole physical range
		in [first,last), proving the fast call memory-safe.
		*/
		return {print_reserve_define(tag, first, value), {}};
	}

	/*
	The precise-size query is non-writing and returns the exact number L of
	logical code units.  It is reached only when the broad SIMD reserve proof
	does not fit; no output has yet been modified.
	*/
	auto const precise_size{print_reserve_precise_size(tag, value)};
	if (capacity < precise_size) [[unlikely]]
	{
		/*
		L exceeds the destination extent, so no valid complete spelling fits.
		Returning `last` with value_too_large while performing no write is the
		strong bounded-buffer contract.
		*/
		return {last, ::fast_io::charconv_errc::value_too_large};
	}
	/*
	Now L<=capacity.  The exact-bounds writer is parameterized by that same L
	and restricts every store to [first,first+L), which proves safety at the
	exact boundary even when the ordinary SIMD writer needed more slack.
	*/
	return {
		print_reserve_precise_define(tag, first, precise_size, value), {}};
}

template <::fast_io::chars_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  bool shortest_general = false,
		  ::fast_io::details::my_floating_point T,
		  ::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_floating_fixed(char_type *first, char_type *last, T value) noexcept
{
	constexpr auto flags{
		::fast_io::details::to_chars_floating_flags<
			format, shortest_general, rounding>()};
	auto source{
		::fast_io::details::make_floating_scalar_manip<flags>(value)};
	using source_type = decltype(source);
	constexpr auto source_tag{
		::fast_io::io_reserve_type<char_type, source_type>};
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	if constexpr (requires {
					  print_compiler_constant_materialization_eligible(
						  source_tag, source);
					  print_compiler_constant_materialize_gate_proven(
						  source_tag, source);
				  })
	{
		if (print_compiler_constant_materialization_eligible(
				source_tag, source))
		{
			/*
			Eligibility proves that extracting the compiler-known raw fields
			can remove runtime classification/conversion work.  The materialized
			proxy contains the identical sign, exponent, mantissa, and flags, so
			the deterministic formatter theorem gives byte-identical output.
			*/
			auto constant_source{
				print_compiler_constant_materialize_gate_proven(
					source_tag, source)};
			return ::fast_io::details::to_chars_floating_emit(
				first, last, constant_source);
		}
	}
#endif
	/*
	If the optimizer cannot prove eligibility, retaining the native source
	avoids proxy construction.  This branch changes representation of the
	input object only in the rejected alternative, never its formatting rules.
	*/
	return ::fast_io::details::to_chars_floating_emit(first, last, source);
}

template <::fast_io::chars_format format,
		  ::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::details::my_floating_point T,
		  ::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_floating_precision_fixed(
	char_type *first, char_type *last, T value,
	::std::size_t precision) noexcept
{
	constexpr auto flags{
		::fast_io::details::to_chars_floating_precision_flags<
			format, rounding>()};
	auto source{
		::fast_io::details::make_floating_scalar_manip_precision<flags>(
			value, precision)};
	using source_type = decltype(source);
	constexpr auto source_tag{
		::fast_io::io_reserve_type<char_type, source_type>};
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	if constexpr (requires {
					  print_compiler_constant_materialization_eligible(
						  source_tag, source);
					  print_compiler_constant_materialize_gate_proven(
						  source_tag, source);
				  })
	{
		if (print_compiler_constant_materialization_eligible(
				source_tag, source))
		{
			/*
			The precision value is stored in both source forms unchanged.
			Consequently the proxy and native paths round on the same grid as
			well as sharing the same raw floating fields.
			*/
			auto constant_source{
				print_compiler_constant_materialize_gate_proven(
					source_tag, source)};
			return ::fast_io::details::to_chars_floating_emit(
				first, last, constant_source);
		}
	}
#endif
	/*
	The dynamic precision path is the semantic baseline.  Falling through
	preserves one copy of the arithmetic implementation and avoids a runtime
	format proxy when no compile-time saving is proved.
	*/
	return ::fast_io::details::to_chars_floating_emit(first, last, source);
}

template <::fast_io::details::my_floating_point T>
[[nodiscard]] inline constexpr auto
to_chars_floating_capture_fields(T value) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	if constexpr (
		::fast_io::details::
			floating_scalar_requires_integer_proxy<floating_type>)
	{
		/*
		Some narrow types (notably bfloat16 on affected ABIs) may be promoted
		or have signaling NaNs quieted by an ordinary by-value floating
		operation.  Capturing their object representation first preserves all
		source bits, after which the proxy exposes the same logical fields.
		*/
		auto const representation{
			::fast_io::details::capture_bfloat16_representation(value)};
		return ::fast_io::details::floating_scalar_proxy_fields<
			floating_type>(representation);
	}
	else
	{
		/*
		For directly supported IEC layouts, get_punned_result is a bitwise
		decomposition.  No floating arithmetic or ambient rounding mode enters
		the classification.
		*/
		return ::fast_io::details::get_punned_result(value);
	}
}

template <typename floating_type>
[[nodiscard]] inline constexpr bool
to_chars_floating_fields_are_integer(
	::fast_io::details::punning_result<floating_type> fields) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u)};
	if (fields.exponent == exponent_mask)
	{
		/*
		An all-ones exponent denotes infinity or NaN.  Neither is an integer in
		the finite divisibility sense used to select an exact fixed candidate.
		*/
		return false;
	}
	if (!fields.exponent)
	{
		/*
		With exponent zero, a zero mantissa is signed zero and hence integral.
		Any nonzero subnormal has magnitude strictly between zero and the least
		normal; for every admitted format it cannot be a nonzero integer.
		*/
		return fields.mantissa == 0u;
	}
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u)
		 << (trait::ebits - 1u)) -
		1u)};
	auto const binary_exponent{
		static_cast<::std::int_least32_t>(fields.exponent) - bias -
		static_cast<::std::int_least32_t>(trait::mbits)};
	if (0 <= binary_exponent)
	{
		/*
		The exact value is integer_significand*2^binary_exponent.  A
		nonnegative exponent multiplies an integer by an integer power of two,
		so divisibility by one is immediate.
		*/
		return true;
	}
	auto const discarded_bits{
		static_cast<::std::uint_least32_t>(-binary_exponent)};
	if (trait::mbits < discarded_bits)
	{
		/*
		The hidden significand has its top bit at `mbits`.  Divisibility by
		2^discarded_bits with discarded_bits>mbits would require the entire
		nonzero significand to vanish, which is impossible.
		*/
		return false;
	}
	auto const significand{static_cast<mantissa_type>(
		fields.mantissa |
		(static_cast<mantissa_type>(1u) << trait::mbits))};
	auto const mask{static_cast<mantissa_type>(
		(static_cast<mantissa_type>(1u) << discarded_bits) - 1u)};
	/*
	For a negative binary exponent, m*2^-k is integral exactly when 2^k divides
	m.  The low-k-bit mask is therefore a necessary and sufficient integer
	test, with no conversion or rounding.
	*/
	return (significand & mask) == 0u;
}

template <::fast_io::details::my_floating_point T>
[[nodiscard]] inline constexpr bool
to_chars_floating_is_integer(T value) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	return ::fast_io::details::to_chars_floating_fields_are_integer<
		floating_type>(
			::fast_io::details::to_chars_floating_capture_fields(value));
}

template <typename floating_type>
[[nodiscard]] inline constexpr bool
to_chars_floating_fields_fixed_cannot_win_shortest(
	::fast_io::details::punning_result<floating_type> fields) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u)
		 << (trait::ebits - 1u)) -
		1u)};
	auto const unbiased_exponent{
		static_cast<::std::int_least32_t>(fields.exponent) - bias};
	constexpr auto maximum_shortest_size{
		::fast_io::details::print_rsv_cache<
			floating_type,
			::fast_io::manipulators::floating_format::decimal>};
	constexpr auto fixed_loss_exponent{
		static_cast<::std::int_least32_t>(
			(10u * maximum_shortest_size + 2u) / 3u)};
	/*
	Let C be `maximum_shortest_size`, a conservative complete-spelling bound
	that already includes a sign, significand separator, exponent marker/sign,
	and the maximum exponent digit count.  A positive normal value with unbiased
	binary exponent E satisfies x>=2^E.  Since

	    2^10 = 1024 > 1000 = 10^3,

	raising both sides to C/3 gives 2^(10C/3)>10^C.  Therefore
	E>=ceil(10C/3) implies x>10^C, so its exact fixed integer needs at least
	C+1 magnitude digits before any sign.  It is strictly longer than every
	possible shortest spelling and cannot win either the primary length order
	or its equal-length distance tie.  The test uses only exponent fields and
	is deliberately one-sided: returning false merely requests the exact
	comparison, so values near the decimal threshold retain full correctness.
	*/
	return fixed_loss_exponent <= unbiased_exponent;
}

template <::fast_io::details::my_floating_point T>
[[nodiscard]] inline constexpr bool
to_chars_floating_fixed_cannot_win_shortest(T value) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	return ::fast_io::details::
		to_chars_floating_fields_fixed_cannot_win_shortest<floating_type>(
			::fast_io::details::to_chars_floating_capture_fields(value));
}

template <typename floating_type>
[[nodiscard]] inline constexpr bool
to_chars_floating_fields_require_exact_fixed_arbitration(
	::fast_io::details::punning_result<floating_type> fields) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto bias{static_cast<::std::uint_least32_t>(
		(static_cast<::std::uint_least32_t>(1u)
		 << (trait::ebits - 1u)) -
		1u)};
	constexpr auto maximum_shortest_size{
		::fast_io::details::print_rsv_cache<
			floating_type,
			::fast_io::manipulators::floating_format::decimal>};
	constexpr auto fixed_loss_exponent{
		static_cast<::std::uint_least32_t>(
			(10u * maximum_shortest_size + 2u) / 3u)};
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) -
		1u)};
	static_assert(
		fixed_loss_exponent <= exponent_mask - bias);
	auto const unbiased_exponent{
		static_cast<::std::uint_least32_t>(fields.exponent) - bias};
#if defined(__APPLE__) &&                                             \
	(defined(__aarch64__) || defined(__arm64__)) && defined(__clang__)
	if constexpr (trait::mbits == 23u && trait::ebits == 8u)
	{
		auto const complete_significand{
			static_cast<mantissa_type>(
				fields.mantissa |
				(static_cast<mantissa_type>(1u)
				 << trait::mbits))};
		auto const adjusted_exponent{
			unbiased_exponent +
			static_cast<::std::uint_least32_t>(
				::std::countr_zero(complete_significand))};
		/*
		For an exponent in the only interval where exact fixed can win,

		    x = u * 2^(E-mbits),  u = 2^mbits + mantissa.

		Writing u=odd*2^v gives

		    x in Z  iff  E-mbits+v >= 0
		            iff  E+v >= mbits.

		complete_significand is nonzero, so countr_zero is defined and equals
		that exact 2-adic valuation v.  The unsigned exponent interval rejects
		sub-bias, too-large, zero/subnormal, and special encodings exactly as
		proved below.  Outside it adjusted_exponent may wrap, but the first
		boolean is false; bitwise boolean conjunction therefore remains false
		without relying on a short-circuit branch.  Inside it addition cannot
		overflow and the second boolean is precisely the integrality theorem.

		This Apple binary32 form turns the former exponent-then-divisibility
		control graph into one final candidate predicate.  It is not selected
		for the filtered noncandidate optimum: the extra count operation costs
		work there, whereas measured mixed public input wins by avoiding entry
		into the roughly twice-as-frequent exponent superset.
		*/
		return (unbiased_exponent < fixed_loss_exponent) &
			(trait::mbits <= adjusted_exponent);
	}
#endif
	if (fixed_loss_exponent <= unbiased_exponent)
	{
		/*
		This single unsigned interval test combines three disjoint rejection
		classes.  For a normal exponent below the bias, subtraction wraps to a
		value greater than `fixed_loss_exponent`; such |x|<1 values cannot be
		nonzero integers.  Exponents at or above bias+fixed_loss_exponent are
		the large fixed-loss class proved above.  Zero/subnormal and all-ones
		special exponents likewise lie outside the interval; their ordinary DA
		spellings need no exact-integer arbitration (signed zero is already
		exact).  Consequently only the narrow unbiased interval

		    0 <= E < fixed_loss_exponent

		reaches the divisibility test.  On a uniformly distributed binary32
		or binary64 encoding this changes the common classification from two
		dependent exponent branches to one subtract-and-compare.
		*/
		return false;
	}
	if (trait::mbits <= unbiased_exponent)
	{
		/*
		x=(2^mbits+mantissa)*2^(E-mbits), and E>=mbits makes the
		second factor an integer power of two.  The exact fixed candidate
		therefore exists.
		*/
		return true;
	}
	auto const discarded_bits{
		static_cast<unsigned>(trait::mbits - unbiased_exponent)};
	auto const mask{static_cast<mantissa_type>(
		(static_cast<mantissa_type>(1u) << discarded_bits) - 1u)};
	/*
	Here 1<=discarded_bits<=mbits.  The hidden bit is above `mask`, so
	divisibility of the complete significand by 2^discarded_bits is equivalent
	to `(mantissa&mask)==0`.  This is the exact remaining condition for x to be
	an integer.  Thus, on the nonzero-normal interval reaching this branch,
	`true` is equivalent to the conjunction formerly tested by
	fixed_cannot_win plus fields_are_integer.  Signed zero is the intentional
	already-final exception described above; the one-test rejection path leaves
	its ordinary DA result and every special spelling unchanged.
	*/
	return (fields.mantissa & mask) == 0u;
}

#if defined(__SIZEOF_INT128__)
using to_chars_floating_small_integer_type = __uint128_t;
#else
using to_chars_floating_small_integer_type = ::std::uint_least64_t;
#endif

template <typename carrier>
struct basic_to_chars_floating_small_integer
{
	carrier magnitude{};
	::std::size_t size{};
	bool negative{};
	bool available{};
};

using to_chars_floating_small_integer =
	::fast_io::details::basic_to_chars_floating_small_integer<
		::fast_io::details::to_chars_floating_small_integer_type>;

template <typename carrier, typename floating_type>
[[nodiscard]] inline constexpr
	::fast_io::details::basic_to_chars_floating_small_integer<carrier>
to_chars_floating_make_small_integer_from_fields_as(
	::fast_io::details::punning_result<floating_type> fields) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	static_assert(::std::numeric_limits<carrier>::is_integer);
	static_assert(!::std::numeric_limits<carrier>::is_signed);
	if (!fields.exponent)
	{
		/*
		The caller has already proved integrality, so exponent zero can only be
		signed zero.  Its exact fixed magnitude is the one decimal digit `0`;
		the sign remains a separate code unit and no shift is required.
		*/
		return {0u, 1u + static_cast<::std::size_t>(fields.sign),
				static_cast<bool>(fields.sign), true};
	}
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u)
		 << (trait::ebits - 1u)) -
		1u)};
	auto const binary_exponent{
		static_cast<::std::int_least32_t>(fields.exponent) - bias -
		static_cast<::std::int_least32_t>(trait::mbits)};
	auto const source_significand{static_cast<carrier>(
		static_cast<mantissa_type>(
			fields.mantissa |
			(static_cast<mantissa_type>(1u) << trait::mbits)))};
	carrier magnitude{};
	if (binary_exponent < 0)
	{
		/*
		Integrality proves 2^(-e) divides the source significand.  Right shift
		therefore performs exact integer division, discarding only zero bits.
		*/
		magnitude = static_cast<carrier>(
			source_significand >>
			static_cast<unsigned>(-binary_exponent));
	}
	else
	{
		constexpr auto carrier_bits{
			::std::numeric_limits<carrier>::digits};
		auto const shift{static_cast<unsigned>(binary_exponent)};
		constexpr auto carrier_max{
			(::std::numeric_limits<carrier>::max)()};
		if (carrier_bits <= shift ||
			static_cast<carrier>(carrier_max >> shift) <
				source_significand)
		{
			/*
			The exact product m*2^e does not fit this target's small-integer
			carrier.  Returning unavailable is conservative: the caller retains
			the arbitrary-precision exact-size/output path, so this optimization
			can never truncate a candidate.
			*/
			return {};
		}
		/*
		The guard proves source_significand<=MAX/2^shift, hence the left shift
		is representable and equals the exact binary integer product.
		*/
		magnitude = static_cast<carrier>(
			source_significand << shift);
	}
	auto const negative{static_cast<bool>(fields.sign)};
	::std::size_t digits{};
#if defined(__APPLE__) &&                                             \
	(defined(__aarch64__) || defined(__arm64__)) && defined(__clang__)
	if constexpr (
		trait::mbits == 23u && trait::ebits == 8u &&
		sizeof(carrier) <= sizeof(::std::uint_least64_t))
	{
		if constexpr (
			sizeof(carrier) <= sizeof(::std::uint_least32_t))
		{
			digits =
				::fast_io::details::itoa_jeaiii_decimal_digits_u32(
					static_cast<::std::uint_least32_t>(
						magnitude));
		}
		else
		{
			digits =
				::fast_io::details::itoa_jeaiii_decimal_digits_u64(
					static_cast<::std::uint_least64_t>(
						magnitude));
		}
	}
	else
	{
		digits =
			::fast_io::details::chars_len<10u, false>(magnitude);
	}
#else
	{
		digits =
			::fast_io::details::chars_len<10u, false>(magnitude);
	}
#endif
	auto const size{
		digits + static_cast<::std::size_t>(negative)};
	/*
	The JEAIII classifiers are exact decompositions of the decimal intervals:
	the uint32 form selects the unique pair among
	[10^(d-1),10^d), and the uint64 form first decomposes at 10^8 (and 10^16
	when needed) before applying that same exact uint32 classifier to the
	leading group.  An unsigned carrier no wider than the selected least-width
	type is preserved by the cast.  Wider carriers and every target outside the
	measured Apple binary32 specialization retain chars_len's complete threshold
	proof.  Thus `digits` is exactly the decimal digit count of the dyadic
	magnitude in every branch.

	On Apple AArch64 binary32 this schedule also matches the subsequent JEAIII
	writer.  It avoids walking the generic descending 10^20 threshold chain
	merely to learn a count that the 64-bit renderer will immediately rediscover
	through its 10^8 grouping.  The narrower gate is intentional: the equivalent
	grouping changed x86 Clang's binary32 block placement adversely, so an exact
	arithmetic identity is not mistaken for a universal code-generation claim.
	Adding the independently captured sign gives the complete fixed spelling
	length without constructing decimal floating metadata.
	*/
	return {magnitude, size, negative, true};
}

template <typename floating_type>
[[nodiscard]] inline constexpr
	::fast_io::details::to_chars_floating_small_integer
to_chars_floating_make_small_integer_from_fields(
	::fast_io::details::punning_result<floating_type> fields) noexcept
{
	return ::fast_io::details::
		to_chars_floating_make_small_integer_from_fields_as<
			::fast_io::details::to_chars_floating_small_integer_type,
			floating_type>(fields);
}

template <::fast_io::details::my_floating_point T>
[[nodiscard]] inline constexpr
	::fast_io::details::to_chars_floating_small_integer
to_chars_floating_make_small_integer(T value) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	return ::fast_io::details::
		to_chars_floating_make_small_integer_from_fields<floating_type>(
			::fast_io::details::to_chars_floating_capture_fields(value));
}

#if defined(__SIZEOF_INT128__)
/*
Fast run-time realization of the standard shortest rule for the overwhelmingly
common binary32/binary64, ASCII, nearest-to-even specialization.

There are two independent minimizations in the standard rule.

1. DA returns the nearest shortest round-tripping decimal carrier and its ASCII
   renderer chooses the shorter complete fixed/scientific spelling, choosing
   fixed on equal lengths.  Call the resulting string A and its length L.
2. If the binary value is an integer, its exact fixed spelling Z is another
   round-tripping candidate.  Z need not be the spelling obtained from the
   shortest-significand carrier: for example a binary32 integer can have an
   equal-length nearby decimal carrier.  The standard first minimizes total
   length and then distance to the exact value.  Consequently Z replaces A
   exactly when |Z| <= L; equality is resolved in favor of Z because its
   distance is zero.

The run-time specialization first rejects, with one unsigned exponent interval
test plus exact dyadic divisibility only inside that interval, values for which
Z does not exist or is already proved unable to win.  Those values enter the
ordinary DA renderer directly.  A residual exact integer is handled out of
line: a decimal-grid/ULP theorem proves the overwhelmingly common small Z
winner before DA; otherwise one DA conversion determines L without writing.
The selected A or Z is then materialized exactly once.

The public caller proves that the complete physical reserve fits before
entering this schedule, so either single-pass writer is memory-safe.  The
constant-evaluation, compiler-known, non-ASCII, non-char, alternate-rounding,
and exact-boundary-buffer paths retain the pure precise-size materializer.
Neither candidate ordering nor any logical output byte changes; the
optimization removes duplicated DA conversion/rendering and keeps cold integer
arbitration out of the scalar dependency chain.
*/
template <typename carrier>
inline constexpr ::fast_io::basic_to_chars_result<char>
to_chars_floating_exact_integer_known_size(
	char *first, carrier magnitude, ::std::size_t complete_size,
	bool negative) noexcept
{
	if (negative)
	{
		*first++ = '-';
	}
	auto const digits{
		complete_size - static_cast<::std::size_t>(negative)};
	if (magnitude < 10u)
	{
		*first =
			::fast_io::char_literal_add<char>(magnitude);
		return {first + 1u, {}};
	}
#if !(defined(__APPLE__) &&                                         \
	  (defined(__aarch64__) || defined(__arm64__)) && defined(__clang__))
	if constexpr (
		sizeof(carrier) <= sizeof(::std::uint_least64_t))
	{
		/*
		The native decimal result kernel is selected specifically for a caller
		that has already proved capacity and the multi-digit precondition.
		Passing the uint64-or-narrower carrier here avoids routing it through the
		wider length-directed reserve primitive.  Its returned pointer is the
		same `first+digits` established above.
		*/
		return ::fast_io::details::
			to_chars_integral_decimal_unchecked(first, magnitude);
	}
#endif
	auto *const end{first + digits};
	/*
	The caller has already derived `digits` exactly from this identical unsigned
	magnitude and has proved the complete floating reserve fits.
	print_reserve_integral_main_impl writes those known digits backwards from
	`end`; it neither needs a second digit-count classification nor a second
	capacity comparison.  The optional sign occupies the disjoint preceding
	code unit, so the returned pointer is first_original+complete_size.

	Apple AArch64 deliberately uses this known-end kernel for native carriers
	too.  Once the floating classifier has proved `digits`, entering the
	pointer-returning JEAIII result kernel would classify the magnitude again.
	The length-directed store removes that duplicate result state and, in the
	measured Apple Clang layout, also shares more code between binary32 and
	binary64.  Other targets retain the result kernel above: equivalent integer
	identities need not have equivalent register allocation or instruction-cache
	cost on a different compiler/ISA pair.
	*/
	::fast_io::details::print_reserve_integral_main_impl<
		10u, false>(end, magnitude, digits);
	return {end, {}};
}

template <typename floating_type>
[[nodiscard]] inline constexpr bool
to_chars_floating_exact_fixed_must_win_before_da(
	::fast_io::details::punning_result<floating_type> fields,
	::fast_io::details::basic_to_chars_floating_small_integer<
		::std::uint_least64_t> exact_integer) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	static_assert(
		(trait::mbits == 23u && trait::ebits == 8u) ||
		(trait::mbits == 52u && trait::ebits == 11u));
	auto const digits{
		exact_integer.size -
		static_cast<::std::size_t>(exact_integer.negative)};
	if (digits <= 5u)
	{
		/*
		A positive scientific spelling has length at least five: one coefficient
		digit, `e`, an exponent sign, and two exponent digits.  Fixed has exactly
		`digits<=5` code units; every equal-length alternative loses to the exact
		integer's zero distance.  A distinct shorter fixed integer is at least one
		unit away, while every value in this range has half-ULP below one.
		*/
		return true;
	}

	/*
	Within the same D-digit decade, D=6 permits only the shorter scientific
	grammar `d e+NN`, whose value is on the 10^5 grid.  For every 7<=D<=20
	native-uint64 value, a scientific spelling with k>=2 coefficient digits has
	length k+5; being shorter than D forces k<=D-6 and hence a same-decade
	value on the grid 10^(D-k), a subset of the 10^6 grid.  The one-digit case
	is a subset of that grid too.

	A shorter spelling in a lower (or upper) decade need not itself lie on this
	grid: for example, 9e+04 is shorter when D=6.  It is nevertheless separated
	from x by the intervening decade boundary.  That boundary is 10^(D-1) (or
	10^D), belongs to the selected grid, and is at least as close to x as every
	value beyond it.  Excluding both adjacent selected-grid points therefore
	excludes all shorter candidates in other decades.  A fixed spelling shorter
	than D likewise cannot retain D integer places; fractional punctuation only
	consumes another character, so the same boundary domination applies.  Thus
	10^5 for D=6 and 10^6 thereafter is a sufficient nearest-candidate grid,
	not a claim that every remote shorter decimal is itself a grid point.
	*/
	::std::uint_least64_t quantum{};
	::std::uint_least64_t remainder{};
	if (digits == 6u)
	{
		quantum = 100000u;
		remainder = exact_integer.magnitude % 100000u;
	}
	else
	{
		quantum = 1000000u;
		remainder = exact_integer.magnitude % 1000000u;
	}
	/*
	Keep the two constant remainders in distinct control-flow arms.  Writing
	`magnitude % (D==6 ? 100000 : 1000000)` makes Clang materialize a variable
	divisor and emit AArch64 UDIV on the hottest six-digit-integer path.  Each
	constant expression instead lowers to multiply-high/shift arithmetic; the
	branch was already required to choose the proven grid and introduces no new
	semantic case.
	*/
	if (!remainder)
	{
		/*
		The exact integer itself is on the selected grid.  Removing at least five
		or six trailing decimal zeroes gives a scientific spelling strictly
		shorter than the D-digit fixed spelling.  DA minimizes over a superset
		containing that spelling, so exact fixed cannot win.
		*/
		return false;
	}
	if (digits == 6u)
	{
		/*
		A non-grid six-digit integer is at least one unit from every relevant
		10^5 grid point.  Its magnitude is below 10^6<2^20.  Binary32 spacing
		there is at most 2^(19-23)=2^-4 and binary64 spacing is smaller, so every
		nearest-even midpoint radius is strictly below one.
		*/
		return true;
	}

	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u)
		 << (trait::ebits - 1u)) -
		1u)};
	auto const spacing_exponent{
		static_cast<::std::int_least32_t>(fields.exponent) - bias -
		static_cast<::std::int_least32_t>(trait::mbits)};
	if (spacing_exponent <= 0)
	{
		/*
		The adjacent spacing is at most one, hence either midpoint is at most
		one half away.  A distinct integer grid point has distance at least one,
		so none lies in the nearest-even interval.
		*/
		return true;
	}

	auto const distance{
		(remainder < quantum - remainder)
			? remainder
			: quantum - remainder};
	if (20 < spacing_exponent)
	{
		/*
		The grid distance is at most 500000, while half the upper-binade spacing
		is at least 2^20.  This sufficient test cannot exclude a shorter grid
		candidate; the exact residual midpoint classifier below remains
		authoritative.
		*/
		return false;
	}
	auto const maximum_midpoint_radius{
		static_cast<::std::uint_least64_t>(1u)
		<< static_cast<unsigned>(spacing_exponent - 1)};
	/*
	At an ordinary point both adjacent gaps are 2^spacing_exponent.  At an exact
	power of two the lower gap is half that and the upper gap is unchanged.
	Therefore every nearest-even midpoint lies no farther than
	2^(spacing_exponent-1).  A grid distance strictly larger than this radius is
	outside the rounding interval independently of endpoint parity.  With every
	shorter spelling excluded, exact fixed is minimal; at equal length its zero
	distance wins.  The argument is sign-symmetric.
	*/
	return maximum_midpoint_radius < distance;
}

template <typename floating_type>
[[nodiscard]] inline constexpr bool
to_chars_floating_exact_fixed_wins_residual_midpoint(
	::fast_io::details::punning_result<floating_type> fields,
	::fast_io::details::basic_to_chars_floating_small_integer<
		::std::uint_least64_t> exact_integer) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u)
		 << (trait::ebits - 1u)) -
		1u)};
	auto const unbiased_exponent{
		static_cast<::std::int_least32_t>(fields.exponent) -
		bias};
	constexpr auto first_midpoint_exponent{
		static_cast<::std::int_least32_t>(trait::mbits) + 7};
	if (static_cast<::std::uint_least32_t>(
			unbiased_exponent - first_midpoint_exponent) >
		12u)
	{
		/*
		A selected-grid midpoint can exist only for

		    mbits+7 <= E <= mbits+19.

		Below that interval the spacing exponent s=E-mbits is too small to
		contain the factor 2^6 of 10^6 at half a ULP.  Above it the half-ULP
		exceeds 500000, the maximum distance to an adjacent 10^6-grid point, so
		the point is strictly inside the rounding cell and DA wins.
		*/
		return false;
	}
	auto const spacing_exponent{
		unbiased_exponent -
		static_cast<::std::int_least32_t>(trait::mbits)};
	auto const midpoint_radius{
		static_cast<::std::uint_least64_t>(1u)
		<< static_cast<unsigned>(spacing_exponent - 1)};
	auto const remainder{
		exact_integer.magnitude % 1000000u};
	auto const distance{
		(remainder < 1000000u - remainder)
			? remainder
			: 1000000u - remainder};
	/*
	The preceding fast theorem returned false, so some adjacent 10^6-grid point
	is no farther than half a ULP.  Strict inequality places that point inside
	x's rounding cell and its six removable decimal zeroes make DA strictly
	shorter.  Equality is the only remaining case.  Nearest-even includes the
	midpoint iff the complete binary significand is even; its low bit is exactly
	the stored mantissa low bit.  Thus an odd significand excludes the sole
	shorter grid candidate, and the grid/boundary theorem proves exact fixed
	wins.  An even significand admits it and DA wins.  Exhausting every positive
	binary32 exact candidate checks 234,881,023 values and finds precisely 6,981
	such odd-midpoint winners; the formula itself is parameterized by `mbits`
	and sign symmetry covers both formats' negative half.
	*/
	return distance == midpoint_radius &&
		(fields.mantissa &
		 static_cast<typename trait::mantissa_type>(1u)) != 0u;
}

template <::fast_io::details::my_floating_point T>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char>
to_chars_floating_standard_shortest_da_ascii_integer_candidate_residual(
	char *first, char *last,
	::fast_io::details::punning_result<::std::remove_cv_t<T>>
		fields,
	::fast_io::details::basic_to_chars_floating_small_integer<
		::std::uint_least64_t> exact_integer64) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	/*
	The public reserve gate has already proved every fixed-width scratch store
	and either logical winner fit.  This residual therefore needs no per-write
	capacity branch; retain `last` in the signature to make that calling
	contract explicit across the outlined boundary.
	*/
	(void)last;
	constexpr auto flags{
		::fast_io::details::to_chars_floating_flags<
			::fast_io::chars_format::general, true,
			::fast_io::manipulators::floating_rounding::
				nearest_to_even>()};
	::fast_io::details::to_chars_floating_small_integer
		exact_integer_wide{};
	if (!exact_integer64.available)
	{
		/*
		The exact m*2^e product exceeded uint64.  Only this residual class needs
		the uint128 carrier.  The branch is representation-exact: the narrow
		constructor returns unavailable before shifting whenever the product is
		not representable, and the wide constructor repeats the same dyadic
		identity with a strictly larger integer range.
		*/
		exact_integer_wide =
			::fast_io::details::
				to_chars_floating_make_small_integer_from_fields<
					floating_type>(fields);
	}
	auto const converted{
		::fast_io::details::da::to_conversion_result<floating_type>(
			fields.mantissa,
			static_cast<::std::int_least32_t>(fields.exponent))};
	auto const finalized{
		::fast_io::details::da::trim_trailing_zeros(
			::fast_io::details::da::finalize<floating_type>(
				converted))};
	auto const shortest_size{
		static_cast<::std::size_t>(fields.sign) +
		::fast_io::details::floating_precise_decimal_layout_size<
			floating_type,
			::fast_io::manipulators::floating_format::decimal,
			false>(finalized.m10, finalized.e10)};
	/*
	The precise layout formula is the renderer's complete fixed/scientific
	length decision evaluated on the same finalized DA carrier.  Therefore this
	comparison knows |A| without materializing A.  If Z wins, emitting it now
	removes the former DA-render-then-overwrite work; if A wins, the raw carrier
	below is rendered exactly once.  finalize only canonicalizes the carrier for
	the size formula and does not alter the raw fields consumed by the direct
	ASCII renderer.
	*/
	if (exact_integer64.available)
	{
		if (exact_integer64.size <= shortest_size)
		{
			/*
			For a strict inequality Z wins the primary length ordering.  At
			equality both strings round-trip, but Z denotes x exactly and has
			distance zero, so it wins the secondary distance ordering.  The
			native 64-bit writer realizes that exact Z without a synthesized
			uint128 division.
			*/
			return ::fast_io::details::
				to_chars_floating_exact_integer_known_size(
					first, exact_integer64.magnitude,
					exact_integer64.size,
					exact_integer64.negative);
		}
	}
	else if (exact_integer_wide.available &&
			 exact_integer_wide.size <= shortest_size)
	{
		/*
		Here the exact value genuinely exceeds uint64.  The same length/distance
		theorem applies to the uint128 magnitude; this branch is a range fallback,
		not a different candidate rule.
		*/
		return ::fast_io::details::
			to_chars_floating_exact_integer_known_size(
				first, exact_integer_wide.magnitude,
				exact_integer_wide.size,
				exact_integer_wide.negative);
	}

	if (!exact_integer64.available && !exact_integer_wide.available)
	{
		/*
		Unavailability of uint128 proves |x| >= 2^128, hence the exact fixed
		magnitude has at least 39 decimal digits because 2^128 > 10^38.
		Binary32/binary64 shortest reserve bounds are both below 39 code units,
		so the DA candidate must win.  The same rendering continuation below is
		used; this observation only proves why no wider integer carrier is
		required.
		*/
		static_assert(
			::fast_io::details::print_rsv_cache<
				floating_type,
				::fast_io::manipulators::floating_format::decimal> < 39u);
	}
	*first = '-';
	auto *const destination{
		first + static_cast<::std::size_t>(fields.sign)};
	auto *shortest_end{
		::fast_io::details::da::print_ascii_shortest<
			floating_type, flags>(destination, converted)};
	if (shortest_end == nullptr)
	{
		/*
		The generic decimal materializer consumes the identical finalized
		carrier when an ISA-specific ASCII layout declines the raw carrier.
		No conversion or size computation is repeated on this fallback.
		*/
		shortest_end =
			::fast_io::details::print_rsvflt_decimal_define_impl<
				floating_type, false, false,
				::fast_io::manipulators::floating_format::decimal,
				false>(
					destination, finalized.m10, finalized.e10);
	}
	/*
	A is strictly shorter than Z, so distance is not consulted.  Retaining the
	single DA result completes the proof of equivalence.
	*/
	return {shortest_end, {}};
}

template <::fast_io::details::my_floating_point T>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
[[nodiscard]] inline constexpr ::fast_io::basic_to_chars_result<char>
to_chars_floating_standard_shortest_da_ascii_proved_ordinary(
	char *first,
	::fast_io::details::punning_result<::std::remove_cv_t<T>>
		fields) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	constexpr auto flags{
		::fast_io::details::to_chars_floating_flags<
			::fast_io::chars_format::general, true,
			::fast_io::manipulators::floating_rounding::
				nearest_to_even>()};
	/*
	The caller has already proved that exact fixed cannot win.  The established
	field renderer therefore produces the final standard result directly.  This
	outlined entry is a placement boundary only: it receives the identical IEC
	fields and flags, performs the identical DA conversion, and writes through
	the same ASCII layout.  It prevents a low-frequency proved-DA integer from
	re-entering the residual size comparison or cloning the full renderer into
	the small exact-integer classifier.
	*/
	return {
		::fast_io::details::print_rsvflt_fields_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e,
			flags.comma, flags.floating, flags.rounding,
			flags.nan_show_sign, flags.nan_show_type,
			flags.json_float, floating_type>(
				first, fields.mantissa, fields.exponent,
				fields.sign),
		{}};
}

template <::fast_io::details::my_floating_point T>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
[[nodiscard]] inline constexpr ::fast_io::basic_to_chars_result<char>
to_chars_floating_standard_shortest_da_ascii_binary32_residual(
	char *first,
	::fast_io::details::punning_result<::std::remove_cv_t<T>>
		fields,
	::fast_io::details::basic_to_chars_floating_small_integer<
		::std::uint_least64_t> exact_integer) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	static_assert(trait::mbits == 23u && trait::ebits == 8u);
	if (::fast_io::details::
			to_chars_floating_exact_fixed_wins_residual_midpoint<
				floating_type>(fields, exact_integer))
	{
		/*
		The selected grid point is an open odd-significand midpoint.  No shorter
		decimal round-trips; exact fixed wins by zero distance.  Capacity was
		proved at the public entry, so the known-size writer is immediately safe.
		*/
		return ::fast_io::details::
			to_chars_floating_exact_integer_known_size(
				first, exact_integer.magnitude,
				exact_integer.size, exact_integer.negative);
	}
	constexpr auto flags{[]() constexpr noexcept {
		auto value{
			::fast_io::details::to_chars_floating_flags<
				::fast_io::chars_format::general, true,
				::fast_io::manipulators::floating_rounding::
					nearest_to_even>()};
		value.floating =
			::fast_io::manipulators::floating_format::scientific;
		return value;
	}()};
	/*
	The only residual alternative to the open odd midpoint is a selected-grid
	decimal in x's rounding cell.  Its complete scientific spelling is strictly
	shorter than the D-digit exact fixed integer.  Every decimal in the cell has
	the same D integer places, except a value beyond an adjacent decade boundary,
	whose fixed form is no shorter.  Hence any standard winner shorter than D
	must itself use scientific notation.  DA may select a still shorter or closer
	coefficient, but changing only the presentation flag to the already-proved
	notation cannot change that carrier or its bytes.

	The input is a finite nonzero integer.  Sign emission followed by the same
	DA conversion and direct scientific ASCII writer is therefore exactly the
	scientific arm of the ordinary decimal renderer, without its fixed-layout
	mask, finalization, or length comparison.
	*/
	*first = '-';
	auto *const destination{
		first + static_cast<::std::size_t>(fields.sign)};
	auto const converted{
		::fast_io::details::da::to_conversion_result<floating_type>(
			fields.mantissa,
			static_cast<::std::int_least32_t>(fields.exponent))};
	return {
		::fast_io::details::da::print_ascii_shortest<
			floating_type, flags>(destination, converted),
		{}};
}

template <::fast_io::details::my_floating_point T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr ::fast_io::basic_to_chars_result<char>
to_chars_floating_standard_shortest_da_ascii_integer_candidate(
	char *first, char *last,
	::fast_io::details::punning_result<::std::remove_cv_t<T>>
		fields) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	auto const exact_integer64{
		::fast_io::details::
			to_chars_floating_make_small_integer_from_fields_as<
				::std::uint_least64_t, floating_type>(fields)};
	if (exact_integer64.available &&
		::fast_io::details::
			to_chars_floating_exact_fixed_must_win_before_da<
				floating_type>(fields, exact_integer64))
	{
		/*
		The grid/cell theorem proves the exact integer is the standard winner
		before constructing a shortest DA carrier.  Keeping its construction,
		grid test, and native integer writer in the public hot function removes
		an otherwise dominant outlined-call boundary for ordinary small
		integers.  The full physical reserve was proved by the public caller, so
		the known-size writer is immediately safe.
		*/
		(void)last;
		return ::fast_io::details::
			to_chars_floating_exact_integer_known_size(
				first, exact_integer64.magnitude,
				exact_integer64.size,
				exact_integer64.negative);
	}
	if constexpr (
		::fast_io::details::iec559_traits<
			floating_type>::mbits == 23u)
	{
		if (exact_integer64.available)
		{
			/*
			The outlined binary32 residual resolves the only open odd midpoint
			and otherwise enters the proved scientific DA arm.  It performs no
			carrier finalization or post-conversion size comparison.
			*/
			return ::fast_io::details::
				to_chars_floating_standard_shortest_da_ascii_binary32_residual<
					floating_type>(
						first, fields, exact_integer64);
		}
	}
	/*
	Binary64 retains the measured general residual comparison, and a binary32
	integer outside uint64 (unreachable under the current fixed-loss interval)
	retains the same range fallback.  Outlining this substantially larger
	continuation keeps conversion state and wide divisions out of the scalar
	common path.
	*/
	return ::fast_io::details::
		to_chars_floating_standard_shortest_da_ascii_integer_candidate_residual<
			floating_type>(
				first, last, fields, exact_integer64);
}

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 12 &&         \
	(defined(__x86_64__) || defined(_M_X64))
template <::fast_io::details::my_floating_point T>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
[[nodiscard]] inline constexpr ::fast_io::basic_to_chars_result<char>
to_chars_floating_standard_shortest_da_ascii_gcc12_noncandidate(
	char *first,
	::fast_io::details::punning_result<::std::remove_cv_t<T>>
		fields) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	constexpr auto flags{
		::fast_io::details::to_chars_floating_flags<
			::fast_io::chars_format::general, true,
			::fast_io::manipulators::floating_rounding::
				nearest_to_even>()};
	/*
	The caller has proved `!require_exact_fixed_arbitration(fields)`, so the
	ordinary DA carrier is already the unique standard winner.  Keeping only
	that dependent renderer in this outlined GCC 12 body prevents its cached
	power and ASCII state from being cloned together with the public
	classifier.  The renderer consumes the original IEC fields directly; no
	floating reconstruction or second classification occurs.
	*/
	return {
		::fast_io::details::print_rsvflt_fields_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e,
			flags.comma, flags.floating, flags.rounding,
			flags.nan_show_sign, flags.nan_show_type,
			flags.json_float, floating_type>(
				first, fields.mantissa, fields.exponent,
				fields.sign),
		{}};
}
#endif

/*
x86 Clang otherwise clones the large DA renderer into this wrapper when the
wrapper is forced into every public call site.  Ordinary inline retains one
shared DA body while the preceding public `__builtin_constant_p` dispatch
remains forced-inline.  This is only a placement decision: both forms receive
the same value, flags, and full-reserve precondition and return the same result.
*/
template <::fast_io::details::my_floating_point T>
#if !(defined(__clang__) &&                                                \
	  (defined(__x86_64__) || defined(_M_X64))) &&                         \
	__has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif !defined(__clang__) && __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr ::fast_io::basic_to_chars_result<char>
to_chars_floating_standard_shortest_da_ascii(
	char *first, char *last, T value) noexcept
{
	using floating_type = ::std::remove_cv_t<T>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	static_assert(
		(trait::mbits == 23u && trait::ebits == 8u) ||
		(trait::mbits == 52u && trait::ebits == 11u));
	constexpr auto flags{
		::fast_io::details::to_chars_floating_flags<
			::fast_io::chars_format::general, true,
			::fast_io::manipulators::floating_rounding::
				nearest_to_even>()};
	auto const fields{
		::fast_io::details::to_chars_floating_capture_fields(value)};

	if (!::fast_io::details::
			to_chars_floating_fields_require_exact_fixed_arbitration<
				floating_type>(fields))
	{
#if defined(__APPLE__) &&                                             \
	(defined(__aarch64__) || defined(__arm64__)) && defined(__clang__)
		if constexpr (trait::mbits == 23u && trait::ebits == 8u)
		{
			constexpr ::std::uint_least32_t exponent_mask{
				(static_cast<::std::uint_least32_t>(1u)
				 << trait::ebits) -
				1u};
			if (fields.exponent != exponent_mask &&
				::fast_io::details::
					print_rsvflt_da_scientific_is_strictly_shorter<
						floating_type>(fields.exponent))
			{
				constexpr auto scientific_flags{[]() constexpr noexcept {
					auto value{
						::fast_io::details::
							to_chars_floating_flags<
								::fast_io::chars_format::general,
								true,
								::fast_io::manipulators::
									floating_rounding::
										nearest_to_even>()};
					value.floating =
						::fast_io::manipulators::
							floating_format::scientific;
					return value;
				}()};
				/*
				For every admitted finite exponent, the presentation theorem
				proves that scientific is strictly shorter for every possible DA
				coefficient length.  The exact-integer predicate is already false,
				so no fixed candidate can replace it.  Writing the sign and
				rendering the identical DA carrier with scientific flags is
				therefore byte-for-byte equal to entering the general decimal
				layout and taking its scientific arm.

				Random binary32 encodings reach these two exponent bands about
				three quarters of the time.  Keeping this proved arm inline lets
				Apple Clang discard the fixed-layout mask, table address and cold
				fallback state from the common scalar dependency chain.
				*/
				*first = '-';
				auto *const destination{
					first +
					static_cast<::std::size_t>(fields.sign)};
				auto const converted{
					::fast_io::details::da::
						to_conversion_result<floating_type>(
							fields.mantissa,
							static_cast<::std::int_least32_t>(
								fields.exponent))};
				return {
					::fast_io::details::da::print_ascii_shortest<
						floating_type, scientific_flags>(
							destination, converted),
					{}};
			}
			return ::fast_io::details::
				to_chars_floating_standard_shortest_da_ascii_proved_ordinary<
					floating_type>(first, fields);
		}
#endif
		/*
		The combined predicate proves either that no exact fixed candidate
		exists or that its complete length cannot win.  Constructing its
		magnitude therefore cannot affect the result.  Classifying before DA
		conversion also prevents the integer-correction state from spanning the
		dependent cached-power and ASCII-render chain on this predominant path.
		Keeping the rejection classes behind one renderer call prevents the
		compiler from cloning that large continuation.
		*/
		return {
			::fast_io::details::print_rsvflt_fields_define_impl<
				flags.showpos, flags.uppercase, flags.uppercase_e,
				flags.comma, flags.floating, flags.rounding,
				flags.nan_show_sign, flags.nan_show_type,
				flags.json_float, floating_type>(
					first, fields.mantissa, fields.exponent,
					fields.sign),
			{}};
	}

	/*
	Only the residual exact-integer class needs the uint128 construction and
	post-render length comparison.  Outlining that class is a placement
	optimization: the helper receives the identical raw fields and invokes the
	identical DA renderer, so its result is still the same pure function of
	(fields, flags).  The common noncandidate branches above no longer carry the
	integer magnitude, decimal digit count, or overwrite control flow through
	the scalar DA dependency chain.
	*/
	return ::fast_io::details::
		to_chars_floating_standard_shortest_da_ascii_integer_candidate<
			floating_type>(
			first, last, fields);
}
#endif

template <::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::details::my_floating_point T,
		  ::fast_io::details::character char_type>
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_floating_standard_fixed(
	char_type *first, char_type *last, T value) noexcept
{
	/*
	For an integral binary value, every fixed spelling has the same mandatory
	integer-place count.  The standard's equal-length tie rule therefore selects
	the exact integer, not a shortest scientific carrier followed by zeroes.
	Nonintegral values retain the shortest fixed carrier.  The field test above
	is the exact divisibility condition m*2^e in Z and performs no floating
	arithmetic or environment-dependent conversion.
	*/
	if (::fast_io::details::to_chars_floating_is_integer(value))
	{
		return ::fast_io::details::to_chars_floating_precision_fixed<
			::fast_io::chars_format::fixed, rounding>(
				first, last, value, 0u);
	}
	return ::fast_io::details::to_chars_floating_fixed<
		::fast_io::chars_format::fixed, rounding>(first, last, value);
}

template <::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::details::my_floating_point T,
		  ::fast_io::details::character char_type>
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_floating_standard_shortest(
	char_type *first, char_type *last, T value) noexcept
{
	if (::fast_io::details::to_chars_floating_is_integer(value))
	{
		if (::fast_io::details::
				to_chars_floating_fixed_cannot_win_shortest(value))
		{
			/*
			The exponent theorem above proves exact fixed is strictly longer,
			so no exact decimal construction or second size pass can affect the
			selected spelling.  Returning the ordinary shortest path here is
			therefore both semantically final and the common large-integer fast
			path.
			*/
			return ::fast_io::details::to_chars_floating_fixed<
				::fast_io::chars_format::general, rounding, true>(
					first, last, value);
		}
		/*
		For an integral x, let D be the number of digits in its exact fixed
		spelling and S the length selected from the shortest decimal carrier.
		Every D-character fixed candidate has the same mandatory place count.
		If D<=S, the standard's primary length ordering selects fixed, and its
		secondary minimum-distance ordering selects exact x (distance zero).
		If S<D, no distance comparison may displace the strictly shorter
		scientific carrier.  Exact reserve sizing obtains D and S without writing,
		so a rejected output range remains untouched.

		This branch is restricted to exact binary integers.  For a nonintegral
		value, precision-zero fixed would round to a different decimal grid and
		cannot participate in the shortest round-trip candidate set.
		*/
		constexpr auto shortest_flags{
			::fast_io::details::to_chars_floating_flags<
				::fast_io::chars_format::general, true, rounding>()};
		auto shortest_source{
			::fast_io::details::make_floating_scalar_manip<
				shortest_flags>(value)};
		using shortest_source_type = decltype(shortest_source);
		constexpr auto shortest_tag{
			::fast_io::io_reserve_type<
				char_type, shortest_source_type>};
		auto const shortest_size{
			print_reserve_precise_size(
				shortest_tag, shortest_source)};

		auto const small_integer{
			::fast_io::details::
				to_chars_floating_make_small_integer(value)};
		if (small_integer.available)
		{
			if (small_integer.size <= shortest_size)
			{
				/*
				The standard orders candidates by total character count first
				and distance second.  `<=` includes the equal-length case, where
				this exact integer has distance zero and is uniquely preferred.
				to_chars_integral_checked first proves full capacity, then emits
				exactly `size` code units, retaining the floating overload's
				no-partial-write contract for every character type.
				*/
				return ::fast_io::details::
					to_chars_integral_checked<10u>(
						first, last, small_integer.magnitude,
						small_integer.negative);
			}
		}
		else
		{
			/*
			Wide residual integers that exceed the native carrier retain the
			exact precision-zero path.  This branch is cold for binary32/64 on
			uint128 targets but is required for binary128 and uint64-only ABIs.
			*/
			constexpr auto fixed_flags{
				::fast_io::details::
					to_chars_floating_precision_flags<
						::fast_io::chars_format::fixed, rounding>()};
			auto fixed_source{
				::fast_io::details::
					make_floating_scalar_manip_precision<fixed_flags>(
						value, 0u)};
			using fixed_source_type = decltype(fixed_source);
			constexpr auto fixed_tag{
				::fast_io::io_reserve_type<
					char_type, fixed_source_type>};
			auto const fixed_size{
				print_reserve_precise_size(
					fixed_tag, fixed_source)};
			if (fixed_size <= shortest_size)
			{
				/*
				The arbitrary-precision size comparison proves the same
				length/distance ordering when no native integer carrier exists.
				*/
				return ::fast_io::details::
					to_chars_floating_precision_fixed<
						::fast_io::chars_format::fixed, rounding>(
							first, last, value, 0u);
			}
		}
	}
	/*
	For nonintegers, or when the shortest carrier is strictly shorter than the
	exact fixed integer, the ordinary shortest interval result is the standard
	winner.  No precision-zero rounding is introduced on this path.
	*/
	return ::fast_io::details::to_chars_floating_fixed<
		::fast_io::chars_format::general, rounding, true>(
			first, last, value);
}

template <::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::details::my_floating_point T,
		  ::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_floating_subnormal_hex(
	char_type *first, char_type *last, T value) noexcept
{
	(void)rounding; // An exact radix-16 expansion has no rounding decision.
	using floating_type = ::std::remove_cv_t<T>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	auto const fields{
		::fast_io::details::to_chars_floating_capture_fields(value)};
	auto mantissa{fields.mantissa};

	::std::uint_least32_t highest_bit{};
	/*
	This helper is called only for exponent==0 and mantissa!=0.  Repeated
	division by two therefore terminates with probe==1, and the iteration count
	is exactly floor(log2(mantissa)).  That bit becomes the explicit leading
	hexadecimal `1` of the normalized subnormal spelling.
	*/
	for (auto probe{mantissa}; 1u < probe; probe >>= 1u)
	{
		++highest_bit;
	}
	auto fractional_digits{
		static_cast<::std::uint_least32_t>((highest_bit + 3u) / 4u)};
	/*
	After removing the leading bit, ceil(highest_bit/4) nibbles cover every
	remaining lower bit.  The lambda aligns nibble `index` immediately below
	the leading bit; its two shift branches are the algebraic cases for a
	nonnegative or negative alignment distance.
	*/
	auto const nibble_at = [mantissa, highest_bit](
							 ::std::uint_least32_t index) constexpr noexcept
	{
		auto const right_position{
			static_cast<::std::int_least32_t>(highest_bit) -
			static_cast<::std::int_least32_t>(4u * (index + 1u))};
		if (0 <= right_position)
		{
			/*
			A nonnegative position selects bits [r,r+3] by a right shift and
			mask.  Since r<=highest_bit<the carrier width, the shift is defined.
			*/
			return static_cast<::std::uint_least32_t>(
				(mantissa >> static_cast<unsigned>(right_position)) &
				static_cast<mantissa_type>(0xfu));
		}
		/*
		For a negative position, at most three zero bits must be appended on
		the right to complete the final nibble.  Left-shifting by that amount
		and masking is exactly the same four-bit window, with no significant
		bit shifted beyond the carrier because this is the terminal partial
		nibble.
		*/
		return static_cast<::std::uint_least32_t>(
			(mantissa << static_cast<unsigned>(-right_position)) &
			static_cast<mantissa_type>(0xfu));
	};
	while (fractional_digits &&
		   nibble_at(fractional_digits - 1u) == 0u)
	{
		/*
		Removing a terminal zero nibble multiplies the written fractional
		coefficient by 16 while reducing its radix scale by 16, so the value is
		unchanged.  The loop stops at the last nonzero nibble and thus proves
		shortest hexadecimal fractional length.
		*/
		--fractional_digits;
	}

	/*
	The largest supported exact spelling is binary128:
	sign + "1." + 28 hexadecimal digits + "p-" + 5 exponent digits.
	Sixty-four code units therefore cover every admitted type independently of
	the destination character width.
	*/
	char_type buffer[64]{};
	auto iter{buffer};
	if (fields.sign)
	{
		/*
		The magnitude derivation is sign-independent; prefixing `-` is the
		exact multiplication by -1 and preserves negative subnormals.
		*/
		*iter++ = ::fast_io::char_literal_v<u8'-', char_type>;
	}
	*iter++ = ::fast_io::char_literal_v<u8'1', char_type>;
	if (fractional_digits)
	{
		/*
		A radix point is necessary iff at least one nonzero fractional nibble
		remains.  Omitting both for an exact power of two yields the shorter
		equivalent spelling `1pE`.
		*/
		*iter++ = ::fast_io::char_literal_v<u8'.', char_type>;
		for (::std::uint_least32_t index{};
			 index != fractional_digits; ++index)
		{
			*iter++ =
				::fast_io::details::charliteralofnumber<char_type, false>(
					static_cast<char8_t>(nibble_at(index)));
		}
	}
	*iter++ = ::fast_io::char_literal_v<u8'p', char_type>;
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u)
		 << (trait::ebits - 1u)) -
		1u)};
	auto const exponent{static_cast<::std::int_least32_t>(
		1 - bias - static_cast<::std::int_least32_t>(trait::mbits) +
		static_cast<::std::int_least32_t>(highest_bit))};
	/*
	A subnormal is mantissa*2^(1-bias-mbits).  Factoring its highest set bit
	into the written leading one adds `highest_bit` to the exponent, giving the
	expression above exactly.
	*/
	iter = ::fast_io::details::with_sign_prt_rsv_exponent_hex_impl<
		trait::e2hexdigits>(iter, exponent);
	auto const size{static_cast<::std::size_t>(iter - buffer)};
	if (static_cast<::std::size_t>(last - first) < size)
	{
		/*
		All construction occurred in local storage, so an insufficient
		destination has not been touched.  Returning `last` satisfies the
		to_chars failure cursor contract.
		*/
		return {last, ::fast_io::charconv_errc::value_too_large};
	}
	/*
	The preceding inequality proves [first,first+size) is inside the caller's
	range.  This scalar copy writes exactly that interval and no SIMD slack.
	*/
	for (::std::size_t index{}; index != size; ++index)
	{
		first[index] = buffer[index];
	}
	return {first + size, {}};
}

template <::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::details::my_floating_point T,
		  ::fast_io::details::character char_type>
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_floating_standard_hex(
	char_type *first, char_type *last, T value) noexcept
{
	auto const fields{
		::fast_io::details::to_chars_floating_capture_fields(value)};
	if (!fields.exponent && fields.mantissa)
	{
		/*
		Only nonzero subnormals need renormalization relative to the stream
		formatter's `0.xxxp(min_normal_exp)` form.  The predicate excludes zero
		and all normal/special encodings exactly by their IEC fields.
		*/
		return ::fast_io::details::to_chars_floating_subnormal_hex<
			rounding>(first, last, value);
	}
	/*
	Normal values, signed zero, infinity, and NaN already use the charconv
	canonical layout in the shared hexadecimal writer, so retaining that hot
	path avoids a duplicate formatter.
	*/
	return ::fast_io::details::to_chars_floating_fixed<
		::fast_io::chars_format::hex, rounding>(first, last, value);
}

} // namespace details

/* Exact decimal already carries its presentation policy.  These overloads
give the proxy the same all-or-error bounded-store contract as native floating
to_chars, including exact-fit buffers and non-ASCII character domains. */
template <::fast_io::details::character char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point floating_type>
	requires(::fast_io::details::
				 print_floating_exact_decimal_supported<flags, floating_type>)
inline constexpr ::fast_io::basic_to_chars_result<char_type> to_chars(
	char_type *first, char_type *last,
	::fast_io::manipulators::exact_decimal_manip_t<
		flags, floating_type> const &value) noexcept
{
	return ::fast_io::details::to_chars_floating_emit(first, last, value);
}

template <::fast_io::details::character char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point floating_type>
	requires(
		::fast_io::details::print_floating_exact_decimal_supported<
			flags, floating_type> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<floating_type>)
inline constexpr ::fast_io::basic_to_chars_result<char_type> to_chars(
	char_type *first, char_type *last,
	::fast_io::manipulators::exact_decimal_field_manip_t<
		flags, floating_type>
		value) noexcept
{
	return ::fast_io::details::to_chars_floating_emit(first, last, value);
}

/* A significant-digit interval also has a value-dependent reserve bound.
The precise protocol selects the same shortest/lower/upper branch a second
time and writes only after the complete spelling is known to fit. */
template <::fast_io::details::character char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point floating_type>
	requires(
		::fast_io::details::floating_precise_range_supported<
			flags, floating_type> &&
		!::fast_io::details::floating_scalar_requires_integer_proxy<
			floating_type>)
inline constexpr ::fast_io::basic_to_chars_result<char_type> to_chars(
	char_type *first, char_type *last,
	::fast_io::manipulators::floating_scalar_precision_range_manip_t<
		flags, floating_type> const &value) noexcept
{
	return ::fast_io::details::to_chars_floating_emit(first, last, value);
}

template <::fast_io::details::character char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point floating_type>
	requires(
		::fast_io::details::floating_precise_range_supported<
			flags, floating_type> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<
			floating_type>)
inline constexpr ::fast_io::basic_to_chars_result<char_type> to_chars(
	char_type *first, char_type *last,
	::fast_io::manipulators::
		floating_scalar_field_precision_range_manip_t<flags, floating_type>
			value) noexcept
{
	return ::fast_io::details::to_chars_floating_emit(first, last, value);
}

template <
	::fast_io::manipulators::floating_rounding rounding =
		::fast_io::manipulators::floating_rounding::nearest_to_even,
	::fast_io::details::my_floating_point T,
	::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars(char_type *first, char_type *last, T value) noexcept
{
	if constexpr (
		rounding ==
		::fast_io::manipulators::floating_rounding::current_environment)
	{
		/*
		The floating environment exposes the four IEC hardware modes.  Each
		switch arm tail-calls the corresponding explicit integer policy, so the
		ambient mode is sampled once and no later floating arithmetic can make
		the result drift.
		*/
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::
			toward_plus_infinity:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					toward_plus_infinity>(first, last, value);
		case ::fast_io::manipulators::floating_rounding::
			toward_minus_infinity:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					toward_minus_infinity>(first, last, value);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::toward_zero>(
					first, last, value);
		default:
			/*
			FE_TONEAREST and every unsupported environment encoding map to
			nearest-to-even, the IEC default returned by
			current_floating_rounding().
			*/
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					nearest_to_even>(first, last, value);
		}
	}
	else
	{
		/*
		An explicit policy is already a compile-time constant.  The common
		binary32/binary64 ASCII nearest-even run-time path is selected here, at
		the public always-inline boundary where __builtin_constant_p still sees
		the caller's expression.  This placement gives a dynamic call one direct
		DA entry instead of first entering the much larger constant/tight-buffer
		dispatcher below.
		*/
#if defined(__SIZEOF_INT128__)
		using floating_type = ::std::remove_cv_t<T>;
		using trait = ::fast_io::details::iec559_traits<floating_type>;
		if constexpr (
			rounding ==
				::fast_io::manipulators::floating_rounding::
					nearest_to_even &&
			::std::same_as<char_type, char> &&
			::fast_io::details::is_ascii<char_type> &&
			((trait::mbits == 23u && trait::ebits == 8u) ||
			 (trait::mbits == 52u && trait::ebits == 11u)))
		{
			bool use_runtime_da{!::std::is_constant_evaluated()};
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
			use_runtime_da =
				use_runtime_da && !__builtin_constant_p(value);
#endif
			if (use_runtime_da)
			{
				constexpr auto shortest_flags{
					::fast_io::details::to_chars_floating_flags<
						::fast_io::chars_format::general, true,
						rounding>()};
				using source_type = decltype(
					::fast_io::details::make_floating_scalar_manip<
						shortest_flags>(value));
				constexpr auto source_tag{
					::fast_io::io_reserve_type<char_type, source_type>};
				constexpr auto reserve_size{
					print_reserve_size(source_tag)};
				auto const capacity{
					static_cast<::std::size_t>(last - first)};
				if (reserve_size <= capacity) [[likely]]
				{
					/*
					The reserve contains every fixed-width DA/ASCII scratch
					store.  The callee may therefore use its single-pass
					renderer and exact-integer winner without a preliminary
					size pass.
					*/
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 12 &&         \
	(defined(__x86_64__) || defined(_M_X64))
					auto const fields{
						::fast_io::details::
							to_chars_floating_capture_fields(value)};
					if (::fast_io::details::
							to_chars_floating_fields_require_exact_fixed_arbitration<
								floating_type>(fields))
					{
						/*
						GCC 12 on x86 clones the large DA continuation when
						the combined wrapper is used here.  The field
						classifier is the proved exact conjunction

						    integer && !fixed_cannot_win.

						Therefore its true arm may enter the identical
						integer-candidate continuation directly.  Its false
						arm enters the outlined ordinary DA body above,
						whose result is already final by the false
						classification.  This compiler-specific placement
						removes the clone without changing the set or order
						of candidate comparisons.
						*/
						return ::fast_io::details::
							to_chars_floating_standard_shortest_da_ascii_integer_candidate<
								floating_type>(
								first, last, fields);
					}
					return ::fast_io::details::
						to_chars_floating_standard_shortest_da_ascii_gcc12_noncandidate<
							floating_type>(
							first, fields);
#else
					return ::fast_io::details::
						to_chars_floating_standard_shortest_da_ascii(
							first, last, value);
#endif
				}
				/*
				An exact-boundary buffer may fit the logical result but not
				the wider scratch stores.  Falling through retains the
				non-writing precise-size check and exact-bounds writer.
				*/
			}
		}
#endif
		return ::fast_io::details::
			to_chars_floating_standard_shortest<rounding>(
				first, last, value);
	}
}

template <
	::fast_io::manipulators::floating_rounding rounding =
		::fast_io::manipulators::floating_rounding::nearest_to_even,
	::fast_io::details::my_floating_point T,
	::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars(char_type *first, char_type *last, T value,
		 ::fast_io::chars_format format) noexcept
{
	if constexpr (
		rounding ==
		::fast_io::manipulators::floating_rounding::current_environment)
	{
		/*
		As in the three-argument overload, environment dispatch substitutes one
		explicit policy while forwarding the format unchanged.  Therefore it
		changes only the rounding interval, never the grammar selection.
		*/
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::
			toward_plus_infinity:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					toward_plus_infinity>(
				first, last, value, format);
		case ::fast_io::manipulators::floating_rounding::
			toward_minus_infinity:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					toward_minus_infinity>(
				first, last, value, format);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::toward_zero>(
				first, last, value, format);
		default:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					nearest_to_even>(
				first, last, value, format);
		}
	}
	else
	{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
		/*
		For a literal format, each conjunction proves both constancy and the
		exact enumerator value before entering its fixed instantiation.  Those
		instantiations are the same callees used by the runtime switch below,
		which proves semantic equivalence by substitution.
		*/
		if (__builtin_constant_p(format) &&
			format == ::fast_io::chars_format::general)
		{
			return ::fast_io::details::to_chars_floating_fixed<
				::fast_io::chars_format::general, rounding>(
				first, last, value);
		}
		if (__builtin_constant_p(format) &&
			format == ::fast_io::chars_format::scientific)
		{
			return ::fast_io::details::to_chars_floating_fixed<
				::fast_io::chars_format::scientific, rounding>(
				first, last, value);
		}
		if (__builtin_constant_p(format) &&
			format == ::fast_io::chars_format::fixed)
		{
			return ::fast_io::details::
				to_chars_floating_standard_fixed<rounding>(
				first, last, value);
		}
		if (__builtin_constant_p(format) &&
			format == ::fast_io::chars_format::hex)
		{
			return ::fast_io::details::
				to_chars_floating_standard_hex<rounding>(
				first, last, value);
		}
#endif
		switch (format)
		{
		/*
		The runtime switch is the disjoint union of the four standard formats.
		General and scientific use their shared carriers directly; fixed and
		hex add only the exact standard-specific branches proved above.
		*/
		case ::fast_io::chars_format::general:
			return ::fast_io::details::to_chars_floating_fixed<
				::fast_io::chars_format::general, rounding>(
				first, last, value);
		case ::fast_io::chars_format::scientific:
			return ::fast_io::details::to_chars_floating_fixed<
				::fast_io::chars_format::scientific, rounding>(
				first, last, value);
		case ::fast_io::chars_format::fixed:
			return ::fast_io::details::
				to_chars_floating_standard_fixed<rounding>(
				first, last, value);
		case ::fast_io::chars_format::hex:
			return ::fast_io::details::
				to_chars_floating_standard_hex<rounding>(
				first, last, value);
		default:
			/*
			No valid grammar corresponds to another bit pattern.  Returning
			`first` before any formatter call proves zero output mutation.
			*/
			return {first, ::fast_io::charconv_errc::invalid_argument};
		}
	}
}

template <
	::fast_io::manipulators::floating_rounding rounding =
		::fast_io::manipulators::floating_rounding::nearest_to_even,
	::fast_io::details::my_floating_point T,
	::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars(char_type *first, char_type *last, T value,
		 ::fast_io::chars_format format, int precision) noexcept
{
	if constexpr (
		rounding ==
		::fast_io::manipulators::floating_rounding::current_environment)
	{
		/*
		Precision is forwarded unchanged through environment dispatch.  Since
		each explicit callee owns the same negative-precision normalization,
		sampling the environment cannot alter precision semantics.
		*/
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::
			toward_plus_infinity:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					toward_plus_infinity>(
				first, last, value, format, precision);
		case ::fast_io::manipulators::floating_rounding::
			toward_minus_infinity:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					toward_minus_infinity>(
				first, last, value, format, precision);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::toward_zero>(
				first, last, value, format, precision);
		default:
			return ::fast_io::to_chars<
				::fast_io::manipulators::floating_rounding::
					nearest_to_even>(
				first, last, value, format, precision);
		}
	}
	else
	{
		if (precision < 0)
		{
			if (format == ::fast_io::chars_format::hex)
			{
				/*
				A negative runtime precision has the printf meaning "precision
				omitted".  For `%a`, omitting precision requests the exact
				radix-16 expansion with only redundant trailing zeroes removed;
				the format-only hexadecimal overload implements exactly that
				spelling.  Delegation therefore preserves both the value and the
				minimal exact digit count, including the subnormal normalization
				proved by to_chars_floating_standard_hex.
				*/
				return ::fast_io::to_chars<rounding>(
					first, last, value, format);
			}
			/*
			Omitted `%f`, `%e`, and `%g` precision is six.  Thus general does
			not become the shortest overload: it rounds to six significant
			decimal digits.  Fixed rounds on the global 10^-6 grid; scientific
			keeps six fractional digits after normalization, hence seven
			significant digits on an exponent-dependent grid.  The format remains
			part of the fixed-template instantiation, so replacing the negative
			sentinel by six selects each of those distinct rules and prevents a
			value-changing unsigned wraparound.
			*/
			precision = 6;
		}
		/*
		At this point precision>=0, so conversion to size_t is value-preserving
		on every supported platform (size_t can represent all nonnegative int
		values).
		*/
		auto const unsigned_precision{
			static_cast<::std::size_t>(precision)};
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
		/*
		Literal-format dispatch supplies the same runtime precision to the same
		four fixed-format templates used below.  `__builtin_constant_p` removes
		only the format branch; it cannot fold or reinterpret the precision.
		*/
		if (__builtin_constant_p(format) &&
			format == ::fast_io::chars_format::general)
		{
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::fast_io::chars_format::general, rounding>(
				first, last, value, unsigned_precision);
		}
		if (__builtin_constant_p(format) &&
			format == ::fast_io::chars_format::scientific)
		{
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::fast_io::chars_format::scientific, rounding>(
				first, last, value, unsigned_precision);
		}
		if (__builtin_constant_p(format) &&
			format == ::fast_io::chars_format::fixed)
		{
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::fast_io::chars_format::fixed, rounding>(
				first, last, value, unsigned_precision);
		}
		if (__builtin_constant_p(format) &&
			format == ::fast_io::chars_format::hex)
		{
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::fast_io::chars_format::hex, rounding>(
				first, last, value, unsigned_precision);
		}
#endif
		switch (format)
		{
		/*
		All cases share the exact quotient/remainder precision engine.  The
		template format changes only radix, grid definition, and final layout
		as recorded in to_chars_floating_precision_flags.
		*/
		case ::fast_io::chars_format::general:
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::fast_io::chars_format::general, rounding>(
				first, last, value, unsigned_precision);
		case ::fast_io::chars_format::scientific:
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::fast_io::chars_format::scientific, rounding>(
				first, last, value, unsigned_precision);
		case ::fast_io::chars_format::fixed:
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::fast_io::chars_format::fixed, rounding>(
				first, last, value, unsigned_precision);
		case ::fast_io::chars_format::hex:
			return ::fast_io::details::to_chars_floating_precision_fixed<
				::fast_io::chars_format::hex, rounding>(
				first, last, value, unsigned_precision);
		default:
			/*
			An invalid format reaches no writer.  The failure therefore returns
			the original cursor and leaves the destination untouched.
			*/
			return {first, ::fast_io::charconv_errc::invalid_argument};
		}
	}
}

} // namespace fast_io
