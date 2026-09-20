#pragma once

namespace fast_io
{

namespace details
{

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
	constexpr simd_vector_type charsvec{::std::bit_cast<simd_vector_type>(
		create_find_simd_vector_with_unsigned_toggle<false, char_type, N>(tofdch))};
#else
	simd_vector_type charsvec;
	constexpr auto chars_array{
		create_find_simd_vector_with_unsigned_toggle<false, char_type, N>(tofdch)};
	charsvec.load(chars_array.data());
#endif
	simd_vector_type vec;
	for (; fromfirst != fromlast && tofirst != tolast; ++fromfirst)
	{
		if (*fromfirst != tofdch)
		{
			*tofirst = *fromfirst;
			++fromfirst;
			++tofirst;
			::std::size_t fromdiff{static_cast<::std::size_t>(fromlast - fromfirst)};
			::std::size_t todiff{static_cast<::std::size_t>(tolast - tofirst)};
			if (todiff < fromdiff)
			{
				fromdiff = todiff;
			}
			fromdiff /= N;
			for (; fromdiff; --fromdiff)
			{
				vec.load(fromfirst);
				// Equality is the searched predicate: an all-zero mask proves that the entire block is ordinary text.
				auto matches{vec == charsvec};
				if (!::fast_io::intrinsics::is_all_zeros(matches))
				{
					break;
				}
				vec.store(tofirst);
				fromfirst += N;
				tofirst += N;
			}
			if (!fromdiff)
			{
				break;
			}
			::std::size_t fromdiff2{static_cast<::std::size_t>(fromlast - fromfirst)};
			::std::size_t todiff2{static_cast<::std::size_t>(tolast - tofirst)};
			if (todiff2 < fromdiff2)
			{
				fromdiff2 = todiff2;
			}
			for (; fromdiff2; --fromdiff2)
			{
				auto ch{*fromfirst};
				if (ch == tofdch)
				{
					break;
				}
				*tofirst = ch;
				++fromfirst;
				++tofirst;
			}
			if (!fromdiff2)
			{
				break;
			}
		}
		if (tofirst + 1 == tolast)
		{
			// A delimiter expands to two output code units. Leave it unconsumed when only one slot remains so the
			// scalar state machine can publish the prefix and resume the expansion without crossing `tolast`.
			break;
		}
		if constexpr (cr)
		{
			*tofirst = lfchct;
			++tofirst;
			*tofirst = lfchr;
			++tofirst;
		}
		else
		{
			*tofirst = lfchr;
			++tofirst;
			*tofirst = lfchct;
			++tofirst;
		}
	}
	return {fromfirst, tofirst};
}

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
	constexpr simd_vector_type charsvec{::std::bit_cast<simd_vector_type>(
		create_find_simd_vector_with_unsigned_toggle<false, char_type, N>(lfchr))};
#else
	simd_vector_type charsvec;
	constexpr auto chars_array{
		create_find_simd_vector_with_unsigned_toggle<false, char_type, N>(lfchr)};
	charsvec.load(chars_array.data());
#endif
	for (simd_vector_type vec;;)
	{
		::std::size_t fromdiff{static_cast<::std::size_t>(fromlast - fromfirst)};
		::std::size_t todiff{static_cast<::std::size_t>(tolast - tofirst)};
		if (fromdiff < Np1 || todiff < N)
		{
			break;
		}
		vec.load(fromfirst);
		// Keep a direct CR-match mask so the no-match proof and first-match lane calculation use the same polarity.
		auto matches{vec == charsvec};
		if (::fast_io::intrinsics::is_all_zeros(matches))
		{
			// Only an all-plain vector may be committed as a whole. A matching vector has a shorter logical output
			// prefix and must not overwrite storage beyond the cursor returned to the decorator driver.
			vec.store(tofirst);
			fromfirst += N;
			tofirst += N;
			continue;
		}
		// mask_countr reports element lanes (not bytes) and preserves address order on every supported endianness.
		// The preceding nonzero proof also establishes `pos < N`, leaving source element N live for pair lookahead.
		unsigned pos{::fast_io::intrinsics::vector_mask_countr_zero(matches)};
		FAST_IO_ASSUME(pos < N);
		// Copy exactly the committed prefix, including the CR at `pos`; the following pair-resolution step may
		// replace that final code unit but cannot expose or modify any byte beyond the returned output cursor.
		::fast_io::details::non_overlapped_copy_n(fromfirst, static_cast<::std::size_t>(pos) + 1u, tofirst);
		fromfirst += pos + 1;
		tofirst += pos + 1;
		if (*fromfirst != lfchct)
		{
			continue;
		}
		++fromfirst;
		if constexpr (cr)
		{
			tofirst[-1] = lfchr;
		}
		else
		{
			tofirst[-1] = lfchct;
		}
	}
	return {fromfirst, tofirst};
}

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
	constexpr simd_vector_type charsvec{::std::bit_cast<simd_vector_type>(
		create_find_simd_vector_with_unsigned_toggle<false, char_type, N>(tofdch))};
	constexpr simd_vector_type threevec{::std::bit_cast<simd_vector_type>(characters_array_impl<3, char_type, N>)};
#else
	simd_vector_type charsvec;
	constexpr auto chars_array{
		create_find_simd_vector_with_unsigned_toggle<false, char_type, N>(tofdch)};
	charsvec.load(chars_array.data());
	simd_vector_type threevec;
	threevec.load(characters_array_impl<3, char_type, N>.data());
#endif

	for (simd_vector_type vec;;)
	{
		::std::size_t fromdiff{static_cast<::std::size_t>(fromlast - fromfirst)};
		::std::size_t todiff{static_cast<::std::size_t>(tolast - tofirst)};
		if (fromdiff < N || todiff < N)
		{
			break;
		}
		vec.load(fromfirst);
		auto comres{vec == charsvec};
		auto comresandres{comres & threevec};
		if constexpr (cr)
		{
			vec -= comresandres;
		}
		else
		{
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
