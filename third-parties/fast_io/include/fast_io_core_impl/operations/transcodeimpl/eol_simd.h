#pragma once

/**
 * @file
 * @brief Retains experimental SIMD kernels beside the EOL engine.
 *
 * These helpers operate on bounded source/destination ranges but are not part
 * of the active adapter aggregation yet. Keeping them in transcodeimpl avoids a
 * second transcoder ownership location while their protocol integration is
 * completed.
 */

namespace fast_io
{

namespace details
{

/** @brief SIMD-accelerates bounded one-unit to two-unit newline expansion. */
template <bool cr = false, ::std::integral char_type>
inline constexpr deco_result<char_type, char_type>
simd_lf_crlf_process_chars(char_type const *fromfirst, char_type const *fromlast, char_type *tofirst,
						   char_type *tolast) noexcept
{
	constexpr ::std::size_t initialdiffn{::fast_io::details::optimal_simd_vector_run_with_cpu_instruction_size};
	constexpr unsigned N{initialdiffn / sizeof(char_type)};
	using simd_vector_type = ::fast_io::intrinsics::simd_vector<char_type, N>;
	constexpr char_type lfchct{char_literal_v<u8'\n', ::std::remove_cvref_t<char_type>>};
	constexpr char_type lfchr{char_literal_v<u8'\r', ::std::remove_cvref_t<char_type>>};

	constexpr char_type tofdch{cr ? lfchr : lfchct};
#if (__cpp_lib_bit_cast >= 201806L) && !defined(__clang__)
	// Constant-evaluate the search vector when the compiler supports this path.
	constexpr simd_vector_type charsvec{::std::bit_cast<simd_vector_type>(
		create_find_simd_vector_with_unsigned_toggle<false, char_type, N>(tofdch))};
#else
	// Load the constant search pattern at runtime on the compatibility path.
	simd_vector_type charsvec;
	constexpr auto chars_array{
		create_find_simd_vector_with_unsigned_toggle<false, char_type, N>(tofdch)};
	charsvec.load(chars_array.data());
#endif
	simd_vector_type vec;
	for (; fromfirst != fromlast && tofirst != tolast; ++fromfirst)
	{
		// Alternate scalar conversion with SIMD copying of nonmatching runs.
		if (*fromfirst != tofdch)
		{
			// Copy a scalar prefix before attempting full SIMD blocks.
			*tofirst = *fromfirst;
			++fromfirst;
			++tofirst;
			::std::size_t fromdiff{static_cast<::std::size_t>(fromlast - fromfirst)};
			::std::size_t todiff{static_cast<::std::size_t>(tolast - tofirst)};
			if (todiff < fromdiff)
			{
				// Bound the vector-copy candidate by destination capacity.
				fromdiff = todiff;
			}
			fromdiff /= N;
			for (; fromdiff; --fromdiff)
			{
				// Copy full vectors until one contains the searched newline unit.
				vec.load(fromfirst);
				// Equality is zero in every lane exactly when this complete block contains no delimiter.
				auto matches{vec == charsvec};
				if (!::fast_io::intrinsics::is_all_zeros(matches))
				{
					// Stop vector copying at a block containing a convertible unit.
					break;
				}
				vec.store(tofirst);
				fromfirst += N;
				tofirst += N;
			}
			if (!fromdiff)
			{
				// No complete vector block remains for the accelerated scan.
				break;
			}
			::std::size_t fromdiff2{static_cast<::std::size_t>(fromlast - fromfirst)};
			::std::size_t todiff2{static_cast<::std::size_t>(tolast - tofirst)};
			if (todiff2 < fromdiff2)
			{
				// Bound the scalar tail scan by destination capacity.
				fromdiff2 = todiff2;
			}
			for (; fromdiff2; --fromdiff2)
			{
				// Locate the exact matching lane in the first mixed vector block.
				auto ch{*fromfirst};
				if (ch == tofdch)
				{
					// Leave the matched unit for the expansion branch below.
					break;
				}
				*tofirst = ch;
				++fromfirst;
				++tofirst;
			}
			if (!fromdiff2)
			{
				// The bounded scalar tail contained no convertible unit.
				break;
			}
		}
		if (tofirst + 1 == tolast)
		{
			// Expansion requires two live destination slots. Preserve the matching source unit for the adapter's
			// resumable scalar path when this bounded kernel observes only the final slot.
			break;
		}
		if constexpr (cr)
		{
			// Expand CR to LFCR when the reverse-order mode is selected.
			*tofirst = lfchct;
			++tofirst;
			*tofirst = lfchr;
			++tofirst;
		}
		else
		{
			// Expand LF to the conventional CRLF sequence.
			*tofirst = lfchr;
			++tofirst;
			*tofirst = lfchct;
			++tofirst;
		}
	}
	return {fromfirst, tofirst};
}

/** @brief SIMD-accelerates bounded two-unit to one-unit newline contraction. */
template <bool cr = false, ::std::integral char_type>
inline constexpr deco_result<char_type, char_type>
simd_crlf_lf_process_chars(char_type const *fromfirst, char_type const *fromlast, char_type *tofirst,
						   char_type *tolast) noexcept
{
	constexpr ::std::size_t initialdiffn{::fast_io::details::optimal_simd_vector_run_with_cpu_instruction_size};
	constexpr unsigned N{initialdiffn / sizeof(char_type)};
	constexpr unsigned Np1{N + 1u};
	using simd_vector_type = ::fast_io::intrinsics::simd_vector<char_type, N>;
	constexpr char_type lfchct{char_literal_v<u8'\n', ::std::remove_cvref_t<char_type>>};
	constexpr char_type lfchr{char_literal_v<u8'\r', ::std::remove_cvref_t<char_type>>};
#if (__cpp_lib_bit_cast >= 201806L) && !defined(__clang__)
	// Constant-evaluate the CR search vector on the supported compiler path.
	constexpr simd_vector_type charsvec{::std::bit_cast<simd_vector_type>(
		create_find_simd_vector_with_unsigned_toggle<false, char_type, N>(lfchr))};
#else
	// Load the CR search vector at runtime on the compatibility path.
	simd_vector_type charsvec;
	constexpr auto chars_array{
		create_find_simd_vector_with_unsigned_toggle<false, char_type, N>(lfchr)};
	charsvec.load(chars_array.data());
#endif
	for (simd_vector_type vec;;)
	{
		// Copy full vectors and resolve each discovered CR with one-unit lookahead.
		::std::size_t fromdiff{static_cast<::std::size_t>(fromlast - fromfirst)};
		::std::size_t todiff{static_cast<::std::size_t>(tolast - tofirst)};
		if (fromdiff < Np1 || todiff < N)
		{
			// Retain one source lookahead unit and a complete vector destination.
			break;
		}
		vec.load(fromfirst);
		// Retain the direct CR predicate for both the all-plain proof and the first matching lane calculation.
		auto matches{vec == charsvec};
		if (::fast_io::intrinsics::is_all_zeros(matches))
		{
			// A vector with no CR can be copied without scalar pair inspection.
			vec.store(tofirst);
			fromfirst += N;
			tofirst += N;
			continue;
		}
		// The mask utility normalizes byte masks to element lanes and preserves source address order across endianness.
		// Since this block contains a CR, the result is strictly below N and element N remains valid lookahead.
		unsigned pos{::fast_io::intrinsics::vector_mask_countr_zero(matches)};
		FAST_IO_ASSUME(pos < N);
		// Commit only through the matched CR. Storing the complete vector here would mutate bytes beyond `to_next`,
		// violating the bounded process contract even though those bytes remain inside the physical allocation.
		::fast_io::details::non_overlapped_copy_n(fromfirst, static_cast<::std::size_t>(pos) + 1u, tofirst);
		fromfirst += pos + 1;
		tofirst += pos + 1;
		if (*fromfirst != lfchct)
		{
			// An unmatched CR remains literal; continue after the copied prefix.
			continue;
		}
		++fromfirst;
		if constexpr (cr)
		{
			// Contract the pair to CR for the requested destination convention.
			tofirst[-1] = lfchr;
		}
		else
		{
			// Contract the pair to LF for the conventional destination.
			tofirst[-1] = lfchct;
		}
	}
	return {fromfirst, tofirst};
}

/** @brief SIMD-accelerates bounded single-unit LF/CR substitution. */
template <bool cr, ::std::integral char_type>
inline constexpr deco_result<char_type, char_type>
simd_lf_cr_process_chars(char_type const *fromfirst, char_type const *fromlast, char_type *tofirst,
						 char_type *tolast) noexcept
{
	constexpr ::std::size_t initialdiffn{::fast_io::details::optimal_simd_vector_run_with_cpu_instruction_size};
	constexpr unsigned N{initialdiffn / sizeof(char_type)};
	using simd_vector_type = ::fast_io::intrinsics::simd_vector<char_type, N>;
	constexpr char_type lfchct{char_literal_v<u8'\n', ::std::remove_cvref_t<char_type>>};
	constexpr char_type lfchr{char_literal_v<u8'\r', ::std::remove_cvref_t<char_type>>};
	constexpr char_type tofdch{cr ? lfchr : lfchct};
#if (__cpp_lib_bit_cast >= 201806L) && !defined(__clang__)
	// Constant-evaluate both search and unit-delta vectors where supported.
	constexpr simd_vector_type charsvec{::std::bit_cast<simd_vector_type>(
		create_find_simd_vector_with_unsigned_toggle<false, char_type, N>(tofdch))};
	constexpr simd_vector_type threevec{::std::bit_cast<simd_vector_type>(characters_array_impl<3, char_type, N>)};
#else
	// Load search and unit-delta vectors on the compatibility path.
	simd_vector_type charsvec;
	constexpr auto chars_array{
		create_find_simd_vector_with_unsigned_toggle<false, char_type, N>(tofdch)};
	charsvec.load(chars_array.data());
	simd_vector_type threevec;
	threevec.load(characters_array_impl<3, char_type, N>.data());
#endif

	for (simd_vector_type vec;;)
	{
		// Apply the unit delta to every complete vector fitting both ranges.
		::std::size_t fromdiff{static_cast<::std::size_t>(fromlast - fromfirst)};
		::std::size_t todiff{static_cast<::std::size_t>(tolast - tofirst)};
		if (fromdiff < N || todiff < N)
		{
			// Leave the non-vector tail to the caller's scalar kernel.
			break;
		}
		vec.load(fromfirst);
		auto comres{vec == charsvec};
		auto comresandres{comres & threevec};
		if constexpr (cr)
		{
			// Subtract the encoded delta to map CR to LF lanes.
			vec -= comresandres;
		}
		else
		{
			// Add the encoded delta to map LF to CR lanes.
			vec += comresandres;
		}
		vec.store(tofirst);
		fromfirst += N;
		tofirst += N;
	}
	return {fromfirst, tofirst};
}

} // namespace details

} // namespace fast_io
