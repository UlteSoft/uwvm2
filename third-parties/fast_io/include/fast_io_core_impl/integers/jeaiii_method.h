#pragma once

/*
Algorithm: JEAIII
Author: jeaiii
*/

namespace fast_io::details::jeaiii
{

template <::std::integral char_type>
struct jeaiii_pair
{
	char_type elements[2u];
};

template <::std::integral char_type>
struct jeaiii_tables
{
	::fast_io::freestanding::array<jeaiii_pair<char_type>, 100u> digits;
	::fast_io::freestanding::array<jeaiii_pair<char_type>, 100u> first_digits;
};

template <::std::integral char_type>
inline consteval auto generate_jeaiii_tables() noexcept
{
	constexpr auto digit_characters{::fast_io::details::generate_digits_table<char_type, 10, false>()};
	jeaiii_tables<char_type> tables;
	for (::std::size_t i{}; i != 100u; ++i)
	{
		tables.digits[i].elements[0] = digit_characters[i << 1u];
		tables.digits[i].elements[1] = digit_characters[(i << 1u) + 1u];
		tables.first_digits[i] = tables.digits[i];
	}
	for (::std::size_t i{}; i != 10u; ++i)
	{
		tables.first_digits[i].elements[0] = ::fast_io::char_literal_add<char_type>(i);
		tables.first_digits[i].elements[1] = char_type{};
	}
	return tables;
}

template <::std::integral char_type>
alignas(64u) inline static constexpr auto jeaiii_tables_cache{generate_jeaiii_tables<char_type>()};

template <::std::integral char_type>
inline constexpr char_type *jeaiii_after_first_pair(char_type *iter, bool short_form) noexcept
{
	if (::std::is_constant_evaluated())
	{
		return iter + 1u + !short_form;
	}
	::std::uintptr_t address{reinterpret_cast<::std::uintptr_t>(iter) + sizeof(char_type) * 2u};
	address -= static_cast<::std::uintptr_t>(short_form) * sizeof(char_type);
	return reinterpret_cast<char_type *>(address);
}

template <::std::integral char_type>
inline constexpr void jeaiii_w(char_type *iter, ::std::uint_least64_t u) noexcept
{
	constexpr auto const *digitstb{jeaiii_tables_cache<char_type>.digits.data()};
	constexpr ::std::size_t tocopybytes{sizeof(char_type) * 2u};
	::fast_io::details::intrinsics::typed_memcpy(iter, (digitstb + u)->elements, tocopybytes);
}

template <::std::integral char_type>
inline constexpr char_type *jeaiii_first_two(char_type *iter, ::std::uint_least32_t u) noexcept
{
	constexpr auto const *digitstb{jeaiii_tables_cache<char_type>.first_digits.data()};
	bool const single{u < 10u};
	::fast_io::details::intrinsics::typed_memcpy(iter, (digitstb + u)->elements, sizeof(char_type) * 2u);
	return iter + 1u + !single;
}

/*
jeaiii_range4, jeaiii_range6, and jeaiii_range8 are bounded arithmetic leaves
whose callers have already selected the represented digit range.  Requesting
GNU always-inline changes only whether their multiply/table graph is placed in
the caller; the range proofs and returned pointer are unchanged.  A frontend
without the attribute uses ordinary inline semantics as the exact fallback.
No retained artifact isolates the profitability of these three attributes over
the complete supported compiler set, so they remain a conservative legacy
layout policy pending native revalidation and carry no numeric claim for
unmeasured frontends.
*/
template <::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline constexpr char_type *jeaiii_range4(char_type *iter, ::std::uint_least32_t u) noexcept
{
	constexpr auto const *digitstb{jeaiii_tables_cache<char_type>.first_digits.data()};
	constexpr ::std::uint_least64_t mask24{(static_cast<::std::uint_least64_t>(1u) << 24u) - 1u};
	constexpr ::std::uint_least64_t multiplier{
		(static_cast<::std::uint_least64_t>(10u) << 24u) / static_cast<::std::uint_least64_t>(1000u) + 1u};
	::std::uint_least64_t const product{multiplier * u};
	::std::uint_least64_t const first{product >> 24u};
	bool const short_form{u < 1000u};
	::fast_io::details::intrinsics::typed_memcpy(iter, (digitstb + first)->elements,
												 sizeof(char_type) * 2u);
	iter = jeaiii_after_first_pair(iter, short_form);
	::std::uint_least64_t const last{((product & mask24) * 100u) >> 24u};
	jeaiii_w(iter, last);
	return iter + 2u;
}

template <::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline constexpr char_type *jeaiii_range6(char_type *iter, ::std::uint_least32_t u) noexcept
{
	constexpr auto const *digitstb{jeaiii_tables_cache<char_type>.first_digits.data()};
	constexpr ::std::uint_least64_t mask32{(static_cast<::std::uint_least64_t>(1u) << 32u) - 1u};
	constexpr ::std::uint_least64_t multiplier{
		(static_cast<::std::uint_least64_t>(10u) << 32u) / static_cast<::std::uint_least64_t>(100000u) + 1u};
	::std::uint_least64_t product{multiplier * u};
	::std::uint_least64_t const first{product >> 32u};
	bool const short_form{u < 100000u};
	::fast_io::details::intrinsics::typed_memcpy(iter, (digitstb + first)->elements,
												 sizeof(char_type) * 2u);
	iter = jeaiii_after_first_pair(iter, short_form);
	product = (product & mask32) * 100u;
	jeaiii_w(iter, product >> 32u);
	product = (product & mask32) * 100u;
	jeaiii_w(iter + 2u, product >> 32u);
	return iter + 4u;
}

template <::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline constexpr char_type *jeaiii_range8(char_type *iter, ::std::uint_least32_t u) noexcept
{
	constexpr auto const *digitstb{jeaiii_tables_cache<char_type>.first_digits.data()};
	constexpr ::std::uint_least64_t mask32{(static_cast<::std::uint_least64_t>(1u) << 32u) - 1u};
	constexpr ::std::uint_least64_t multiplier{
		(static_cast<::std::uint_least64_t>(10u) << 48u) / static_cast<::std::uint_least64_t>(10000000u) + 1u};
	::std::uint_least64_t product{(multiplier * u) >> 16u};
	::std::uint_least64_t const first{product >> 32u};
	bool const short_form{u < 10000000u};
	::fast_io::details::intrinsics::typed_memcpy(iter, (digitstb + first)->elements,
												 sizeof(char_type) * 2u);
	iter = jeaiii_after_first_pair(iter, short_form);
	product = (product & mask32) * 100u;
	jeaiii_w(iter, product >> 32u);
	product = (product & mask32) * 100u;
	jeaiii_w(iter + 2u, product >> 32u);
	product = (product & mask32) * 100u;
	jeaiii_w(iter + 4u, product >> 32u);
	return iter + 6u;
}

template <::std::integral char_type>
inline constexpr char_type *jeaiii_range10(char_type *iter, ::std::uint_least64_t u) noexcept
{
	constexpr auto const *firsttb{jeaiii_tables_cache<char_type>.first_digits.data()};
	constexpr auto const *digitstb{jeaiii_tables_cache<char_type>.digits.data()};
	constexpr ::std::uint_least64_t mask57{(static_cast<::std::uint_least64_t>(1u) << 57u) - 1u};
	constexpr ::std::uint_least64_t multiplier{
		(static_cast<::std::uint_least64_t>(10u) << 57u) / static_cast<::std::uint_least64_t>(1000000000u) + 1u};
	::std::uint_least64_t product{multiplier * u};
	::std::uint_least64_t const first{product >> 57u};
	bool const short_form{u < static_cast<::std::uint_least64_t>(1000000000u)};
	::fast_io::details::intrinsics::typed_memcpy(iter, (firsttb + first)->elements, sizeof(char_type) * 2u);
	iter = jeaiii_after_first_pair(iter, short_form);
	for (::std::size_t i{}; i != 4u; ++i)
	{
		product = (product & mask57) * 100u;
		::std::uint_least64_t const pair{product >> 57u};
		::fast_io::details::intrinsics::typed_memcpy(iter, (digitstb + pair)->elements, sizeof(char_type) * 2u);
		iter += 2u;
	}
	return iter;
}

template <::std::size_t n, ::std::integral char_type>
inline constexpr ::std::uint_least64_t jeaiii_a(char_type *iter, ::std::uint_least32_t u) noexcept
{
	constexpr ::std::uint_least64_t one{1u};
	constexpr ::std::uint_least64_t v{n / 5u * n * 53u / 16u};
	constexpr ::std::uint_least64_t constant{
		(one << (32u + v)) / ::fast_io::details::compile_pow10<::std::uint_least64_t, n> + 1u + n / 6u - n / 8u};
	::std::uint_least64_t t{constant * u};
	t >>= v;
	constexpr ::std::uint_least64_t add_factor{n / 6u * 4u};
	if constexpr (add_factor != 0u)
	{
		t += add_factor;
	}
	jeaiii_w(iter, static_cast<::std::uint_fast32_t>(t >> 32u));
	return t;
}

template <::std::size_t n, ::std::integral char_type>
inline constexpr void jeaiii_s(char_type *iter, ::std::uint_least64_t t) noexcept
{
	constexpr ::std::uint_least64_t ten{10u};
	iter[n] = ::fast_io::char_literal_add<char_type>((ten * static_cast<::std::uint_least32_t>(t)) >> 32u);
}

template <::std::size_t n, bool last = false, ::std::integral char_type>
inline constexpr auto jeaiii_d(char_type *iter, ::std::uint_least64_t t) noexcept
{
	constexpr ::std::uint_least64_t hundred{100u};
	jeaiii_w(iter + n, static_cast<::std::uint_least32_t>((t = hundred * static_cast<::std::uint_least32_t>(t)) >> 32));
	if constexpr (!last)
	{
		return t;
	}
}

template <::std::size_t n, ::std::integral char_type>
inline constexpr void jeaiii_c(char_type *iter, ::std::uint_least32_t u) noexcept
{
	if constexpr (n == 0)
	{
		*iter = ::fast_io::char_literal_add<char_type>(u);
	}
	else if constexpr (n == 1)
	{
		jeaiii_w(iter, u);
	}
	else if constexpr (n == 2)
	{
		jeaiii_s<2>(iter, jeaiii_a<1>(iter, u));
	}
	else if constexpr (n == 3)
	{
		jeaiii_d<2, true>(iter, jeaiii_a<2>(iter, u));
	}
	else if constexpr (n == 4)
	{
		jeaiii_s<4>(iter, jeaiii_d<2>(iter, jeaiii_a<3>(iter, u)));
	}
	else if constexpr (n == 5)
	{
		jeaiii_d<4, true>(iter, jeaiii_d<2>(iter, jeaiii_a<4>(iter, u)));
	}
	else if constexpr (n == 6)
	{
		jeaiii_s<6>(iter, jeaiii_d<4>(iter, jeaiii_d<2>(iter, jeaiii_a<5>(iter, u))));
	}
	else if constexpr (n == 7)
	{
		constexpr ::std::uint_least64_t multiplier{
			(static_cast<::std::uint_least64_t>(1u) << 48u) / static_cast<::std::uint_least64_t>(1000000u) + 1u};
		::std::uint_least64_t t{((multiplier * u) >> 16u) + 1u};
		jeaiii_w(iter, t >> 32u);
		t = static_cast<::std::uint_least32_t>(t) * static_cast<::std::uint_least64_t>(100u);
		jeaiii_w(iter + 2u, t >> 32u);
		t = static_cast<::std::uint_least32_t>(t) * static_cast<::std::uint_least64_t>(100u);
		jeaiii_w(iter + 4u, t >> 32u);
		t = static_cast<::std::uint_least32_t>(t) * static_cast<::std::uint_least64_t>(100u);
		jeaiii_w(iter + 6u, t >> 32u);
	}
	else if constexpr (n == 8)
	{
		::std::uint_least64_t t{jeaiii_a<7>(iter, u)};
		t = jeaiii_d<2>(iter, t);
		t = jeaiii_d<4>(iter, t);
		t = jeaiii_d<6>(iter, t);
		jeaiii_s<8>(iter, t);
	}
	else if constexpr (n == 9)
	{
		::std::uint_least64_t t{jeaiii_a<8>(iter, u)};
		t = jeaiii_d<2>(iter, t);
		t = jeaiii_d<4>(iter, t);
		t = jeaiii_d<6>(iter, t);
		jeaiii_d<8, true>(iter, t);
	}
}

template <::std::size_t n, ::std::integral char_type>
inline constexpr char_type *jeaiii_f(char_type *iter, ::std::uint_least32_t u) noexcept
{
	constexpr ::std::size_t np1{n + 1};
	jeaiii_c<n>(iter, u);
	return iter + np1;
}

/// @brief Holds the division result and the first JEAIII multiplier state for a 17--20 digit unsigned value.
/// @details Every uint64 value in [10^16, 2^64) has one variable-width leading block followed by two exact eight-digit
///          blocks.  The three decimal blocks uniquely reconstruct the input as
///          `top * 10^16 + middle * 10^8 + low`.  Preparing the first fixed-block multiplier is exact because each
///          remainder is strictly below 10^8, which is precisely the precondition of `jeaiii_c<7>`.
struct jeaiii_u64_long_state
{
	::std::uint_least64_t middle;
	::std::uint_least64_t low;
	::std::uint_least32_t top;
};

/// @brief Advances an exact eight-digit JEAIII block to the first table-lookup state.
/// @param value an integer in [0, 10^8)
/// @return the fixed-point accumulator consumed by four successive two-digit emissions
[[nodiscard]] inline constexpr ::std::uint_least64_t
jeaiii_prepare_fixed_eight_digits(::std::uint_least32_t value) noexcept
{
	constexpr ::std::uint_least64_t multiplier{
		(static_cast<::std::uint_least64_t>(1u) << 48u) /
			static_cast<::std::uint_least64_t>(1000000u) +
		1u};
	return ((multiplier * value) >> 16u) + 1u;
}

/// @brief Splits a proved 17--20 digit uint64 value and prepares both exact-width tails.
/// @details Division by 10^8 yields `high` and `low`; division by 10^16 then separates `high` into `top` and
///          `middle`.  Both subtractions are exact Euclidean remainders.  Compilers lower the constant divisions to
///          independent multiply-high sequences, so preparing adjacent values before emission exposes those chains
///          to out-of-order execution without changing a digit or performing temporary character stores.
/// @param value an integer in [10^16, 2^64)
/// @return the complete arithmetic state needed for character emission
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr jeaiii_u64_long_state
jeaiii_prepare_u64_long(::std::uint_least64_t value) noexcept
{
	constexpr ::std::uint_least64_t divisor8{100000000u};
	constexpr ::std::uint_least64_t divisor16{divisor8 * divisor8};
	auto const high{value / divisor8};
	auto const low{static_cast<::std::uint_least32_t>(value - high * divisor8)};
	auto const top{static_cast<::std::uint_least32_t>(value / divisor16)};
	auto const middle{static_cast<::std::uint_least32_t>(
		high - static_cast<::std::uint_least64_t>(top) * divisor8)};
	return {jeaiii_prepare_fixed_eight_digits(middle),
			jeaiii_prepare_fixed_eight_digits(low), top};
}

/// @brief Emits one prepared exact eight-digit block.
/// @details At each step the accumulator's high word is the next base-100 digit pair and multiplying its low word by
///          100 advances the fixed-point fraction.  This is the same recurrence as `jeaiii_c<7>` with its first
///          multiply moved into the independent preparation phase.
template <::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
jeaiii_emit_fixed_eight_digits(char_type *iter, ::std::uint_least64_t prepared) noexcept
{
	for (::std::size_t offset{}; offset != 8u; offset += 2u)
	{
		jeaiii_w(iter + offset, prepared >> 32u);
		prepared = static_cast<::std::uint_least32_t>(prepared) *
				   static_cast<::std::uint_least64_t>(100u);
	}
	return iter + 8u;
}

/// @brief Emits a prepared 17--20 digit uint64 value without repeating division or tail setup.
/// @details `top` lies in [1, 1844].  The two-way first/range4 classifier therefore covers its complete one-to-four
///          digit domain, after which both tail blocks have exactly eight digits, including leading zeroes.
template <::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
jeaiii_emit_u64_long(char_type *iter, jeaiii_u64_long_state const &state) noexcept
{
	if (state.top < 100u)
	{
		iter = jeaiii_first_two(iter, state.top);
	}
	else
	{
		iter = jeaiii_range4(iter, state.top);
	}
	iter = jeaiii_emit_fixed_eight_digits(iter, state.middle);
	return jeaiii_emit_fixed_eight_digits(iter, state.low);
}

/// @brief Converts a proved 17--20 digit uint64 value through the shared prepared representation.
/// @details The ordinary single-value classifier intentionally does not call this convenience wrapper. M4 per-width
///          A/B probes showed that inserting the long arm before classification regressed 9--14 digits, while moving
///          or outlining it later still perturbed 11--14-digit code placement. Two-value staged emission obtains its
///          gain without changing that mature scalar function, so the prepared kernel remains isolated here.
template <::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
jeaiii_u64_long(char_type *iter, ::std::uint_least64_t value) noexcept
{
	return jeaiii_emit_u64_long(iter, jeaiii_prepare_u64_long(value));
}

template <::std::size_t left, ::std::size_t right, ::std::integral char_type>
/*
Outlining this bounded classification tree changes code placement only; every
leaf still calls the same exact-width JEAIII formatter and returns the same end
pointer.  GNU and MSVC attribute spellings express the same request, while an
unsupported frontend retains ordinary inline semantics.  The audited reports
do not contain an isolated native comparison for this boundary, so no
front-end or throughput benefit is claimed: it is a conservative legacy
code-size policy pending native revalidation.
*/
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr char_type *jeaiii_tree(char_type *iter, ::std::uint_least32_t u) noexcept
{
	static_assert(left <= right);
/*
binary search tree
*/
#if 0
	if constexpr(left==0&&right==9)
	{
		if(u<::fast_io::details::compile_pow10<::std::uint_least32_t,2>)
		{
			if(u<::fast_io::details::compile_pow10<::std::uint_least32_t,1>)
			{
				*iter=::fast_io::char_literal_add<char_type>(u);
				return iter+1;
			}
			else
			{
				return jeaiii_f<1>(iter,u);
			}
		}
		else if(u<::fast_io::details::compile_pow10<::std::uint_least32_t,6>)
		{
			if(u<::fast_io::details::compile_pow10<::std::uint_least32_t,4>)
			{
				if(u<::fast_io::details::compile_pow10<::std::uint_least32_t,3>)
				{
					return jeaiii_f<2>(iter,u);
				}
				else
				{
					return jeaiii_f<3>(iter,u);
				}
			}
			else if(u<::fast_io::details::compile_pow10<::std::uint_least32_t,5>)
			{
				return jeaiii_f<4>(iter,u);
			}
			else
			{
				return jeaiii_f<5>(iter,u);
			}
		}
		else if(u<::fast_io::details::compile_pow10<::std::uint_least32_t,8>)
		{
			if(u<::fast_io::details::compile_pow10<::std::uint_least32_t,7>)
			{
				return jeaiii_f<6>(iter,u);
			}
			else
			{
				return jeaiii_f<7>(iter,u);
			}
		}
		else
		{
			if(u<::fast_io::details::compile_pow10<::std::uint_least32_t,9>)
			{
				return jeaiii_f<8>(iter,u);
			}
			else
			{
				return jeaiii_f<9>(iter,u);
			}
		}
	}
	else
#endif
	{
		if constexpr (left == 0 && right == 7)
		{
			if (u < 10000u)
			{
				if (u < 100u)
				{
					return jeaiii_first_two(iter, u);
				}
				return u < 1000u ? jeaiii_f<2>(iter, u) : jeaiii_f<3>(iter, u);
			}
			if (u < 1000000u)
			{
				return u < 100000u ? jeaiii_f<4>(iter, u) : jeaiii_f<5>(iter, u);
			}
			return u < 10000000u ? jeaiii_f<6>(iter, u) : jeaiii_f<7>(iter, u);
		}
		else if constexpr (left == 2 && right == 7)
		{
			if (u < 10000u)
			{
				return jeaiii_range4(iter, u);
			}
			if (u < 1000000u)
			{
				return jeaiii_range6(iter, u);
			}
			return jeaiii_range8(iter, u);
		}
		else if constexpr (left == right)
		{
			return jeaiii_f<right>(iter, u);
		}
		else if constexpr (left + 1 == right)
		{
			if (u < (::fast_io::details::compile_pow10<::std::uint_least32_t, right>))
			{
				return jeaiii_f<left>(iter, u);
			}
			else
			{
				return jeaiii_f<right>(iter, u);
			}
		}
		else if constexpr (left + 2 == right)
		{
			if (u < (::fast_io::details::compile_pow10<::std::uint_least32_t, left + 1>))
			{
				return jeaiii_f<left>(iter, u);
			}
			else
			{
				return jeaiii_tree<left + 1, right>(iter, u);
			}
		}
		else if constexpr (left == 0)
		{
			if (u < 100u)
			{
				return jeaiii_tree<0, 1>(iter, u);
			}
			else
			{
				return jeaiii_tree<2, right>(iter, u);
			}
		}
		else
		{
			constexpr ::std::size_t middle{(left + right) / 2};
			if (u < (::fast_io::details::compile_pow10<::std::uint_least32_t, middle + 1>))
			{
				return jeaiii_tree<left, middle>(iter, u);
			}
			else
			{
				return jeaiii_tree<middle + 1, right>(iter, u);
			}
		}
	}
}

template <typename result_type, ::std::integral char_type>
inline constexpr result_type jeaiii_result(char_type *iter) noexcept
{
	if constexpr (::std::same_as<result_type, char_type *>)
	{
		return iter;
	}
	else
	{
		return {iter, {}};
	}
}

/*
single_digit_checked records a caller-to-callee proof, not an optional
correctness check.  It may be true only when the top-level caller has already
established n >= 10.  The public result-returning entry needs the complementary
case below because jeaiii_first_two deliberately copies a two-code-unit table
entry; for n < 10 that entry contains the digit followed by a zero code unit.
Internal reserve formatters returning char_type * retain that staging behavior,
whereas the bounded public result path writes exactly one code unit and returns
iter + 1.

The check is limited to a full, non-Ryu, non-recursive result entry.  Recursive
JEAIII calls format decimal chunks with their own width/position invariants, so
turning a chunk into the top-level short form would be incorrect.  Pointer-only
and Ryu callers likewise preserve the established primitive contract.  For a
128-bit top-level value, the proof is made before any narrowing; if its high
half is zero, narrowing cannot turn an already-proved n >= 10 into one digit,
and recursive calls retain their own conservative default.

Encoding the proved state in the specialization lets a caller-side check
remove the duplicate comparison and gives that code-generation choice a
distinct specialization and mangled identity.  Current callers select the
caller-side placement for AArch64 from native M4 measurements plus
cross-target static evidence, and for x86-64 only on the measured GCC 15 and
Clang 21 combinations.  Unmeasured AArch64 cores and compiler lowerings are not
native performance evidence.  The default remains false for correctness and
conservative code generation.  char_literal_add is used rather than an ASCII
offset, so all supported character types and execution character sets,
including EBCDIC char, produce the proper digit.
*/
template <bool ryu_mode = false, bool recursive = false, ::std::integral char_type,
		  typename result_type = char_type *, bool single_digit_checked = false,
		  ::fast_io::details::my_unsigned_integral U>
inline constexpr result_type jeaiii_main(char_type *iter, U n) noexcept
{
	if constexpr (!single_digit_checked && !::std::same_as<result_type, char_type *> &&
				  !ryu_mode && !recursive)
	{
		if (n < 10u)
		{
			*iter = ::fast_io::char_literal_add<char_type>(n);
			return jeaiii_result<result_type>(iter + 1u);
		}
	}
	if constexpr (sizeof(U) > sizeof(::std::uint_least64_t) && sizeof(U) == 16) //__uint128_t
	{
		if (static_cast<::std::uint_least64_t>(n >> 64u) == 0)
		{
			return jeaiii_result<result_type>(jeaiii_main<false, false, char_type, char_type *, false>(
				iter, static_cast<::std::uint_least64_t>(n)));
		}
		constexpr ::std::uint_least64_t divisor{static_cast<::std::uint_least64_t>(10000000000) *
												static_cast<::std::uint_least64_t>(1000000000)};
		U a{n / divisor};
		::std::uint_least64_t u{static_cast<::std::uint_least64_t>(n % divisor)};
		::std::uint_least64_t alow{static_cast<::std::uint_least64_t>(a)};
		if constexpr (ryu_mode)
		{
			iter = jeaiii_main<false, false, char_type, char_type *, false>(
				iter, static_cast<::std::uint_least64_t>(alow));
		}
		else
		{
			if (a != static_cast<U>(alow))
			{
				::std::uint_least32_t v{static_cast<::std::uint_least32_t>(a / divisor)};
				::std::uint_least64_t m{static_cast<::std::uint_least64_t>(a % divisor)};
				jeaiii_c<0>(iter, v);
				++iter;
				alow = m;
				iter = jeaiii_main<false, true, char_type, char_type *, false>(
					iter, static_cast<::std::uint_least64_t>(alow));
			}
			else
			{
				iter = jeaiii_main<false, false, char_type, char_type *, false>(
					iter, static_cast<::std::uint_least64_t>(alow));
			}
		}
		return jeaiii_result<result_type>(
			jeaiii_main<false, true, char_type, char_type *, false>(
				iter, static_cast<::std::uint_least64_t>(u)));
	}
	else if constexpr (sizeof(U) == sizeof(::std::uint_least64_t))
	{
		if constexpr (!ryu_mode && !recursive)
		{
			constexpr ::std::uint_least32_t divisor8{100000000u};
			if (n < 100u)
			{
				return jeaiii_result<result_type>(jeaiii_first_two(iter, static_cast<::std::uint_least32_t>(n)));
			}
			if (n < divisor8)
			{
				::std::uint_least32_t const u{static_cast<::std::uint_least32_t>(n)};
				/*
				Targets admitted by the __x86_64__/_M_X64 guard classify exact widths
				with ordered magnitude tests; for example, u in [1000,9999] is exactly
				the precondition of jeaiii_f<3>.
				The non-x86 range helpers perform the same classification and emit the
				same table entries.  An isolated M4 replacement by the exact-width tree was
				mixed: the affected 3--8-digit aggregate was 1.0039x, while 64- and
				128-bit carriers regressed to 0.9548x--0.9734x and AArch64 code grew by
				about 36--39%.  The shared range layout is therefore retained; the
				Cortex/Neoverse size figures are static code generation, not native timing.

				No retained native x86 A/B result isolates the positive value of the
				exact-width arm.  It remains a conservative legacy x86 code-generation
				policy pending native revalidation; admitted but unmeasured x86 compilers
				and cores inherit only the range proof, not a numeric speed claim.  This
				native-x86 guard excludes ARM64EC, which uses the semantically equivalent
				range4/range6/range8 fallback.
				*/
#if (defined(__x86_64__) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
				if (u < 10000u)
				{
					return jeaiii_result<result_type>(u < 1000u ? jeaiii_f<2>(iter, u) : jeaiii_f<3>(iter, u));
				}
				if (u < 1000000u)
				{
					return jeaiii_result<result_type>(u < 100000u ? jeaiii_f<4>(iter, u) : jeaiii_f<5>(iter, u));
				}
				return jeaiii_result<result_type>(u < 10000000u ? jeaiii_f<6>(iter, u) : jeaiii_f<7>(iter, u));
#else
				if (u < 10000u)
				{
					return jeaiii_result<result_type>(jeaiii_range4(iter, u));
				}
				if (u < 1000000u)
				{
					return jeaiii_result<result_type>(jeaiii_range6(iter, u));
				}
				return jeaiii_result<result_type>(jeaiii_range8(iter, u));
#endif
			}
			/*
			At this point n >= 10^8.  On AArch64 the exact-width branch and the
			shared split form a complete, disjoint partition:

			  10^8 <= n < 10^9: jeaiii_f<8> emits the exact nine-digit spelling
			  n >= 10^9: high = floor(n / 10^8), low = n mod 10^8
			             shortest(n) = shortest(high) || fixed_width_8(low)

			The second precondition proves high >= 10; the quotient/remainder
			identity proves the character sequence and boundary. On Apple M4,
			Clang 23 is the first verified frontend where marking the isolated
			nine-digit arm unlikely keeps the 10--20-digit fallthrough as the layout
			priority: the emitted converter contracts from 304 to 275 instructions,
			paired nine-digit timing remains neutral, and every measured 10--20-digit
			width is neutral or faster. Later Clang versions inherit that narrowly
			targeted layout contract; non-Apple AArch64, earlier Clang, and other
			frontends retain the unannotated branch because no Cortex/Neoverse
			performance claim is inferred from the M4 result.
			*/
#if defined(__aarch64__) || defined(_M_ARM64)
			if (n < static_cast<::std::uint_least64_t>(1000000000u))
#if defined(__APPLE__) && defined(__clang__) && 23 <= __clang_major__
				[[unlikely]]
#endif
			{
				return jeaiii_result<result_type>(
					jeaiii_f<8>(iter, static_cast<::std::uint_least32_t>(n)));
			}
#else
			if (n < static_cast<::std::uint_least64_t>(1000000000u))
			{
				/*
				AVX-VNNI uses one ISA-wide nine-digit policy.  The exact-width f<8>
				kernel was 1.043x--1.402x faster than range10 in strict native tests
				across GCC 13--16 and Clang 18--21, while range10 was 21--30% larger
				in the static code-size probes.  llvm-mca disagreed for some GCC 15/16
				Intel models, so the whole-call native result, rather than the isolated
				static region, selects the path.  Removing Zen tune exclusions prevents
				microarchitecture names from changing the algorithm under the same ISA
				contract.  Frontends and compiler versions outside GCC 13--16 and Clang
				18--21 inherit the semantically equivalent AVX-VNNI choice without a
				native performance claim; targets without the feature macro retain
				range10.
				*/
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__AVXVNNI__) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
				return jeaiii_result<result_type>(jeaiii_f<8>(iter, static_cast<::std::uint_least32_t>(n)));
#else
				return jeaiii_result<result_type>(jeaiii_range10(iter, n));
#endif
			}
#endif
			::std::uint_least64_t const high{n / divisor8};
			::std::uint_least32_t const low{static_cast<::std::uint_least32_t>(n % divisor8)};
			if (high < divisor8)
			{
				if (high < 100u)
				{
					iter = jeaiii_first_two(iter, static_cast<::std::uint_least32_t>(high));
				}
				else if (high < 10000u)
				{
					iter = jeaiii_range4(iter, static_cast<::std::uint_least32_t>(high));
				}
				else if (high < 1000000u)
				{
					iter = jeaiii_range6(iter, static_cast<::std::uint_least32_t>(high));
				}
				else
				{
					iter = jeaiii_range8(iter, static_cast<::std::uint_least32_t>(high));
				}
			}
			else
			{
				constexpr ::std::uint_least64_t divisor16{static_cast<::std::uint_least64_t>(divisor8) * divisor8};
				::std::uint_least32_t const high_first{static_cast<::std::uint_least32_t>(n / divisor16)};
				::std::uint_least32_t const high_low{
					static_cast<::std::uint_least32_t>(high - static_cast<::std::uint_least64_t>(high_first) * divisor8)};
				if (high_first < 100u)
				{
					iter = jeaiii_first_two(iter, high_first);
				}
				else
				{
					iter = jeaiii_range4(iter, high_first);
				}
				iter = jeaiii_f<7>(iter, high_low);
			}
			return jeaiii_result<result_type>(jeaiii_f<7>(iter, low));
		}
		constexpr ::std::uint_least32_t divisor{1000000000u};
		if constexpr (recursive)
		{
			constexpr ::std::uint_least64_t divisor18{static_cast<::std::uint_least64_t>(divisor) * divisor};
			::std::uint_least64_t high{n / divisor};
			::std::uint_least32_t low{static_cast<::std::uint_least32_t>(n % divisor)};
			::std::uint_least32_t high_first{static_cast<::std::uint_least32_t>(n / divisor18)};
			::std::uint_least32_t high_low{static_cast<::std::uint_least32_t>(
				high - static_cast<::std::uint_least64_t>(high_first) * divisor)};
			jeaiii_c<0>(iter, high_first);
			++iter;
			iter = jeaiii_f<8>(jeaiii_f<8>(iter, high_low), low);
		}
		else
		{
			if (n < static_cast<::std::uint_least64_t>(1000000000u))
			{
				return jeaiii_result<result_type>(jeaiii_tree<0, 9>(iter, static_cast<::std::uint_least32_t>(n)));
			}
			::std::uint_least64_t a{n / divisor};
			::std::uint_least32_t u{static_cast<::std::uint_least32_t>(n % divisor)};
			::std::uint_least32_t alow{static_cast<::std::uint_least32_t>(a)};
			if constexpr (ryu_mode)
			{
				iter = jeaiii_tree<0, 7>(iter, alow);
			}
			else
			{
				if (a != static_cast<::std::uint_least64_t>(alow))
				{
					constexpr ::std::uint_least64_t divisor18{static_cast<::std::uint_least64_t>(divisor) * divisor};
					::std::uint_least32_t v{static_cast<::std::uint_least32_t>(n / divisor18)};
					alow = static_cast<::std::uint_least32_t>(
						a - static_cast<::std::uint_least64_t>(v) * divisor);
					if (v < 10u)
					{
						jeaiii_c<0>(iter, v);
						++iter;
					}
					else
					{
						jeaiii_w(iter, v);
						iter += 2;
					}
					iter = jeaiii_f<8>(iter, alow);
				}
				else
				{
					iter = jeaiii_tree<0, 9>(iter, alow);
				}
			}
			iter = jeaiii_f<8>(iter, u);
		}
		return jeaiii_result<result_type>(iter);
	}
	else
	{
		static_assert(!recursive);
		if constexpr (ryu_mode)
		{
			return jeaiii_result<result_type>(jeaiii_tree<0, 8>(iter, n));
		}
		else
		{
			constexpr ::std::uint_least32_t divisor8{100000000u};
			if (n < 100u)
			{
				return jeaiii_result<result_type>(jeaiii_first_two(iter, n));
			}
			if (n < divisor8)
			{
				// Mirror the uint_least64_t-storage-width split above.  Exact bounds
				// prove every fixed-width call; the branch admitted by the
				// __x86_64__/_M_X64 guard and the range-helper branch are semantically
				// equivalent.  ARM64EC uses the range-helper fallback; native x86 retains
				// the legacy-policy status, pending revalidation, and no-performance-claim
				// boundary recorded at the primary split.
#if (defined(__x86_64__) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
				if (n < 10000u)
				{
					return jeaiii_result<result_type>(n < 1000u ? jeaiii_f<2>(iter, n) : jeaiii_f<3>(iter, n));
				}
				if (n < 1000000u)
				{
					return jeaiii_result<result_type>(n < 100000u ? jeaiii_f<4>(iter, n) : jeaiii_f<5>(iter, n));
				}
				return jeaiii_result<result_type>(n < 10000000u ? jeaiii_f<6>(iter, n) : jeaiii_f<7>(iter, n));
#else
				if (n < 10000u)
				{
					return jeaiii_result<result_type>(jeaiii_range4(iter, n));
				}
				if (n < 1000000u)
				{
					return jeaiii_result<result_type>(jeaiii_range6(iter, n));
				}
				return jeaiii_result<result_type>(jeaiii_range8(iter, n));
#endif
			}
			/*
			Mirror the ISA-wide AVX-VNNI nine-digit choice above for the narrower
			integer branch; the exact same bound proves the fixed width.  It inherits
			the GCC 13--16/Clang 18--21 native matrix, static-size result, llvm-mca
			disagreement, and unmeasured-frontend caveat recorded above.  Without
			AVX-VNNI, range10 remains the semantically equivalent fallback below.
			*/
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__AVXVNNI__) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
			if (n < static_cast<::std::uint_least64_t>(1000000000u))
			{
				return jeaiii_result<result_type>(jeaiii_f<8>(iter, n));
			}
#endif
			return jeaiii_result<result_type>(jeaiii_range10(iter, n));
		}
	}
}

