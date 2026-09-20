#pragma once

/**
 * @file
 * @brief Implements stateful bounded end-of-line conversion.
 *
 * CRLF expansion/contraction may split at either source or destination
 * boundaries. pending_character records exactly that one unresolved code unit,
 * and sync-flush/finish drain it without accessing an unbounded destination.
 */

namespace fast_io
{

namespace transcoders
{

/** @brief Identifies supported logical and platform-native newline encodings. */
enum class eol_scheme
{
	lf,
	crlf,
	cr,
	nl, /* EBCDIC */
#if 0
	lfcr,
	newline,
#endif
#if (defined(_WIN32) && !defined(__WINE__)) || defined(__CYGWIN__) || defined(__MSDOS__)
	// Windows-family native text convention.
	native = crlf
#else
	// POSIX and other platforms use LF as the native convention.
	native = lf
#endif
};

/** @brief Stateful bounded engine converting between newline conventions. */
template <::std::integral char_type, eol_scheme from_scheme, eol_scheme to_scheme>
struct basic_eol
{
	using from_value_type = char_type;
	using to_value_type = char_type;

	bool pending_character{};

private:
	struct copy_prefix_result
	{
		char_type const *from_next;
		char_type *to_next;
	};

	/**
	 * @brief Copies the maximal bounded prefix which excludes delimiter.
	 *
	 * Formal invariant: on return, [old_from, from_next) equals
	 * [old_to, to_next), both cursor advances are equal, neither cursor crosses
	 * its bound, and either source/output is exhausted or *from_next is
	 * delimiter. On admitted SIMD targets, lane equality produces either
	 * canonical zero/all-ones byte predicates (AArch64, AVX2, and SSE2) or a
	 * direct AVX-512 k-mask. AArch64 horizontal maximum, x86 movemask, and the
	 * direct k-mask are zero exactly when every lane differs from delimiter. The
	 * portable byte path applies the standard zero-byte lemma
	 * `(x - 0x01..) & ~x & 0x80..` after XOR with a repeated delimiter. Both
	 * kernels test the complete block before committing it, so a detected
	 * delimiter can only shorten the scalar suffix; neither kernel can copy
	 * across the first match. These predicates and the raw block copy are
	 * independent of native byte order.
	 */
	inline static constexpr copy_prefix_result copy_plain_prefix(
		char_type const *from_first, char_type const *from_last,
		char_type *to_first, char_type *to_last,
		char_type delimiter) noexcept
	{
		if (!__builtin_is_constant_evaluated())
		{
			if constexpr (sizeof(char_type) == 1u)
			{
#if ((defined(__aarch64__) || defined(__arm64__)) && defined(__ARM_NEON) && \
	 (defined(__clang__) || (defined(__GNUC__) && !defined(__clang__))))
				// Keep the byte-domain projection inside the native branch which consumes it. Portable targets compare the
				// original character value below, so a wider declaration would be an unused object under strict diagnostics.
				unsigned char const delimiter_byte{
					static_cast<unsigned char>(delimiter)};
				constexpr ::std::size_t vector_size{16u};
				using native_u8x16 [[gnu::vector_size(16)]] = unsigned char;
				native_u8x16 const delimiters{
					delimiter_byte, delimiter_byte, delimiter_byte, delimiter_byte,
					delimiter_byte, delimiter_byte, delimiter_byte, delimiter_byte,
					delimiter_byte, delimiter_byte, delimiter_byte, delimiter_byte,
					delimiter_byte, delimiter_byte, delimiter_byte, delimiter_byte};
				for (;;)
				{
					::std::size_t const from_size{
						static_cast<::std::size_t>(from_last - from_first)};
					::std::size_t const to_size{
						static_cast<::std::size_t>(to_last - to_first)};
					if (vector_size > from_size || vector_size > to_size)
					{
						break;
					}
					native_u8x16 source;
					::fast_io::freestanding::my_memcpy(
						__builtin_addressof(source), from_first, vector_size);
					auto const compared{source == delimiters};
					native_u8x16 const matches{
						__builtin_bit_cast(native_u8x16, compared)};
					unsigned char maximum_match;
#if defined(__clang__)
					maximum_match = static_cast<unsigned char>(
						__builtin_neon_vmaxvq_u8(matches));
#else
					maximum_match = static_cast<unsigned char>(
						__builtin_aarch64_reduc_umax_scal_v16qi_uu(matches));
#endif
					if (maximum_match != 0u)
					{
						break;
					}
					::fast_io::freestanding::my_memcpy(
						to_first, __builtin_addressof(source), vector_size);
					from_first += vector_size;
					to_first += vector_size;
				}
#elif (((defined(__x86_64__) || defined(__i386__)) &&     \
		!defined(__arm64ec__) && !defined(_M_ARM64EC)) && \
	   defined(__SSE2__) &&                               \
	   FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb128))
				// The x86 equality vectors operate on the unsigned byte representation; non-x86 fallbacks do not need this
				// projection and therefore cannot inherit an unused declaration from the SIMD implementation.
				unsigned char const delimiter_byte{
					static_cast<unsigned char>(delimiter)};
#if defined(__AVX512F__) && defined(__AVX512BW__) && \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_ucmpb512_mask)
				// A 64-byte iteration is selected only for translation units whose
				// target explicitly enables AVX-512F and AVX-512BW. Targets which
				// avoid wide-vector frequency or power costs retain the AVX2/SSE2
				// fallback without a runtime-dispatch or code-size penalty.
				constexpr ::std::size_t native_vector_size{64u};
				using native_u8_vector [[gnu::vector_size(64)]] = unsigned char;
				using native_char_vector [[gnu::vector_size(64)]] = char;
#elif defined(__AVX2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb256)
				constexpr ::std::size_t native_vector_size{32u};
				using native_u8_vector [[gnu::vector_size(32)]] = unsigned char;
				using native_char_vector [[gnu::vector_size(32)]] = char;
#else
				constexpr ::std::size_t native_vector_size{16u};
				using native_u8_vector [[gnu::vector_size(16)]] = unsigned char;
				using native_char_vector [[gnu::vector_size(16)]] = char;
#endif
				native_u8_vector delimiters{};
				// GNU vector scalar addition broadcasts the unsigned delimiter byte
				// without an aggregate construction.  Because the vector is
				// zero-initialized, every lane becomes the delimiter's exact modulo-256
				// bit pattern; this also gives older GCC releases one vpbroadcastb
				// instead of scalar materialization through a temporary stack object.
				delimiters += delimiter_byte;
#if defined(__AVX512F__) && defined(__AVX512BW__) && \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_ucmpb512_mask)
				::std::size_t const source_block_count{
					static_cast<::std::size_t>(from_last - from_first) /
					native_vector_size};
				::std::size_t const destination_block_count{
					static_cast<::std::size_t>(to_last - to_first) /
					native_vector_size};
				::std::size_t remaining_blocks{
					source_block_count < destination_block_count
						? source_block_count
						: destination_block_count};
				// Compute the common bounded block count once for AVX-512. Rechecking
				// two pointer differences after every committed 64-byte vector made
				// Clang retain two dependent countdown chains. The minimum proves that
				// every remaining load and store fits both ranges; a matching block is
				// still left uncommitted for the scalar first-match suffix. AVX2 and
				// SSE2 deliberately retain their pointer-bound loop below: their
				// shorter blocks do not amortize this quotient setup on small ranges.
				for (; remaining_blocks != 0u; --remaining_blocks)
#else
				for (;;)
#endif
				{
#if !(defined(__AVX512F__) && defined(__AVX512BW__) && \
	  FAST_IO_HAS_BUILTIN(__builtin_ia32_ucmpb512_mask))
					::std::size_t const from_size{
						static_cast<::std::size_t>(from_last - from_first)};
					::std::size_t const to_size{
						static_cast<::std::size_t>(to_last - to_first)};
					if (native_vector_size > from_size ||
						native_vector_size > to_size)
					{
						break;
					}
#endif
					native_u8_vector source;
					::fast_io::freestanding::my_memcpy(
						__builtin_addressof(source), from_first, native_vector_size);
#if defined(__AVX512F__) && defined(__AVX512BW__) && \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_ucmpb512_mask)
					// The AVX-512 comparison builtin requires a plain-char vector,
					// although equality itself is independent of byte signedness.
					// These bit-casts retain the unsigned source and delimiter bit
					// patterns exactly; no value conversion can alter a high-bit lane.
					native_char_vector const source_bits{
						__builtin_bit_cast(native_char_vector, source)};
					native_char_vector const delimiter_bits{
						__builtin_bit_cast(native_char_vector, delimiters)};
					unsigned long long const match_mask{
						static_cast<unsigned long long>(
							__builtin_ia32_ucmpb512_mask(
								source_bits, delimiter_bits, 0, ~0ULL))};
#else
					auto const compared{source == delimiters};
					// GCC requires the exact plain-char vector ABI type for each
					// pmovmskb builtin; the bit-cast preserves every comparison bit.
					native_char_vector const matches{
						__builtin_bit_cast(native_char_vector, compared)};
#if defined(__AVX2__) && FAST_IO_HAS_BUILTIN(__builtin_ia32_pmovmskb256)
					unsigned int const match_mask{static_cast<unsigned int>(
						__builtin_ia32_pmovmskb256(matches))};
#else
					unsigned int const match_mask{static_cast<unsigned int>(
						__builtin_ia32_pmovmskb128(matches))};
#endif
#endif
					// The range checks prove that the selected 64-byte AVX-512BW,
					// 32-byte AVX2, or 16-byte SSE2 iteration cannot cross either
					// bound.  AVX-512 produces its predicate directly in a k-mask;
					// AVX2/SSE2 first produce canonical byte predicates and then
					// reduce their sign bits with movemask.  In either representation,
					// mask == 0 iff every lane is plain and the whole block may be
					// committed. For a nonzero mask, countr_zero(mask) denotes the
					// first delimiter lane. Leaving that block uncommitted lets the
					// scalar tail reach the same lane without paying for ctz or
					// partial-block branches on delimiter-dense input.
					if (match_mask != 0u)
					{
						break;
					}
					::fast_io::freestanding::my_memcpy(
						to_first, __builtin_addressof(source), native_vector_size);
					from_first += native_vector_size;
					to_first += native_vector_size;
				}
#else
				// The portable word kernel preserves the same all-or-nothing commit
				// proof for targets without a native 16-byte any-match reduction and
				// leaves every sub-word tail to the scalar postcondition loop.
				constexpr ::std::size_t word_size{sizeof(::std::size_t)};
				constexpr unsigned char byte_max{
					::std::numeric_limits<unsigned char>::max()};
				constexpr ::std::size_t ones{
					::std::numeric_limits<::std::size_t>::max() / byte_max};
				constexpr ::std::size_t highs{ones * (byte_max / 2u + 1u)};
				::std::size_t const repeated{
					ones * static_cast<unsigned char>(delimiter)};

