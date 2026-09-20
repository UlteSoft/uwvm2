#pragma once

/**
 * @file
 * @brief Validates cursors returned across transcoder customization boundaries.
 */

namespace fast_io::details
{

/** @brief Carries one validated closed range and offsets measured from its base. */
template <typename T>
struct basic_transcode_closed_range_offsets
{
	T *begin{};
	T *current{};
	T *end{};
	::std::size_t current_offset{};
	::std::size_t end_offset{};
	bool valid{};
};

/** @brief Materializes a runtime address without retaining optimizer provenance. */
template <typename T>
[[nodiscard]] inline ::std::uintptr_t
transcode_runtime_pointer_address(T *pointer) noexcept
{
	auto address{reinterpret_cast<::std::uintptr_t>(pointer)};
#if defined(__GNUC__) || defined(__clang__)
	// The empty register barrier is code-generation free, but prevents an
	// optimizer from rewriting the following integer inequalities back into
	// unrelated-pointer comparisons. Pointer-pair sanitizers remain enabled for
	// every actual pointer relation outside this deliberately integer-only proof.
	__asm__ __volatile__("" : "+r"(address));
	return address;
#else
	// Standard volatile materialization provides the same semantic barrier on
	// implementations without GNU-style register constraints.
	::std::uintptr_t volatile materialized_address{address};
	return materialized_address;
#endif
}

/**
 * @brief Validates `[begin,end]` and `current`, then rebuilds both derived cursors.
 *
 * @details Formal contract: a successful result proves that the three pointers
 * describe the canonical all-null empty range or that their byte addresses are
 * aligned for `T` and satisfy `B <= C <= E`, with both `C-B` and `E-B`
 * divisible by `sizeof(T)`. Runtime validation performs those predicates only
 * on `uintptr_t`; it never orders or subtracts possibly unrelated pointers.
 * After proof, `current` and `end` are reconstructed exclusively as
 * `begin + current_offset` and `begin + end_offset`. During constant evaluation,
 * pointer subtraction deliberately retains the language's same-array rule: an
 * unrelated pointer makes the expression non-constant instead of being hidden
 * behind an implementation address representation.
 */
template <typename T>
[[nodiscard]] inline constexpr basic_transcode_closed_range_offsets<T>
validate_transcode_closed_range_offsets(T *begin, T *end,
										T *current) noexcept
{
	if (begin == nullptr)
	{
		// The only representable null range is the canonical all-null empty range.
		return {nullptr, nullptr, nullptr, 0u, 0u,
				end == nullptr && current == nullptr};
	}
	if (end == nullptr || current == nullptr)
	{
		// A non-null base cannot have a null bound or cursor.
		return {};
	}

	if (::std::is_constant_evaluated())
	{
		// These differences are intentionally evaluated before any ordering test.
		// The core-language constant evaluator therefore enforces same-array
		// provenance for both the claimed end and the returned cursor.
		auto const end_distance{end - begin};
		auto const current_distance{current - begin};
		if (end_distance < 0 || current_distance < 0 ||
			current_distance > end_distance)
		{
			// Same-array cursors outside the supplied closed range are invalid.
			return {};
		}
		auto const end_offset{static_cast<::std::size_t>(end_distance)};
		auto const current_offset{
			static_cast<::std::size_t>(current_distance)};
		return {begin, begin + current_offset, begin + end_offset,
				current_offset, end_offset, true};
	}

	auto const begin_address{
		::fast_io::details::transcode_runtime_pointer_address(begin)};
	auto const end_address{
		::fast_io::details::transcode_runtime_pointer_address(end)};
	auto const current_address{
		::fast_io::details::transcode_runtime_pointer_address(current)};
	if (begin_address % alignof(T) != 0u ||
		end_address % alignof(T) != 0u ||
		current_address % alignof(T) != 0u ||
		end_address < begin_address || current_address < begin_address ||
		current_address > end_address)
	{
		// Integer-address predicates reject malformed topology without invoking
		// the language's unrelated-pointer ordering rules.
		return {};
	}
	auto const end_bytes{end_address - begin_address};
	auto const current_bytes{current_address - begin_address};
	if (end_bytes % sizeof(T) != 0u || current_bytes % sizeof(T) != 0u)
	{
		// A numerically enclosed byte address must still denote a whole T unit.
		return {};
	}
	auto const end_units{end_bytes / sizeof(T)};
	auto const current_units{current_bytes / sizeof(T)};
	if (end_units > static_cast<::std::uintptr_t>(PTRDIFF_MAX))
	{
		// A range wider than ptrdiff_t cannot support the contiguous-range
		// difference contract advertised by buffered stream operations.
		return {};
	}
	auto const end_offset{static_cast<::std::size_t>(end_units)};
	auto const current_offset{static_cast<::std::size_t>(current_units)};
	return {begin, begin + current_offset, begin + end_offset,
			current_offset, end_offset, true};
}

} // namespace fast_io::details