template <::std::size_t n, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void jeaiii_hash(char_type *iter, ::std::uint_least32_t u, ::std::uint_least32_t len) noexcept
{
	if constexpr (n == 7)
	{
		switch (len)
		{
		case 1:
		{
			jeaiii_c<0>(iter, u);
			return;
		}
		case 2:
		{
			jeaiii_c<1>(iter, u);
			return;
		}
		case 3:
		{
			jeaiii_c<2>(iter, u);
			return;
		}
		case 4:
		{
			jeaiii_c<3>(iter, u);
			return;
		}
		case 5:
		{
			jeaiii_c<4>(iter, u);
			return;
		}
		case 6:
		{
			jeaiii_c<5>(iter, u);
			return;
		}
		case 7:
		{
			jeaiii_c<6>(iter, u);
			return;
		}
		default:
		{
			jeaiii_c<7>(iter, u);
			return;
		}
		}
	}
	else if constexpr (n == 8)
	{
		switch (len)
		{
		case 1:
		{
			jeaiii_c<0>(iter, u);
			return;
		}
		case 2:
		{
			jeaiii_c<1>(iter, u);
			return;
		}
		case 3:
		{
			jeaiii_c<2>(iter, u);
			return;
		}
		case 4:
		{
			jeaiii_c<3>(iter, u);
			return;
		}
		case 5:
		{
			jeaiii_c<4>(iter, u);
			return;
		}
		case 6:
		{
			jeaiii_c<5>(iter, u);
			return;
		}
		case 7:
		{
			jeaiii_c<6>(iter, u);
			return;
		}
		case 8:
		{
			jeaiii_c<7>(iter, u);
			return;
		}
		default:
		{
			jeaiii_c<8>(iter, u);
			return;
		}
		}
	}
	else if constexpr (n == 9)
	{
		switch (len)
		{
		case 1:
		{
			jeaiii_c<0>(iter, u);
			return;
		}
		case 2:
		{
			jeaiii_c<1>(iter, u);
			return;
		}
		case 3:
		{
			jeaiii_c<2>(iter, u);
			return;
		}
		case 4:
		{
			jeaiii_c<3>(iter, u);
			return;
		}
		case 5:
		{
			jeaiii_c<4>(iter, u);
			return;
		}
		case 6:
		{
			jeaiii_c<5>(iter, u);
			return;
		}
		case 7:
		{
			jeaiii_c<6>(iter, u);
			return;
		}
		case 8:
		{
			jeaiii_c<7>(iter, u);
			return;
		}
		case 9:
		{
			jeaiii_c<8>(iter, u);
			return;
		}
		default:
		{
			jeaiii_c<9>(iter, u);
			return;
		}
		}
	}
	else
	{
		static_assert(n == SIZE_MAX, "no supported");
	}
}