				for (;;)
				{
					::std::size_t const from_size{
						static_cast<::std::size_t>(from_last - from_first)};
					::std::size_t const to_size{
						static_cast<::std::size_t>(to_last - to_first)};
					if (word_size * 2u > from_size || word_size * 2u > to_size)
					{
						break;
					}

					::std::size_t first_word;
					::std::size_t second_word;
					::fast_io::freestanding::my_memcpy(
						__builtin_addressof(first_word), from_first, word_size);
					::fast_io::freestanding::my_memcpy(
						__builtin_addressof(second_word), from_first + word_size,
						word_size);
					::std::size_t const first_match{first_word ^ repeated};
					::std::size_t const second_match{second_word ^ repeated};
					::std::size_t const match_mask{
						((first_match - ones) & ~first_match & highs) |
						((second_match - ones) & ~second_match & highs)};
					if (match_mask != 0u)
					{
						break;
					}
					::fast_io::freestanding::my_memcpy(
						to_first, __builtin_addressof(first_word), word_size);
					::fast_io::freestanding::my_memcpy(
						to_first + word_size, __builtin_addressof(second_word),
						word_size);
					from_first += word_size * 2u;
					to_first += word_size * 2u;
				}
#endif
			}
		}

		while (from_first != from_last && to_first != to_last &&
			   *from_first != delimiter)
		{
			*to_first = *from_first;
			++from_first;
			++to_first;
		}
		return {from_first, to_first};
	}

	/** @brief Derives the process status from the remaining bounded source range. */
	inline static constexpr ::fast_io::basic_transcode_process_result<char_type, char_type>
	process_result(char_type const *from_next, char_type const *from_last,
				   char_type *to_next) noexcept
	{
		return {from_next, to_next,
				from_next == from_last
					? ::fast_io::transcode_step_status::need_input
					: ::fast_io::transcode_step_status::need_output};
	}

	/** @brief Emits the single code unit retained across a bounded call boundary. */
	inline constexpr ::fast_io::basic_transcode_drain_result<char_type>
	drain(char_type *to_first, char_type *to_last) noexcept
	{
		if constexpr (!((from_scheme == eol_scheme::lf &&
						 to_scheme == eol_scheme::crlf) ||
						(from_scheme == eol_scheme::crlf &&
						 to_scheme == eol_scheme::lf)))
		{
			// Formal invariant: every other EOL mapping consumes and emits one code
			// unit in the same process step, so process never creates drain state.
			return {to_first, ::fast_io::transcode_drain_status::complete};
		}
		else
		{
			// A pending LF completes an earlier CR insertion; a pending CR from CRLF
			// contraction becomes literal when no following LF is available.
			if (!pending_character)
			{
				// No expansion or contraction state remains to be emitted.
				return {to_first, ::fast_io::transcode_drain_status::complete};
			}
			if (to_first == to_last)
			{
				// Preserve pending state until the caller supplies writable capacity.
				return {to_first, ::fast_io::transcode_drain_status::need_output};
			}
			if constexpr (from_scheme == eol_scheme::lf &&
						  to_scheme == eol_scheme::crlf)
			{
				// Complete the LF half of an earlier CRLF expansion.
				*to_first = ::fast_io::char_literal_v<u8'\n', char_type>;
			}
			else
			{
				// Emit an unmatched CR retained by CRLF contraction.
				static_assert(from_scheme == eol_scheme::crlf &&
							  to_scheme == eol_scheme::lf);
				*to_first = ::fast_io::char_literal_v<u8'\r', char_type>;
			}
			pending_character = false;
			return {to_first + 1, ::fast_io::transcode_drain_status::complete};
		}
	}

