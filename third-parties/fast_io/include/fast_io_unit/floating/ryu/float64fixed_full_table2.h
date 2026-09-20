#pragma once

#include "float64fixed_full_table.h"

namespace fast_io::details::ryu
{

namespace fixed_precision_table_generation
{

inline constexpr ::std::size_t second_table_group_count{68u};
inline constexpr ::std::size_t second_table_row_count{3133u};
inline constexpr ::std::size_t second_table_last_block{121u};
inline constexpr ::std::size_t retained_half_limb_suffix_count{
	second_table_group_count + 1u};

constexpr ::std::size_t bit_length(binary_integer const &value) noexcept
{
	if (value.used == 0u)
	{
		return 0u;
	}
	auto top{value.limbs[binary_limb_guard + value.used - 1u]};
	::std::size_t top_bits{};
	while (top != 0u)
	{
		++top_bits;
		top >>= 1u;
	}
	return (value.used - 1u) * binary_limb_bits + top_bits;
}

// Group k uses shift s = 120 - 16*k.  Its first omitted block is the first i
// for which 10^(9*(i+1))*2^s is divisible by
//
//   10^9 * 2^136 = 2^145 * 5^9.
//
// The factor 5^9 is already present for every i >= 0, while the 2-adic
// valuation is 9*(i+1)+s.  Therefore the exclusive stop index is exactly
// ceil((145-s)/9)-1.  This closed form removes a generation prepass and gives
// stop(67)=121, the global upper bound used below.
constexpr ::std::size_t second_table_stop(::std::size_t index) noexcept
{
	return (33u + 16u * index) / 9u - 1u;
}

constexpr scalar_table<::std::uint_least8_t, second_table_group_count>
make_second_table_stops() noexcept
{
	scalar_table<::std::uint_least8_t, second_table_group_count> result{};
	for (::std::size_t index{}; index != second_table_group_count; ++index)
	{
		result.elements[index] =
			static_cast<::std::uint_least8_t>(second_table_stop(index));
	}
	return result;
}

// Before the first retained block the value is below 2^66 and, hence, far
// below the 166-bit modulus, so modular reduction cannot change the threshold
// comparison.  For threshold t = 66-s, 10^(9*(i+1)) >= 2^t iff its bit length
// exceeds t.  Generate the 35 relevant bit lengths once, then select every
// minimum without embedding another lookup table.
constexpr scalar_table<::std::uint_least8_t, second_table_group_count + 1u>
make_second_table_minimum_blocks() noexcept
{
	::std::size_t power_bit_lengths[35]{};
	binary_integer power{};
	power.limbs[binary_limb_guard] = 1u;
	power.used = 1u;
	for (::std::size_t block{}; block != 35u; ++block)
	{
		power.multiply_by_decimal_block();
		power_bit_lengths[block] = bit_length(power);
	}
	scalar_table<::std::uint_least8_t, second_table_group_count + 1u> result{};
	for (::std::size_t index{}; index != second_table_group_count; ++index)
	{
		auto const threshold{static_cast<int>(16u * index) - 54};
		if (threshold <= 0)
		{
			continue;
		}
		for (::std::size_t block{}; block != 35u; ++block)
		{
			if (power_bit_lengths[block] > static_cast<::std::size_t>(threshold))
			{
				result.elements[index] = static_cast<::std::uint_least8_t>(block);
				break;
			}
		}
	}
	return result;
}

constexpr scalar_table<::std::uint_least16_t, second_table_group_count + 1u>
make_second_table_offsets() noexcept
{
	auto const minimum_blocks{make_second_table_minimum_blocks()};
	scalar_table<::std::uint_least16_t, second_table_group_count + 1u> result{};
	::std::size_t offset{};
	for (::std::size_t index{}; index != second_table_group_count; ++index)
	{
		result.elements[index] = static_cast<::std::uint_least16_t>(offset);
		offset += second_table_stop(index) - minimum_blocks.elements[index];
	}
	result.elements[second_table_group_count] =
		static_cast<::std::uint_least16_t>(offset);
	return result;
}

struct half_limb_suffixes
{
	::std::uint_least64_t modulo_decimal_block[retained_half_limb_suffix_count]{};
};

// All high quotients needed for one decimal power end at 16-bit boundaries:
// for group k, (value*2^(120-16*k)) >> 136 = value >> (16*(k+1)).
// Let H_m=value>>16m.  Since H_m=chunk_m+2^16*H_(m+1), a single reverse
// Horner scan computes H_m mod 10^9 for every m.  Its intermediate is below
// 10^9*2^16 < 2^46, so uint_least64_t is sufficient.  This shares one scan
// among all 68 groups instead of rescanning an ever-growing integer 68 times.
constexpr half_limb_suffixes make_half_limb_suffixes(binary_integer const &value) noexcept
{
	half_limb_suffixes result{};
	::std::uint_least64_t remainder{};
	for (auto i{value.used}; i != 0u; --i)
	{
		auto const limb{value.limbs[binary_limb_guard + i - 1u]};
		remainder = (remainder * 65536u + (limb >> 16u)) % decimal_block_base;
		auto const high_index{2u * i - 1u};
		if (high_index < retained_half_limb_suffix_count)
		{
			result.modulo_decimal_block[high_index] = remainder;
		}
		remainder = (remainder * 65536u + (limb & 0xffffu)) % decimal_block_base;
		auto const low_index{2u * i - 2u};
		if (low_index < retained_half_limb_suffix_count)
		{
			result.modulo_decimal_block[low_index] = remainder;
		}
	}
	return result;
}

constexpr residue reduce_with_known_high_remainder(binary_integer const &value,
												   ::std::size_t index, ::std::uint_least64_t high_remainder) noexcept
{
	// The low 136-bit source window starts at 16*k-120.  Its 32-bit word
	// number is floor((16*k-120)/32)=floor(k/2)-4, and its intra-word offset
	// is eight bits for even k or 24 bits for odd k.  Computing these once per
	// row replaces five general signed-shift decompositions.
	auto const source_limb{static_cast<int>(index / 2u) - 4};
	auto const bit_offset{index % 2u == 0u ? 8u : 24u};
	auto const base{static_cast<::std::size_t>(
		static_cast<int>(binary_limb_guard) + source_limb)};
	auto const inverse_offset{binary_limb_bits - bit_offset};
	auto const limb0{(value.limbs[base] >> bit_offset) |
					 ((value.limbs[base + 1u] << inverse_offset) & binary_limb_mask)};
	auto const limb1{(value.limbs[base + 1u] >> bit_offset) |
					 ((value.limbs[base + 2u] << inverse_offset) & binary_limb_mask)};
	auto const limb2{(value.limbs[base + 2u] >> bit_offset) |
					 ((value.limbs[base + 3u] << inverse_offset) & binary_limb_mask)};
	auto const limb3{(value.limbs[base + 3u] >> bit_offset) |
					 ((value.limbs[base + 4u] << inverse_offset) & binary_limb_mask)};
	auto const limb4{(value.limbs[base + 4u] >> bit_offset) |
					 ((value.limbs[base + 5u] << inverse_offset) & binary_limb_mask)};
	residue result{};
	result.limbs[0] = limb0 | (limb1 << binary_limb_bits);
	result.limbs[1] = limb2 | (limb3 << binary_limb_bits);
	result.limbs[2] = (limb4 & 0xffu) + (high_remainder << 8u);
	return result;
}

constexpr triple_table<second_table_row_count> make_second_table() noexcept
{
	auto const minimum_blocks{make_second_table_minimum_blocks()};
	auto const offsets{make_second_table_offsets()};
	auto const stops{make_second_table_stops()};
	triple_table<second_table_row_count> result{};
	binary_integer power{};
	power.limbs[binary_limb_guard] = 1u;
	power.used = 1u;
	for (::std::size_t block{}; block != second_table_last_block; ++block)
	{
		power.multiply_by_decimal_block();
		auto const suffixes{make_half_limb_suffixes(power)};
		for (::std::size_t index{}; index != second_table_group_count; ++index)
		{
			if (block < minimum_blocks.elements[index] ||
				block >= stops.elements[index])
			{
				continue;
			}
			auto const row{reduce_with_known_high_remainder(power, index,
															suffixes.modulo_decimal_block[index + 1u])};
			auto const output{static_cast<::std::size_t>(offsets.elements[index]) +
							  block - minimum_blocks.elements[index]};
			result.elements[output][0] = row.limbs[0];
			result.elements[output][1] = row.limbs[1];
			result.elements[output][2] = row.limbs[2];
		}
	}
	return result;
}

static_assert(make_second_table_offsets()[second_table_group_count] ==
			  second_table_row_count);
static_assert(sizeof(triple_table<second_table_row_count>) ==
			  second_table_row_count * 3u * sizeof(::std::uint_least64_t));
static_assert(sizeof(scalar_table<::std::uint_least16_t,
								  second_table_group_count + 1u>) ==
			  (second_table_group_count + 1u) * sizeof(::std::uint_least16_t));
static_assert(sizeof(scalar_table<::std::uint_least8_t,
								  second_table_group_count + 1u>) ==
			  (second_table_group_count + 1u) * sizeof(::std::uint_least8_t));

} // namespace fixed_precision_table_generation

inline constexpr ::std::size_t table_size_2{69u};
inline constexpr ::std::size_t addtional_bits_2{120u};

alignas(fixed_precision_table_generation::public_table_alignment) inline constexpr auto pow10_offset_2{
	fixed_precision_table_generation::make_second_table_offsets()};
alignas(fixed_precision_table_generation::public_table_alignment) inline constexpr auto min_block_2{
	fixed_precision_table_generation::make_second_table_minimum_blocks()};
alignas(fixed_precision_table_generation::public_table_alignment) inline constexpr auto pow10_split_2{
	fixed_precision_table_generation::make_second_table()};

} // namespace fast_io::details::ryu