template <bool ryu_mode = false, bool recursive = false, ::std::integral char_type,
		  ::fast_io::details::my_unsigned_integral U>
inline constexpr void jeaiii_main_len(char_type *iter, U n, ::std::uint_least32_t len) noexcept
{
	if constexpr (sizeof(U) > sizeof(::std::uint_least64_t) && sizeof(U) == 16) //__uint128_t
	{
		if (static_cast<::std::uint_least64_t>(n >> 64u) == 0)
		{
			return jeaiii_main_len(iter, static_cast<::std::uint_least64_t>(n), len);
		}
		constexpr ::std::uint_least32_t full_length{19u};
		constexpr ::std::uint_least64_t divisor{static_cast<::std::uint_least64_t>(10000000000) *
												static_cast<::std::uint_least64_t>(1000000000)};
		U a{n / divisor};
		::std::uint_least64_t u{static_cast<::std::uint_least64_t>(n % divisor)};
		::std::uint_least64_t alow{static_cast<::std::uint_least64_t>(a)};
		if constexpr (ryu_mode)
		{
			::std::uint_least32_t len_sub{len - full_length};
			jeaiii_main_len(iter, static_cast<::std::uint_least64_t>(alow), len_sub);
			iter += len_sub;
		}
		else
		{
			if (a != static_cast<U>(alow))
			{
				::std::uint_least32_t v{static_cast<::std::uint_least32_t>(a / divisor)};
				::std::uint_least64_t m{static_cast<::std::uint_least64_t>(a % divisor)};
				jeaiii_c<0>(iter, v);
				++iter;
				alow = m;
				jeaiii_main_len<false, true>(iter, static_cast<::std::uint_least64_t>(alow), full_length);
				iter += full_length;
			}
			else
			{
				::std::uint_least32_t len_sub{len - full_length};
				jeaiii_main_len(iter, static_cast<::std::uint_least64_t>(alow), len_sub);
				iter += len_sub;
			}
		}
		jeaiii_main_len<false, true>(iter, static_cast<::std::uint_least64_t>(u), full_length);
	}
	else if constexpr (sizeof(U) == sizeof(::std::uint_least64_t))
	{
		constexpr ::std::uint_least32_t full_length{9u};
		constexpr ::std::uint_least32_t divisor{1000000000u};
		if constexpr (recursive)
		{
			constexpr ::std::uint_least64_t divisor18{static_cast<::std::uint_least64_t>(divisor) * divisor};
			::std::uint_least64_t high{n / divisor};
			::std::uint_least32_t low{static_cast<::std::uint_least32_t>(n % divisor)};
			::std::uint_least32_t high_first{static_cast<::std::uint_least32_t>(n / divisor18)};
			::std::uint_least32_t high_low{static_cast<::std::uint_least32_t>(
				high - static_cast<::std::uint_least64_t>(high_first) * divisor)};
			jeaiii_c<0>(iter, high_first);
			++iter;
			iter = jeaiii_f<8>(jeaiii_f<8>(iter, high_low), low);
		}
		else
		{
			if (len <= 9u)
			{
				return jeaiii_hash<9>(iter, static_cast<::std::uint_least32_t>(n), len);
			}
			::std::uint_least64_t a{n / divisor};
			::std::uint_least32_t u{static_cast<::std::uint_least32_t>(n % divisor)};
			::std::uint_least32_t alow{static_cast<::std::uint_least32_t>(a)};
			if constexpr (ryu_mode)
			{
				::std::uint_least32_t len_sub{len - full_length};
				jeaiii_hash<7>(iter, alow, len_sub);
				iter += len_sub;
			}
			else
			{
				if (a != static_cast<::std::uint_least64_t>(alow))
				{
					constexpr ::std::uint_least64_t divisor18{static_cast<::std::uint_least64_t>(divisor) * divisor};
					::std::uint_least32_t v{static_cast<::std::uint_least32_t>(n / divisor18)};
					alow = static_cast<::std::uint_least32_t>(
						a - static_cast<::std::uint_least64_t>(v) * divisor);
					if (v < 10u)
					{
						jeaiii_c<0>(iter, v);
						++iter;
					}
					else
					{
						jeaiii_w(iter, v);
						iter += 2;
					}
					jeaiii_f<8>(iter, alow);
					iter += full_length;
				}
				else
				{
					::std::uint_least32_t len_sub{len - full_length};
					jeaiii_hash<9>(iter, alow, len_sub);
					iter += len_sub;
				}
			}
			jeaiii_f<8>(iter, u);
		}
	}
	else
	{
		static_assert(!recursive);
		if constexpr (ryu_mode)
		{
			jeaiii_hash<8>(iter, n, len);
		}
		else
		{
			jeaiii_hash<9>(iter, n, len);
		}
	}
}

} // namespace fast_io::details::jeaiii