public:
	/** @brief Converts as much source as bounded destination capacity permits. */
	inline constexpr ::fast_io::basic_transcode_process_result<char_type, char_type>
	process(char_type const *from_first, char_type const *from_last,
			char_type *to_first, char_type *to_last) noexcept
	{
		if constexpr (from_scheme == eol_scheme::lf &&
					  to_scheme == eol_scheme::crlf)
		{
			// Expand each LF to CRLF while retaining a split trailing LF if needed.
			if (pending_character)
			{
				// Complete an expansion split at the preceding destination boundary.
				if (to_first == to_last)
				{
					// Request capacity without consuming any new source.
					return process_result(from_first, from_last, to_first);
				}
				*to_first = ::fast_io::char_literal_v<u8'\n', char_type>;
				++to_first;
				pending_character = false;
			}
			while (from_first != from_last && to_first != to_last)
			{
				// Copy maximal ordinary runs before handling one expansion delimiter.
				auto const copied{copy_plain_prefix(
					from_first, from_last, to_first, to_last,
					::fast_io::char_literal_v<u8'\n', char_type>)};
				from_first = copied.from_next;
				to_first = copied.to_next;
				if (from_first == from_last || to_first == to_last)
				{
					break;
				}
				// The prefix postcondition proves that the remaining unit is LF.
				++from_first;
				*to_first = ::fast_io::char_literal_v<u8'\r', char_type>;
				++to_first;
				if (to_first == to_last)
				{
					// Remember that the LF half must lead the next process/drain call.
					pending_character = true;
					break;
				}
				*to_first = ::fast_io::char_literal_v<u8'\n', char_type>;
				++to_first;
			}
		}
		else if constexpr (from_scheme == eol_scheme::crlf &&
						   to_scheme == eol_scheme::lf)
		{
			// Contract CRLF pairs while preserving unmatched CR code units.
			if (pending_character)
			{
				// Resolve a CR that ended the preceding bounded source range.
				if (to_first == to_last || from_first == from_last)
				{
					// Resolution requires both one output slot and one lookahead unit.
					return process_result(from_first, from_last, to_first);
				}
				if (*from_first == ::fast_io::char_literal_v<u8'\n', char_type>)
				{
					// Consume the LF and publish the contracted newline.
					*to_first = ::fast_io::char_literal_v<u8'\n', char_type>;
					++from_first;
				}
				else
				{
					// Publish the retained CR when lookahead is not LF.
					*to_first = ::fast_io::char_literal_v<u8'\r', char_type>;
				}
				++to_first;
				pending_character = false;
			}
			while (from_first != from_last && to_first != to_last)
			{
				// Copy maximal ordinary runs before resolving one possible CRLF pair.
				auto const copied{copy_plain_prefix(
					from_first, from_last, to_first, to_last,
					::fast_io::char_literal_v<u8'\r', char_type>)};
				from_first = copied.from_next;
				to_first = copied.to_next;
				if (from_first == from_last || to_first == to_last)
				{
					break;
				}
				// The prefix postcondition proves that the remaining unit is CR.
				++from_first;
				if (from_first == from_last)
				{
					// Retain a trailing CR until more input or terminal drain.
					pending_character = true;
					break;
				}
				char_type ch{::fast_io::char_literal_v<u8'\r', char_type>};
				if (*from_first == ::fast_io::char_literal_v<u8'\n', char_type>)
				{
					// Replace the CRLF pair with one LF and consume lookahead.
					ch = ::fast_io::char_literal_v<u8'\n', char_type>;
					++from_first;
				}
				*to_first = ch;
				++to_first;
			}
		}
		else if constexpr ((from_scheme == eol_scheme::lf &&
							to_scheme == eol_scheme::cr) ||
						   (from_scheme == eol_scheme::cr &&
							to_scheme == eol_scheme::lf))
		{
			// Substitute single-unit LF and CR conventions without expansion.
			constexpr bool from_cr{from_scheme == eol_scheme::cr};
			constexpr char_type from_character{
				from_cr ? ::fast_io::char_literal_v<u8'\r', char_type>
						: ::fast_io::char_literal_v<u8'\n', char_type>};
			constexpr char_type to_character{
				from_cr ? ::fast_io::char_literal_v<u8'\n', char_type>
						: ::fast_io::char_literal_v<u8'\r', char_type>};
			while (from_first != from_last && to_first != to_last)
			{
				// Preserve ordinary runs and rewrite exactly one delimiter per step.
				auto const copied{copy_plain_prefix(
					from_first, from_last, to_first, to_last, from_character)};
				from_first = copied.from_next;
				to_first = copied.to_next;
				if (from_first == from_last || to_first == to_last)
				{
					break;
				}
				*to_first = to_character;
				++from_first;
				++to_first;
			}
		}
		else if constexpr ((from_scheme == eol_scheme::lf &&
							to_scheme == eol_scheme::nl) ||
						   (from_scheme == eol_scheme::nl &&
							to_scheme == eol_scheme::lf))
		{
			// Substitute between ASCII LF and the execution-set NL character.
			constexpr bool from_nl{from_scheme == eol_scheme::nl};
			constexpr char_type from_character{
				from_nl
					? ::fast_io::details::execution_newline_literal<char_type>()
					: ::fast_io::char_literal_v<u8'\n', char_type>};
			constexpr char_type to_character{
				from_nl
					? ::fast_io::char_literal_v<u8'\n', char_type>
					: ::fast_io::details::execution_newline_literal<char_type>()};
			while (from_first != from_last && to_first != to_last)
			{
				// Preserve ordinary runs and rewrite exactly one delimiter per step.
				auto const copied{copy_plain_prefix(
					from_first, from_last, to_first, to_last, from_character)};
				from_first = copied.from_next;
				to_first = copied.to_next;
				if (from_first == from_last || to_first == to_last)
				{
					break;
				}
				*to_first = to_character;
				++from_first;
				++to_first;
			}
		}
		else
		{
			if (from_first == from_last || to_first == to_last)
			{
				// Identity conversion has no state to drain. Preserve the appropriate need-input/need-output status while
				// avoiding subtraction and pointer advancement on a valid all-null empty range.
				return process_result(from_first, from_last, to_first);
			}
			// The protocol does not require disjoint source and destination ranges.
			// Therefore the copied prefix has snapshot semantics even when either
			// range begins inside the other: overlapped_copy uses a temporary during
			// constant evaluation and memmove at run time. Exact in-place identity
			// needs no copy, but both cursors still advance by the maximal common
			// extent selected by the two independent bounds.
			::std::size_t const from_size{
				static_cast<::std::size_t>(from_last - from_first)};
			::std::size_t const to_size{
				static_cast<::std::size_t>(to_last - to_first)};
			::std::size_t const copy_size{
				from_size < to_size ? from_size : to_size};
			if (from_first == to_first)
			{
				to_first += copy_size;
			}
			else
			{
				to_first = ::fast_io::freestanding::overlapped_copy(
					from_first, from_first + copy_size, to_first);
			}
			from_first += copy_size;
		}
		return process_result(from_first, from_last, to_first);
	}

	/** @brief Nonterminally emits any pending split newline code unit. */
	inline constexpr ::fast_io::basic_transcode_drain_result<char_type>
	sync_flush(char_type *to_first, char_type *to_last) noexcept
	{
		return drain(to_first, to_last);
	}

	/** @brief Terminally emits any pending expansion or unmatched CR. */
	inline constexpr ::fast_io::basic_transcode_drain_result<char_type>
	finish(char_type *to_first, char_type *to_last) noexcept
	{
		return drain(to_first, to_last);
	}
};

