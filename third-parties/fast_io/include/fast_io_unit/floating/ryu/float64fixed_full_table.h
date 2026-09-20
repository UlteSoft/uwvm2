#pragma once

namespace fast_io::details::ryu
{

namespace fixed_precision_table_generation
{

inline constexpr ::std::size_t binary_limb_bits{32u};
inline constexpr ::std::size_t binary_limb_capacity{128u};
inline constexpr ::std::size_t binary_limb_guard{4u};
inline constexpr ::std::uint_least64_t binary_limb_mask{0xffffffffu};
inline constexpr ::std::uint_least64_t decimal_block_base{1000000000u};
inline constexpr ::std::uint_least64_t residue_high_limit{256000000000u};

// This compile-time integer is deliberately based on 32-bit limbs and 64-bit
// intermediates.  Therefore table construction neither depends on a native
// 128-bit integer type nor changes with a compiler's extension policy.  For a
// limb x and carry c below 10^9,
//
//   x * 10^9 + c < 2^32 * 10^9 < 2^62,
//
// so every multiplication intermediate is representable by
// std::uint_least64_t.  The largest construction below uses fewer than 3,950
// bits; 128 limbs provide 4,096 bits and prove that no limb can be discarded.
struct binary_integer
{
	// Four leading zero limbs make the lowest signed source window used by the
	// second table directly addressable.  They are compile-time workspace only.
	::std::uint_least64_t limbs[binary_limb_capacity + binary_limb_guard]{};
	::std::size_t used{};

	constexpr void multiply_by_decimal_block() noexcept
	{
		::std::uint_least64_t carry{};
		for (::std::size_t i{}; i != used; ++i)
		{
			auto const product{limbs[binary_limb_guard + i] * decimal_block_base + carry};
			limbs[binary_limb_guard + i] = product & binary_limb_mask;
			carry = product >> binary_limb_bits;
		}
		if (carry != 0u)
		{
			limbs[binary_limb_guard + used++] = carry;
		}
	}
};

struct residue
{
	::std::uint_least64_t limbs[3]{};
};

// The first table stores (q + 1) modulo 10^9 * 2^136.  Incrementing the
// already reduced residue is equivalent because reduction is a ring
// homomorphism.  The only wrap point is the modulus itself, represented by
// {0, 0, 256000000000} in the three-limb layout.
constexpr void increment(residue &value) noexcept
{
	value.limbs[0] = (value.limbs[0] + 1u) & 0xffffffffffffffffu;
	if (value.limbs[0] == 0u)
	{
		value.limbs[1] = (value.limbs[1] + 1u) & 0xffffffffffffffffu;
		if (value.limbs[1] == 0u)
		{
			++value.limbs[2];
		}
	}
	if (value.limbs[0] == 0u && value.limbs[1] == 0u &&
		value.limbs[2] == residue_high_limit)
	{
		value.limbs[2] = 0u;
	}
}

template <::std::size_t size>
struct triple_table
{
	using row_type = ::std::uint_least64_t[3];

	::std::uint_least64_t elements[size][3]{};

	constexpr auto operator[](::std::size_t index) const noexcept
		-> ::std::uint_least64_t const (&)[3]
	{
		return elements[index];
	}

	constexpr operator row_type const *() const noexcept
	{
		return elements;
	}
};

template <typename value_type, ::std::size_t size>
struct scalar_table
{
	value_type elements[size]{};

	constexpr value_type const &operator[](::std::size_t index) const noexcept
	{
		return elements[index];
	}

