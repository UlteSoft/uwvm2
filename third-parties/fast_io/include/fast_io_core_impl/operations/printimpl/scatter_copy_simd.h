#pragma once

/*
 * Target-aware implementation leaf for proved IO scatter copies.
 *
 * This header supplies the SIMD definition declared by `scatter_copy.h` after
 * the core vector facilities are available. It changes only the copy mechanism
 * for an already-approved put-area strategy. Protocol selection, range proofs,
 * destination capacity, and device transfer remain responsibilities of the
 * surrounding IO operation.
 */

namespace fast_io::details::decay
{

/// @brief Implements one target-supported SIMD tier of a proved put-area scatter copy.
/// @details A payload between one and two vectors is copied with head and tail vectors.  Otherwise the next narrower
///          target tier is tried.  This makes the source independent of a fixed x86 width while leaving the backend
///          free to split a nominal vector when that is cheaper for the selected microarchitecture.
template <::std::size_t vector_bytes, ::std::integral value_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline value_type *put_area_scatter_copy_simd_impl(
	value_type const *first, ::std::size_t count, value_type *result) noexcept
{
	if constexpr (vector_bytes == 0u)
	{
		return ::fast_io::details::non_overlapped_copy_n(first, count, result);
	}
	else
	{
		if constexpr (vector_bytes % sizeof(value_type) == 0u)
		{
			constexpr ::std::size_t lanes{vector_bytes / sizeof(value_type)};
			if (count >= lanes && count <= lanes * 2u)
			{
				if constexpr (!::fast_io::details::can_simd_vector_run_with_cpu_instruction<vector_bytes>)
				{
					::std::byte head[vector_bytes];
					::fast_io::freestanding::my_memcpy(head, first, vector_bytes);
					::std::byte tail[vector_bytes];
					::fast_io::freestanding::my_memcpy(
						tail, first + count - lanes, vector_bytes);
					::fast_io::freestanding::my_memcpy(result, head, vector_bytes);
					::fast_io::freestanding::my_memcpy(
						result + count - lanes, tail, vector_bytes);
				}
				else
				{
					// Move object representations as bytes.  Besides avoiding needless vector element-type variants, this keeps the
					// integral API valid for bool, which GCC and Clang reject as a native vector element type.
					using vector_type = ::fast_io::intrinsics::simd_vector<unsigned char, vector_bytes>;
					vector_type head;
					head.load(first);
					vector_type tail;
					tail.load(first + count - lanes);
					head.store(result);
					tail.store(result + count - lanes);
				}
				return result + count;
			}
		}
		return ::fast_io::details::decay::put_area_scatter_copy_simd_impl<vector_bytes / 2u>(
			first, count, result);
	}
}

/// @brief Copies a run-time scatter into a preflighted stable put area with target-selected vector widths.
/// @details Constant evaluation uses an ordinary element loop.  At run time, payloads up to twice the widest native
///          vector use a descending head/tail SIMD strategy; larger payloads retain the general memcpy-shaped path.
///          Consequently x86-64-v2, v3, and v4 builds can lower to SSE, AVX, and AVX-512 respectively instead of being
///          capped by a hard-coded 16-byte implementation.  The caller proves non-overlap and both complete extents.
template <::std::integral value_type>
inline constexpr value_type *put_area_scatter_copy_n(
	value_type const *first, ::std::size_t count, value_type *result) noexcept
{
	if (count == 0u)
	{
		return result;
	}
	if (__builtin_is_constant_evaluated())
	{
		for (::std::size_t index{}; index != count; ++index)
		{
			result[index] = first[index];
		}
		return result + count;
	}
	else
	{
		constexpr ::std::size_t native_vector_bytes{
			::fast_io::details::optimal_simd_vector_run_with_cpu_instruction_size};
		constexpr ::std::size_t initial_block_bytes{
			native_vector_bytes == 0u ? 8u : native_vector_bytes};
		if constexpr (initial_block_bytes % sizeof(value_type) != 0u)
		{
			return ::fast_io::details::non_overlapped_copy_n(first, count, result);
		}
		else
		{
			constexpr ::std::size_t lanes{initial_block_bytes / sizeof(value_type)};
			if (count > lanes * 2u)
			{
				return ::fast_io::details::non_overlapped_copy_n(first, count, result);
			}
			return ::fast_io::details::decay::put_area_scatter_copy_simd_impl<initial_block_bytes>(
				first, count, result);
		}
	}
}

} // namespace fast_io::details::decay