/** @brief Exposes bounded newline processing through the transcode process CPO. */
template <::std::integral char_type, eol_scheme from_scheme, eol_scheme to_scheme>
inline constexpr ::fast_io::basic_transcode_process_result<char_type, char_type>
transcode_process_define(
	basic_eol<char_type, from_scheme, to_scheme> &engine,
	char_type const *from_first, char_type const *from_last,
	char_type *to_first, char_type *to_last) noexcept
{
	return engine.process(from_first, from_last, to_first, to_last);
}

/** @brief Exposes nonterminal newline drain through the sync-flush CPO. */
template <::std::integral char_type, eol_scheme from_scheme, eol_scheme to_scheme>
inline constexpr ::fast_io::basic_transcode_drain_result<char_type>
transcode_sync_flush_define(
	basic_eol<char_type, from_scheme, to_scheme> &engine,
	char_type *to_first, char_type *to_last) noexcept
{
	return engine.sync_flush(to_first, to_last);
}

/** @brief Exposes terminal newline drain through the finish CPO. */
template <::std::integral char_type, eol_scheme from_scheme, eol_scheme to_scheme>
inline constexpr ::fast_io::basic_transcode_drain_result<char_type>
transcode_finish_define(
	basic_eol<char_type, from_scheme, to_scheme> &engine,
	char_type *to_first, char_type *to_last) noexcept
{
	return engine.finish(to_first, to_last);
}