	constexpr operator value_type const *() const noexcept
	{
		return elements;
	}
};

inline constexpr ::std::size_t first_table_group_count{64u};
inline constexpr ::std::size_t first_table_row_count{1224u};
inline constexpr ::std::size_t decimal_power_limb_capacity{40u};
// GCC on x86-64 gives the former large raw arrays 32-byte COMDAT alignment,
// while Clang gives them 16-byte alignment.  The generated facade has a
// smaller natural alignment, so retain the established object layout
// explicitly.  This condition affects data ABI only; table values and lookup
// instructions are identical on both branches.
// The GCC-x86 branch is an open compiler-family layout hypothesis.  The 16-byte
// fallback preserves the measured Clang x86-64 layout and deliberately gives
// every unmeasured configuration the same conservative explicit alignment; it
// is not evidence that each target's former raw arrays were exactly 16-byte
// aligned.  Changing either value requires checking symbol/section alignment,
// COMDAT/linkonce coalescing, relocations and addends, table-load instructions,
// neighboring constant placement, and linked data size on that ABI.
#if defined(__GNUC__) && !defined(__clang__) && \
	(defined(__x86_64__) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
inline constexpr ::std::size_t public_table_alignment{32u};
#else
inline constexpr ::std::size_t public_table_alignment{16u};
#endif

// The fixed-point constant 1292913986/2^32 under-approximates log10(2) by less
// than 1.15e-10.  Across 0 <= k < 64 the scaled error is below 1.16e-7, while
// the nearest positive fractional part of 16*k*log10(2) is above 0.0075.
// Therefore the shift computes floor(16*k*log10(2)) exactly on the complete
// domain.  The resulting block-count formula is bounded by 36.
constexpr ::std::size_t first_table_group_size(::std::size_t index) noexcept
{
	// Keep the fixed-point product at least 64 bits wide. On 32-bit targets size_t is commonly unsigned int, so allowing
	// the usual arithmetic conversions to select that type would both wrap the product and make the 32-bit shift invalid.
	auto const scaled{
		(static_cast<::std::uint_least64_t>(16u) *
		 static_cast<::std::uint_least64_t>(index) *
		 static_cast<::std::uint_least64_t>(1292913986u)) >>
		32u};
	return static_cast<::std::size_t>((scaled + 25u) / 9u);
}

constexpr ::std::size_t first_table_generated_size() noexcept
{
	::std::size_t result{};
	for (::std::size_t i{}; i != first_table_group_count; ++i)
	{
		result += first_table_group_size(i);
	}
	return result;
}

// The first numerator is more economical in radix 10^9.  Dividing
// 2^(16*k+120) by 10^(9*i) then becomes the exact removal of i low limbs.
// Adjacent groups differ by only 2^16, so all 64 numerators require one small
// multiplication each rather than 1,224 multi-limb divisions.
struct decimal_power
{
	::std::uint_least64_t limbs[decimal_power_limb_capacity]{};
	::std::size_t used{1u};

	constexpr void multiply(::std::uint_least64_t multiplier) noexcept
	{
		::std::uint_least64_t carry{};
		for (::std::size_t i{}; i != used; ++i)
		{
			auto const product{limbs[i] * multiplier + carry};
			limbs[i] = product % decimal_block_base;
			carry = product / decimal_block_base;
		}
		if (carry != 0u)
		{
			limbs[used++] = carry;
		}
	}
};

// Evaluate (value*10^9+digit) modulo 10^9*2^136.  Write the low 136 bits of
// value as five 32-bit limbs, the fifth containing only eight bits.  The first
// four products are below 2^62.  After the fifth product, its low eight bits
// are the new low part and product>>8 is the carry across bit 136.  The old
// high coefficient vanishes because its product by 10^9 is a multiple of the
// modulus.  Since the low part is below 2^136, the carry is strictly below
// 10^9 and needs no further reduction.
constexpr residue append_decimal_limb(residue value,
									  ::std::uint_least64_t digit) noexcept
{
	auto product{(value.limbs[0] & binary_limb_mask) * decimal_block_base + digit};
	auto limb0{product & binary_limb_mask};
	auto carry{product >> binary_limb_bits};
	product = (value.limbs[0] >> binary_limb_bits) * decimal_block_base + carry;
	auto const limb1{product & binary_limb_mask};
	carry = product >> binary_limb_bits;
	product = (value.limbs[1] & binary_limb_mask) * decimal_block_base + carry;
	auto limb2{product & binary_limb_mask};
	carry = product >> binary_limb_bits;
	product = (value.limbs[1] >> binary_limb_bits) * decimal_block_base + carry;
	auto const limb3{product & binary_limb_mask};
	carry = product >> binary_limb_bits;
	product = (value.limbs[2] & 0xffu) * decimal_block_base + carry;
	limb0 |= limb1 << binary_limb_bits;
	limb2 |= limb3 << binary_limb_bits;
	return {{limb0, limb2, (product & 0xffu) + ((product >> 8u) << 8u)}};
}

constexpr scalar_table<::std::uint_least16_t, first_table_group_count>
make_first_table_offsets() noexcept
{
	scalar_table<::std::uint_least16_t, first_table_group_count> result{};
	::std::size_t offset{};
	for (::std::size_t i{}; i != first_table_group_count; ++i)
	{
		result.elements[i] = static_cast<::std::uint_least16_t>(offset);
		offset += first_table_group_size(i);
	}
	return result;
}

constexpr triple_table<first_table_row_count> make_first_table() noexcept
{
	triple_table<first_table_row_count> result{};
	auto const offsets{make_first_table_offsets()};
	decimal_power power{};
	power.limbs[0] = 1u;
	power.multiply(256u);
	for (::std::size_t i{}; i != 7u; ++i)
	{
		power.multiply(65536u);
	}
	for (::std::size_t index{}; index != first_table_group_count; ++index)
	{
		auto const count{first_table_group_size(index)};
		residue suffix{};
		for (auto i{power.used}; i != 0u; --i)
		{
			auto const row_index{i - 1u};
			suffix = append_decimal_limb(suffix, power.limbs[row_index]);
			if (row_index < count)
			{
				auto row{suffix};
				increment(row);
				auto const output{
					static_cast<::std::size_t>(offsets.elements[index]) + row_index};
				result.elements[output][0] = row.limbs[0];
				result.elements[output][1] = row.limbs[1];
				result.elements[output][2] = row.limbs[2];
			}
		}
		power.multiply(65536u);
	}
	return result;
}

static_assert(first_table_generated_size() == first_table_row_count);
static_assert(sizeof(triple_table<first_table_row_count>) ==
			  first_table_row_count * 3u * sizeof(::std::uint_least64_t));
static_assert(sizeof(scalar_table<::std::uint_least16_t, first_table_group_count>) ==
			  first_table_group_count * sizeof(::std::uint_least16_t));

} // namespace fixed_precision_table_generation

inline constexpr ::std::size_t table_size{64u};

alignas(fixed_precision_table_generation::public_table_alignment) inline constexpr auto power_offset{
	fixed_precision_table_generation::make_first_table_offsets()};

alignas(fixed_precision_table_generation::public_table_alignment) inline constexpr auto pow10_split{
	fixed_precision_table_generation::make_first_table()};

} // namespace fast_io::details::ryu