/** @brief Guarantees that every newline phase can progress with one output unit. */
template <::std::integral char_type, eol_scheme from_scheme, eol_scheme to_scheme>
inline constexpr ::std::size_t transcode_min_output_size_define(
	::fast_io::transcode_reserve_t<basic_eol<char_type, from_scheme, to_scheme>>,
	::fast_io::transcode_phase) noexcept
{
	return 1u;
}

using lf_to_crlf = basic_eol<char, eol_scheme::lf, eol_scheme::crlf>;
using crlf_to_lf = basic_eol<char, eol_scheme::crlf, eol_scheme::lf>;

using wlf_to_crlf = basic_eol<wchar_t, eol_scheme::lf, eol_scheme::crlf>;
using wcrlf_to_lf = basic_eol<wchar_t, eol_scheme::crlf, eol_scheme::lf>;

using u8lf_to_crlf = basic_eol<char8_t, eol_scheme::lf, eol_scheme::crlf>;
using u8crlf_to_lf = basic_eol<char8_t, eol_scheme::crlf, eol_scheme::lf>;

using u16lf_to_crlf = basic_eol<char16_t, eol_scheme::lf, eol_scheme::crlf>;
using u16crlf_to_lf = basic_eol<char16_t, eol_scheme::crlf, eol_scheme::lf>;

using u32lf_to_crlf = basic_eol<char32_t, eol_scheme::lf, eol_scheme::crlf>;
using u32crlf_to_lf = basic_eol<char32_t, eol_scheme::crlf, eol_scheme::lf>;

} // namespace transcoders

} // namespace fast_io
